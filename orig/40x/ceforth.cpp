///
/// @file
/// @brief eForth implemented in 100% C/C++ for portability and education
///
#include "ceforth.h"
///====================================================================
///
///> Global memory blocks
///
/// Note:
///   1.By separating pmem from dictionary,
///   * it makes dictionary uniform size which eliminates the need for link field
///   * however, it requires array size tuning manually
///   2.Using 16-bit xt offset in parameter field (instead of full 32 or 64 bits),
///   * it unified xt/pfa parameter storage and use the LSB for id flag
///   * that compacts memory usage while avoiding the double lookup of
///   * token threaded indexing.
///   * However, it limits function pointer spread within range of 64KB
///   3.For ease of byte counting, U8* is used for pmem instead of U16*.
///   * this makes IP increment by 2 instead of word size.
///   * If needed, it can be readjusted.
///
///> Dictionary structure (N=E4_DICT_SZ in config.h)
///     dict[0].xt ---------> pointer to build-in word lambda[0]
///     dict[1].xt ---------> pointer to built-in word lambda[1]
///     ...
///     dict[N-1].xt -------> pointer to last built-in word lambda[N-1]
///
///> Parameter memory structure (memory block=E4_PMEM_SZ in config.h)
///     dict[N].xt ----+ user defined colon word)    dict[N+1].xt------+
///                    |                                               |
///     +--MEM0        v                                               v
///     +--------------+--------+--------+-----+------+----------------+-----
///     | str nameN \0 |  parm1 |  parm2 | ... | ffff | str nameN+1 \0 | ...
///     +--------------+--------+--------+-----+------+----------------+-----
///     ^              ^        ^        ^     ^      ^
///     | strlen+1     | 2-byte | 2-byte |     |      |
///     +--------------+--------+--------+-----+------+---- 2-byte aligned
///
///> Parameter structure - 16-bit aligned (use MSB for colon/primitive word flag)
///   * primitive word
///     16-bit xt offset with MSB set to 1, where opcode < MAX_OP
///     +-+--------------+
///     |1|   opcode     |   call exec_prim(opcode)
///     +-+--------------+
///
///   * colon word (user defined)
///     16-bit pmem offset with MSB set to 1, where dict.pfa >= MAX_OP
///     +--------------+-+
///     |1|   dict.pfa   |   IP = dict.pfa
///     +--------------+-+
///
///   * built-in word
///     16-bit xt offset with MSB set to 0 (1 less mem lookup for xt)
///     +-+--------------+
///     |0| dict.xtoff() |   call (XT0 + *IP)() to execute
///     +-+--------------+
///
List<DU,   E4_RS_SZ>   rs;         ///< return stack
List<Code, E4_DICT_SZ> dict;       ///< dictionary
List<U8,   E4_PMEM_SZ> pmem;       ///< parameter memory (for colon definitions)
U8  *MEM0 = &pmem[0];              ///< base of parameter memory block
///
///> Macros to abstract dict and pmem physical implementation
///  Note:
///    so we can change pmem implementation anytime without affecting opcodes defined below
///
///@name Dictionary and data stack access macros
///@{
#define TOS       (vm._tos)                /**< Top of stack                            */
#define SS        (vm._ss)                 /**< parameter stack (per task)              */
#define IP        (vm._ip)                 /**< instruction pointer (per task)          */
#define BOOL(f)   ((f)?-1:0)               /**< Forth boolean representation            */
#define HERE      (pmem.idx)               /**< current parameter memory index          */
#define LAST      (dict[dict.idx-1])       /**< last colon word defined                 */
#define MEM(a)    (MEM0 + (IU)UINT(a))     /**< pointer to address fetched from pmem    */
#define IGET(ip)  (*(IU*)MEM(ip))          /**< instruction fetch from pmem+ip offset   */
#define CELL(a)   (*(DU*)&pmem[a])         /**< fetch a cell from parameter memory      */
#define SETJMP(a) (*(IU*)&pmem[a] = HERE)  /**< address offset for branching opcodes    */
///@}
///
///> Primitive words (to simplify compiler), see nest() for details
///
Code prim[] = {
    Code(";",   EXIT), Code("nop",  NOP),   Code("next", NEXT),  Code("loop", LOOP),
    Code("lit", LIT),  Code("var",  VAR),   Code("str",  STR),   Code("dotq", DOTQ),
    Code("bran",BRAN), Code("0bran",ZBRAN), Code("vbran",VBRAN), Code("does>",DOES),
    Code("for", FOR),  Code("do",   DO),    Code("key",  KEY)
};
#define DICT(w) (IS_PRIM(w) ? prim[w & ~EXT_FLAG] : dict[w])
///
///====================================================================
///
///> Dictionary search functions - can be adapted for ROM+RAM
///
IU find(const char *s) {
    IU v = 0;
    for (IU i = dict.idx - 1; !v && i > 0; --i) {
        if (STRCMP(s, dict[i].name)==0) v = i;
    }
#if CC_DEBUG > 1
    LOG_HDR("find", s); if (v) { LOG_DIC(v); } else LOG_NA();
#endif // CC_DEBUG > 1
    return v;
}
///====================================================================
///
///> Colon word compiler
///  Note:
///    * we separate dict and pmem space to make word uniform in size
///    * if they are combined then can behaves similar to classic Forth
///    * with an addition link field added.
///
void colon(const char *name) {
    char *nfa = (char*)&pmem[HERE]; ///> current pmem pointer
    int sz = STRLEN(name);          ///> string length, aligned
    pmem.push((U8*)name,  sz);      ///> setup raw name field

    Code c(nfa, (FPTR)0, false);    ///> create a local blank word
    c.attr = UDF_ATTR;              ///> specify a colon (user defined) word
    c.pfa  = HERE;                  ///> capture code field index

    dict.push(c);                   ///> deep copy Code struct into dictionary
}
void add_iu(IU i) { pmem.push((U8*)&i, sizeof(IU)); }  ///< add an instruction into pmem
void add_du(DU v) { pmem.push((U8*)&v, sizeof(DU)); }  ///< add a cell into pmem
int  add_str(const char *s) {       ///< add a string to pmem
    int sz = STRLEN(s);
    pmem.push((U8*)s,  sz);         /// * add string terminated with zero
    return sz;
}
void add_w(IU w) {                  ///< add a word index into pmem
    Code &c = DICT(w);              /// * code ref to primitive or dictionary entry
    IU   ip = (w & EXT_FLAG)        /// * is primitive?
        ? (UFP)c.xt                 /// * get primitive/built-in token
        : (IS_UDF(w)                /// * colon word?
           ? (c.pfa | EXT_FLAG)     /// * pfa with colon word flag
           : c.xtoff());            /// * XT offset of built-in
    add_iu(ip);
#if CC_DEBUG > 1
    LOG_KV("add_w(", w); LOG_KX(") => ", ip);
    LOGS(" "); LOGS(c.name); LOGS("\n");
#endif // CC_DEBUG > 1
}
void add_var(IU op, DU v=DU0) {     ///< add a varirable header
    add_w(op);                      /// * VAR or VBRAN
    if (op==VBRAN) add_iu(0);       /// * pad offset field
    pmem.idx = DALIGN(pmem.idx);    /// * data alignment (WASM 4, other 2)
    if (op!=VBRAN) add_du(v);       /// * default variable = 0
}
///====================================================================
///
///> functions to reduce verbosity
///
#define PUSH(v) (SS.push(TOS), TOS = v)
#define POP()   ({ DU n=TOS; TOS=SS.pop(); n; })

