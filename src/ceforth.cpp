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
FV<Code*> dict;                        ///< Forth dictionary
FV<DU>    ss;                          ///< data stack
FV<DU>    rs;                          ///< return stack
DU        tos     = -1;                ///< cached top of stack
bool      compile = false;             ///< compiling flag
Code      *last;                       ///< cached dict[-1]
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
inline  DU POP()     { DU n=tos; tos=ss.pop(); return n; }
#define PUSH(v)      (ss.push(tos), tos=(v))
#define BOOL(f)      ((f) ? -1 : 0)
#define VAR(i_w)     (*(dict[(int)((i_w) & 0xffff)]->pf[0]->q.data()+((i_w) >> 16)))
#define DICT_PUSH(c) (dict.push(last=(c)))
#define DICT_POP()   (dict.pop(), last=dict[-1])
#define BRAN_TGT()   (dict[-2]->pf[-1]) /* branching target */
#define BASE         (VAR(0))           /* borrow dict[0] to store base (numeric radix) */
#define STR(i_w)     (                                  \
        EQ(i_w, UINT(-DU1))                             \
        ? pad.c_str()                                   \
        : dict[(i_w) & 0xffff]->pf[(i_w) >> 16]->name   \
        )
///
///> Forth Dictionary Assembler
/// @note:
///    1. Dictionary construction sequence
///       * Code rom[] in statically build in compile-time
///       * vector<Code*> dict is populated in forth_init, i.e. first thing in main()
///    2. Using __COUNTER__ for array token/index can potetially
///       make the dictionary static but need to be careful the
///       potential issue comes with it.
///    3. a degenerated lambda becomes a function pointer
///
#define CODE(s, g)  { s, #g, [](Code *c){ g; }, __COUNTER__ }
#define IMMD(s, g)  { s, #g, [](Code *c){ g; }, __COUNTER__ | Code::IMMD_FLAG }

