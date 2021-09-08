#include <string.h>         // strcasecmp
#include <iomanip>          // setbase, setw, setfill
#include "ceforth.h"

int Code::fence = 0, Code::IP = 0;
///
/// Code class constructors
///
#if NO_STD_FUNCTION
Code::Code(string n, F fn, bool im) {
    name = n; token = fence++; immd = im; xt = new XT<F>(fn);
}
#else // NO_STD_FUNCTION
Code::Code(string n, fop fn, bool im) {
    name = n; token = fence++; immd = im; xt = fn;
}
#endif // NO_STD_FUNCTION
Code::Code(string n, bool f)   { name = n; if (f) token = fence++; }
Code::Code(CodeP c, DTYPE v)   { name = c->name; xt = c->xt; qf.push(v); }
Code::Code(CodeP c, string s)  { name = c->name; xt = c->xt; if (s.size()>0) literal = s;  }

Code&  Code::addcode(CodeP w)  { pf.push(w); return *this; }
string Code::to_s()            { return name + " " + to_string(token) + (immd ? "*" : ""); }
string Code::see(int dp) {
    stringstream cout(string("", 256));
    auto see_pf = [&cout](int dp, string s, ForthList<CodeP>& pf) {   // lambda for indentation and recursive dump
        int i = dp; cout << ENDL; while (i--) cout << "  "; cout << s;
        for (CodeP w: pf.v) cout << w->see(dp + 1);
    };
    auto see_qf = [&cout](vector<DTYPE>& v) { cout << " = "; for (DTYPE i : v) cout << i << " "; };
    see_pf(dp, "[ " + to_s(), pf);
    if (pf1.size() > 0) see_pf(dp, "1--", pf1);
    if (pf2.size() > 0) see_pf(dp, "2--", pf2);
    if (qf.size()  > 0) see_qf(qf.v);
    cout << "]";
    return cout.str();
}
void  Code::nest() {
#if NO_STD_FUNCTION
    if (xt) { (*xt)(this); return; }                 /// * execute primitive word
#else
    if (xt) { xt(*this); return; }
#endif // NO_STD_FUNCTION
    int tmp = IP, n = pf.size(); IP = 0;             /// * or, setup call frame
    while (IP < n) { yield(); pf[IP++]->nest(); }    /// * and run inner interpreter
    IP = tmp;                                        /// * resture call frame
}
///
/// ForthVM class constructor
///
ForthVM::ForthVM(istream &in, ostream &out)
    : cin(in), cout(out), idiom(string("", 256)) {}
