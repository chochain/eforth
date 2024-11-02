#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <stdio.h>
#include <stdint.h>     // uintxx_t
#include <exception>    // try...catch, throw
#include "config.h"     // configuation and cross-platform support
using namespace std;
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
        v = N ? new T[N] : 0;                     ///< dynamically allocate array storage
        if (N && !v) throw "ERR: List allot failed";
    }
    ~List() { if (v) delete[] v;   }              ///< free memory

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
typedef enum { STOP=0, HOLD, QUERY, NEST, IO } vm_state;
typedef struct _VM {
    vm_state state   = QUERY;
    IU       _ip     = 0;
    DU       _tos    = -DU1;    ///< top of stack (cached)
    bool     compile = false;   ///< compiler flag
    IU       load_dp = 0;       ///< depth of recursive include
    IU       *base;             ///< numeric radix (a pointer)
    IU       *dflt;             ///< use float data unit flag
    List<DU, E4_SS_SZ> _ss;     ///< parameter stack
    List<DU, E4_RS_SZ> _rs;     ///< parameter stack
} VM;
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

#define IS_UDF(w) (dict[w].attr & UDF_ATTR)
#define IS_IMM(w) (dict[w].attr & IMM_ATTR)
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
///
///> Universal functor (no STL) and Code class
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
///
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
///
///> Add a Word to dictionary
/// Note:
///    a lambda without capture can degenerate into a function pointer
#define ADD_CODE(n, g, im) {         \
    Code c(n, [](VM& vm){ g; }, im); \
    dict.push(c);                    \
    }
#define CODE(n, g) ADD_CODE(n, g, false)
#define IMMD(n, g) ADD_CODE(n, g, true)
///
///> System interface
///
VM&  vm_instance(int id=0);
void forth_init();
int  forth_vm(const char *cmd, void(*hook)(int, const char*)=NULL);
int  forth_include(const char *fn);       /// load external Forth script
void outer(istream &in);                  ///< Forth outer loop
///
///> IO functions
///
typedef enum { BASE=0, BL, CR, DOT, DOTR, EMIT, SPCS } io_op;

void fin_setup(const char *line);
void fout_setup(void (*hook)(int, const char*));

char *scan(char c);                       ///< scan input stream for a given char
int  fetch(string &idiom);                ///< read input stream into string
char *word();                             ///< get next idiom
char key();                               ///< read key from console
void load(const char* fn);                ///< load external Forth script
void spaces(int n);                       ///< show spaces
void put(io_op op, DU v=DU0, DU v2=DU0);  ///< print literals
void pstr(const char *str, io_op op=BL);  ///< print string
///
///> Debug functions
///
void see(IU pfa);                         ///< disassemble user defined word
void words();                             ///< list dictionary words
void ss_dump(bool forced=false);          ///< show data stack content
void dict_dump();                         ///< dump dictionary
void mem_dump(U32 addr, IU sz);           ///< dump memory frm addr...addr+sz
void mem_stat();                          ///< display memory statistics
///
///> Javascript interface
///
#if DO_WASM
void native_api();
#endif // DO_WASM

#endif // __EFORTH_SRC_CEFORTH_H
