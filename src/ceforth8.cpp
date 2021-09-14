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
#include <iomanip>     // setbase
#include <string>      // string class
using namespace std;
#define ENDL    endl
istringstream   fin;   /// forth_in
ostringstream   fout;  /// forth_out
string strbuf;
///
/// input token
///
const char *next_idiom(char delim=0) {
    delim ? getline(fin, strbuf, delim) : fin >> strbuf; return strbuf.c_str();
}
///
/// debug functions
///
void dot_r(int n, DU v) {
    fout << setw(n) << setfill(' ') << v;
}
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
inline IU   PARAM()    { return (dict[WP]->pf+IP+sizeof(IU));  }  // get parameter field
#define     CODE(s, g) { s, [&](IU c){ g; }, 0 }
#define     IMMD(s, g) { s, [&](IU c){ g; }, 1 }
#define     BOOL(f)    ((f)?-1:0)
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
    ///
    /// @defgroup Stack ops
    /// @{
    CODE("dup",  PUSH(top)),
    CODE("drop", top = ss.pop()),
    CODE("over", PUSH(ss[-1])),
    CODE("swap", DU n = ss.pop(); PUSH(n)),
    CODE("rot",  DU n = ss.pop(); DU m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("pick", DU i = top; top = ss[-i]),
    CODE(">r",   rs.push(POP())),
    CODE("r>",   PUSH(rs.pop())),
    CODE("r@",   PUSH(rs[-1])),
    /// @}
    /// @defgroup Stack ops - double
    /// @{
    CODE("2dup", PUSH(ss[-1]); PUSH(ss[-1])),
    CODE("2drop",ss.pop(); top = ss.pop()),
    CODE("2over",PUSH(ss[-3]); PUSH(ss[-3])),
    CODE("2swap",
        DU n = ss.pop(); DU m = ss.pop(); DU l = ss.pop();
        ss.push(n); PUSH(l); PUSH(m)),
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",    top += ss.pop()),
    CODE("-",    top =  ss.pop() - top),
    CODE("*",    top *= ss.pop()),
    CODE("/",    top =  ss.pop() / top),
    CODE("mod",  top =  ss.pop() % top),
    CODE("*/",   top = ss.pop() * ss.pop() / top),
    CODE("*/mod",
        DU n = ss.pop() * ss.pop();
        DU t = top;
        ss.push(n % t); top = (n / t)),
    CODE("and",  top = ss.pop()&top),
    CODE("or",   top = ss.pop()|top),
    CODE("xor",  top = ss.pop()^top),
    CODE("negate", top = -top),
    CODE("abs",  top = abs(top)),
    CODE("max",  DU n=ss.pop(); top = (top>n)?top:n),
    CODE("min",  DU n=ss.pop(); top = (top<n)?top:n),
    CODE("2*",   top *= 2),
    CODE("2/",   top /= 2),
    CODE("1+",   top += 1),
    CODE("1-",   top -= 1),
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0= ",  top = BOOL(top == 0)),
    CODE("0<",   top = BOOL(top <  0)),
    CODE("0>",   top = BOOL(top >  0)),
    CODE("=",    top = BOOL(ss.pop() == top)),
    CODE(">",    top = BOOL(ss.pop() >  top)),
    CODE("<",    top = BOOL(ss.pop() <  top)),
    CODE("<>",   top = BOOL(ss.pop() != top)),
    CODE(">=",   top = BOOL(ss.pop() >= top)),
    CODE("<=",   top = BOOL(ss.pop() <= top)),
    /// @}
    /// @defgroup IO ops
    /// @{
    CODE("base@",   PUSH(base)),
    CODE("base!",   fout << setbase(base = POP())),
    CODE("hex",     fout << setbase(base = 16)),
    CODE("decimal", fout << setbase(base = 10)),
    CODE("cr",      fout << ENDL),
    CODE(".",       fout << POP() << " "),
    CODE(".r",      DU n = POP(); dot_r(n, POP())),
    CODE("u.r",     DU n = POP(); dot_r(n, abs(POP()))),
    CODE(".f",      DU n = POP(); fout << setprecision(n) << POP()),
    CODE("key",     PUSH(next_idiom()[0])),
    CODE("emit",    char b = (char)POP(); fout << b),
    CODE("space",   fout << " "),
    CODE("spaces",  for (int n = POP(), i = 0; i < n; i++) fout << " "),
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("dovar",
        PUSH(PARAM()); IP += sizeof(DU)),   // push and advance to next instruction
    CODE("dolit",
    	DU *i = (DU*)&pmem[PARAM()];        // fetch the value
        PUSH(*i); IP += sizeof(DU)),        // push and advance to next instruction
    CODE("dotstr",
        const char *s = (char*)&pmem[PARAM()];    // get string pointer
        fout << s;                          // send to output console
        IP += ALIGN(strlen(s)+1)),          // advance to next instruction
    CODE("[", compile = false),
    CODE("]", compile = true),
    IMMD(".\"", addstr(next_idiom('"')+1)),
    IMMD("(",   next_idiom(')')),
    IMMD(".(",  fout << next_idiom(')')),
    CODE("\\",  next_idiom('\n')),
    CODE("$\"", addstr(next_idiom('"')+1)),
#if 0
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    IMMD("bran", bool f = POP() != 0; call(f ? c.pf : c.pf1)),
    IMMD("if",
        dict[-1]->addcode(BRAN("bran"));
        dict.push(TEMP())),      // use last cell of dictionay as scratch pad
    IMMD("else",
        CodeP temp = dict[-1]; CodeP last = dict[-2]->pf[-1];
        last->pf.merge(temp->pf);
        temp->pf.clear();
        last->stage = 1),
    IMMD("then",
        CodeP temp = dict[-1]; CodeP last = dict[-2]->pf[-1];
        if (last->stage == 0) {                     // if...then
            last->pf.merge(temp->pf);
            dict.pop();
        }
        else {                                      // if...else...then, or
            last->pf1.merge(temp->pf);             // for...aft...then...next
            if (last->stage == 1) dict.pop();
            else temp->pf.clear();
        }),
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    CODE("loop",
        while (true) {
            call(c.pf);                                           // begin...
            int f = INT(top);
            if (c.stage == 0 && (top = ss.pop(), f != 0)) break;  // ...until
            if (c.stage == 1) continue;                           // ...again
            if (c.stage == 2 && (top = ss.pop(), f == 0)) break;  // while...repeat
            call(c.pf1);
        }),
    IMMD("begin",
        dict[-1]->addcode(BRAN("loop"));
        dict.push(TEMP())),
    IMMD("while",
        CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
        last->pf.merge(temp->pf);
        temp->pf.clear(); last->stage = 2),
    IMMD("repeat",
        CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
        last->pf1.merge(temp->pf); dict.pop()),
    IMMD("again",
        CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
        last->pf.merge(temp->pf);
        last->stage = 1; dict.pop()),
    IMMD("until",
        CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
        last->pf.merge(temp->pf); dict.pop()),
    /// @}
    /// @defgrouop For loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    CODE("cycle",
       do { call(c.pf); }
        while (c.stage == 0 && rs.dec_i() >= 0);    // for...next only
        while (c.stage > 0) {                       // aft
            call(c.pf2);                            // then...next
            if (rs.dec_i() < 0) break;
            call(c.pf1);                            // aft...then
        }
        rs.pop()),
    IMMD("for",
        dict[-1]->addcode(find(">r"));
        dict[-1]->addcode(BRAN("cycle"));
        dict.push(TEMP())),
    IMMD("aft",
        CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
        last->pf.merge(temp->pf);
        temp->pf.clear(); last->stage = 3),
    IMMD("next",
        CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
        if (last->stage == 0) last->pf.merge(temp->pf);
        else last->pf2.merge(temp->pf); dict.pop()),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("exit", int x = top; throw domain_error(string())),   // need x=top, Arduino bug
    CODE("exec", int n = INT(top); call(dict[n])),
