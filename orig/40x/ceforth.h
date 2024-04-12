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
template<class T, int N>
struct List {
    T   *v;             ///< fixed-size array storage
    int idx = 0;        ///< current index of array
    int max = 0;        ///< high watermark for debugging

    List()  {
        v = N ? new T[N] : 0;                     ///< dynamically allocate array storage
        if (!v) throw "ERR: List allot failed";
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
    T push(T t) INLINE { return v[max=idx++] = t; }   ///< deep copy element

#endif // RANGE_CHECK
    void push(T *a, int n) INLINE { for (int i=0; i<n; i++) push(*(a+i)); }
    void merge(List& a)    INLINE { for (int i=0; i<a.idx; i++) push(a[i]); }
    void clear(int i=0)    INLINE { idx=i; }
};
///
///@name Code flag masking options
///@{
#define UDF_ATTR   0x0001   /** user defined word  */
#define IMM_ATTR   0x0002   /** immediate word     */
#if DO_WASM
#define MSK_ATTR   ~0x0     /** no masking         */
#define UDF_FLAG   0x8000   /** xt/pfa selector    */
#else  // !DO_WASM
#define MSK_ATTR   ~0x3
#define UDF_FLAG   0x0001   /** xt/pfa selector    */
#endif // DO_WASM

#define IS_UDF(w) (dict[w].attr & UDF_ATTR)
#define IS_IMM(w) (dict[w].attr & IMM_ATTR)
///@}
///
///> Universal functor (no STL) and Code class
///  Code class on 64-bit systems (expand pfa possible)
///  +-------------------+-------------------+
///  |    *name          |       xt          |
///  +-------------------+----+----+---------+
///                      |attr|pfa |xxxxxxxxx|
///                      +----+----+---------+
///  Code class on 32-bit systems (memory best utilized)
///  +---------+---------+
///  |  *name  |   xt    |
///  +---------+----+----+
///            |attr|pfa |
///            +----+----+
///  Code class on WASM/32-bit (a bit wasteful)
///  +---------+---------+---------+
///  |  *name  |   xt    |attr|xxxx|
///  +---------+----+----+---------+
///            |pfa |xxxx|
///            +----+----+
///
typedef void (*FPTR)();     ///< function pointer
struct Code {
    static UFP XT0;         ///< function pointer base (in registers hopefully)
    const char *name = 0;   ///< name field
#if DO_WASM
    union {
        FPTR xt = 0;        ///< WASM fptr is just index to vtable
        IU   pfa;           ///< i.e. no LSBs available
    };
    IU attr = 0;            ///< So, attributes need to be separated
#else // !DO_WASM
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< lambda pointer (4-byte align, 2 LSBs can be used for attr)
        struct {
            IU attr;        ///< steal 2 LSBs because xt is 4-byte aligned on 32-bit CPU
            IU pfa;         ///< offset to pmem space (16-bit for 64K range)
        };
    };
#endif // DO_WASM
    static FPTR XT(IU ix)   INLINE { return (FPTR)(XT0 + (UFP)ix); }
    static void exec(IU ix) INLINE { (*XT(ix))(); }

    Code(const char *n, FPTR fp, bool im) : name(n), xt(fp) {
        if ((UFP)xt < XT0) XT0 = (UFP)xt; ///> collect xt base
        if (im) attr |= IMM_ATTR;
#if CC_DEBUG > 1
		LOG_KX("XT0=", XT0);  LOG_KX(" xt=", (UFP)xt); 
		LOG_KX(", nm=", (UFP)n); LOGS(" "); LOGS(n); LOGS("\n");
#endif // CC_DEBUG > 1
    }
    Code() {}               ///< create a blank struct (for initilization)
    IU   xtoff() INLINE { return (IU)((UFP)xt - XT0); }  ///< xt offset in code space
    void call()  INLINE { (*(FPTR)((UFP)xt & MSK_ATTR))(); }
};
///
///> Add a Word to dictionary
/// Note:
///    a lambda without capture can degenerate into a function pointer
#define ADD_CODE(n, g, im) {    \
    Code c(n, []{ g; }, im);	\
    dict.push(c);               \
    }
#define CODE(n, g) ADD_CODE(n, g, false)
#define IMMD(n, g) ADD_CODE(n, g, true)

extern void forth_init();
extern void forth_vm(const char *cmd, void(*callback)(int, const char*));
extern int  forth_include(const char *fn);
;

#endif // __EFORTH_SRC_CEFORTH_H