void _if();
const Code rom[] = {               ///< Forth dictionary
    CODE("bye",    exit(0)),       // exit to OS
    ///
    /// @defgroup ALU ops
    /// @{
    CODE("+",      tos += ss.pop()),
    CODE("-",      tos =  ss.pop() - tos),
    CODE("*",      tos *= ss.pop()),
    CODE("/",      tos =  ss.pop() / tos),
    CODE("mod",    tos =  MOD(ss.pop(), tos)),
    CODE("*/",     tos =  ss.pop() * ss.pop() / tos),
    CODE("/mod",   DU n = ss.pop(); DU t = tos;
                   DU m = MOD(n, t);
                   ss.push(m); tos = UINT(n / t)),
    CODE("*/mod",  DU2 n = (DU2)ss.pop() * ss.pop(); DU2 t=tos;
                   DU2 m = MOD(n, t);
                   ss.push((DU)m); tos = UINT(n / t)),
    CODE("and",    tos = UINT(tos) & UINT(ss.pop())),
    CODE("or",     tos = UINT(tos) | UINT(ss.pop())),
    CODE("xor",    tos = UINT(tos) ^ UINT(ss.pop())),
    CODE("abs",    tos =  ABS(tos)),
    CODE("negate", tos =  -tos),
    CODE("invert", tos =  ~UINT(tos)),
    CODE("rshift", tos =  UINT(ss.pop()) >> UINT(tos)),
    CODE("lshift", tos =  UINT(ss.pop()) << UINT(tos)),
    CODE("max",    DU n=ss.pop(); tos = (tos>n) ? tos : n),
    CODE("min",    DU n=ss.pop(); tos = (tos<n) ? tos : n),
    CODE("2*",     tos *= 2),
    CODE("2/",     tos /= 2),
    CODE("1+",     tos += 1),
    CODE("1-",     tos -= 1),
#if USE_FLOAT
    CODE("int",    tos = UINT(tos)),
#endif // USE_FLOAT
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0=",     tos = BOOL(ZEQ(tos))),
    CODE("0<",     tos = BOOL(LT(tos, DU0))),
    CODE("0>",     tos = BOOL(GT(tos, DU0))),
    CODE("=",      tos = BOOL(EQ(ss.pop(), tos))),
    CODE(">",      tos = BOOL(GT(ss.pop(), tos))),
    CODE("<",      tos = BOOL(LT(ss.pop(), tos))),
    CODE("<>",     tos = BOOL(!EQ(ss.pop(), tos))),
    CODE(">=",     tos = BOOL(!LT(ss.pop(), tos))),
    CODE("<=",     tos = BOOL(!GT(ss.pop(), tos))),
    CODE("u<",     tos = BOOL(UINT(ss.pop()) < UINT(tos))),
    CODE("u>",     tos = BOOL(UINT(ss.pop()) > UINT(tos))),
    /// @}
    /// @defgroup Data Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",    PUSH(tos)),
    CODE("drop",   tos=ss.pop()),  // note: ss.pop() != POP()
    CODE("swap",   DU n = ss.pop(); PUSH(n)),
    CODE("over",   PUSH(ss[-2])),
    CODE("rot",    DU n = ss.pop(); DU m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("-rot",   DU n = ss.pop(); DU m = ss.pop(); PUSH(m);  PUSH(n)),
    CODE("pick",   tos = ss[-tos]),
    CODE("nip",    ss.pop()),
    CODE("?dup",   if (tos != DU0) PUSH(tos)),
    /// @}
    /// @defgroup Data Stack ops - double
    /// @{
    CODE("2dup",   PUSH(ss[-2]); PUSH(ss[-2])),
    CODE("2drop",  ss.pop(); tos=ss.pop()),
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
    CODE("type",   POP(); U32 i_w=UINT(POP()); fout << STR(i_w)),
    /// @}
    /// @defgroup Literal ops
    /// @{
    IMMD("(",      word(')')),
    IMMD(".(",     fout << word(')')),
    IMMD("\\",     string s; getline(fin, s, '\n')), // flush input
    IMMD(".\"",
         string s = word('"').substr(1);
         last->append(new Str(s))),
    IMMD("s\"",
         string s = word('"').substr(1);
         if (compile) {
             last->append(new Str(s, last->token, last->pf.size()));
         }
         else {
             pad = s;                                // keep string on pad
             PUSH(-DU1); PUSH(s.length());           // -1 = pad, len
         }),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    ///     dict[-2]->pf[0,1,2,...,-1] as *last
    ///                              \--->pf[...] if  <--+ merge
    ///                               \-->p1[...] else   |
    ///     dict[-1]->pf[...] as *tmp -------------------+
    /// @{
    IMMD("if",
         last->append(new Bran(_if));
         DICT_PUSH(new Tmp())),
    IMMD("else",
         Code *b = BRAN_TGT();
         b->pf.merge(last->pf);
         b->stage = 1),
    IMMD("then",
         Code *b = BRAN_TGT();
         int  s  = b->stage;                   ///< branching state
         if (s==0) {
             b->pf.merge(last->pf);            /// * if.{pf}.then
             DICT_POP();
         }
         else {                                /// * else.{p1}.then, or
             b->p1.merge(last->pf);            /// * then.{p1}.next
             if (s==1) DICT_POP();             /// * if..else..then
         }),
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",
         last->append(new Bran(_begin));
         DICT_PUSH(new Tmp())),                /// as branch target
    IMMD("while",
         Code *b = BRAN_TGT();
         b->pf.merge(last->pf);                /// * begin.{pf}.f.while
         b->stage = 2),
    IMMD("repeat",
         Code *b = BRAN_TGT();
         b->p1.merge(last->pf); DICT_POP()),   /// * while.{p1}.repeat
    IMMD("again",
         Code *b = BRAN_TGT();
         b->pf.merge(last->pf); DICT_POP();    /// * begin.{pf}.again
         b->stage = 1),
    IMMD("until",
         Code *b = BRAN_TGT();
         b->pf.merge(last->pf); DICT_POP()),   /// * begin.{pf}.f.until
    /// @}
    /// @defgrouop FOR loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for",
         last->append(new Bran(_tor));
         last->append(new Bran(_for));
         DICT_PUSH(new Tmp())),                /// as branch target
    IMMD("aft",
         Code *b = BRAN_TGT();
         b->pf.merge(last->pf);                /// * for.{pf}.aft
         b->stage = 3),
    IMMD("next",
         Code *b = BRAN_TGT();
         if (b->stage==0) b->pf.merge(last->pf);  /// * for.{pf}.next
         else             b->p2.merge(last->pf);  /// * then.{p2}.next
         DICT_POP()),
    /// @}
    /// @defgrouop DO loops
    /// @brief  - do...loop, do..leave..loop
    /// @{
    IMMD("do",
         last->append(new Bran(_tor2)); ///< ( limit first -- )
         last->append(new Bran(_loop));
         DICT_PUSH(new Tmp())),
    CODE("i",      PUSH(rs[-1])),
    CODE("leave",
         rs.pop(); rs.pop(); throw 0), /// * exit loop
    IMMD("loop",
         Code *b = BRAN_TGT();
         b->pf.merge(last->pf);        /// * do.{pf}.loop
         DICT_POP()),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("exit",   throw 0),           // -- (exit from word)
    CODE("[",      compile = false),
    CODE("]",      compile = true),
    CODE(":",
         DICT_PUSH(new Code(word()));  // create new word
         compile = true),
    IMMD(";", compile = false),
    CODE("constant",
         DICT_PUSH(new Code(word()));
         Code *w = last->append(new Lit(POP()));
         w->pf[0]->token = w->token),
    CODE("variable",
         DICT_PUSH(new Code(word()));
         Code *w = last->append(new Var(DU0));
         w->pf[0]->token = w->token),
    CODE("immediate", last->immd = 1),
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   dict[POP()]->exec()),              // w --
    CODE("create",
         DICT_PUSH(new Code(word()));
         Code *w = last->append(new Var(DU0));
         w->pf[0]->token = w->token;
         w->pf[0]->q.pop()),
    IMMD("does>",
         last->append(new Bran(_does));
         last->pf[-1]->token = last->token),          // keep WP
    CODE("to",                                        // n --
         Code *w=find(word()); if (!c) return;
         VAR(w->token) = POP()),                      // update value
    CODE("is",                                        // w -- 
         DICT_PUSH(new Code(word(), false));          // create word
         int w = POP();                               // like this word
         last->xt = dict[w]->xt;                      // if primitive
         last->pf = dict[w]->pf),                     // or colon word
    /// @}
    /// @defgroup Memory Access ops
    /// @{
    CODE("@",       U32 i_w = UINT(POP()); PUSH(VAR(i_w))),           // a -- n
    CODE("!",       U32 i_w = UINT(POP()); VAR(i_w) = POP()),         // n a -- 
    CODE("+!",      U32 i_w = UINT(POP()); VAR(i_w) += POP()),
    CODE("?",       U32 i_w = UINT(POP()); fout << VAR(i_w) << " "),
    CODE(",",       last->pf[0]->q.push(POP())),
    CODE("allot",   U32 n = UINT(POP());                              // n --
                    for (U32 i=0; i<n; i++) last->pf[0]->q.push(DU0)),
    ///> Note:
    ///>   allot allocate elements in a word's q[] array
    ///>   to access, both indices to word itself and to q array are needed
    ///>   'th' a word that compose i_w, a 32-bit value, the 16 high bits
    ///>   serves as the q index and lower 16 lower bit as word index
    ///>   so a variable (array with 1 element) can be access as usual
    ///>
    CODE("th",      U32 i = UINT(POP()) << 16; tos = UINT(tos) | i),  // w i -- i_w
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("here",    PUSH(last->token)),
    CODE("'",       Code *w = find(word()); if (w) PUSH(w->token)),
    CODE(".s",      ss_dump(BASE)),      // dump parameter stack
    CODE("words",   words()),            // display word lists
    CODE("see",     Code *w = find(word()); if (w) see(w); fout << ENDL),
    CODE("depth",   PUSH(ss.size())),    // data stack depth
    /// @}
    /// @defgroup OS ops
    /// @{
    CODE("mstat",   mem_stat()),         // display memory stat
    CODE("ms",      PUSH(millis())),     // get system clock in msec
    CODE("rnd",     PUSH(RND())),        // get a random number
    CODE("delay",   delay(UINT(POP()))), // n -- delay n msec
    CODE("included",
         POP(); U32 i_w = UINT(POP()); load(STR(i_w))),
    CODE("forget",
         Code *w = find(word()); if (!w) return;
         int   t = max((int)w->token, find("boot")->token + 1);
         for (int i=dict.size(); i>t; i--) DICT_POP()),
    CODE("boot",
         int t = find("boot")->token + 1;
         for (int i=dict.size(); i>t; i--) DICT_POP()),
};
///====================================================================
///
///> Code Class constructors
///
Code::Code(const char *s, const char *d, XT fp, U32 a)  ///> primitive word
    : name(s), desc(d), xt(fp), attr(a) {}
