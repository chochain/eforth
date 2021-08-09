#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <iostream>         // cin, cout
#include <iomanip>          // setbase
#include <vector>           // vector
#include <functional>       // function

using namespace std;

template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern
    T& operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    T operator<<(T t) { v.push_back(t); }
    T pop() {
        if (v.empty()) throw length_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t;
    }
    T dec_i() { return v.back() -= 1; }     /// decrement stack top
    void push(T t) { v.push_back(t); }
    void clear() { v.clear(); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
    void erase(int i) { v.erase(v.begin() + i, v.end()); }
};

class Code;                                 /// forward declaration
typedef function<void(Code*)> fop;          /// Forth operator

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
    ForthList<int>   qf;

    Code(string n, fop fn, bool im = false);/// primitive
    Code(string n, bool f = false);         /// new colon word or temp
    Code(string n, int d);                  /// dolit
    Code(string n, string l);               /// dostr

    Code* immediate();                      /// set immediate flag
    Code* addcode(Code* w);                 /// append colon word

    string to_s();                          /// debugging
    void   see(int dp);
    void   exec();                          /// execute word
};

class ForthVM {
public:
    /// Forth virtual machine variables
    ForthList<int>   rs;                    /// return stack
    ForthList<int>   ss;                    /// parameter stack
    ForthList<Code*> dict;                  /// dictionary
    bool compile = false;                   /// compiling flag
    int  base    = 10;                      /// numeric radix
    int  top     = -1;                      /// cached top of stack
    int  IP, WP;                            /// instruction and parameter pointers

    void init();
    void outer();
    
private:
    int POP();
    int PUSH(int v);
    
    Code *find(string s);                   /// search dictionary reversely
    string next_idiom(char delim=0);
    
    void dot_r(int n, string s);
    void ss_dump();
    void words();
};
#endif // __EFORTH_SRC_CEFORTH_H
