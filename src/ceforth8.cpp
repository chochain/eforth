///
/// ceForth8 experimental
///
#include <stdint.h>     // uintxx_t
#include <stdlib.h>     // strtol
#include <string.h>     // strcmp
#include <exception>    // try...catch, throw
///
/// logical units (instead of physical) for type check and portability
///
typedef uint16_t IU;    // instruction pointer unit
typedef int32_t  DU;    // data unit
typedef uint16_t U16;   // unsigned 16-bit integer
typedef uint8_t  U8;    // byte, unsigned character
///
/// alignment macros
///
#define ALIGN(sz)       ((sz) + (-(sz) & 0x1))
//#define ALIGN4(sz)        ((sz) + (-(sz) & 0x3))
///
/// array class template (so we don't have dependency on C++ STL)
/// Note:
///   * using decorator pattern
///   * this is similar to vector class but much simplified
///
template<class T, int N>
struct List {
    T   v[N];           /// fixed-size array storage
    int idx = 0;        /// current index of array

    T& operator[](int i)   { return i < 0 ? v[idx + i] : v[i]; }
    T pop() {
        if (idx>0) return v[--idx];
        throw "ERR: List empty";
    }
    T push(T t) {
        if (idx<N) return v[idx++] = t;
        throw "ERR: List full";
    }
    void push(T *a, int n)  { for (int i=0; i<n; i++) push(*(a+i)); }
    void merge(List& a)     { for (int i=0; i<a.idx; i++) push(a[i]);}
    void clear(int i=0)     { idx=i; }
};
///
/// functor implementation - for lambda support (without STL)
///
struct fop { virtual void operator()(IU) = 0; };
template<typename F>
struct XT : fop {           // universal functor
    F fp;
    XT(F &f) : fp(f) {}
    void operator()(IU c) { fp(c); }
};
///
/// universal Code class
/// Note:
///   * 8-byte on 32-bit machine, 16-byte on 64-bit machine
///
struct Code {
    const char *name = 0;   /// name field
    union {                 /// either a primitive or colon word
        fop *xt = 0;        /// lambda pointer
        struct {            /// a colon word
            U16 def:  1;    /// colon defined word
            U16 immd: 1;    /// immediate flag
            U16 len:  14;   /// len of pf
            IU  pf;         /// offset to pmem space
        };
    };
    template<typename F>    /// template function for lambda
    Code(const char *n, F f, bool im=false) : name(n) {
        xt = new XT<F>(f);
        immd = im ? 1 : 0;
    }
};
///
/// main storages in RAM
/// Note:
///   1.By separating pmem from dictionary, it makes dictionary uniform size
///   * (i.e. the RISC vs CISC debate) which eliminates the need for link field
///   * however, it requires size tuning manually
///   2.For ease of byte counting, we use U8 for pmem instead of U16.
///   * this makes IP increment by 2 instead of word size. If needed, it can be
///   * readjusted.
///
List<DU,    64>      ss;   /// data stack, can reside in registers for some processors
List<DU,    64>      rs;   /// return stack
List<Code*, 1024>    dict; /// fixed sized dictionary (RISC vs CISC)
List<U8,    48*1024> pmem; /// storage for all colon definitions
///
/// system variables
///
bool compile = false;
DU  top = -1, base = 10;
IU  WP = 0, IP = 0;
///
/// dictionary search functions - can be adapted for ROM+RAM
///
int find(const char *s) {
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (strcmp(s, dict[i]->name)==0) return i;
    }
    return -1;
}
///
/// colon word compiler
/// Note:
///   * we separate dict and pmem space to make word uniform in size
///   * if they are combined then can behaves similar to classic Forth
///   * with an addition link field added.
///
void colon(const char *name) {
    const char *nfd = (const char*)&pmem[pmem.idx];      // current pmem pointer
    dict.push(new Code(nfd, [](int){}));    // create a new word on dictionary
    dict[-1]->def = 1;                      // specify a colon word
    dict[-1]->len = 0;                      // advance counter (by number of U16)
    int sz = ALIGN(strlen(name)+1);         // IU (16-bit) alignment
    pmem.push((U8*)name, sz);               // setup raw name field
    dict[-1]->pf  = pmem.idx;               // capture code field index
};
void addcode(IU c) {
    pmem.push((U8*)&c, sizeof(IU));         // add an opcode to pf
    dict[-1]->len += sizeof(IU);            // advance by instruction size
}
void addvar() {                             // add a dovar (variable)
    DU n = 0;                               // default variable value
    addcode(find("dovar"));                 // dovar (+parameter field)
    pmem.push((U8*)&n, sizeof(DU));         // data storage (32-bit integer now)
    dict[-1]->len += sizeof(DU);            // skip to next field
}
void addlit(DU n) {                         // add a dolit (constant)
    addcode(find("dolit"));                 // dovar (+parameter field)
    pmem.push((U8*)&n, sizeof(DU));         // data storage (32-bit integer now)
    dict[-1]->len += sizeof(DU);            // skip to next field
}
void addstr(const char *s) {                // add a string
    int sz = ALIGN(strlen(s)+1);            // IU (16-bit) alignment
    addcode(find("dotstr"));                // dostr, (+parameter field)
    pmem.push((U8*)s, sz);                  // byte0, byte1, byte2, ..., byteN
    dict[-1]->len += sz;                    // skip to next field
}
///
/// Forth inner interpreter
///
int maxbss = 0;
void nest(IU c) {
    ///
    /// by not using any temp variable here can prevent auto stack allocation
    ///
    if (!dict[c]->def) {                    // is a primitive?
        (*(fop*)(((uintptr_t)dict[c]->xt)&~0x3))(c);  // mask out immd (and def), and execute
        return;
    }
    // is a colon words
    rs.push(WP); rs.push(IP); WP=c; IP=0;   // setup call frame
    if (rs.idx > maxbss) maxbss = rs.idx;   // keep rs sizing matrics
    try {
        while (IP < dict[c]->len) {         // in instruction range
            nest(pmem[dict[c]->pf + IP]);   // fetch/exec instruction from pmem space
            IP += sizeof(IU);               // advance to next instruction
        }
    }
    catch(...) {}
    IP = rs.pop(); WP = rs.pop();           // restore call frame
}
///
/// utilize C++ standard template libraries for core functions only
///
#include <sstream>     // istringstream, ostringstream, cin, cout
#include <string>      // string class
using namespace std;
#define ENDL    endl
istringstream   fin;   /// forth_in
ostringstream   fout;  /// forth_out
string strbuf;
///
/// debug functions
///
void ss_dump() {
    fout << " <"; for (int i=0; i<ss.idx; i++) { fout << ss[i] << " "; }
    fout << top << "> ok" << ENDL;
}
void see(IU c) {
    // TODO
}
void words() {
    for (int i=dict.idx - 1; i>=0; i--) {
        Code *w = dict[i];
        fout << w->name << " " << i << (w->immd ? "* " : " ");
        if ((i%10)==0) fout << ENDL;
    }
}
///
/// macros to reduce verbosity
///
inline DU   PUSH(DU v) { ss.push(top); return top = v;         }
inline DU   POP()      { DU n=top; top=ss.pop(); return n;     }
inline void WORD()     { fin >> strbuf; colon(strbuf.c_str()); }  // create a colon word
inline IU   PARAM()    { return (dict[WP]->pf+IP+sizeof(IU));  }  // get parameter field
#define     CODE(s, g) { s, [&](IU c){ g; }, 0 }
#define     IMMD(s, g) { s, [&](IU c){ g; }, 1 }
///
/// primitives (ROMable)
/// Note:
///   * we merge prim into dictionary in main()
///   * However, since primitive is statically compiled
///   * it can be stored in ROM, and only
///   * find() needs to be modified to support ROM+RAM
///
auto _colon = [&](int c) {
    fin >> strbuf;
    colon(strbuf.c_str());
    compile=true;
};
static Code prim[] = {
    CODE("dup",  PUSH(top)),
    CODE("drop", top = ss.pop()),
    CODE("over", PUSH(ss[-1])),
    CODE("swap", DU n = ss.pop(); PUSH(n)),
    CODE("+",    top += ss.pop()),
    CODE("-",    top =  ss.pop() - top),
    CODE(".",    fout << POP() << " "),
    CODE("dovar",
         IU x = PARAM();                     // get pmem offset to parameter field
         PUSH(x); IP += sizeof(DU)),         // push and advance to next instruction
    CODE("dolit",
         IU x = PARAM();                     // parameter field
         DU *i = (DU*)&pmem[x];              // fetch the value
         PUSH(*i); IP += sizeof(DU)),        // push and advance to next instruction
   CODE("dotstr",
         IU x = PARAM();                     // pmem offset to parameter field
         const char *s = (char*)&pmem[x];    // get string pointer
         fout << s;                          // send to output console
         IP += ALIGN(strlen(s)+1)),          // advance to next instruction
    IMMD(".\"",
         getline(fin, strbuf, '"');
         addstr(strbuf.substr(1).c_str())),
    CODE("variable", WORD(); addvar()),
    CODE("constant", WORD(); addlit(POP())),
    CODE(":", WORD(); compile=true),
    IMMD(";", compile = false),
    CODE("words", words()),
    CODE("bye", exit(0))
};
const int PSZ = sizeof(prim)/sizeof(Code);
///
/// outer interpreter
///
int forth_outer() {
    while (fin >> strbuf) {
        const char *idiom = strbuf.c_str();
        // printf("%s=>", idiom);
        int w = find(idiom);           /// * search through dictionary
        if (w>=0) {                                 /// * word found?
            // printf("%s\n", dict[w]->name);
            if (compile && !dict[w]->immd) {        /// * in compile mode?
                addcode(w);                         /// * add to colon word
            }
            else { nest(w); ss_dump(); }            /// * execute forth word
            continue;
        }
        // try as a number
        char *p;
        int n = static_cast<int>(strtol(idiom, &p, base));
        // printf("%d\n", n);
        if (*p != '\0') {                           /// * not number
            fout << idiom << "? " << ENDL;          ///> display error prompt
            compile = false;                        ///> reset to interpreter mode
            break;                                  ///> skip the entire input buffer
        }
        // is a number
        if (compile) addlit(n);                     /// * add literal when in compile mode
        else { PUSH(n); ss_dump(); }                ///> or, add value onto data stack
    }
    return 1;
}
void forth_init() {
    for (int i=0; i<PSZ; i++) {                     /// copy prim(ROM) into RAM dictionary,
        dict.push(&prim[i]);                        /// we don't need to do this if modify
    }                                               /// find to support both
    words();
}
int main(int ac, char* av[]) {
    forth_init();
    cout << unitbuf << "eForth8" << ENDL;
    while (cin >> strbuf) {
        fout.str("");
        fin.clear();
        fin.str(strbuf.c_str());
        forth_outer();
        cout << fout.str();
    }
    cout << "Done." << ENDL;
    return 0;
}
