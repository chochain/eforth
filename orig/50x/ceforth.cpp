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
///     | strlen+1     | 4-byte | 4-byte |     |      |
///     +--------------+--------+--------+-----+------+---- 4-byte aligned
///
List<Code, 0> dict;                ///< dictionary
List<U8,   0> pmem;                ///< parameter memory (for colon definitions)
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
#define LAST      (dict[dict.idx-1])       /**< last colon word defined                 */
#define MEM(a)    (MEM0 + (IU)UINT(a))     /**< pointer to address fetched from pmem    */
#define BASE      (MEM(vm.base))           /**< pointer to base in VM user area         */
#define IGET(ip)  (*(Param*)MEM(ip))       /**< instruction fetch from pmem+ip offset   */
#define CELL(a)   (*(DU*)&pmem[a])         /**< fetch a cell from parameter memory      */
#define SETJMP(a) ((*(Param*)&pmem[a]).ioff = HERE)  /**< address offset for branching  */
///@}
///@name Primitive words (to simplify compiler), see nest() for details
///@{
Code prim[] = {
    Code(";",   EXIT), Code("nop",  NOP),   Code("next", NEXT),  Code("loop", LOOP),
    Code("lit", LIT),  Code("var",  VAR),   Code("str",  STR),   Code("dotq", DOTQ),
    Code("bran",BRAN), Code("0bran",ZBRAN), Code("vbran",VBRAN), Code("does>",DOES),
    Code("for", FOR),  Code("do",   DO),    Code("key",  KEY)
};
#define DICT(w) (IS_PRIM(w) ? prim[w] : dict[w])
///
///====================================================================
///@}
///@name Dictionary search functions - can be adapted for ROM+RAM
///@{
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
///@}
///@name Colon word compiler
///@brief
///    * we separate dict and pmem space to make word uniform in size
///    * if they are combined then can behaves similar to classic Forth
///    * with an addition link field added.
///@{
void colon(const char *name) {
    printf("colon %s HERE=%x\n", name, HERE);
    char *nfa = (char*)&pmem[HERE]; ///> current pmem pointer
    int sz = STRLEN(name);          ///> string length, aligned
    pmem.push((U8*)name,  sz);      ///> setup raw name field

    Code c(nfa, NULL, false);       ///> create a local blank word
    c.pfa = HERE | UDF_ATTR;        ///> capture code field index

    dict.push(c);                   ///> deep copy Code struct into dictionary
}
void add_iu(IU i) { pmem.push((U8*)&i, sizeof(IU)); }  ///< add an instruction into pmem
void add_du(DU v) { pmem.push((U8*)&v, sizeof(DU)); }  ///< add a cell into pmem
int  add_str(const char *s) {       ///< add a string to pmem
    int sz = STRLEN(s);
    pmem.push((U8*)s,  sz);         /// * add string terminated with zero
    return sz;
}
void add_p(prim_op op, IU ip=0) {
    Param p(ip, op, false);
    add_iu(p.pack);
};
void add_w(IU w) {                  ///< add a word index into pmem
    Param p(dict[w].ip(), MAX_OP, IS_UDF(w));
    add_iu(p.pack);
#if CC_DEBUG > 1
    Code &c = dict[w];
    LOG_KV("add_w(", w); LOG_KX(") => ", c.ip());
    LOGS(" "); LOGS(c.name); LOGS("\n");
#endif // CC_DEBUG > 1
}
void add_var(prim_op op, DU v=DU0) {  ///< add a literal/varirable
    bool neg = op==LIT && LT(v, DU0); ///> negative number
//    add_p(neg ? NLIT : op, (IU)v);
    add_p(op, static_cast<IU>(v));
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
        add_p(op, STRLEN(s));       ///> dostr, (+parameter field)
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
///  Note:
///  * overhead here in C call/return vs NEXT threading (in assembly)
///  * use of dp (iterative depth control) instead of WP by Dr. Ting
///    speeds up 8% vs recursive calls but loose the flexibity of Forth
///  * computed-goto entire dict runs 15% faster, but needs long macros (for enum) and extra memory
///  * use of cached _NXT address speeds up 10% on AMD but
///    5% slower on ESP32 probably due to shallow pipeline
///  * separate primitive opcodes into nest() with 'switch' speeds up 15%.
///  * separate nest() with computed-goto slows 2% (lost the gain above).
///  * use local stack speeds up 10%, but allot 4*64 bytes extra
///  * extra vm& passing performs about the same (via EAX on x86).
///
#define DISPATCH(op) switch(op)
#define CASE(op, g)  case op : { g; } break
#define OTHER(g)     default : { g; } break
#define UNNEST()     (IP=UINT(RS.pop()))

void nest(VM& vm) {
    vm.state = NEST;                                 /// * activate VM
    while (IP) {
        Param ix = IGET(IP);                         ///< fetched opcode, hopefully in register
        VM_HDR(&vm, ":%x", ix.op);
        IP += sizeof(IU);
        DISPATCH(ix.op) {                            /// * opcode dispatcher
        CASE(EXIT, UNNEST());
        CASE(NOP,  { /* do nothing */});
        CASE(NEXT,
             if (GT(RS[-1] -= DU1, -DU1)) {          ///> loop done?
                 ix = IGET(IP);                      /// * no, loop back
             }
             else {                                  /// * yes, loop done!
                 RS.pop();                           /// * pop off loop counter
                 IP += sizeof(IU);                   /// * next instr.
             });
        CASE(LOOP,
             if (GT(RS[-2], RS[-1] += DU1)) {        ///> loop done?
                 ix = IGET(IP);                      /// * no, loop back
             }
             else {                                  /// * yes, done
                 RS.pop(); RS.pop();                 /// * pop off counters
                 IP += sizeof(IU);                   /// * next instr.
             });
        CASE(LIT, SS.push(TOS); TOS = ix.lit());     ///> get short lit
        CASE(VAR, PUSH(DALIGN(IP)); UNNEST());       ///> get var addr
        CASE(STR,
             PUSH(IP); PUSH(ix.ioff); IP += ix.ioff);
        CASE(DOTQ,                                   /// ." ..."
             const char *s = (const char*)MEM(IP);   ///< get string pointer
             pstr(s); IP += ix.ioff);                /// * send to output console
        CASE(BRAN, ix = IGET(IP));                   /// * unconditional branch
        CASE(ZBRAN,                                  /// * conditional branch
             IP = POP() ? IP+sizeof(IU) : IGET(IP).ioff);
        CASE(VBRAN,
             PUSH(DALIGN(IP + sizeof(IU)));          /// * put param addr on tos
             if ((IP = IGET(IP).ioff)==0) UNNEST()); /// * jump target of does> if given
        CASE(DOES,
             IU *p = (IU*)MEM(LAST.pfa);             ///< memory pointer to pfa 
             *(p+1) = IP;                            /// * encode current IP, and bail
             UNNEST());
        CASE(FOR, RS.push(ix.ioff));                 /// * setup FOR..NEXT call frame
        CASE(DO,  RS.push(POP()); RS.push(ix.ioff)); /// * setup DO..LOOP call frame
        CASE(KEY, PUSH(key()); UNNEST());            /// * fetch single keypress
        OTHER(
            if (ix.op==MAX_OP) {                     /// * colon word?
                RS.push(IP);                         /// * setup call frame
                IP = ix.ioff;                        /// * IP = word.pfa
            }
            else Code::exec(vm, ix.ioff));           ///> execute built-in word
        }
        VM_TLR(&vm, " => SS=%d, RS=%d, IP=%x", SS.idx, RS.idx, IP);
    }
}
///
///> CALL - inner-interpreter proxy (inline macro does not run faster)
///
void CALL(VM& vm, IU w) {
    if (IS_UDF(w)) {                   /// colon word
        RS.push(IP);                   /// * terminating IP
        IP = dict[w].ip();             /// setup task context
        nest(vm);
    }
    else dict[w].call(vm);             /// built-in word
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
    CODE("int",
         TOS = TOS < DU0 ? -DU1 * UINT(-TOS) : UINT(TOS)); // float => integer
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
    CODE("type",    POP(); pstr((const char*)MEM(POP())));     // pass string pointer
    IMMD("key",     if (vm.compile) add_p(KEY); else PUSH(key()));
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
    IMMD("if",      PUSH(HERE); add_p(ZBRAN));                  // if    ( -- here )
    IMMD("else",    IU h=HERE;  add_p(BRAN);                    // else ( here -- there )
                    SETJMP(POPI()); PUSH(h));
    IMMD("then",    SETJMP(POPI()));                            // backfill jump address
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",   PUSH(HERE));
    IMMD("again",   add_p(BRAN, POPI()));                       // again    ( there -- )
    IMMD("until",   add_p(ZBRAN, POPI()));                      // until    ( there -- )
    IMMD("while",   PUSH(HERE); add_p(ZBRAN));                  // while    ( there -- there here )
    IMMD("repeat",                                              // repeat    ( there1 there2 -- )
         IU t=POPI(); add_p(BRAN, POPI()); SETJMP(t));          // set forward and loop back address
    /// @}
    /// @defgrouop FOR...NEXT loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for" ,    add_p(FOR, POPI()); PUSH(HERE));            // for ( -- here )
    IMMD("next",    add_p(NEXT, POPI()));                       // next ( here -- )
    IMMD("aft",                                                 // aft ( here -- here there )
         POP(); IU h=HERE; add_p(BRAN); PUSH(h); PUSH(h));
    /// @}
    /// @}
    /// @defgrouop DO..LOOP loops
    /// @{
    IMMD("do" ,     add_p(DO, POPI()); PUSH(HERE));             // for ( -- here )
    CODE("i",       PUSH(RS[-1]));
    CODE("leave",   RS.pop(); RS.pop(); UNNEST());              // quit DO..LOOP
    IMMD("loop",    add_p(LOOP, POPI()));                       // next ( here -- )
    /// @}
    /// @defgrouop return stack ops
    /// @{
    CODE(">r",      RS.push(POP()));
    CODE("r>",      PUSH(RS.pop()));
    CODE("r@",      PUSH(RS[-1]));                              // same as I (the loop counter)
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("[",       vm.compile = false);
    CODE("]",       vm.compile = true);
    CODE(":",       vm.compile = def_word(word()));
    IMMD(";",       add_p(EXIT); vm.compile = false);
    CODE("variable",def_word(word()); add_var(VAR));            // create a variable
    CODE("constant",                                            // create a constant
         def_word(word());                                      // create a new word on dictionary
         add_var(LIT, POP());                                   // dovar (+parameter field)
         add_p(EXIT));
    IMMD("immediate", dict[-1].pfa |= IMM_ATTR);
    CODE("exit",    UNNEST());                                  // early exit the colon word
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   IU w = POP(); CALL(vm, w));                  // execute word
    CODE("create", def_word(word()); add_var(VBRAN));           // bran + offset field
    IMMD("does>",  add_p(DOES));
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
         IU w = POPI();
         PUSH(w < USER_AREA ? (DU)IGET(w).ioff : CELL(w)));     // check user area
    CODE("!",     IU w = POPI(); CELL(w) = POP(););             // n w --
    CODE("+!",    IU w = POPI(); CELL(w) += POP());             // n w --
    CODE("?",     IU w = POPI(); dot(DOT, CELL(w)));            // w --
    CODE(",",     DU n = POP(); add_du(n));                     // n -- , compile a cell
    CODE("cells", IU i = POPI(); PUSH(i * sizeof(DU)));         // n -- n'
    CODE("allot",                                               // n --
         IU n = POPI();                                         // number of bytes
         for (IU i = 0; i < n; i+=sizeof(DU)) add_du(DU0));     // zero padding
    CODE("th",    IU i = POPI(); TOS += i * sizeof(DU));        // w i -- w'
    /// @}
