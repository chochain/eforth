///
/// @file
/// @brief eForth - C++ vector-based object-threaded implementation
///
///====================================================================
#include <sstream>                     /// iostream, stringstream
#include <cstring>
#include "ceforth.h"

using namespace std;
///
///> Forth VM state variables
///
FV<Code*> dict;                        ///< Forth dictionary
Code      *last;                       ///< cached dict[-1]
///
///> macros to reduce verbosity (but harder to single-step debug)
///
#define VAR(i_w)     (*(dict[(int)((i_w) & 0xffff)]->pf[0]->q.data()+((i_w) >> 16)))
#define STR(i_w)     (                                  \
        EQ(i_w, UINT(-DU1))                             \
        ? vm.pad.c_str()                                \
        : dict[(i_w) & 0xffff]->pf[(i_w) >> 16]->name   \
        )
#define BASE         ((U8*)&VAR(vm.id << 16))
#define DICT_PUSH(c) (dict.push(last=(Code*)(c)))
#define DICT_POP()   (dict.pop(), last=dict[-1])
#define ADD_W(w)     (last->append(w))
#define BTGT()       (dict[-2]->pf[-1])      /** branching target   */
#define BRAN(p)      ((p).merge(last->pf))   /** add branching code */
#define NEST(pf)     for (auto w : (pf)) w->nest(vm)
#define UNNEST()     throw 0
///
///> Forth Dictionary Assembler
/// @note:
///    1. Dictionary construction sequence
///       * Code rom[] in statically build in compile-time
///       * vector<Code*> dict is populated in forth_init, i.e. first thing in main()
///    2. Macro CODE/IMMD use __COUNTER__ for array token/index can potetially
///       make the dictionary static but need to be careful the
///       potential issue comes with it.
///    3. a degenerated lambda becomes a function pointer
///
const Code rom[] = {               ///< Forth dictionary
    CODE("bye",    t_pool_stop(); exit(0)),   // exit to OS
    ///
    /// @defgroup ALU ops
    /// @{
    CODE("+",      TOS += SS.pop()),
    CODE("-",      TOS =  SS.pop() - TOS),
    CODE("*",      TOS *= SS.pop()),
    CODE("/",      TOS =  SS.pop() / TOS),
    CODE("mod",    TOS =  MOD(SS.pop(), TOS)),
    CODE("*/",     TOS =  SS.pop() * SS.pop() / TOS),
    CODE("/mod",   DU n = SS.pop(); DU t = TOS;
                   DU m = MOD(n, t);
                   SS.push(m); TOS = UINT(n / t)),
    CODE("*/mod",  DU2 n = (DU2)SS.pop() * SS.pop(); DU2 t=TOS;
                   DU2 m = MOD(n, t);
                   SS.push((DU)m); TOS = UINT(n / t)),
    CODE("and",    TOS = UINT(TOS) & UINT(SS.pop())),
    CODE("or",     TOS = UINT(TOS) | UINT(SS.pop())),
    CODE("xor",    TOS = UINT(TOS) ^ UINT(SS.pop())),
    CODE("abs",    TOS =  ABS(TOS)),
    CODE("negate", TOS =  -TOS),
    CODE("invert", TOS =  ~UINT(TOS)),
    CODE("rshift", TOS =  UINT(SS.pop()) >> UINT(TOS)),
    CODE("lshift", TOS =  UINT(SS.pop()) << UINT(TOS)),
    CODE("max",    DU n=SS.pop(); TOS = (TOS>n) ? TOS : n),
    CODE("min",    DU n=SS.pop(); TOS = (TOS<n) ? TOS : n),
    CODE("2*",     TOS *= 2),
    CODE("2/",     TOS /= 2),
    CODE("1+",     TOS += 1),
    CODE("1-",     TOS -= 1),
#if USE_FLOAT
    CODE("int",
        TOS = TOS < DU0 ? -DU1 * UINT(-TOS) : UINT(TOS)), // float to int
#endif // USE_FLOAT
    /// @}
    /// @defgroup Logic ops
    /// @{
    CODE("0=",     TOS = BOOL(ZEQ(TOS))),
    CODE("0<",     TOS = BOOL(LT(TOS, DU0))),
    CODE("0>",     TOS = BOOL(GT(TOS, DU0))),
    CODE("=",      TOS = BOOL(EQ(SS.pop(), TOS))),
    CODE(">",      TOS = BOOL(GT(SS.pop(), TOS))),
    CODE("<",      TOS = BOOL(LT(SS.pop(), TOS))),
    CODE("<>",     TOS = BOOL(!EQ(SS.pop(), TOS))),
    CODE(">=",     TOS = BOOL(!LT(SS.pop(), TOS))),
    CODE("<=",     TOS = BOOL(!GT(SS.pop(), TOS))),
    CODE("u<",     TOS = BOOL(UINT(SS.pop()) < UINT(TOS))),
    CODE("u>",     TOS = BOOL(UINT(SS.pop()) > UINT(TOS))),
    /// @}
    /// @defgroup Data Stack ops
    /// @brief - opcode sequence can be changed below this line
    /// @{
    CODE("dup",    PUSH(TOS)),
    CODE("drop",   TOS=SS.pop()),  // note: SS.pop() != POP()
    CODE("swap",   DU n = SS.pop(); PUSH(n)),
    CODE("over",   PUSH(SS[-2])),
    CODE("rot",    DU n = SS.pop(); DU m = SS.pop(); SS.push(n); PUSH(m)),
    CODE("-rot",   DU n = SS.pop(); DU m = SS.pop(); PUSH(m);  PUSH(n)),
    CODE("pick",   TOS = SS[-TOS]),
    CODE("nip",    SS.pop()),
    CODE("?dup",   if (TOS != DU0) PUSH(TOS)),
    /// @}
    /// @defgroup Data Stack ops - double
    /// @{
    CODE("2dup",   PUSH(SS[-2]); PUSH(SS[-2])),
    CODE("2drop",  SS.pop(); TOS=SS.pop()),
    CODE("2swap",  DU n = SS.pop(); DU m = SS.pop(); DU l = SS.pop();
                   SS.push(n); PUSH(l); PUSH(m)),
    CODE("2over",  PUSH(SS[-4]); PUSH(SS[-4])),
    /// @}
    /// @defgroup Return Stack ops
    /// @{
    CODE(">r",     RS.push(POP())),
    CODE("r>",     PUSH(RS.pop())),
    CODE("r@",     PUSH(RS[-1])),
    /// @}
    /// @defgroup IO ops
    /// @{
    CODE("base",   PUSH(vm.id << 16)),   // dict[0]->pf[0]->q[id] used for base
    CODE("decimal",dot(RDX, *BASE=10)),
    CODE("hex",    dot(RDX, *BASE=16)),
    CODE("bl",     PUSH(0x20)),
    CODE("cr",     dot(CR)),
    CODE(".",      dot(DOT,  POP())),
    CODE("u.",     dot(UDOT, POP())),
    CODE(".r",     IU w = POPI(); dotr(w, POP(), *BASE)),
    CODE("u.r",    IU w = POPI(); dotr(w, POP(), *BASE, true)),
    CODE("type",   POP(); U32 i_w=POPI(); pstr(STR(i_w))),
    CODE("key",    PUSH(key())),
    CODE("emit",   dot(EMIT, POP())),
    CODE("space",  dot(SPCS, DU1)),
    CODE("spaces", dot(SPCS, POP())),
    /// @}
    /// @defgroup Literal ops
    /// @{
    IMMD("(",      scan(')')),
    IMMD(".(",     pstr(scan(')'))),
    IMMD("\\",     scan('\n')),
    IMMD("s\"",
         string s = word('"').substr(1);
         if (vm.compile) {
             ADD_W(new Str(s, last->token, last->pf.size()));
         }
         else {
             vm.pad = s;                             // keep string on pad
             PUSH(-DU1); PUSH(s.length());           // -1 = pad, len
         }),
    IMMD(".\"",
         string s = word('"').substr(1);
         ADD_W(new Str(s))),
    /// @}
    /// @defgroup Branching ops
    /// @brief - if...then, if...else...then
    ///     dict[-2]->pf[0,1,2,...,-1] as *last
    ///                              \--->pf[...] if  <--+ merge
    ///                               \-->p1[...] else   |
    ///     dict[-1]->pf[...] as *tmp -------------------+
    /// @{
    IMMD("if",
         ADD_W(new Bran(_if));
         DICT_PUSH(new Tmp())),
    IMMD("else",
         Code *b = BTGT();
         BRAN(b->pf);
         b->stage = 1),
    IMMD("then",
         Code *b = BTGT();
         int  s  = b->stage;                   ///< branching state
         if (s==0) {
             BRAN(b->pf);                      /// * if.{pf}.then
             DICT_POP();
         }
         else {                                /// * else.{p1}.then, or
             BRAN(b->p1);                      /// * then.{p1}.next
             if (s==1) DICT_POP();             /// * if..else..then
         }),
    /// @}
    /// @defgroup Loops
    /// @brief  - begin...again, begin...f until, begin...f while...repeat
    /// @{
    IMMD("begin",
         ADD_W(new Bran(_begin));
         DICT_PUSH(new Tmp())),                /// as branch target
    IMMD("while",
         Code *b = BTGT();
         BRAN(b->pf);                          /// * begin.{pf}.f.while
         b->stage = 2),
    IMMD("repeat",
         Code *b = BTGT();
         BRAN(b->p1); DICT_POP()),             /// * while.{p1}.repeat
    IMMD("again",
         Code *b = BTGT();
         BRAN(b->pf); DICT_POP();              /// * begin.{pf}.again
         b->stage = 1),
    IMMD("until",
         Code *b = BTGT();
         BRAN(b->pf); DICT_POP()),             /// * begin.{pf}.f.until
    /// @}
    /// @defgrouop FOR loops
    /// @brief  - for...next, for...aft...then...next
    /// @{
    IMMD("for",
         ADD_W(new Bran(_tor));
         ADD_W(new Bran(_for));
         DICT_PUSH(new Tmp())),                /// as branch target
    IMMD("aft",
         Code *b = BTGT();
         BRAN(b->pf);                          /// * for.{pf}.aft
         b->stage = 3),
    IMMD("next",
         Code *b = BTGT();
         BRAN(b->stage==0 ? b->pf : b->p2);    /// * for.{pf}.next, or
         DICT_POP()),                          /// * then.{p2}.next
    /// @}
    /// @defgrouop DO loops
    /// @brief  - do...loop, do..leave..loop
    /// @{
    IMMD("do",
         ADD_W(new Bran(_tor2));               ///< ( limit first -- )
         ADD_W(new Bran(_loop));
         DICT_PUSH(new Tmp())),
    CODE("i",      PUSH(RS[-1])),
    CODE("leave",
         RS.pop(); RS.pop(); UNNEST()), /// * exit loop
    IMMD("loop",
         Code *b = BTGT();
         BRAN(b->pf);                   /// * do.{pf}.loop
         DICT_POP()),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("[",      vm.compile = false),
    CODE("]",      vm.compile = true),
    CODE(":",
         DICT_PUSH(new Code(word()));   // create new word
         vm.compile = true),
    IMMD(";", vm.compile = false),
    CODE("constant",
         DICT_PUSH(new Code(word()));
         Code *w = ADD_W(new Lit(POP()));
         w->pf[0]->token = w->token),
    CODE("variable",
         DICT_PUSH(new Code(word()));
         Code *w = ADD_W(new Var(DU0));
         w->pf[0]->token = w->token),
    CODE("immediate", last->immd = 1),
    CODE("exit",   UNNEST()),           // -- (exit from word)
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   dict[POPI()]->nest(vm)),           // w --
    CODE("create",
         DICT_PUSH(new Code(word()));
         Code *w = ADD_W(new Var(DU0));
         w->pf[0]->token = w->token;
         w->pf[0]->q.pop()),
    IMMD("does>",
         ADD_W(new Bran(_does));
         last->pf[-1]->token = last->token),          // keep WP
    CODE("to",                                        // n --
         Code *w=find(word()); if (!w) return;
         VAR(w->token) = POP()),                      // update value
    CODE("is",                                        // w -- 
         DICT_PUSH(new Code(word(), false));          // create word
         int w = POP();                               // like this word
         last->xt = dict[w]->xt;                      // if primitive
         last->pf = dict[w]->pf),                     // or colon word
    /// @}
    /// @defgroup Memory Access ops
    /// @{
    CODE("@",       U32 i_w = POPI(); PUSH(VAR(i_w))),           // a -- n
    CODE("!",       U32 i_w = POPI(); VAR(i_w) = POP()),         // n a -- 
    CODE("+!",      U32 i_w = POPI(); VAR(i_w) += POP()),
    CODE("?",       U32 i_w = POPI(); dot(DOT, VAR(i_w))),
    CODE(",",       last->pf[0]->q.push(POP())),
    CODE("cells",   { /* for backward compatible */ }),           // array index, inc by 1
    CODE("allot",   U32 n = POPI();                              // n --
                    for (U32 i=0; i<n; i++) last->pf[0]->q.push(DU0)),
    ///> Note:
    ///>   allot allocate elements in a word's q[] array
    ///>   to access, both indices to word itself and to q array are needed
    ///>   'th' a word that compose i_w, a 32-bit value, the 16 high bits
    ///>   serves as the q index and lower 16 lower bit as word index
    ///>   so a variable (array with 1 element) can be access as usual
    ///>
    CODE("th",      U32 i = POPI() << 16; TOS = UINT(TOS) | i),  // w i -- i_w
    /// @}