int def_word(const char* name) {    ///< display if redefined
    if (name[0]=='\0') {            /// * missing name?
        pstr(" name?", CR); return 0;
    }  
    if (find(name)) {               /// * word redefined?
        pstr(name); pstr(" reDef? ", CR);
    }
    colon(name);                    /// * create a colon word
    return 1;                       /// * created OK
}
char *word() {                      ///< get next idiom
    static string tmp;              ///< temp string holder
    if (!fetch(tmp)) tmp.clear();   /// * input buffer exhausted?
    return (char*)tmp.c_str();
}
void key() {
    VM& vm = vm_instance();
    PUSH(word()[0]);
}
void s_quote(prim_op op) {
    const char *s = scan('"')+1;    ///> string skip first blank
    VM& vm = vm_instance();
    if (vm.compile) {
        add_w(op);                  ///> dostr, (+parameter field)
        add_str(s);                 ///> byte0, byte1, byte2, ..., byteN
    }
    else {                          ///> use PAD ad TEMP storage
        IU h0  = HERE;              ///> keep current memory addr
        DU len = add_str(s);        ///> write string to PAD
        PUSH(h0);                   ///> push string address
        PUSH(len);                  ///> push string length
        HERE = h0;                  ///> restore memory addr
    }
}
void load(const char* fn) {
    VM& vm = vm_instance();
    vm.load_dp++;                   /// * increment depth counter
    rs.push(IP);                    /// * save context
    vm.state = NEST;                /// * +recursive
    forth_include(fn);              /// * include file
    IP = UINT(rs.pop());            /// * restore context
    --vm.load_dp;                   /// * decrement depth counter
}
///====================================================================
///
///> Forth inner interpreter (handles a colon word)
///  Note:
///  * overhead here in C call/return vs NEXT threading (in assembly)
///  * use of dp (iterative depth control) instead of WP by Dr. Ting
///    speeds up 8% vs recursive calls but loose the flexibity of Forth
///  * use of cached _NXT address speeds up 10% on AMD but
///    5% slower on ESP32 probably due to shallow pipeline
///  * computed label runs 15% faster, but needs long macros (for enum)
///  * use local stack speeds up 10%, but allot 4*64 bytes extra
///
///  TODO: performance tuning
///    1. Just-in-time cache(ip, dp)
///    2. Co-routine
///
#define DISPATCH(op) switch(op)
#define CASE(op, g)  case op : { g; } break
#define OTHER(g)     default : { g; } break
#define UNNEST()     (vm.state = (IP=UINT(rs.pop())) ? HOLD : STOP)

