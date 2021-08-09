#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <vector>           // vector
#include <functional>       // function

using namespace std;

template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern

    T& operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    T operator<<(T t)    { v.push_back(t); }

    T dec_i() { return v.back() -= 1; }     /// decrement stack top
    T pop()   {
        if (v.empty()) throw length_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t;
    }
    void push(T t)            { v.push_back(t); }
    void clear()              { v.clear(); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
    void erase(int i)         { v.erase(v.begin() + i, v.end()); }
};

class Code;                                 /// forward declaration
class ForthVM;
typedef function<void(ForthVM&,Code*)> fop; /// Forth operator

class Code {
public:
    static int fence;                       /// token incremental counter
    string name;                            /// name of word
    int    token = 0;                       /// dictionary order token
    bool   immd  = false;                   /// immediate flag
    int    stage = 0;                       /// branching stage
    string literal;                         /// string literal
    fop    xt    = NULL;                    /// primitive function

    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<float> qf;

    Code(string n, fop fn, bool im=false);  /// primitive
    Code(string n, bool f=false);           /// new colon word or temp
    Code(Code *c,  float d);                /// dolit, dovar
    Code(Code *c,  string s=string());      /// dotstr

    Code* immediate();                      /// set immediate flag
    Code* addcode(Code* w);                 /// append colon word

    string to_s();                          /// debugging
    void   see(int dp);
    void   exec(ForthVM &vm);               /// execute word
};
///
/// Forth virtual machine variables
///
class ForthVM {
public:
    ForthList<float> rs;                    /// return stack
    ForthList<float> ss;                    /// parameter stack
    ForthList<Code*> dict;                  /// dictionary

    bool  compile = false;                   /// compiling flag
    int   base    = 10;                      /// numeric radix
    float top     = -1;                      /// cached top of stack
    int   IP, WP;                            /// instruction and parameter pointers

    void init();
    void outer();
    
private:
    float POP();
    float PUSH(float v);
    
    Code *find(string s);                   /// search dictionary reversely
    string next_idiom(char delim=0);
    
    void dot_r(int n, float v);
    void ss_dump();
    void words();
};
#endif // __EFORTH_SRC_CEFORTH_H
