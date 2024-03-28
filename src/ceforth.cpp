///
/// @file
/// @brief eForth - C++ vector-based token-threaded implementation
///
///====================================================================
#include <sstream>                     /// iostream, stringstream
#include "ceforth.h"

using namespace std;
///
///> Forth VM state variables
///
FV<DU> ss;                             ///< data stack
FV<DU> rs;                             ///< return stack
DU     top     = -1;                   ///< cached top of stack
bool   compile = false;                ///< compiling flag
///
///> I/O streaming interface
///
istringstream   fin;                   ///< forth_in
ostringstream   fout;                  ///< forth_out
string          pad;                   ///< input string buffer
void (*fout_cb)(int, const char*);     ///< forth output callback functi
///
///> Code class implementation
///
Code *find(string s);                  ///> forward declare for Code
void words();

///====================================================================
///> Code Class implementation
///
int Code::here = 0;                             ///< init static var
Code::Code(string n, bool f)                    ///> colon word
    : name(n), token(f ? here++ : 0) {
    Code *w = find(n); xt = w ? w->xt : NULL;
}
Code::Code(string n, int d)                     ///> dolit, dovar
    : name(""), xt(find(n)->xt) { q.push(d); }
Code::Code(string n, string s)                  ///> dostr, dotstr
    : name("_$"), xt(find(n)->xt), str(s) {}
///
///> macros to reduce verbosity (but harder to single-step debug)
///
inline  DU POP() { DU n=top; top=ss.pop(); return n; }
#define PUSH(v)  (ss.push(top), top=(v))
#define BOOL(f)  ((f) ? -1 : 0)
#define VAR(i)   (*dict[(int)(i)]->pf[0]->q.data())
#define BASE     (VAR(0))   /* borrow dict[0] to store base (numeric radix) */
///
///> IO functions
///
string next_idiom(char delim=0) {    ///> read next idiom form input stream
    string s; delim ? getline(fin, s, delim) : fin >> s; return s;
}
void ss_dump() {                     ///> display data stack and ok promt
    fout << "< "; for (DU i : ss) fout << i << " ";
    fout << top << " > ok" << ENDL;
}
void see(Code *c, int dp) {          ///> disassemble a colon word
    auto pp = [](int dp, string s, FV<Code*> v) {     ///> recursive dump with indent
        int i = dp; fout << ENDL; while (i--) fout << "  "; fout << s;
        for (Code *w : v) if (dp < 2) see(w, dp + 1); /// * depth controlled
    };
    pp(dp, (c->name=="_$" ? ".\" "+c->str+"\"" : c->name), c->pf);
    if (c->p1.size() > 0) pp(dp, "( 1-- )", c->p1);
    if (c->p2.size() > 0) pp(dp, "( 2-- )", c->p2);
    if (c->q.size()  > 0) for (DU i : c->q) fout << i << " ";
}
///
///> Forth Dictionary Assembler
/// @note:
///    1. Words are assembled by calling C++ initializer_list
///       Currently, C++ compiler instainciate them at start up
///       but we hope it can become a static ROM in the future.
///    2. a degenerated lambda becomes a function pointer
///
#define CODE(s, g)  (new Code(s, [](Code *c){ g; }, false))
#define IMMD(s, g)  (new Code(s, [](Code *c){ g; }, true))