#endif
    CODE(":", colon(next_idiom()); compile=true),              // create a new word
    IMMD(";", compile = false),
    CODE("variable", colon(next_idiom()); addvar()),
    CODE("constant", colon(next_idiom()); addlit(POP())),
#if 0
    CODE("@",      int w = INT(POP()); PUSH(dict[w]->pf[0]->qf[0])),         // w -- n
    CODE("!",      int w = INT(POP()); dict[w]->pf[0]->qf[0] = POP()),       // n w --
    CODE("+!",     int w = INT(POP()); dict[w]->pf[0]->qf[0] += POP()),      // n w --
    CODE("?",      int w = INT(POP()); cout << dict[w]->pf[0]->qf[0] << " "),// w --
    CODE("array@", int a = INT(POP()); PUSH(dict[POP()]->pf[0]->qf[a])),     // w a -- n
    CODE("array!", int a = INT(POP()); int w = POP();  dict[w]->pf[0]->qf[a] = POP()),   // n w a --
    CODE("allot",                                           // n --
        for (int n = INT(POP()), i = 0; i < n; i++) dict[-1]->pf[0]->qf.push(DVAL)),
    CODE(",",      dict[-1]->pf[0]->qf.push(POP())),
    /// @}
    /// @defgroup metacompiler
    /// @{
    CODE("create",
        dict.push(WORD());                                  // create a new word
        Code& last = dict[-1]->addcode(LIT("dovar", DVAL));
        last.pf[0]->token = last.token;
        last.pf[0]->qf.clear()),
    CODE("does",
        ForthList<CodeP>& src = dict[WP]->pf;               // source word : xx create...does...;
        int n = src.size();
        while (Code::IP < n) dict[-1]->pf.push(src[Code::IP++])),       // copy words after "does" to new the word
    CODE("to",                                              // n -- , compile only
        CodeP tgt = find(next_idiom());
        if (tgt) tgt->pf[0]->qf[0] = POP()),                // update constant
    CODE("is",                                              // w -- , execute only
        CodeP tgt = find(next_idiom());
        if (tgt) {
            tgt->pf.clear();
            tgt->pf.merge(dict[POP()]->pf);
        }),
    CODE("[to]",
        ForthList<CodeP>& src = dict[WP]->pf;               // source word : xx create...does...;
        src[Code::IP++]->pf[0]->qf[0] = POP()),             // change the following constant
