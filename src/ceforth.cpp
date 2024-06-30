///
/// @file
/// @brief eForth - C++ vector-based object-threaded implementation
///
///====================================================================
#include <sstream>                     /// iostream, stringstream
#include <cstring>
#include "ceforth.h"

using namespace std;
///
///> Forth VM state variables
///
FV<DU> ss;                             ///< data stack
FV<DU> rs;                             ///< return stack
DU     top     = -1;                   ///< cached top of stack
bool   compile = false;                ///< compiling flag
Code   *last;                          ///< cached dict[-1]
///
///> I/O streaming interface
///
istringstream   fin;                   ///< forth_in
ostringstream   fout;                  ///< forth_out
string          pad;                   ///< string buffers
void (*fout_cb)(int, const char*);     ///< forth output callback functi
///
///> macros to reduce verbosity (but harder to single-step debug)
///
inline  DU POP()     { DU n=top; top=ss.pop(); return n; }
#define PUSH(v)      (ss.push(top), top=(v))
#define BOOL(f)      ((f) ? -1 : 0)    /* Forth use -1 as true     */
#define IS_NA(w)     (!(w))            /* signify a word not found */
#define VAR(i)       (*dict[(int)(i)].pf[0].q.data())
#define DICT_PUSH(c) (dict.push((Code &)c), last=&dict[-1])
#define DICT_POP()   (dict.pop(), last=&dict[-1])
#define BRAN_TGT()   (dict[-2].pf[-1]) /* branching target */
#define BASE         (VAR(0))          /* borrow dict[0] to store base (numeric radix) */
///
///> Forth Dictionary Assembler
/// @note:
///    1. Dictionary is assembled by calling C++ initializer_list
///       Currently, C++ compiler instainciate them at start up.
///       We like to find a way to make it as a static ROM and
///       tokenize to make it portable.
///    2. Using __COUNTER__ for array token/index can potetially
///       make the dictionary static but need to be careful the
///       potential issue comes with it.
///    3. a degenerated lambda becomes a function pointer
///
#define NEXT()      { return c->next; }
#define CODE(s, g)  { s, [](Code *c)-> int { g; NEXT();   }, __COUNTER__ }
#define EXIT(s, g)  { s, [](Code *c)-> int { g; return 1; }, __COUNTER__ }
#define IMMD(s, g)  { s, [](Code *c)-> int { g; return 0; }, __COUNTER__ | Code::IMMD_FLAG }