FV<Code*> dict = {                 ///< Forth dictionary
    CODE("bye",    exit(0)),       // exit to OS
    ///
    /// @defgroup Data Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",    PUSH(top)),
    CODE("drop",   top=ss.pop()),  // note: ss.pop() != POP()
    CODE("swap",   DU n = ss.pop(); PUSH(n)),
    CODE("over",   PUSH(ss[-2])),
    CODE("rot",    DU n = ss.pop(); DU m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("-rot",   DU n = ss.pop(); DU m = ss.pop(); PUSH(m);  PUSH(n)),
    CODE("pick",   top = ss[-top]),
    CODE("nip",    ss.pop()),
    CODE("?dup",   if (top != DU0) PUSH(top)),
    /// @}
    /// @defgroup Data Stack ops - double
    /// @{
    CODE("2dup",   PUSH(ss[-2]); PUSH(ss[-2])),
    CODE("2drop",  ss.pop(); top=ss.pop()),
    CODE("2swap",  DU n = ss.pop(); DU m = ss.pop(); DU l = ss.pop();
                   ss.push(n); PUSH(l); PUSH(m)),
    CODE("2over",  PUSH(ss[-4]); PUSH(ss[-4])),
    /// @}
    /// @defgroup Return Stack ops
    /// @{
    CODE(">r",     rs.push(POP())),
    CODE("r>",     PUSH(rs.pop())),
    CODE("r@",     PUSH(rs[-1])),
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",      top += ss.pop()),
    CODE("-",      top =  ss.pop() - top),
    CODE("*",      top *= ss.pop()),
    CODE("/",      top =  ss.pop() / top),
    CODE("mod",    top =  ss.pop() % top),
    CODE("*/",     top =  ss.pop() * ss.pop() / top),
    CODE("/mod",   DU n = ss.pop(); DU t = top;
                   ss.push(n % t); top = (n / t)),
    CODE("*/mod",  DU2 n = (DU2)ss.pop() * ss.pop(); DU2 t=top;
                   ss.push((DU)(n % t)); top = (DU)(n / t)),
    CODE("and",    top &= ss.pop()),
    CODE("or",     top |= ss.pop()),
    CODE("xor",    top ^= ss.pop()),
    CODE("abs",    top = abs(top)),
    CODE("negate", top = -top),
    CODE("invert", top = ~top),
    CODE("rshift", top = ss.pop() >> top),
    CODE("lshift", top = ss.pop() << top),
    CODE("max",    DU n=ss.pop(); top = (top>n)?top:n),
    CODE("min",    DU n=ss.pop(); top = (top<n)?top:n),
    CODE("2*",     top *= 2),
    CODE("2/",     top /= 2),
    CODE("1+",     top += 1),
    CODE("1-",     top -= 1),
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0=",     top = BOOL(top == DU0)),
    CODE("0<",     top = BOOL(top <  DU0)),
    CODE("0>",     top = BOOL(top >  DU0)),
    CODE("=",      top = BOOL(ss.pop() == top)),
    CODE(">",      top = BOOL(ss.pop() >  top)),
    CODE("<",      top = BOOL(ss.pop() <  top)),
    CODE("<>",     top = BOOL(ss.pop() != top)),
    CODE(">=",     top = BOOL(ss.pop() >= top)),
    CODE("<=",     top = BOOL(ss.pop() <= top)),
    CODE("u<",     top = BOOL(UINT(ss.pop()) < UINT(top))),
    CODE("u>",     top = BOOL(UINT(ss.pop()) > UINT(top))),
    /// @}
    /// @defgroup IO ops
    /// @{
    CODE("base",   PUSH(0)),   // dict[0]->pf[0]->q[0] used for base
    CODE("decimal",BASE = 10),
    CODE("hex",    BASE = 16),
    CODE("bl",     fout << " "),
    CODE("cr",     fout << ENDL),
    CODE(".",      fout << setbase(BASE) << POP() << " "),
    CODE(".r",     fout << setbase(BASE) << setw(POP()) << POP()),
    CODE("u.r",    fout << setbase(BASE) << setw(POP()) << abs(POP())),
    CODE("key",    PUSH(next_idiom()[0])),
    CODE("emit",   fout << (char)POP()),
    CODE("space",  fout << " "),
    CODE("spaces", fout << setw(POP()) << ""),
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("_str",   fout << c->str),
    CODE("_lit",   PUSH(c->q[0])),
    CODE("_var",   PUSH(c->token)),
    IMMD("(",      next_idiom(')')),
    IMMD(".(",     fout << next_idiom(')')),
    IMMD("\\",     string s; getline(fin, s, '\n')), // flush input
    IMMD(".\"",
         string s = next_idiom('"');
         dict[-1]->add(new Code("_str", s.substr(1)))),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    CODE("_bran",
         for (Code *w : (POP() ? c->pf : c->p1)) w->exec()),
    IMMD("if",
         dict[-1]->add(new Code("_bran"));
         dict.push(new Code("tmp"))),                // scratch pad
    IMMD("else",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         tmp->pf.clear();
         last->stage = 1),
    IMMD("then",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         if (last->stage == 0) {               // if...then
             last->pf.merge(tmp->pf);
             dict.pop();                       // CC: memory leak?
         }
         else {                                // if..else..then, or
             last->p1.merge(tmp->pf);          // for..aft..then..next
             if (last->stage == 1) dict.pop(); // CC: memory leak?
             else tmp->pf.clear();
         }),
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    CODE("_loop", int b = c->stage;            ///< stage=looping type
         while (true) {
             for (Code *w : c->pf) w->exec();  // begin..
             if (b==0 && POP()!=0) break;      // ..until
             if (b==1)             continue;   // ..again
             if (b==2 && POP()==0) break;      // ..while..repeat
             for (Code *w : c->p1) w->exec();
         }),
    IMMD("begin",
         dict[-1]->add(new Code("_loop"));
         dict.push(new Code("tmp"))),
    IMMD("while",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         tmp->pf.clear(); last->stage = 2),
    IMMD("repeat",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->p1.merge(tmp->pf); dict.pop()),
    IMMD("again",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         last->stage = 1; dict.pop()),
    IMMD("until",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf); dict.pop()),
    /// @}
    /// @defgrouop For loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    CODE("_for",
         do { for (Code *w : c->pf) w->exec(); }
         while (c->stage==0 && rs.dec_i() >=0);   // for...next only
         while (c->stage > 0) {                   // aft
             for (Code *w : c->p2) w->exec();     // then...next
             if (rs.dec_i() < 0) break;
             for (Code *w : c->p1) w->exec();     // aft...then
         }
         rs.pop()),
    IMMD("for",
         dict[-1]->add(new Code(">r"));
         dict[-1]->add(new Code("_for"));
         dict.push(new Code("tmp"))),
    IMMD("aft",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         tmp->pf.clear(); last->stage = 3),
    IMMD("next",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         if (last->stage == 0) last->pf.merge(tmp->pf);
         else last->p2.merge(tmp->pf);
         dict.pop()),                             // CC: memory leak?
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("exit",   throw length_error("")),       // --
    CODE("[",      compile = false),
    CODE("]",      compile = true),
    CODE(":",
         dict.push(new Code(next_idiom(), true)); // create new word
         compile = true),
    IMMD(";", compile = false),
    CODE("constant",
         dict.push(new Code(next_idiom(), true));
         Code *last = dict[-1]->add(new Code("_lit", POP()));
         last->pf[0]->token = last->token),
    CODE("variable",
         dict.push(new Code(next_idiom(), true));
         Code *last = dict[-1]->add(new Code("_var", 0));
         last->pf[0]->token = last->token),
    CODE("immediate", dict[-1]->immediate()),
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   dict[top]->exec()),            // xt --
    CODE("create",
         dict.push(new Code(next_idiom(), true));
         Code *last = dict[-1]->add(new Code("_var", 0));
         last->pf[0]->token = last->token;
         last->pf[0]->q.pop()),
    CODE("_does",
         bool hit = false;
         for (Code *w : dict[c->token]->pf) {
             if (hit) dict[-1]->add(w);               // copy rest of pf
             if (w->name=="_does") hit = true;
         }
         throw length_error("")),                     // exit caller
    IMMD("does>",
         dict[-1]->add(new Code("_does"));
         dict[-1]->pf[-1]->token = dict[-1]->token),  // keep WP
    CODE("to",                                        // n --
         Code *w=find(next_idiom()); if (!w) return;
         VAR(w->token) = POP()),                      // update value
    CODE("is",                                        // w --
         dict.push(new Code(next_idiom()));           // create word
         int n = POP();                               // like this word
         dict[-1]->xt = dict[n]->xt;                  // if primitive
         dict[-1]->pf = dict[n]->pf),                 // or colon word
    /// @}
    /// @defgroup Memory Access ops
    /// @{
    CODE("@",      DU w=POP(); PUSH(VAR(w))),                     // w -- n
    CODE("!",      DU w=POP(); VAR(w) = POP()),                   // n w --
    CODE("+!",     DU w=POP(); VAR(w) += POP()),                  // n w --
    CODE("?",      DU w=POP(); fout << VAR(w) << " "),            // w --
    CODE("array@", DU i=POP(); int w=POP(); PUSH(*(&VAR(w)+i))),  // w i -- n
    CODE("array!", DU i=POP(); int w=POP(); *(&VAR(w)+i)=POP()),  // n w i --
    CODE(",",      dict[-1]->pf[0]->q.push(POP())),
    CODE("allot",                                     // n --
         int n = POP();
         for (int i=0; i<n; i++) dict[-1]->pf[0]->q.push(0)),
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("here",  PUSH(dict[-1]->token)),
    CODE("'",     Code *w = find(next_idiom()); if (w) PUSH(w->token)),
    CODE(".s",    ss_dump()),
    CODE("words", words()),
    CODE("see",   Code *w = find(next_idiom()); if (w) see(w, 0); fout << ENDL),
    /// @}
    /// @defgroup OS ops
    /// @{
    CODE("mstat", mem_stat()),
    CODE("ms",    PUSH(millis())),
    CODE("delay", delay(POP())),
    CODE("included",                        // include external file
         const char *fn = "TODO";           // file name
         forth_include(fn)),                // include file
    CODE("forget",
         Code *w = find(next_idiom()); if (!w) return;
         int t = max(w->token, find("boot")->token);
         for (int i=dict.size(); i>t; i--) dict.pop()),
    CODE("boot",
         int t = find("boot")->token + 1;
         for (int i=dict.size(); i>t; i--) dict.pop())
};
Code *find(string s) {      ///> scan dictionary, last to first
    for (int i = dict.size() - 1; i >= 0; --i) {
        if (s == dict[i]->name) return dict[i];
    }
    return NULL;            /// * word not found
}
void words() {              ///> display word list
    const int WIDTH = 60;
    int i = 0, x = 0;
    fout << setbase(16) << setfill('0');
    for (Code *w : dict) {
#if CC_DEBUG
        fout << setw(4) << w->token << "> "
             << setw(8) << (uintptr_t)w->xt
             << ":" << (w->immd ? '*' : ' ')
             << w->name << "  " << ENDL;
#else
        if (w->name[0]=='_') continue;
        fout << w->name << "  ";
        x += (w->name.size() + 2);
#endif
        if (x > WIDTH) { fout << ENDL; x = 0; }
    }
    fout << setbase(BASE) << ENDL;
}
///
///> setup user variables
///
void forth_init() {
    dict[0]->add(new Code("_var", 10));   /// * borrow dict[0] for base
}
///====================================================================
///
///> Forth outer interpreter
///
DU parse_number(string idiom, int *err) {
    const char *cs = idiom.c_str();
    int b = BASE;
    switch (*cs) {                    ///> base override
    case '%': b = 2;  cs++; break;
    case '&':
    case '#': b = 10; cs++; break;
    case '$': b = 16; cs++; break;
    }
    char *p;
    *err = errno = 0;
#if DU==float
    DU n = (b==10)
        ? static_cast<DU>(strtof(cs, &p))
        : static_cast<DU>(strtol(cs, &p, b));
#else
    DU n = static_cast<DU>(strtol(cs, &p, b));
#endif
    if (errno || *p != '\0') *err = 1;
    return n;
}

void forth_core(string idiom) {
    Code *w = find(idiom);            /// * search through dictionary
    if (w) {                          /// * word found?
        if (compile && !w->immd)
            dict[-1]->add(w);         /// * add token to word
        else w->exec();               /// * execute forth word
        return;
    }
	// try as a number
	int err = 0;
	DU  n   = parse_number(idiom, &err);
	if (err) throw length_error("");        /// * not number
	if (compile)
		dict[-1]->add(new Code("_lit", n)); /// * add to current word
	else PUSH(n);                           /// * add value to data stack
}
///
///> Forth VM - interface to outside world
///
void forth_vm(const char *cmd, void(*callback)(int, const char*)) {
    fin.clear();                ///> clear input stream error bit if any
    fin.str(cmd);               ///> feed user command into input stream
    fout_cb = callback;         ///> setup callback function
    fout.str("");               ///> clean output buffer, ready for next run
    string idiom;
    while (fin >> idiom) {           ///> outer interpreter loop
        try { forth_core(idiom); }   ///> single command to Forth core
        catch(...) {
            fout << idiom << "? " << ENDL;
            compile = false;
            getline(fin, pad, '\n'); /// * flush to end-of-line
        }
    }
    if (!compile) ss_dump();
}
///====================================================================