#if DO_MULTITASK    
    /// @defgroup Multitasking ops
    /// @}
    CODE("task",                                                // w -- task_id
         IU w = POPI();                                         ///< dictionary index
         if (IS_UDF(w)) PUSH(task_create(dict[w].pfa));         /// create a task starting on pfa
         else pstr("  ?colon word only\n"));
    CODE("rank",  PUSH(vm.id));                                 /// ( -- n ) thread id
    CODE("start", task_start(POPI()));                          /// ( task_id -- )
    CODE("join",  vm.join(POPI()));                             /// ( task_id -- )
    CODE("lock",  vm.io_lock());                                /// wait for IO semaphore
    CODE("unlock",vm.io_unlock());                              /// release IO semaphore
    CODE("send",  IU t = POPI(); vm.send(t, POPI()));           /// ( v1 v2 .. vn n tid -- ) pass values onto task's stack
    CODE("recv",  vm.recv());                                   /// ( -- v1 v2 .. vn ) waiting for values passed by sender
    CODE("bcast", vm.bcast(POPI()));                            /// ( v1 v2 .. vn -- )
    CODE("pull",  IU t = POPI(); vm.pull(t, POPI()));           /// ( tid n -- v1 v2 .. vn )
    /// @}
#endif // DO_MULTITASK    
    /// @defgroup Debug ops
    /// @{
    CODE("abort", TOS = -DU1; SS.clear(); RS.clear());          // clear ss, rs
    CODE("here",  PUSH(HERE));
    IMMD("'",     IU w = find(word()); if (w) PUSH(w));
    CODE(".s",    ss_dump(vm, true));
    CODE("words", words(*BASE));
    CODE("see",
         IU w = find(word()); if (!w) return;
         pstr(": "); pstr(dict[w].name);
         if (IS_UDF(w)) see(dict[w].pfa, *BASE);
         else           pstr(" ( built-ins ) ;");
         dot(CR));
    CODE("depth", IU i = UINT(SS.idx); PUSH(i));
    CODE("r",     PUSH(RS.idx));
    CODE("dump",
         U32 n = POPI();
         mem_dump(POPI(), n, *BASE));
    CODE("dict",  dict_dump(*BASE));
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
    CODE("clock", PUSH(millis()));
    CODE("rnd",   PUSH(RND()));              // generate random number
    CODE("ms",    delay(POPI()));
    CODE("included",                         // include external file
         POP();                              // string length, not used
         load(vm, (const char*)MEM(POP()))); // include external file
