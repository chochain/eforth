#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <sstream>
#include <vector>           // vector
#include <functional>       // function
#include <exception>

#if _WIN32 || _WIN64
#define ENDL "\r\n"
#else
#define ENDL endl
#endif // _WIN32 || _WIN64

#if ARDUINO
#include <Arduino.h>
#define to_string(i)    string(String(i).c_str())
#if ESP32
#define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
#endif // ESP32
#else
#include <chrono>
#include <thread>
#define millis()        chrono::duration_cast<chrono::milliseconds>( \
                            chrono::steady_clock::now().time_since_epoch()).count()
#define delay(ms)       this_thread::sleep_for(chrono::milliseconds(ms))
#define yield()         this_thread::yield()
#endif // ARDUINO

using namespace std;

#ifdef USE_FLOAT
typedef float DTYPE;
#define DVAL  0.0f
#else // USE_FLOAT
typedef int   DTYPE;
#define DVAL  0
#endif // USE_FLOAT

template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern

    T& operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    ForthList& operator<<(T t){ v.push_back(t); return *this; }

    T dec_i() { return v.back() -= 1; }     /// decrement stack top
    T pop()   {
        if (v.empty()) throw underflow_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t;
    }
    int  size()               { return (int)v.size(); }
    void push(T t)            { v.push_back(t); }
    void clear()              { v.clear(); }
    void merge(ForthList& a)  { v.insert(v.end(), a.v.begin(), a.v.end()); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
    void erase(int i)         { v.erase(v.begin() + i, v.end()); }
};

class Code;
#if NO_STD_FUNCTION
struct fop {                                /// alternate solution for function
    virtual void operator()(Code*) = 0;
};
template<typename F>
struct XT : fop {
    F fp;
    XT(F &f) : fp(f) {}
    void operator()(Code *c) { fp(c); }
};
#else
using fop = function<void(Code*)>;          /// Forth operator
#endif // NO_STD_FUNCTION

class Code {
public:
    static int fence, IP;                   /// token incremental counter
    string name;                            /// name of word
    int    token = 0;                       /// dictionary order token
    bool   immd  = false;                   /// immediate flag
    int    stage = 0;                       /// branching stage
#if NO_STD_FUNCTION
    fop    *xt   = NULL;
#else // NO_STD_FUNCTION
    fop    xt    = NULL;                    /// primitive function
#endif // NO_STD_FUNCTION
    string literal;                         /// string literal

    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<DTYPE> qf;

#if NO_STD_FUNCTION
    template<typename F>
    Code(string n, F fn, bool im=false);    /// primitive
#else // NO_STD_FUNCTION
    Code(string n, fop fn, bool im=false);  /// primitive
#endif // NO_STD_FUNCTION
    Code(string n, bool f=false);           /// new colon word or temp
    Code(Code *c,  DTYPE d);                /// dolit, dovar
    Code(Code *c,  string s=string());      /// dotstr

    Code *immediate();                      /// set immediate flag
    Code *addcode(Code *w);                 /// append colon word

    string  to_s();                         /// debugging
    string  see(int dp);
    void    nest();                         /// execute word
};
///
/// Forth virtual machine variables
///
class ForthVM {
public:
    istream          &cin;                  /// stream input
    ostream          &cout;                 /// stream output

    ForthList<DTYPE> rs;                    /// return stack
    ForthList<DTYPE> ss;                    /// parameter stack
    ForthList<Code*> dict;                  /// dictionary

    bool  compile = false;                  /// compiling flag
    int   base    = 10;                     /// numeric radix
    int   WP      = 0;                      /// instruction and parameter pointers
    DTYPE top     = DVAL;                   /// cached top of stack

    ForthVM(istream &in, ostream &out);

    void init();
    void outer();

private:
    DTYPE POP();
    DTYPE PUSH(DTYPE v);

    Code *find(string s);                   /// search dictionary reversely
    string next_idiom(char delim=0);
    void call(Code *c);                     /// execute a word
    void call(ForthList<Code*>& pf);        /// execute a word

    void dot_r(int n, DTYPE v);
    void ss_dump();
    void words();
};
#endif // __EFORTH_SRC_CEFORTH_H
