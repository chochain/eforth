///
/// @file
/// @brief eForth implemented in 100% C/C++ for portability and education
///
#include <string.h>      // strcmp
#include <strings.h>     // strcasecmp
#include "ceforth.h"
///
/// version info
///
#define APP_NAME         "eForth"
#define MAJOR_VERSION    "8"
#define MINOR_VERSION    "6"
///==============================================================================
///
///> Global memory blocks
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
///
///> Dictionary memory structure
///     dict[0].xt ---------> pointer to primitive word lambda[0]
///     dict[1].xt ---------> pointer to primitive word lambda[1]
///     ...
///     dict[N-1].xt -------> pointer to last primitive word lambda[N-1]
///
///> Parameter memory structure
///     dict[N].xt ----+ user defined colon word)    dict[N+1].xt------+
///                    |                                               |
///     +--MEM0        v                                               v
///     +--------------+--------+--------+-----+------+----------------+-----
///     | str nameN \0 |  parm1 |  parm2 | ... | 0000 | str nameN+1 \0 | ...
///     +--------------+--------+--------+-----+------+----------------+-----
///     ^              ^        ^        ^     ^      ^
///     | strlen+1     | 2-byte | 2-byte |     |      |
///     +--------------+--------+--------+-----+------+---- 2-byte aligned
///
///> Parameter structure - 16-bit aligned (LSB used for colon word identification)
///   * primitive word
///     16-bit xt offset with LSB set to 0
///     +--------------+-+
///     | dict.xtoff() |0|    call (XT0 + *IP)() to execute
///     +--------------+-+
///
///   * colon word (user defined)
///     16-bit pmem offset with LSB set to 1
///     +--------------+-+
///     |  dict.pfa    |1|   next IP = *(MEM0 + (*IP & ~1))
///     +--------------+-+
///
///==================================================================================
///
///> VM states
///
bool compile = false;              ///< compiler flag
bool ucase   = false;              ///< caseless string compare
DU   base    = 10;                 ///< numeric radix
DU   top     = -1;                 ///< top of stack (cached)
IU   WP      = 0;                  ///< current word pointer (used by DOES only)
IU   IP      = 0;                  ///< current instruction pointer and cached base pointer
///
///> Macros to abstract dict and pmem physical implementation
///  Note:
///    so we can change pmem implementation anytime without affecting opcodes defined below
///
///@name Dictionary access macros
///@{
#define BOOL(f)   ((f)?-1:0)               /**< Forth boolean representation            */
#define HERE      (pmem.idx)               /**< current parameter memory index          */
#define MEM(ip)   (MEM0 + (IU)(ip))        /**< pointer to IP address fetched from pmem */
#define CELL(a)   (*(DU*)&pmem[a])         /**< fetch a cell from parameter memory      */
#define SETJMP(a) (*(IU*)&pmem[a] = HERE)  /**< address offset for branching opcodes    */
///@}
///
/// enum used for built-in opcodes to simplify compiler
/// Note: make sure the sequence is in-sync with vm_init word list
///
typedef enum {
    EXIT = 0, DONEXT, DOVAR, DOLIT, DOSTR, DOTSTR, BRAN, ZBRAN, DODOES, TOR
} forth_opcode;

