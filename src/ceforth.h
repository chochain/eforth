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
struct Code;                 ///< Code class forward declaration
Code   *find(string s);      ///< dictionary scanner forward declare
typedef void (*XT)(Code*);   ///< function pointer
struct Code {
    static int here;         ///< token incremental counter
    string    name;          ///< name of word
    XT        xt = NULL;     ///< execution token
    FV<Code*> pf;            ///< parameter field
    FV<Code*> p1;            ///< parameter field - if..else, aft..then
    FV<Code*> p2;            ///< parameter field - then..next
    FV<DU>    q;             ///< parameter field - literal
    union {                  ///< union to reduce struct size
        int attr = 0;        /// * zero all sub-fields
        struct {
            U32 token : 28;  ///< dict index, 0=param word
            U32 stage :  2;  ///< branching state
            U32 str   :  1;  ///< string node
            U32 immd  :  1;  ///< immediate flag
        };
    };
    Code(string n, XT fp, bool im) : name(n), xt(fp), immd(im), token(here++) {} ///> primitive
    Code(string n, bool t=true);                                                 ///> colon word, t=new word
    Code(XT fp, string s)          : name(s), xt(fp), token(0), str(0) {}        ///> bran, cycle, for, does>
    Code(XT fp, string s, int t)   : name(s), xt(fp), token(t), str(1) {}        ///> dostr, dotstr
    Code(XT fp, DU d)              : name(""), xt(fp) { q.push(d); }             ///> dolit, dovar
    Code *immediate()  { immd = 1;   return this; } ///> set flag
    Code *add(Code *w) { pf.push(w); return this; } ///> add token
    void exec() {                         ///> inner interpreter
        if (xt) { xt(this); return; }     /// * run primitive word
        for (Code *w : pf) {              /// * run colon word
            try { w->exec(); }            /// * execute recursively
            catch (...) { break; }        /// * also handle exit
        }
    }
};
///
///> macros to create primitive words (opcodes)
///
#define CODE(s, g)  (new Code(s, [](Code *c){ g; }, false))
#define IMMD(s, g)  (new Code(s, [](Code *c){ g; }, true))
///
///> OS platform specific implementation
///
extern void mem_stat();
extern void forth_include(const char *fn);

#endif  // __EFORTH_SRC_CEFORTH_H
