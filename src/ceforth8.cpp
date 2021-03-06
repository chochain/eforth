///
/// ceForth8 - eForth implemented in C++
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
#if !(ARDUINO || ESP32)
#include <stdint.h>     // uintxx_t
#include <stdlib.h>     // strtol
#include <string.h>     // strcmp
#include <exception>    // try...catch, throw

using namespace std;
/// version
#define APP_NAME        "eForth"
#define MAJOR_VERSION   "8"
#define MINOR_VERSION   "1"
///
/// conditional compililation options
///
/// Note: lambda enables VM taking parameters (with capture)
///       but slows nest() by 3%
///
#define LAMBDA_OK       1
#define RANGE_CHECK     0
#define INLINE          __attribute__((always_inline))
///
/// memory block configuation
///
#define E4_SS_SZ        64
#define E4_RS_SZ        64
#define E4_DICT_SZ      1024
#define E4_PMEM_SZ      (48*1024)
///
/// multi-platform support
///
#if _WIN32 || _WIN64
#define ENDL "\r\n"
#else  // _WIN32 || _WIN64
#define ENDL endl; fout_cb(fout.str().length(), fout.str().c_str()); fout.str("")
#endif // _WIN32 || _WIN64
///
/// system timing macros
///
#include <chrono>
#include <thread>
#define millis()        chrono::duration_cast<chrono::milliseconds>( \
                            chrono::steady_clock::now().time_since_epoch()).count()
#define delay(ms)       this_thread::sleep_for(chrono::milliseconds(ms))
#define yield()         this_thread::yield()
///
/// logical units (instead of physical) for type check and portability
///
typedef uint16_t  IU;    // instruction pointer unit
typedef int32_t   DU;    // data unit
typedef int64_t   DU2;   // double precision data unit
typedef uint16_t  U16;   // unsigned 16-bit integer
typedef uint8_t   U8;    // byte, unsigned character
typedef uintptr_t UFP;
///
/// alignment macros
///
#define ALIGN2(sz)      ((sz) + (-(sz) & 0x1))
#define ALIGN4(sz)      ((sz) + (-(sz) & 0x3))
#define ALIGN16(sz)     ((sz) + (-(sz) & 0xf))
#define ALIGN32(sz)     ((sz) + (-(sz) & 0x1f))
#define ALIGN(sz)       ALIGN2(sz)
#define STRLEN(s)       (ALIGN(strlen(s)+1))  /** calculate string size with alignment */
///
/// array class template (so we don't have dependency on C++ STL)
/// Note:
///   * using decorator pattern
///   * this is similar to vector class but much simplified
///
template<class T, int N>
struct List {
    T   *v;             /// fixed-size array storage
    int idx = 0;        /// current index of array
    int max = 0;        /// high watermark for debugging

    List()  { v = new T[N]; }      /// dynamically allocate array storage
    ~List() { delete[] v;   }      /// free memory

