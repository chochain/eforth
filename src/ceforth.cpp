#include <iomanip>          // setbase, setw, setfill
#include "ceforth.h"
///
/// version info
///
#define APP_NAME         "eForth"
#define MAJOR_VERSION    "8"
#define MINOR_VERSION    "1"
///
/// macros to abstract dict and pmem physical implementation
/// Note:
///   so we can change pmem implementation anytime without affecting opcodes defined below
///
#define PFA(w)    ((U8*)&pmem[dict[w].pfa]) /** parameter field pointer of a word        */
#define HERE      (pmem.idx)                /** current parameter memory index           */
#define OFF(ip)   ((IU)((U8*)(ip) - MEM0))  /** IP offset (index) in parameter memory    */
#define MEM(ip)   (MEM0 + *(IU*)(ip))       /** pointer to IP address fetched from pmem  */
#define CELL(a)   (*(DU*)&pmem[a])          /** fetch a cell from parameter memory       */
#define SETJMP(a) (*(IU*)&pmem[a])          /** address offset for branching opcodes     */
#define BOOL(f)   ((f)?-1:0)
#define INT(f)    (static_cast<int>(f))
///
/// macros for ESP memory space access (be very careful of these)
/// note: 4000_0000 is instruction bus, access needs to be 32-bit aligned
///       3fff_ffff and below is data bus, no alignment requirement
///
#define PEEK(a)        (U32)(*(U32*)((UFP)(a)))
#define POKE(a, c)     (*(U32*)((UFP)(a))=(U32)(c))
///
/// metacompiler opcodes sequence
///
enum {
    EXIT = 0, DOVAR, DOLIT, DOSTR, DOTSTR, BRAN, ZBRAN, DONEXT, DOES, TOR
} forth_opcode;
///
/// global memory blocks
///
List<DU,   E4_SS_SZ>   rs;             /// return stack
List<DU,   E4_RS_SZ>   ss;             /// parameter stack
List<Code, E4_DICT_SZ> dict;           /// dictionary
List<U8,   E4_PMEM_SZ> pmem;           /// parameter memory (for colon definitions)
U8  *MEM0 = &pmem[0];                  /// base of cached memory
UFP DICT0;                             /// base of dictionary
///
/// VM IO streams
///
istream *fin;                          /// VM stream input
ostream *fout;                         /// VM stream output
///
/// VM state variables
///
bool    compile = false;               /// compiling flag
bool    ucase   = true;                /// case sensitivity control
DU      base    = 10;                  /// numeric radix
DU      top     = DVAL;                /// top of stack (cached)
IU      WP      = 0;                   /// current word
U8      *IP     = MEM0;                /// current intruction pointer
string  idiom;
///
/// compiler methods
///
void add_iu(IU i) { pmem.push((U8*)&i, sizeof(IU));  dict[-1].len += sizeof(IU); }  /** add an instruction into pmem */
void add_du(DU v) { pmem.push((U8*)&v, sizeof(DU)),  dict[-1].len += sizeof(DU); }  /** add a cell into pmem         */
void add_str(const char *s) {                                               /** add a string to pmem         */
        int sz = STRLEN(s); pmem.push((U8*)s,  sz); dict[-1].len += sz;
}
void add_w(IU w) {
    Code *c  = &dict[w];
    IU   ipx = c->def ? (c->pfa | 1) : (IU)((UFP)c->xt - DICT0);
    add_iu(ipx);
    printf("add_w(%d) => %4x:%p %s\n", w, ipx, c->xt, c->name);
}
string &next_idiom(char delim=0) {
    delim ? getline(*fin, idiom, delim) : *fin >> idiom; return idiom;
}
void  colon(const char *name) {
    char *nfa = (char*)&pmem[HERE];         // current pmem pointer
    int sz = STRLEN(name);                  // string length, aligned
    pmem.push((U8*)name,  sz);              // setup raw name field
#if LAMBDA_OK
    Code c(nfa, [](){});                    // create a new word on dictionary
#else  // LAMBDA_OK
    Code c(nfa, NULL);
#endif // LAMBDA_OK
    c.def = 1;                              // specify a colon word
    c.len = 0;                              // advance counter (by number of U16)
    c.pfa = HERE;                           // capture code field index
    dict.push(c);                           // deep copy Code struct into dictionary
    printf("%3d> pfa=%x, name=%4x:%p %s\n", dict.idx-1,
        dict[-1].pfa,
        (U16)(dict[EXIT].name - (const char*)MEM0),
        dict[-1].name, dict[-1].name);
}
void  colon(string &s) { colon(s.c_str()); }
///
/// dictionary search methods
///
int   pfa2word(U8 *ip) {
    IU   ipx = *(IU*)ip;
    U8   *fp = (U8*)(DICT0 + ipx);
    for (int i = dict.idx - 1; i >= 0; --i) {
        if (ipx & 1) {
            if (dict[i].pfa == (ipx & ~1)) return i;
        }
        else if ((U8*)dict[i].xt == fp) return i;
    }
    return -1;
}
int   streq(const char *s1, const char *s2) {
    return ucase ? strcasecmp(s1, s2)==0 : strcmp(s1, s2)==0;
}
int   find(const char *s) {
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (streq(s, dict[i].name)) return i;
    }
    return -1;
}
int   find(string &s) { return find(s.c_str()); }
///
/// VM ops
///
void  PUSH(DU v) { ss.push(top); top = v; }
DU    POP()      { DU n = top; top = ss.pop(); return n; }
void  nest() {
    int dp = 0;                                      /// iterator depth control
    while (dp >= 0) {
        /// function core
        auto ipx = *(IU*)IP;                         /// hopefully use register than cached line
        while (ipx) {
            if (ipx & 1) {
                rs.push(WP);                         /// * setup callframe (ENTER)
                rs.push(OFF(IP) + sizeof(IU));
                IP = MEM0 + (ipx & ~0x1);            /// word pfa (def masked)
                dp++;
            }
            else {
                UFP xt = DICT0 + (ipx & ~0x3);       /// * function pointer
                IP += sizeof(IU);                    /// advance to next pfa
                (*(fop*)xt)();
            }
            ipx = *(IU*)IP;
        }
        if (dp-- > 0) {
            IP = MEM0 + rs.pop();                    /// * restore call frame (EXIT)
            WP = rs.pop();
        }
    }
    yield();                                ///> give other tasks some time
}
void  call(IU w) {
    if (dict[w].def) {
        WP = w;
        IP = MEM0 + dict[w].pfa;
        nest();
    }
    else {
#if LAMBDA_OK
        (*(fop*)((UFP)dict[w].xt & ~0x3))();
#else  // LAMBDA_OK
        (*(fop)(DICT0 + (*(IU*)IP & ~0x3)))();
#endif // LAMBDA_OK
    }
}
///
/// debug functions
///
void dot_r(int n, int v) {
    *fout << setw(n) << setfill(' ') << v;
}
void to_s(IU c) {
    *fout << dict[c].name << " " << c << (dict[c].immd ? "* " : " ");
}
///
/// recursively disassemble colon word
///
void see(U8 *ip, int dp=0) {
    while (*(IU*)ip) {
        *fout << ENDL; for (int i=dp; i>0; i--) *fout << "  ";      // indentation
        *fout << setw(4) << OFF(ip) << "[ " << setw(-1);
        IU c = pfa2word(ip);
        to_s(c);                                                    // name field
        if (dict[c].def && dp <= 2) {                               // is a colon word
            see(PFA(c), dp+1);                                      // recursive into child PFA
        }
        ip += sizeof(IU);
        switch (c) {
        case DOVAR: case DOLIT:
            *fout << "= " << *(DU*)ip; ip += sizeof(DU); break;
        case DOSTR: case DOTSTR:
            *fout << "= \"" << (char*)ip << '"';
            ip += STRLEN((char*)ip); break;
        case BRAN: case ZBRAN: case DONEXT:
            *fout << "j" << *(IU*)ip; ip += sizeof(IU); break;
        }
        *fout << "] ";
    }
}
void words() {
    *fout << setbase(16);
    for (int i=0; i<dict.idx; i++) {
        if ((i%10)==0) { *fout << ENDL; yield(); }
        to_s(i);
    }
    *fout << setbase(base);
}
void ss_dump() {
    *fout << " <"; for (int i=0; i<ss.idx; i++) { *fout << ss[i] << " "; }
    *fout << top << "> ok" << ENDL;
}
void mem_dump(IU p0, DU sz) {
    *fout << setbase(16) << setfill('0') << ENDL;
    for (IU i=ALIGN16(p0); i<=ALIGN16(p0+sz); i+=16) {
        *fout << setw(4) << i << ": ";
        for (int j=0; j<16; j++) {
            U8 c = pmem[i+j];
            *fout << setw(2) << (int)c << (j%4==3 ? "  " : " ");
        }
        for (int j=0; j<16; j++) {   // print and advance to next byte
            U8 c = pmem[i+j] & 0x7f;
            *fout << (char)((c==0x7f||c<0x20) ? '_' : c);
        }
        *fout << ENDL;
        yield();
    }
    *fout << setbase(base);
}
///
/// macros to reduce verbosity (but harder to single-step debug)
///
#define CODE(s, g) { s, []{ g; }, 0 }
#define IMMD(s, g) { s, []{ g; }, 1 }
//#define CODE(s, g)     make_shared<Code>(string(s), [&](Code& c){ g; })
//#define IMMD(s, g)     make_shared<Code>(string(s), [&](Code& c){ g; }, true)
//#define WORD()         make_shared<Code>(next_idiom(), true)
///
/// dictionary initializer
///
/// Note: sequenced by enum forth_opcode as following
void _init() {
    const Code prim[] = {
    ///
    /// @defgroup Execution flow ops
    /// @brief - DO NOT change the sequence here (see forth_opcode enum)
    /// @{
    CODE("exit",    {}),
    CODE("dovar",   PUSH(OFF(IP)); IP += sizeof(DU)),
    CODE("dolit",   PUSH(*(DU*)IP); IP += sizeof(DU)),
    CODE("dostr",
        const char *s = (const char*)IP;           // get string pointer
        PUSH(OFF(IP)); IP += STRLEN(s)),
    CODE("dotstr",
        const char *s = (const char*)IP;           // get string pointer
        *fout << s;  IP += STRLEN(s)),              // send to output console
    CODE("branch" , IP = MEM(IP)),           // unconditional branch
    CODE("0branch", IP = POP() ? IP + sizeof(IU) : MEM(IP)), // conditional branch
    CODE("donext",
         if ((rs[-1] -= 1) >= 0) IP = MEM(IP);     // rs[-1]-=1 saved 200ms/1M cycles
         else { IP += sizeof(IU); rs.pop(); }),
    CODE("does",                                   // CREATE...DOES... meta-program
         IU *ip = (IU*)PFA(WP);
         while (*ip != DOES) ip++;                 // find DOES
         while (*ip) add_iu(*ip);),                // copy&paste code
    CODE(">r",   rs.push(POP())),
    CODE("r>",   PUSH(rs.pop())),
    CODE("r@",   PUSH(rs[-1])),
    /// @}
    /// @defgroup Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",  PUSH(top)),
    CODE("drop", top = ss.pop()),
    CODE("over", PUSH(ss[-1])),
    CODE("swap", DU n = ss.pop(); PUSH(n)),
    CODE("rot",  DU n = ss.pop(); DU m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("pick", int i = INT(top); top = ss[-i]),
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
    CODE("*",    top *= ss.pop()),
    CODE("-",    top =  ss.pop() - top),
    CODE("/",    top =  ss.pop() / top),
    CODE("mod",  top =  ss.pop() % top),
    CODE("*/",   top = ss.pop() * ss.pop() / top),
    CODE("*/mod",
        int n = INT(ss.pop() * ss.pop());
        int t = INT(top);
        ss.push(n % t); top = (n / t)),
    CODE("and",  top = ss.pop() & top),
    CODE("or",   top = ss.pop() | top),
    CODE("xor",  top = ss.pop() ^ top),
    CODE("abs",  top = abs(top)),
    CODE("negate", top = -top),
    CODE("max",  DU n = ss.pop(); top = (top>n)?top:n),
    CODE("min",  DU n = ss.pop(); top = (top<n)?top:n),
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
    CODE("base!",   *fout << setbase(base = POP())),
    CODE("hex",     *fout << setbase(base = 16)),
    CODE("decimal", *fout << setbase(base = 10)),
    CODE("cr",      *fout << ENDL),
    CODE(".",       *fout << POP() << " "),
    CODE(".r",      int n = INT(POP()); dot_r(n, POP())),
    CODE("u.r",     int n = INT(POP()); dot_r(n, abs(POP()))),
    CODE(".f",      int n = INT(POP()); *fout << setprecision(n) << POP()),
    CODE("key",     string s = next_idiom(); PUSH(s.c_str()[0])),
    CODE("emit",    char b = (char)POP(); *fout << b),
    CODE("space",   *fout << " "),
    CODE("spaces",  for (int n = INT(POP()), i = 0; i < n; i++) *fout << " "),
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("[",       compile = false),
    CODE("]",       compile = true),
    IMMD("(",       next_idiom(')')),
    IMMD(".(",      *fout << next_idiom(')')),
    CODE("\\",      next_idiom('\n')),
    CODE("$\"",
        string s = next_idiom('"').substr(1);
        add_w(DOSTR);
        add_str(s.c_str())),
    IMMD(".\"",
        string s = next_idiom('"').substr(1);
        add_w(DOTSTR);
        add_str(s.c_str())),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    IMMD("if",      add_w(ZBRAN); PUSH(HERE); add_iu(0)),       // if    ( -- here )
    IMMD("else",                                                // else ( here -- there )
        add_w(BRAN);
        IU h=HERE; add_iu(0); SETJMP(POP()) = HERE; PUSH(h)),
    IMMD("then",    SETJMP(POP()) = HERE),                      // backfill jump address
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",   PUSH(HERE)),
    IMMD("again",   add_w(BRAN);  add_iu(POP())),               // again    ( there -- )
    IMMD("until",   add_w(ZBRAN); add_iu(POP())),               // until    ( there -- )
    IMMD("while",   add_w(ZBRAN); PUSH(HERE); add_iu(0)),       // while    ( there -- there here )
    IMMD("repeat",  add_w(BRAN);                                // repeat    ( there1 there2 -- )
        IU t=POP(); add_iu(POP()); SETJMP(t) = HERE),           // set forward and loop back address
    /// @}
    /// @defgrouop For loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for" ,    add_w(TOR); PUSH(HERE)),                    // for ( -- here )
    IMMD("next",    add_w(DONEXT); add_iu(POP())),              // next ( here -- )
    IMMD("aft",                                                 // aft ( here -- here there )
        POP(); add_w(BRAN);
        IU h=HERE; add_iu(0); PUSH(HERE); PUSH(h)),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE(":", colon(next_idiom()); compile=true),
    IMMD(";", add_w(EXIT); compile = false),
    CODE("variable",                                             // create a variable
        colon(next_idiom());                                     // create a new word on dictionary
        add_w(DOVAR);                                            // dovar (+parameter field)
        DU n = 0; add_du(n)),                                    // data storage (32-bit integer now)
    CODE("constant",                                             // create a constant
        colon(next_idiom());                                     // create a new word on dictionary
        add_w(DOLIT);                                            // dovar (+parameter field)
        add_du(POP())),                                          // data storage (32-bit integer now)
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",  call(POP())),                                  // execute word
    CODE("create",
        colon(next_idiom());                                     // create a new word on dictionary
        add_w(DOVAR)),                                           // dovar (+ parameter field)
    CODE("to",              // 3 to x                            // alter the value of a constant
        IU w = find(next_idiom());                               // to save the extra @ of a variable
        *(DU*)(PFA(w) + sizeof(IU)) = POP()),
    CODE("is",              // ' y is x                          // alias a word
        IU w = find(next_idiom());                               // can serve as a function pointer
        dict[POP()].pfa = dict[w].pfa),                          // but might leave a dangled block
    CODE("[to]",            // : xx 3 [to] y ;                   // alter constant in compile mode
        IU w = *(IU*)IP; IP += sizeof(IU);                       // fetch constant pfa from 'here'
        *(DU*)(PFA(w) + sizeof(IU)) = POP()),

    ///
    /// be careful with memory access, especially BYTE because
    /// it could make access misaligned which slows the access speed by 2x
    ///
    CODE("@",     IU w = POP(); PUSH(CELL(w))),                  // w -- n
    CODE("!",     IU w = POP(); CELL(w) = POP();),               // n w --
    CODE(",",     DU n = POP(); add_du(n)),
    CODE("allot", DU v = 0; for (IU n = POP(), i = 0; i < n; i++) add_du(v)), // n --
    CODE("+!",    IU w = POP(); CELL(w) += POP()),               // n w --
    CODE("?",     IU w = POP(); *fout << CELL(w) << " "),         // w --
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("here",  PUSH(HERE)),
    CODE("ucase", ucase = POP()),
    CODE("words", words()),
    CODE("'",     IU w = find(next_idiom()); PUSH(w)),
    CODE(".s",    ss_dump()),
    CODE("see",
        IU w = find(next_idiom());
        *fout << "[ "; to_s(w); see(PFA(w)); *fout << "]" << ENDL),
    CODE("dump",  DU n = POP(); IU a = POP(); mem_dump(a, n)),
    CODE("peek",  int a = INT(POP()); PUSH(PEEK(a))),
    CODE("poke",  int a = INT(POP()); POKE(a, POP())),
    CODE("forget",
        IU w = find(next_idiom());
        if (w<0) return;
        IU b = find("boot")+1;
        dict.clear(w > b ? w : b)),
    CODE("clock", PUSH(millis())),
    CODE("delay", delay(POP())),
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
    CODE("bye",  exit(0)),
    CODE("boot", dict.clear(find("boot") + 1); pmem.clear())
    };
    const int PSZ = sizeof(prim)/sizeof(Code);

    for (int i=0; i<PSZ; i++) {              /// copy prim(ROM) into fast RAM dictionary,
        dict.push(prim[i]);                  /// find() can be modified to support
        printf("%3d> xt=%4x:%p name=%4x:%p %s\n", i,
            (U16)((UFP)dict[i].xt - (UFP)dict[EXIT].xt), dict[i].xt,
            (U16)(dict[i].name - dict[EXIT].name),
            dict[i].name, dict[i].name);
    }                                        /// searching both spaces
    IP    = &pmem[0];
    DICT0 = (UFP)dict[EXIT].xt;
}
ForthVM::ForthVM(istream &in, ostream &out) {
    fin  = &in;
    fout = &out;
}
///
/// ForthVM dictionary setup
///
void ForthVM::init() { _init(); }
///
/// ForthVM Outer interpreter
///
void ForthVM::outer() {
    while (*fin >> idiom) {
        //printf("%s=>", idiom.c_str());
        int w = find(idiom);                        /// * search through dictionary
        if (w >= 0) {                               /// * word found?
            //printf("%s(%ld)\n", w->to_s().c_str(), w.use_count());
            if (compile && !dict[w].immd)           /// * in compile mode?
                add_w(w);                           /// * add to colon word
            else call(w);                           /// * execute forth word
            continue;
        }
        // try as a number
        char *p;
#if DU==float
        DU n = (base==10)
            ? static_cast<DU>(strtof(idiom.c_str(), &p))
            : static_cast<DU>(strtol(idiom.c_str(), &p, base));
#else
        DU n = static_cast<DU>(strtol(idiom.c_str(), &p, base));
#endif
        //printf("%d\n", n);
        if (*p != '\0') {                           /// * not number
            *fout << idiom << "? " << ENDL;          ///> display error prompt
            compile = false;                        ///> reset to interpreter mode
            break;                                  ///> skip the entire input buffer
        }
        // is a number
        if (compile) {                              /// * a number in compile mode?
            add_w(DOLIT);                           ///> add to current word
            add_du(n);
        }
        else PUSH(n);                               ///> or, add value onto data stack
    }
    if (!compile) ss_dump();   /// * dump stack and display ok prompt
}

