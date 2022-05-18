#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <memory>           // shared_ptr, make_shared
#include <sstream>          // istream, ostream
#include <exception>        // try...catch, throw
#include <string.h>         // strlen, strcasecmp
///
/// conditional compililation options
///
#define LAMBDA_OK       1
#define RANGE_CHECK     0
#define INLINE          __attribute__((always_inline))
///
/// configuation
///
#define E4_SS_SZ        64
#define E4_RS_SZ        64
#define E4_DICT_SZ      1024
#define E4_PMEM_SZ      (48*1024)
///
/// multi-platform support
///
#if _WIN32 || _WIN64
#define ENDL "\r\n"
#else  // _WIN32 || _WIN64
#define ENDL endl
#endif // _WIN32 || _WIN64

#if ARDUINO
#include <Arduino.h>
#define to_string(i)    string(String(i).c_str())
#if ESP32
#define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
#endif // ESP32
#else  // ARDUINO
#include <chrono>
#include <thread>
#define millis()        chrono::duration_cast<chrono::milliseconds>( \
                            chrono::steady_clock::now().time_since_epoch()).count()
#define delay(ms)       this_thread::sleep_for(chrono::milliseconds(ms))
#define yield()         this_thread::yield()
#define PROGRAM
#endif // ARDUINO

using namespace std;
///
/// logical units (instead of physical) for type check and portability
///
typedef uint32_t        U32;   // unsigned 32-bit integer
typedef uint16_t        U16;   // unsigned 16-bit integer
typedef uint8_t         U8;    // byte, unsigned character
typedef uintptr_t       UFP;
#ifdef USE_FLOAT
typedef float           DU;
#define DVAL            0.0f
#else // USE_FLOAT
typedef int32_t         DU;
#define DVAL            0
#endif // USE_FLOAT
typedef uint16_t        IU;    // instruction pointer unit
///
/// alignment macros
///
#define ALIGN2(sz)      ((sz) + (-(sz) & 0x1))
#define ALIGN4(sz)      ((sz) + (-(sz) & 0x3))
#define ALIGN16(sz)     ((sz) + (-(sz) & 0xf))
#define ALIGN32(sz)     ((sz) + (-(sz) & 0x1f))
#define ALIGN(sz)       ALIGN2(sz)
#define STRLEN(s)       (ALIGN(strlen(s)+1))  /** calculate string size with alignment */
///
/// array class template (so we don't have dependency on C++ STL)
/// Note:
///   * using decorator pattern
///   * this is similar to vector class but much simplified
///
template<class T, int N=0>
struct List {
    T   *v;             /// fixed-size array storage
    int idx = 0;        /// current index of array
    int max = 0;        /// high watermark for debugging

    List()  { v = N ? new T[N] : 0; }  /// dynamically allocate array storage
    ~List() { delete[] v;   }          /// free memory

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
#else  // RANGE_CHECK
    T pop()     INLINE { return v[--idx]; }
    T push(T t) INLINE { return v[max=idx++] = t; }
#endif // RANGE_CHECK
    void push(T *a, int n) INLINE { for (int i=0; i<n; i++) push(*(a+i)); }
    void merge(List& a)    INLINE { for (int i=0; i<a.idx; i++) push(a[i]);}
    void clear(int i=0)    INLINE { idx=i; }
};
///
/// functor implementation - for lambda support (without STL)
///
#if LAMBDA_OK
struct fop { virtual void operator()() = 0; };
template<typename F>
struct XT : fop {           // universal functor
    F fp;
    XT(F &f) : fp(f) {}
    void operator()() INLINE { fp(); }
};
#else  // LAMBDA_OK
typedef void (*fop)();
#endif // LAMBDA_OK

///
/// universal Code class
/// Note:
///   * 8-byte on 32-bit machine, 16-byte on 64-bit machine
///
#if LAMBDA_OK
struct Code {
    const char *name = 0;   /// name field
    union {                 /// either a primitive or colon word
        fop *xt = 0;        /// lambda pointer
        struct {            /// a colon word
            U16 def:  1;    /// colon defined word
            U16 immd: 1;    /// immediate flag
            U16 len:  14;   /// len of pfa
            IU  pfa;         /// offset to pmem space
        };
    };
    template<typename F>    /// template function for lambda
    Code(const char *n, F f, bool im=false) : name(n) {
        xt = new XT<F>(f);
        immd = im ? 1 : 0;
    }
    Code() {}               /// create a blank struct (for initilization)
};
#else  // LAMBDA_OK
struct Code {
    const char *name = 0;   /// name field
    union {                 /// either a primitive or colon word
        fop xt = 0;         /// lambda pointer
        struct {            /// a colon word
            U16 def:  1;    /// colon defined word
            U16 immd: 1;    /// immediate flag
            U16 len:  14;   /// len of pf (16K max)
            IU  pfa;        /// offset to pmem space (16-bit for 64K range)
        };
    };
    Code(const char *n, fop f, bool im=false) : name(n), xt(f) {
        immd = im ? 1 : 0;
    }
    Code() {}               /// create a blank struct (for initilization)
};
#endif // LAMBDA_OK
///
/// Forth virtual machine class
///
class ForthVM {
public:
    ForthVM(istream &in, ostream &out);

    void init();
    void outer();
};
#endif // __EFORTH_SRC_CEFORTH_H