    List &operator=(T *a)   INLINE { v = a; return *this; }
    T    &operator[](int i) INLINE { return i < 0 ? v[idx + i] : v[i]; }

#if RANGE_CHECK
    T pop()     INLINE {
        if (idx>0) return v[--idx];
        throw "ERR: List empty";
    }
    T push(T t) INLINE {
        if (idx<N) return v[max=idx++] = t;
        throw "ERR: List full";
    }
#else  // RANGE_CHECK
    T pop()     INLINE { return v[--idx]; }
    T push(T t) INLINE { return v[max=idx++] = t; }
#endif // RANGE_CHECK
    void push(T *a, int n) INLINE { for (int i=0; i<n; i++) push(*(a+i)); }
    void merge(List& a)    INLINE { for (int i=0; i<a.idx; i++) push(a[i]);}
    void clear(int i=0)    INLINE { idx=i; }
};
///
/// universal functor (no STL) and Code class
/// Note:
///   * 8-byte on 32-bit machine, 16-byte on 64-bit machine
///
#if LAMBDA_OK
struct fop { virtual void operator()() = 0; };
template<typename F>
struct XT : fop {           // universal functor
    F fp;
    XT(F &f) : fp(f) {}
    void operator()() INLINE { fp(); }
};
typedef fop* FPTR;          // lambda function pointer
struct Code {
    const char *name = 0;   /// name field
    union {                 /// either a primitive or colon word
        FPTR xt = 0;        /// lambda pointer
        struct {            /// a colon word
            U16 def:  1;    /// colon defined word
            U16 immd: 1;    /// immediate flag
            U16 xxx:  14;   /// reserved
            IU  pfa;        /// offset to pmem space
        };
    };
    template<typename F>    /// template function for lambda
    Code(const char *n, F f, bool im=false) : name(n) {
        xt = new XT<F>(f);
        immd = im ? 1 : 0;
    }
    Code() {}               /// create a blank struct (for initilization)
};
#define CODE(s, g) { s, []() { g; }}
#define IMMD(s, g) { s, []() { g; }, true }
#else  // LAMBDA_OK
///
/// a lambda without capture can degenerate into a function pointer
///
typedef void (*FPTR)();
struct Code {
    const char *name = 0;   /// name field
    union {                 /// either a primitive or colon word
        FPTR xt = 0;        /// lambda pointer
        struct {            /// a colon word
            U16 def:  1;    /// colon defined word
            U16 immd: 1;    /// immediate flag
            U16 len:  14;   /// len of pf (16K max)
            IU  pfa;        /// offset to pmem space (16-bit for 64K range)
        };
    };
    Code(const char *n, FPTR f, bool im=false) : name(n), xt(f) {
        immd = im ? 1 : 0;
    }
    Code() {}               /// create a blank struct (for initilization)
};
#define CODE(s, g) { s, []{ g; }}
#define IMMD(s, g) { s, []{ g; }, true }
#endif // LAMBDA_OK
///==============================================================================
///
/// main storages in RAM
/// Note:
///   1.By separating pmem from dictionary, it makes dictionary uniform size
///   * (i.e. the RISC vs CISC debate) which eliminates the need for link field
///   * however, it requires size tuning manually
///   2.For ease of byte counting, we use U8 for pmem instead of U16.
///   * this makes IP increment by 2 instead of word size. If needed, it can be
///   * readjusted.
///
List<DU,   E4_RS_SZ>   rs;        /// return stack
List<DU,   E4_SS_SZ>   ss;        /// data stack, can reside in registers for some processors
List<Code, E4_DICT_SZ> dict;      /// fixed sized dictionary (RISC vs CISC)
List<U8,   E4_PMEM_SZ> pmem;      /// parameter memory i.e. storage for all colon definitions
U8   *MEM0 = &pmem[0];            /// cached memory base (saved 200ms/1M cycles)
UFP  XT0   = ~0;                  /// base of function pointers, set in init()
///
/// system variables
///
bool compile = false;             /// compiler flag
bool ucase   = true;              /// case sensitivity control
DU   top = -1, base = 10;
IU   WP  = 0;                     /// current word pointer
IU   IP  = 0;                     /// current instruction pointer and cached base pointer
///
/// macros to abstract dict and pmem physical implementation
/// Note:
///   so we can change pmem implementation anytime without affecting opcodes defined below
///
#define BOOL(f)   ((f)?-1:0)
#define PFA(w)    (dict[w].pfa)             /** parameter field pointer of a word        */
#define HERE      (pmem.idx)                /** current parameter memory index           */
#define MEM(ip)   (MEM0 + (ip))             /** pointer to IP address fetched from pmem  */
#define XTOFF(xp) ((IU)((UFP)(xp) - XT0))   /** XT offset (index) in code space          */
#define XT(xt)    (XT0 + ((xt) & ~0x3))     /** convert XT offset to function pointer    */
#define CELL(a)   (*(DU*)&pmem[a])          /** fetch a cell from parameter memory       */
#define SETJMP(a) (*(IU*)&pmem[a] = HERE)   /** address offset for branching opcodes     */

typedef enum {
    EXIT = 0, DONEXT, DOVAR, DOLIT, DOSTR, DOTSTR, BRAN, ZBRAN, DOES, TOR
} forth_opcode;

