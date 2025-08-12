///
/// @file
/// @brief eForth header - C++ vector-based, token-threaded
///
///====================================================================
#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <iostream>                    /// cin, cout
#include <iomanip>                     /// setbase
#include <vector>                      /// vector
#include <chrono>
#include "config.h"

using namespace std;

#if DO_MULTITASK
#include <mutex>
#include <condition_variable>
typedef  thread             THREAD;
typedef  mutex              MUTEX;
typedef  condition_variable COND_VAR;
#define  GUARD(m)           lock_guard<mutex>  _grd_(m)
#define  XLOCK(m)           unique_lock<mutex> _xlck_(m)   /** exclusive lock     */
#define  WAIT(cv,g)         (cv).wait(_xlck_, g)           /** wait for condition */
#define  NOTIFY(cv)         (cv).notify_one()              /** wake up one task   */
#define  NOTIFY_ALL(cv)     (cv).notify_all();

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef _POSIX_VERSION
#include <sched.h>                 /// CPU affinity
#endif // _POSIX_VERSION
#endif // DO_MULTITASK

template<typename T>
struct FV : public vector<T> {      ///< our super-vector class
    FV *merge(FV<T> &v) {
        this->insert(this->end(), v.begin(), v.end()); v.clear(); return this;
    }
    ~FV() {                         ///< free pointed elements
        if constexpr(is_pointer<T>::value) {
            for (T t : *this) if (t != nullptr) { delete t; t = nullptr; }
        }
    }
    void push(T n) { this->push_back(n); }
    T    pop()     { T n = this->back(); this->pop_back(); return n; }
    T    &operator[](int i) {
#if CC_DEBUG
        return this->at(i < 0 ? (this->size() + i) : i); // with range checked
#else  // !CC_DEBUG
        return vector<T>::operator[](i < 0 ? (this->size() + i) : i);
#endif // CC_DEBUG
    }
};
///====================================================================
///
///> VM context (single task)
///
typedef enum { STOP=0, HOLD, QUERY, NEST } vm_state;
struct ALIGNAS VM {
    FV<DU>   ss;                   ///< data stack
    FV<DU>   rs;                   ///< return stack
    
    DU       tos     = -DU1;       ///< cached top of stack
    IU       id      = 0;          ///< vm id
    IU       wp      = 0;          ///< word pointer
    
    U8       *base   = 0;          ///< numeric radix (a pointer)
    vm_state state   = STOP;       ///< VM status
    bool     compile = false;      ///< compiler flag

    string   pad;
#if DO_MULTITASK
    static int      NCORE;         ///< number of hardware cores
    
    static bool     io_busy;       ///< IO locking control
    static MUTEX    io;            ///< mutex for io access
    static MUTEX    tsk;           ///< mutex for tasker
    static COND_VAR cv_io;         ///< io control
    static COND_VAR cv_tsk;        ///< tasker control
    static void _ss_dup(VM &dst, VM &src, int n);
    ///
    /// task life cycle methods
    ///
    void set_state(vm_state st);   ///< set VM state (synchronized)
    void reset(IU w, vm_state st); ///< reset a VM user variables
    void join(int tid);            ///< wait for the given task to end
    void stop();                   ///< stop VM
    ///
    /// messaging interface
    ///
    void send(int tid, int n);     ///< send onto destination VM's stack (blocking, wait for receiver availabe)
    void recv();                   ///< receive data from any sending VM's stack (blocking, wait for sender's message)
    void bcast(int n);             ///< broadcast to all receivers
    void pull(int tid, int n);     ///< pull n items from the stack of a stopped task
    ///
    /// IO interface
    ///
    void io_lock();                ///< lock IO
    void io_unlock();              ///< unlock IO
#else  // DO_MULTITASK
    
    void set_state(vm_state st) { state = st; }
#endif // DO_MULTITASK
};
///
///> data structure for dictionary entry
///
struct Code;                       ///< Code class forward declaration
typedef void (*XT)(VM &vm, Code&); ///< function pointer

