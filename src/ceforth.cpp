#include <iomanip>          // setbase, setw, setfill
#include "ceforth.h"

int Code::fence = 0;
///
/// Code class constructors
///
#if NO_FUNCTION
Code::Code(string n, F fn, bool im) {
	name = n; token = fence++; immd = im; xt = new XT<F>(fn);
}
#else
Code::Code(string n, fop fn, bool im) {
	name = n; token = fence++; immd = im; xt = fn;
}
#endif // NO_FUNCTION
Code::Code(string n, bool f)   { name = n; if (f) token = fence++; }
Code::Code(Code *c, int v)     { name = c->name; xt = c->xt; qf.push(v); }
Code::Code(Code *c, string s)  { name = c->name; xt = c->xt; if (s!=string()) literal = s;  }

Code* Code::immediate()        { immd = true;  return this; }
Code* Code::addcode(Code* w)   { pf.push(w);   return this; }

string Code::to_s()    { return name + " " + to_string(token) + (immd ? "*" : ""); }
string Code::see(int dp) {
    stringstream cout("");
    auto see_pf = [&cout](int dp, string s, vector<Code*> v) {   // lambda for indentation and recursive dump
        int i = dp; cout << ENDL; while (i--) cout << "  "; cout << s;
        for (Code* w: v) cout << w->see(dp + 1);
    };
    auto see_qf = [&cout](vector<int> v) { cout << " = "; for (int i : v) cout << i << " "; };
    see_pf(dp, "[ " + to_s(), pf.v);
    if (pf1.v.size() > 0) see_pf(dp, "1--", pf1.v);
    if (pf2.v.size() > 0) see_pf(dp, "2--", pf2.v);
    if (qf.v.size()  > 0) see_qf(qf.v);
    cout << "]";
    return cout.str();
}
void  Code::exec() {
#if NO_FUNCTION
    if (xt) (*xt)(this);                             /// * execute primitive word
#else
    if (xt) xt(this);
#endif // NO_FUNCTION
    else {
        for (Code* w : pf.v) { yield(); w->exec(); } /// * or, run inner interpreter
    }
}
///
/// ForthVM class constructor
///
ForthVM::ForthVM(istream &in, ostream &out) : cin(in), cout(out) {}
///
/// dictionary and input stream search functions
///
inline int ForthVM::POP()       { int n = top; top = ss.pop(); return n; }
inline int ForthVM::PUSH(int v) { ss.push(top); return top = v; }

