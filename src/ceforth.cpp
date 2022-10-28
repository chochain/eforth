///
/// @file
/// @brief eForth implemented in C/C++ for portability
///
/// CC 20220512:
///       Though the goal of eForth is to show how a Forth can be
///       easily understood and cleanly constructed.
///       However, the threading method used is very inefficient (slow)
///       because each call needs 2 indirect lookups (token->dict, dict->xt)
///       and a callframe needs to be setup/teardown. Plus, every call
///       will miss the branch prediction stalling CPU pipeline. Bad stuffs!
/// CC 20220514:
///       Refactor to subroutine indirect threading.
///       Using 16-bit offsets for pointer arithmatics in order to speed up
///       while maintaining space consumption
///
/// @benchmark: 10K*10K cycles on desktop (3.2GHz AMD)
/// + 1200ms - orig/esp32forth8_1, token call threading
/// +  981ms - call threading, inline list methods
///
/// @benchmark: 1K*1K cycles on NodeMCU ESP32S
/// + 1440ms - Dr. Ting's orig/esp32forth_82
/// + 1045ms - orig/esp32forth8_1, token call threading
/// +  839ms - call threading, inline list methods
///
#include <string.h>         // strcmp, strcasecmp
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
/// Note:
///   1.By separating pmem from dictionary,
///   * it makes dictionary uniform size which eliminates the need for link field
///   * however, it requires array size tuning manually
///   2.For ease of byte counting, we use U8 for pmem instead of U16.
///   * this makes IP increment by 2 instead of word size. If needed, it can be
///   * readjusted.
///
List<DU,   E4_RS_SZ>   rs;         ///< return stack
List<DU,   E4_SS_SZ>   ss;         ///< parameter stack
List<Code, E4_DICT_SZ> dict;       ///< dictionary
List<U8,   E4_PMEM_SZ> pmem;       ///< parameter memory (for colon definitions)
U8  *MEM0 = &pmem[0];              ///< base of parameter memory block
UFP XT0   = ~0;                    ///< base of function pointers
UFP NM0   = ~0;                    ///< base of name string
///
/// system variables
///
bool compile = false;              ///< compiler flag
bool ucase   = true;               ///< case sensitivity control
DU   top     = -1;                 ///< top of stack (cached)
DU   base    = 10;                 ///< numeric radix
IU   WP      = 0;                  ///< current word pointer (used by DOES only)
IU   IP      = 0;                  ///< current instruction pointer and cached base pointer
///
/// macros to abstract dict and pmem physical implementation
/// Note:
///   so we can change pmem implementation anytime without affecting opcodes defined below
///
///@name Dictionary access macros
///@{
#define BOOL(f)   ((f)?-1:0)                /**< Forth boolean representation            */
#define PFA(w)    (dict[w].pfa)             /**< parameter field pointer of a word       */
#define HERE      (pmem.idx)                /**< current parameter memory index          */
#define MEM(ip)   (MEM0 + (IU)(ip))         /**< pointer to IP address fetched from pmem */
#define XTOFF(xp) ((IU)((UFP)(xp) - XT0))   /**< XT offset (index) in code space         */
#define XT(xt)    (XT0 + ((UFP)(xt) & ~0x3))/**< convert XT offset to function pointer   */
#define CELL(a)   (*(DU*)&pmem[a])          /**< fetch a cell from parameter memory      */
#define SETJMP(a) (*(IU*)&pmem[a] = HERE)   /**< address offset for branching opcodes    */
///@}
typedef enum {
    EXIT = 0, DONEXT, DOVAR, DOLIT, DOSTR, DOTSTR, BRAN, ZBRAN, DOES, TOR
} forth_opcode;

///==============================================================================
///
/// dictionary search functions - can be adapted for ROM+RAM
///
int pfa2word(IU ix) {
    IU   def = ix & 1;
    IU   pfa = ix & ~0x1;             ///> TODO: handle colon immediate words when > 64K
    UFP  xt  = XT(ix);                ///> can xt be immediate? i.e. ix & ~0x3
    for (int i = dict.idx - 1; i >= 0; --i) {
        if (def) {
            if (dict[i].pfa == pfa) return i;      ///> compare pfa in PMEM
        }
        else if ((UFP)dict[i].xt == xt) return i;  ///> compare xt (no immediate?)
    }
    return -1;
}
int streq(const char *s1, const char *s2) {
    return ucase ? strcasecmp(s1, s2)==0 : strcmp(s1, s2)==0;
}
#if CC_DEBUG
int find(const char *s) {
#if    (ARDUINO || ESP32)
    LOGF("find("); LOG(s); LOGF(") => ");
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (streq(s, dict[i].name)) {
            LOG(dict[i].name); LOG(" "); LOG(i); LOGF("\n");
            return i;
        }
    }
