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
#if    ESP32
#define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
#endif // ESP32
#else  // !ARDUINO
#include <chrono>
#include <thread>
#define millis()        chrono::duration_cast<chrono::milliseconds>( \
                            chrono::steady_clock::now().time_since_epoch()).count()
#define delay(ms)       this_thread::sleep_for(chrono::milliseconds(ms))
#define yield()         this_thread::yield()
#define PROGMEM
#endif // ARDUINO
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
#else // USE_FLOAT
typedef int64_t         DU2;
typedef int32_t         DU;
#define DVAL            0
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
    	v = N ? new T[N] : 0;                      ///< dynamically allocate array storage
    	if (!v) throw "ERR: List allot failed";
    }
    ~List() { if (v) delete[] v;   }               ///< free memory

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
    T push(T t) INLINE { return v[max=idx++] = t; }
#endif // RANGE_CHECK
    void push(T *a, int n) INLINE { for (int i=0; i<n; i++) push(*(a+i)); }
    void merge(List& a)    INLINE { for (int i=0; i<a.idx; i++) push(a[i]);}
    void clear(int i=0)    INLINE { idx=i; }
};
///
/// universal functor (no STL) and Code class
/// Note:
///   * 8-byte on 32-bit machine, 16-byte on 64-bit machine
///
#if LAMBDA_OK
struct fop { virtual void operator()() = 0; };
template<typename F>
struct XT : fop {           ///< universal functor
    F fp;
    XT(F &f) : fp(f) {}
    void operator()() INLINE { fp(); }
};
typedef fop* FPTR;          ///< lambda function pointer
struct Code {
    const char *name = 0;   ///< name field
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< lambda pointer
        struct {            ///< a colon word
            U16 def:  1;    ///< colon defined word
            U16 immd: 1;    ///< immediate flag
            U16 len:  14;   ///< len of pfa
            IU  pfa;        ///< offset to pmem space
        };
    };
    template<typename F>    ///< template function for lambda
    Code(const char *n, F f, bool im=false) : name(n), xt(new XT<F>(f)) {
        immd = im ? 1 : 0;
    }
    Code() {}               ///< create a blank struct (for initilization)
};
#define CODE(s, g) { s, []() { g; }}
#define IMMD(s, g) { s, []() { g; }, true }
#else  // !LAMBDA_OK
///
/// a lambda without capture can degenerate into a function pointer
///
typedef void (*FPTR)();     ///< function pointer
struct Code {
    const char *name = 0;   ///< name field
    union {                 ///< either a primitive or colon word
        FPTR xt = 0;        ///< lambda pointer
        struct {            ///< a colon word
            U16 def:  1;    ///< colon defined word
            U16 immd: 1;    ///< immediate flag
            U16 len:  14;   ///< reserved
            IU  pfa;        ///< offset to pmem space (16-bit for 64K range)
        };
    };
    Code(const char *n, FPTR f, bool im=false) : name(n), xt(f) {
        immd = im ? 1 : 0;
    }
    Code() {}               ///< create a blank struct (for initilization)
};
#define CODE(s, g) { s, []{ g; }}
#define IMMD(s, g) { s, []{ g; }, true }
#endif // LAMBDA_OK
///
/// Forth Virtual Machine (front-end proxy) class
///
class ForthVM {
public:
    void init();
    void outer(const char *cmd, void(*callback)(int, const char*));

    const char *version();

    void mem_stat();
    void dict_dump();
};
#endif // __EFORTH_SRC_CEFORTH_H
