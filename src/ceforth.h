///
/// @file
/// @brief eForth header - C++ vector-based, token-threaded
///
///====================================================================
#ifndef __EFORTH_SRC_CEFORTH_H
#define __EFORTH_SRC_CEFORTH_H
#include <iostream>                    /// cin, cout
#include <iomanip>                     /// setbase
#include <vector>                      /// vector
#include <chrono>
#include "config.h"
using namespace std;

template<typename T>
struct FV : public vector<T> {         ///< our super-vector class
    FV()                        : vector<T>()    {}
    FV(initializer_list<T> lst) : vector<T>(lst) {}
    FV *merge(FV<T> &v) {
        this->insert(this->end(), v.begin(), v.end()); v.clear(); return this;
    }
    T    dec_i()   { return (this->back() -= 1); }
    void push(T n) { this->push_back(n); }
    T    pop()     { T n = this->back(); this->pop_back(); return n; }
    T    operator[](int i) { return this->at(i < 0 ? (this->size() + i) : i); }
};
///
///> data structure for dictionary entry
///
struct Code;                 ///< forward declaration
typedef void (*XT)(Code*);   ///< function pointer
struct Code {
    static int here;         ///< token incremental counter
    string    name;          ///< name of word
    XT        xt    = NULL;  ///< execution token
    int       token = 0;     ///< dict index, 0=param word
    bool      immd  = false; ///< immediate flag
    int       stage = 0;     ///< branching stage (looping condition)
    FV<Code*> pf;            ///< parameter field
    FV<Code*> p1;            ///< parameter field - if..else, aft..then
    FV<Code*> p2;            ///< parameter field - then..next
    FV<DU>    q;             ///< parameter field - literal
    Code(string n, XT fp, bool im)        ///> primitive
        : name(n), xt(fp), immd(im), token(here++) {}
    Code(string n, bool f=false);         ///> colon word, f=new word
    Code(string n, DU d);                 ///> dolit, dovar
    Code(string n, string s);             ///> dostr, dotstr
    Code *immediate()  { immd = true; return this; }  ///> set flag
    Code *add(Code *w) { pf.push(w);  return this; }  ///> add token
    void exec() {                         ///> inner interpreter
        if (xt) { xt(this); return; }     /// * run primitive word
        for (Code *w : pf) {              /// * run colon word
            try { w->exec(); }            /// * execute recursively
            catch (...) { break; }        /// * also handle exit
        }
    }
};
///
///> OS platform specific implementation
///
extern void mem_stat();
extern void forth_include(const char *fn);

#endif  // __EFORTH_SRC_CEFORTH_H