void nest(VM& vm) {
    vm.state = NEST;                                 /// * activate VM
    while (vm.state==NEST && IP) {
        IU ix = IGET(IP);                            ///< fetched opcode, hopefully in register
//        printf("[%4x]:%4x", IP, ix);
        IP += sizeof(IU);
        DISPATCH(ix) {                               /// * opcode dispatcher
        CASE(EXIT, UNNEST());
        CASE(NOP,  { /* do nothing */});
        CASE(NEXT,
             if (GT(rs[-1] -= DU1, -DU1)) {          ///> loop done?
                 IP = IGET(IP);                      /// * no, loop back
             }
             else {                                  /// * yes, loop done!
                 rs.pop();                           /// * pop off loop counter
                 IP += sizeof(IU);                   /// * next instr.
             });
        CASE(LOOP,
             if (GT(rs[-2], rs[-1] += DU1)) {        ///> loop done?
                 IP = IGET(IP);                      /// * no, loop back
             }
             else {                                  /// * yes, done
                 rs.pop(); rs.pop();                 /// * pop off counters
                 IP += sizeof(IU);                   /// * next instr.
             });
        CASE(LIT,
             SS.push(TOS);
             IP  = DALIGN(IP);
             TOS = *(DU*)MEM(IP);                    ///> from hot cache, hopefully
             IP += sizeof(DU));                      /// * hop over the stored value
        CASE(VAR, PUSH(DALIGN(IP)); UNNEST());       ///> get var addr, alignment?
        CASE(STR,
             const char *s = (const char*)MEM(IP);   ///< get string pointer
             IU    len = STRLEN(s);
             PUSH(IP); PUSH(len); IP += len);
        CASE(DOTQ,                                   /// ." ..."
             const char *s = (const char*)MEM(IP);   ///< get string pointer
             pstr(s); IP += STRLEN(s));              /// * send to output console
        CASE(BRAN, IP = IGET(IP));                   /// * unconditional branch
        CASE(ZBRAN,                                  /// * conditional branch
             IP = POP() ? IP+sizeof(IU) : IGET(IP));
        CASE(VBRAN,
             PUSH(DALIGN(IP + sizeof(IU)));          /// * skip target address
             if ((IP = IGET(IP))==0) UNNEST());      /// * jump target of does> if given
        CASE(DOES,
             IU *p = (IU*)MEM(LAST.pfa);             ///< memory pointer to pfa 
             *(p+1) = IP;                            /// * encode current IP, and bail
             UNNEST());
        CASE(FOR,  rs.push(POP()));                  /// * setup FOR..NEXT call frame
        CASE(DO,                                     /// * setup DO..LOOP call frame
             rs.push(SS.pop()); rs.push(POP()));
        CASE(KEY,  key(); vm.state = IO);            /// * fetch single keypress
        OTHER(
            if (ix & EXT_FLAG) {                     /// * colon word?
                rs.push(IP);                         /// * setup call frame
                IP = ix & ~EXT_FLAG;                 /// * IP = word.pfa
            }
            else Code::exec(vm, ix));               ///> execute built-in word
        }
//        printf("   => IP=%4x, rs.idx=%d, VM=%d\n", IP, rs.idx, vm.state);
    }
}
///
///> CALL - inner-interpreter proxy (inline macro does not run faster)
///
void CALL(VM& vm, IU w) {
    if (IS_UDF(w)) {                   /// colon word
        rs.push(DU0);                  /// * terminating IP
        IP = dict[w].pfa;              /// setup task context
        nest(vm);
    }
    else dict[w].call(vm);               /// built-in word
}
///====================================================================
///
///> eForth dictionary assembler
///  Note: sequenced by enum forth_opcode as following
///
void dict_compile() {  ///< compile built-in words into dictionary
    CODE("nul ",    {});               /// dict[0], not used, simplify find()
    ///
    /// @defgroup Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",     PUSH(TOS));
    CODE("drop",    TOS = SS.pop());
    CODE("over",    PUSH(SS[-1]));
    CODE("swap",    DU n = SS.pop(); PUSH(n));
    CODE("rot",     DU n = SS.pop(); DU m = SS.pop(); SS.push(n); PUSH(m));
    CODE("-rot",    DU n = SS.pop(); DU m = SS.pop(); PUSH(m); PUSH(n));
    CODE("nip",     SS.pop());
    CODE("pick",    DU i = TOS; TOS = SS[-i]);
    /// @}
    /// @defgroup Stack ops - double
    /// @{
    CODE("2dup",    PUSH(SS[-1]); PUSH(SS[-1]));
    CODE("2drop",   SS.pop(); TOS = SS.pop());
    CODE("2over",   PUSH(SS[-3]); PUSH(SS[-3]));
    CODE("2swap",   DU n = SS.pop(); DU m = SS.pop(); DU l = SS.pop();
                    SS.push(n); PUSH(l); PUSH(m));
    CODE("?dup",    if (TOS != DU0) PUSH(TOS));
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",       TOS += SS.pop());
    CODE("*",       TOS *= SS.pop());
    CODE("-",       TOS =  SS.pop() - TOS);
    CODE("/",       TOS =  SS.pop() / TOS);
    CODE("mod",     TOS =  MOD(SS.pop(), TOS));
    CODE("*/",      TOS =  (DU2)SS.pop() * SS.pop() / TOS);
    CODE("/mod",    DU  n = SS.pop();
                    DU  t = TOS;
                    DU  m = MOD(n, t);
                    SS.push(m); TOS = UINT(n / t));
    CODE("*/mod",   DU2 n = (DU2)SS.pop() * SS.pop();
                    DU2 t = TOS;
                    DU  m = MOD(n, t);
                    SS.push(m); TOS = UINT(n / t));
    CODE("and",     TOS = UINT(TOS) & UINT(SS.pop()));
    CODE("or",      TOS = UINT(TOS) | UINT(SS.pop()));
    CODE("xor",     TOS = UINT(TOS) ^ UINT(SS.pop()));
    CODE("abs",     TOS = ABS(TOS));
    CODE("negate",  TOS = -TOS);
    CODE("invert",  TOS = ~UINT(TOS));
    CODE("rshift",  TOS = UINT(SS.pop()) >> UINT(TOS));
    CODE("lshift",  TOS = UINT(SS.pop()) << UINT(TOS));
    CODE("max",     DU n=SS.pop(); TOS = (TOS>n) ? TOS : n);
    CODE("min",     DU n=SS.pop(); TOS = (TOS<n) ? TOS : n);
    CODE("2*",      TOS *= 2);
    CODE("2/",      TOS /= 2);
    CODE("1+",      TOS += 1);
    CODE("1-",      TOS -= 1);
#if USE_FLOAT
    CODE("int",     TOS = UINT(TOS));         // float => integer
#endif // USE_FLOAT
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0=",      TOS = BOOL(ZEQ(TOS)));
    CODE("0<",      TOS = BOOL(LT(TOS, DU0)));
    CODE("0>",      TOS = BOOL(GT(TOS, DU0)));
    CODE("=",       TOS = BOOL(EQ(SS.pop(), TOS)));
    CODE(">",       TOS = BOOL(GT(SS.pop(), TOS)));
    CODE("<",       TOS = BOOL(LT(SS.pop(), TOS)));
    CODE("<>",      TOS = BOOL(!EQ(SS.pop(), TOS)));
    CODE(">=",      TOS = BOOL(!LT(SS.pop(), TOS)));
    CODE("<=",      TOS = BOOL(!GT(SS.pop(), TOS)));
    CODE("u<",      TOS = BOOL(UINT(SS.pop()) < UINT(TOS)));
    CODE("u>",      TOS = BOOL(UINT(SS.pop()) > UINT(TOS)));
    /// @}
    /// @defgroup IO ops
    /// @{
    CODE("base",    PUSH(((U8*)vm.base - MEM0)));
    CODE("decimal", put(BASE, 10));
    CODE("hex",     put(BASE, 16));
    CODE("bl",      put(BL));
    CODE("cr",      put(CR));
    CODE(".",       put(DOT, POP()));
    CODE("u.",      put(DOT, UINT(POP())));
    CODE(".r",      IU w = UINT(POP()); put(DOTR, w, POP()));
    CODE("u.r",     IU w = UINT(POP()); put(DOTR, w, UINT(POP())));
    CODE("type",    POP(); pstr((const char*)MEM(POP())));     // pass string pointer
    IMMD("key",     if (vm.compile) add_w(KEY); else key());
    CODE("emit",    put(EMIT, POP()));
    CODE("space",   put(SPCS, DU1));
    CODE("spaces",  put(SPCS, POP()));
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("[",       vm.compile = false);
    CODE("]",       vm.compile = true);
    IMMD("(",       scan(')'));
    IMMD(".(",      pstr(scan(')')));
    IMMD("\\",      scan('\n'));
    IMMD("s\"",     s_quote(STR));
    IMMD(".\"",     s_quote(DOTQ));
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
    /// @defgrouop FOR...NEXT loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for" ,    add_w(FOR); PUSH(HERE));                    // for ( -- here )
    IMMD("next",    add_w(NEXT); add_iu(POP()));                // next ( here -- )
    IMMD("aft",                                                 // aft ( here -- here there )
         POP(); add_w(BRAN);
         IU h=HERE; add_iu(0); PUSH(HERE); PUSH(h));
    /// @}
    /// @}
    /// @defgrouop DO..LOOP loops
    /// @{
    IMMD("do" ,     add_w(DO); PUSH(HERE));                     // for ( -- here )
    CODE("i",       PUSH(rs[-1]));
    CODE("leave",   rs.pop(); rs.pop(); UNNEST());              // quit DO..LOOP
    IMMD("loop",    add_w(LOOP); add_iu(POP()));                // next ( here -- )
    /// @}
    /// @defgrouop return stack ops
    /// @{
    CODE(">r",      rs.push(POP()));
    CODE("r>",      PUSH(rs.pop()));
    CODE("r@",      PUSH(rs[-1]));                              // same as I (the loop counter)
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE(":",       vm.compile = def_word(word()));
    IMMD(";",       add_w(EXIT); vm.compile = false);
    CODE("exit",    UNNEST());                                  // early exit the colon word
    CODE("variable",def_word(word()); add_var(VAR));            // create a variable
    CODE("constant",                                            // create a constant
         def_word(word());                                      // create a new word on dictionary
         add_var(LIT, POP());                                   // dovar (+parameter field)
         add_w(EXIT));
    IMMD("immediate", dict[-1].attr |= IMM_ATTR);
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   IU w = POP(); CALL(vm, w));                  // execute word
    CODE("create", def_word(word()); add_var(VBRAN));           // bran + offset field
    IMMD("does>",  add_w(DOES));
    IMMD("to",                                                  // alter the value of a constant, i.e. 3 to x
         IU w = vm.state==QUERY ? find(word()) : POP();         // constant addr
         if (!w) return;
         if (vm.compile) {
             add_var(LIT, (DU)w);                               // save addr on stack
             add_w(find("to"));                                 // encode to opcode
         }
         else {
             w = dict[w].pfa + sizeof(IU);                      // calculate address to memory
             *(DU*)MEM(DALIGN(w)) = POP();                      // update constant
         });
    IMMD("is",              // ' y is x                         // alias a word, i.e. ' y is x
         IU w = vm.state==QUERY ? find(word()) : POP();         // word addr
         if (!w) return;
         if (vm.compile) {
             add_var(LIT, (DU)w);                               // save addr on stack
             add_w(find("is"));
         }
         else {
             dict[POP()].xt = dict[w].xt;
         });
    ///
    /// be careful with memory access, especially BYTE because
    /// it could make access misaligned which slows the access speed by 2x
    ///
    CODE("@",                                                   // w -- n
         IU w = UINT(POP());
         PUSH(w < USER_AREA ? (DU)IGET(w) : CELL(w)));          // check user area
    CODE("!",     IU w = UINT(POP()); CELL(w) = POP(););        // n w --
    CODE(",",     DU n = POP(); add_du(n));                     // n -- , compile a cell
    CODE("cells", IU i = UINT(POP()); PUSH(i * sizeof(DU)));    // n -- n'
    CODE("allot",                                               // n --
         IU n = UINT(POP());                                    // number of bytes
         for (int i = 0; i < n; i+=sizeof(DU)) add_du(DU0));    // zero padding
    CODE("th",    IU i = UINT(POP()); TOS += i * sizeof(DU));   // w i -- w'
    CODE("+!",    IU w = UINT(POP()); CELL(w) += POP());        // n w --
    CODE("?",     IU w = UINT(POP()); put(DOT, CELL(w)));       // w --
    /// @}
    /// @defgroup Debug ops
    /// @{
    CODE("abort", TOS = -DU1; SS.clear(); rs.clear());          // clear ss, rs
    CODE("here",  PUSH(HERE));
    CODE("'",     IU w = find(word()); if (w) PUSH(w));
    CODE(".s",    ss_dump(true));
    CODE("depth", PUSH(SS.idx));
    CODE("r",     PUSH(rs.idx));
    CODE("words", words());
    CODE("see",
         IU w = find(word()); if (!w) return;
         pstr(dict[w].name);
         if (IS_UDF(w)) see(dict[w].pfa);
         else           pstr(" ( built-ins ) ;");
         put(CR));
    CODE("dump",  U32 n = UINT(POP()); mem_dump(UINT(POP()), n));
    CODE("dict",  dict_dump());
    CODE("forget",
         IU w = find(word()); if (!w) return;                  // bail, if not found
         IU b = find("boot")+1;
         if (w > b) {                                          // clear to specified word
             pmem.clear(dict[w].pfa - STRLEN(dict[w].name));
             dict.clear(w);
         }
         else {                                                // clear to 'boot'
             pmem.clear(USER_AREA);
             dict.clear(b);
         }
    );
    /// @}
    /// @defgroup OS ops
    /// @{
    CODE("mstat", mem_stat());
    CODE("ms",    PUSH(millis()));
    CODE("rnd",   PUSH(RND()));             // generate random number
    CODE("delay", delay(UINT(POP())));
    CODE("included",                        // include external file
         POP();                             // string length, not used
         load((const char*)MEM(POP())));    // include external file
