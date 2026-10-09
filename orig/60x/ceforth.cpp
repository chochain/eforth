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
#define SS        (vm.ss.v)
#define RS        (vm.rs.v)
#define SP        (vm.ss.idx)
#define RP        (vm.rs.idx)
#define IP        (vm.ip)
#define TOS       (sp[-1])                 /**< top of stack (memory shadow slot)       */
#define NOS       (sp[-2])                 /**< next of stack                           */
#define OS3       (sp[-3])                 /**< 3rd of stack in memory                  */
#define OS4       (sp[-4])                 /**< 4th of stack in memory                  */
#define DEPTH     ((int)(sp - SS))         /**< number of items on stack                */
#if !E4_NOS
#define nos       (sp[-2])                 /**< TOS-only cache: NOS stays in memory     */
#endif // !E4_NOS
// Direct array-index tracking mappings for high-speed manipulation
#define HERE      (pmem.idx)               /**< current parameter memory index          */
#define HERE_PTR  ((IU*)&pmem[HERE])
#define HERE_TGT  (TOK(HERE_PTR))          /**< token (pointer) of current pmem position */
#if XT0_U32
#define XT(i)     ((IU*)(UFP)(IU)(i))
#else  // !XT0_U32
#define XT(i)     ((IU*)(Code::XT0 | (UFP)(IU)(i)))
#endif // XT0_U32
#define TOK(p)    ((IU)Code::Token((void*)(p)))
#define MEM(a)    ((U8*)XT(UINT(a)))       /**< pointer to address fetched from pmem    */
#define BASE      (MEM0 + vm.base)         /**< pointer to base in VM user area         */
#define IGET(ip)  (*(IU*)MEM(ip))          /**< instruction fetch from pmem+ip offset   */
#define CELL(a)   (*(DU*)XT(a))            /**< fetch a cell from parameter memory      */
#define JMP()     ip = XT(*ip)             /**< set IP to target address                */
#define SETJMP(a) (*(IU*)&pmem[a] = HERE)  /**< address offset for branching opcodes    */
#define SCAN(c)   (scan(c, vm.pad, E4_PAD_SZ))
#define WORD()    (word(vm.pad, E4_PAD_SZ))
///@}
///====================================================================
///@name Colon word compiler
///@brief
///    * we separate dict and pmem space to make word uniform in size
///    * if they are combined then can behaves similar to classic Forth
///    * with an addition link field added.
///@{
void add_iu(IU i) { pmem.push((U8*)&i, sizeof(IU)); }  ///< add an instruction into pmem
void add_du(DU v) { pmem.push((U8*)&v, sizeof(DU)); }  ///< add a cell into pmem
void add_w(const Code *c) {
    DEBUG("add_w(%08zx) => %08zx << %08x:%s\n", (UFP)c, (UFP)HERE_PTR, TOK(c->xt), c->name);
    // Compile the target body memory address pointer as a 32-bit data payload block
    if (c->is_udf()) add_xt("_:");   /// doLIST
    add_iu(TOK(c->xt));
}
int  add_str(const char *s) {        ///< add a string to pmem
    U16 len = (U16)strlen(s);
    int bsz = sizeof(U16) + len + 1; ///< 16-bit len + string + '\0'
    int asz = ALIGN(bsz);            ///> string length, aligned

    pmem.push((U8*)&len, sizeof(U16));
    pmem.push((U8*)s, len);          /// * add string

    for (int i = bsz-1; i < asz; i++) pmem.push((U8)0);  /// * '\0' padding
    return asz;
}
void add_xt(const char *name) {
    const Code *w = find(name);
    add_w(w);
}
void colon(const char *name) {
    char *nfa = (char*)&pmem[HERE];  ///> current pmem pointer
    int  sz   = strlen(name) + 1;
    DEBUG("colon %08zx:%04x, sz=%d", (UFP)nfa, HERE, sz);
    pmem.push((U8*)name, ALIGN(sz));

    Code *c = new Code(nfa, (FPTR)HERE_PTR, (U8)UDF_ATTR);
    dict.push(c);                   ///> deep copy Code struct into dictionary
    DEBUG(" => %08zx:%04x dict.idx=%d '%s'\n", (UFP)HERE_PTR, HERE, dict.idx, nfa);
}
///@}
///@name Dictionary search functions - can be adapted for ROM+RAM
///@{
///
const Code *find(const char *s) {
    const Code *w = NULL;
    int i;
    for (i = dict.idx - 1; dict.idx && !w && i >= 0; --i) {
        if (STRCMP(s, dict[i]->name)==0) w = dict[i];
    }
    for (i = g_romsz - 1; !w && i > 0; --i) {
        if (STRCMP(s, g_rom[i].name)==0) w = &g_rom[i];
    }
    if (w) {
        DEBUG("find(%s) => %s[%d]:%08zx attr=%d %s\n",
            s, w->is_udf() ? "dict" : "g_rom", i, (UFP)w->xt, w->attr, w->name);
    }
    else DEBUG("find(%s) => %s\n", s, "N/A");
    return w;
}
///@}
///====================================================================
///
///> functions to reduce verbosity
///
#if E4_NOS
#define FILL()   ({ sp--; nos = NOS; })
#define PUSH(v)  ({ NOS = nos; nos = tos; tos = (DU)(v); sp++; })
#define SPILL()  ({ SP = sp - SS; TOS = tos; NOS = nos; }) /**< registers => memory  */
#define RELOAD() ({ sp = SS + SP; tos = TOS; nos = NOS; }) /**< memory => registers  */
#else  // E4_NOS
#define FILL()   ({ sp--; })
#define PUSH(v)  ({ DU _v = (DU)(v); TOS = tos; tos = _v; sp++; })
#define SPILL()  ({ SP = sp - SS; TOS = tos; })            /**< no NOS */
#define RELOAD() ({ sp = SS + SP; tos = TOS; })            /**< no nos */
#endif // E4_NOS