#if DO_MULTITASK
    /// @defgroup Multitasking ops
    /// @}
    CODE("task",                                                // w -- task_id
         IU w = POPI();                                         ///< dictionary index
         if (dict[w]->xt) pstr("  ?colon word only\n");
         else PUSH(task_create(w))),                            /// create a task starting on pfa
    CODE("rank",    PUSH(vm.id)),                               /// ( -- n ) thread id
    CODE("start",   task_start(POPI())),                        /// ( task_id -- )
    CODE("join",    vm.join(POPI())),                           /// ( task_id -- )
    CODE("lock",    vm.io_lock()),                              /// wait for IO semaphore
    CODE("unlock",  vm.io_unlock()),                            /// release IO semaphore
    CODE("send",    IU t = POPI(); vm.send(t, POPI())),         /// ( v1 v2 .. vn n tid -- ) pass values onto task's stack
    CODE("recv",    vm.recv()),                                 /// ( -- v1 v2 .. vn ) waiting for values passed by sender
    CODE("bcast",   vm.bcast(POPI())),                          /// ( v1 v2 .. vn -- )
    CODE("pull",    IU t = POPI(); vm.pull(t, POPI())),         /// ( tid n -- v1 v2 .. vn )
    /// @}
#endif // DO_MULTITASK    
    /// @defgroup Debug ops
    /// @{
    CODE("abort",   TOS = -DU1; SS.clear(); RS.clear()),        // clear ss, rs
    CODE("here",    PUSH(last->token)),
    CODE("'",       Code *w = find(word()); if (w) PUSH(w->token)),
    CODE(".s",      ss_dump(vm, true)),  // dump parameter stack
    CODE("words",   words(*vm.base)),    // display word lists
    CODE("see",
         Code *w = find(word());
         if (w) see(w, *vm.base);
         dot(CR)),
    CODE("dict",    dict_dump(*vm.base)),// display dictionary
    CODE("dump",    IU w1 = POPI(); mem_dump(POPI(), w1, *vm.base)),
    CODE("depth",   PUSH(SS.size())),    // data stack depth
    /// @}
    /// @defgroup OS ops
    /// @{
    CODE("mstat",   mem_stat()),         // display memory stat
    CODE("ms",      PUSH(millis())),     // get system clock in msec
    CODE("rnd",     PUSH(RND())),        // get a random number
    CODE("delay",   delay(POPI())),      // n -- delay n msec
    CODE("included",
         POP(); U32 i_w = POPI(); load(vm, STR(i_w))),
    CODE("forget",
         Code *w = find(word()); if (!w) return;
         int   t = max((int)w->token, find("boot")->token + 1);
         for (int i=dict.size(); i>t; i--) DICT_POP()),
    CODE("boot",
         int t = find("boot")->token + 1;
         for (int i=dict.size(); i>t; i--) DICT_POP())
};
///====================================================================
///
///> Code Class constructors
///
Code::Code(const char *s, const char *d, XT fp, U32 a)  ///> primitive word
    : name(s), desc(d), xt(fp), attr(a) {}
