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
string          pad;                   ///< string buffers
void (*fout_cb)(int, const char*);     ///< forth output callback functi

///====================================================================
///> Code Class implementation
///
int Code::here = 0;                    ///< init static var
Code::Code(string n, bool t)
    : name(n), token(t ? here++ : 0) { ///> colon word, t=new word
        Code *w = find(n); xt = w ? w->xt : NULL;
        if (t && w) fout << "reDef?";
    }
///
///> macros to reduce verbosity (but harder to single-step debug)
///
inline  DU POP() { DU n=top; top=ss.pop(); return n; }
#define PUSH(v)  (ss.push(top), top=(v))
#define BOOL(f)  ((f) ? -1 : 0)
#define VAR(i)   (*dict[(int)(i)]->pf[0]->q.data())
#define BASE     (VAR(0))   /* borrow dict[0] to store base (numeric radix) */
///
///> Internal literal and branching words
///
void _str(Code *c)  {
    if (!c->token) fout << c->name;
    else { PUSH(c->token); PUSH(c->name.size()); }
}
void _lit(Code *c)  { PUSH(c->q[0]);  }
void _var(Code *c)  { PUSH(c->token); }
void _tor(Code *c)  { rs.push(POP()); }
void _dor(Code *c)  { rs.push(ss.pop()); rs.push(POP()); }
void _bran(Code *c) {
    for (Code *w : (POP() ? c->pf : c->p1)) w->exec();
}
void _cycle(Code *c) {           ///> begin.while.repeat, begin.until
    int b = c->stage;            ///< branching state
    while (true) {
        for (Code *w : c->pf) w->exec();     /// * begin..
        if (b==0 && POP()!=0) break;         /// * ..until
        if (b==1)             continue;      /// * ..again
        if (b==2 && POP()==0) break;         /// * ..while..repeat
        for (Code *w : c->p1) w->exec();
    }
}
void _for(Code *c) {             ///> for..next
    int b = c->stage;                        /// * kept in register
    try {
        do {
            for (Code *w : c->pf) w->exec();
        } while (b==0 && (rs[-1]-=1) >=0);   /// * for...next only
        while (b) {                          /// * aft
            for (Code *w : c->p2) w->exec(); /// * then...next
            if ((rs[-1]-=1) < 0) break;      /// * decrement counter
            for (Code *w : c->p1) w->exec(); /// * aft...then
        }
        rs.pop();
    }
    catch (...) { rs.pop(); }                // handle EXIT
}
void _doloop(Code *c) {         ///> do..loop
    try { 
        do {
            for (Code *w : c->pf) w->exec();
        } while ((rs[-1]+=1) < rs[-2]);      // increment counter
        rs.pop(); rs.pop();
    }
    catch (...) {}                           // handle LEAVE
}
void _does(Code *c);  ///> forward declared, needs dict
///
///> IO functions
///
string word(char delim=0) {          ///> read next idiom form input stream
    string s; delim ? getline(fin, s, delim) : fin >> s; return s;
}
void ss_dump(DU base) {              ///> display data stack and ok promt
    char buf[34];
    auto rdx = [&buf](DU v, int b) {      ///> display v by radix
        int i = 33;  buf[i]='\0';         /// * C++ can do only 8,10,16
        DU  n = v < 0 ? -v : v;           ///< handle negative
        while (n && i) {                  ///> digit-by-digit
            U8 d = (U8)(n % b);  n /= b;
            buf[--i] = d > 9 ? (d-10)+'a' : d+'0';
        }
        if (v < 0) buf[--i]='-';
        return &buf[i];
    };
    for (DU v : ss) {
        fout << rdx(v, base) << ' ';
    }
    fout << rdx(top, base) << " -> ok" << ENDL;
}
void see(Code *c, int dp) {          ///> disassemble a colon word
    auto pp = [](int dp, string s, FV<Code*> v) {     ///> recursive dump with indent
        int i = dp; fout << ENDL; while (i--) fout << "  "; fout << s;
        for (Code *w : v) if (dp < 3) see(w, dp + 1); /// * depth controlled
    };
    string sn = c->str
        ? (c->token ? "s\" " : ".\" ")+c->name+"\"" : c->name;
    string bn = c->stage==2    ? "_while" : (c->stage==3 ? "_aft" : "_else");
    pp(dp, sn, c->pf);
    if (c->p1.size() > 0) pp(dp, bn, c->p1);
    if (c->p2.size() > 0) pp(dp, "_then", c->p2);
    if (c->q.size()  > 0) for (DU i : c->q) fout << i << " ";
}
void words();                      /// forward declard, needs dict
///
///> Forth Dictionary Assembler
/// @note:
///    1. Words are assembled by calling C++ initializer_list
///       Currently, C++ compiler instainciate them at start up
///       but we hope it can become a static ROM in the future.
///    2. a degenerated lambda becomes a function pointer
///
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
    CODE("abs",    top =  abs(top)),
    CODE("negate", top =  -top),
    CODE("invert", top =  ~top),
    CODE("rshift", top =  (U32)ss.pop() >> top),
    CODE("lshift", top =  (U32)ss.pop() << top),
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
    CODE("u<",     top = BOOL(abs(ss.pop()) < abs(top))),
    CODE("u>",     top = BOOL(abs(ss.pop()) > abs(top))),
    /// @}
    /// @defgroup IO ops
    /// @{
    CODE("base",   PUSH(0)),   // dict[0]->pf[0]->q[0] used for base
    CODE("decimal",fout << setbase(BASE = 10)),
    CODE("hex",    fout << setbase(BASE = 16)),
    CODE("bl",     fout << " "),
    CODE("cr",     fout << ENDL),
    CODE(".",      fout << setbase(BASE) << POP() << " "),
    CODE(".r",     fout << setbase(BASE) << setw(POP()) << POP()),
    CODE("u.r",    fout << setbase(BASE) << setw(POP()) << abs(POP())),
    CODE("key",    PUSH(word()[0])),
    CODE("emit",   fout << (char)POP()),
    CODE("space",  fout << " "),
    CODE("spaces", fout << setw(POP()) << ""),
    CODE("type",   POP(); DU w_i = POP();            // len is not used
                   fout << dict[w_i >> 16]->pf[w_i & 0xffff]->name),
    /// @}
    /// @defgroup Literal ops
    /// @{
    IMMD("(",      word(')')),
    IMMD(".(",     fout << word(')')),
    IMMD("\\",     string s; getline(fin, s, '\n')), // flush input
    IMMD(".\"",
         string s = word('"').substr(1);
         dict[-1]->add(new Code(_str, s, 0))),
    IMMD("s\"",
         string s = word('"').substr(1);
         if (compile) {
             Code *last = dict[-1];
             DU    w_i  = (last->token << 16) | last->pf.size();
             last->add(new Code(_str, s, w_i));
         }
         else pad = s),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    ///     dict[-2]->pf[0,1,2,...,-1] as *last
    ///                              \--->pf[...] if  <--+ merge
    ///                               \-->p1[...] else   |
    ///     dict[-1]->pf[...] as *tmp -------------------+
    /// @{
    IMMD("if",
         dict[-1]->add(new Code(_bran, "_if"));
         dict.push(new Code(" tmp", false))),  // scratch pad
    IMMD("else",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         last->stage = 1),
    IMMD("then",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         int  b = last->stage;                 ///< branching state
         if (b==0) {                           
             last->pf.merge(tmp->pf);          /// * if.{pf}.then
             dict.pop();
         }
         else {                                /// * else.{p1}.then, or
             last->p1.merge(tmp->pf);          /// * then.{p1}.next
             if (b==1) dict.pop();             /// * if..else..then
         }),
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",
         dict[-1]->add(new Code(_cycle, "_begin"));
         dict.push(new Code(" tmp", false))),
    IMMD("while",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);                      /// * begin.{pf}.f.while
         last->stage = 2),
    IMMD("repeat",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->p1.merge(tmp->pf); dict.pop()),         /// * while.{p1}.repeat
    IMMD("again",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf); dict.pop();          /// * begin.{pf}.again
         last->stage = 1),
    IMMD("until",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf); dict.pop()),         /// * begin.{pf}.f.until
    /// @}
    /// @defgrouop FOR loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for",
         dict[-1]->add(new Code(_tor, ">r"));
         dict[-1]->add(new Code(_for, "_for"));
         dict.push(new Code(" tmp", false))),
    IMMD("aft",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);                      /// * for.{pf}.aft
         last->stage = 3),
    IMMD("next",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         if (last->stage==0) last->pf.merge(tmp->pf);  /// * for.{pf}.next
         else                last->p2.merge(tmp->pf);  /// * then.{p2}.next
         dict.pop()),
    /// @}
    /// @defgrouop DO loops
    /// @brief  - do...loop, do..leave..loop
    /// @{
    IMMD("do",
         dict[-1]->add(new Code(_dor, "swap >r >r"));  ///< ( limit first -- )
         dict[-1]->add(new Code(_doloop, "_do"));
         dict.push(new Code(" tmp", false))),
    CODE("i",      PUSH(rs[-1])),
    CODE("leave",
         rs.pop(); rs.pop(); throw runtime_error("")),
    IMMD("loop",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);      /// * do.{pf}.loop
         dict.pop()),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("exit",   throw runtime_error("")), // -- (exit from word)
    CODE("[",      compile = false),
    CODE("]",      compile = true),
    CODE(":",
         dict.push(new Code(word()));  // create new word
         compile = true),
    IMMD(";", compile = false),
    CODE("constant",
         dict.push(new Code(word()));
         Code *last = dict[-1]->add(new Code(_lit, POP()));
         last->pf[0]->token = last->token),
    CODE("variable",
         dict.push(new Code(word()));
         Code *last = dict[-1]->add(new Code(_var, 0));
         last->pf[0]->token = last->token),
    CODE("immediate", dict[-1]->immediate()),
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   dict[POP()]->exec()),              // w --
    CODE("create",
         dict.push(new Code(word()));
         Code *last = dict[-1]->add(new Code(_var, 0));
         last->pf[0]->token = last->token;
         last->pf[0]->q.pop()),
    IMMD("does>",
         dict[-1]->add(new Code(_does, "_does"));
         dict[-1]->pf[-1]->token = dict[-1]->token),  // keep WP
    CODE("to",                                        // n --
         Code *w=find(word()); if (!w) return;
         VAR(w->token) = POP()),                      // update value
    CODE("is",                                        // w -- 
         dict.push(new Code(word(), false));    // create word
         int w = POP();                               // like this word
         dict[-1]->xt = dict[w]->xt;                  // if primitive
         dict[-1]->pf = dict[w]->pf),                 // or colon word
    /// @}
    /// @defgroup Memory Access ops
    /// @{
    CODE("@",       DU w=POP(); PUSH(VAR(w))),                     // w -- n
    CODE("!",       DU w=POP(); VAR(w) = POP()),                   // n w --
    CODE("+!",      DU w=POP(); VAR(w) += POP()),                  // n w --
    CODE("?",       DU w=POP(); fout << VAR(w) << " "),            // w --
    CODE("array@",  DU i=POP(); int w=POP(); PUSH(*(&VAR(w)+i))),  // w i -- n
    CODE("array!",  DU i=POP(); int w=POP(); *(&VAR(w)+i)=POP()),  // n w i --
    CODE(",",       dict[-1]->pf[0]->q.push(POP())),
    CODE("allot",   int n = POP();                                 // n --
                    for (int i=0; i<n; i++) dict[-1]->pf[0]->q.push(0)),
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("here",    PUSH(dict[-1]->token)),
    CODE("'",       Code *w = find(word()); if (w) PUSH(w->token)),
    CODE(".s",      ss_dump(BASE)),    // dump parameter stack
    CODE("words",   words()),          // display word lists
    CODE("see",     Code *w = find(word()); if (w) see(w, 0); fout << ENDL),
    CODE("depth",   PUSH(ss.size())),  // data stack depth
    /// @}
    /// @defgroup OS ops
    /// @{
    CODE("mstat",   mem_stat()),       // display memory stat
    CODE("ms",      PUSH(millis())),   // get system clock in msec
    CODE("delay",   delay(POP())),     // n -- delay n msec
    CODE("included",
         const char *fn = pad.c_str(); // file name from cached string
         forth_include(fn)),           // load external Forth script
    CODE("forget",
         Code *w = find(word()); if (!w) return;
         int   t = max((int)w->token, find("boot")->token + 1);
         for (int i=dict.size(); i>t; i--) dict.pop()),
    CODE("boot",
         int t = find("boot")->token + 1;
         for (int i=dict.size(); i>t; i--) dict.pop()),
};
///
///> forward declared, post implementated
///
Code *find(string s) {      ///> scan dictionary, last to first
    for (int i = dict.size() - 1; i >= 0; --i) {
        if (s == dict[i]->name) return dict[i];
    }
    return NULL;            /// * word not found
}
void _does(Code *c) {
    bool hit = false;
    for (Code *w : dict[c->token]->pf) {
        if (hit) dict[-1]->add(w);          // copy rest of pf
        if (w->name=="does>") hit = true;
    }
    throw runtime_error("");                // exit caller
}
void words() {              ///> display word list
    const int WIDTH = 60;
    int x = 0;
    fout << setbase(16) << setfill('0');
    for (Code *w : dict) {
#if CC_DEBUG
        fout << setw(4) << w->token << "> "
             << setw(8) << (UFP)w->xt
             << ":" << (w->immd ? '*' : ' ')
             << w->name << "  " << ENDL;
#else
        fout << "  " << w->name;
        x += (w->name.size() + 2);
        if (x > WIDTH) { fout << ENDL; x = 0; }
#endif
    }
    fout << setfill(' ') << setbase(BASE) << ENDL;
}
///
///> setup user variables
///
void forth_init() {
    dict[0]->add(new Code(_var, 10)); /// * borrow dict[0] for base
#if DO_WASM
    fout << "WASM build" << endl;
#endif
}
///====================================================================
///
///> Forth outer interpreter
///
DU parse_number(string idiom) {
    const char *cs = idiom.c_str();
    int b = BASE;
    switch (*cs) {                    ///> base override
    case '%': b = 2;  cs++; break;
    case '&':
    case '#': b = 10; cs++; break;
    case '$': b = 16; cs++; break;
    }
    char *p;
    errno = 0;                       ///> clear overflow flag
#if DU==float
    DU n = (b==10)
        ? static_cast<DU>(strtof(cs, &p))
        : static_cast<DU>(strtol(cs, &p, b));
#else
    DU n = static_cast<DU>(strtol(cs, &p, b));
#endif
    if (errno || *p != '\0') throw runtime_error("");
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
    DU  n = parse_number(idiom);      ///< try as a number
    if (compile)
        dict[-1]->add(new Code(_lit, n));   /// * add to current word
    else PUSH(n);                           /// * add value to data stack
}
///
///> Forth VM - interface to outside world
///
void forth_vm(const char *cmd, void(*hook)(int, const char*)=NULL) {
    auto outer = []() {               ///< outer interpreter
        string idiom;
        while (fin >> idiom) {
            try { forth_core(idiom); }     ///> single command to Forth core
            catch(...) {
                fout << idiom << "? " << ENDL;
                compile = false;
                getline(fin, idiom, '\n'); /// * flush to end-of-line
            }
        }
    };
    auto cb = [](int,const char* rst) { cout << rst; };
    fout_cb = hook ? hook : cb;       ///> setup callback function
    
    printf(">>%s\n", cmd);
    istringstream istm(cmd);          ///< input stream
    string        line;               ///< one line of command
    fout.str("");                     ///> clean output buffer
    while (getline(istm, line)) {     /// * fetch line by line
        fin.clear();                  ///> clear input stream error bit if any
        fin.str(line);                ///> feed user command into input stream
        outer();                      /// * call Forth outer interpreter
    }
    if (!compile) ss_dump(BASE);
}
///====================================================================