#else   // !(ARDUINO || ESP32)
    printf("find(%s) => ", s);
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (streq(s, dict[i].name)) {
            printf("%s %d\n", dict[i].name, i);
            return i;
        }
    }
#endif  // (ARDUINO || ESP32)
    return -1;
}
#else // !CC_DEBUG
int find(const char *s) {
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (streq(s, dict[i].name)) return i;
    }
    return -1;
}
#endif // CC_DEBUG    
int find(string &s) { return find(s.c_str()); }
///==============================================================================
///
/// colon word compiler
/// Note:
///   * we separate dict and pmem space to make word uniform in size
///   * if they are combined then can behaves similar to classic Forth
///   * with an addition link field added.
///
void  colon(const char *name) {
    char *nfa = (char*)&pmem[HERE];         ///> current pmem pointer
    int sz = STRLEN(name);                  ///> string length, aligned
    pmem.push((U8*)name,  sz);              ///> setup raw name field
#if    LAMBDA_OK
    Code c(nfa, [](){});                    ///> create a new word on dictionary
#else  // !LAMBDA_OK
    Code c(nfa, NULL);
#endif // LAMBDA_OK
    c.def = 1;                              ///> specify a colon word
    c.pfa = HERE;                           ///> capture code field index
    dict.push(c);                           ///> deep copy Code struct into dictionary
}
void colon(string &s) { colon(s.c_str()); }
void add_iu(IU i)     { pmem.push((U8*)&i, sizeof(IU)); }                   /**< add an instruction into pmem */
void add_du(DU v)     { pmem.push((U8*)&v, sizeof(DU)); }                   /**< add a cell into pmem         */
void add_str(const char *s) { int sz = STRLEN(s); pmem.push((U8*)s,  sz); } /**< add a string to pmem         */
void add_w(IU w) {                                                          /**< add a word index into pmem   */
    Code &c = dict[w];
    IU   ip = c.def ? (c.pfa | 1) : (w==EXIT ? 0 : XTOFF(c.xt));
    add_iu(ip);
    // printf("add_w(%d) => %4x:%p %s\n", w, ip, c.xt, c.name);
}
///============================================================================
///
/// Forth inner interpreter (handles a colon word)
/// Note: call threading is slower with call/return
///
/// interative version (8% faster than recursive version)
/// TODO: performance tuning
///   1. Just-in-time code(ip, dp) for inner loop
///      * use local stack, 840ms => 784ms, but allot 4*64 bytes extra
///   2. computed label
///   3. co-routine
///
void nest() {
    static IU _NXT = XTOFF(dict[find("donext")].xt); ///> cache offset to subroutine address
    int dp = 0;                                      ///> iterator depth control
    while (dp >= 0) {
        IU ix = *(IU*)MEM(IP);                       ///> fetch opcode
        while (ix) {                                 ///> fetch till EXIT
            IP += sizeof(IU);                        /// * advance inst. ptr
            if (ix & 1) {                            ///> is it a colon word?
                rs.push(WP);                         ///> * setup callframe (ENTER)
                rs.push(IP);
                IP = ix & ~0x1;                      ///> word pfa (def masked)
                dp++;                                ///> go one level deeper
            }
#if  !(ARDUINO || ESP32)
            else if (ix == _NXT) {                          ///> cached DONEXT handler (save 600ms / 100M cycles on AMD)
                if ((rs[-1] -= 1) >= 0) IP = *(IU*)MEM(IP); ///> but on ESP32, it slows down 100ms / 1M cycles
                else { IP += sizeof(IU); rs.pop(); }        ///> most likely due to its shallow pipeline
            }
#endif // !(ARDUINO || ESP32)
            else (*(FPTR)XT(ix))();                  ///> execute primitive word
            ix = *(IU*)MEM(IP);                      ///> fetch next opcode
        }
        if (dp-- > 0) {                              ///> pop off a level
            IP = rs.pop();                           ///> * restore call frame (EXIT)
            WP = rs.pop();
        }
        yield();                                     ///> give other tasks some time
    }
}
///
/// CALL macro (inline to speed-up)
///
#define CALL(w)                                       \
    if (dict[w].def) { WP = w; IP = PFA(w); nest(); } \
    else (*(FPTR)((UFP)dict[w].xt & ~0x3))()