Code::Code(string s, bool n) {           ///< new colon word
    Code *w = find(s);                   /// * scan the dictionary
    name  = (new string(s))->c_str();
    desc  = "";
    xt    = w ? w->xt : NULL;
    token = n ? dict.size() : 0;
    if (n && w) pstr("reDef?");          /// * warn word redefined
}
///
///> Forth inner interpreter
///
void Code::nest(VM &vm) {
    vm.state = NEST;
    if (xt) { xt(vm, *this); return; }   /// * run primitive word
    for (Iter c = pf.begin(); c != pf.end(); c++) {
        try         { (*c)->nest(vm); }  /// * execute recursively
        catch (...) { break; }
//        VM_LOG(&vm, "%-3x => RS=%ld, SS=%ld %s", (int)(c - pf.begin()), vm.rs.size(), vm.ss.size(), (*c)->name);
    }
}
///====================================================================
///
///> Primitive Functions
///
void _str(VM &vm, Code &c)  {
    if (!c.token) pstr(c.name);
    else { PUSH(c.token); PUSH(strlen(c.name)); }
}
void _lit(VM &vm, Code &c)  { PUSH(c.q[0]);  }
void _var(VM &vm, Code &c)  { PUSH(c.token); }
void _tor(VM &vm, Code &c)  { RS.push(POP()); }
void _tor2(VM &vm, Code &c) { RS.push(SS.pop()); RS.push(POP()); }
void _if(VM &vm, Code &c)   { NEST(POP() ? c.pf : c.p1); }
void _begin(VM &vm, Code &c){    ///> begin.while.repeat, begin.until
    int b = c.stage;             ///< branching state
    while (true) {
        NEST(c.pf);                            /// * begin..
        if (b==0 && POP()!=0) break;           /// * ..until
        if (b==1)             continue;        /// * ..again
        if (b==2 && POP()==0) break;           /// * ..while..repeat
        NEST(c.p1);
    }
}
void _for(VM &vm, Code &c) {     ///> for..next, for..aft..then..next
    int b = c.stage;                           /// * kept in register
    try {
        do {
            NEST(c.pf);
        } while (b==0 && (RS[-1]-=1) >=0);     /// * for..next only
        while (b) {                            /// * aft
            NEST(c.p2);                        /// * then..next
            if ((RS[-1]-=1) < 0) break;        /// * decrement counter
            NEST(c.p1);                        /// * aft..then
        }
        RS.pop();
    }
    catch (...) { RS.pop(); }                // handle EXIT
}
void _loop(VM &vm, Code &c) {                ///> do..loop
    try { 
        do {
            NEST(c.pf);
        } while ((RS[-1]+=1) < RS[-2]);      // increment counter
        RS.pop(); RS.pop();
    }
    catch (...) {}                           // handle LEAVE
}
void _does(VM &vm, Code &c) {
    bool hit = false;
    for (auto w : dict[c.token]->pf) {
        if (hit) ADD_W(w);                   // copy rest of pf
        if (STRCMP(w->name, "does>")==0) hit = true;
    }
    UNNEST();                                // exit caller
}
///====================================================================
///
///> Forth outer interpreter
///
Code *find(string s) {      ///> scan dictionary, last to first
    for (int i = dict.size() - 1; i >= 0; --i) {
        if (STRCMP(s.c_str(), dict[i]->name)==0) return dict[i];
    }
    return NULL;            /// * word not found
}

