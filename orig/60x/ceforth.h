#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <stdio.h>
#include <stdint.h>     // uintxx_t
#include <exception>    // try...catch, throw
#include "config.h"     // configuation and cross-platform support
#ifndef XT0_U32
  #if __SIZEOF_POINTER__ == 8
  #define XT0_U32  0      /** 64-bit: build with -DXT0_U32=1 -no-pie to fold XT0 */
  #else
  #define XT0_U32  1      /** 32-bit target: pointers are already tokens         */
  #endif
#endif
#define XT0_MSK  0xFFFFFFFF00000000ULL

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
    int ro  = 0;        ///< readonly index

    List()  {
        v = N ? new T[N] : 0;                        ///< dynamically allocate array storage
        if (N && !v) throw "ERR: List allot failed";
    }
    ~List() {
        clear(ro);
        if (v) delete[] v;                           ///< free container
    }              
    List &operator=(T *a)   INLINE { v = a; return *this; }
    T    &operator[](int i) INLINE { return i < 0 ? v[idx + i] : v[i]; }
    void readonly_below(int i) { ro = i; }

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
    void clear(int tgt = 0) {
        int mx = (tgt > ro) ? tgt : ro;
        if constexpr (std::is_pointer<T>::value) {
            for (int i = mx; i < idx; i++) { if (v[i]) delete v[i]; }
        }
        idx = mx;
    }
};

// The lightweight register window context passed down the execution chain by value
struct Stk {
    int sp;     ///< Stack Pointer Depth
    DU  tos;    ///< Top of Stack
    DU  nos;    ///< Next of Stack
};
///====================================================================
///
///> VM context (single task)
///
typedef enum { STOP=0, HOLD, QUERY, NEST } vm_state;
struct ALIGNAS VM {
    List<DU, E4_SS_SZ> ss;         ///< parameter stack
    List<DU, E4_RS_SZ> rs;         ///< parameter stack
    char     pad[E4_PAD_SZ];       ///< temp pad buffer

    IU       id      = 0;          ///< vm id
    IU       *ip     = NULL;       ///< instruction pointer
    int      sp      = 0;
    DU       tos     = -DU1;       ///< top of stack (cached)
    DU       nos     = -DU1;

    vm_state state   = STOP;       ///< VM status
    IU       base    = 0;          ///< numeric radix (a pointer)
    bool     compile = false;      ///< compiler flag

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
#define UDF_DICT   0x8000   /** user defined word    */
///}
///@name Code class
///@brief - basic struct of dictionary entries
///
///  1. name is the pointer to word name string
///  2. xt   is the pointer to lambda function
///  3. pfa  takes 16-bit, max 64K range
///  4. attr[LSB]  : user defined flag (i.e. colon word)
///  5. attr[LSB+1]: immediate flag
///
///  Note: attr can union with xt/pfa, maskign required,
///        breaks C++ constexpr compilation rule
///
///  Code class on 64-bit systems (expand pfa to 32-bit possible)
///  +-------------------+-------------------+-------+
///  |    *name          |        xt         |  attr |
///  +-------------------+----------+--------+-------+
///                      |    pfa   |xxxxxxxx|
///                      +----------+--------+
///
///  Code class on 32-bit system
///  +---------+---------+--------+
///  |  *name  |   xt    |  attr  |
///  +---------+---------+--------+
///            |   pfa   |
///            +---------+
///@{
/// @brief Unified Function Pointer signature for the Direct-Threaded Continuation Trampoline
/// @param vm Context reference tracking task-isolated persistent structures
/// @param ip Instruction pointer passed by reference to allow inline branches and nesting jumps
/// @param Stk Localized register pack
typedef void *(*FPTR)(VM &vm, IU* ip, int sp, DU tos, DU nos);  ///< tail-call (returns NEXT)
struct Code {
#if XT0_U32
    static constexpr UFP XT0 = 0;   ///< all code & pmem below 4GB (-no-pie, or 32-bit target): folds away
#else
    static UFP XT0;                 ///< function pointer base, set at run time (PIE builds)
#endif
    const char *name = 0;   ///< name field
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< lambda pointer or offset to pmem space (4-byte align)
        UFP  pfa;           ///< user defined word offset
    };
    U8 attr = 0;            ///< only 2 LSBs used (can steal from xt/pfa)

#if __SIZEOF_POINTER__ == 8
    static IU Token(void *fp) INLINE { return (IU)((UFP)fp & 0xFFFFFFFF); }
#else
    static IU Token(void *fp) INLINE { return (IU)((UFP)fp); }
#endif
    ///
    ///> constructors for built-in, and colon words
    ///
    constexpr Code(const char *n, FPTR f, U8 a=0) : name(n), xt(f), attr(a) {}        ///< built-in
    bool is_imm() const INLINE { return attr & IMM_ATTR;    }
    bool is_udf() const INLINE { return attr & UDF_ATTR;    }
    void imm()    INLINE { attr |= IMM_ATTR;          }
};
///@}
///@name Dictionary Compiler macros
///@note - a lambda without capture can degenerate into a function pointer
///@{
constexpr Code rom_code(const char *name, FPTR fp, U8 im) {
    return { name, fp, im }; // Code(name, fp, im);
}