///==============================================================================
///
/// utilize C++ standard template libraries for core IO functions only
/// Note:
///   * we use STL for its convinence, but
///   * if it takes too much memory for target MCU,
///   * these functions can be replaced with our own implementation
///
#include <iomanip>                  /// setbase, setw, setfill
#include <sstream>                  /// iostream, stringstream
using namespace std;                /// default to C++ standard template library
istringstream   fin;                ///< forth_in
ostringstream   fout;               ///< forth_out
string          strbuf;             ///< input string buffer
void (*fout_cb)(int, const char*);  ///< forth output callback function (see ENDL macro)
///================================================================================
///
/// IO & debug functions
///
inline void dot_r(int n, int v) { fout << setw(n) << setfill(' ') << v; }
inline void to_s(IU w) {
    fout << dict[w].name << " " << w << (dict[w].immd ? "* " : " ");
}
///
/// recursively disassemble colon word
///
void see(IU pfa, int dp=1) {
    U8 *ip = MEM(pfa);
    while (*(IU*)ip) {
        fout << ENDL; for (int i=dp; i>0; i--) fout << "  ";        ///> indentation
        fout << setw(4) << (ip - MEM0) << "[ " << setw(-1);         ///> display word offset
        IU c = pfa2word(*(IU*)ip);                                  ///> fetch word index by pfa
        to_s(c);                                                    ///> display name
        if (dict[c].def && dp < 2) {                                ///> is a colon word
            see(PFA(c), dp+1);                                      ///> recursive into child PFA
        }
        ip += sizeof(IU);
        switch (c) {
        case DOVAR: case DOLIT:
            fout << "= " << *(DU*)ip; ip += sizeof(DU); break;
        case DOSTR: case DOTSTR:
            fout << "= \"" << (char*)ip << '"';
            ip += STRLEN((char*)ip); break;
        case BRAN: case ZBRAN: case DONEXT:
            fout << "j" << *(IU*)ip; ip += sizeof(IU); break;
        }
        fout << "] ";
    }
}
void words() {
    fout << setbase(10);
    for (int i=0; i<dict.idx; i++) {
        if ((i%10)==0) { fout << ENDL; yield(); }
        to_s(i);
    }
    fout << setbase(base);
}
void ss_dump() {
    fout << " <"; for (int i=0; i<ss.idx; i++) { fout << ss[i] << " "; }
    fout << top << "> ok" << ENDL;
}
void mem_dump(IU p0, DU sz) {
    fout << setbase(16) << setfill('0') << ENDL;
    for (IU i=ALIGN16(p0); i<=ALIGN16(p0+sz); i+=16) {
        fout << setw(4) << i << ": ";
        for (int j=0; j<16; j++) {
            U8 c = pmem[i+j];
            fout << setw(2) << (int)c << (j%4==3 ? "  " : " ");
        }
        for (int j=0; j<16; j++) {   // print and advance to next byte
            U8 c = pmem[i+j] & 0x7f;
            fout << (char)((c==0x7f||c<0x20) ? '_' : c);
        }
        fout << ENDL;
        yield();
    }
    fout << setbase(base);
}
///================================================================================
///
/// macros to reduce verbosity
///
inline char *next_idiom() { fin >> strbuf; return (char*)strbuf.c_str(); } ///< get next idiom
inline char *scan(char c) { getline(fin, strbuf, c); return (char*)strbuf.c_str(); }
inline void PUSH(DU v)    { ss.push(top); top = v; }
inline DU   POP()         { DU n=top; top=ss.pop(); return n; }
///
//#define     PUSH(v)       { ss.push(top); top = (v); }
///
/// global memory access macros
///
/// macros for ESP memory space access (be very careful of these)
/// note: 4000_0000 is instruction bus, access needs to be 32-bit aligned
///       3fff_ffff and below is data bus, no alignment requirement
///
#define     PEEK(a)    (DU)(*(DU*)((UFP)(a)))
#define     POKE(a, c) (*(DU*)((UFP)(a))=(DU)(c))
///==========================================================================
///
/// eForth core implementation
///
void forth_outer(const char *cmd, void(*callback)(int, const char*)); // forward declaration