DU parse_number(string idiom, int b) {
    const char *cs = idiom.c_str();
    switch (*cs) {                    ///> base override
    case '%': b = 2;  cs++; break;
    case '&':
    case '#': b = 10; cs++; break;
    case '$': b = 16; cs++; break;
    }
    char *p;
    errno = 0;                       ///> clear overflow flag
#if DU==float
    DU n = (b==10)
        ? static_cast<DU>(strtof(cs, &p))
        : static_cast<DU>(strtol(cs, &p, b));
#else
    DU n = static_cast<DU>(strtol(cs, &p, b));
#endif
    if (errno || *p != '\0') throw runtime_error("");
    return n;
}

void forth_core(VM &vm, string idiom) {
    Code *w = find(idiom);            /// * search through dictionary
    if (w) {                          /// * word found?
        if (vm.compile && !w->immd)   /// * are we compiling new word?
            ADD_W(w);                 /// * append word ptr to it
        else w->nest(vm);             /// * execute forth word
        return;
    }
    DU  n = parse_number(idiom, *vm.base);  ///< try as a number
    if (vm.compile)                   /// * are we compiling new word?
        ADD_W(new Lit(n));            /// * append numeric literal to it
    else PUSH(n);                     /// * add value to data stack
}
///====================================================================
///
///> Forth VM - interface to outside world
///
void forth_init() {
    static bool init = false;         ///< singleton
    if (init) return;
    
    const int sz = (int)(sizeof(rom))/(sizeof(Code));
    dict.reserve(sz * 2);             /// * pre-allocate vector
    for (const Code &c : rom) {
        DICT_PUSH(&c);
    }

    t_pool_init();                    /// * initialize thread pool
    uvar_init();                      /// * initialize user area
    VM &vm0   = vm_get(0);            ///< main thread
    vm0.state = QUERY;
}

int forth_vm(const char *line, void(*hook)(int, const char*)) {
    VM &vm = vm_get(0);               ///< main thread
    fout_setup(hook);                 /// * init output stream
    fin_setup(line);                  /// * refresh buffer if not resuming
    
    string idiom;
    while (fetch(idiom)) {            /// * parse a word
        try {
            forth_core(vm, idiom);    /// * send to Forth core
        }
        catch (exception &e) {
            pstr(idiom.c_str());
            pstr("?"); pstr(e.what(), CR);
            vm.compile = false;
            scan('\n');                /// * exhaust input line
        }
    }
    if (!vm.compile) ss_dump(vm);
    
    return 0;
}
