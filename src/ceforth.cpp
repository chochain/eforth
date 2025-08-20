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
        ZEQ(i_w)                                        \
        ? vm.pad.c_str()                                \
        : dict[(i_w) & 0xffff]->pf[(i_w) >> 16]->name   \
        )
#define BASE         ((U8*)&VAR((vm.id << 16) | BASE_NODE))
#define DICT_PUSH(c) (dict.push(last=(Code*)(c)))
#define DICT_POP()   (dict.pop(), last=dict[-1])
#define ADD_W(w)     (last->append((Code*)w))
#define BTGT()       ((Bran*)dict[-2]->pf[-1])      /** branching target   */
#define BRAN(p)      ((p).merge(last->pf))          /** add branching code */
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
const Code rom[] {               ///< Forth dictionary
    CODE("bye",    forth_quit()),
    ///
    /// @defgroup ALU ops
    /// @{
    CODE("+",      TOS += SS.pop()),
    CODE("-",      TOS =  SS.pop() - TOS),
    CODE("*",      TOS *= SS.pop()),
    CODE("/",      TOS =  SS.pop() / TOS),
    CODE("mod",    TOS =  INT(MOD(SS.pop(), TOS))),          /// ( a b -- c ) c integer, see fmod
    CODE("*/",     TOS =  (DU2)SS.pop() * SS.pop() / TOS),   /// ( a b c -- d ) d=a*b / c (float)
    CODE("/mod",   DU  n = SS.pop();                         /// ( a b -- c d ) c=a%b, d=int(a/b)
                   DU  t = TOS;
                   DU  m = MOD(n, t);
                   SS.push(m); TOS = INT(n / t)),
    CODE("*/mod",  DU2 n = (DU2)SS.pop() * SS.pop();         /// ( a b c -- d e ) d=(a*b)%c, e=(a*b)/c
                   DU2 t = TOS;
                   DU  m = MOD(n, t);
                   SS.push(m); TOS = INT(n / t)),
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
    CODE("fmod",   TOS = MOD(SS.pop(), TOS)),             /// -3.5 2 fmod => -1.5
    CODE("f>s",    TOS = INT(TOS)),                       /// 1.9 => 1, -1.9 => -1
#else
    CODE("f>s",     /* do nothing */),
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
    CODE("drop",   TOS=SS.pop()),  /// note: SS.pop() != POP()
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
    CODE("base",   PUSH((vm.id << 16) | BASE_NODE)),   /// dict[0]->pf[0]->q[id] used for base
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
         const char *s = word('"'); if (!s) return;
         if (vm.compile) {
             ADD_W(new Str(s+1, last->token, (int)last->pf.size()));
         }
         else {
             vm.pad = s+1;                           /// copy string onto pad
             PUSH(0); PUSH(STRLEN(s+1));             /// 0 = pad, len
         }),
    IMMD(".\"",
         const char *s = word('"'); if (!s) return;
         if (vm.compile) ADD_W(new Str(s+1));
         else            pstr(s+1)),
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
         Bran *b = BTGT();
         BRAN(b->pf);
         b->stage = 1),
    IMMD("then",
         Bran *b = BTGT();
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
         Bran *b = BTGT();
         BRAN(b->pf);                          /// * begin.{pf}.f.while
         b->stage = 2),
    IMMD("repeat",
         Bran *b = BTGT();
         BRAN(b->p1); DICT_POP()),             /// * while.{p1}.repeat
    IMMD("again",
         Bran *b = BTGT();
         BRAN(b->pf); DICT_POP();              /// * begin.{pf}.again
         b->stage = 1),
    IMMD("until",
         Bran *b = BTGT();
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
         Bran *b = BTGT();
         BRAN(b->pf);                          /// * for.{pf}.aft
         b->stage = 3),
    IMMD("next",
         Bran *b = BTGT();
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
    CODE("leave",  UNNEST()),                  /// * exit loop
    IMMD("loop",
         Bran *b = BTGT();
         BRAN(b->pf);                          /// * do.{pf}.loop
         DICT_POP()),
    /// @}
    /// @defgrouop Compiler ops
    /// @{
    CODE("[",      vm.compile = false),
    CODE("]",      vm.compile = true),
    CODE(":",
         DICT_PUSH(new Code(word()));          /// create new word
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
    IMMD("postpone",
         const Code *w = find(word()); if (!w) return;
         ADD_W(w)),
    CODE("immediate", last->immd = 1),
    CODE("exit",   UNNEST()),                  /// -- (exit from word)
    /// @}
    /// @defgroup metacompiler
    /// @brief - dict is directly used, instead of shield by macros
    /// @{
    CODE("exec",   dict[POPI()]->nest(vm)),           /// w --
    CODE("create",
         DICT_PUSH(new Code(word()));
         Code *w = ADD_W(new Var(DU0));
         w->pf[0]->token = w->token;
         w->pf[0]->q.pop()),
    IMMD("does>",
         ADD_W(new Bran(_does));
         last->pf[-1]->token = last->token),          /// keep WP
    CODE("to",                                        /// n --
         const Code *w = find(word()); if (!w) return;
         VAR(w->token) = POP()),                      /// update value
    CODE("is",                                        /// w -- 
         DICT_PUSH(new Code(word(), false));          /// create word
         int w = POP();                               /// like this word
         last->xt = dict[w]->xt;                      /// if primitive
         last->pf = dict[w]->pf),                     /// or colon word
    /// @}
    /// @defgroup Memory Access ops
    /// @{
    CODE("@",       U32 i_w = POPI(); PUSH(VAR(i_w))),           /// a -- n
    CODE("!",       U32 i_w = POPI(); VAR(i_w) = POP()),         /// n a -- 
    CODE("+!",      U32 i_w = POPI(); VAR(i_w) += POP()),
    CODE("?",       U32 i_w = POPI(); dot(DOT, VAR(i_w))),
    CODE(",",       last->pf[0]->q.push(POP())),
    CODE("cells",   { /* for backward compatible */ }),          /// array index, inc by 1
    CODE("allot",   U32 n = POPI();                              /// n --
         for (U32 i=0; i<n; i++) last->pf[0]->q.push(DU0)),
    ///> Note:
    ///>   allot allocate elements in a word's q[] array
    ///>   to access, both indices to word itself and to q array are needed
    ///>   'th' a word that compose i_w, a 32-bit value, the 16 high bits
    ///>   serves as the q index and lower 16 lower bit as word index
    ///>   so a variable (array with 1 element) can be access as usual
    ///>
    CODE("th",      U32 i = POPI() << 16; TOS = UINT(TOS) | i),  /// w i -- i_w
    /// @}
#if DO_MULTITASK
    /// @defgroup Multitasking ops
    /// @}
    CODE("task",                                                /// w -- task_id
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
    CODE("abort",   TOS = -DU1; SS.clear(); RS.clear()),        /// clear ss, rs
    CODE("here",    PUSH(last->token)),
    CODE("'",
         const Code *w = find(word()); if (w) PUSH(w->token)),
    CODE(".s",      ss_dump(vm, true)),                         /// dump parameter stack
    CODE("words",   words(*vm.base)),                           /// display word lists
    CODE("see",
         const Code *w = find(word());
         if (w) see(*w, *vm.base);
         dot(CR)),
    CODE("dict",    dict_dump(*vm.base)),                       /// display dictionary
    CODE("dump",                                                /// ' xx 1 dump
         IU n = POPI(); mem_dump(POPI(), n, *vm.base)), 
    CODE("depth",   PUSH(SS.size())),                           /// data stack depth
    CODE("r",       PUSH(RS.size())),                           /// return stack depth
    /// @}
    /// @defgroup OS ops
    /// @{
    IMMD("include", load(vm, word())),                          /// include an OS file
    CODE("included",                                            /// include a file (programmable)
         POP(); U32 i_w = POPI(); load(vm, STR(i_w))),
    CODE("ok",      mem_stat()),                                /// display memory stat
    CODE("clock",   PUSH(millis())),                            /// get system clock in msec
    CODE("rnd",     PUSH(RND())),                               /// get a random number
    CODE("ms",      IU i = POPI(); delay(i)),                   /// n -- delay n msec
    CODE("forget",
         const Code *w = find(word()); if (!w) return;
         int   t = MAX((int)w->token, (int)find("boot")->token + 1);
         for (int i=(int)dict.size(); i>t; i--) DICT_POP()),
    CODE("boot",
         int t = find("boot")->token + 1;
         for (int i=(int)dict.size(); i>t; i--) DICT_POP())
};
///====================================================================
///
///> Code Class constructors
///
Code::Code(const char *s, const char *d, XT fp, U32 a)    ///> primitive word
    : name(s), desc(d), xt(fp), attr(a) {}
Code::Code(const char *s, bool n) {  ///< new colon word
    const Code *w = find(s);                              /// * scan the dictionary
    name  = w ? w->name : (new string(s))->c_str();       /// * copy the name
    desc  = "";
    xt    = w ? w->xt : NULL;
    token = n ? dict.size() : 0;
    if (n && w) pstr("reDef?");          /// * warn word redefined
}
///
///> Forth inner interpreter
///
void Code::nest(VM &vm) {
//    vm.set_state(NEST);                /// * this => lock, major slow down
    vm.state = NEST;                     /// * racing? No, helgrind says so
    if (xt) { xt(vm, *this); return; }   /// * run primitive word

    for (int i=0; i < (int)pf.size(); i++) {
        try         { pf[i]->nest(vm); } /// * execute recursively
        catch (...) { break; }
        // printf("%-3x => RS=%d, SS=%d %s", i, (int)vm.rs.size(), (int)vm.ss.size(), pf[i]->name);
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
void _tor2(VM &vm, Code &c) { RS.push(POP()); RS.push(POP()); }
void _if(VM &vm, Code &c)   { NEST(POP() ? c.pf : ((Bran&)c).p1); }
void _begin(VM &vm, Code &c){    ///> begin.while.repeat, begin.until
    int b = c.stage;             ///< branching state
    while (true) {
        NEST(c.pf);                            /// * begin..
        if (b==0 && POP()!=0) break;           /// * ..until
        if (b==1)             continue;        /// * ..again
        if (b==2 && POP()==0) break;           /// * ..while..repeat
        NEST(((Bran&)c).p1);
    }
}
void _for(VM &vm, Code &c) {     ///> for..next, for..aft..then..next
    int b = c.stage;                           /// * kept in register
    try {
        do {
            NEST(c.pf);
        } while (b==0 && (RS[-1]-=1) >=0);     /// * for..next only
        while (b) {                            /// * aft
            NEST(((Bran&)c).p2);               /// * then..next
            if ((RS[-1]-=1) < 0) break;        /// * decrement counter
            NEST(((Bran&)c).p1);               /// * aft..then
        }
    }
    catch (...) { /* exit, leave */ }          /// handle EXIT, LEAVE
    RS.pop();
}
void _loop(VM &vm, Code &c) {                  ///> do..loop
    try {
        DU m = RS.pop();
        do {
            NEST(c.pf);
        } while ((RS[-1]+=1) < m);             /// increment counter
    }
    catch (...) {}                             /// handle LEAVE
    RS.pop();                                  /// pop off indicies
}
void _does(VM &vm, Code &c) {
    bool hit = false;
    for (auto w : dict[c.token]->pf) {
        if (hit) ADD_W(w);                     /// copy rest of pf
        if (STRCMP(w->name, "does>")==0) hit = true;
    }
    UNNEST();                                  /// exit caller
}
///====================================================================
///
///> Forth outer interpreter
///
const Code *find(const char *s) {              ///> scan dictionary, last to first
    for (int i = (int)dict.size() - 1; i >= 0; --i) {
        if (STRCMP(s, dict[i]->name)==0) return dict[i];
    }
    return NULL;                               /// * word not found
}

DU parse_number(const char *s, int b) {
    switch (*s) {                              ///> base override
    case '%': b = 2;  s++; break;
    case '&':   
    case '#': b = 10; s++; break;
    case '$': b = 16; s++; break;
    }
    char *p;
    errno = 0;                                 ///> clear overflow flag
#if USE_FLOAT
    DU n = (b==10)
        ? static_cast<DU>(strtof(s, &p))
        : static_cast<DU>(strtol(s, &p, b));
#else
    DU n = static_cast<DU>(strtol(s, &p, b));
#endif
    if (errno || *p != '\0') throw runtime_error("");
    return n;
}

void forth_core(VM &vm, const char *idiom) {
    Code *w = (Code*)find(idiom);     ///< find the word named idiom in dict
    if (w) {                          /// * word found?
        if (vm.compile && !w->immd)   /// * are we compiling new word?
            ADD_W(w);                 /// * append word ptr to it
        else w->nest(vm);             /// * execute forth word
        return;
    }
    DU  n = parse_number(idiom, *vm.base);  ///< try as a number, throw exception
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

    for (const Code &c : rom) {       /// * populate the dictionary
        DICT_PUSH(&c);                /// * ROM => RAM
    }

    uvar_init();                      /// * initialize user area
    t_pool_init();                    /// * initialize thread pool
    VM &vm0   = vm_get(0);            ///< main thread
    vm0.state = HOLD;
}

void forth_teardown() {
    t_pool_stop();
    dict.clear();
}

int forth_vm(const char *line, void(*hook)(int, const char*)) {
    VM &vm = vm_get(0);               ///< main thread
    fout_setup(hook);                 /// * init output stream
    fin_setup(line);                  /// * refresh buffer if not resuming

    string idiom;
    while (fetch(idiom)) {            /// * read a word from line
        const char *s = idiom.c_str();
        try {
            vm.set_state(QUERY);
            forth_core(vm, s);        /// * send to Forth core
        }
        catch (exception &e) {
            pstr(s); pstr("?"); pstr(e.what(), CR);
            vm.compile = false;
            scan('\n');               /// * exhaust input line
        }
    }
    if (!vm.compile) ss_dump(vm);
    
    return vm.state==STOP;
}