#if DO_WASM
    CODE("JS",    native_api(vm));           // Javascript interface
#else    
    CODE("bye",   t_pool_stop(); exit(0));
#endif // DO_WASM
    /// @}
    CODE("boot",  dict.clear(find("boot") + 1); pmem.clear(sizeof(DU)));
}
///
///> init base of xt pointer and xtoff range check
///
#if DO_WASM
UFP Code::XT0 = 0;       ///< WASM xt is vtable index (0 is min)
#else // !DO_WASM
UFP Code::XT0 = ~0;      ///< init to max value
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
    // try as a number
    int err = 0;
    DU  n   = parse_number(idiom, *BASE, &err);
    if (err) {                           /// * not number
        pstr(idiom); pstr("? ", CR);     ///> display error prompt
        pstr(strerror(err), CR);         ///> and error description
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
void forth_init() {
    static bool init    = false;
    if (init) return;                    ///> check dictionary initilized

    dict  = new Code[E4_DICT_SZ];        ///< allocate dictionary
    pmem  = new U8[E4_PMEM_SZ];          ///< allocate parameter memory
    if (!dict.v || !pmem.v) {
        LOGS("forth_init memory allocation failed, bail...\n");
        exit(0);
    }
    MEM0  = &pmem[0];

    uvar_init();                         /// * initialize user area
    t_pool_init();                       /// * initialize thread pool
    VM &vm0   = vm_get(0);               /// * initialize main vm
    vm0.state = QUERY;
    
    for (int i = pmem.idx; i < USER_AREA; i+=sizeof(IU)) {
        add_iu(~0);                      /// * reserved user area
    }
    dict_compile();                      ///> compile dictionary
}
int forth_vm(const char *line, void(*hook)(int, const char*)) {
    VM &vm = vm_get(0);                  ///< get main thread
    fout_setup(hook);
    fin_setup(line);                     /// * refresh buffer if not resuming
    
    string idiom;
    while (fetch(idiom)) {               /// * parse a word
        forth_core(vm, idiom.c_str());   /// * outer interpreter
    }
    if (!vm.compile) ss_dump(vm);
    
    return 0;
}
