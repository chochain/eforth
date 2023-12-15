#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <stdio.h>
#include <stdint.h>                          // uintxx_t
#include <exception>                         // try...catch, throw
#pragma GCC optimize("align-functions=4")    // we need fn alignment
///
/// Benchmark: 10K*10K cycles on desktop (3.2G AMD)
///    RANGE_CHECK     0 cut 100ms
///    INLINE            cut 545ms
///
///@name Conditional compililation options
///@}
#define CC_DEBUG        0               /**< debug tracing flag  */
#define RANGE_CHECK     0               /**< vector range check  */
#define DO_WASM         __EMSCRIPTEN__  /**< for WASM output     */
///@}
///@name Memory block configuation
///@{
#define E4_RS_SZ        64
#define E4_SS_SZ        64
#define E4_DICT_SZ      1024
#define E4_PMEM_SZ      (32*1024)
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
    #define LOGS(s)         Serial.print(F(s))
    #define LOG(v)          Serial.print(v)
    #define LOGX(v)         Serial.print(v, HEX)
    #if    ESP32
        #define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
    #endif // ESP32

#elif  DO_WASM
    #include <emscripten.h>
    #define millis()        EM_ASM_INT({ return Date.now(); })
    #define delay(ms)       EM_ASM({ let t = setTimeout(()=>clearTimeout(t), $0); }, ms)
    #define yield()

#else  // !ARDUINO && !DO_WASM
    #include <chrono>
    #include <thread>
    #define millis()        chrono::duration_cast<chrono::milliseconds>( \
                            chrono::steady_clock::now().time_since_epoch()).count()
    #define delay(ms)       this_thread::sleep_for(chrono::milliseconds(ms))
    #define yield()         this_thread::yield()
    #define PROGMEM

#endif // ARDUINO && DO_WASM
///@}
///@name Debugging support
///@{
#if CC_DEBUG
#if ARDUINO
    #define LOG_KV(k, v)    LOGS(k); LOG(v)
    #define LOG_KX(k, x)    LOGS(k); LOGX(v)
    #define LOG_HDR(f, s)   LOGS(f); LOGS("("); LOGS(s); LOGS(") => ")
    #define LOG_DIC(i)      LOGS("dict["); LOG(i); LOGS("] "); \
                            LOG(dict[i].name); LOGS(" attr="); LOGX(dict[i].attr); \
                            LOGS("\n")
    #define LOG_NA()        LOGS("not found\n")

#else  // !ARDUINO
    #define LOG_KV(k, v)    printf("%s%d", k, v)
    #define LOG_KX(k, x)    printf("%s%x", k, x)
    #define LOG_HDR(f, s)   printf("%s(%s) => ", f, s)
    #define LOG_DIC(i)      printf("dict[%d] %s attr=%x\n", i, dict[i].name, dict[i].attr)
    #define LOG_NA()        printf("not found\n")

#endif // ARDUINO

#else  // !CC_DEBUG
    #define LOG_KV(k, v)
    #define LOG_KX(k, x)
    #define LOG_HDR(f, s)
    #define LOG_DIC(i)
    #define LOG_NA()

#endif // CC_DEBUG
///@}
using namespace std;
///
///@name Logical units (instead of physical) for type check and portability
///@{
typedef uint32_t        U32;   ///< unsigned 32-bit integer
typedef int32_t         S32;   ///< signed 32-bit integer
typedef uint16_t        U16;   ///< unsigned 16-bit integer
typedef uint8_t         U8;    ///< byte, unsigned character
typedef uintptr_t       UFP;   ///< function pointer as integer

#ifdef USE_FLOAT
typedef double          DU2;
typedef float           DU;
#define DU0             0.0f
#define UINT(v)         (fabs(v)))

#else // !USE_FLOAT
typedef int64_t         DU2;
typedef int32_t         DU;
#define DU0             0
#define UINT(v)         (abs(v))

#endif // USE_FLOAT
typedef uint16_t        IU;    ///< instruction pointer unit
///@}
///@name Inline & Alignment macros
///@{
#define INLINE          __attribute__((always_inline))
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
#define WORD_NA    -1
#if !DO_WASM                      /** WASM function ptr is index to vtable */
    #define UDF_ATTR   0x0001     /** user defined word  */
    #define IMM_ATTR   0x0002     /** immediate word     */
    #define MSK_ATTR   ~0x3       /** attribute mask     */
    #define UDF_FLAG   0x0001
#else // DO_WASM
    #define UDF_ATTR   0x8000     /** user defined word  */
    #define IMM_ATTR   0x4000     /** immediate word     */
    #define MSK_ATTR   0x3fffffff /** attribute mask     */
    #define UDF_FLAG   0x8000     /** colon word flag    */
#endif // !DO_WASM

#define IS_UDF(w) (dict[w].attr & UDF_ATTR)
#define IS_IMM(w) (dict[w].attr & IMM_ATTR)
///@}
///
///> Universal functor (no STL) and Code class
///
typedef void (*FPTR)();     ///< function pointer
struct Code {
    static UFP XT0, NM0;
    const char *name = 0;   ///< name field
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< lambda pointer (4-byte align, 2 LSBs can be used for attr)
#if !DO_WASM
        struct {
            IU attr;        ///< steal 2 LSBs because xt is 4-byte aligned on 32-bit CPU
            IU pfa;         ///< offset to pmem space (16-bit for 64K range)
        };
#else // DO_WASM
        struct {
            IU pfa;         ///< offset to pmem space (16-bit for 64K range)
            IU attr;        ///< WASM xt is index to vtable (so LSBs will be used)
        };
#endif // !DO_WASM
    };

    static FPTR XT(IU ix)   INLINE { return (FPTR)(XT0 + (UFP)ix); }
    static void exec(IU ix) INLINE { (*XT(ix))(); }
    
    Code(const char *n, FPTR fp, bool im) : name(n), xt(fp) {
        if (((UFP)xt - 4) < XT0) XT0 = ((UFP)xt - 4);    ///> collect xt base (4 prevent dXT==0)
        if ((UFP)n  < NM0) NM0 = (UFP)n;                 ///> collect name string base
        if (im) attr |= IMM_ATTR;
#if CC_DEBUG > 1
        printf("XT0=%lx xt=%lx %s\n", XT0, (UFP)xt, n);
#endif // CC_DEBUG
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
#define WORD_NULL  (FPTR)0     /** blank function pointer */

#define CODE(n, g) ADD_CODE(n, g, false)
#define IMMD(n, g) ADD_CODE(n, g, true)

#endif // __EFORTH_SRC_CEFORTH_H