#if  (ARDUINO || ESP32)
#include <SPIFFS.h>
///
/// eForth bootstrap loader (from Flash memory)
///
int forth_load(const char *fname) {
    auto dummy = [](int, const char *) { /* do nothing */ };
    if (!SPIFFS.begin()) {
        LOGF("Error mounting SPIFFS"); return 1; }
    File file = SPIFFS.open(fname, "r");
    if (!file) {
        LOGF("Error opening file:"); LOG(fname); return 1; }
    LOGF("Loading file: "); LOG(fname); LOGF("...");
    while (file.available()) {
        char cmd[256], *p = cmd, c;
        while ((c = file.read())!='\n') *p++ = c;   // one line a time
        *p = '\0';
        LOGF("\n<< "); LOG(cmd);                    // show bootstrap command
        forth_outer(cmd, dummy);
    }
    LOGF("Done loading.\n");
    file.close();
    SPIFFS.end();
    return 0;
}
#else  // !(ARDUINO || ESP32)
int forth_load(const char *fname) {
    printf("TODO: load resident applications from %s...\n", fname);
    return 0;
}
#endif // (ARDUINO || ESP32)
///
/// eForth - dictionary initializer
///
/// Note: sequenced by enum forth_opcode as following
static Code prim[] = {
    ///
    /// @defgroup Execution flow ops
    /// @brief - DO NOT change the sequence here (see forth_opcode enum)
    /// @{
    CODE("exit",    IP = rs.pop(); WP = rs.pop()),      // handled in nest()
    CODE("donext",                                      // handled in nest()
         if ((rs[-1] -= 1) >= 0) IP = *(IU*)MEM(IP);    // rs[-1]-=1 saved 200ms/1M cycles
         else { IP += sizeof(IU); rs.pop(); }),
    CODE("dovar",   PUSH(IP);            IP += sizeof(DU)),
    CODE("dolit",   PUSH(*(DU*)MEM(IP)); IP += sizeof(DU)),
    CODE("dostr",
        const char *s = (const char*)MEM(IP);      // get string pointer
        PUSH(IP); IP += STRLEN(s)),
    CODE("dotstr",
        const char *s = (const char*)MEM(IP);      // get string pointer
        fout << s;  IP += STRLEN(s)),              // send to output console
    CODE("branch" , IP = *(IU*)MEM(IP)),           // unconditional branch
    CODE("0branch", IP = POP() ? IP + sizeof(IU) : *(IU*)MEM(IP)), // conditional branch
    CODE("does",                                   // CREATE...DOES... meta-program
         IU *ip = (IU*)MEM(PFA(WP));
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
    CODE("*/",   top =  (DU2)ss.pop() * ss.pop() / top),
    CODE("/mod",
        DU n = ss.pop(); DU t = top;
        ss.push(n % t); top = (n / t)),
    CODE("*/mod",
        DU2 n = (DU2)ss.pop() * ss.pop();
        DU2 t = top;
        ss.push((DU)(n % t)); top = (DU)(n / t)),
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
    CODE("base!",   fout << setbase(base = POP())),
    CODE("hex",     fout << setbase(base = 16)),
    CODE("decimal", fout << setbase(base = 10)),
    CODE("cr",      fout << ENDL),
    CODE(".",       fout << POP() << " "),
    CODE(".r",      DU n = POP(); dot_r(n, POP())),
    CODE("u.r",     DU n = POP(); dot_r(n, abs(POP()))),
    CODE("key",     PUSH(next_idiom()[0])),
    CODE("emit",    char b = (char)POP(); fout << b),
    CODE("space",   fout << " "),
    CODE("spaces",  for (int n = POP(), i = 0; i < n; i++) fout << " "),
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("[",       compile = false),
    CODE("]",       compile = true),
    IMMD("(",       scan(')')),
    IMMD(".(",      fout << scan(')')),
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
        IU h=HERE; add_iu(0); SETJMP(POP()); PUSH(h)),
    IMMD("then",    SETJMP(POP())),                             // backfill jump address
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",   PUSH(HERE)),
    IMMD("again",   add_w(BRAN);  add_iu(POP())),               // again    ( there -- )
    IMMD("until",   add_w(ZBRAN); add_iu(POP())),               // until    ( there -- )
    IMMD("while",   add_w(ZBRAN); PUSH(HERE); add_iu(0)),       // while    ( there -- there here )
    IMMD("repeat",  add_w(BRAN);                                // repeat    ( there1 there2 -- )
        IU t=POP(); add_iu(POP()); SETJMP(t)),                  // set forward and loop back address
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
        int n = 0; add_du(n);                                    // data storage (32-bit integer now)
        add_w(EXIT)),
    CODE("constant",                                             // create a constant
        colon(next_idiom());                                     // create a new word on dictionary
        add_w(DOLIT);                                            // dovar (+parameter field)
        add_du(POP());                                           // data storage (32-bit integer now)
        add_w(EXIT)),
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
        IU w = *(IU*)MEM(IP); IP += sizeof(IU);                  // fetch constant pfa from 'here'
        *(DU*)MEM(PFA(w) + sizeof(IU)) = POP()),
    ///
    /// be careful with memory access, especially BYTE because
    /// it could make access misaligned which slows the access speed by 2x
    ///
    CODE("@",     IU w = POP(); PUSH(CELL(w))),                  // w -- n
    CODE("!",     IU w = POP(); CELL(w) = POP();),               // n w --
    CODE(",",     DU n = POP(); add_du(n)),
    CODE("allot", DU v = 0; for (IU n = POP(), i = 0; i < n; i++) add_du(v)), // n --
    CODE("+!",    IU w = POP(); CELL(w) += POP()),               // n w --
    CODE("?",     IU w = POP(); fout << CELL(w) << " "),         // w --
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("here",  PUSH(HERE)),
    CODE("ucase", ucase = POP()),
    CODE("'",     IU w = find(next_idiom()); PUSH(w)),
    CODE(".s",    ss_dump()),
    CODE("words", words()),
    CODE("see",
        IU w = find(next_idiom());
        fout << "[ "; to_s(w); if (dict[w].def) see(PFA(w)); fout << "]" << ENDL),
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
///
/// eForth dictionary initializer
///
void forth_init() {
    for (int i=0; i<PSZ; i++) {              ///> copy prim(ROM) into fast RAM dictionary,
        dict.push(prim[i]);                  ///> find() can be modified to support
        if (((UFP)prim[i].xt - 4) < XT0) XT0 = ((UFP)prim[i].xt - 4);  ///> collect xt base
        if ((UFP)prim[i].name < NM0) NM0 = (UFP)prim[i].name;
    }
    forth_load("/load.txt");                 ///> compile /data/load.txt
}
///==========================================================================
///
/// ForthVM Outer interpreter
///
DU parse_number(const char *idiom, int *err) {
    char *p;
    *err = errno = 0;
#if DU==float
    DU n = (base==10)
        ? static_cast<DU>(strtof(idiom, &p))
        : static_cast<DU>(strtol(idiom, &p, base));
#else
    DU n = static_cast<DU>(strtol(idiom, &p, base));
#endif
    if (errno || *p != '\0') *err = 1;
#if CC_DEBUG
#if     (ARDUINO || ESP32)
    LOG(n); LOGF("\n");
#else  // !(ARDUINO || ESP32)
    printf("%d\n", n);
#endif // (ARDUINO || ESP32)
#endif // CC_DEBUG
    return n;
}

