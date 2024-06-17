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
    void push(T n) { this->push_back(n); }
    T    pop()     { T n = this->back(); this->pop_back(); return n; }
    T    &operator[](int i) { return this->at(i < 0 ? (this->size() + i) : i); }
};
///
///> Primitve object and function forward declarations
///
struct Code;                 ///< Code class forward declaration
typedef void (*XT)(Code*);   ///< function pointer

void   _str(Code *c);        ///< dotstr, dostr
void   _lit(Code *c);        ///< numeric liternal
void   _var(Code *c);        ///< variable and constant
void   _tor(Code *c);        ///< >r (for..next)
void   _dor(Code *c);        ///< swap >r >r (do..loop)
void   _bran(Code *c);       ///< if
void   _cycle(Code *c);      ///< repeat
void   _for(Code *c);        ///< for...next
void   _doloop(Code *c);     ///< do...loop
void   _does(Code *c);       ///< does> 
///
///> IO function declarations
///
string word(char delim=0);   ///< read next idiom from input stream
void   ss_dump(DU base);     ///< display data stack contents
void   see(Code *c, int dp); ///< disassemble word
void   words();              ///< list words in dictionary
void   load(const char *fn); ///< include script from stream
Code   *find(string s);      ///< dictionary scanner forward declare
///
///> data structure for dictionary entry
///
struct Code {
    string    name;          ///< name of word
    XT        xt = NULL;     ///< execution token
    FV<Code*> pf;            ///< parameter field
    FV<Code*> p1;            ///< parameter field - if..else, aft..then
    FV<Code*> p2;            ///< parameter field - then..next
    FV<DU>    q;             ///< parameter field - literal
    union {                  ///< union to reduce struct size
        U32 attr = 0;        /// * zero all sub-fields
        struct {
            U32 token : 28;  ///< dict index, 0=param word
            U32 stage :  2;  ///< branching state
            U32 is_str:  1;  ///< string flag
            U32 immd  :  1;  ///< immediate flag
        };
    };
    Code(string n, XT fp, bool im);       ///> primitive 
    Code(string n, bool t=true);          ///> colon word
    ~Code() {}                            ///> do nothing now
    
    Code *immediate()  { immd = 1;   return this; }   ///> set flag
    Code *add(Code *w) { pf.push(w); return this; }   ///> add token
    void exec() {                         ///> inner interpreter
        if (xt) { xt(this); return; }     /// * run primitive word
        for (Code *w : pf) {              /// * run colon word
            try { w->exec(); }            /// * execute recursively
            catch (...) { break; }        /// * also handle exit
        }
    }
};
///
///> polymophic constructors
///
struct Tmp : Code { Tmp() : Code(" tmp", NULL, false) { token=0;   } };
struct Lit : Code { Lit(DU d) : Code("", _lit, false) { q.push(d); } };
struct Var : Code { Var(DU d) : Code("", _var, false) { q.push(d); } };
struct Str : Code {
    Str(string s, int t=0) : Code(s, _str, false) { token=t; is_str=1; }
};
struct Bran: Code {
    Bran(XT fp) : Code("", fp, false) {
        const char *nm[] = {
            "_if", "_begin", ">r", "_for", "swap >r >r", "_do", "_does"
        };
        XT xt[] = { _bran, _cycle, _tor, _for, _dor, _doloop, _does };
    
        for (int i=0; i < (int)(sizeof(nm)/sizeof(const char*)); i++) {
            if ((uintptr_t)xt[i]==(uintptr_t)fp) name = nm[i];
        }
        token = is_str = 0;
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