///==============================================================================
///
///> Dictionary search functions - can be adapted for ROM+RAM
///
int streq(const char *s1, const char *s2) {
    return ucase ? strcasecmp(s1, s2)==0 : strcmp(s1, s2)==0;
}
int find(const char *s) {
    LOG_HDR("find", s);
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (streq(s, dict[i].name)) {
            LOG_DIC(i);
            return i;
        }
    }
    LOG_NA();
    return WORD_NA;
}
///==============================================================================
///
///> Colon word compiler
///  Note:
///    * we separate dict and pmem space to make word uniform in size
///    * if they are combined then can behaves similar to classic Forth
///    * with an addition link field added.
///
void colon(const char *name) {
    char *nfa = (char*)&pmem[HERE];         ///> current pmem pointer
    int sz = STRLEN(name);                  ///> string length, aligned
    pmem.push((U8*)name,  sz);              ///> setup raw name field

    Code c(nfa, WORD_NULL, false);          ///> create a local blank word
    c.attr |= UDF_ATTR;                     ///> specify a colon (user defined) word
    c.pfa  = HERE;                          ///> capture code field index

    dict.push(c);                           ///> deep copy Code struct into dictionary
}
void add_iu(IU i)     { pmem.push((U8*)&i, sizeof(IU)); }                    ///< add an instruction into pmem
void add_du(DU v)     { pmem.push((U8*)&v, sizeof(DU)); }                    ///< add a cell into pmem
void add_str(const char *s) { int sz = STRLEN(s); pmem.push((U8*)s,  sz); }  ///< add a string to pmem
void add_w(IU w) {                                                           ///< add a word index into pmem
    IU ip = IS_UDF(w) ? (dict[w].pfa | UDF_FLAG) : (w==EXIT ? 0 : dict[w].xtoff());
    add_iu(ip);
#if CC_DEBUG > 1
    printf("add_w(%d) => %4x:%p %s\n", w, ip, dict[w].xt, dict[w].name);
#endif // CC_DEBUG > 1
}
///============================================================================
///
///> Forth inner interpreter (handles a colon word)
///  Note: call threading is slower with call/return
///
///  interactive version (8% faster than recursive version)
///  TODO: performance tuning
///    1. Just-in-time code(ip, dp) for inner loop
///       * use local stack, 840ms => 784ms, but allot 4*64 bytes extra
///    2. computed label
///    3. co-routine
///
void nest() {
    static IU _NXT = dict[find("_donext")].xtoff();  ///> cache offset to subroutine address
    int dp = 0;                                      ///> iterator depth control
    while (dp >= 0) {
        IU ix = *(IU*)MEM(IP);                       ///> fetch opcode, hopefully cached
        while (ix) {                                 ///> fetch till EXIT
            IP += sizeof(IU);                        /// * advance inst. ptr
            if (ix & UDF_FLAG) {                     ///> is it a colon word?
                rs.push(IP);
                IP = ix & ~UDF_FLAG;                 ///> word pfa (def masked)
                dp++;                                ///> go one level deeper
            }
#if !(ARDUINO || ESP32)
            else if (ix == _NXT) {                          ///> cached DONEXT handler (save 600ms / 100M cycles on AMD)
                if ((rs[-1] -= 1) >= 0) IP = *(IU*)MEM(IP); ///> but on ESP32, it slows down 100ms / 1M cycles
                else { IP += sizeof(IU); rs.pop(); }        ///> most likely due to its shallow pipeline
            }
#endif // !(ARDUINO || ESP32)
            else Code::exec(ix);                     ///> execute primitive word

            ix = *(IU*)MEM(IP);                      ///> fetch next opcode
        }
        if (dp-- > 0) IP = rs.pop();                 ///> pop off a level

        yield();                                     ///> give other tasks some time
    }
}
///
///> CALL - inner-interpreter proxy (inline macro does not run faster)
///
void CALL(IU w) {
    if (IS_UDF(w)) { WP = (w); IP = dict[w].pfa; nest(); }
    else dict[w].call();
}
///
///> Global memory access macros
///
///  macros for ESP memory space access (be very careful of these)
///  note: 4000_0000 is instruction bus, access needs to be 32-bit aligned
///        3fff_ffff and below is data bus, no alignment requirement
///
#define PEEK(a)    (DU)(*(DU*)((UFP)(a)))
#define POKE(a, c) (*(DU*)((UFP)(a))=(DU)(c))

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
/// macros to reduce verbosity
///
int def_word(const char* name) {                ///< display if redefined
    if (name[0]=='\0') { fout << " name?" << ENDL; return 0; }  /// * missing name?
    if (find(name) != WORD_NA) {                /// * word redefined?
        fout << name << " reDef? " << ENDL;
    }
    colon(name);                                /// * create a colon word
    return 1;                                   /// * created OK
}
char *next_idiom() {                            ///< get next idiom
    if (!(fin >> strbuf)) strbuf.clear();       /// * input buffer exhausted?
    return (char*)strbuf.c_str();
}
inline char *scan(char c) { getline(fin, strbuf, c); return (char*)strbuf.c_str(); }
inline void PUSH(DU v)    { ss.push(top); top = v; }
inline DU   POP()         { DU n=top; top=ss.pop(); return n; }
///================================================================================
///
///> IO & debug functions
///
inline void dot_r(int n, int v) { fout << setw(n) << setfill(' ') << v; }
inline void to_s(IU w) {
#if CC_DEBUG
    fout << dict[w].name << " " << w << (dict[w].attr ? "* " : " ");
#else  // !CC_DEBUG
    fout << " " << dict[w].name;
#endif // CC_DEBUG
}
void spaces(int n) { for (int i = 0; i < n; i++) fout << " "; }
void s_quote(IU op) {
    const char *s = scan('"')+1;       ///> string skip first blank
    if (compile) {
        add_w(op);                     ///> dostr, (+parameter field)
        add_str(s);                    ///> byte0, byte1, byte2, ..., byteN
    }
    else fout << s;                    ///> just print
}
///
/// recursively disassemble colon word
///
void see(IU pfa, int dp=1) {
    auto pfa2word = [](IU ix) {
        IU   pfa = ix & ~UDF_FLAG;                 ///> pfa (mask colon word)
        FPTR xt  = Code::XT(pfa);                  ///> lambda pointer
        for (int i = dict.idx - 1; i >= 0; --i) {
            if (ix & UDF_FLAG) {
                if (dict[i].pfa == pfa) return i;  ///> compare pfa in PMEM
            }
            else if (dict[i].xt == xt) return i;   ///> compare xt
        }
        return WORD_NA;
    };
    U8 *ip = MEM(pfa);
    while (*(IU*)ip) {
        fout << ENDL; for (int i=dp; i>0; i--) fout << "  ";  ///> indentation
        fout << setw(4) << (ip - MEM0) << "[" << setw(-1);    ///> display word offset
        IU c = pfa2word(*(IU*)ip);                            ///> fetch word index by pfa
        to_s(c);                                              ///> display name
        if (IS_UDF(c) && dp < 2) {                            ///> is a colon word
            see(dict[c].pfa, dp+1);                           ///> recursive into child
        }
        ip += sizeof(IU);
        switch (c) {
        case DOVAR: case DOLIT:
            fout << "= " << *(DU*)ip; ip += sizeof(DU); break;
        case DOSTR: case DOTSTR:
            fout << "= \"" << (char*)ip << '"';
            ip += STRLEN((char*)ip); break;
        case BRAN: case ZBRAN: case DONEXT:
            fout << "= " << *(IU*)ip; ip += sizeof(IU); break;
        case DODOES:
            ip += sizeof(IU); break;
        }
        fout << " ] ";
    }
}
void words() {
    const int WIDTH = 68;
    int sz = 0;
    fout << setbase(10);
    for (int i=0; i<dict.idx; i++) {
        const char *nm = dict[i].name;
        if (nm[0] != '_') {
            sz += strlen(nm) + 2;
            fout << "  " << nm;
        }
        if (sz > WIDTH) {
            sz = 0;
            fout << ENDL;
            yield();
        }
    }
    fout << setbase(base) << ENDL;
}
void ss_dump() {
#if   DO_WASM
    fout << "ok" << ENDL;
#else  // DO_WASM
    fout << " <"; for (int i=0; i<ss.idx; i++) { fout << ss[i] << " "; }
    fout << top << "> ok" << ENDL;
#endif // DO_WASM
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
///====================================================================================
///
///> ForthVM - front-end proxy class
///
const char *vm_version(){
    static string ver = string(APP_NAME) + " " + MAJOR_VERSION + "." + MINOR_VERSION;
    return ver.c_str();
}
///
///> eForth dictionary initializer
///  Note: sequenced by enum forth_opcode as following
///
UFP Code::XT0 = ~0;    ///< init base of xt pointers (before calling CODE macros)
UFP Code::NM0 = ~0;    ///< init base of name pointers

void dict_compile() {  ///< compile primitive words into dictionary
    ///
    /// @defgroup Execution flow ops
    /// @brief - DO NOT change the sequence here (see forth_opcode enum)
    /// @{
    CODE("_exit",    IP = rs.pop());                    // handled in nest()
    CODE("_donext",                                     // handled in nest()
         if ((rs[-1] -= 1) >= 0) IP = *(IU*)MEM(IP);    // rs[-1]-=1 saved 200ms/1M cycles
         else { IP += sizeof(IU); rs.pop(); });
    CODE("_dovar",   PUSH(IP);            IP += sizeof(DU));
    CODE("_dolit",   PUSH(*(DU*)MEM(IP)); IP += sizeof(DU));
    CODE("_dostr",
         const char *s = (const char*)MEM(IP);     // get string pointer
         IU    len = STRLEN(s);
         PUSH(IP); PUSH(len); IP += len);
    CODE("_dotstr",
         const char *s = (const char*)MEM(IP);     // get string pointer
         fout << s;  IP += STRLEN(s));             // send to output console
    CODE("_branch" , IP = *(IU*)MEM(IP));          // unconditional branch
    CODE("_0branch", IP = POP() ? IP + sizeof(IU) : *(IU*)MEM(IP)); // conditional branch
    CODE("_dodoes",
    	 add_w(BRAN);                              // branch to does> section
    	 add_iu(IP + sizeof(IU));                  // encode IP
    	 add_w(EXIT));                             // EXIT is not really needed but nicer
    CODE(">r",   rs.push(POP()));
    CODE("r>",   PUSH(rs.pop()));
    CODE("r@",   PUSH(rs[-1]));                    // same as I (the loop counter)
    /// @}
    /// @defgroup Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",  PUSH(top));
    CODE("drop", top = ss.pop());
    CODE("over", PUSH(ss[-1]));
    CODE("swap", DU n = ss.pop(); PUSH(n));
    CODE("rot",  DU n = ss.pop(); DU m = ss.pop(); ss.push(n); PUSH(m));
    CODE("pick", DU i = top; top = ss[-i]);
    /// @}
    /// @defgroup Stack ops - double
    /// @{
    CODE("2dup", PUSH(ss[-1]); PUSH(ss[-1]));
    CODE("2drop",ss.pop(); top = ss.pop());
    CODE("2over",PUSH(ss[-3]); PUSH(ss[-3]));
    CODE("2swap",
         DU n = ss.pop(); DU m = ss.pop(); DU l = ss.pop();
         ss.push(n); PUSH(l); PUSH(m));
    CODE("?dup", if (top != DU0) PUSH(top));
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",    top += ss.pop());
    CODE("*",    top *= ss.pop());
    CODE("-",    top =  ss.pop() - top);
    CODE("/",    top =  ss.pop() / top);
    CODE("mod",  top =  ss.pop() % top);
    CODE("*/",   top =  (DU2)ss.pop() * ss.pop() / top);
    CODE("/mod",
         DU n = ss.pop(); DU t = top;
         ss.push(n % t); top = (n / t));
    CODE("*/mod",
         DU2 n = (DU2)ss.pop() * ss.pop();
         DU2 t = top;
         ss.push((DU)(n % t)); top = (DU)(n / t));
    CODE("and",  top = ss.pop() & top);
    CODE("or",   top = ss.pop() | top);
    CODE("xor",  top = ss.pop() ^ top);
    CODE("abs",  top = abs(top));
    CODE("negate", top = -top);
    CODE("invert", top = ~top);
    CODE("rshift", top = ss.pop() >> top);
    CODE("lshift", top = ss.pop() << top);
    CODE("max",  DU n=ss.pop(); top = (top>n)?top:n);
    CODE("min",  DU n=ss.pop(); top = (top<n)?top:n);
    CODE("2*",   top *= 2);
    CODE("2/",   top /= 2);
    CODE("1+",   top += 1);
    CODE("1-",   top -= 1);
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0= ",  top = BOOL(top == DU0));
    CODE("0<",   top = BOOL(top <  DU0));
    CODE("0>",   top = BOOL(top >  DU0));
    CODE("=",    top = BOOL(ss.pop() == top));
    CODE(">",    top = BOOL(ss.pop() >  top));
    CODE("<",    top = BOOL(ss.pop() <  top));
    CODE("<>",   top = BOOL(ss.pop() != top));
    CODE(">=",   top = BOOL(ss.pop() >= top));
    CODE("<=",   top = BOOL(ss.pop() <= top));
    CODE("u<",   top = BOOL(UINT(ss.pop()) < UINT(top)));
    CODE("u>",   top = BOOL(UINT(ss.pop()) > UINT(top)));
    /// @}
    /// @defgroup IO ops
    /// @{
    CODE("ucase!",  ucase = POP());
    CODE("base@",   PUSH(base));
    CODE("base!",   fout << setbase(base = POP()));
    CODE("hex",     fout << setbase(base = 16));
    CODE("decimal", fout << setbase(base = 10));
    CODE("bl",      fout << " ");
    CODE("cr",      fout << ENDL);
    CODE(".",       fout << POP() << " ");
    CODE("u.",      fout << UINT(POP()) << " ");
    CODE(".r",      DU n = POP(); dot_r(n, POP()));
    CODE("u.r",     DU n = POP(); dot_r(n, UINT(POP())));
    CODE("type",
         IU    len  = POP();                        // string length (not used)
         const char *s = (const char*)MEM(POP());   // get string pointer
         fout << s);                                // send to output console
    CODE("key",     PUSH(next_idiom()[0]));
    CODE("emit",    char b = (char)POP(); fout << b);
    CODE("space",   spaces(1));
    CODE("spaces",  spaces(POP()));
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("[",       compile = false);
    CODE("]",       compile = true);
    IMMD("(",       scan(')'));
    IMMD(".(",      fout << scan(')'));
    IMMD("\\",      scan('\n'));
    IMMD("s\"",     s_quote(DOSTR));
    IMMD(".\"",     s_quote(DOTSTR));
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    IMMD("if",      add_w(ZBRAN); PUSH(HERE); add_iu(0));       // if    ( -- here )
    IMMD("else",                                                // else ( here -- there )
         add_w(BRAN);
         IU h=HERE; add_iu(0); SETJMP(POP()); PUSH(h));
    IMMD("then",    SETJMP(POP()));                             // backfill jump address
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",   PUSH(HERE));
    IMMD("again",   add_w(BRAN);  add_iu(POP()));               // again    ( there -- )
    IMMD("until",   add_w(ZBRAN); add_iu(POP()));               // until    ( there -- )
    IMMD("while",   add_w(ZBRAN); PUSH(HERE); add_iu(0));       // while    ( there -- there here )
    IMMD("repeat",  add_w(BRAN);                                // repeat    ( there1 there2 -- )
         IU t=POP(); add_iu(POP()); SETJMP(t));                 // set forward and loop back address
    /// @}
    /// @defgrouop For loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
/*
  do
  loop
  +loop
*/
    IMMD("for" ,    add_w(TOR); PUSH(HERE));                    // for ( -- here )
    IMMD("next",    add_w(DONEXT); add_iu(POP()));              // next ( here -- )
    IMMD("aft",                                                 // aft ( here -- here there )
         POP(); add_w(BRAN);
         IU h=HERE; add_iu(0); PUSH(HERE); PUSH(h));
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE(":",
         compile = true;
         if (!def_word(next_idiom())) compile = false);
    IMMD(";", add_w(EXIT); compile = false);
    CODE("variable",                                             // create a variable
         if (def_word(next_idiom())) {                           // create a new word on dictionary
             add_w(DOVAR);                                       // dovar (+parameter field)
             add_du(DU0);                                        // data storage (32-bit integer now)
             add_w(EXIT);
         });
    CODE("constant",                                             // create a constant
         if (def_word(next_idiom())) {                           // create a new word on dictionary
             add_w(DOLIT);                                       // dovar (+parameter field)
             add_du(POP());                                      // data storage (32-bit integer now)
             add_w(EXIT);
         });
    IMMD("immediate", dict[-1].attr |= IMM_ATTR);
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",  IU w = POP(); CALL(w));                        // execute word
    CODE("create",                                               // dovar (+ parameter field)
         if (def_word(next_idiom())) {                           // create a new word on dictionary
             add_w(DOVAR);
         });
    IMMD("does>", add_w(DODOES); add_w(EXIT));                   // CREATE...DOES>... meta-program
    CODE("to",              // 3 to x                            // alter the value of a constant
         IU w = find(next_idiom());                              // to save the extra @ of a variable
         *(DU*)(MEM(dict[w].pfa) + sizeof(IU)) = POP());
    CODE("is",              // ' y is x                          // alias a word
         IU w = find(next_idiom());                              // copy entire union struct
         dict[POP()].xt = dict[w].xt);
    ///
    /// be careful with memory access, especially BYTE because
    /// it could make access misaligned which slows the access speed by 2x
    ///
    CODE("@",     IU w = POP(); PUSH(CELL(w)));                  // w -- n
    CODE("!",     IU w = POP(); CELL(w) = POP(););               // n w --
    CODE(",",     DU n = POP(); add_du(n));
    CODE("allot", for (IU n = POP(), i = 0; i < n; i++) add_du(DU0)); // n --
    CODE("+!",    IU w = POP(); CELL(w) += POP());               // n w --
    CODE("?",     IU w = POP(); fout << CELL(w) << " ");         // w --
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("here",  PUSH(HERE));
    CODE("'",     IU w = find(next_idiom()); PUSH(w));
    CODE(".s",    fout << (char*)MEM(POP()));
    CODE("words", words());
    CODE("see",
         IU w = find(next_idiom());
         fout << "["; to_s(w);
         if (IS_UDF(w)) see(dict[w].pfa);                        // recursive call
         fout << "]" << ENDL);
    CODE("dump",  DU n = POP(); IU a = POP(); mem_dump(a, n));
    CODE("peek",  DU a = POP(); PUSH(PEEK(a)));
    CODE("poke",  DU a = POP(); POKE(a, POP()));
    CODE("forget",
         int w = find(next_idiom());
         if (w == WORD_NA) return;
         int b = find("boot")+1;
         if (w > b) {
             dict.clear(w);
             pmem.clear(dict[w].pfa - STRLEN(dict[w].name));
         }
         else { dict.clear(b); pmem.clear(); }
    );    
    CODE("ms",    PUSH(millis()));
    CODE("delay", delay(POP()));
    /// @}
    CODE("bye",   exit(0));
    CODE("boot",  dict.clear(find("boot") + 1); pmem.clear());
}
///==========================================================================
///
///> ForthVM Outer interpreter
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
    return n;
}