// External hardware dictionary configuration registers
extern const Code g_rom[] PROGMEM;
extern const int  g_romsz;
extern       U8   *MEM0;
extern       List<Code*, E4_DICT_SZ> dict;
extern       List<U8,    E4_PMEM_SZ> pmem;

// =====================================================================
// 2. High-Performance Token Unpacking Profile (Cross-Bit Portability)
// =====================================================================
#if XT0_U32
#define NEXT_FP  ((FPTR)(UFP)(*ip++))
#else
#define NEXT_FP  ((FPTR)(Code::XT0 | (UFP)*ip++))
#endif
#define NEXT()   ({ FPTR fp = NEXT_FP; return fp(vm, ip, sp, tos, nos);})   /** true tail call */

#define CODE(n, g)                                              \
    rom_code(n, [](VM &vm, IU* ip, int sp, DU tos, DU nos)      \
        INLINE -> void *{ g; NEXT(); }, (U8)0)
#define IMMD(n, g)                                              \
    rom_code(n, [](VM &vm, IU* ip, int sp, DU tos, DU nos)      \
        INLINE -> void *{ g; NEXT(); }, (U8)IMM_ATTR)
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
void forth_teardown();
void forth_core(VM &vm, const char *idiom);
int  forth_vm(const char *cmd, void(*hook)(int, const char*)=nullptr);
void forth_include(const char *fn);       /// load external Forth script
void outer(istream &in);                  ///< Forth outer loop
///@}
///@name Compiler Engine methods
///@{
void add_iu(IU i);
void add_du(DU v);
void add_w(const Code *w);
int  add_str(const char *s);
void add_xt(const char *name);
void colon(const char *name);
///@}
///@name Inner-interpreter methods
void nest(VM &vm);
void CALL(VM &vm, const Code &c);
///@name Dictionary Search methods
///@{
inline const Code *get_word(IU w);
const Code *find(const char *s);
///@}
///@name IO functions
///{@
typedef enum { CR=0, DOT, UDOT, EMIT, SPCS } io_op;

void fin_setup(const char *line);
void fout_setup(void (*hook)(int, const char*));

const char *scan(char c, char *buf, int max=E4_PAD_SZ);  ///< scan input stream for a given char
const char *word(char *buf, int max=E4_PAD_SZ);          ///< get next idiom
int  fetch(char *buf, int max=E4_IBUF_SZ);               ///< read input stream into buffer
char key(void);                                          ///< read key from console
void load(VM &vm, const char* fn);                       ///< load external Forth script
void spaces(int n);                                      ///< show spaces
void dot(io_op op, DU v=DU0, int base=10);               ///< print literals
void dotr(int w, DU v, int base=10, bool u=false);       ///< print fixed width literals
void pstr(const char *str, io_op op=SPCS);               ///< print string
///@}
///@name Debug functions
///@{
void ss_dump(VM &vm, bool forced=false);  ///< show data stack content
void see(IU pfa, int base);               ///< disassemble user defined word
void words();                             ///< list dictionary words
void dict_dump();                         ///< dump dictionary
void mem_dump(U32 addr, IU sz, int base); ///< dump memory frm addr...addr+sz
void mem_stat();                          ///< display memory statistics
///@}
///@name Javascript interface
///@{
void native_api(VM &vm);
///@}
#endif // __EFORTH_SRC_CEFORTH_H
