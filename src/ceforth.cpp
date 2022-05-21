#include <iomanip>          // setbase, setw, setfill
#include "ceforth.h"
///
/// version info
///
#define APP_NAME         "eForth"
#define MAJOR_VERSION    "8"
#define MINOR_VERSION    "1"
///==============================================================================
///
/// global memory blocks
///
List<DU,   E4_SS_SZ>   rs;         /// return stack
List<DU,   E4_RS_SZ>   ss;         /// parameter stack
List<Code, E4_DICT_SZ> dict;       /// dictionary
List<U8,   E4_PMEM_SZ> pmem;       /// parameter memory (for colon definitions)
U8  *MEM0 = &pmem[0];              /// base of parameter memory block
UFP XT0   = ~0;                    /// base of function pointers
IU  NXT   = 0;                     /// cached DONEXT xt address
///
/// system variables
///
bool compile = false;             /// compiler flag
bool ucase   = true;              /// case sensitivity control
DU   top = -1, base = 10;
IU   WP  = 0;                     /// current word pointer
U8   *IP = MEM0;                  /// current instruction pointer and cached base pointer
///
/// macros to abstract dict and pmem physical implementation
/// Note:
///   so we can change pmem implementation anytime without affecting opcodes defined below
///
#define BOOL(f)   ((f)?-1:0)
#define PFA(w)    ((U8*)&pmem[dict[w].pfa]) /** parameter field pointer of a word        */
#define HERE      (pmem.idx)                /** current parameter memory index           */
#define OFF(ip)   ((IU)((U8*)(ip) - MEM0))  /** IP offset (index) in parameter memory    */
#define MEM(ipx)  (MEM0 + (ipx))            /** pointer to IP address fetched from pmem  */
#define XTOFF(xt) ((IU)((UFP)(xt) - XT0))   /** XT offset (index) in code space          */
#define XT(xtx)   (XT0 + (xtx))             /** convert XT offset to function pointer    */
#define CELL(a)   (*(DU*)&pmem[a])          /** fetch a cell from parameter memory       */
#define SETJMP(a) (*(IU*)&pmem[a])          /** address offset for branching opcodes     */

typedef enum {
    EXIT = 0, DOVAR, DOLIT, DOSTR, DOTSTR, BRAN, ZBRAN, DONEXT, DOES, TOR
} forth_opcode;

///==============================================================================
///
/// dictionary search functions - can be adapted for ROM+RAM
///
int pfa2word(U8 *ip) {
    IU   ipx = *(IU*)ip;
    IU   def = ipx & 0x1;
    IU   pfa = ipx & ~0x1;     /// TODO: handle immediate colon word (when > 64K needed)
    UFP  xt  = XT(ipx);
    for (int i = dict.idx - 1; i >= 0; --i) {
        if (def) {
            if (dict[i].pfa == pfa) return i;      /// compare pfa in PMEM
        }
        else if ((UFP)dict[i].xt == xt) return i;  /// compare xt (no immediate?)
    }
    return -1;
}
int streq(const char *s1, const char *s2) {
    return ucase ? strcasecmp(s1, s2)==0 : strcmp(s1, s2)==0;
}
int find(const char *s) {
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (streq(s, dict[i].name)) return i;
    }
    return -1;
}
int find(string &s) { return find(s.c_str()); }
///============================================================================
///
/// Forth inner interpreter (handles a colon word)
///
#define CALL(w) \
    if (dict[w].def) { WP = w; IP = PFA(w); nest(); } \
    else (*(FPTR)((UFP)dict[w].xt & ~0x3))()