void forth_outer(const char *cmd, void(*callback)(int, const char*)) {
    fin.clear();                             ///> clear input stream error bit if any
    fin.str(cmd);                            ///> feed user command into input stream
    fout_cb = callback;                      ///> setup callback function
    fout.str("");                            ///> clean output buffer, ready for next run
    while (fin >> strbuf) {
        const char *idiom = strbuf.c_str();
        int w = find(idiom);                 ///> * search through dictionary
        if (w >= 0) {                        ///> * word found?
            if (compile && !dict[w].immd)    /// * in compile mode?
                add_w(w);                    /// * add to colon word
            else CALL(w);                    /// * execute forth word
            continue;
        }
        // try as a number
        int err = 0;
        DU  n   = parse_number(idiom, &err);
        if (err) {                           /// * not number
            fout << idiom << "? " << ENDL;   ///> display error prompt
            compile = false;                 ///> reset to interpreter mode
            break;                           ///> skip the entire input buffer
        }
        // is a number
        if (compile) {                       /// * a number in compile mode?
            add_w(DOLIT);                    ///> add to current word
            add_du(n);
        }
        else PUSH(n);                        ///> or, add value onto data stack
    }
    if (!compile) ss_dump();   /// * dump stack and display ok prompt
}
///===================================================================================================
///
/// ForthVM - front-end proxy class
///
void ForthVM::init()      { forth_init(); }
void ForthVM::outer(const char *cmd, void(*callback)(int, const char*)) {
    forth_outer(cmd, callback);
}
const char *ForthVM::version(){
    static string ver = string(APP_NAME) + " " + MAJOR_VERSION + "." + MINOR_VERSION;
    return ver.c_str();
}
///
/// memory statistics - for heap and stack debugging
///
#if CC_DEBUG
#if    (ARDUINO || ESP32)
void ForthVM::mem_stat()  {
    LOGF("Core:");           LOG(xPortGetCoreID());
    LOGF(" heap[maxblk=");   LOG(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    LOGF(", avail=");        LOG(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    LOGF(", ss_max=");       LOG(ss.max);
    LOGF(", rs_max=");       LOG(rs.max);
    LOGF(", pmem=");         LOG(HERE);
    LOGF("], lowest[heap="); LOG(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    LOGF(", stack=");        LOG(uxTaskGetStackHighWaterMark(NULL));
    LOGF("]\n");
    if (!heap_caps_check_integrity_all(true)) {
//        heap_trace_dump();     // dump memory, if we have to
        abort();                 // bail, on any memory error
    }
}
void ForthVM::dict_dump() {
    LOGF("XT0=");        LOGX(XT0);
    LOGF("NM0=");        LOGX(NM0);
    LOGF(", sizeof(Code)="); LOG(sizeof(Code));
    LOGF("\n");
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        LOG(i);
        LOGF("> xt=");   LOGX((UFP)c.xt - XT0);
        LOGF(":");       LOGX((UFP)c.xt);
        LOGF(", name="); LOGX((UFP)c.name - NM0);
        LOGF(":");       LOGX((UFP)c.name);
        LOG(" ");        LOG(c.name);
        LOGF("\n");
    }
}
#else  // !(ARDUINO || ESP32)
void ForthVM::mem_stat()  {
    printf("Core: %x", xPortGetCoreID());
    printf(" heap[maxblk=%x", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf(", avail=%x", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    printf(", ss_max=%x", ss.max);
    printf(", rs_max=%x", rs.max);
    printf(", pmem=%x", HERE);
    printf("], lowest[heap=%x", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    printf(", stack=%x", uxTaskGetStackHighWaterMark(NULL));
    printf("]\n");
}
void ForthVM::dict_dump() {
    printf("XT0=%lx, NM0=%lx, sizeof(Code)=%ld byes\n", XT0, NM0, sizeof(Code));
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        printf("%3d> xt=%4x:%p name=%4x:%p %s\n",
            i, XTOFF(c.xt), c.xt,
            (U16)((UFP)c.name - NM0),
            c.name, c.name);
    }
}
#endif // (ARDUINO || ESP32)
#else  // !CC_DEBUG
void ForthVM::mem_stat()  {}
void ForthVM::dict_dump() {
    printf("XT0=%lx, NM0=%lx, sizeof(Code)=%ld byes\n", XT0, NM0, sizeof(Code));
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        printf("%3d> xt=%4x:%p name=%4x:%p %s\n",
            i, XTOFF(c.xt), c.xt,
            (U16)((UFP)c.name - NM0),
            c.name, c.name);
    }
}
#endif // CC_DEBUG

#if !(ARDUINO || ESP32)
///
/// main program for testing on PC
/// Arduino and ESP32 have their own main
///
#include <iostream>               // cin, cout
int main(int ac, char* av[]) {
    static auto send_to_con = [](int len, const char *rst) { cout << rst; };
    string cmd;
    ForthVM vm;                                         // create FVM instance
    vm.init();                                          // initialize dictionary
    vm.dict_dump();

    cout << vm.version() << endl;
    while (getline(cin, cmd)) {                         // fetch user input
        // printf("cmd=<%s>\n", cmd.c_str());
        vm.outer(cmd.c_str(), send_to_con);             // execute outer interpreter
    }
    cout << "Done!" << endl;
    return 0;
}
#endif // !(ARDUINO || ESP32)