struct Code  {                     ///> Colon words
    const static U32 IMMD_FLAG = 0x80000000;
    const char *name;              ///< name of word
    const char *desc;              ///< reserved
    XT         xt = NULL;          ///< execution token
    FV<Code*>  pf;                 ///< parameter field
    FV<DU>     q;                  ///< parameter field - literal
    union {                        ///< union to reduce struct size
        U32 attr = 0;              /// * zero all sub-fields
        struct {
            U32 token   : 24;      ///< dict index, 0=param word
            U32 xxx     :  3;      ///< reserved
            U32 is_bran :  1;      ///< branching opcode
            U32 stage   :  2;      ///< branching state
            U32 is_str  :  1;      ///< string flag
            U32 immd    :  1;      ///< immediate flag
        };
    };
    Code(const char *s, const char *d, XT fp, U32 a);  ///> primitive
    Code(const char *s, bool n=true);                  ///> colon, n=new word
    Code(XT fp) : Code("", "", fp, 0) {}               ///> sub-classes
    ~Code() { if (!xt) { delete name; delete desc; } } ///> delete name of colon word
    Code *append(Code *w) { pf.push(w); return this; } ///> add token
    void nest(VM &vm);                                 ///> inner interpreter
};
///
///> macros to reduce verbosity (but harder to single-step debug)
///
#define BASE_NODE   0                /* Code node to keep VM.base */
#define TOS         (vm.tos)
#define SS          (vm.ss)
#define RS          (vm.rs)
#define BOOL(f)     ((f) ? -1 : 0)
#define CODE(s, g)  { s, #g, [](VM &vm, Code &c){ g; }, __COUNTER__ }
#define IMMD(s, g)  { s, #g, [](VM &vm, Code &c){ g; }, __COUNTER__ | Code::IMMD_FLAG }
#define PUSH(v)     (SS.push(TOS), TOS=((DU)(v)))
#define POP()       ([&vm](){ DU n = TOS; TOS = SS.pop(); return n; }())
#define POPI()      UINT(POP())
///
///> Primitve object and function forward declarations
///
void   _str(VM &vm, Code &c);        ///< dotstr, dostr
void   _lit(VM &vm, Code &c);        ///< numeric liternal
void   _var(VM &vm, Code &c);        ///< variable and constant
void   _tor(VM &vm, Code &c);        ///< >r (for..next)
void   _tor2(VM &vm, Code &c);       ///< swap >r >r (do..loop)
void   _if(VM &vm,  Code &c);        ///< if..then, if..else..then
void   _begin(VM &vm, Code &c);      ///< ..until, ..again, ..while..repeat
void   _for(VM &vm, Code &c);        ///< for..next, for..aft..then..next
void   _loop(VM &vm, Code &c);       ///< do..loop
void   _does(VM &vm, Code &c);       ///< does>
///
///> polymorphic constructors
///
struct Tmp : Code { Tmp()     : Code((XT)NULL) {} };
struct Lit : Code { Lit(DU d) : Code(_lit) { q.push(d); } };
struct Var : Code { Var(DU d) : Code(_var) { q.push(d); } };
struct Str : Code {
    Str(const char *s, int tok=0, int len=0) : Code(_str) {
        name  = (new string(s))->c_str();   /// * hardcopy the string
        token = (len << 16) | tok;   /// * encode word index and string length
        is_str= 1;
    }
};
struct Bran : Code {
    FV<Code*>  p1;                   ///< parameter field - if..else, aft..then
    FV<Code*>  p2;                   ///< parameter field - then..next
    Bran(XT fp) : Code(fp) {
        const char *nm[] = {
            "if", "begin", "\t", "for", "\t", "do", "does>"
        };
        XT xt[] = { _if, _begin, _tor, _for, _tor2, _loop, _does };
    
        for (int i=0; i < (int)(sizeof(nm)/sizeof(const char*)); i++) {
            if ((uintptr_t)xt[i]==(uintptr_t)fp) name = nm[i];
        }
        is_str  = 0;
        is_bran = 1;
    }
};
///
///> Multitasking support
///
VM&  vm_get(int id=0);                    ///< get a VM with given id
void uvar_init();                         ///< initialize user area

#if DO_MULTITASK
void t_pool_init();
void t_pool_stop();
int  task_create(IU w);                   ///< create a VM starting on dict[w]
void task_start(int tid);                 ///< start a thread with given task/VM id
#else  // !DO_MULTITASK
#define t_pool_init()  {}
#define t_pool_stop()  {}
#endif // !DO_MULTITASK
///
///> System interface
///
void forth_init();
void forth_include(const char *fn);       /// load external Forth script
void outer(istream &in);                  ///< Forth outer loop
#if DO_WASM
#define forth_quit() {}
#else // !DO_WASM
#define forth_quit() { vm.state=STOP; }   ///< exit to OS
#endif // DO_WASM
///
///> IO functions
///
typedef enum { RDX=0, CR, DOT, UDOT, EMIT, SPCS } io_op;

void fin_setup(const char *line);
void fout_setup(void (*hook)(int, const char*));

const Code *find(const char *s);          ///< dictionary scanner forward declare
const char *scan(char c);                 ///< scan input stream for a given char
const char *word(char delim=0);           ///< read next idiom from input stream
int  fetch(string &idiom);                ///< read input stream into string
char key();                               ///< read key from console
void load(VM &vm, const char *fn);        ///< load external Forth script
void spaces(int n);                       ///< show spaces
void dot(io_op op, DU v=DU0);             ///< print literals
void dotr(int w, DU v, int b, bool u=false); ///< print fixed width literals
void pstr(const char *str, io_op op=SPCS);///< print string
///
///> Debug functions
///
void ss_dump(VM &vm, bool forced=false);  ///< show data stack content
void see(const Code &c, int base);        ///< disassemble user defined word
void words(int base);                     ///< list dictionary words
void dict_dump(int base);                 ///< dump dictionary
void mem_dump(IU w0, IU w1, int base);    ///< dump memory for a given wordrm addr...addr+sz
void mem_stat();                          ///< display memory statistics
#endif  // __EFORTH_SRC_CEFORTH_H