void vm_outer(const char *cmd, void(*callback)(int, const char*)) {
    fin.clear();                             ///> clear input stream error bit if any
    fin.str(cmd);                            ///> feed user command into input stream
    fout_cb = callback;                      ///> setup callback function
    fout.str("");                            ///> clean output buffer, ready for next run
    while (fin >> strbuf) {
        const char *idiom = strbuf.c_str();
        int w = find(idiom);                 ///> * get token by searching through dict
        if (w != WORD_NA) {                  ///> * word found?
            if (compile && !IS_IMM(w)) {     /// * in compile mode?
                add_w(w);                    /// * add to colon word
            }
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
///=================================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
#if CC_DEBUG
#if (ARDUINO || ESP32)
void mem_stat()  {
    LOG_KV("Core:",          xPortGetCoreID());
    LOG_KV(" heap[maxblk=",  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    LOG_KV(", avail=",       heap_caps_get_free_size(MALLOC_CAP_8BIT));
    LOG_KV(", ss_max=",      ss.max);
    LOG_KV(", rs_max=",      rs.max);
    LOG_KV(", pmem=",        HERE);
    LOG_KV("], lowest[heap=",heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    LOG_KV(", stack=",       uxTaskGetStackHighWaterMark(NULL));
    LOGS("]\n");
    if (!heap_caps_check_integrity_all(true)) {
//        heap_trace_dump();     // dump memory, if we have to
        abort();                 // bail, on any memory error
    }
}
void dict_dump() {
    LOG_KX("XT0=",        Code::XT0);
    LOG_KX("NM0=",        Code::NM0);
    LOG_KV(", sizeof(Code)=", sizeof(Code));
    LOGS("\n");
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        LOG(i);
        LOG_KX("> xt=",   (UFP)c.xt - Code::XT0);
        LOG_KX(":",       (UFP)c.xt);
        LOG_KX(", name=", (UFP)c.name - Code::NM0);
        LOG_KX(":"),      (UFP)c.name);
        LOGS(" ");        LOG(c.name);
        LOGS("\n");
    }
}

#else // (ARDUINO || ESP32)
void mem_stat()  {
    LOG_KX("heap[maxblk=", E4_PMEM_SZ);
    LOG_KX(", avail=",     E4_PMEM_SZ - HERE);
    LOG_KX(", ss_max=",    ss.max);
    LOG_KX(", rs_max=",    rs.max);
    LOG_KX(", pmem=",      HERE);
    LOG_KX("], stack_sz=", E4_SS_SZ);
}
void dict_dump() {
    printf("XT0=%lx, NM0=%lx, sizeof(Code)=%ld bytes\n",
           Code::XT0, Code::NM0, sizeof(Code));
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        printf("%3d> xt=%4x:%p name=%4x:%p %s\n",
            i, c.xtoff(), c.xt,
            (U16)((UFP)c.name - Code::NM0),
            c.name, c.name);
    }
}
#endif // (ARDUINO || ESP32)
#else  // CC_DEBUG
void mem_stat()   {}
void dict_dump()  {}

#endif // CC_DEBUG

///=================================================================================
///
///> Arduino/ESP32 SPIFFS interfaces
///
#if (ARDUINO || ESP32)
/// Arduino extra string handlers
int  find(string &s)  { return find(s.c_str()); }
void colon(string &s) { colon(s.c_str()); }
///
///> eForth turn-key code loader (from Flash memory)
///
#include <SPIFFS.h>
int forth_load(const char *fname) {
    auto dummy = [](int, const char *) { /* do nothing */ };
    if (!SPIFFS.begin()) {
        LOGS("Error mounting SPIFFS"); return 1;
    }
    File file = SPIFFS.open(fname, "r");
    if (!file) {
        LOGS("Error opening file:"); LOG(fname); return 1;
    }
    LOGS("Loading file: "); LOG(fname); LOGS("...");
    while (file.available()) {
        char cmd[256], *p = cmd, c;
        while ((c = file.read())!='\n') *p++ = c;   // one line a time
        *p = '\0';
        LOGS("\n<< "); LOG(cmd);                    // show bootstrap command
        forth_outer(cmd, dummy);
    }
    LOGS("Done loading.\n");
    file.close();
    SPIFFS.end();
    return 0;
}

#else // (ARDUINO || ESP32)
int forth_load(const char *fname) {
    printf("TODO: load resident applications from %s...\n", fname);
    return 0;
}

#endif // (ARDUINO || ESP32)

///==========================================================================
///
/// main program - Note: Arduino and ESP32 have their own main-loop
///
#include <iostream>                                     // cin, cout
#if DO_WASM
///
/// export functions (to WASM)
///
extern "C" {
void forth(int n, char *cmd) {
    static auto send_to_con = [](int len, const char *rst) { cout << rst; };
    vm_outer(cmd, send_to_con);
}
int  vm_base()       { return base;     }
int  vm_ss_idx()     { return ss.idx;   }
int  vm_dict_idx()   { return dict.idx; }
DU   *vm_ss()        { return &ss[0];   }
char *vm_dict(int i) { return (char*)dict[i].name; }
char *vm_mem()       { return (char*)&pmem[0]; }
}
int main(int ac, char* av[]) {
    dict_compile();                                     // initialize dictionary
    dict_dump();
    return 0;
}

#else // !DO_WASM
///
/// main program for testing on Desktop PC (Linux and Cygwin)
///
int main(int ac, char* av[]) {
    dict_compile();                                     ///> initialize dictionary
    forth_load("/load.txt");                            ///> compile /data/load.txt

    dict_dump();
    cout << vm_version() << endl;

    static auto send_to_con = [](int len, const char *rst) { cout << rst; };
    string cmd;
    while (getline(cin, cmd)) {                         ///> fetch user input
        // printf("cmd=<%s>\n", cmd.c_str());
        vm_outer(cmd.c_str(), send_to_con);             ///> execute outer interpreter
    }
    cout << "Done!" << endl;
    return 0;
}
#endif // DO_WASM