///
/// dictionary and input stream search functions
///
inline DTYPE ForthVM::POP()         { DTYPE n = top; top = ss.pop(); return n; }
inline DTYPE ForthVM::PUSH(DTYPE v) { ss.push(top); return top = v; }
inline bool  ForthVM::STREQ(string& s1, string& s2) {
    return ucase
        ? strcasecmp(s1.c_str(), s2.c_str())==0
        : s1 == s2;
}
/// search dictionary reversely
CodeP ForthVM::find(string s) {
    for (int i = dict.size() - (compile ? 2 : 1); i >= 0; --i) {
        if (STREQ(s, dict[i]->name)) return dict[i];
    }
    return NULL;
}
string& ForthVM::next_idiom(char delim) {
    delim ? getline(cin, idiom, delim) : cin >> idiom; return idiom;
}
void ForthVM::dot_r(int n, DTYPE v) {
    cout << setw(n) << setfill(' ') << v;
}
void ForthVM::ss_dump() {
    cout << " <"; for (DTYPE i : ss.v) { cout << i << " "; }
    cout << top << "> ok" << ENDL;
}
void ForthVM::words() {
    int i = 0;
    for (CodeP w : dict.v) {
        if ((i++ % 10) == 0) { cout << ENDL; yield(); }
        cout << w->to_s() << " ";
    }
}
void ForthVM::call(CodeP w) {
    int tmp = WP;                                       /// * setup call frame
    WP = w->token;
    try { w->nest(); }                                  /// * run inner interpreter recursively
    catch (exception& e) {
        string msg = e.what();                          /// * capture exception message
        if (msg != string()) cout << msg << ENDL;
    }
    WP = tmp;                                           /// * restore call frame
    yield();
}
void ForthVM::call(ForthList<CodeP>& pf) {
    for (int i=0, n=pf.size(); i<n; i++) call(pf[i]);
}
///
/// macros for ESP memory space access (be very careful of these)
/// note: 4000_0000 is instruction bus, access needs to be 32-bit aligned
///       3fff_ffff and below is data bus, no alignment requirement
///
typedef unsigned int   U32;
#define PEEK(a)        (U32)(*(U32*)((uintptr_t)(a)))
#define POKE(a, c)     (*(U32*)((uintptr_t)(a))=(U32)(c))
///
/// macros to reduce verbosity (but harder to single-step debug)
///
#define CODE(s, g)     make_shared<Code>(string(s), [&](Code& c){ g; })
#define IMMD(s, g)     make_shared<Code>(string(s), [&](Code& c){ g; }, true)
#define WORD()         make_shared<Code>(next_idiom(), true)
#define LIT(s,l)       make_shared<Code>(find(s), (l))
#define BRAN(s)        make_shared<Code>(find(s))
#define TEMP(s)        make_shared<Code>("temp")
#define INT(f)         (static_cast<int>(f))
#define ALU(a, OP, b)  (INT(a) OP INT(b))
#define BOOL(f) ((f) ? -1 : 0)
///
/// dictionary initializer
///
void ForthVM::init() {
    dict.v = {                      /// TODO: singleton, created once
    ///
    /// @defgroup Stack ops
    /// @{
    CODE("dup",  PUSH(top)),
    CODE("drop", top = ss.pop()),
    CODE("over", PUSH(ss[-1])),
    CODE("swap", DTYPE n = ss.pop(); PUSH(n)),
    CODE("rot",  DTYPE n = ss.pop(); DTYPE m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("pick", int i = INT(top); top = ss[-i]),
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
        DTYPE n = ss.pop(); DTYPE m = ss.pop(); DTYPE l = ss.pop();
        ss.push(n); PUSH(l); PUSH(m)),
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",    top += ss.pop()),
    CODE("-",    top =  ss.pop() - top),
    CODE("*",    top *= ss.pop()),
    CODE("/",    top =  ss.pop() / top),
    CODE("mod",  top = ALU(ss.pop(), %, top)),
    CODE("*/",   top = ss.pop() * ss.pop() / top),
    CODE("*/mod",
        int n = INT(ss.pop() * ss.pop());
        int t = INT(top);
        ss.push(n % t); top = (n / t)),
    CODE("and",  top = ALU(ss.pop(), &, top)),
    CODE("or",   top = ALU(ss.pop(), |, top)),
    CODE("xor",  top = ALU(ss.pop(), ^, top)),
    CODE("negate", top = -top),
    CODE("abs",  top = abs(top)),
    CODE("max",  int n=ss.pop();top = (top>n)?top:n),
    CODE("min",  int n=ss.pop();top = (top<n)?top:n),
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
    CODE("base!",   cout << setbase(base = POP())),
    CODE("hex",     cout << setbase(base = 16)),
    CODE("decimal", cout << setbase(base = 10)),
    CODE("cr",      cout << ENDL),
    CODE(".",       cout << POP() << " "),
    CODE(".r",      int n = INT(POP()); dot_r(n, POP())),
    CODE("u.r",     int n = INT(POP()); dot_r(n, abs(POP()))),
    CODE(".f",      int n = INT(POP()); cout << setprecision(n) << POP()),
    CODE("key",     PUSH(next_idiom()[0])),
    CODE("emit",    char b = (char)POP(); cout << b),
    CODE("space",   cout << " "),
    CODE("spaces",  for (int n = INT(POP()), i = 0; i < n; i++) cout << " "),
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("dotstr",  cout << c.literal),
    CODE("dolit",   PUSH(c.qf[0])),
    CODE("dovar",   PUSH(c.token)),
    CODE("[",       compile = false),
    CODE("]",       compile = true),
    IMMD("(",       next_idiom(')')),
    IMMD(".(",      cout << next_idiom(')')),
    CODE("\\",      next_idiom('\n')),
    CODE("$\"",
        string s = next_idiom('"').substr(1);
        dict[-1]->addcode(LIT("dovar", s))),
    IMMD(".\"",
        string s = next_idiom('"').substr(1);
        dict[-1]->addcode(LIT("dotstr", s))),
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
    CODE(":", dict.push(WORD()); compile = true),              // create new word
    IMMD(";", compile = false),
    CODE("variable",
        dict.push(WORD());
        Code& last = dict[-1]->addcode(LIT("dovar", DVAL));
        last.pf[0]->token = last.token),
    CODE("constant",
        dict.push(WORD());
        Code& last = dict[-1]->addcode(LIT("dolit", POP()));
        last.pf[0]->token = last.token),
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
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("bye",   exit(0)),
    CODE("here",  PUSH(dict[-1]->token)),
    CODE("words", words()),
    CODE(".s",    ss_dump()),
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
    };
}
///
/// ForthVM Outer interpreter
///
void ForthVM::outer() {
    while (cin >> idiom) {
        //printf("%s=>", idiom.c_str());
        CodeP w = find(idiom);                      /// * search through dictionary
        if (w) {                                    /// * word found?
            //printf("%s(%ld)\n", w->to_s().c_str(), w.use_count());
            if (compile && !w->immd)                /// * in compile mode?
                dict[-1]->addcode(w);               /// * add to colon word
            else call(w);                           /// * execute forth word
            continue;
        }
        // try as a number
        char *p;
#if DTYPE==float
        DTYPE n = (base==10)
            ? static_cast<DTYPE>(strtof(idiom.c_str(), &p))
            : static_cast<DTYPE>(strtol(idiom.c_str(), &p, base));
#else
        DTYPE n = static_cast<DTYPE>(strtol(idiom.c_str(), &p, base));
#endif
        //printf("%d\n", n);
        if (*p != '\0') {                           /// * not number
            cout << idiom << "? " << ENDL;          ///> display error prompt
            compile = false;                        ///> reset to interpreter mode
            break;                                  ///> skip the entire input buffer
        }
        // is a number
        if (compile)                                /// * a number in compile mode?
            dict[-1]->addcode(LIT("dolit", n));     ///> add to current word
        else PUSH(n);                               ///> or, add value onto data stack
    }
    if (!compile) ss_dump();   /// * dump stack and display ok prompt
}

#if !_WIN32 && !_WIN64 && !ARDUINO
#include <iostream>
/// main program
int main(int ac, char* av[]) {
    istringstream forth_in;
    ostringstream forth_out;
    string cmd;

    ForthVM *vm = new ForthVM(forth_in, forth_out);     // create FVM instance
    vm->init();                                         // initialize dictionary

    cout << "ceforth 5.01" << ENDL;
    while (getline(cin, cmd)) {                         // fetch user input
        //printf("cmd=<%s>\n", line.c_str());
        forth_in.clear();                               // clear any input stream error bit
        forth_in.str(cmd);                              // send command to FVM
        vm->outer();                                    // execute outer interpreter
        cout << forth_out.str();                        // send VM result to output
        forth_out.str(string());                        // clear output buffer
    }
    cout << "done!" << ENDL;
    return 0;
}
#endif // !_WIN32 && !_WIN64 && !ARDUINO
