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
/// universal functor (no STL) and Code class
/// Note:
///   * 8-byte on 32-bit machine, 16-byte on 64-bit machine
///
#if LAMBDA_OK
struct fop { virtual void operator()() = 0; };
template<typename F>
struct XT : fop {           // universal functor
    F fp;
    XT(F &f) : fp(f) {}
    void operator()() INLINE { fp(); }
};
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
typedef void (*fop)();      /// function pointer
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
///
/// macros to abstract dict and pmem physical implementation
/// Note:
///   so we can change pmem implementation anytime without affecting opcodes defined below
///
#define BOOL(f)   ((f)?-1:0)
#define PFA(w)    ((U8*)&pmem[dict[w].pfa]) /** parameter field pointer of a word        */
#define HERE      (pmem.idx)                /** current parameter memory index           */
#define XT(ipx)   (DICT0 + (S16)ipx)        /** fetch xt from dictionary offset          */
#define OFF(ip)   ((IU)((U8*)(ip) - MEM0))  /** IP offset (index) in parameter memory    */
#define MEM(ip)   (MEM0 + *(IU*)(ip))       /** pointer to IP address fetched from pmem  */
#define CELL(a)   (*(DU*)&pmem[a])          /** fetch a cell from parameter memory       */
#define SETJMP(a) (*(IU*)&pmem[a])          /** address offset for branching opcodes     */
///
/// opcode index
///
enum {
    EXIT = 0, DOVAR, DOLIT, DOSTR, DOTSTR, BRAN, ZBRAN, DONEXT, DOES, TOR
} forth_opcode;
///
/// global memory blocks
///
extern List<DU,   E4_SS_SZ>   rs;             /// return stack
extern List<DU,   E4_RS_SZ>   ss;             /// parameter stack
extern List<Code, E4_DICT_SZ> dict;           /// dictionary
extern List<U8,   E4_PMEM_SZ> pmem;           /// parameter memory (for colon definitions)
extern U8  *MEM0;                             /// base of cached memory
extern UFP DICT0;                             /// base of dictionary

class ForthVM {
public:
    istream &fin;                             /// VM stream input
    ostream &fout;                            /// VM stream output

    bool    compile = false;                  /// compiling flag
    bool    ucase   = true;                   /// case sensitivity control
    DU      base    = 10;                     /// numeric radix
    DU      top     = DVAL;                   /// top of stack (cached)
    IU      WP      = 0;                      /// current word
    U8      *IP;                              /// current intruction pointer

    string  idiom;

    ForthVM(istream &in, ostream &out) : fin(in), fout(out) {}

    void init();
    void outer();

private:
    ///
    /// dictionary search methods
    ///
    int   pfa2word(U8 *ip);
    int   streq(const char *s1, const char *s2);
    int   find(const char *s);
    int   find(string &s) { return find(s.c_str()); }
    ///
    /// compiler methods
    ///
    void  colon(const char *name);
    void  colon(string &s) { colon(s.c_str()); }
    void  add_iu(IU i) { pmem.push((U8*)&i, sizeof(IU));  dict[-1].len += sizeof(IU); }  /** add an instruction into pmem */
    void  add_du(DU v) { pmem.push((U8*)&v, sizeof(DU)),  dict[-1].len += sizeof(DU); }  /** add a cell into pmem         */
    void  add_str(const char *s) {                                               /** add a string to pmem         */
        int sz = STRLEN(s); pmem.push((U8*)s,  sz); dict[-1].len += sz;
    }
    void  add_w(IU w) {
        Code *c  = &dict[w];
        IU   ipx = c->def ? (c->pfa | 1) : (IU)((UFP)c->xt - DICT0);
        add_iu(ipx);
        printf("add_w(%d) => %4x:%p %s\n", w, ipx, c->xt, c->name);
    }
    ///
    /// inner interpreter ops
    ///
    void  PUSH(DU v) { ss.push(top); top = v; }
    DU    POP()      { DU n = top; top = ss.pop(); return n; }
    void  call(IU w);
    void  nest();
    ///
    /// IO & debug methods
    ///
    string &next_idiom(char c=0);
    void  dot_r(int n, int v);
    void  to_s(IU c);
    void  see(U8 *ip, int dp=1);
    void  words();
    void  ss_dump();
    void  mem_dump(IU p0, DU sz);
};
#endif // __EFORTH_SRC_CEFORTH_H