///==============================================================================
///
/// dictionary search functions - can be adapted for ROM+RAM
///
int pfa2word(IU ix) {
    IU   def = ix & 1;
    IU   pfa = ix & ~0x1;             /// TODO: handle colon immediate words when > 64K
    UFP  xt  = XT(ix);                /// can xt be immediate? i.e. ix & ~0x3
    for (int i = dict.idx - 1; i >= 0; --i) {
        if (def) {
            if (dict[i].pfa == pfa) return i;      // compare pfa in PMEM
        }
        else if ((UFP)dict[i].xt == xt) return i;  // compare xt (no immediate?)
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
///
/// inline functions to add (i.e. 'comma') object into parameter memory
///
inline void add_iu(IU i) { pmem.push((U8*)&i, sizeof(IU));  }                      /** add an instruction into pmem */
inline void add_du(DU v) { pmem.push((U8*)&v, sizeof(DU));  }                      /** add a cell into pmem         */
inline void add_str(const char *s) { int sz = STRLEN(s); pmem.push((U8*)s,  sz); } /** add a string to pmem         */
void add_w(IU w) {
    Code &c = dict[w];
    IU   ip = c.def ? (c.pfa | 1) : (w==EXIT ? 0 : XTOFF(c.xt));
    add_iu(ip);
    //printf("add_w(%d) => %4x:%p %s\n", w, ip, c.xt, c.name);
}
///==============================================================================
///
/// colon word compiler
/// Note:
///   * we separate dict and pmem space to make word uniform in size
///   * if they are combined then can behaves similar to classic Forth
///   * with an addition link field added.
///
void colon(const char *name) {
    char *nfa = (char*)&pmem[HERE];         // current pmem pointer
    int sz = STRLEN(name);                  // string length, aligned
    pmem.push((U8*)name,  sz);              // setup raw name field
#if LAMBDA_OK
    Code c(nfa, [](){});                    // create a new word on dictionary
#else  // LAMBDA_OK
    Code c(nfa, NULL);
#endif // LAMBDA_OK
    c.def = 1;                              // specify a colon word
    c.pfa = HERE;                           // capture code field index
    dict.push(c);                           // deep copy Code struct into dictionary
    printf("%3d> pfa=%x, name=%4x:%p %s\n", dict.idx-1,
        dict[-1].pfa, (IU)((U8*)dict[-1].name - MEM0), dict[-1].name, dict[-1].name);
};
///
/// Forth inner interpreter
///
#define CALL(w) \
    if (dict[w].def) { WP = w; IP = PFA(w); nest(); } \
    else (*(FPTR)((UFP)dict[w].xt & ~0x3))()
///
/// recursive version (look nicer but use system stack)
/// Note: superceded by iterator version below (~8% faster)
/*
void nest() {
    IU *ip = MEM0 + IP;
    while (*ip) {
        if (*ip & 1) {
            /// ENTER
            rs.push(WP);
            rs.push(IP + sizeof(IU));       /// * setup call frame
            IP = *ip & ~0x1;
            nest();
            /// EXIT
            IP = MEM(rs.pop());                  /// * restore call frame
            WP = rs.pop();
        }
        else {
            UFP xt = XT(*(IU*)IP & ~0x3);        /// * function pointer
            IP += sizeof(IU);                    /// * advance to next opcode
            (*(FPTR)xt)();
        }
    }
    yield();                                ///> give other tasks some time
}
*/
///
/// interative version
///
/// TODO: build Just-in-time code(ip, dp) for inner loop
///
void nest() {
	static IU _NXT = XTOFF(dict[find("donext")].xt); /// cache offset to subroutine address
    int dp = 0;                                      /// iterator depth control
    while (dp >= 0) {
        IU ix = *(IU*)MEM(IP);                       /// hopefully use register than cached line
        while (ix) {                                 /// fetch till EXIT
            IP += sizeof(IU);                        /// advance instruction pointer
            if (ix & 1) {                            /// is a colon word?
                rs.push(WP);                         /// * setup callframe (ENTER)
                rs.push(IP);
                IP = ix & ~0x1;                      /// word pfa (def masked)
                dp++;                                /// one level deeper
            }
            else if (ix == _NXT) {                   /// check cached opcodes (saved 250ms / 100M cycles)
                if ((rs[-1] -= 1) >= 0) IP = *(IU*)MEM(IP);  /// fetch branch target address
                else { IP += sizeof(IU); rs.pop(); }         /// break, step through
            }
            else (*(FPTR)XT(ix))();                  /// * execute primitive word
            ix = *(IU*)MEM(IP);                      /// fetch next opcode
        }
        if (dp-- > 0) {
            IP = rs.pop();                           /// * restore call frame (EXIT)
            WP = rs.pop();
        }
        yield();                                     ///> give other tasks some time
    }
}
///==============================================================================
///
/// utilize C++ standard template libraries for core IO functions only
/// Note:
///   * we use STL for its convinence, but
///   * if it takes too much memory for target MCU,
///   * these functions can be replaced with our own implementation
///
#include <sstream>      // iostream, stringstream
#include <iomanip>      // setbase
#include <string>       // string class
istringstream   fin;    // forth_in
ostringstream   fout;   // forth_out
void (*fout_cb)(int, const char*);   // forth output callback function
string strbuf;          // input string buffer
///==============================================================================
///
/// debug functions
///
void dot_r(int n, int v) { fout << setw(n) << setfill(' ') << v; }
void to_s(IU c) {
    fout << dict[c].name << " " << c << (dict[c].immd ? "* " : " ");
}
///
/// recursively disassemble colon word
///
void see(IU pfa, int dp=1) {
	U8 *ip = MEM(pfa);
    while (*(IU*)ip) {
        fout << ENDL; for (int i=dp; i>0; i--) fout << "  ";   // indentation
        fout << setw(4) << (ip - MEM0) << "[ " << setw(-1);
        IU c = pfa2word(*(IU*)ip);
        to_s(c);                                               // name field
        if (dict[c].def && dp < 2) {                           // is a colon word
            see(PFA(c), dp+1);                                 // recursive into child PFA
        }
        ip += sizeof(IU);
        switch (c) {
        case DOVAR: case DOLIT:
            fout << "= " << *(DU*)ip; ip += sizeof(DU); break;
        case DOSTR: case DOTSTR:
            fout << "= \"" << (char*)ip << '"';
            ip += STRLEN((char*)ip); break;
        case DONEXT: case BRAN: case ZBRAN:
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
inline char *next_idiom() { fin >> strbuf; return (char*)strbuf.c_str(); } // get next idiom
inline char *scan(char c) { getline(fin, strbuf, c); return (char*)strbuf.c_str(); }
inline DU   POP()         { DU n=top; top=ss.pop(); return n; }
inline void PUSH(DU v)    { ss.push(top); top = v; }
///
/// global memory access macros
///
#define     PEEK(a)    (DU)(*(DU*)((UFP)(a)))
#define     POKE(a, c) (*(DU*)((UFP)(a))=(DU)(c))
///================================================================================
///
/// primitives (ROMable)
/// Note:
///   * we merge prim into dictionary in main()
///   * However, since primitive is statically compiled
///   * it can be stored in ROM, and only
///   * find() needs to be modified to support ROM+RAM
///
static Code prim[] = {
    ///
    /// @defgroup Execution flow ops
    /// @brief - DO NOT change the sequence here (see forth_opcode enum)
    /// @{
    CODE("exit",    IP = rs.pop(); WP = rs.pop()), // handled in nest()
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
    CODE(".f",      DU n = POP(); fout << setprecision(n) << POP()),
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
        *(DU*)(MEM(PFA(w)) + sizeof(IU)) = POP()),
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
///================================================================================
/// Forth Virtual Machine
///
///   dictionary initialization
///
void forth_init() {
    for (int i=0; i<PSZ; i++) {              /// copy prim(ROM) into fast RAM dictionary,
        dict.push(prim[i]);                  /// find() can be modified to support
        if (((UFP)dict[i].xt - 4) < XT0) XT0 = ((UFP)dict[i].xt - 4); /// collect xt base
    }
    printf("XT0=%lx, sizeof(Code)=%ld byes\n", XT0, sizeof(Code));

    for (int i=0; i<PSZ; i++) {
        printf("%3d> xt=%4x:%p name=%4x:%p %s\n", i,
            XTOFF(dict[i].xt), dict[i].xt,
            (U16)(dict[i].name - dict[0].name),
            dict[i].name, dict[i].name);
    }
}
///
/// outer interpreter
///
void forth_outer(const char *cmd, void(*callback)(int, const char*)) {
    fin.clear();                             /// clear input stream error bit if any
    fin.str(cmd);                            /// feed user command into input stream
    fout_cb = callback;                      /// setup callback function
    fout.str("");                            /// clean output buffer, ready for next
    while (fin >> strbuf) {
        const char *idiom = strbuf.c_str();
        // printf("find(%s) => ",idiom);
        int w = find(idiom);                 /// * search through dictionary
        if (w>=0) {                          /// * word found?
            // printf("%s %d\n", dict[w].name, w);
            if (compile && !dict[w].immd) {  /// * in compile mode?
                add_w(w);                    /// * add found word to new colon word
            }
            else CALL(w);                    /// * execute word
            continue;
        }
        // try as a number
        char *p;
        int n = static_cast<int>(strtol(idiom, &p, base));
        // printf("%d\n", n);
        if (*p != '\0') {                    /// * not number
            fout << idiom << "? " << ENDL;   ///> display error prompt
            compile = false;                 ///> reset to interpreter mode
            break;                           ///> skip the entire input buffer
        }
        // is a number
        if (compile) {                       /// * add literal when in compile mode
            add_w(DOLIT);                    ///> dovar (+parameter field)
            add_du(n);                       ///> data storage (32-bit integer now)
        }
        else PUSH(n);                        ///> or, add value onto data stack
    }
    if (!compile) ss_dump();
}

#include <iostream>     // cin, cout
int main(int ac, char* av[]) {
    static auto send_to_con = [](int len, const char *rst) { cout << rst; };
    forth_init();
    cout << unitbuf << APP_NAME << " " << MAJOR_VERSION << "." << MINOR_VERSION << endl;

    string line;
    while (getline(cin, line)) {             /// fetch line from user console input
        forth_outer(line.c_str(), send_to_con);
    }
    cout << "Done." << endl;

    return 0;
}
#endif // !(ARDUINO || ESP32)