#define POP()    ({ DU _rst = tos; tos = nos; FILL(); _rst; })
#define POPI()   (UINT(POP()))
#define BOOL(f)  ((f)?-1:0)               /**< Forth boolean representation */
#define ALU(op)  ({ tos = nos op tos; FILL(); })
#define UALU(op) ({ tos = UINT(nos) op UINT(tos); FILL(); })
#define CMP(op)  ({ tos = BOOL(op(nos, tos)); FILL(); })
#define UCMP(op) ({ tos = BOOL(op(UINT(nos), UINT(tos))); FILL(); })

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
void s_quote(XT_REFS, bool dotq=false) {
    const char *s = SCAN('"')+1;    ///> string skip first blank
    if (vm.compile) {
        add_xt(dotq ? "_dotq" : "_str"); 
        add_str(s);                 ///> 16-bit len, byte0, byte1, byte2, ..., byteN, '\0'
    }
    else {                          ///> use PAD ad TEMP storage
        IU h0  = HERE;              ///> keep current memory addr
        DU len = add_str(s);        ///> write string to PAD
        char *str = (char*)&pmem[h0] + sizeof(U16);
        if (dotq) pstr(str, CR);
        else {
            PUSH((DU)TOK(str));     ///> addr, len
            PUSH(len);
        }
        HERE = h0;                  ///> restore memory addr
    }
}
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
XT_RET doSTOP(XT_ARGS) {
#if E4_TRAMP
    return Ret{ 0, ip, sp, tos };                          /// * fp==0 ends the trampoline
#else // E4_TRAMP
    // Commit current register window variables back to permanent storage on exit
    IP  = ip;
    SP  = sp - SS;
    TOS = tos;
#if E4_NOS
    NOS = nos;
#endif // E4_NOS
    return NULL;
#endif // E4_TRAMP
}
static IU gStop[] = { TOK(doSTOP) };  ///< tempoline sentinal

