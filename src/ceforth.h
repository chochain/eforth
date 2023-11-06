#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <stdint.h>                          // uintxx_t
#include <exception>                         // try...catch, throw
#pragma GCC optimize("align-functions=4")    // we need fn alignment
///
/// Benchmark: 10K*10K cycles on desktop (3.2G AMD)
///    LAMBDA_OK       0 cut 80ms
///    RANGE_CHECK     0 cut 100ms
///    INLINE            cut 545ms
///
/// Note: use LAMBDA_OK=1 for full ForthVM class
///    if lambda needs to capture [this] for Code
///    * it slow down nest() by 2x (1200ms -> 2500ms) on AMD
///    * with one parameter, it slows 160ms extra
///
///@name Conditional compililation options
///@}
#define DO_WASM         0     /**< for WASM output                        */
#define LAMBDA_OK       0     /**< lambda support, set 1 for ForthVM.this */
#define RANGE_CHECK     0     /**< vector range check                     */
#define CC_DEBUG        0     /**< debug tracing flag                     */
#define INLINE          __attribute__((always_inline))
///@}
///@name Memory block configuation
///@{
#define E4_RS_SZ        64
#define E4_SS_SZ        64
#define E4_DICT_SZ      2048
#define E4_PMEM_SZ      (64*1024)
///@}
///@name Multi-platform support
///@{
#if    _WIN32 || _WIN64
#define ENDL "\r\n"
#else  // !(_WIN32 || _WIN64)
#define ENDL endl; fout_cb(fout.str().length(), fout.str().c_str()); fout.str("")
#endif // _WIN32 || _WIN64

#if    ARDUINO
    #include <Arduino.h>
    #define to_string(i)    string(String(i).c_str())
    #define LOGF(s)         Serial.print(F(s))
    #define LOG(v)          Serial.print(v)
    #define LOGX(v)         Serial.print(v, HEX)
    #define LOGN(v)         Serial.println(v)

    #if    ESP32
        #define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
    #endif // ESP32

#elif  __EMSCRIPTEN__
    #include <emscripten.h>
    #define millis()        EM_ASM_INT({ return Date.now(); })
    #define delay(ms)       EM_ASM({ let t = setTimeout(()=>clearTimeout(t), $0); }, ms)
    #define yield()

#else  // !ARDUINO && !__EMSCRIPTEN__
    #include <chrono>
    #include <thread>
    #define millis()        chrono::duration_cast<chrono::milliseconds>( \
                            chrono::steady_clock::now().time_since_epoch()).count()
    #define delay(ms)       this_thread::sleep_for(chrono::milliseconds(ms))
    #define yield()         this_thread::yield()
    #define PROGMEM
    #define LOGN(v)         printf("%d\n", v)

#endif // ARDUINO && __EMSCRIPTEN__
///@}
using namespace std;
///
///@name Logical units (instead of physical) for type check and portability
///@{
typedef uint32_t        U32;   ///< unsigned 32-bit integer
typedef uint16_t        U16;   ///< unsigned 16-bit integer
typedef uint8_t         U8;    ///< byte, unsigned character
typedef uintptr_t       UFP;   ///< function pointer as integer

#ifdef USE_FLOAT
typedef double          DU2;
typedef float           DU;
#define DVAL            0.0f
#define UINT(v)         (fabs(v)))

#else // !USE_FLOAT
typedef int64_t         DU2;
typedef int32_t         DU;
#define DVAL            0
#define UINT(v)         (abs(v))

#endif // USE_FLOAT
typedef uint16_t        IU;    ///< instruction pointer unit
///@}
///@name Alignment macros
///@{
#define ALIGN2(sz)      ((sz) + (-(sz) & 0x1))
#define ALIGN4(sz)      ((sz) + (-(sz) & 0x3))
#define ALIGN16(sz)     ((sz) + (-(sz) & 0xf))
#define ALIGN32(sz)     ((sz) + (-(sz) & 0x1f))
#define ALIGN(sz)       ALIGN2(sz)
#define STRLEN(s)       (ALIGN(strlen(s)+1))  /** calculate string size with alignment */
///@}
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
        v = N ? new T[N] : 0;                         ///< dynamically allocate array storage
        if (!v) throw "ERR: List allot failed";
    }
    ~List() { if (v) delete[] v;   }                  ///< free memory

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
/// Code flag masking options
///
#define WORD_NA        -1
#if DO_WASM                     /** WASM function ptr is not aligned */
    #define UDF_FLAG   0x8000   /** user defined word  */
    #define IMM_FLAG   0x4000   /** immediate word     */
    #define UDF_MASK   0x3fff   /** user defined word  */

