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
#define HERE_PTR  ((IU*)&pmem[HERE])
#define HERE_TGT  ((IU)*HERE_PTR)
#define XT(i)     ((IU*)(Code::XT0 | (UFP)(i)))
#define TOK(p)    ((IU)Code::Token((void*)(p)))
#define MEM(a)    (MEM0 + (IU)UINT(a))     /**< pointer to address fetched from pmem    */
#define BASE      (MEM(vm.base))           /**< pointer to base in VM user area         */
#define IGET(ip)  (*(IU*)MEM(ip))          /**< instruction fetch from pmem+ip offset   */
#define CELL(a)   (*(DU*)&pmem[a])         /**< fetch a cell from parameter memory      */
#define JMP()     ip = XT(*ip)             /**< set IP to target address                */
#define SETJMP(a) (*(IU*)&pmem[a] = HERE)  /**< address offset for branching opcodes    */
#define SCAN(c)   (scan(c, vm.pad, E4_PAD_SZ))
#define WORD()    (word(vm.pad, E4_PAD_SZ))
///@}
///====================================================================
///@}
///@name Colon word compiler
///@brief
///    * we separate dict and pmem space to make word uniform in size
///    * if they are combined then can behaves similar to classic Forth
///    * with an addition link field added.
///@{
void add_iu(IU i) { pmem.push((U8*)&i, sizeof(IU)); }  ///< add an instruction into pmem
void add_du(DU v) { pmem.push((U8*)&v, sizeof(DU)); }  ///< add a cell into pmem
int  add_str(const char *s) {       ///< add a string to pmem
    int sz = STRLEN(s);             ///> string length, aligned
    pmem.push((U8*)s,  sz);         /// * add string terminated with zero
    return sz;
}
void colon(const char *name) {
    char *nfa = (char*)&pmem[HERE]; ///> current pmem pointer
    add_str(nfa);

    Code *c = new Code(nfa, (FPTR)HERE_PTR, (U8)UDF_ATTR);
    dict.push(c);                   ///> deep copy Code struct into dictionary
}

inline const Code *get_word(IU w) {
    return w & UDF_DICT ? dict[w & ~UDF_DICT] : &g_rom[w];
}

void add_w(IU w) {
    const Code *c = get_word(w);
    
    if (c->is_udf()) {
        // User-defined word: compile CALL pre-processor
        // Truncate the function address down to a clean lower 32-bit token integer
        add_iu(TOK(doLIST));
    }
    // Compile the target body memory address pointer as a 32-bit data payload block
    add_iu(TOK(c->xt));
    
#if CC_DEBUG > 1
    LOG("add_w(%d) => %p %s\n", w, ip, c->name);
#endif // CC_DEBUG > 1
}

void add_xt(const char *name) {
    IU w = find(name);
    add_w(w);
}

void add_var(IU op, DU v=DU0) {     ///< add a varirable header
    add_w(op);                      /// * VAR or VBRAN
    if (op==VBRAN) add_iu(0);       /// * pad offset field
    pmem.idx = DALIGN(pmem.idx);    /// * data alignment
    if (op!=VBRAN) add_du(v);       /// * default variable = 0
}
///====================================================================
///
///> functions to reduce verbosity
///
#define PUSH(v) ({SS[++sp]=tos; tos = v;})
#define POP()   ({ DU n=tos; tos=SS[sp--]; n;})
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
void s_quote(VM &vm, prim_op op, int &sp, DU &tos) {
    const char *s = SCAN('"')+1;    ///> string skip first blank
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
#define UNNEST()     {                          \
        if (RS.idx <= 0) return NULL;           \
        ip = XT(RS.pop());       \
        return NEXT();                          \
    }

///====================================================================
///
///> eForth dictionary assembler
///  Note: sequenced by enum forth_opcode as following
///
void *dodoes(VM &vm, IU* &ip, int &sp, DU &tos) {
    SS[++sp] = tos;
    IU *t = (IU*)XT(*ip++);
    tos = (DU)TOK(ip);
    RS.push((DU)TOK(++ip));
    ip = t;
    return NEXT();
}