#define UNNEST()     {                       \
        if (RP <= 0) return doSTOP(XT_PASS); \
        ip = XT(RS[--RP]);                   \
        NEXT();                              \
    }

#if E4_TRAMP
static void run(VM &vm, Ret r) {      ///< trampoline: constant C stack, one call+return per word
    while (r.fp) r = r.fp(vm, r.ip, r.sp, r.tos);
    IP = r.ip; SP = r.sp - SS; r.sp[-1] = r.tos;     /// TOS shadow slot
}
#endif // E4_TRAMP

void nest(VM& vm) {                   ///< inner-interpreter i.e. doLIST, tail-call
    vm.state = NEST;
    
    /// Extract core virtual machine tracking metrics locally onto the local struct
    IU  *ip = IP;
    DU *sp  = SS + SP;
    DU  tos = TOS;                   /// Direct load. Unguarded garbage behavior if empty
#if E4_NOS    
    DU  nos = NOS;
#endif // E4_NOS

    DEBUG("\nXT0=%zx *IP=[%x,%x] ", Code::XT0, *ip, *(ip+1));
    DEBUG("nest(%08x) sp%d, rp%d, [%d, %d]\n",
          *ip, DEPTH, RP, DEPTH > 1 ? nos : 0, DEPTH > 0 ? tos : 0);

    FPTR fp = NEXT_FP;
#if E4_TRAMP
    run(vm, Ret{ fp, ip, sp, tos });
#else // E4_TRAMP
    fp(XT_PASS);                    /// * the whole word chain runs by tail calls; doSTOP returns
#endif // E4_TRAMP

    DEBUG("  %p: sp%d, rp%d, [%d, %d]\n",
          fp, SP, RP, SP > 1? NOS : 0, SP > 0 ? TOS : 0);
}

void CALL(VM &vm, const Code &c) {
    if (c.is_udf()) {
        RS[RP++] = TOK(gStop);
        IP = (IU*)c.xt;
        DEBUG("\n  CALL(%x): sp%d, rp%d [%d,%d] ",
              *vm.ip, SP, RP, SP > 1 ? NOS : 0, SP > 0 ? TOS : 0);
        nest(vm);
    }
    else {
        IU *ip  = gStop;
        DU *sp  = SS + SP;
        DU  tos = TOS;
#if E4_NOS
        DU  nos = NOS;
#endif // E4_NOS
        
#if E4_TRAMP
        run(vm, c.xt(vm, ip, sp, tos));
#else // E4_TRAMP
        c.xt(XT_PASS);
#endif // E4_TRAMP
    }
}