///
/// recursive version (look nicer but use system stack)
/// Note: superceded by iterator version below (~8% faster)
/*
void nest() {
    while (*(IU*)IP) {
        if (*(IU*)IP & 1) {
            /// ENTER
            rs.push(WP);                        /// * setup callframe
            IP = MEM(*(IU*)IP & ~0x1);
            nest();
            /// EXIT
            IP = MEM(rs.pop());                 /// * restore call frame
            WP =rs.pop();
        }
        else {
            UFP xt = XT(*(IU*)IP & ~0x3);       /// * function pointer
            IP += sizeof(IU);                   /// * advance to next opcode
            (*(FPTR)xt)();
        }
    }
    yield();                                ///> give other tasks some time
}
*/
///
/// interative version
///
void nest() {
    int dp = 0;                                      /// iterator depth control
    while (dp >= 0) {
        /// function core
        auto ipx = *(IU*)IP;                         /// hopefully use register than cached le
        while (ipx) {
            IP += sizeof(IU);
            if (ipx & 1) {
                rs.push(WP);                         /// * setup callframe (ENTER)
                rs.push(OFF(IP));
                IP = MEM(ipx & ~0x1);                /// word pfa (def masked)
                dp++;
            }
            else if (ipx == NXT) {                   /// check cached DONEXT (save 600ms / 100M cycles)
                if ((rs[-1] -= 1) >= 0) IP = MEM(*(IU*)IP);
                else { IP += sizeof(IU); rs.pop(); }
            }
            else (*(FPTR)XT(ipx & ~0x3))();          /// * execute primitive word
            ipx = *(IU*)IP;                          /// * fetch next opcode
        }
        if (dp-- > 0) {
            IP = MEM(rs.pop());                      /// * restore call frame (EXIT)
            WP = rs.pop();
        }
        yield();                                     ///> give other tasks some time
    }
}
///==============================================================================
///
/// colon word compiler
/// Note:
///   * we separate dict and pmem space to make word uniform in size
///   * if they are combined then can behaves similar to classic Forth
///   * with an addition link field added.
///
void  colon(const char *name) {
    char *nfa = (char*)&pmem[HERE];         // current pmem pointer
    int sz = STRLEN(name);                  // string length, aligned
    pmem.push((U8*)name,  sz);              // setup raw name field
#if LAMBDA_OK
    Code c(nfa, [](IU){});                  // create a new word on dictionary
#else  // LAMBDA_OK
    Code c(nfa, NULL);
#endif // LAMBDA_OK
    c.def = 1;                              // specify a colon word
    c.len = 0;                              // advance counter (by number of U16)
    c.pfa = HERE;                           // capture code field index
    dict.push(c);                           // deep copy Code struct into dictionary
    printf("%3d> pfa=%x, name=%4x:%p %s\n", dict.idx-1,
        dict[-1].pfa, OFF(dict[-1].name), dict[-1].name, dict[-1].name);
}
void  colon(string &s) { colon(s.c_str()); }
inline void add_iu(IU i) { pmem.push((U8*)&i, sizeof(IU));  dict[-1].len += sizeof(IU); }  /** add an instruction into pmem */
inline void add_du(DU v) { pmem.push((U8*)&v, sizeof(DU)),  dict[-1].len += sizeof(DU); }  /** add a cell into pmem         */
inline void add_str(const char *s) {                                               /** add a string to pmem         */
        int sz = STRLEN(s); pmem.push((U8*)s,  sz); dict[-1].len += sz;
}
void add_w(IU w) {
    Code &c  = dict[w];
    IU   ipx = c.def ? (c.pfa | 1) : (w==EXIT ? 0 : XTOFF(c.xt));
    add_iu(ipx);
    //printf("add_w(%d) => %4x:%p %s\n", w, ipx, c.xt, c.name);
}
///==============================================================================
///
/// debug functions
///
istream *fin;                          /// VM stream input
ostream *fout;                         /// VM stream output
string  strbuf;
///
void dot_r(int n, int v) { *fout << setw(n) << setfill(' ') << v; }
void to_s(IU c) {
    *fout << dict[c].name << " " << c << (dict[c].immd ? "* " : " ");
}
///
/// recursively disassemble colon word
///
void see(U8 *ip, int dp=1) {
    while (*(IU*)ip) {
        *fout << ENDL; for (int i=dp; i>0; i--) *fout << "  ";        // indentation
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
    *fout << setbase(10);
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
///================================================================================
///
/// macros to reduce verbosity
///
inline char *next_idiom() { *fin >> strbuf; return (char*)strbuf.c_str(); } // get next idiom
inline char *scan(char c) { getline(*fin, strbuf, c); return (char*)strbuf.c_str(); }
inline DU   POP()         { DU n=top; top=ss.pop(); return n; }
///
/// This is a killer!!! 3400ms vs 1200ms per 100M cycles, TODO: why?
/// inline PUSH(DU v)     { ss.push(top); top = v; }
///
#define     PUSH(v)       { ss.push(top); top = (v); }
///
/// global memory access macros
///
#define     PEEK(a)    (DU)(*(DU*)((UFP)(a)))
#define     POKE(a, c) (*(DU*)((UFP)(a))=(DU)(c))
///
/// dictionary initializer
///
/// Note: sequenced by enum forth_opcode as following
static Code prim[] = {
    ///
    /// @defgroup Execution flow ops
    /// @brief - DO NOT change the sequence here (see forth_opcode enum)
    /// @{
    CODE("exit",    IP = MEM(rs.pop()); WP = rs.pop()), // cached in nest()
    CODE("dovar",   PUSH(OFF(IP)); IP += sizeof(DU)),
    CODE("dolit",   PUSH(*(DU*)IP); IP += sizeof(DU)),
    CODE("dostr",
        const char *s = (const char*)IP;           // get string pointer
        PUSH(OFF(IP)); IP += STRLEN(s)),
    CODE("dotstr",
        const char *s = (const char*)IP;           // get string pointer
        fout << s;  IP += STRLEN(s)),              // send to output console
    CODE("branch" , IP = MEM(*(IU*)IP)),           // unconditional branch
    CODE("0branch", IP = POP() ? IP + sizeof(IU) : MEM(*(IU*)IP)), // conditional branch
    CODE("donext",                                 // cached in nest()
         if ((rs[-1] -= 1) >= 0) IP = MEM(*(IU*)IP); // rs[-1]-=1 saved 200ms/1M cycles
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
    CODE("pick", DU i = top; top = ss[-i]),
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
    CODE("*/",   top =  ss.pop() * ss.pop() / top),
    CODE("/mod",
        DU n = ss.pop(); DU t = top;
        ss.push(n % t); top = (n / t)),
    CODE("*/mod",
        DU n = ss.pop() * ss.pop();
        DU t = top;
        ss.push(n % t); top = (n / t)),
    CODE("and",  top = ss.pop() & top),
    CODE("or",   top = ss.pop() | top),
    CODE("xor",  top = ss.pop() ^ top),
    CODE("abs",  top = abs(top)),
    CODE("negate", top = -top),
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
    CODE("base!",   *fout << setbase(base = POP())),
    CODE("hex",     *fout << setbase(base = 16)),
    CODE("decimal", *fout << setbase(base = 10)),
    CODE("cr",      *fout << ENDL),
    CODE(".",       *fout << POP() << " "),
    CODE(".r",      DU n = POP(); dot_r(n, POP())),
    CODE("u.r",     DU n = POP(); dot_r(n, abs(POP()))),
    CODE("key",     PUSH(next_idiom()[0])),
    CODE("emit",    char b = (char)POP(); *fout << b),
    CODE("space",   *fout << " "),
    CODE("spaces",  for (int n = POP(), i = 0; i < n; i++) *fout << " "),
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("[",       compile = false),
    CODE("]",       compile = true),
    IMMD("(",       scan(')')),
    IMMD(".(",      *fout << scan(')')),
    CODE("\\",      scan('\n')),
    CODE("$\"",
        const char *s = scan('"')+1;        // string skip first blank
        add_w(DOSTR);                       // dostr, (+parameter field)
        add_str(s)),                        // byte0, byte1, byte2, ..., byteN
    IMMD(".\"",
        const char *s = scan('"')+1;        // string skip first blank
        add_w(DOTSTR);                      // dostr, (+parameter field)
        add_str(s)),                        // byte0, byte1, byte2, ..., byteN
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
        int n = 0; add_du(n)),                                   // data storage (32-bit integer now)
    CODE("constant",                                             // create a constant
        colon(next_idiom());                                     // create a new word on dictionary
        add_w(DOLIT);                                            // dovar (+parameter field)
        add_du(POP())),                                          // data storage (32-bit integer now)
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",  CALL(POP())),                                  // execute word
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
    CODE("peek",  DU a = POP(); PUSH(PEEK(a))),
    CODE("poke",  DU a = POP(); POKE(a, POP())),
    CODE("forget",
        IU w = find(next_idiom());
        if (w<0) return;
        IU b = find("boot")+1;
        dict.clear(w > b ? w : b)),
    CODE("clock", PUSH(millis())),
    CODE("delay", delay(POP())),
    /// @}
    CODE("bye",   exit(0)),
    CODE("boot",  dict.clear(find("boot") + 1); pmem.clear())
};
const int PSZ = sizeof(prim)/sizeof(Code);
///================================================================================
/// Forth Virtual Machine
///
///   dictionary initialization
///
void forth_init() {
    for (int i=0; i<PSZ; i++) {              /// copy prim(ROM) into fast RAM dictionary,
        dict.push(prim[i]);                  /// find() can be modified to support
        if ((UFP)dict[i].xt < XT0) XT0 = (UFP)dict[i].xt; /// collect xt base
    }
    NXT = XTOFF(dict[DONEXT].xt);
    printf("XT0=%lx, sizeof(Code)=%ld byes\n", XT0, sizeof(Code));
    /*
    for (int i=0; i<PSZ; i++) {
        printf("%3d> xt=%4x:%p name=%4x:%p %s\n", i,
            XTOFF(dict[i].xt), dict[i].xt,
            (U16)(dict[i].name - dict[0].name),
            dict[i].name, dict[i].name);
    }
    */
}
ForthVM::ForthVM(istream &in, ostream &out) {
    fin  = &in;
    fout = &out;
}
///
/// ForthVM dictionary setup
///
void ForthVM::init() { forth_init(); }
///
/// ForthVM Outer interpreter
///
void ForthVM::outer() {
    string idiom;
    while (*fin >> idiom) {
        //printf("%s=>", idiom.c_str());
        int w = find(idiom);                        /// * search through dictionary
        if (w >= 0) {                               /// * word found?
            //printf("%s(%ld)\n", w->to_s().c_str(), w.use_count());
            if (compile && !dict[w].immd)           /// * in compile mode?
                add_w(w);                           /// * add to colon word
            else CALL(w);                           /// * execute forth word
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
            *fout << idiom << "? " << ENDL;         ///> display error prompt
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

#if !(ARDUINO || ESP32)
#include <iostream>
/// main program
int main(int ac, char* av[]) {
    istringstream forth_in;
    ostringstream forth_out;
    string cmd;

    ForthVM *vm = new ForthVM(forth_in, forth_out);     // create FVM instance
    vm->init();                                         // initialize dictionary

    cout << APP_NAME << " " << MAJOR_VERSION << "." << MINOR_VERSION << endl;
    while (getline(cin, cmd)) {                         // fetch user input
        //printf("cmd=<%s>\n", line.c_str());
        forth_in.clear();                               // clear any input stream error bit
        forth_in.str(cmd);                              // send command to FVM
        vm->outer();                                    // execute outer interpreter
        cout << forth_out.str();                        // send VM result to output
        forth_out.str(string());                        // clear output buffer
    }
    cout << "Done!" << endl;
    return 0;
}
#endif // !(ARDUINO || ESP32)
