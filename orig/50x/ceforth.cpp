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
List<Code*, E4_DICT_SZ> dict;      ///< dictionary
List<U8,    E4_PMEM_SZ> pmem;      ///< parameter memory (for colon definitions)
U8  *MEM0;                         ///< base of parameter memory block
///
///> Macros to abstract dict and pmem physical implementation
///  Note:
///    so we can change pmem implementation anytime without affecting opcodes defined below
///
///@name Dictionary and data stack access macros
///@{
#define TOS       (vm.tos)                 /**< Top of stack                            */
#define SS        (vm.ss)                  /**< parameter stack (per task)              */
#define IP        (vm.ip)                  /**< instruction pointer (per task)          */
#define RS        (vm.rs)                  /**< return stack (per task)                 */
#define BOOL(f)   ((f)?-1:0)               /**< Forth boolean representation            */
#define HERE      (pmem.idx)               /**< current parameter memory index          */
#define MEM(a)    (MEM0 + (IU)UINT(a))     /**< pointer to address fetched from pmem    */
#define BASE      (MEM(vm.base))           /**< pointer to base in VM user area         */
#define IGET(ip)  (*(IU*)MEM(ip))          /**< instruction fetch from pmem+ip offset   */
#define CELL(a)   (*(DU*)&pmem[a])         /**< fetch a cell from parameter memory      */
#define SETJMP(a) (*(IU*)&pmem[a] = HERE)  /**< address offset for branching opcodes    */
///@}
///@name Primitive words (to simplify compiler), see nest() for details
///@{
Code prim[] = {
    Code(";",   EXIT), Code("nop",  NOP),   Code("next", NEXT),  Code("loop", LOOP),
    Code("lit", LIT),  Code("var",  VAR),   Code("str",  STR),   Code("dotq", DOTQ),
    Code("bran",BRAN), Code("0bran",ZBRAN), Code("vbran",VBRAN), Code("does>",DOES),
    Code("for", FOR),  Code("do",   DO),    Code("key",  KEY)
};
#define DICT(w) (IS_PRIM(w) ? &prim[w & ~EXT_FLAG] : dict[w])
///
///====================================================================
///@}
///@name Dictionary search functions - can be adapted for ROM+RAM
///@{
///
IU find(const char *s) {
    IU v = 0;
    for (IU i = dict.idx - 1; !v && i > 0; --i) {
        if (STRCMP(s, dict[i]->name)==0) v = i;
    }
#if CC_DEBUG > 1
    LOG_HDR("find", s); if (v) { LOG_DIC(v); } else LOG_NA();
#endif // CC_DEBUG > 1
    return v;
}
///====================================================================
///@}
///@name Colon word compiler
///@brief
///    * we separate dict and pmem space to make word uniform in size
///    * if they are combined then can behaves similar to classic Forth
///    * with an addition link field added.
///@{
void colon(const char *name) {
    char *nfa = (char*)&pmem[HERE]; ///> current pmem pointer
    int sz = STRLEN(name);          ///> string length, aligned
    pmem.push((U8*)name,  sz);      ///> setup raw name field

    Code *c = new Code(nfa, (FPTR)0, false);
    c->attr = UDF_ATTR;             ///> specify a colon (user defined) word
    c->pfa  = HERE;                 ///> capture code field index

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
    Code *c = DICT(w);              /// * code ref to primitive or dictionary entry
    IU   ip = (w & EXT_FLAG)        /// * is primitive?
        ? (UFP)c->xt                /// * get primitive/built-in token
        : (IS_UDF(w)                /// * colon word?
           ? (c->pfa | EXT_FLAG)    /// * pfa with colon word flag
           : c->xtoff());           /// * XT offset of built-in
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
#define POPI()  (UINT(POP()))

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
void s_quote(VM &vm, prim_op op) {
    const char *s = scan('"')+1;    ///> string skip first blank
    if (vm.compile) {
        add_w(op);                  ///> dostr, (+parameter field)
        add_str(s);                 ///> byte0, byte1, byte2, ..., byteN
    }
    else {                          ///> use PAD ad TEMP storage
        IU h0  = HERE;              ///> keep current memory addr
        DU len = add_str(s);        ///> write string to PAD
        switch (op) {
        case STR:  PUSH(h0); PUSH(len);        break; ///> addr, len
        case DOTQ: pstr((const char*)MEM(h0)); break; ///> to console
        default:   pstr("s_quote unknown op:");
        }
        HERE = h0;                  ///> restore memory addr
    }
}
///@}
///====================================================================
///
///> Forth inner interpreter (handles a colon word)
///  Note: on performance
///  1. C call/return carry stackframe overhead vs NEXT threading in assembly
///  2. Use of IP=0 for depth control, instead of WP by Dr. Ting,
///     speeds up 8% vs recursive calls.
///  3. Computed-goto entire dict runs 15% faster, but
///     needs long macros (for enum) and extra memory.
///     3.1 Use of just one cached _NXT address for loop speeds up 10% on AMD but
///         5% slower on ESP32. Probably due to shallow pipeline.
///     3.2 Elect 16 primitive opcodes for nest() switch dispatch speeds up 15%.
///         About 60% total time spent in nest() loop, now.
///     3.3 Computed-goto 16 elected opcode slows about 2% (lost the gain of 3.2).
///  4. Use local stack speeds up 10%, but needs allot 4*64 bytes extra
///  5. Extra vm& passing for multitasking performs about the same. x86 uses EAX.
///  6. 32-bit Param struct simplify bit masking.
///     6.1. However, nesting 32-bit is 25% slower than the 16-bit version.
///          Hotspot on Param* fetch. (Ir/Dr 6/3=>24/9 with valgrind).
///          Other being about the same.
///     6.2  benchmark 32-bit nest() on Param ix <= MEM(IP), by valgrind/cachegrind
///          * 32-bit Param hardcopy  Ir/Dr = 3.8M/1.1M (930ms)
///          * 32-bit Param pointer   Ir/Dr = 3.2M/0.9M (899ms)
///          * 32-bit Param ref       Ir/Dr = 3.1M/0.8M (843ms)
///
#define DISPATCH(op) switch(op)
#define CASE(op, g)  case op : { g; } break
#define OTHER(g)     default : { g; } break
#define UNNEST()     (IP=UINT(RS.pop()))

void nest(VM& vm) {
    vm.state = NEST;                                 /// * activate VM
    while (IP) {
        IU ix = IGET(IP);                            ///< fetched opcode, hopefully in register
//        VM_HDR(&vm, ":%4x", ix);
        IP += sizeof(IU);
        DISPATCH(ix) {                               /// * opcode dispatcher
        CASE(EXIT, UNNEST());
        CASE(NOP,  { /* do nothing */});
        CASE(NEXT,
             if (GT(RS[-1] -= DU1, -DU1)) {          ///> loop done?
                 IP = IGET(IP);                      /// * no, loop back
             }
             else {                                  /// * yes, loop done!
                 RS.pop();                           /// * pop off loop counter
                 IP += sizeof(IU);                   /// * next instr.
             });
        CASE(LOOP,
             if (GT(RS[-2], RS[-1] += DU1)) {        ///> loop done?
                 IP = IGET(IP);                      /// * no, loop back
             }
             else {                                  /// * yes, done
                 RS.pop(); RS.pop();                 /// * pop off counters
                 IP += sizeof(IU);                   /// * next instr.
             });
        CASE(LIT,
             SS.push(TOS);
             IP  = DALIGN(IP);                       /// * 32-bit data align (WASM only)
             TOS = *(DU*)MEM(IP);                    ///> from hot cache, hopefully
             IP += sizeof(DU));                      /// * hop over the stored value
        CASE(VAR, PUSH(DALIGN(IP)); UNNEST());       ///> get var addr
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
             PUSH(DALIGN(IP + sizeof(IU)));          /// * put param addr on tos
             if ((IP = IGET(IP))==0) UNNEST());      /// * jump target of does> if given
        CASE(DOES,
             IU *p = (IU*)MEM(dict[-1]->pfa);        ///< memory pointer to pfa 
             *(p+1) = IP;                            /// * encode current IP, and bail
             UNNEST());
        CASE(FOR,  RS.push(POP()));                  /// * setup FOR..NEXT call frame
        CASE(DO,                                     /// * setup DO..LOOP call frame
             RS.push(SS.pop()); RS.push(POP()));
        CASE(KEY,  PUSH(key()); UNNEST());           /// * fetch single keypress
        OTHER(
            if (ix & EXT_FLAG) {                     /// * colon word?
                RS.push(IP);                         /// * setup call frame
                IP = ix & ~EXT_FLAG;                 /// * IP = word.pfa
            }
            else Code::exec(vm, ix));               ///> execute built-in word
        }
//        VM_TLR(&vm, " => SS=%d, RS=%d, IP=%x", SS.idx, RS.idx, IP);
    }
}
///
///> CALL - inner-interpreter proxy (inline macro does not run faster)
///
void CALL(VM& vm, IU w) {
    if (IS_UDF(w)) {                   /// colon word
        RS.push(IP);                   /// * terminating IP
        IP = dict[w]->pfa;             /// setup task context
        nest(vm);
    }
    else dict[w]->call(vm);            /// built-in word
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
    CODE("over",    DU v = SS[-1]; PUSH(v));
    CODE("swap",    DU n = SS.pop(); PUSH(n));
    CODE("rot",     DU n = SS.pop(); DU m = SS.pop(); SS.push(n); PUSH(m));
    CODE("-rot",    DU n = SS.pop(); DU m = SS.pop(); PUSH(m); PUSH(n));
    CODE("pick",    IU i = UINT(TOS); TOS = SS[-i]);
    CODE("nip",     SS.pop());
    CODE("?dup",    if (TOS != DU0) PUSH(TOS));
    /// @}
    /// @defgroup Stack ops - double
    /// @{
    CODE("2dup",    DU v = SS[-1]; PUSH(v); v = SS[-1]; PUSH(v));
    CODE("2drop",   SS.pop(); TOS = SS.pop());
    CODE("2over",   DU v = SS[-3]; PUSH(v); v = SS[-3]; PUSH(v));
    CODE("2swap",   DU n = SS.pop(); DU m = SS.pop(); DU l = SS.pop();
                    SS.push(n); PUSH(l); PUSH(m));
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",       TOS += SS.pop());
    CODE("*",       TOS *= SS.pop());
    CODE("-",       TOS =  SS.pop() - TOS);
    CODE("/",       TOS =  SS.pop() / TOS);
    CODE("mod",     TOS =  INT(MOD(SS.pop(), TOS)));           /// ( a b -- c ) c integer, see fmod
    CODE("*/",      TOS =  (DU2)SS.pop() * SS.pop() / TOS);    /// ( a b c -- d ) d=a*b / c (float)
    CODE("/mod",    DU  n = SS.pop();                          /// ( a b -- c d ) c=a%b, d=int(a/b)
                    DU  t = TOS;
                    DU  m = MOD(n, t);
                    SS.push(m); TOS = INT(n / t));
    CODE("*/mod",   DU2 n = (DU2)SS.pop() * SS.pop();          /// ( a b c -- d e ) d=(a*b)%c, e=(a*b)/c
                    DU2 t = TOS;
                    DU  m = MOD(n, t);
                    SS.push(m); TOS = INT(n / t));
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
    CODE("fmod",    TOS = MOD(SS.pop(), TOS));                /// -3.5 2 fmod => -1.5
    CODE("f>s",     TOS = INT(TOS));                          /// 1.9 => 1, -1.9 => -1
#else
    CODE("f>s",     /* do nothing */);
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
    CODE("base",    PUSH(vm.base));
    CODE("decimal", dot(RDX, *BASE=10));
    CODE("hex",     dot(RDX, *BASE=16));
    CODE("bl",      PUSH(0x20));
    CODE("cr",      dot(CR));
    CODE(".",       dot(DOT,  POP()));
    CODE("u.",      dot(UDOT, POP()));
    CODE(".r",      IU w = POPI(); dotr(w, POP(), *BASE));
    CODE("u.r",     IU w = POPI(); dotr(w, POP(), *BASE, true));
    CODE("type",    POP(); pstr((const char*)MEM(POP())));   /// pass string pointer
    IMMD("key",     if (vm.compile) add_w(KEY); else PUSH(key()));
    CODE("emit",    dot(EMIT, POP()));
    CODE("space",   dot(SPCS, DU1));
    CODE("spaces",  dot(SPCS, POP()));
    /// @}
    /// @defgroup Literal ops
    /// @{
    IMMD("(",       scan(')'));
    IMMD(".(",      pstr(scan(')')));
    IMMD("\\",      scan('\n'));
    IMMD("s\"",     s_quote(vm, STR));
    IMMD(".\"",     s_quote(vm, DOTQ));
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    IMMD("if",      add_w(ZBRAN); PUSH(HERE); add_iu(0));    /// if    ( -- here )
    IMMD("else",                                             /// else ( here -- there )
         add_w(BRAN);
         IU h=HERE; add_iu(0); SETJMP(POP()); PUSH(h));
    IMMD("then",    SETJMP(POP()));                          /// backfill jump address
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",   PUSH(HERE));
    IMMD("again",   add_w(BRAN);  add_iu(POP()));            /// again    ( there -- )
    IMMD("until",   add_w(ZBRAN); add_iu(POP()));            /// until    ( there -- )
    IMMD("while",   add_w(ZBRAN); PUSH(HERE); add_iu(0));    /// while    ( there -- there here )
    IMMD("repeat",  add_w(BRAN);                             /// repeat    ( there1 there2 -- )
         IU t=POP(); add_iu(POP()); SETJMP(t));              /// set forward and loop back address
    /// @}
    /// @defgrouop FOR...NEXT loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for" ,    add_w(FOR); PUSH(HERE));                 /// for ( -- here )
    IMMD("next",    add_w(NEXT); add_iu(POP()));             /// next ( here -- )
    IMMD("aft",                                              /// aft ( here -- here there )
         POP(); add_w(BRAN);
         IU h=HERE; add_iu(0); PUSH(HERE); PUSH(h));
    /// @}
    /// @}
    /// @defgrouop DO..LOOP loops
    /// @{
    IMMD("do" ,     add_w(DO); PUSH(HERE));                  /// for ( -- here )
    CODE("i",       PUSH(RS[-1]));
    CODE("leave",   RS.pop(); RS.pop(); UNNEST());           /// quit DO..LOOP
    IMMD("loop",    add_w(LOOP); add_iu(POP()));             /// next ( here -- )
    /// @}
    /// @defgrouop return stack ops
    /// @{
    CODE(">r",      RS.push(POP()));
    CODE("r>",      PUSH(RS.pop()));
    CODE("r@",      PUSH(RS[-1]));                           /// same as I (the loop counter)
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("[",       vm.compile = false);
    CODE("]",       vm.compile = true);
    CODE(":",       vm.compile = def_word(word()));
    IMMD(";",       add_w(EXIT); vm.compile = false);
    CODE("variable",def_word(word()); add_var(VAR));         /// create a variable
    CODE("constant",                                         /// create a constant
         def_word(word());                                   /// create a new word on dictionary
         add_var(LIT, POP());                                /// dovar (+parameter field)
         add_w(EXIT));
    IMMD("postpone",  IU w = find(word()); if (w) add_w(w));
    CODE("immediate", dict[-1]->attr |= IMM_ATTR);
    CODE("exit",    UNNEST());                               /// early exit the colon word
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   IU w = POP(); CALL(vm, w));               /// execute word
    CODE("create", def_word(word()); add_var(VBRAN));        /// bran + offset field
    IMMD("does>",  add_w(DOES));
    IMMD("to",                                               /// alter the value of a constant, i.e. 3 to x
         IU w = vm.state==QUERY ? find(word()) : POP();      /// constant addr
         if (!w) return;
         if (vm.compile) {
             add_var(LIT, (DU)w);                            /// save addr on stack
             add_w(find("to"));                              /// encode to opcode
         }
         else {
             w = dict[w]->pfa + sizeof(IU);                  /// calculate address to memory
             *(DU*)MEM(DALIGN(w)) = POP();                   /// update constant
         });
    IMMD("is",              /// ' y is x                     /// alias a word, i.e. ' y is x
         IU w = vm.state==QUERY ? find(word()) : POP();      /// word addr
         if (!w) return;
         if (vm.compile) {
             add_var(LIT, (DU)w);                            /// save addr on stack
             add_w(find("is"));
         }
         else {
             dict[POP()]->xt = dict[w]->xt;
         });
    ///
    /// be careful with memory access, especially BYTE because
    /// it could make access misaligned which slows the access speed by 2x
    ///
    CODE("@",                                                /// w -- n
         IU w = POPI();
         PUSH(w < USER_AREA ? (DU)IGET(w) : CELL(w)));       /// check user area
    CODE("!",     IU w = POPI(); CELL(w) = POP(););          /// n w --
    CODE("+!",    IU w = POPI(); CELL(w) += POP());          /// n w --
    CODE("?",     IU w = POPI(); dot(DOT, CELL(w)));         /// w --
    CODE(",",     DU n = POP(); add_du(n));                  /// n -- , compile a cell
    CODE("cells", IU i = POPI(); PUSH(i * sizeof(DU)));      /// n -- n'
    CODE("allot",                                            /// n --
         IU n = POPI();                                      /// number of bytes
         for (int i = 0; i < n; i+=sizeof(DU)) add_du(DU0)); /// zero padding
    CODE("th",    IU i = POPI(); TOS += i * sizeof(DU));     /// w i -- w'
    /// @}