///====================================================================
///
///> eForth dictionary assembler
///  Note: sequenced by enum forth_opcode as following
///
///@name Built-in Dictionary (lambda-based, ROMable)
///@{
constexpr Code g_rom[] = {
    CODE("nop ",    {}),                             /// dict[0], not used, simplify find()
    CODE("_:",                                       ///< doLIST
         IU *w = XT(*ip++);                          /// * compiled as [doLIST][body ptr]
         RS[RP++] = (DU)TOK(ip);                     /// * return address
         ip = w),
    CODE("_;",      ip = XT(RS[--RP])),              ///< EXIT
    CODE("_lit",                                     ///< doconst
         DU v = *(DU*)ip;
         ip += sizeof(DU)/sizeof(IU);                /// * should DU/IU size different
         PUSH(v)),  /// doconst
    CODE("_var",    PUSH(TOK(ip)); UNNEST()),
    CODE("_str",
         U16 len = *(U16*)ip;                        /// 2-byte length
         char *str = (char*)ip + sizeof(U16);
         PUSH((DU)TOK(str));
         PUSH((DU)len);
         int bsz = sizeof(U16) + len + 1;            /// 16-bit len + string + '\0'
         ip = (IU*)((U8*)ip + ALIGN(bsz))),
    CODE("_dotq",
         U16 len = *(U16*)ip;
         char *str = (char*)ip + sizeof(U16);
         pstr(str, CR);
         int bsz = sizeof(U16) + len + 1;
         ip = (IU*)((U8*)ip + ALIGN(bsz))),
    CODE("_create", PUSH(TOK(++ip)); ip++),
    CODE("_does>",
         IU *t = (IU*)dict[-1]->xt;                  ///< memory pointer to pfa 
         *(t+1) = TOK(ip);                           /// * encode does> body token, and bail
         UNNEST()),
    CODE("_next",
         if (GT(RS[RP-1] -= DU1, -DU1)) JMP();       ///> loop done? no, loop back
         else { --RP; ip++; }),                      /// * yes, bail!
    CODE("_loop",
         if (GT(RS[RP-2], RS[RP-1] += DU1)) JMP();   ///> loop done? no, loop back
         else { --RP; --RP; ip++; }),                /// * pop off counters
    CODE("_bran", JMP()),                            ///< unconditional jmp
    CODE("_0bran",if (ZEQ(POP())) JMP(); else ip++), /// conditional jmp
    CODE("vbran",
         IU tgt = *ip;                               /// * does> target token (0 if none)
         PUSH(TOK(ip + 1));                          /// * put param addr on tos
         if (tgt) ip = XT(tgt); else UNNEST()),      /// * jump to does> body, or return
    CODE("_for", RS[RP++] = POP()),
    CODE("_do",                                      /// ( limit start -- )
         DU t = POP(); DU n = POP(); RS[RP++] = n; RS[RP++] = t),
    CODE("_key", PUSH(key()); UNNEST()),
    ///
    /// @defgroup Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",     PUSH(tos)),
    CODE("drop",    POP()),
    CODE("over",    DU v = nos; PUSH(v)),
    CODE("swap",    std::swap(tos, nos)),
    CODE("rot",     DU n = OS3; DU m = nos; OS3 = m; nos = tos; tos = n),
    CODE("-rot",    DU n = tos; tos = nos; nos = OS3; OS3 = n),
    CODE("pick",    int i = (int)UINT(tos); tos = i ? sp[-2 - i] : nos),
    CODE("nip",     nos = OS3; sp--),
    CODE("?dup",    if (!ZEQ(tos)) { DU v = tos; PUSH(v); }),
    /// @}
    /// @defgroup Stack ops - double
    /// @{
    CODE("2dup",    DU v1 = nos; DU v2 = tos; PUSH(v1); PUSH(v2)),
    CODE("2drop",   POP(); POP()),
    CODE("2over",   DU v1 = OS4; DU v2 = OS3; PUSH(v1); PUSH(v2)),
    CODE("2swap",
         DU v1 = tos; DU v2 = nos;
         tos = OS3; nos = OS4; OS3 = v1; OS4 = v2),
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",       ALU(+)),
    CODE("*",       ALU(*)),
    CODE("-",       ALU(-)),
    CODE("/",       ALU(/)),
    CODE("mod",     ALU(%)),
    CODE("*/",                              ///< ( a b c -- a*b/c )
         DU2 n = (DU2)nos * OS3; tos = (DU)(n / tos); sp -= 2; nos = NOS),
    CODE("/mod",
         DU n = nos; DU t = tos; DU m = MOD(n, t);
         nos = m; tos = INT(n / t)),
    CODE("*/mod",                           ///< ( a b c -- (a*b)%c )
         DU2 n = (DU2)OS3; n *= nos; DU2 t = tos; DU m = MOD(n, t); DU q = INT(n / t);
         sp--; nos = m; tos = q),                        /// sp-- first: nos may live in memory
    CODE("and",     UALU(&)),
    CODE("or",      UALU(|)),
    CODE("xor",     UALU(^)),
    CODE("rshift",  UALU(>>)),
    CODE("lshift",  UALU(<<)),
    CODE("max",     tos = std::max(nos, tos); FILL()),
    CODE("min",     tos = std::min(nos, tos); FILL()),
    CODE("abs",     tos = ABS(tos)),
    CODE("negate",  tos = -tos),
    CODE("invert",  tos = ~UINT(tos)),
    CODE("2*",      tos *= 2),
    CODE("2/",      tos /= 2),
    CODE("1+",      tos += 1),
    CODE("1-",      tos -= 1),
#if USE_FLOAT
    CODE("fmod",    ALU(%)),
    CODE("f>s",     tos = INT(tos)),
#else
    CODE("f>s",     /* do nothing */),
#endif // USE_FLOAT
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0=",      tos = BOOL(ZEQ(tos))),
    CODE("0<",      tos = BOOL(LT(tos, DU0))),
    CODE("0>",      tos = BOOL(GT(tos, DU0))),
    CODE("=",       CMP(EQ)),
    CODE(">",       CMP(GT)),
    CODE("<",       CMP(LT)),
    CODE("<>",      CMP(!EQ)),
    CODE(">=",      CMP(!LT)),
    CODE("<=",      CMP(!GT)),
    CODE("u<",      UCMP(LT)),
    CODE("u>",      UCMP(GT)),
    /// @}
    /// @defgroup IO ops
    /// @{
    CODE("base",    PUSH(vm.base)),
    CODE("decimal", *BASE=10),
    CODE("hex",     *BASE=16),
    CODE("bl",      PUSH(0x20)),
    CODE("cr",      dot(CR)),
    CODE(".",       dot(DOT,  POP(), *BASE)),
    CODE("u.",      dot(UDOT, POP(), *BASE)),
    CODE(".r",      IU w = POPI(); dotr(w, POP(), *BASE)),
    CODE("u.r",     IU w = POPI(); dotr(w, POP(), *BASE, true)),
    CODE("type",    POP(); pstr((const char*)MEM(POP()))),
    IMMD("key",     if (vm.compile) add_xt("_key"); else PUSH(key())),
    CODE("emit",    dot(EMIT, POP())),
    CODE("space",   dot(SPCS, DU1)),
    CODE("spaces",  dot(SPCS, POP())),
    /// @}
    /// @defgroup Literal ops
    /// @{
    IMMD("(",       SCAN(')')),
    IMMD(".(",      pstr(SCAN(')'))),
    IMMD("\\",      SCAN('\n')),
    IMMD("s\"",     s_quote(XT_RPASS, false)),
    IMMD(".\"",     s_quote(XT_RPASS, true)),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    IMMD("if",
         add_xt("_0bran");                         /// if    ( -- here )
         PUSH(HERE_TGT);                           /// save ip0
         add_iu(0)),
    IMMD("else",                                   /// else ( here -- there )
         add_xt("_bran");
         IU tgt  = HERE_TGT;                       /// save target
         add_iu(0);
         IU *ip0 = (IU*)MEM(POP());                /// fetch ip0
         *ip0 = HERE_TGT;
         PUSH(tgt)),
    IMMD("then",
         IU *ip0 = (IU*)MEM(POP());
         *ip0 = HERE_TGT),                         /// backfill jump address
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin", PUSH(HERE_TGT)),
    IMMD("again", add_xt("_bran");  add_iu(POPI())),         /// again    ( there -- )
    IMMD("until", add_xt("_0bran"); add_iu(POPI())),         /// until    ( there -- )
    IMMD("while",                                            /// while    ( there -- there here )
         add_xt("_0bran");
         PUSH(HERE_TGT);
         add_iu(0)),
    IMMD("repeat",                                           /// repeat    ( there1 there2 -- )
         add_xt("_bran");
         IU *t = (IU*)MEM(POP());                            /// set forward and loop back address
         add_iu(POPI());
         *t = HERE_TGT),
    /// @}
    /// @defgroup FOR...NEXT loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for" ,    add_xt("_for"); PUSH(HERE_TGT)),         /// for ( -- here )
    IMMD("next",    add_xt("_next"); add_iu(POPI())),        /// next ( here -- )
    IMMD("aft",                                              /// aft ( here -- here there )
         POP();
         add_xt("_bran");
         IU h = HERE_TGT;
         add_iu(0);
         PUSH(HERE_TGT);
         PUSH(h)),
    /// @}
    /// @}
    /// @defgroup DO..LOOP loops
    /// @{
    IMMD("do" ,     add_xt("_do"); PUSH(HERE_TGT)),          /// do ( -- here )
    CODE("i",       PUSH(RS[RP-1])),
    CODE("leave",   --RP; --RP; UNNEST()),                   /// NOTE: exits the word, not just the loop
    IMMD("loop",    add_xt("_loop"); add_iu(POPI())),        /// loop ( here -- )
    /// @}
    /// @defgroup return stack op
    /// @{
    CODE(">r",      RS[RP++] = POP()),
    CODE("r>",      PUSH(RS[--RP])),
    CODE("r@",      PUSH(RS[RP-1])),                         /// same as I (the loop counter)
    /// @}
    /// @defgroup Compiler ops
    /// @{
    CODE("[",       vm.compile = false),
    CODE("]",       vm.compile = true),
    CODE(":",       vm.compile = def_word(WORD())),
    IMMD(";",       add_xt("_;"); vm.compile = false),
    ///=============================================================================
    CODE("variable",                                         /// create a variable
         def_word(WORD());
         add_xt("_var");
         add_du(0)),
    CODE("constant",                                         /// create a constant
         def_word(WORD());                                   /// create a new word on dictionary
         add_xt("_lit");                                     /// dovar (+parameter field)
         add_du(POP());
         add_xt("_;")),
    IMMD("postpone",  const Code *w = find(WORD()); if (w) add_w(w)),
    CODE("immediate", dict[-1]->imm()),                      /// set immediate flag
    CODE("exit",    UNNEST()),                               /// early exit the colon word
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
//    CODE("exec",   IU w = POP(); doLIST(vm, w)),             /// execute word
    CODE("create",
         def_word(WORD());
         add_xt("vbran");                                    /// bran + offset field
         add_iu(0)),
    IMMD("does>",  add_xt("_does>")),
    IMMD("to",                                               /// alter the value of a constant, i.e. 3 to x
         if (vm.compile) {
             const Code *w = find(WORD());                   /// constant addr
             add_xt("_lit");
             add_iu(TOK(w->xt));                             /// push body address (no _: wrapper)
             add_xt("to");                                   /// encode to opcode
         }
         else {
             DU i = POP() + sizeof(IU);                      /// calculate address to memory
             *(DU*)MEM(DALIGN(i)) = POP();                   /// update constant
         }),
    IMMD("is",              /// ' y is x                     /// alias a word, i.e. ' y is x
         const Code *w = find(WORD());
         if (vm.compile) {
             add_xt("_lit");
             add_iu(TOK(w->xt));                             /// save addr on stack
             add_xt("is");
         }
         else {
             *(IU*)XT(POP()) = TOK(w->xt);
         }),
    ///
    /// be careful with memory access, especially BYTE because
    /// it could make access misaligned which slows the access speed by 2x
    ///
    CODE("@",                                                /// w -- n
         IU w = POPI();
         PUSH(w < USER_AREA ? (DU)IGET(w) : CELL(w))),       /// check user area
    CODE("!",     IU w = POPI(); CELL(w) = POP()),           /// n w --
    CODE("+!",    IU w = POPI(); CELL(w) += POP()),          /// n w --
    CODE("?",     IU w = POPI(); dot(DOT, CELL(w))),         /// w --
    CODE(",",     DU n = POP(); add_du(n)),                  /// n -- , compile a cell
    CODE("cells", tos = tos * sizeof(DU)),                   /// n -- n'
    CODE("allot",                                            /// n --
         IU n = POPI();                                      /// number of bytes
         for (IU i = 0; i < n; i+=sizeof(DU)) add_du(DU0)),  /// zero padding
    CODE("th",    tos = nos + tos * sizeof(DU); FILL()),     /// w i -- w'
    /// @}
#if DO_MULTITASK
    /// @defgroup Multitasking ops
    /// @}
    CODE("task",                                             /// w -- task_id
         IU w = POPI();                                      ///< dictionary index
         if (dict[w]->is_udf()) {
             SPILL();
             IU tid = task_create(dict[w]->pfa);             /// create a task starting on pfa
             RELOAD();
             PUSH(tid);
         }
         else pstr("  ?colon word only\n")),
    CODE("start", IU t = POPI(); SPILL(); task_start(t); RELOAD()),                       /// ( task_id -- )
    CODE("join",  IU t = POPI(); SPILL(); vm.join(t); RELOAD()),                          /// ( task_id -- )
    CODE("lock",  vm.io_lock()),                             /// wait for IO semaphore
    CODE("unlock",vm.io_unlock()),                           /// release IO semaphore
    CODE("send",  IU t = POPI(); IU n = POPI(); SPILL(); vm.send(t, n); RELOAD()),        /// ( v1 v2 .. vn n tid -- ) pass values onto task's stack
    CODE("recv",  SPILL(); vm.recv(); RELOAD()),                                /// ( -- v1 v2 .. vn ) waiting for values passed by sender
    CODE("bcast", IU n = POPI(); SPILL(); vm.bcast(n); RELOAD()),                         /// ( v1 v2 .. vn -- )
    CODE("pull",  IU t = POPI(); IU n = POPI(); SPILL(); vm.pull(t, n); RELOAD()),        /// ( tid n -- v1 v2 .. vn )
    /// @}
#endif // DO_MULTITASK
    /// @defgroup Debug ops
    /// @{
    CODE("rank",  PUSH(vm.id)),                              /// ( -- n ) thread id
    CODE("abort", sp = SS; RP = 0; ip = gStop),              /// clear ss, rs, and stop
    CODE("here",  PUSH(HERE)),
    IMMD("'",     const Code *w = find(WORD()); if (w) PUSH(TOK(w->xt))),
    CODE(".s",    SPILL(); ss_dump(vm, true)),
    CODE("words", words()),
    CODE("see",
         const Code *w = find(WORD());
         if (w) {
             pstr(": "); pstr(w->name, CR);
             if (w->is_udf()) see(TOK(w->xt), *BASE);
             else             pstr(" ( built-ins ) ;");
             dot(CR);
         }),
    CODE("depth", PUSH(DEPTH)),
    CODE("r",     PUSH(RP)),
    CODE("dump",
         U32 n = POPI();
         mem_dump(POPI(), n, *BASE)),
    CODE("dict",  dict_dump()),
    CODE("forget",
         const Code *w = find(WORD());                      /// bail, if not found
         if (w && w->is_udf()) {                            /// clear to specified word
             pmem.clear((int)((U8*)w->pfa - MEM0) - STRLEN(w->name));
             for (int i=dict.idx - 1; i >=0; dict.clear(i--)) {
                 if (dict[i] == w) { dict.clear(i); break; }
             }
         }
         else if (w) LOG("%s is built-in\n", w->name);      /// clear to 'boot'
    ),
    /// @}
    /// @defgroup OS ops
    /// @{
    IMMD("include",                                         /// include an OS file
         const char *f = WORD(); SPILL(); load(vm, f); RELOAD()),  
    CODE("included",                                        /// include file spec on stack
         POP();                                             /// string length, not used
         const char *f = (const char*)MEM(POP());
         SPILL(); load(vm, f); RELOAD()),                   /// include external file
    CODE("ok",    mem_stat()),
    CODE("clock", PUSH(millis())),
    CODE("rnd",   PUSH(RND())),                             /// generate random number
    CODE("ms",    delay(POPI())),
    CODE("bye",   vm.state=STOP),
    /// @}
    CODE("boot",  dict.clear(); pmem.clear(sizeof(DU)))
};
constexpr int  g_romsz = sizeof(g_rom)/sizeof(Code);
///@}
///
///> init base of xt pointer and xtoff range check
///
#if !XT0_U32
UFP Code::XT0 = 0;                             ///< init for 32-bit
#endif // !XT0_U32
void dict_compile() {                          ///< compile built-in words into dictionary
#if __SIZEOF_POINTER__ == 8
    // 1. Grab the full 64-bit runtime address of your first primitive lambda
    U64 base = (U64)(g_rom[0].xt);
    
    // 2. Isolate the top 32 bits (the memory page segment)
    // Mask out the lower 32 bits so it's ready for a lightning-fast bitwise OR
#if XT0_U32
    (void)base;                                /// XT0 is constexpr 0, just verify below
#else
    Code::XT0 = base & XT0_MSK;
#endif
    
    // 3. Safety Verification
    // Ensure ALL primitives reside within this exact same 4GB segment boundary page
    ///  pmem and the stop cell must share the segment too, or tokens can't be expanded back
    if ((((UFP)&pmem[0] + E4_PMEM_SZ) & XT0_MSK) != Code::XT0 ||
        (((UFP)gStop)                 & XT0_MSK) != Code::XT0 ||
        (((UFP)doSTOP)                & XT0_MSK) != Code::XT0) {
        ERR("pmem/gStop not in the same 4GB segment as code (use -no-pie, or allocate pmem low)!");
        exit(1);
    }
#endif // __SIZEOF_POINTER__ == 8 
}

