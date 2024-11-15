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
List<Code, 0> dict;                ///< dictionary
List<U8,   0> pmem;                ///< parameter memory (for colon definitions)
U8  *MEM0;                         ///< base of parameter memory block
VM  vm0;
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
#define IGET(ip)  (*(IU*)MEM(ip))          /**< instruction fetch from pmem+ip offset   */
#define CELL(a)   (*(DU*)&pmem[a])         /**< fetch a cell from parameter memory      */
#define SETJMP(a) (*(IU*)&pmem[a] = HERE)  /**< address offset for branching opcodes    */
///@}
///@name Primitive words
///@{
Code prim[] = {
    Code(";",   EXIT), Code("nop",  NOP),   Code("next", NEXT),  Code("loop", LOOP),
    Code("lit", LIT),  Code("var",  VAR),   Code("str",  STR),   Code("dotq", DOTQ),
    Code("bran",BRAN), Code("0bran",ZBRAN), Code("vbran",VBRAN), Code("does>",DOES),
    Code("for", FOR),  Code("do",   DO),    Code("key",  KEY)
};
#define DICT(w) (IS_PRIM(w) ? prim[w & ~EXT_FLAG] : dict[w])
///@}
///
///> user variables
///
IU   load_dp = 0;       ///< depth of recursive include
///@}
///
///> inline functions to reduce verbosity
///
#define PUSH(v) ({ SS.push(TOS); TOS = v; })
#define POP()   ({ DU n=TOS; TOS=SS.pop(); n; })
#define POPI()  (UINT(POP()))
///
///====================================================================
///
///@name Dictionary search functions - can be adapted for ROM+RAM
///@{
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
    Code &c = DICT(w);              /// * ref to primitive or dictionary
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
void add_var(IU op, DU v=DU0) {     ///< add a literal/varirable header
    add_w(op);                      /// * VAR or VBRAN
    if (op==VBRAN) add_iu(0);       /// * pad offset field
    pmem.idx = DALIGN(pmem.idx);    /// * data alignment (WASM 4, other 2)
    if (op!=VBRAN) add_du(v);       /// * default variable = 0
}
int def_word(const char* name) {    ///< display if redefined
    if (name[0]=='\0') { pstr(" name?", CR); return 0; }  /// * missing name?
    if (find(name)) {               /// * word redefined?
        pstr(" reDef? ", CR);
    }
    colon(name);                    /// * create a colon word
    return 1;                       /// * created OK
}
char *word() {                      ///< get next idiom
    static string tmp;              ///< tmp string holder
    if (!(fetch(tmp))) tmp.clear(); /// * input buffer exhausted?
    return (char*)tmp.c_str();
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
        PUSH(h0);                   ///> push string address
        PUSH(len);                  ///> push string length
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
#define UNNEST()     (vm.state = (IP=UINT(RS.pop())) ? HOLD : STOP)

void nest(VM& vm) {
    vm.state = NEST;                                 /// * activate VM
    while (vm.state==NEST && IP) {
        IU ix = IGET(IP);                            ///< fetched opcode, hopefully in register
//        printf("\033[%dm%02d[%4x]:%4x\033[0m", vm.id ? 38-vm.id : 37, vm.id, IP, ix);
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
             pstr(s);  IP += STRLEN(s));             /// * send to output console
        CASE(BRAN, IP = IGET(IP));                   /// * unconditional branch
        CASE(ZBRAN,                                  /// * conditional branch
             IP = POP() ? IP+sizeof(IU) : IGET(IP));
        CASE(VBRAN,
             PUSH(DALIGN(IP + sizeof(IU)));          /// * put param addr on tos
             if ((IP = IGET(IP))==0) UNNEST());      /// * jump target of does> if given
        CASE(DOES,
             IU *p = (IU*)MEM(LAST.pfa);             ///< memory pointer to pfa 
             *(p+1) = IP;                            /// * encode current IP, and bail
             UNNEST());
        CASE(FOR,  RS.push(POP()));                  /// * setup FOR..NEXT call frame
        CASE(DO,                                     /// * setup DO..LOOP call frame
             RS.push(SS.pop()); RS.push(POP()));
        CASE(KEY,  PUSH(key()); vm.state = IO);      /// * fetch single keypress
        OTHER(
            if (ix & EXT_FLAG) {                     /// * colon word?
                RS.push(IP);                         /// * setup call frame
                IP = ix & ~EXT_FLAG;                 /// * IP = word.pfa
            }
            else Code::exec(vm, ix));               ///> execute built-in word
        }
//        printf("\033[%dm   => IP=%4x, SS=%d, RS=%d, state=%d\033[0m\n", vm.id ? 38-vm.id : 37, IP, SS.idx, RS.idx, vm.state);
/*        
        U8 cpu = sched_getcpu();
        if (vm.id != cpu) {            /// check affinity
            vm.id = cpu; vm.xcpu++;
        }
*/
    }
}
///
///> CALL - inner-interpreter proxy (inline macro does not run faster)
///
void CALL(VM& vm, IU w) {
    if (IS_UDF(w)) {                   /// colon word
        RS.push(DU0);                  /// * terminating IP
        IP = dict[w].pfa;              /// setup task context
        nest(vm);
    }
    else dict[w].call(vm);             /// built-in word
}
///
///> Forth script loader
///
void load(VM &vm, const char* fn) {
//    printf("\n%s IP=%4x, VM=%d, load_dp=%d", fn, IP, VM, load_dp);
    load_dp++;                             /// * increment depth counter
    RS.push(vm.ip); RS.push(vm.state);     /// * save context
    vm.state = NEST;                       /// * +recursive
    forth_include(fn);                     /// * include file
    vm.state = static_cast<vm_state>(RS.pop());
    vm.ip = UINT(RS.pop());                /// * restore context
    --load_dp;                             /// * decrement depth counter
//    printf("   => IP=%4x, SS.idx=%d, rs.idx=%d, VM=%d, load_dp=%d\n", IP, SS.idx, rs.idx, VM, load_dp);
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
    CODE("base",    PUSH(((U8*)vm.base - MEM0)));
    CODE("decimal", put(BASE, *vm.base=10));
    CODE("hex",     put(BASE, *vm.base=16));
    CODE("bl",      PUSH(0x20));
    CODE("cr",      put(CR));
    CODE(".",       put(DOT,  POP()));
    CODE("u.",      put(UDOT, POP()));
    CODE(".r",      DU w = POP(); put(DOTR,  w, POP()));
    CODE("u.r",     DU w = POP(); put(UDOTR, w, POP()));
    CODE("type",    POP(); pstr((const char*)MEM(POP())));    // get string pointer
    IMMD("key",     if (vm.compile) add_w(KEY); else PUSH(key()));
    CODE("emit",    put(EMIT, POP()));
    CODE("space",   spaces(1));
    CODE("spaces",  spaces(POP()));
    /// @}
    /// @defgroup Literal ops
    /// @{
    CODE("[",       vm.compile = false);
    CODE("]",       vm.compile = true);
    IMMD("(",       scan(')'));
    IMMD(".(",      pstr(scan(')')));
    IMMD("\\",      scan('\n'));
    IMMD("s\"",     s_quote(vm, STR));
    IMMD(".\"",     s_quote(vm, DOTQ));
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
    CODE("i",       PUSH(RS[-1]));
    CODE("leave",   RS.pop(); RS.pop(); UNNEST());              // quit DO..LOOP
    IMMD("loop",    add_w(LOOP); add_iu(POP()));                // next ( here -- )
    /// @}
    /// @defgrouop return stack ops
    /// @{
    CODE(">r",      RS.push(POP()));
    CODE("r>",      PUSH(RS.pop()));
    CODE("r@",      PUSH(RS[-1]));                              // same as I (the loop counter)
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE(":",       vm.compile = def_word(word()));
    IMMD(";",       add_w(EXIT); vm.compile = false);
    CODE("exit",    UNNEST());                                  // early exit the colon word
    CODE("variable",def_word(word()); add_var(VAR));            // create a variable (default 0)
    CODE("constant",                                            // create a constant
         def_word(word());                                      // create a new word on dictionary
         add_var(LIT, POP()); add_w(EXIT));                     // add literal
    IMMD("immediate", dict[-1].attr |= IMM_ATTR);
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   IU w = POP(); CALL(vm, w));                  // execute word
    CODE("create", def_word(word()); add_var(VBRAN));           // vbran + offset field
    IMMD("does>",  add_w(DOES));
    IMMD("to",                                                  // alter the value of a constant, i.e. 3 to x
         IU w = vm.state==QUERY ? find(word()) : POP();         // constant addr
         if (!w) return;
         if (vm.compile) {
             add_var(LIT, (DU)w);                               // save addr on stack
             add_w(find("to"));                                 // encode to opcode
         }
         else {
             w = dict[w].pfa + sizeof(IU);                      // get memory addr to constant
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
         PUSH(w < USER_AREA ? (DU)IGET(w) : CELL(w)));          // check user area
    CODE("!",     IU w = POPI(); CELL(w) = POP(););             // n w --
    CODE(",",     DU n = POP(); add_du(n));                     // n -- , compile a cell
    CODE("cells", IU i = POPI(); PUSH(i * sizeof(DU)));         // n -- n'
    CODE("allot",                                               // n --
         IU n = POPI();                                         // number of bytes
         for (int i = 0; i < n; i+=sizeof(DU)) add_du(DU0));    // zero padding
    CODE("th",    IU i = POPI(); TOS += i * sizeof(DU));        // w i -- w'
    CODE("+!",    IU w = POPI(); CELL(w) += POP());             // n w --
    CODE("?",     IU w = POPI(); put(DOT, CELL(w)));            // w --
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
    CODE("recv",  IU t = POPI(); vm.recv(t, POPI()));           /// ( n tid -- v1 v2 .. vn ) fetch values from task's stack
    CODE("bcast", vm.bcast(POPI()));                            /// ( v1 v2 .. vn n -- )
    /// @}
#endif // DO_MULTITASK    
    /// @defgroup Debug ops
    /// @{
    CODE("abort", TOS = -DU1; SS.clear(); RS.clear());          // clear ss, rs
    CODE("here",  PUSH(HERE));
    CODE("'",     IU w = find(word()); if (w) PUSH(w));
    CODE(".s",    ss_dump(vm, true));
    CODE("depth", IU i = UINT(SS.idx); PUSH(i));
    CODE("r",     PUSH(RS.idx));
    CODE("words", words(*vm.base));
    CODE("see",
         IU w = find(word()); if (!w) return;
         pstr(": "); pstr(dict[w].name);
         if (IS_UDF(w)) see(dict[w].pfa, *vm.base);
         else           pstr(" ( built-ins ) ;");
         put(CR));
    CODE("dump",  U32 n = POPI(); mem_dump(POPI(), n, *vm.base));
    CODE("dict",  dict_dump(*vm.base));
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
    CODE("rnd",   PUSH(RND()));              // generate random number
    CODE("delay", delay(POPI()));
    CODE("included",                         // include external file
         POP();                              // string length, not used
         load(vm, (const char*)MEM(POP()))); // include external file
#if DO_WASM
    CODE("JS",    native_api());            // Javascript interface
#else  // !DO_WASM
    CODE("bye",   exit(0));
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
    DU2 n = (b==10)
        ? static_cast<DU2>(strtod(idiom, &p))
        : static_cast<DU2>(strtoll(idiom, &p, b));
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
        else CALL(vm, w);                /// * execute forth word
        return;
    }
    // try as a number
    int err = 0;
    DU  n   = parse_number(idiom, *vm.base, &err);
    if (err) {                           /// * not number
        pstr(idiom); pstr(" ?", CR);     ///> display error prompt
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
    static bool init = false;
    if (init) return;                    ///> check dictionary initilized

    dict  = new Code[E4_DICT_SZ];        ///< allocate dictionary
    pmem  = new U8[E4_PMEM_SZ];          ///< allocate parameter memory
    if (!dict.v || !pmem.v) {
        LOGS("forth_init memory allocation failed, bail...\n");
        exit(0);
    }
    MEM0  = &pmem[0];

    for (int i = pmem.idx; i < USER_AREA; i+=sizeof(IU)) {
        add_iu(0xffff);                  /// * reserved user area
    }
    dict_compile();                      ///< compile dictionary
    dict_validate();                     ///< collect XT0, and check xtoff range
    ///
    /// initialize VM0
    ///
    vm0.state = QUERY;
    vm0.base  = &pmem[0];
    *vm0.base = 10;
}
int forth_vm(const char *line, void(*hook)(int, const char*)) {
    auto time_up = []() {                /// * time slice up
        static long t0 = 0;              /// * real-time support, 10ms = 100Hz
        long t1 = millis();              ///> check timing
        return (t1 >= t0) ? (t0 = t1 + t0, 1) : 0;
    };
    fout_setup();                        ///< serial output hook up

    VM &vm = vm0;
    bool resume =                        ///< check VM resume status
        (vm.state==HOLD || vm.state==IO);
    
    if (resume) vm.ip = UINT(RS.pop());  /// * restore context
    else fin_setup(line);                ///> refresh buffer if not resuming
    
    string idiom;
    while (resume || fetch(idiom)) {     /// * parse a word
        if (resume) nest(vm);                      /// * resume task
        else        forth_core(vm, idiom.c_str()); /// * send to Forth core
        resume = vm.state==HOLD;
        if (resume && time_up()) break;  ///> multi-threading support
    }
    bool yield = vm.state==HOLD || vm.state==IO;   /// * yield to other tasks

    if (yield)            RS.push(IP);   /// * save context
    else if (!vm.compile) ss_dump(vm);   /// * optionally display stack contents
    
    return yield;
}
///====================================================================
///
///@name IO functions
///@brief - C++ standard template libraries used for core IO functions
///@note
///   * we use STL for its syntaxial convinence, but
///   * if it takes too much memory for target MCU,
///   * these functions can be replaced with our own implementation
///@{
#include <iomanip>                  /// setbase, setw, setfill
#include <sstream>                  /// iostream, stringstream
using namespace std;                /// default to C++ standard template library
istringstream   fin;                ///< forth_in
ostringstream   fout;               ///< forth_out
string          pad;                ///< input string buffer
void (*fout_cb)(int, const char*);  ///< forth output callback function (see ENDL macro)
///====================================================================
void fin_setup(const char *line) {
    fout.str("");                   /// * clean output buffer
    fin.clear();                    /// * clear input stream error bit if any
    fin.str(line);                  /// * reload user command into input stream
}
void fout_setup(void (*hook)(int, const char*)) {
    auto cb = [](int, const char *rst) { printf("%s", rst); };
    fout_cb = hook ? hook : cb;     ///< serial output hook up
}
char key()         { return 0; }
char *scan(char c) { getline(fin, pad, c); return (char*)pad.c_str(); }
int  fetch(string &idiom) { return !(fin >> idiom)==0; }