#endif
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("bye",   exit(0)),
    CODE("here",  PUSH(dict.idx)),
    CODE("words", words()),
    CODE(".s",    ss_dump()),
#if 0
    CODE("'",     CodeP w = find(next_idiom()); PUSH(w->token)),
    CODE("see",
        CodeP w = find(next_idiom());
        if (w) cout << w->see() << ENDL),
    CODE("forget",
        CodeP w = find(next_idiom());
         if (w == NULL) return;
         dict.clear(Code::fence=max(w->token, find("boot")->token + 1))),
    CODE("clock", PUSH(millis())),
    CODE("delay", delay(POP())),
    CODE("peek",  int a = INT(POP()); PUSH(PEEK(a))),
    CODE("poke",  int a = INT(POP()); POKE(a, POP())),
#if ARDUINO || ESP32
    /// @}
    /// @defgroup Arduino specific ops
    /// @{
    CODE("pin",   int p = INT(POP()); pinMode(p, POP())),
    CODE("in",    PUSH(digitalRead(POP()))),
    CODE("out",   int p = INT(POP()); digitalWrite(p, POP())),
    CODE("adc",   PUSH(analogRead(POP()))),
    CODE("pwm",   int p = INT(POP()); analogWrite(p, POP(), 255)),
#if ESP32
    CODE("attach",int p = INT(POP()); ledcAttachPin(p, POP())),
    CODE("setup", int p = INT(POP()); int freq = INT(POP()); ledcSetup(p, freq, POP())),
    CODE("tone",  int p = INT(POP()); ledcWriteTone(p, POP())),
#endif // ESP32
#endif // ARDUINO || ESP32
    /// @}
    CODE("boot", dict.clear(Code::fence=find("boot")->token + 1))
#endif
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

#include <iostream>		// cin, cout
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