constexpr Code g_rom[] = {
    CODE("nop ",    {}),                                /// dict[0], not used, simplify find()
    CODE("_:",      RS.push((DU)TOK(ip)); JMP()),       /// docolon
    CODE("_const",  SS[++sp] = tos; tos = (DU)(*ip++)), /// doconst
    CODE("_var",    SS[++sp] = tos; tos = (DU)TOK(ip++)),
    CODE("_str",
         SS[++sp] = tos;
         U32 len = *ip++;
         SS[++sp] = (DU)TOK(ip);
         ip += (len + 3) >> 2),
    CODE("_dotq",
         U32 len = *ip++;
         pstr((char*)ip);
         ip += (len + 3) >> 2),
    CODE("_create", SS[++sp] = tos; tos = (DU)TOK(++ip); ip++),
    CODE("_does>",
         Code *c = dict[dict.idx = 1];
         IU   *t = (IU*)c->xt;
         *t++ = TOK(dodoes);
         *t   = TOK(ip)),
    ///=====================================================================
    CODE(";",       UNNEST()),
    CODE("next",
         if (GT(RS[-1] -= DU1, -DU1)) JMP();     ///> loop done? no, loop back
         else { RS.pop(); ip++; }),              /// * yes, bail!
    CODE("loop",
         if (GT(RS[-2], RS[-1] += DU1)) JMP();   ///> loop done? no, loop back
         else { RS.pop(); RS.pop(); ip++; }),    /// * pop off counters
    CODE("lit",
         SS[++sp] = tos;
         tos = *(DU*)(ip++)),
    CODE("var", PUSH(TOK(ip)); UNNEST()),
    CODE("str",
         const char *s = (const char*)ip;        ///< get string pointer
         U32 len = STRLEN(s);
         PUSH(TOK(ip));
         PUSH(len);
         ip += len),
    CODE("dotq",
         const char *s = (const char*)ip;        ///< get string pointer
         pstr(s); ip += STRLEN(s)),              /// * send to output console
    CODE("bran", JMP()),                         ///< unconditional jmp
    CODE("0bran",
         if (ZEQ(tos)) JMP(); else ip++;         /// conditional jmp
         tos = SS[sp--]),                        /// pop tos
    CODE("vbran",
         PUSH(TOK(++ip));                        /// * put param addr on tos
         if ((ip = (IU*)MEM(*ip))==0) UNNEST()), /// * jump target of does> if given
    CODE("does>",
         IU *t = (IU*)dict[-1]->xt;              ///< memory pointer to pfa 
         *(t+1) = *ip;                           /// * encode current IP, and bail
         UNNEST()),
    CODE("for", RS.push(POP())),
    CODE("do",  RS.push(SS[sp--]); RS.push(POP())),
    CODE("key", PUSH(key()); UNNEST()),
    ///
    /// @defgroup Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",     SS[sp++] = tos),
    CODE("drop",    tos = SS[sp--]),
    CODE("over",    DU v = SS[-1]; PUSH(v)),
    CODE("swap",    DU n = SS[sp--]; PUSH(n)),
    CODE("rot",     DU n = SS[sp--]; DU m = SS[sp--]; SS[++sp] = m; SS[++sp] = tos; tos = n),
    CODE("-rot",    DU n = SS[sp--]; DU m = SS[sp--]; SS[++sp] = tos; SS[++sp] = n; tos = m),
    CODE("pick",    IU i = UINT(tos); tos = SS[-i]),
    CODE("nip",     sp--),
    CODE("?dup",    if (tos != DU0) SS[sp++] = tos),
    /// @}
    /// @defgroup Stack ops - double
    /// @{
    CODE("2dup",    DU v = SS[-1]; PUSH(v); v = SS[-1]; PUSH(v)),
    CODE("2drop",   sp--; tos = SS[sp--]),
    CODE("2over",   DU v = SS[-3]; PUSH(v); v = SS[-3]; PUSH(v)),
    CODE("2swap",   DU n = SS[sp--]; DU m = SS[sp--]; DU l = SS[sp--];
                    SS.push(n); PUSH(l); PUSH(m)),
    /// @}
    /// @defgroup ALU ops
    /// @{
    CODE("+",       tos += SS[sp--]),
    CODE("*",       tos *= SS[sp--]),
    CODE("-",       tos =  SS[sp--] - tos),
    CODE("/",       tos =  SS[sp--] / tos),
    CODE("mod",     tos =  INT(MOD(SS[sp--], tos))),           /// ( a b -- c ) c integer, see fmod
    CODE("*/",
         DU2 ss0 = (DU2)SS[sp--];
         tos =  ss0 * SS[sp--] / tos),    /// ( a b c -- d ) d=a*b / c (float)
    CODE("/mod",    DU  n = SS[sp--];                          /// ( a b -- c d ) c=a%b, d=int(a/b)
                    DU  t = tos;
                    DU  m = MOD(n, t);
                    SS[++sp] = m; tos = INT(n / t)),
    CODE("*/mod",
         DU2 n = (DU2)SS[sp--];
         n *= SS[sp--];          /// ( a b c -- d e ) d=(a*b)%c, e=(a*b)/c
         DU2 t = tos;
         DU  m = MOD(n, t);
         SS[++sp] = m; tos = INT(n / t)),
    CODE("and",     tos = UINT(tos) & UINT(SS[sp--])),
    CODE("or",      tos = UINT(tos) | UINT(SS[sp--])),
    CODE("xor",     tos = UINT(tos) ^ UINT(SS[sp--])),
    CODE("abs",     tos = ABS(tos)),
    CODE("negate",  tos = -tos),
    CODE("invert",  tos = ~UINT(tos)),
    CODE("rshift",  tos = UINT(SS[sp--]) >> UINT(tos)),
    CODE("lshift",  tos = UINT(SS[sp--]) << UINT(tos)),
    CODE("max",     DU n=SS[sp--]; tos = (tos>n) ? tos : n),
    CODE("min",     DU n=SS[sp--]; tos = (tos<n) ? tos : n),
    CODE("2*",      tos *= 2),
    CODE("2/",      tos /= 2),
    CODE("1+",      tos += 1),
    CODE("1-",      tos -= 1),
#if USE_FLOAT
    CODE("fmod",    tos = MOD(SS[sp--], tos)),                /// -3.5 2 fmod => -1.5
    CODE("f>s",     tos = INT(tos)),                          /// 1.9 => 1, -1.9 => -1
#else
    CODE("f>s",     /* do nothing */),
#endif // USE_FLOAT
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0=",      tos = BOOL(ZEQ(tos))),
    CODE("0<",      tos = BOOL(LT(tos, DU0))),
    CODE("0>",      tos = BOOL(GT(tos, DU0))),
    CODE("=",       tos = BOOL(EQ(SS[sp--], tos))),
    CODE(">",       tos = BOOL(GT(SS[sp--], tos))),
    CODE("<",       tos = BOOL(LT(SS[sp--], tos))),
    CODE("<>",      tos = BOOL(!EQ(SS[sp--], tos))),
    CODE(">=",      tos = BOOL(!LT(SS[sp--], tos))),
    CODE("<=",      tos = BOOL(!GT(SS[sp--], tos))),
    CODE("u<",      tos = BOOL(UINT(SS[sp--]) < UINT(tos))),
    CODE("u>",      tos = BOOL(UINT(SS[sp--]) > UINT(tos))),
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
    CODE("type",    pstr((const char*)MEM(SS[sp--])); tos = SS[sp--]),   /// pass string pointer
    IMMD("key",     if (vm.compile) add_w(KEY); else PUSH(key())),
    CODE("emit",    dot(EMIT, POP())),
    CODE("space",   dot(SPCS, DU1)),
    CODE("spaces",  dot(SPCS, POP())),
    /// @}
    /// @defgroup Literal ops
    /// @{
    IMMD("(",       SCAN(')')),
    IMMD(".(",      pstr(SCAN(')'))),
    IMMD("\\",      SCAN('\n')),
    IMMD("s\"",     s_quote(vm, STR, sp, tos)),
    IMMD(".\"",     s_quote(vm, DOTQ, sp, tos)),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    /// @{
    IMMD("if",
         add_xt("0bran");                          /// if    ( -- here )
         SS[++sp] = (DU)HERE_TGT;                  /// save ip0
         add_iu(0)),
    IMMD("else",                                   /// else ( here -- there )
         add_xt("bran");
         IU tgt  = HERE_TGT;                       /// save target
         add_iu(0);
         IU *ip0 = (IU*)MEM(SS[sp]);               /// fetch ip0
         *ip0 = HERE_TGT;
         SS[sp] = (DU)tgt),
    IMMD("then",
         IU *ip0 = (IU*)MEM(SS[sp--]);
         *ip0 = HERE_TGT),                         /// backfill jump address
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin", SS[++sp] = (DU)HERE_TGT),
    IMMD("again", add_xt("bran");  add_iu(SS[sp--])),        /// again    ( there -- )
    IMMD("until", add_xt("0bran"); add_iu(SS[sp--])),        /// until    ( there -- )
    IMMD("while",                                            /// while    ( there -- there here )
         add_xt("0bran");
         SS[++sp] = (DU)HERE_TGT;                            /// not touching tos
         add_xt(0)),
    IMMD("repeat",                                           /// repeat    ( there1 there2 -- )
         add_xt("bran");
         IU *t = (IU*)MEM(SS[sp--]);                         /// set forward and loop back address
         add_iu(SS[sp--]);
         add_xt("bran");
         *t = HERE_TGT),
    /// @}
    /// @defgrouop FOR...NEXT loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for" ,    add_xt("for"); SS[++sp] = HERE_TGT),     /// for ( -- here )
    IMMD("next",    add_xt("next"); add_iu(SS[sp--])),       /// next ( here -- )
    IMMD("aft",                                              /// aft ( here -- here there )
         sp--;
         add_xt("bran");
         IU h = HERE_TGT;
         add_iu(0);
         SS[++sp] = (DU)HERE_TGT;
         SS[++sp] = (DU)h),
    /// @}
    /// @}
    /// @defgrouop DO..LOOP loops
    /// @{
    IMMD("do" ,     add_xt("do"); SS[++sp]=(DU)HERE_TGT),    /// for ( -- here )
    CODE("i",       PUSH(RS[-1])),
    CODE("leave",   RS.pop(); RS.pop(); UNNEST()),           /// quit DO..LOOP
    IMMD("loop",    add_xt("loop"); add_iu(SS[sp--])),       /// next ( here -- )
    /// @}
    /// @defgrouop return stack op
    /// @{
    CODE(">r",      RS.push(POP())),
    CODE("r>",      PUSH(RS.pop())),
    CODE("r@",      PUSH(RS[-1])),                           /// same as I (the loop counter)
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("[",       vm.compile = false),
    CODE("]",       vm.compile = true),
    CODE(":",       vm.compile = def_word(WORD())),
    IMMD(";",       add_w(EXIT); vm.compile = false),
#if 0
    ///=============================================================================
    CODE("variable",def_word(WORD()); add_var(VAR)),         /// create a variable
    CODE("constant",                                         /// create a constant
         def_word(WORD());                                   /// create a new word on dictionary
         add_var(LIT, POP());                                /// dovar (+parameter field)
         add_w(EXIT)),
    IMMD("postpone",  IU w = find(WORD()); if (w) add_w(w)),
    CODE("immediate", dict[-1]->attr |= IMM_ATTR),
    CODE("exit",    UNNEST()),                               /// early exit the colon word
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   IU w = POP(); doLIST(vm, w)),             /// execute word
    CODE("create", def_word(WORD()); add_var(VBRAN)),        /// bran + offset field
    IMMD("does>",  add_w(DOES)),
    IMMD("to",                                               /// alter the value of a constant, i.e. 3 to x
         IU w = vm.state==QUERY ? find(WORD()) : POP();      /// constant addr
         if (!w) return;
         if (vm.compile) {
             add_var(LIT, (DU)w);                            /// save addr on stack
             add_w(find("to"));                              /// encode to opcode
         }
         else {
             w = dict[w]->pfa + sizeof(IU);                  /// calculate address to memory
             *(DU*)MEM(DALIGN(w)) = POP();                   /// update constant
         }),
    IMMD("is",              /// ' y is x                     /// alias a word, i.e. ' y is x
         IU w = vm.state==QUERY ? find(WORD()) : POP();      /// word add
         if (!w) return;
         if (vm.compile) {
             add_var(LIT, (DU)w);                            /// save addr on stack
             add_w(find("is"));
         }
         else {
             dict[POP()]->xt = dict[w]->xt;
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
    CODE("cells", IU i = POPI(); PUSH(i * sizeof(DU))),      /// n -- n'
    CODE("allot",                                            /// n --
         IU n = POPI();                                      /// number of bytes
         for (int i = 0; i < n; i+=sizeof(DU)) add_du(DU0)), /// zero padding
    CODE("th",    IU i = POPI(); TOS += i * sizeof(DU)),     /// w i -- w'
#endif
    /// @}
#if DO_MULTITASK
    /// @defgroup Multitasking ops
    /// @}
    CODE("task",                                             /// w -- task_id
         IU w = POPI();                                      ///< dictionary index
         if (!dict[w]->is_udf()) PUSH(task_create(dict[w]->pfa));  /// create a task starting on pfa
         else pstr("  ?colon word only\n")),
    CODE("rank",  PUSH(vm.id)),                              /// ( -- n ) thread id
    CODE("start", task_start(POPI())),                       /// ( task_id -- )
    CODE("join",  vm.join(POPI())),                          /// ( task_id -- )
    CODE("lock",  vm.io_lock()),                             /// wait for IO semaphore
    CODE("unlock",vm.io_unlock()),                           /// release IO semaphore
    CODE("send",  IU t = POPI(); vm.send(t, POPI())),        /// ( v1 v2 .. vn n tid -- ) pass values onto task's stack
    CODE("recv",  vm.recv()),                                /// ( -- v1 v2 .. vn ) waiting for values passed by sender
    CODE("bcast", vm.bcast(POPI())),                         /// ( v1 v2 .. vn -- )
    CODE("pull",  IU t = POPI(); vm.pull(t, POPI())),        /// ( tid n -- v1 v2 .. vn )
    /// @}
#endif // DO_MULTITASK
    /// @defgroup Debug ops
    /// @{
    CODE("abort", tos = -DU1; SS.clear(); RS.clear()),       /// clear ss, rs
    CODE("here",  PUSH(HERE)),
    IMMD("'",     IU w = find(WORD()); if (w) PUSH(w)),
    CODE(".s",    ss_dump(vm, true)),
    CODE("words", words()),
    CODE("see",
         IU w = find(WORD()); if (!w) return NEXT();
         const Code *c = get_word(w);
         pstr(": "); pstr(c->name, CR);
         if (w & UDF_DICT) see(TOK(c->xt), *BASE);
         else              pstr(" ( built-ins ) ;");
         dot(CR)),
    CODE("depth", IU i = UINT(SS.idx); PUSH(i)),
    CODE("r",     PUSH(RS.idx)),
    CODE("dump",
         U32 n = POPI();
         mem_dump(POPI(), n, *BASE)),
    CODE("dict",  dict_dump()),
    CODE("forget",
         IU w = find(WORD()); if (!w) return NEXT();        /// bail, if not found
         IU b = find("boot")+1;
         if (w > b) {                                       /// clear to specified word
             pmem.clear(dict[w]->pfa - STRLEN(dict[w]->name));
             dict.clear(w);
         }
         else {                                             /// clear to 'boot'
             pmem.clear(USER_AREA);
             dict.clear(b);
         }
    ),
    /// @}
    /// @defgroup OS ops
    /// @{
    IMMD("include", load(vm, WORD())),                      /// include an OS file
    CODE("included",                                        /// include file spec on stack
         POP();                                             /// string length, not used
         load(vm, (const char*)MEM(POP()))),                /// include external file
    CODE("ok",    mem_stat()),
    CODE("clock", PUSH(millis())),
    CODE("rnd",   PUSH(RND())),                             /// generate random number
    CODE("ms",    delay(POPI())),
    CODE("bye",   vm.state=STOP),
    /// @}
    CODE("boot",  dict.clear(); pmem.clear(sizeof(DU)))
};
constexpr int  g_romsz = sizeof(g_rom)/sizeof(Code);
///
///@name Dictionary search functions - can be adapted for ROM+RAM
///@{
///
IU find(const char *s) {
    IU v = 0;
    for (IU i = dict.idx - 1; dict.idx && !v && i >= 0; --i) {
        LOG("  dict[%d] => %s\n", i, (char*)dict[i]->name);
        if (STRCMP(s, dict[i]->name)==0) v = i | UDF_DICT;
    }
    for (IU i = g_romsz - 1; !v && i > 0; --i) {
        LOG("  g_rom[%d] => %s\n", i, (char*)g_rom[i].name);
        if (STRCMP(s, g_rom[i].name)==0) v = i;
    }
#if CC_DEBUG > 1
    const Code *c = v > g_romsz ? dict[v - g_romsz] : &g_rom[v];
    LOG("find(%s) => %s[%d] %s attr=%d\n",
        s, v > g_romsz ? "dict" : "g_rom", v, c->name, c->attr);
#endif // CC_DEBUG > 1
    return v;
}

void nest(VM& vm) /* tail call */ {
    vm.state = NEST;

    /* 1. Extract core virtual machine tracking metrics locally onto the local stack frame */
    IU  *ip = IP;                 /* Local Instruction Pointer map */
    int &sp = SS.idx;             /* Local Data Stack index map */
    DU  tos = TOS;                /* Local cached Top-of-Stack register map */

    /* 2. Read the initial function execution token from the current array offset */
    FPTR fp = (FPTR)NEXT();

    /* 
     * 3. THE TAIL-CALL TRAMPOLINE DRIVER ENGINE:
     * While next points to a valid function address, invoke it.
     * The compiler flattens this assignment sequence into an optimized 
     * 'jx' or 'jmp' assembly branch instruction under C++17 rules.
     * This is called "Scalar Replacement of Aggregates and Reference Propagation"
     */
    while (fp) {
        fp = (FPTR)fp(vm, ip, sp, tos);
    }

    /* 4. Flush the final stable register configurations back into the persistent VM memory block */
    IP  = ip;
    TOS = tos;
}
///
///> doLIST - inner-interpreter proxy (inline macro does not run faster)
///
void *doLIST(VM& vm, IU* &ip, int &sp, DU &tos) {
    LOG(" doLIST=[%x,%x] ", *ip, *(ip+1));
    RS.push((DU)TOK(ip));
    IU t = *ip++;
    ip = XT(t);
    LOG(" => t=%x ip=%p\n", t, ip);
    return NEXT();
}
///
///> init base of xt pointer and xtoff range check
///
UFP Code::XT0 = 0;                             ///< init for 32-bit
void dict_compile() {                          ///< compile built-in words into dictionary
#if __SIZEOF_POINTER__ == 8    
    // 1. Grab the full 64-bit runtime address of your first primitive lambda
    U64 base = (U64)(g_rom[0].xt);
    
    // 2. Isolate the top 32 bits (the memory page segment)
    // Mask out the lower 32 bits so it's ready for a lightning-fast bitwise OR
    Code::XT0 = base & 0xFFFFFFFF00000000ULL;
    
    // 3. Safety Verification
    // Ensure ALL primitives reside within this exact same 4GB segment boundary page
    for (int i = 1; i < g_romsz; i++) {
        U64 addr = (U64)(g_rom[i].xt);
        if ((addr & 0xFFFFFFFF00000000ULL) != Code::XT0) {
            ERR("Primitives crossed a 4GB segment boundary layer!");
        }
    }
#endif // __SIZEOF_POINTER__ == 8 
}

void dict_validate() {
    UFP max = 0;

    for (int i = 0; i < g_romsz; i++) {
        UFP addr = (UFP)g_rom[i].xt;
        if (addr > max) max = addr;
    }
    U64 off = max - Code::XT0;

    LOG("XT0: 0x%zx, OFF, 0x%zx\n", Code::XT0, off);
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
    LOG("forth_core(%s) ", idiom);
    
    vm.state = QUERY;
    IU w = find(idiom);                          ///> * get token by searching through dict

    if (w) {                                     ///> * word found?
        const Code *c = get_word(w);
        LOG(" => [%d] %s", w, c->name);
        if (vm.compile && !c->is_imm()) {        /// * in compile mode?
            add_w(w);                            /// * add to colon word
        }
        else {
            IU stub[2] = { TOK(c->xt), 0 };
            LOG(" stub=[%x,%x]\n", stub[0], stub[1]);
            IU  *ip = stub;
            int &sp = SS.idx;
            DU  tos = TOS;
            doLIST(vm, ip, sp, tos);   /// * execute forth word
            TOS = tos;
        }
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
    LOG(" => %d", n);
    /// is a number
    if (vm.compile) {                    /// * a number in compile mode?
        add_xt("lit");                   ///> add to current word
        add_du(n);                       
    }
    else {                               ///> or, add value onto data stack
        SS.push(vm.tos);
        vm.tos = n;                      
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
    return 1;
    if (!vm.compile) ss_dump(vm);
    
    return vm.state==STOP;
}