FV<Code> dict = {
    EXIT("bye",    exit(0)),          // exit to OS
    ///
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
    /// @defgroup IO ops
    /// @{
    CODE("base",   PUSH(0)),   // dict[0].pf[0].q[0] used for base
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
    CODE("type",   POP(); DU i_w = POP();            // decode pf and word indices
                   fout << dict[i_w & 0xffff].pf[i_w >> 16].name),
    /// @}
    /// @defgroup Literal ops
    /// @{
    IMMD("(",      word(')')),
    IMMD(".(",     fout << word(')')),
    IMMD("\\",     string s; getline(fin, s, '\n')), // flush input
    IMMD(".\"",
         Str s(word('"').substr(1));
         last->append(s)),
    IMMD("s\"",
         pad = word('"').substr(1);                  // cache in pad var
         if (compile) {
             DU i_w = (last->pf.size() << 16) | last->token; // encode pf and word indices
             Str s(pad, i_w);
             last->append(s);
         }),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    ///     dict[-2].pf[0,1,2,...,-1] as *last
    ///                              \--->pf[...] if  <--+ merge
    ///                               \-->p1[...] else   |
    ///                                                  |
    ///     dict[-1].pf[...] as *tmp --------------------+
    /// @{
    IMMD("if",
         Bran b(_bran); Tmp t;
         last->append(b);
         DICT_PUSH(t)),                        /// as branch target
    IMMD("else",
         Code &b = BRAN_TGT();
         last->stop();
         b.pf.merge(last->pf);
         b.stage = 1),
    IMMD("then",
         Code &b = BRAN_TGT();
         int  s  = b.stage;                    ///< branching state
         if (s==0) {                           
             last->stop();
             b.pf.merge(last->pf);             /// * if.{pf}.then
             DICT_POP();
         }
         else {                                /// * else.{p1}.then, or
             last->stop();
             b.p1.merge(last->pf);             /// * then.{p1}.next
             if (s==1) DICT_POP();             /// * if..else..then
         }),
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",
         Bran b(_cycle); Tmp t;
         last->append(b);
         DICT_PUSH(t)),                        /// as branch target
    IMMD("while",
         Code &b = BRAN_TGT();
         last->stop();
         b.pf.merge(last->pf);                 /// * begin.{pf}.f.while
         b.stage = 2),
    IMMD("repeat",
         Code &b = BRAN_TGT();
         last->stop();
         b.p1.merge(last->pf); DICT_POP()),    /// * while.{p1}.repeat
    IMMD("again",
         Code &b = BRAN_TGT();
         last->stop();
         b.pf.merge(last->pf); DICT_POP();     /// * begin.{pf}.again
         b.stage = 1),
    IMMD("until",
         Code &b = BRAN_TGT();
         last->stop();
         b.pf.merge(last->pf); DICT_POP()),    /// * begin.{pf}.f.until
    /// @}
    /// @defgrouop FOR loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for",
         Bran r(_tor); Bran b(_for); Tmp t;
         last->append(r);
         last->append(b);
         DICT_PUSH(t)),                        /// as branch target
    IMMD("aft",
         Code &b = BRAN_TGT();
         last->stop();
         b.pf.merge(last->pf);                 /// * for.{pf}.aft
         b.stage = 3),
    IMMD("next",
         Code &b = BRAN_TGT();
         last->stop();
         if (b.stage==0) b.pf.merge(last->pf); /// * for.{pf}.next
         else            b.p2.merge(last->pf); /// * then.{p2}.next
         DICT_POP()),
    /// @}
    /// @defgrouop DO loops
    /// @brief  - do...loop, do..leave..loop
    /// @{
    IMMD("do",
         Bran r(_dor); Bran b(_doloop); Tmp t;
         last->append(r);              ///< ( limit first -- )
         last->append(b);
         DICT_PUSH(t)),
    CODE("i",      PUSH(rs[-1])),
    EXIT("leave", {}),                 /// * exit loop
    IMMD("loop",
         Code &b = BRAN_TGT();
         last->stop();
         b.pf.merge(last->pf);         /// * do.{pf}.loop
         DICT_POP()),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    EXIT("exit",   {}),                // -- (exit from word)
    CODE("[",      compile = false),
    CODE("]",      compile = true),
    CODE(":",
         Code w(word());
         DICT_PUSH(w);                 // create new word
         compile = true),
    IMMD(";",
         last->stop();
         compile = false),
    CODE("constant",
         Code w(word()); Lit v(POP());
         DICT_PUSH(w);
         Code &lit = last->append(v).stop();
         lit.pf[0].token = lit.token),
    CODE("variable",
         Code w(word()); Var v(DU0);
         DICT_PUSH(w);
         Code &var = last->append(v).stop();
         var.pf[0].token = var.token),
    CODE("immediate", last->immd = 1),
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   Code::exec(&dict[POP()])),     // w --
    CODE("create",
         Code w(word()); Var v(DU0);
         DICT_PUSH(w);
         Code &var = last->append(v).stop();
         var.pf[0].token = var.token;
         var.pf[0].q.pop()),
    IMMD("does>",
         Bran b(_does);
         last->append(b);
         last->pf[-1].token = last->token),       // keep WP
    CODE("to",                                    // n --
         Code *w=find(word()); if (IS_NA(w)) return 0;
         VAR(w->token) = POP()),                  // update value
    CODE("is",                                    // w --
         Code w(word(), false);
         DICT_PUSH(w);                            // create word
         Code &x = dict[POP()];                   // like this word
         last->xt = x.xt;                         // if primitive
         last->pf.merge(x.pf)),                   // or colon word
    /// @}
    /// @defgroup Memory Access ops
    /// @{
    CODE("@",       DU w=POP(); PUSH(VAR(w))),                     // w -- n
    CODE("!",       DU w=POP(); VAR(w) = POP()),                   // n w --
    CODE("+!",      DU w=POP(); VAR(w) += POP()),                  // n w --
    CODE("?",       DU w=POP(); fout << VAR(w) << " "),            // w --
    CODE("array@",  DU i=POP(); int w=POP(); PUSH(*(&VAR(w)+i))),  // w i -- n
    CODE("array!",  DU i=POP(); int w=POP(); *(&VAR(w)+i)=POP()),  // n w i --
    CODE(",",       last->pf[0].q.push(POP())),
    CODE("allot",   int n = POP();                                 // n --
                    for (int i=0; i<n; i++) last->pf[0].q.push(DU0)),
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("here",    PUSH(last->token)),
    CODE("'",       Code *w = find(word());
                    if (!IS_NA(w)) PUSH(w->token)),
    CODE(".s",      ss_dump(BASE)),    // dump parameter stack
    CODE("words",   words()),          // display word lists
    CODE("see",     Code *w = find(word());
                    if (!IS_NA(w)) see(*w, 0); fout << ENDL),
    CODE("depth",   PUSH(ss.size())),  // data stack depth
    /// @}
    /// @defgroup OS ops
    /// @{
    CODE("mstat",   mem_stat()),       // display memory stat
    CODE("ms",      PUSH(millis())),   // get system clock in msec
    CODE("delay",   delay(POP())),     // n -- delay n msec
    CODE("included", load(pad.c_str())),
    CODE("forget",
         Code *w = find(word()); if (IS_NA(w)) return 0;
         int  t  = max((int)w->token, find("boot")->token + 1);
         for (int i=dict.size(); i>t; i--) DICT_POP()),
    CODE("boot",
         int t = find("boot")->token + 1;
         for (int i=dict.size(); i>t; i--) DICT_POP()),
};
///====================================================================
///
///> Code Class constructors
///
Code::Code(const string s, bool t) {      ///< new colon word
    Code *w = find(s);                    /// * scan the dictionary
    name  = (new string(s))->c_str();
    xt    = IS_NA(w) ? NULL : w->xt;
    token = t ? dict.size() : 0;
    if (t && xt) fout << "reDef?";        /// * warn word redefined
}
///====================================================================
///
///> Primitive Functions
///
int _str(Code *c)  {
    if (c->token) { PUSH(c->token); PUSH(strlen(c->name)); }
    else fout << c->name;
    NEXT();
}
int _lit(Code *c)  { PUSH(c->q[0]); NEXT(); }
int _var(Code *c)  { PUSH(c->token); NEXT(); }
int _tor(Code *c)  { rs.push(POP()); NEXT(); }
int _dor(Code *c)  { rs.push(ss.pop()); rs.push(POP()); NEXT(); }
int _bran(Code *c) {
    Code::exec(POP() ? c->pf.data() : c->p1.data());
    NEXT();
}
int _cycle(Code *c) {            ///> begin.while.repeat, begin.until
    int b = c->stage;            ///< branching state
    while (true) {
        if (Code::exec(c->pf.data())) break;  /// * begin..
        if (b==0 && POP()!=0) break;            /// * ..until
        if (b==1)             continue;         /// * ..again
        if (b==2 && POP()==0) break;            /// * ..while..repeat
        if (Code::exec(c->p1.data())) break;
    }
    NEXT();
}
int _for(Code *c) {             ///> for..next
    int b = c->stage;                    /// * kept in register
    do {
        printf("_for %d\n", rs[-1]);
        if (Code::exec(c->pf.data())) break;
    } while (b==0 && (rs[-1]-=1) >=0);          /// * for...next only
    while (b) {                                 /// * aft
        if (Code::exec(c->p2.data())) break;  /// * then...next
        if ((rs[-1]-=1) < 0) break;             /// * decrement counter
        if (Code::exec(c->p1.data())) break;  /// * aft...then
    }
    rs.pop();
    printf("_for done t=%x\n", c->attr);
    NEXT();
}
int _doloop(Code *c) {      ///> do..loop
    do {
        if (Code::exec(c->pf.data())) break;
    } while ((rs[-1]+=1) < rs[-2]);      // increment counter
    rs.pop(); rs.pop();
    NEXT();
}
int _does(Code *c) {
    bool hit = false;
    for (Code &w : dict[c->token].pf) {
        if (hit) last->append(w);           // copy rest of pf
        if (strcmp(w.name, "_does")==0) hit = true;
    }
    return 1;                               // exit caller
}
///====================================================================
///
///> IO functions
///
string word(char delim) {            ///> read next idiom form input stream
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
    ss.push(top);
    for (DU v : ss) { fout << rdx(v, base) << ' '; }
    top = ss.pop();
    fout << "-> ok" << ENDL;
}
void see(Code &c, int dp) {  ///> disassemble a colon word
    auto pp = [](int dp, string s, FV<Code> &v) {     ///> recursive dump with indent
        int i = dp; fout << ENDL; while (i--) fout << "  "; fout << s;
        for (Code &w : v) if (dp < 3) see(w, dp + 1); /// * depth controlled
    };
    string bn = c.stage==2 ? "_whie" : (c.stage==3 ? "_aft" : "_else");
    string sn(c.name);
    if (c.is_str) sn = (c.token ? "s\" " : ".\" ") + sn + "\"";
    if (c.next)   sn += " ~";

    pp(dp, sn, c.pf);
    if (c.p1.size() > 0) pp(dp, bn, c.p1);
    if (c.p2.size() > 0) pp(dp, "_then", c.p2);
    if (c.q.size()  > 0) for (DU i : c.q) fout << i << " ";
}
void words() {              ///> display word list
    const int WIDTH = 60;
    int x = 0;
    fout << setbase(16) << setfill('0');
    for (Code &w : dict) {
#if CC_DEBUG
        fout << setw(4) << w.token << "> "
             << (UFP)&w << ' '
             << setw(8) << static_cast<U32>((UFP)w.xt)
             << (w.is_str ? '"' : ':') << (w.immd ? '*' : ' ')
             << w.name << "  " << ENDL;
#else // !CC_DEBUG
        fout << "  " << w.name;
        x += (strlen(w.name) + 2);
        if (x > WIDTH) { fout << ENDL; x = 0; }
#endif // CC_DEBUG
    }
    fout << setfill(' ') << setbase(BASE) << ENDL;
}
void load(const char *fn) {            ///> include script from stream
    void (*cb)(int, const char*) = fout_cb;  ///< keep output function
    string in; getline(fin, in);             ///< keep input buffers
    fout << ENDL;                            /// * flush output
    
    forth_include(fn);                       /// * send script to VM
    
    fout_cb = cb;                            /// * restore output cb
    fin.clear(); fin.str(in);                /// * restore input
}
///====================================================================
///
///> Forth outer interpreter
///
Code *find(string s) {         ///> scan dictionary, last to first
    for (int i = dict.size() - 1; i >= 0; --i) {
        if (s == dict[i].name) return &dict[i];
    }
    return NULL;              /// * word not found, use IS_NA(w) to check
}

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
    errno = 0;                        ///> clear overflow flag
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
    if (!IS_NA(w)) {                  /// * word found?
        if (compile && !w->immd)      /// * are we compiling new word?
            last->append(*w);         /// * append word ptr to it
        else Code::exec(w);           /// * execute forth word
        return;
    }
    DU  n = parse_number(idiom);      ///< try as a number
    if (compile) {                    /// * are we compiling new word?
        Lit v(n);
        last->append(v);              /// * append numeric literal to it
    }
    else PUSH(n);                     /// * add value to data stack
}
///====================================================================
///
///> Forth VM - interface to outside world
///
///
///> setup user variables
///
void forth_init() {
    Var v(10);
    dict[0].append(v);                /// * borrow dict[0] for base
    last = &dict[-1];                 /// * cache last word
#if DO_WASM
    fout << "WASM build" << endl;
#endif
}

void forth_vm(const char *cmd, void(*hook)(int, const char*)=NULL) {
    auto outer = []() {               ///< outer interpreter
        string idiom;
        while (fin >> idiom) {
            try { forth_core(idiom); }     ///> single command to Forth core
#if CC_DEBUG            
            catch(exception &e) {          /// * 6% slower?
                fout << idiom << "? " << e.what() << ENDL;
#else // !CC_DEBUG
            catch(...) {
                fout << idiom << "? " << ENDL;
#endif // CC_DEBUG                
                compile = false;
                getline(fin, idiom, '\n'); /// * flush to end-of-line
            }
        }
    };
    auto cb = [](int,const char* rst) { cout << rst; };
    fout_cb = hook ? hook : cb;       ///> setup callback function
    
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