void dict_validate() {
    UFP max = 0;

    for (int i = 0; i < g_romsz; i++) {
        UFP addr = (UFP)g_rom[i].xt;
        if (addr > max) max = addr;
    }
    if ((UFP)doSTOP > max) max = (UFP)doSTOP;
    U64 off = max - Code::XT0;

    LOG("XT0: 0x%zx, OFF: 0x%zx\n", Code::XT0, off);
    if (off > 0xFFFFFFFFULL)
        ERR("Execution memory space exceeds 32-bit offset limits!");
}
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
    DEBUG("forth_core(%s) ", idiom);
    
    vm.state = QUERY;
    const Code *w = find(idiom);                 ///> * get token by searching through dict

    if (w) {                                     ///> * word found?
        if (vm.compile && !w->is_imm()) {        /// * in compile mode?
            add_w(w);                            /// * add to colon word
        }
        else CALL(vm, *w);                       /// * execute word
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
        return;
    }
    DEBUG(" => %d\n", n);
    /// is a number
    if (vm.compile) {                    /// * a number in compile mode?
        add_xt("_lit");                  ///> add to current word
        add_du(n);
    }
    else {
        vm.ss.push(n);                   ///> or, add value onto data stack
    }
}
///====================================================================
///
/// Forth VM external command processor
///
void forth_init() {
    static bool init    = false;
    if (init) return;                    ///> check dictionary initilized

    if (!dict.v || !pmem.v) {
        LOG("forth_init memory allocation failed, %s...\n", "bail");
        exit(0);
    }
    MEM0 = &pmem[0];

    uvar_init();                         /// * initialize user area
    t_pool_init();                       /// * initialize thread pool
    VM &vm0   = vm_get(0);               /// * initialize main vm
    vm0.state = QUERY;
    vm0.ip    = (IU*)MEM0;

    for (int i = pmem.idx; i < USER_AREA; i+=sizeof(IU)) {
        add_iu(0xffff);                  /// * reserved user area
    }
    dict_compile();                      ///> compile dictionary
    dict_validate();                     ///< collect XT0, and check xtoff range

    init = true;
}

void forth_teardown() {
    t_pool_stop();
}

int forth_vm(const char *line, void(*hook)(int, const char*)) {
    VM &vm = vm_get(0);                                     ///< get main thread
    fout_setup(hook);
    fin_setup(line);                                        /// * refresh buffer if not resuming
    
    char idiom[E4_IBUF_SZ];
    while (fetch(idiom, E4_IBUF_SZ)) {                      /// * parse a word
        forth_core(vm, idiom);                              /// * outer interpreter
    }
    if (!vm.compile) ss_dump(vm);
    
    return vm.state==STOP;
}
