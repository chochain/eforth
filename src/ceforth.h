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
    FV *merge(FV<T> &v) {
        this->insert(this->end(), v.begin(), v.end()); v.clear(); return this;
    }
    void push(T n) { this->push_back(n); }
    T    pop()     { T n = this->back(); this->pop_back(); return n; }
    T    &operator[](int i) {
#if CC_DEBUG
        return this->at(i < 0 ? (this->size() + i) : i); // with range checked
#else  // !CC_DEBUG
        return vector<T>::operator[](i < 0 ? (this->size() + i) : i);
#endif // CC_DEBUG
    }
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
void   _tor2(Code *c);       ///< swap >r >r (do..loop)
void   _if(Code *c);         ///< if..then, if..else..then
void   _begin(Code *c);      ///< ..until, ..again, ..while..repeat
void   _for(Code *c);        ///< for..next, for..aft..then..next
void   _loop(Code *c);       ///< do..loop
void   _does(Code *c);       ///< does> 
///
///> IO function declarations
///
string word(char delim=0);   ///< read next idiom from input stream
void   ss_dump(DU base);     ///< display data stack contents
void   see(Code *c);         ///< disassemble word
void   words();              ///< list words in dictionary
void   load(const char *fn); ///< include script from stream
Code   *find(string s);      ///< dictionary scanner forward declare
///
///> data structure for dictionary entry
///
struct Code {
    const static U32 IMMD_FLAG = 0x80000000;
    const char *name;        ///< name of word
    const char *desc;
    XT         xt = NULL;    ///< execution token
    FV<Code*>  pf;           ///< parameter field
    FV<Code*>  p1;           ///< parameter field - if..else, aft..then
    FV<Code*>  p2;           ///< parameter field - then..next
    FV<DU>     q;            ///< parameter field - literal
    union {                  ///< union to reduce struct size
        U32 attr = 0;        /// * zero all sub-fields
        struct {
            U32 token : 28;  ///< dict index, 0=param word
            U32 stage :  2;  ///< branching state
            U32 is_str:  1;  ///< string flag
            U32 immd  :  1;  ///< immediate flag
        };
    };
    Code(const char *s, const char *d, XT fp, U32 a);  ///> primitive
    Code(const string s, bool n=true);                 ///> colon, n=new word
    Code(XT fp) : name(""), xt(fp), attr(0) {}         ///> sub-classes
    ~Code() {}                            ///> do nothing now
    
    Code *append(Code *w) { pf.push(w); return this; } ///> add token
    void exec() {                         ///> inner interpreter
        if (xt) { xt(this); return; }     /// * run primitive word
        for (Code *w : pf) {              /// * run colon word
            try { w->exec(); }            /// * execute recursively
            catch (...) { break; }        /// * break loop with throw 0
        }
    }
};
///
///> polymorphic constructors
///
struct Tmp : Code { Tmp() : Code(NULL) {} };
struct Lit : Code { Lit(DU d) : Code(_lit) { q.push(d); } };
struct Var : Code { Var(DU d) : Code(_var) { q.push(d); } };
struct Str : Code {
    Str(string s, int tok=0, int len=0) : Code(_str) {
        name  = (new string(s))->c_str();
        token = (len << 16) | tok;        /// * encode word index and string length
        is_str= 1;
    }
};
struct Bran: Code {
    Bran(XT fp) : Code(fp) {
        const char *nm[] = {
            "if", "begin", "\t", "for", "\t", "do", "does>"
        };
        XT xt[] = { _if, _begin, _tor, _for, _tor2, _loop, _does };
    
        for (int i=0; i < (int)(sizeof(nm)/sizeof(const char*)); i++) {
            if ((uintptr_t)xt[i]==(uintptr_t)fp) name = nm[i];
        }
        is_str = 0;
    }
};
///
///> OS platform specific implementation
///
extern void mem_stat();
extern void forth_include(const char *fn);

#endif  // __EFORTH_SRC_CEFORTH_H
