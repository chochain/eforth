#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <stdio.h>
#include <stdint.h>     // uintxx_t
#include <exception>    // try...catch, throw
#include <string>       // string class
#include "config.h"     // configuation and cross-platform support

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

#ifdef _POSIX_VERSION
#include <sched.h>                    /// CPU affinity
#endif // _POSIX_VERSION

#ifndef _GNU_SOURCE
#define _GNU_SOURCE                    /** Emscripten needs this */
#endif
#endif // DO_MULTITASK
///
/// array class template (so we don't have dependency on C++ STL)
/// Note:
///   * using decorator pattern
///   * this is similar to vector class but much simplified
///
template<class T, int N=0>
struct List {
    T   *v;             ///< fixed-size array storage
    int idx = 0;        ///< current index of array
    int max = 0;        ///< high watermark for debugging

    List()  {
        v = N ? new T[N] : 0;                        ///< dynamically allocate array storage
        if (N && !v) throw "ERR: List allot failed";
    }
    ~List() {
        if constexpr(is_pointer<T>::value) {         ///< free elements
            for (int i=0; i<idx; i++) delete v[i];
        }
        if (v) delete[] v;                           ///< free container
    }              
    List &operator=(T *a)   INLINE { v = a; return *this; }
    T    &operator[](int i) INLINE { return i < 0 ? v[idx + i] : v[i]; }

#if RANGE_CHECK
    T pop()     INLINE {
        if (idx>0) return v[--idx];
        throw "ERR: List empty";
    }
    T push(T t) INLINE {
        if (idx<N) return v[max=idx++] = t;
        throw "ERR: List full";
    }

#else  // !RANGE_CHECK
    T pop()     INLINE { return v[--idx]; }
    T push(T t) INLINE { return v[idx++] = t; }   ///< deep copy element

#endif // RANGE_CHECK
    void push(T *a, int n) INLINE { for (int i=0; i<n; i++) push(*(a+i)); }
    void merge(List& a)    INLINE { for (int i=0; i<a.idx; i++) push(a[i]); }
    void clear(int i=0)    INLINE { idx=i; }
};
///====================================================================
///
///> VM context (single task)
///
typedef enum { STOP=0, HOLD, QUERY, NEST } vm_state;
struct ALIGNAS VM {
    List<DU, E4_SS_SZ> ss;         ///< parameter stack
    List<DU, E4_RS_SZ> rs;         ///< parameter stack

    IU       id      = 0;          ///< vm id
    IU       ip      = 0;          ///< instruction pointer
    DU       tos     = -DU1;       ///< top of stack (cached)

    bool     compile = false;      ///< compiler flag
    vm_state state   = STOP;       ///< VM status
    IU       base    = 0;          ///< numeric radix (a pointer)
    
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
    void reset(IU ip, vm_state st);///< reset a VM user variables
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
#endif // DO_MULTITASK
};
///
///@name Code flag masking options
///@{
#define UDF_ATTR   0x0001   /** user defined word    */
#define IMM_ATTR   0x0002   /** immediate word       */
#define EXT_FLAG   0x8000   /** prim/xt/pfa selector */
#if DO_WASM
#define MSK_ATTR   ~0x0     /** no masking needed    */
#else  // !DO_WASM
#define MSK_ATTR   ~0x3     /** mask udf,imm bits    */
#endif // DO_WASM

#define IS_UDF(w) (dict[w]->attr & UDF_ATTR)
#define IS_IMM(w) (dict[w]->attr & IMM_ATTR)
///}
///@name primitive opcode
///{
typedef enum {
    EXIT=0|EXT_FLAG, NOP, NEXT, LOOP, LIT, VAR, STR, DOTQ, BRAN, ZBRAN,
    VBRAN, DOES, FOR, DO, KEY, MAX_OP
} prim_op;