/// search dictionary reversely
Code *ForthVM::find(string s) {
        for (int i = (int)dict.v.size() - 1; i >= 0; --i) {
        if (s == dict.v[i]->name) return dict.v[i];
    }
    return NULL;
}
string ForthVM::next_idiom(char delim) {
    string s; delim ? getline(cin, s, delim) : cin >> s; return s;
}
void ForthVM::dot_r(int n, int v) {
    cout << setw(n) << setfill(' ') << v;
}
void ForthVM::ss_dump() {
    cout << " <"; for (int i : ss.v) { cout << i << " "; }
    cout << top << "> ok" << ENDL;
}
void ForthVM::words() {
    int i = 0;
    for (Code* w : dict.v) {
        if ((i++ % 10) == 0) { cout << ENDL; yield(); }
        cout << w->to_s() << " ";
    }
}
void ForthVM::call(Code *w) {
    int tmp = WP;                                       /// * setup call frame
    WP = w->token;
    try { w->exec(); }                                  /// * run inner interpreter recursively
    catch (exception& e) {
        string msg = e.what();                          /// * capture exception message
        if (msg != string()) cout << msg << ENDL;
    }
    WP = tmp;                                           /// * restore call frame
    yield();
}
///
/// macros to reduce verbosity (but harder to single-step debug)
///
#define CODE(s, g) new Code(string(s), [this](Code *c){ g; })
#define IMMD(s, g) new Code(string(s), [this](Code *c){ g; }, true)
#define ALU(a, OP, b)  (static_cast<int>(a) OP static_cast<int>(b))
#define BOOL(f) ((f) ? -1 : 0)
///
/// dictionary initializer
///
void ForthVM::init() {
    static vector<Code*> prim = {                       /// singleton, build once onl    ///
    /// @defgroup Stack op
    /// @{
    CODE("dup",  PUSH(top)),
    CODE("over", PUSH(ss[-1])),
    CODE("2dup", PUSH(ss[-1]); PUSH(ss[-1])),
    CODE("2over",PUSH(ss[-3]); PUSH(ss[-3])),
    CODE("4dup", PUSH(ss[-3]); PUSH(ss[-3]); PUSH(ss[-3]); PUSH(ss[-3])),
    CODE("swap", int n = ss.pop(); PUSH(n)),
    CODE("rot",  int n = ss.pop(); int m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("-rot", int n = ss.pop(); int m = ss.pop(); PUSH(n); PUSH(m)),
    CODE("2swap",
        int n = ss.pop(); int m = ss.pop(); int l = ss.pop();
        ss.push(n); PUSH(l); PUSH(m)),
    CODE("pick", int i = top; top = ss[-i]),
    //    CODE("roll", int i = top; top = ss[-i]),
    CODE("drop", top = ss.pop()),
    CODE("nip",  ss.pop()),
    CODE("2drop",ss.pop(); top = ss.pop()),
    CODE(">r",   rs.push(POP())),
    CODE("r>",   PUSH(rs.pop())),
    CODE("r@",   PUSH(rs[-1])),
    CODE("push", rs.push(POP())),
    CODE("pop",  PUSH(rs.pop())),
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
        int n = static_cast<int>(ss.pop() * ss.pop());
        int t = static_cast<int>(top);
        ss.push(n % t); top = (n / t)),
    CODE("and",  top = ALU(ss.pop(), &, top)),
    CODE("or",   top = ALU(ss.pop(), |, top)),
    CODE("xor",  top = ALU(ss.pop(), ^, top)),
    CODE("negate", top = -top),
    CODE("abs",  top = abs(top)),
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
    CODE(".r",      int n = POP(); dot_r(n, POP())),
    CODE("u.r",     int n = POP(); dot_r(n, abs(POP()))),
    CODE(".f",      int n = POP(); cout << setprecision(n) << POP()),
    CODE("key",     PUSH(next_idiom()[0])),
    CODE("emit",    char b = (char)POP(); cout << b),
    CODE("space",   cout << " "),
    CODE("spaces",  for (int n = POP(), i = 0; i < n; i++) cout << " "),
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("dotstr",  cout << c->literal),
    CODE("dolit",   PUSH(c->qf[0])),
    CODE("dovar",   PUSH(c->token)),
    CODE("[",       compile = false),
    CODE("]",       compile = true),
    IMMD("(",       next_idiom(')')),
    IMMD(".(",      cout << next_idiom(')')),
    CODE("\\",      cout << next_idiom('\n')),
    CODE("$\"",
        string s = next_idiom('"').substr(1);
        dict[-1]->addcode(new Code(find("dovar"), s))),
    IMMD(".\"",
        string s = next_idiom('"').substr(1);
        dict[-1]->addcode(new Code(find("dotstr"), s))),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    IMMD("bran",
        bool f = POP() != 0;                        // check flag
        for (Code* w : (f ? c->pf.v : c->pf1.v)) call(w)),
    IMMD("if",
        dict[-1]->addcode(new Code(find("bran")));
        dict.push(new Code("temp"))),               // use last cell of dictionay as scratch pad
    IMMD("else",
        Code *temp = dict[-1]; Code *last = dict[-2]->pf[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear();
        last->stage = 1),
    IMMD("then",
        Code *temp = dict[-1]; Code *last = dict[-2]->pf[-1];
        if (last->stage == 0) {                     // if...then
            last->pf.merge(temp->pf.v);
            dict.pop();
        }
        else {                                      // if...else...then, or
             last->pf1.merge(temp->pf.v);           // for...aft...then...next
             if (last->stage == 1) dict.pop();
             else temp->pf.clear();
        }),
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    CODE("loop",
        while (true) {
            for (Code* w : c->pf.v) call(w);                       // begin...
            int f = top;
            if (c->stage == 0 && (top = ss.pop(), f != 0)) break;  // ...until
            if (c->stage == 1) continue;                           // ...again
            if (c->stage == 2 && (top = ss.pop(), f == 0)) break;  // while...repeat
            for (Code* w : c->pf1.v) call(w);
        }),
    IMMD("begin",
        dict[-1]->addcode(new Code(find("loop")));
        dict.push(new Code("temp"))),
    IMMD("while",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear(); last->stage = 2),
    IMMD("repeat",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf1.merge(temp->pf.v); dict.pop()),
    IMMD("again",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf.merge(temp->pf.v);
        last->stage = 1; dict.pop()),
    IMMD("until",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf.merge(temp->pf.v); dict.pop()),
    /// @}
    /// @defgrouop For loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    CODE("cycle",
        do { for (Code* w : c->pf.v) call(w); }
        while (c->stage == 0 && rs.dec_i() >= 0);    // for...next only
        while (c->stage > 0) {                       // aft
            for (Code* w : c->pf2.v) call(w);        // then...next
            if (rs.dec_i() < 0) break;
            for (Code* w : c->pf1.v) call(w);        // aft...then
        }
        rs.pop()),
    IMMD("for",
        dict[-1]->addcode(new Code(find(">r")));
        dict[-1]->addcode(new Code(find("cycle")));
        dict.push(new Code("temp"))),
    IMMD("aft",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear(); last->stage = 3),
    IMMD("next",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        if (last->stage == 0) last->pf.merge(temp->pf.v);
        else last->pf2.merge(temp->pf.v); dict.pop()),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("exit", int x = top; throw domain_error(string())),   // need x=top, Arduino bug
    CODE("exec", int n = top; call(dict[n])),
    CODE(":",
        dict.push(new Code(next_idiom(), true));    // create new word
        compile = true),
    IMMD(";", compile = false),
    CODE("variable",
        dict.push(new Code(next_idiom(), true));
        Code *last = dict[-1]->addcode(new Code(find("dovar"), 0));
        last->pf[0]->token = last->token),
    CODE("constant",
        dict.push(new Code(next_idiom(), true));
        Code *last = dict[-1]->addcode(new Code(find("dolit"), POP()));
        last->pf[0]->token = last->token),
    CODE("@",      int w = POP(); PUSH(dict[w]->pf[0]->qf[0])),         // w -- n
    CODE("!",      int w = POP(); dict[w]->pf[0]->qf[0] = POP()),       // n w --
    CODE("+!",     int w = POP(); dict[w]->pf[0]->qf[0] += POP()),      // n w --
    CODE("?",      int w = POP(); cout << dict[w]->pf[0]->qf[0] << " "),// w --
    CODE("array@", int a = POP(); PUSH(dict[POP()]->pf[0]->qf[a])),     // w a -- n
    CODE("array!", int a = POP(); int w = POP();  dict[w]->pf[0]->qf[a] = POP()),   // n w a --
    CODE("allot",                                           // n --
        for (int n = POP(), i = 0; i < n; i++) dict[-1]->pf[0]->qf.push(0)),
    CODE(",",      dict[-1]->pf[0]->qf.push(POP())),
    /// @}
    /// @defgroup metacompiler
    /// @{
    CODE("create",
        dict.push(new Code(next_idiom(), true));            // create a new word
        Code *last = dict[-1]->addcode(new Code(find("dovar"), 0));
        last->pf[0]->token = last->token;
        last->pf[0]->qf.clear()),
    CODE("does",
        vector<Code*> src = dict[WP]->pf.v;                 // source word : xx create...does...;
        int i = 0; int n = (int)src.size();
        while (i < n && src[i]->name != "does") i++;        // find the "does"
        while (++i < n) dict[-1]->pf.push(src[i]);          // copy words after "does" to new the word
        throw domain_error(string())),                      // break out of for { c->exec() } loop
    CODE("to",                                              // n -- , compile only
        Code *tgt = find(next_idiom());
        if (tgt) tgt->pf[0]->qf[0] = POP()),                // update constant
    CODE("is",                                              // w -- , execute only
        Code *tgt = find(next_idiom());
        if (tgt) {
            tgt->pf.clear();
            tgt->pf.merge(dict[POP()]->pf.v);
        }),
    CODE("[to]",
        vector<Code*> src = dict[WP]->pf.v;                 // source word : xx create...does...;
        int i = 0; int n = (int)src.size();
        while (i < n && src[i]->name != "[to]") i++;        // find the "does"
        src[++i]->pf[0]->qf[0] = POP()),                    // change the following constant
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("bye",   exit(0)),
    CODE("here",  PUSH(dict[-1]->token)),
    CODE("words", words()),
    CODE(".s",    ss_dump()),
    CODE("'",     Code *w = find(next_idiom()); PUSH(w->token)),
    CODE("see",
        Code *w = find(next_idiom());
        if (w) cout << w->see(0) << ENDL),
    CODE("forget",
        Code *w = find(next_idiom());
         if (w == NULL) return;
         dict.erase(Code::fence=max(w->token, find("boot")->token + 1))),
    CODE("clock", PUSH(millis())),
    CODE("delay", delay(POP())),
    CODE("boot", dict.erase(Code::fence=find("boot")->token + 1))
    /// @}
    };
    dict.merge(prim);                                    /// * populate dictionary
    
#if ARDUINO
    static vector<Code*> mcu = {                         /// singleton, build once only
    /// @defgroup Arduino specific ops
    /// @{
    CODE("in",    PUSH(digitalRead(POP()))),
    CODE("ain",   PUSH(analogRead(POP()))),
    CODE("out",   int p = POP(); digitalWrite(p, POP())),
    CODE("pwm",   int p = POP(); analogWrite(p, POP())),
    CODE("pin",   int p = POP(); pinMode(p, POP()))
    /// @}
    };
    dict.merge(mcu);
#endif // ARDUINO
}
///
/// ForthVM Outer interpreter
///
void ForthVM::outer() {
    string idiom;
    while (cin >> idiom) {
        //Serial.print(idiom.c_str()); Serial.print("=>");
    	//printf("%s=>", idiom.c_str());
        Code *w = find(idiom);                          /// * search through dictionary
        if (w) {                                        /// * word found?
            //Serial.println(w->to_s().c_str());
            //printf("%s\n", w->to_s().c_str());
            if (compile && !w->immd)                    /// * in compile mode?
                dict[-1]->addcode(w);                   /// * add to colon word
            else call(w);                               /// * execute forth word
            continue;
        }
        // try as a number
        char *p;
        int n = (int)strtol(idiom.c_str(), &p, base);
        //Serial.println(n, base);
        //printf("%d\n", n);
        if (*p != '\0') {                           /// * not number
            cout << idiom << "? " << ENDL;          ///> display error prompt
            compile = false;                        ///> reset to interpreter mode
            getline(cin, idiom, '\n');              ///> skip the entire line
            continue;
        }
        // is a number
        if (compile)                           /// * a number in compile mode?
            dict[-1]->addcode(new Code(find("dolit"), n)); ///> add to current word
        else PUSH(n);                           	///> or, add value onto data stack
    }
    if (!compile) ss_dump();  /// * dump stack and display ok prompt
}

#if !_WIN32 && !_WIN64 && !ARDUINO
#include <iostream>
/// main program
int main(int ac, char* av[]) {
    istringstream forth_in;
    ostringstream forth_out;
    string cmd;

    ForthVM *vm = new ForthVM(forth_in, forth_out);		// create FVM instance
    vm->init();                                 		// initialize dictionary

    cout << "ceforth 5.01" << ENDL;
    while (getline(cin, cmd)) {							// fetch user input
    	//printf("cmd=<%s>\n", line.c_str());
    	forth_in.clear();								// clear any input stream error bit
    	forth_in.str(cmd);								// send command to FVM
        vm->outer();									// execute outer interpreter
        cout << forth_out.str();						// send VM result to output
        forth_out.str(string());						// clear output buffer
    }
    cout << "done!" << ENDL;
    return 0;
}
#endif // !_WIN32 && !_WIN64 && !ARDUINO