#if DO_MULTITASK    
    /// @defgroup Multitasking ops
    /// @}
    CODE("task",                                             /// w -- task_id
         IU w = POPI();                                      ///< dictionary index
         if (IS_UDF(w)) PUSH(task_create(dict[w]->pfa));     /// create a task starting on pfa
         else pstr("  ?colon word only\n"));
    CODE("rank",  PUSH(vm.id));                              /// ( -- n ) thread id
    CODE("start", task_start(POPI()));                       /// ( task_id -- )
    CODE("join",  vm.join(POPI()));                          /// ( task_id -- )
    CODE("lock",  vm.io_lock());                             /// wait for IO semaphore
    CODE("unlock",vm.io_unlock());                           /// release IO semaphore
    CODE("send",  IU t = POPI(); vm.send(t, POPI()));        /// ( v1 v2 .. vn n tid -- ) pass values onto task's stack
    CODE("recv",  vm.recv());                                /// ( -- v1 v2 .. vn ) waiting for values passed by sender
    CODE("bcast", vm.bcast(POPI()));                         /// ( v1 v2 .. vn -- )
    CODE("pull",  IU t = POPI(); vm.pull(t, POPI()));        /// ( tid n -- v1 v2 .. vn )
    /// @}
#endif // DO_MULTITASK    
    /// @defgroup Debug ops
    /// @{
    CODE("abort", TOS = -DU1; SS.clear(); RS.clear());       /// clear ss, rs
    CODE("here",  PUSH(HERE));
    IMMD("'",     IU w = find(word()); if (w) PUSH(w));
    CODE(".s",    ss_dump(vm, true));
    CODE("words", words(*BASE));
    CODE("see",
         IU w = find(word()); if (!w) return;
         pstr(": "); pstr(dict[w]->name);
         if (IS_UDF(w)) see(dict[w]->pfa, *BASE);
         else           pstr(" ( built-ins ) ;");
         dot(CR));
    CODE("depth", IU i = UINT(SS.idx); PUSH(i));
    CODE("r",     PUSH(RS.idx));
    CODE("dump",
         U32 n = POPI();
         mem_dump(POPI(), n, *BASE));
    CODE("dict",  dict_dump(*BASE));
    CODE("forget",
         IU w = find(word()); if (!w) return;               /// bail, if not found
         IU b = find("boot")+1;
         if (w > b) {                                       /// clear to specified word
             pmem.clear(dict[w]->pfa - STRLEN(dict[w]->name));
             dict.clear(w);
         }
         else {                                             /// clear to 'boot'
             pmem.clear(USER_AREA);
             dict.clear(b);
         }
    );
    /// @}
    /// @defgroup OS ops
    /// @{
    IMMD("include", load(vm, word()));                      /// include an OS file
    CODE("included",                                        /// include file spec on stack
         POP();                                             /// string length, not used
         load(vm, (const char*)MEM(POP())));                /// include external file
    CODE("ok",    mem_stat());
    CODE("clock", PUSH(millis()));
    CODE("rnd",   PUSH(RND()));                             /// generate random number
    CODE("ms",    delay(POPI()));