#if DO_WASM
    CODE("JS",    call_js());               // Javascript interface
#endif // DO_WASM
    CODE("bye",   exit(0));
    /// @}
    CODE("boot",  dict.clear(find("boot") + 1); pmem.clear(sizeof(DU)));
}
///
///> init base of xt pointer and xtoff range check
///
#if DO_WASM
UFP Code::XT0 = 0;       ///< WASM xt is vtable index (0 is min)
void dict_validate() {}  ///> no need to adjust xt offset base

#else // !DO_WASM
UFP Code::XT0 = ~0;      ///< init to max value

void dict_validate() {
    /// collect Code::XT0 i.e. xt base pointer
    UFP max = (UFP)0;
    for (int i=0; i < dict.idx; i++) {
        Code &c = dict[i];
        if ((UFP)c.xt < Code::XT0) Code::XT0 = (UFP)c.xt;
        if ((UFP)c.xt > max)       max       = (UFP)c.xt;
    }
    /// check xtoff range
    max -= Code::XT0;
    if (max & EXT_FLAG) {                   // range check
        LOG_KX("*** Init ERROR *** xtoff overflow max = 0x", max);
        LOGS("\nEnter 'dict' to verify, and please contact author!\n");
    }
}
#endif // DO_WASM
///====================================================================
///
///> ForthVM - Outer interpreter
///
DU parse_number(const char *idiom, int base, int *err) {
    switch (*idiom) {                        ///> base override
    case '%': base = 2;  idiom++; break;
    case '&':
    case '#': base = 10; idiom++; break;
    case '$': base = 16; idiom++; break;
    }
    char *p;
    *err = errno = 0;
#if USE_FLOAT
    DU n = (b==10)
        ? static_cast<DU>(strtof(idiom, &p))
        : static_cast<DU>(strtol(idiom, &p, base));
#else  // !USE_FLOAT
    DU n = static_cast<DU>(strtol(idiom, &p, base));
#endif // USE_FLOAT
    if (errno || *p != '\0') *err = 1;
    return n;
}