void spaces(int n) { for (int i = 0; i < n; i++) fout << " "; }
void put(io_op op, DU v, DU v2) {
    switch (op) {
    case BASE:  fout << setbase(UINT(v));               break;
    case CR:    fout << ENDL;                           break;
    case DOT:   fout << v << " ";                       break;
    case UDOT:  fout << static_cast<U32>(v) << " ";     break;
    case DOTR:  fout << setw(UINT(v)) << v2;            break;
    case UDOTR: fout << setw(UINT(v))
                     << static_cast<U32>(v2);           break;
    case EMIT:  { char b = (char)UINT(v); fout << b; }  break;
    case SPCS:  spaces(UINT(v));                        break;
    default:    fout << "unknown io_op=" << op << ENDL; break;
    }
}
void pstr(const char *str, io_op op) {
    fout << str;
    if (op==CR) fout << ENDL;
}
///@}
///@name Debug functions
///@{
#define TONAME(w) (dict[w].pfa - STRLEN(dict[w].name))
///
///> convert pfa to dictionary entry index
///
int pfa2didx(IU ix) {                          ///> reverse lookup
    if (IS_PRIM(ix)) return (int)ix;           ///> primitives
    IU pfa = ix & ~EXT_FLAG;                   ///> pfa (mask colon word)
    for (int i = dict.idx - 1; i > 0; --i) {
        Code &c = dict[i];
        if (IS_UDF(i)) {                       /// * user defined words
            if ((ix & EXT_FLAG) && pfa == c.pfa) return i;
        }
        else if (pfa == c.xtoff()) return i;   /// * built-in words
    }
    return 0;                                  /// * not found
}
///
///> calculate number of variables by given pfa
///
int  pfa2allot(IU pfa) {
    IU  op = IGET(pfa);                        ///< fetch opcode
    if (op != VAR && op != VBRAN) return 0;

    IU  i0 = pfa2didx(pfa | EXT_FLAG);         ///< index to this word
    if (!i0) return 0;
    
    IU  p1 = (i0+1) < dict.idx ? TONAME(i0+1) : HERE;
    pfa += (op==VBRAN ? 2 : 1) * sizeof(IU);   ///> get parameter field
    
    return p1 - DALIGN(pfa);                   ///< calc bytes allot
}
///
///> display opcode and literal
///
void to_s(IU w, U8 *ip, int base) {
#if CC_DEBUG
    fout << setbase(16) << "( ";
    fout << setfill('0') << setw(4) << (ip - MEM0);       ///> addr
    fout << '[' << setfill(' ') << setw(4) << w << ']';   ///> word ref
    fout << " ) " << setbase(base);
#endif // CC_DEBUG
    
    ip += sizeof(IU);                  ///> calculate next ip
    switch (w) {
    case LIT:
        w = (IU)(ip - MEM0);
        fout << *(DU*)MEM(DALIGN(w)) << " ( lit )"; break;
    case STR:  fout << "s\" " << (char*)ip << '"';  break;
    case DOTQ: fout << ".\" " << (char*)ip << '"';  break;
    case VAR:
    case VBRAN: {
        int pfa = ip - sizeof(IU) - MEM0;
        int n   = pfa2allot(pfa);
        pfa += sizeof(IU) * (w==VBRAN ? 2 : 1);
        for (int i = 0, a=DALIGN(pfa); i < n; i+=sizeof(DU)) {
            fout << *(DU*)MEM(a + i) << ' ';
        }
    }                                               /// no break, fall through
    default: Code &c = DICT(w); fout << c.name;     break;
    }
    switch (w) {
    case NEXT: case LOOP:
    case BRAN: case ZBRAN: case VBRAN:             ///> display jmp target
        fout << " $" << setfill('0') << setbase(16)
             << setw(4) << *(IU*)ip;
        break;
    default: /* do nothing */ break;
    }
    fout << setfill(' ') << setw(-1);   ///> restore output format settings
}
///
///> Forth disassembler
///
void see(IU pfa, int base) {
    U8 *ip = MEM(pfa);
    while (1) {
        IU w = pfa2didx(*(IU*)ip);      ///> fetch word index by pfa
        if (!w) break;                  ///> loop guard
        
        fout << ENDL; fout << "  ";     /// * indent
        to_s(w, ip, base);              /// * display opcode
        if (w==EXIT || w==VAR) return;  /// * end of word
        
        ip += sizeof(IU);               ///> advance ip (next opcode)
        switch (w) {                    ///> extra bytes to skip
        case LIT:
            w  = (IU)(ip - MEM0);
            ip = MEM(DALIGN(w) + sizeof(DU));            break;
        case STR:   case DOTQ:  ip += STRLEN((char*)ip); break;
        case BRAN:  case ZBRAN:
        case NEXT:  case LOOP:  ip += sizeof(IU);        break;
        case VBRAN:
            w  = *(IU*)ip; if (w==0) return;  ///> skip if no jmp target
            ip = MEM(w);                                 break;
        }
    }
}
///
///> display built-in and user defined words in dictionary
///
void words(int base) {
    const int WIDTH = 60;
    int sz = 0;
    fout << setbase(10);
    for (int i=0; i<dict.idx; i++) {
        const char *nm = dict[i].name;
        const int  len = strlen(nm);
#if CC_DEBUG > 1
        if (nm[0]) {
#else  //  CC_DEBUG > 1
        if (nm[len-1] != ' ') {
#endif // CC_DEBUG > 1
            sz += len + 2;
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
///
///> show data stack content
///
void ss_dump(VM &vm, bool forced) {
    if (load_dp) return;                  /// * skip when including file
#if DO_WASM    
    if (!forced) { fout << "ok" << ENDL; return; }
#endif // DO_WASM
    static char buf[34];                  ///< static buffer
    auto rdx = [&vm](DU v, int b) {       ///< display v by radix
#if USE_FLOAT
        DU t, f = modf(v, &t);            ///< integral, fraction
        if (ABS(f) > DU_EPS) {            /// * if != 0 
            sprintf(buf, "%0.6g", v);
            return buf;
        }
#endif // USE_FLOAT
        int i = 33;  buf[i]='\0';         /// * C++ can do only base=8,10,16
        int dec = *vm.base==10;
        U32 n   = dec ? UINT(ABS(v)) : UINT(v);  ///< handle negative
        do {                              ///> digit-by-digit
            U8 d = (U8)MOD(n,b);  n /= b;
            buf[--i] = d > 9 ? (d-10)+'a' : d+'0';
        } while (n && i);
        if (dec && v < DU0) buf[--i]='-';
        return &buf[i];
    };
    SS.push(TOS);
    for (int i=0; i<SS.idx; i++) {
        fout << rdx(SS[i], *vm.base) << ' ';
    }
    TOS = SS.pop();
    fout << "-> ok" << ENDL;
}
///
///> dump memory content range from [p0, p0+sz)
///
void mem_dump(U32 p0, IU sz, int base) {
    fout << setbase(16) << setfill('0');
    for (IU i=ALIGN16(p0); i<=ALIGN16(p0+sz); i+=16) {
        fout << setw(4) << i << ": ";
        for (int j=0; j<16; j++) {
            U8 c = pmem[i+j];
            fout << setw(2) << (int)c << (MOD(j,4)==3 ? " " : "");
        }
        for (int j=0; j<16; j++) {   // print and advance to next byte
            U8 c = pmem[i+j] & 0x7f;
            fout << (char)((c==0x7f||c<0x20) ? '_' : c);
        }
        fout << ENDL;
        yield();
    }
    fout << setbase(base) << setfill(' ');
}
///@}
///@name WASM/Emscripten ccall interfaces
///@{
///
///> display dictionary attributes
///
void dict_dump(int base) {
    fout << setbase(16) << setfill('0') << "XT0=" << Code::XT0 << ENDL;
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        bool ud = IS_UDF(i);
        fout << setfill('0') << setw(3) << i
             << "> name=" << setw(8) << (UFP)c.name
             << ", xt="   << setw(8) << ((UFP)c.xt & MSK_ATTR)
             << ", attr=" << (c.attr & 0x3)
             << (ud ? ", pfa=" : ", off=")
             << setw(4)   << (ud ? c.pfa : c.xtoff())
             << " "       << c.name << ENDL;
    }
    fout << setbase(base) << setfill(' ') << setw(-1);
}
///
///> Javascript web worker message sender
///
#if DO_WASM
EM_JS(void, js_call, (const char *ops), {
        const req = UTF8ToString(ops).split(/\\s+/);
        const wa  = wasmExports;
        const mem = wa.vm_mem();
        let msg = [Date.now()], tfr = [];       ///< t0 anchor for performance
        for (let i=0, n=req.length; i < n; i++) {
            if (req[i]=='p') {
                const a = new Float32Array(     ///< create a buffer ref
                    wa.memory.buffer,           /// * WASM ArrayBuffer
                    mem + (req[i+1]|0),         /// * pointer address
                    req[i+2]|0                  /// * length
                );
                i += 2;                         /// *  skip over addr, len
                const t = new Float64Array(a);  ///< create a transferable
                msg.push(t);                    /// * which speeds postMessage
                tfr.push(t.buffer);             /// * from 20ms => 5ms
            }
            else msg.push(req[i]);
        }
        postMessage(['js', msg], tfr);
});
///
///> Javascript Native Interface, before passing to js_call()
///
///  String substitude similar to printf
///    %d - integer
///    %f - float
///    %x - hex
///    %s - string
///    %p - pointer (memory block)
///
void native_api() {                        ///> ( n addr u -- )
    stringstream n;
    auto t2s = [&n](char c) {              ///< template to string
        n.str("");                         /// * clear stream
        switch (c) {
        case 'd': n << UINT(POP());                break;
        case 'f': n << (DU)POP();                  break;
        case 'x': n << "0x" << hex << UINT(POP()); break;
        case 's': POP(); n << (char*)MEM(POP());   break;  /// also handles raw stream
        case 'p':
            n << "p " << UINT(POP());
            n << ' '  << UINT(POP());              break;
        default : n << c << '?';                   break;
        }
        return n.str();
    };
    POP();                                 /// * strlen, not used
    pad.clear();                           /// * borrow PAD for string op
    pad.append((char*)MEM(POP()));         /// copy string on stack
    for (size_t i=pad.find_last_of('%');   ///> find % from back
         i!=string::npos;                  /// * until not found
         i=pad.find_last_of('%',i?i-1:0)) {
        if (i && pad[i-1]=='%') {          /// * double %%
            pad.replace(--i,1,"");         /// * drop one %
        }
        else pad.replace(i, 2, t2s(pad[i+1]));
    }
    js_call(pad.c_str());    /// * pass to Emscripten function above
}
#endif // DO_WASM