#define USER_AREA  (ALIGN16(MAX_OP & ~EXT_FLAG))
#define IS_PRIM(w) ((w & EXT_FLAG) && (w < MAX_OP))
///@}
///@name Code class
///@brief - basic struct of dictionary entries
///
///  1. name is the pointer to word name string
///  2. xt   is the pointer to lambda function
///  3. pfa  takes 16-bit, max 64K range
///  4. attr[LSB]  : user defined flag (i.e. colon word)
///  5. attr[LSB+1]: immediate flag
///
///  Code class on 64-bit systems (expand pfa to 32-bit possible)
///  +-------------------+-------------------+
///  |    *name          |       xt          |
///  +-------------------+----+----+---------+
///                      |attr|pfa |xxxxxxxxx|
///                      +----+----+---------+
///
///  Code class on 32-bit systems (memory best utilized)
///  +---------+---------+
///  |  *name  |   xt    |
///  +---------+----+----+
///            |attr|pfa |
///            +----+----+
///
///  Code class on WASM systems (a bit wasteful but faster)
///  +---------+---------+----+
///  |  *name  |   xt    |attr|
///  +---------+----+----+----+
///            |pfa |xxxx|
///            +----+----+
///@{
typedef void (*FPTR)(VM&);  ///< function pointer
struct Code {
    static UFP XT0;         ///< function pointer base (in registers hopefully)
    const char *name = 0;   ///< name field
#if DO_WASM
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< vtable index
        IU   pfa;           ///< offset to pmem space (16-bit for 64K range)
    };
    IU attr;                ///< xt is vtable index so attrs need to be separated
#else // !DO_WASM
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< lambda pointer (4-byte align, 2 LSBs can be used for attr)
        struct {
            IU attr;        ///< steal 2 LSBs because xt is 4-byte aligned on 32-bit CPU
            IU pfa;         ///< offset to pmem space (16-bit for 64K range)
        };
    };
#endif // DO_WASM
    static FPTR XT(IU ix)   INLINE { return (FPTR)(XT0 + (UFP)(ix & MSK_ATTR)); }
    static void exec(VM &vm, IU ix) INLINE { (*XT(ix))(vm); }

    Code() {}               ///< blank struct (for initilization)
    Code(const char *n, IU w) : name(n), xt((FPTR)((UFP)w)) {} ///< primitives
    Code(const char *n, FPTR fp, bool im) : name(n), xt(fp) {  ///< built-in and colon words
        attr |= im ? IMM_ATTR : 0;
    }
    IU   xtoff() INLINE { return (IU)(((UFP)xt - XT0) & MSK_ATTR); }  ///< xt offset in code space
    void call(VM& vm)  INLINE { (*(FPTR)((UFP)xt & MSK_ATTR))(vm); }
};
///@}
///@name Dictionary Compiler macros
///@note - a lambda without capture can degenerate into a function pointer
///@{
#define ADD_CODE(n, g, im) {                     \
    Code *c = new Code(n, [](VM& vm){ g; }, im); \
    dict.push(c);                                \
    }
#define CODE(n, g) ADD_CODE(n, g, false)
#define IMMD(n, g) ADD_CODE(n, g, true)
///@}
///@name Multitasking support
///@{
VM&  vm_get(int id=0);                    ///< get a VM with given id
void uvar_init();                         ///< setup user area

#if DO_MULTITASK
void t_pool_init();                       ///< initialize thread pool
void t_pool_stop();                       ///< stop thread pool
int  task_create(IU pfa);                 ///< create a VM starting on pfa
void task_start(int tid);                 ///< start a thread with given task/VM id
#else
#define t_pool_init()
#define t_pool_stop()
#endif // DO_MULTITASK
///@}
///@name System interface
///@{
void forth_init();
int  forth_vm(const char *cmd, void(*hook)(int, const char*)=NULL);
void forth_include(const char *fn);       /// load external Forth script
void outer(istream &in);                  ///< Forth outer loop
///@}
///@name IO functions
///{@
typedef enum { RDX=0, CR, DOT, UDOT, EMIT, SPCS } io_op;

void fin_setup(const char *line);
void fout_setup(void (*hook)(int, const char*));

const char *scan(char c);                 ///< scan input stream for a given char
const char *word();                       ///< get next idiom
int  fetch(string &idiom);                ///< read input stream into string
char key();                               ///< read key from console
void load(VM &vm, const char* fn);        ///< load external Forth script
void spaces(int n);                       ///< show spaces
void dot(io_op op, DU v=DU0);             ///< print literals
void dotr(int w, DU v, int b, bool u=false); ///< print fixed width literals
void pstr(const char *str, io_op op=SPCS);///< print string
///@}
///@name Debug functions
///@{
void ss_dump(VM &vm, bool forced=false);  ///< show data stack content
void see(IU pfa, int base);               ///< disassemble user defined word
void words(int base);                     ///< list dictionary words
void dict_dump(int base);                 ///< dump dictionary
void mem_dump(U32 addr, IU sz, int base); ///< dump memory frm addr...addr+sz
void mem_stat();                          ///< display memory statistics
///@}
///@name Javascript interface
///@{
#if DO_WASM
void native_api(VM &vm);
#endif // DO_WASM
///@}
#endif // __EFORTH_SRC_CEFORTH_H