#else // !DO_WASM
    #define UDF_FLAG   0x0001
    #define IMM_FLAG   0x0002
    #define UDF_MASK  ~0x0003

#endif // DO_WASM

///
/// universal functor (no STL) and Code class
/// Note:
///   * 8-byte on 32-bit machine, 16-byte on 64-bit machine
///
#if LAMBDA_OK

struct fop { virtual void operator()() = 0; };
template<typename F>
struct FP : fop {           ///< universal functor
    F fp;
    FP(F &f) INLINE : fp(f) {}
    void operator()() INLINE { fp(); }
};
typedef fop* FPTR;          ///< lambda function pointer
struct Code {
    static UFP XT0, NM0;
    const char *name = 0;   ///< name field
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< lambda pointer
        struct {            ///< a colon word
            U16 attr;       ///< colon defined word
            IU  pfa;        ///< offset to pmem space
        };
    };
    static FPTR XT(IU ix)   INLINE { return (FPTR)(XT0 + ((UFP)ix & UDF_MASK)); }
    static void exec(IU ix) INLINE { (*(FPTR)XT(ix))(); }
    template<typename F>    ///< template function for lambda
    Code(const char *n, F f, bool im) : name(n), xt(new FP<F>(f)) {
        if ((UFP)xt < XT0) XT0 = (UFP)xt;              ///> collect xt base
        if ((UFP)n  < NM0) NM0 = (UFP)n;               ///> collect name string base
        if (im) attr |= IMM_FLAG;
#if CC_DEBUG        
        printf("XT0=%lx xt=%lx %s\n", XT0, (UFP)xt, n);
#endif // CC_DEBUG
    }
    Code() {}               ///< create a blank struct (for initilization)
    IU xtoff() INLINE { return (IU)((UFP)xt - XT0); }  ///< xt offset in code space
};
#define ADD_CODE(n, g, im) {     \
    Code c(n, []() { g; }, im);  \
    dict.push(c);                \
    }

#else  // !LAMBDA_OK
///
/// a lambda without capture can degenerate into a function pointer
///
typedef void (*FPTR)();     ///< function pointer
struct Code {
    static UFP XT0, NM0;
    const char *name = 0;   ///< name field
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< lambda pointer
        struct {
            U16 attr;       ///< attributes (def, imm, xx=reserved)
            IU  pfa;        ///< offset to pmem space (16-bit for 64K range)
        };
    };
    static FPTR XT(IU ix)   INLINE { return (FPTR)(XT0 + ((UFP)ix & UDF_MASK)); }
    static void exec(IU ix) INLINE { (*(FPTR)XT(ix))(); }
    Code(const char *n, FPTR fp, bool im) : name(n), xt(fp) {
        if ((UFP)xt < XT0) XT0 = (UFP)xt;                ///> collect xt base
        if ((UFP)n  < NM0) NM0 = (UFP)n;                 ///> collect name string base
        if (im) attr |= IMM_FLAG;
#if CC_DEBUG        
        printf("XT0=%lx xt=%lx %s\n", XT0, (UFP)xt, n);
#endif // CC_DEBUG
    }
    Code() {}               ///< create a blank struct (for initilization)
    IU xtoff() INLINE { return (IU)((UFP)xt - XT0); }    ///< xt offset in code space
};
#define ADD_CODE(n, g, im) {    \
    Code c(n, []{ g; }, im);	\
    dict.push(c);               \
    }

#endif // LAMBDA_OK

#define CODE(n, g) ADD_CODE(n, g, false)
#define IMMD(n, g) ADD_CODE(n, g, true)

#define IS_UDF(w) (dict[w].attr & UDF_FLAG)
#define IS_IMM(w) (dict[w].attr & IMM_FLAG)

#endif // __EFORTH_SRC_CEFORTH_H