void forth_core(VM& vm, const char *idiom) {     ///> aka QUERY
    vm.state = QUERY;
    IU w = find(idiom);                  ///> * get token by searching through dict
    if (w) {                             ///> * word found?
        if (vm.compile && !IS_IMM(w)) {  /// * in compile mode?
            add_w(w);                    /// * add to colon word
        }
        else CALL(vm, w);                    /// * execute forth word
        return;
    }
    // try as a number
    int err  = 0;
    int base = static_cast<int>(*vm.base);
    DU  n    = parse_number(idiom, base, &err);
    if (err) {                           /// * not number
        pstr(idiom); pstr("? ", CR);     ///> display error prompt
        vm.compile = false;              ///> reset to interpreter mode
        vm.state   = STOP;               ///> skip the entire input buffer
    }
    // is a number
    if (vm.compile) {                    /// * a number in compile mode?
        add_var(LIT, n);                 ///> add to current word
    }
    else PUSH(n);                        ///> or, add value onto data stack
}
///====================================================================
///
/// Forth VM external command processor
///
VM& vm_instance(int id) {
    static VM vm;
    return vm;
}
void forth_init() {
    static bool init = false;
    if (init) return;                    ///> check dictionary initilized
    VM& vm = vm_instance();

    vm.base = &IGET(HERE);               ///< set pointer to base
    add_iu(10);                          ///< allocate space for base
    vm.dflt = &IGET(HERE);               ///< set pointer to dfmt
    add_iu(USE_FLOAT);
    
    for (int i=pmem.idx; i<USER_AREA; i+=sizeof(IU)) {
        add_iu(0xffff);                  /// * padding user area
    }
    dict_compile();                      ///> compile dictionary
    dict_validate();                     ///< collect XT0, and check xtoff range
}
int forth_vm(const char *line, void(*hook)(int, const char*)) {
    auto time_up = []() {                /// * time slice up
        static long t0 = 0;              /// * real-time support, 10ms = 100Hz
        long t1 = millis();              ///> check timing
        return (t1 >= t0) ? (t0 = t1 + t0, 1) : 0;
    };
    VM& vm = vm_instance();
    fout_setup(hook);

    bool resume =                        ///< check VM resume status
        (vm.state==HOLD || vm.state==IO);
    if (resume) IP = UINT(rs.pop());     /// * restore context
    else        fin_setup(line);         /// * refresh buffer if not resuming
    
    string idiom;
    while (resume || fetch(idiom)) {     /// * parse a word
        if (resume) nest(vm);                      /// * resume task
        else        forth_core(vm, idiom.c_str()); /// * send to Forth core
        resume = vm.state==HOLD;
        if (resume && time_up()) break;          ///> multi-threading support
    }
    bool yield = vm.state==HOLD || vm.state==IO; /// * yield to other tasks
    
    if (yield)            rs.push(IP);   /// * save context
    else if (!vm.compile) ss_dump();     /// * optionally display stack contents
    
    return yield;
}