Code::Code(string s, bool n) {           ///< new colon word
    Code *w = find(s);                   /// * scan the dictionary
    name  = (new string(s))->c_str();
    desc  = "";
    xt    = w ? w->xt : NULL;
    token = n ? dict.size() : 0;
    if (n && w) fout << "reDef?";        /// * warn word redefined
}
///====================================================================
///
///> Primitive Functions
///
void _str(Code *c)  {
    if (!c->token) fout << c->name;
    else { PUSH(c->token); PUSH(strlen(c->name)); }
}
void _lit(Code *c)  { PUSH(c->q[0]);  }
void _var(Code *c)  { PUSH(c->token); }
void _tor(Code *c)  { rs.push(POP()); }
void _tor2(Code *c) { rs.push(ss.pop()); rs.push(POP()); }
void _if(Code *c)   {
    for (Code *w : (POP() ? c->pf : c->p1)) w->exec();
}
void _begin(Code *c) {           ///> begin.while.repeat, begin.until
    int b = c->stage;            ///< branching state
    while (true) {
        for (Code *w : c->pf) w->exec();     /// * begin..
        if (b==0 && POP()!=0) break;         /// * ..until
        if (b==1)             continue;      /// * ..again
        if (b==2 && POP()==0) break;         /// * ..while..repeat
        for (Code *w : c->p1) w->exec();
    }
}
void _for(Code *c) {             ///> for..next, for..aft..then..next
    int b = c->stage;                        /// * kept in register
    try {
        do {
            for (Code *w : c->pf) w->exec();
        } while (b==0 && (rs[-1]-=1) >=0);   /// * for..next only
        while (b) {                          /// * aft
            for (Code *w : c->p2) w->exec(); /// * then..next
            if ((rs[-1]-=1) < 0) break;      /// * decrement counter
            for (Code *w : c->p1) w->exec(); /// * aft..then
        }
        rs.pop();
    }
    catch (...) { rs.pop(); }                // handle EXIT
}
void _loop(Code *c) {           ///> do..loop
    try { 
        do {
            for (Code *w : c->pf) w->exec();
        } while ((rs[-1]+=1) < rs[-2]);      // increment counter
        rs.pop(); rs.pop();
    }
    catch (...) {}                           // handle LEAVE
}
void _does(Code *c) {
    bool hit = false;
    for (Code *w : dict[c->token]->pf) {
        if (hit) last->append(w);           // copy rest of pf
        if (strcmp(w->name, "does>")==0) hit = true;
    }
    throw 0;                                // exit caller
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
#if USE_FLOAT
        sprintf(buf, "%0.6g", v);
        return buf;
#else // !USE_FLOAT
        int i = 33;  buf[i]='\0';         /// * C++ can do only 8,10,16
        DU  n = ABS(v);                   ///< handle negative
        do {
            U8 d = (U8)MOD(n, b);  n /= b;
            buf[--i] = d > 9 ? (d-10)+'a' : d+'0';
        } while (n && i);
        if (v < 0) buf[--i]='-';
        return &buf[i];
#endif // USE_FLOAT
    };
    ss.push(tos);
    for (DU v : ss) { fout << rdx(v, base) << ' '; }
    tos = ss.pop();
    fout << "-> ok" << ENDL;
}
void _see(Code *c, int dp) {         ///> disassemble a colon word
    auto pp = [](string s, FV<Code*> v, int dp) {  ///> recursive dump with indent
        int i = dp;
        if (dp && s!="\t") fout << ENDL;           ///> newline control
        while (i--) { fout << "  "; } fout << s;   ///> indentation control
        if (dp < 2) for (Code *w : v) _see(w, dp + 1);
    };
    auto pq = [](FV<DU> q) {
        for (DU i : q) fout << i << (q.size() > 1 ? " " : "");
    };
    const FV<Code*> zz = {};
    string sn(c->name);
    if (c->is_str) sn = (c->token ? "s\" " : ".\" ") + sn + "\"";
    pp(sn, c->pf, dp);
    if (sn=="if")    {
        if (c->stage==1) pp("else", c->p1, dp);
        pp("then", zz, dp);
    }
    else if (sn=="begin") {
        switch (c->stage) {
        case 0: pp("until", zz, dp); break;
        case 1: pp("again", zz, dp); break;
        case 2:
            pp("while",  c->p1, dp);
            pp("repeat", zz,    dp);
            break;
        }
    }
    else if (sn=="for") {
        if (c->stage==3) {
            pp("aft",  c->p1, dp);
            pp("then", c->p2, dp);
        }
        pp("next", zz, dp);
    }
    else if (sn=="do") {
        pp("loop", zz, dp);
    }
    else pq(c->q);
}
void see(Code *c) {
    if (c->xt) fout << "  ->{ " << c->desc << "; }";
    else {
        fout << ": "; _see(c, 0); fout << " ;";
    }
}
void words() {              ///> display word list
    const int WIDTH = 60;
    int x = 0;
    fout << setbase(16) << setfill('0');
    for (Code *w : dict) {
#if CC_DEBUG > 1
        fout << setw(4) << w->token << "> "
             << (UFP)w << ' '
             << setw(8) << static_cast<U32>((UFP)w->xt)
             << (w->is_str ? '"' : ':') << (w->immd ? '*' : ' ')
             << w->name << "  " << ENDL;
#else // !CC_DEBUG
        fout << "  " << w->name;
        x += (strlen(w->name) + 2);
        if (x > WIDTH) { fout << ENDL; x = 0; }
#endif // CC_DEBUG
    }
    fout << setfill(' ') << setbase(BASE) << ENDL;
}
void load(const char *fn) {          ///> include script from stream
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
Code *find(string s) {      ///> scan dictionary, last to first
    for (int i = dict.size() - 1; i >= 0; --i) {
        if (s == dict[i]->name) return dict[i];
    }
    return NULL;            /// * word not found
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
        if (compile && !w->immd)      /// * are we compiling new word?
            last->append(w);          /// * append word ptr to it
        else w->exec();               /// * execute forth word
        
        return;
    }
    DU  n = parse_number(idiom);      ///< try as a number
    if (compile)                      /// * are we compiling new word?
        last->append(new Lit(n));     /// * append numeric literal to it
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
    const int sz = (int)(sizeof(rom))/(sizeof(Code));
    dict.reserve(sz * 2);             /// * pre-allocate vector
    for (int i = 0; i < sz; i++) {    /// * collect Code pointers
        DICT_PUSH((Code*)&rom[i]);
    }
    dict[0]->append(new Var(10));     /// * borrow dict[0] for base
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