#if DO_WASM
    CODE("JS",    native_api(vm));                          /// Javascript interface
#else    
    CODE("bye",   vm.state=STOP);
#endif // DO_WASM
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
        Code *c = dict[i];
        if ((UFP)c->xt < Code::XT0) Code::XT0 = (UFP)c->xt;
        if ((UFP)c->xt > max)       max       = (UFP)c->xt;
    }
    /// check xtoff range
    max -= Code::XT0;
    if (max & EXT_FLAG) {                     /// range check
        LOG_KX("*** Init ERROR *** xtoff overflow max = 0x", max);
        LOGS("\nEnter 'dict' to verify, and please contact author!\n");
    }
}
#endif // DO_WASM
///====================================================================
///
///> ForthVM - Outer interpreter
///
DU2 parse_number(const char *idiom, int base, int *err) {
    switch (*idiom) {                        ///> base override
    case '%': base = 2;  idiom++; break;
    case '&':
    case '#': base = 10; idiom++; break;
    case '$': base = 16; idiom++; break;
    }
    char *p;
    *err = errno = 0;
#if USE_FLOAT
    DU2 n = (base==10)
        ? static_cast<DU2>(strtod(idiom, &p))
        : static_cast<DU2>(strtoll(idiom, &p, base));
#else  // !USE_FLOAT
    DU2 n = static_cast<DU2>(strtoll(idiom, &p, base));
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
        else { IP = DU0; CALL(vm, w); }  /// * execute forth word
        return;
    }
    /// try as a number
    int err = 0;
    DU  n   = parse_number(idiom, *BASE, &err);
    if (err) {                           /// * not number
        pstr(idiom); pstr("? ", CR);     ///> display error prompt
        pstr(strerror(err), CR);         ///> and error description
        vm.compile = false;              ///> reset to interpreter mode
        vm.state   = STOP;               ///> skip the entire input buffer
    }
    /// is a number
    if (vm.compile) {                    /// * a number in compile mode?
        add_var(LIT, n);                 ///> add to current word
    }
    else PUSH(n);                        ///> or, add value onto data stack
}
///====================================================================
///
/// Forth VM external command processor
///
void forth_init() {
    static bool init    = false;
    if (init) return;                    ///> check dictionary initilized

    if (!dict.v || !pmem.v) {
        LOGS("forth_init memory allocation failed, bail...\n");
        exit(0);
    }
    MEM0 = &pmem[0];

    uvar_init();                         /// * initialize user area
    t_pool_init();                       /// * initialize thread pool
    VM &vm0   = vm_get(0);               /// * initialize main vm
    vm0.state = QUERY;

    for (int i = pmem.idx; i < USER_AREA; i+=sizeof(IU)) {
        add_iu(0xffff);                  /// * reserved user area
    }
    dict_compile();                      ///> compile dictionary
    dict_validate();                     ///< collect XT0, and check xtoff range
}

void forth_teardown() {
    t_pool_stop();
}

int forth_vm(const char *line, void(*hook)(int, const char*)) {
    VM &vm = vm_get(0);                                     ///< get main thread
    fout_setup(hook);
    fin_setup(line);                                        /// * refresh buffer if not resuming
    
    string idiom;
    while (fetch(idiom)) {                                  /// * parse a word
        forth_core(vm, idiom.c_str());                      /// * outer interpreter
    }
    if (!vm.compile) ss_dump(vm);
    
    return vm.state==STOP;
}
