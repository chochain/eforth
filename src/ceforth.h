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
    FV &merge(FV<T> &v) {
        this->insert(this->end(), v.begin(), v.end()); v.clear(); return *this;
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
typedef int (*XT)(Code*);    ///< function pointer

int   _str(Code *c);         ///< dotstr, dostr
int   _lit(Code *c);         ///< numeric liternal
int   _var(Code *c);         ///< variable and constant
int   _tor(Code *c);         ///< >r (for..next)
int   _dor(Code *c);         ///< swap >r >r (do..loop)
int   _bran(Code *c);        ///< if..then, if..else..then
int   _cycle(Code *c);       ///< begin..repeat, begin..while..repeat
int   _for(Code *c);         ///< for..next
int   _doloop(Code *c);      ///< do..loop
int   _does(Code *c);        ///< create..does>..
///
///> IO function declarations
///
string word(char delim=0);   ///< read next idiom from input stream
void   ss_dump(DU base);     ///< display data stack contents
string to_s(Code &c);        ///< display a Code node
void   see(Code &c, int dp); ///< disassemble word
void   words();              ///< list words in dictionary
void   load(const char *fn); ///< include script from stream
Code   *find(string s);      ///< dictionary scanner forward declare
///
///> data structure for dictionary entry
///
struct Code {
    const static U32 IMMD_FLAG = 0x80000000;
    const char *name;        ///< name of word
    XT         xt = NULL;    ///< execution token
    FV<Code>   pf;           ///< parameter field
    FV<Code>   p1;           ///< parameter field - if..else, aft..then
    FV<Code>   p2;           ///< parameter field - then..next
    FV<DU>     q;            ///< parameter field - literal
    union {                  ///< union to reduce struct size
        U32 attr = 0;        /// * zero all sub-fields
        struct {
            U32 token : 27;  ///< dict index, 0=param word
            U32 eop   :  1;  ///< stop bit (end of parameter array)
            U32 stage :  2;  ///< branching state
            U32 is_str:  1;  ///< string flag
            U32 immd  :  1;  ///< immediate flag
        };
    };
    static int dolist(Code *w) {
        printf("%p> %s xt=%p t=%x\n", w, w->name, w->xt, w->attr);
        if (w->exec()) return 0;          /// * walk the tree
        return dolist(w + 1);             /// * tail recursion => jmp
    }
    Code(const char *s, XT fp, U32 a);    ///> primitive
    Code(const string s, bool n=true);    ///> colon, n=new word
    Code(XT fp)                           ///> sub-classes
        : name(""), xt(fp), attr(0) {}
    ~Code() {}                            ///> do nothing now
    
    Code &append(Code &w) { pf.push(w);     return *this; } ///> add token
    Code &stop()          { pf[-1].eop = 1; return *this; } ///> mark end of parameter array
    int exec() {                               ///> inner interpreter
        if (xt) { return xt(this) || eop ; }   /// * run primitive word
        return Code::dolist(pf.data());        /// * tail call => jmp
    }
};
///
///> polymophic constructors
///
struct Tmp : Code { Tmp() : Code(NULL) {} };
struct Lit : Code { Lit(DU d) : Code(_lit) { q.push(d); } };
struct Var : Code { Var(DU d) : Code(_var) { q.push(d); } };
struct Str : Code {
    Str(string s, int t=0) : Code(_str) {
        name  = (new string(s))->c_str();
        token = t;
        is_str= 1;
    }
};
struct Bran: Code {
    Bran(XT fp) : Code(fp) {
        const char *nm[] = {
            "_if", "_begin", ">r", "_for", "swap >r >r", "_do", "_does"
        };
        XT vt[] = { _bran, _cycle, _tor, _for, _dor, _doloop, _does };

        for (int i=0; i < (int)(sizeof(nm)/sizeof(const char*)); i++) {
            if ((uintptr_t)vt[i]==(uintptr_t)fp) name = nm[i];
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
