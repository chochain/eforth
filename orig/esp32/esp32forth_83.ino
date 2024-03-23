/******************************************************************************/
/* esp32Forth, Version 8.2: for NodeMCU ESP32S                                */
/******************************************************************************/
/* 31aug21ccl/cht  _82   c++, web page                                        */
/* 16jun20cht  _63       web server                                           */
/* 16jun19cht  _62       structures                                           */
/* 14jun19cht  _61       macro assembler with labels                          */
/* 10may19cht  _54       robot tests                                          */
/* 21jan19cht  _51       8 channel electronic organ                           */
/* 15jan19cht  _50       Clean up for AIR robot                               */
/* 03jan19cht  _47-49    Move to ESP32                                        */
/* 07jan19cht  _46       delete UDP                                           */
/* 03jan19cht  _45       Move to NodeMCU ESP32S Kit                           */
/* 18jul17cht  _44       Byte code sequencer                                  */
/* 14jul17cht  _43       Stacks in circular buffers                           */
/* 01jul17cht  _42       Compiled as an Arduino sketch                        */
/* 20mar17cht  _41       Compiled as an Arduino sketch                        */
/* Follow the ceForth model with 64 primitives                                */
/* Serial Monitor at 115200 baud                                              */
/* Send and receive UDP packets in parallel with Serial Monitor               */
/* Case insensitive interpreter                                               */
/* data[] must be filled with rom42.h eForth dictionary                       */
/* 22jun17cht Stacks are 256 cell circular buffers, byte pointers R and S     */
/* All references to R and S are forced to (unsigned char)                    */
/* All multiply-divide words cleaned up                                       */
/******************************************************************************/
#include <memory>           // shared_ptr, make_shared 
#include <sstream>          // iostream, stringstream, string
#include <iomanip>          // setw, setbase, ...
#include <vector>           // vector
#include <functional>       // function
#include <exception>        // try...catch, throw
#include <string.h>         // strcasecmp
#include "SPIFFS.h"         // flash memory
#include <WebServer.h>
using namespace std;        // default namespace to std
///
/// macros for portability
///
#define ENDL                endl
#define to_string(i)        string(String(i).c_str())
#define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
///
/// Forth vector template proxy class
///
template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern
    T& operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    T dec_i() { return v.back() -= 1; }     /// decrement stack top
    T pop()   {
        if (v.empty()) throw underflow_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t; }
    int  size()               { return (int)v.size(); }
    void push(T t)            { v.push_back(t); }
    void merge(ForthList& a)  { v.insert(v.end(), a.v.begin(), a.v.end()); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
    void clear(int i=0)       { v.erase(v.begin() + i, v.end()); }
};
///
/// Forth universal Code class
///
class Code;
using fop   = function<void(Code&)>;        /// Forth functor
using CodeP = shared_ptr<Code>;             /// C++ managed smart pointer
class Code {
public:
    static int fence, IP, maxbss;           /// token incremental counter
    string name;                            /// name of word
    int    token = 0;                       /// dictionary order token
    bool   immd  = false;                   /// immediate flag
    int    stage = 0;                       /// branching stage
    fop    xt    = NULL;                    /// primitive function
    string literal;                         /// string literal
    ForthList<CodeP> pf;
    ForthList<CodeP> pf1;
    ForthList<CodeP> pf2;
    ForthList<int>   qf;
    Code(string n, fop fn, bool im=false) { /// primitive
        name = n; token = fence++; immd = im; xt = fn; }
    Code(string n, bool f=false) { name = n; if (f) token = fence++; }
    Code(CodeP c, int v) { name = c->name; xt = c->xt; qf.push(v); }
    Code(CodeP c, string s=string()) { name = c->name; xt = c->xt; if (s.size()>0) literal = s;  }
    ~Code() { pf.clear(); pf1.clear(); pf2.clear(); qf.clear(); }         // free allocated memory
    Code& addcode(CodeP w) { pf.push(w);   return *this; }
    string to_s()          { return name + " " + to_string(token) + (immd ? "*" : ""); }
    string see(int dp=0) {
        stringstream cout("");
        auto see_pf = [&cout](int dp, string s, ForthList<CodeP>& pf) {   // lambda for indentation and recursive dump
            int i = dp; cout << ENDL; while (i--) cout << "  "; cout << s;
            for (CodeP w: pf.v) cout << w->see(dp + 1); };
        auto see_qf = [&cout](ForthList<int>& qf) { cout << " = "; for (int i : qf.v) cout << i << " "; };
        see_pf(dp, "[ " + to_s(), pf);
        if (pf1.size() > 0) see_pf(dp, "1--", pf1);
        if (pf2.size() > 0) see_pf(dp, "2--", pf2);
        if (qf.size()  > 0) see_qf(qf);
        cout << "]";
        return cout.str(); }
    void  nest() {
        int tmp = IP;
        int off = (int)((unsigned char*)&tmp - pxTaskGetStackStart(NULL));
        if (off > Code::maxbss) Code::maxbss = off;
        if (xt) { xt(*this); return; }
        int n = pf.size(); IP = 0;                       /// * or, setup call frame
        while (IP < n) { yield(); pf[IP++]->nest(); }    /// * and run inner interpreter
        IP = tmp; }                                      /// * resture call frame
};
/// initialize static variables
int Code::fence = 0, Code::IP = 0, Code::maxbss = 0;
///
/// macros to reduce verbosity (but harder to single-step debug)
///
#define CODE(s, g) make_shared<Code>(s, [this](Code& c){ g; })
#define IMMD(s, g) make_shared<Code>(s, [this](Code& c){ g; }, true)
#define WORD()     make_shared<Code>(next_idiom(), true)
#define LIT(s,l)   make_shared<Code>(find(s), (l))
#define BRAN(s)    make_shared<Code>(find(s))
#define TEMP(s)    make_shared<Code>("temp")
#define BOOL(f)    ((f) ? -1 : 0)
///
/// macros for memory space access (be very careful of these)
/// note: 4000_0000 is instruction bus, access needs to be 32-bit aligned
///       3fff_ffff and below is data bus, no alignment requirement
///
typedef unsigned int U32;
#define PEEK(a)    (U32)(*(U32*)((uintptr_t)(a)))
#define POKE(a, c) (*(U32*)((uintptr_t)(a))=(U32)(c))
///
/// Forth Virtual Machine class
///
class ForthVM {
    istream          &cin;                /// stream input
    ostream          &cout;               /// stream output
    ForthList<int>   rs;                  /// return stack
    ForthList<int>   ss;                  /// parameter stack
    ForthList<CodeP> dict;                /// dictionary
    bool  compile = false;                /// compiling flag
    int   ucase   = 1;                    /// case sensitive control
    int   base    = 10;                   /// numeric radix
    int   WP      = 0;                    /// instruction and parameter pointers
    int   top     = 0;                    /// cached top of stack
    string idiom; 
    inline int POP()         { int n = top; top = ss.pop(); return n; }
    inline int PUSH(int v) { ss.push(top); return top = v; }
    inline int STREQ(string& s1, string& s2) {
        return ucase ? strcasecmp(s1.c_str(), s2.c_str())==0 : s1==s2; }
    /// search dictionary reversely
    CodeP find(string s) {
        for (int i = dict.size() - (compile ? 2 : 1); i >= 0; --i) {
            if (STREQ(s, dict[i]->name)) return dict[i]; }
        return NULL; }
    string& next_idiom(char delim=0) {
        delim ? getline(cin, idiom, delim) : cin >> idiom; return idiom; }
    void dot_r(int n, int v) { cout << setw(n) << setfill(' ') << v; }
    void ss_dump() {
        cout << " <"; for (int i : ss.v) { cout << i << " "; }
        cout << top << "> ok" << ENDL; }
    void words() {
        int i = 0;
        for (CodeP w : dict.v) {
            if ((i++ % 10) == 0) { cout << ENDL; yield(); }
            cout << w->to_s() << " "; } }
    void call(CodeP w) {
        int tmp = WP;                                       /// * setup call frame
        WP = w->token;
        try { w->nest(); }                                  /// * run inner interpreter recursively
        catch (exception& e) {
            string msg = e.what();                          /// * capture exception message
            if (msg != string()) cout << msg << ENDL; }
        WP = tmp;                                           /// * restore call frame
        yield(); }
    void call(ForthList<CodeP>& pf) {
        for (int i=0, n=pf.size(); i<n; i++) call(pf[i]); }
public:
    ForthVM(istream &in, ostream &out)
        : cin(in), cout(out), idiom(string("", 256)) {}
    void init() {
        dict.v = {                      /// TODO: singleton, built once (=>ROM)
        ///
        /// @defgroup Stack ops
        /// @{
        CODE("dup",  PUSH(top)),
        CODE("drop", top = ss.pop()),
        CODE("over", PUSH(ss[-1])),
        CODE("swap", int n = ss.pop(); PUSH(n)),
        CODE("rot",  int n = ss.pop(); int m = ss.pop(); ss.push(n); PUSH(m)),
        CODE("pick", int i = top; top = ss[-i]),
        CODE(">r",   rs.push(POP())),
        CODE("r>",   PUSH(rs.pop())),
        CODE("r@",   PUSH(rs[-1])),
        /// @}
        /// @defgroup Stack ops - double
        /// @{
        CODE("2dup", PUSH(ss[-1]); PUSH(ss[-1])),
        CODE("2drop",ss.pop(); top = ss.pop()),
        CODE("2over",PUSH(ss[-3]); PUSH(ss[-3])),
        CODE("2swap",
             int n = ss.pop(); int m = ss.pop(); int l = ss.pop();
             ss.push(n); PUSH(l); PUSH(m)),
        /// @}
        /// @defgroup ALU ops
        /// @{
        CODE("+",    top += ss.pop()),
        CODE("-",    top =  ss.pop() - top),
        CODE("*",    top *= ss.pop()),
        CODE("/",    top =  ss.pop() / top),
        CODE("mod",  top =  ss.pop() % top),
        CODE("*/",   top =  ss.pop() * ss.pop() / top),
        CODE("/mod",
             int n = ss.pop(); int t = top;
             ss.push(n % t); top = (n / t)),
        CODE("*/mod",
             int n = ss.pop() * ss.pop();
             int t = top;
             ss.push(n % t); top = (n / t)),
        CODE("and",  top = ss.pop() & top),
        CODE("or",   top = ss.pop() | top),
        CODE("xor",  top = ss.pop() ^ top),
        CODE("abs",  top = abs(top)),
        CODE("negate", top = -top),
        CODE("abs",  top = abs(top)),
        CODE("max",  int n=ss.pop();top = (top>n)?top:n),
        CODE("min",  int n=ss.pop();top = (top<n)?top:n),
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
        CODE("base!",   cout << setbase(base = POP())),
        CODE("hex",     cout << setbase(base = 16)),
        CODE("decimal", cout << setbase(base = 10)),
        CODE("cr",      cout << ENDL),
        CODE(".",       cout << POP() << " "),
        CODE(".r",      int n = POP(); dot_r(n, POP())),
        CODE("u.r",     int n = POP(); dot_r(n, abs(POP()))),
        CODE(".f",      int n = POP(); cout << setprecision(n) << POP()),
        CODE("key",     PUSH(next_idiom()[0])),
        CODE("emit",    char b = (char)POP(); cout << b),
        CODE("space",   cout << " "),
        CODE("spaces",  for (int n = POP(), i = 0; i < n; i++) cout << " "),
        /// @}
        /// @defgroup Literal ops
        /// @{
        CODE("dotstr",  cout << c.literal),
        CODE("dolit",   PUSH(c.qf[0])),
        CODE("dovar",   PUSH(c.token)),
        CODE("[",       compile = false),
        CODE("]",       compile = true),
        IMMD("(",       next_idiom(')')),
        IMMD(".(",      cout << next_idiom(')')),
        CODE("\\",      next_idiom('\n')),
        CODE("$\"",
             string s = next_idiom('"').substr(1);
             dict[-1]->addcode(LIT("dovar", s))),
        IMMD(".\"",
             string s = next_idiom('"').substr(1);
             dict[-1]->addcode(LIT("dotstr", s))),
        /// @}
        /// @defgroup Branching ops
        /// @brief - if...then, if...else...then
        /// @{
        IMMD("bran", bool f = POP() != 0; call(f ? c.pf : c.pf1)),
        IMMD("if",
             dict[-1]->addcode(BRAN("bran"));
             dict.push(TEMP())),               // use last cell of dictionay as scratch pad
        IMMD("else",
             CodeP temp = dict[-1]; CodeP last = dict[-2]->pf[-1];
             last->pf.merge(temp->pf);
             temp->pf.clear();
             last->stage = 1),
        IMMD("then",
             CodeP temp = dict[-1]; CodeP last = dict[-2]->pf[-1];
             if (last->stage == 0) {                     // if...then
                 last->pf.merge(temp->pf);
                 dict.pop(); }
             else {                                      // if...else...then, or
                 last->pf1.merge(temp->pf);              // for...aft...then...next
                 if (last->stage == 1) dict.pop();
                 else temp->pf.clear(); }),
        /// @}
        /// @defgroup Loops
        /// @brief  - begin...again, begin...f until, begin...f while...repeat
        /// @{
        CODE("loop",
             while (true) {
                 call(c.pf);                                           // begin...
                 int f = top;
                 if (c.stage == 0 && (top = ss.pop(), f != 0)) break;  // ...until
                 if (c.stage == 1) continue;                           // ...again
                 if (c.stage == 2 && (top = ss.pop(), f == 0)) break;  // while...repeat
                 call(c.pf1); }),
        IMMD("begin",
             dict[-1]->addcode(BRAN("loop"));
             dict.push(TEMP())),
        IMMD("while",
             CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
             last->pf.merge(temp->pf);
             temp->pf.clear(); last->stage = 2),
        IMMD("repeat",
             CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
             last->pf1.merge(temp->pf); dict.pop()),
        IMMD("again",
             CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
             last->pf.merge(temp->pf);
             last->stage = 1; dict.pop()),
        IMMD("until",
             CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
             last->pf.merge(temp->pf); dict.pop()),
        /// @}
        /// @defgrouop For loops
        /// @brief  - for...next, for...aft...then...next
        /// @{
        CODE("cycle",
             do { call(c.pf); }
             while (c.stage == 0 && rs.dec_i() >= 0);    // for...next only
             while (c.stage > 0) {                       // aft
                 call(c.pf2);                            // then...next
                 if (rs.dec_i() < 0) break;
                 call(c.pf1); }                          // aft...then
             rs.pop()),
        IMMD("for",
             dict[-1]->addcode(find(">r"));
             dict[-1]->addcode(BRAN("cycle"));
             dict.push(TEMP())),
        IMMD("aft",
             CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
             last->pf.merge(temp->pf);
             temp->pf.clear(); last->stage = 3),
        IMMD("next",
             CodeP last = dict[-2]->pf[-1]; CodeP temp = dict[-1];
             if (last->stage == 0) last->pf.merge(temp->pf);
             else last->pf2.merge(temp->pf); dict.pop()),
        /// @}
        /// @defgrouop Compiler ops
        /// @{
        CODE("exit", int x = top; throw domain_error(string())),   // need x=top, Arduino bug
        CODE("exec", int n = top; call(dict[n])),
        CODE(":", dict.push(WORD()); compile = true),     // create new word
        IMMD(";", compile = false),
        CODE("variable",
             dict.push(WORD());
             Code& last = dict[-1]->addcode(LIT("dovar", 0));
             last.pf[0]->token = last.token),
        CODE("constant",
             dict.push(WORD());
             Code& last = dict[-1]->addcode(LIT("dolit", POP()));
             last.pf[0]->token = last.token),
        CODE("@",      int w = POP(); PUSH(dict[w]->pf[0]->qf[0])),         // w -- n
        CODE("!",      int w = POP(); dict[w]->pf[0]->qf[0] = POP()),       // n w --
        CODE("+!",     int w = POP(); dict[w]->pf[0]->qf[0] += POP()),      // n w --
        CODE("?",      int w = POP(); cout << dict[w]->pf[0]->qf[0] << " "),// w --
        CODE("array@", int a = POP(); PUSH(dict[POP()]->pf[0]->qf[a])),     // w a -- n
        CODE("array!", int a = POP(); int w = POP();  dict[w]->pf[0]->qf[a] = POP()),   // n w a --
        CODE("allot",                                            // n --
             for (int n = POP(), i = 0; i < n; i++) dict[-1]->pf[0]->qf.push(0)),
        CODE(",",      dict[-1]->pf[0]->qf.push(POP())),
        /// @}
        /// @defgroup metacompiler
        /// @{
        CODE("create",
             dict.push(WORD());                                  // create a new word
             Code& last = dict[-1]->addcode(LIT("dovar", 0));
             last.pf[0]->token = last.token;
             last.pf[0]->qf.clear()),
        CODE("does",
             ForthList<CodeP> &src = dict[WP]->pf;               // source word : xx create...does...;
             int n = src.size();
             while (Code::IP < n) dict[-1]->pf.push(src[Code::IP++])),       // copy words after "does" to new the word
        CODE("to",                                               // n -- , compile only
             CodeP tgt = find(next_idiom());
             if (tgt) tgt->pf[0]->qf[0] = POP()),                // update constant
        CODE("is",                                               // w -- , execute only
             CodeP tgt = find(next_idiom());
             if (tgt) {
                 tgt->pf.clear();
                 tgt->pf.merge(dict[POP()]->pf);
             }),
        CODE("[to]",
             ForthList<CodeP> &src = dict[WP]->pf;               // source word : xx create...does...;
             src[Code::IP++]->pf[0]->qf[0] = POP()),             // change the following constant
        /// @}
        /// @defgroup Debug ops
        /// @{
        CODE("bye",   exit(0)),                                  // reboot ESP32
        CODE("here",  PUSH(dict[-1]->token)),
        CODE("words", words()),
        CODE(".s",    ss_dump()),
        CODE("'",     CodeP w = find(next_idiom()); PUSH(w->token)),
        CODE("see",
             CodeP w = find(next_idiom());
             if (w) cout << w->see() << ENDL),
        CODE("forget",
             CodeP w = find(next_idiom());
             if (w == NULL) return;
             dict.clear(Code::fence=max(w->token, find("boot")->token + 1))),
        CODE("clock", PUSH(millis())),
        CODE("delay", delay(POP())),
        CODE("peek",  int a = POP(); PUSH(PEEK(a))),
        CODE("poke",  int a = POP(); POKE(a, POP())),
        CODE("ucase", ucase = POP()),
        /// @}
        /// @defgroup Arduino specific ops
        /// @{
        CODE("pin",   int p = POP(); pinMode(p, POP())),
        CODE("in",    PUSH(digitalRead(POP()))),
        CODE("out",   int p = POP(); digitalWrite(p, POP())),
        CODE("adc",   PUSH(analogRead(POP()))),
        CODE("duty",  int p = POP(); analogWrite(p, POP(), 255)),
        CODE("attach",int p  = POP(); ledcAttachPin(p, POP())),
        CODE("setup", int ch = POP(); int freq=POP(); ledcSetup(ch, freq, POP())),
        CODE("tone",  int ch = POP(); ledcWriteTone(ch, POP())),
        CODE("heap",  PUSH(esp_get_free_heap_size())),
        /// @}
        CODE("boot", dict.clear(Code::fence=find("boot")->token + 1))
        };
    }
    void outer() {
        while (cin >> idiom) {
            CodeP w = find(idiom);                      /// * search through dictionary
            if (w) {                                    /// * word found?
                if (compile && !w->immd)                /// * in compile mode?
                    dict[-1]->addcode(w);               /// * add to colon word
                else call(w);                           /// * execute forth word
                continue; }
            // try as a number
            char *p;
            int n = static_cast<int>(strtol(idiom.c_str(), &p, base));
            if (*p != '\0') {                           /// * not number
                cout << idiom << "? " << ENDL;          ///> display error prompt
                compile = false;                        ///> reset to interpreter mode
                ss.clear(); top=0;
                break; }
            // is a number
            if (compile)                                /// * a number in compile mode?
                dict[-1]->addcode(LIT("dolit", n));     ///> add literal to current word
            else PUSH(n); }                             ///> or, add value onto data stack 
        if (!compile) ss_dump(); }  /// * dump stack and display ok prompt
};
///==========================================================================
/// ESP32 Web Serer connection and index page
///==========================================================================
const char *ssid = "Frontier7008";
const char *pass = "8551666595";

WebServer server(80);

/******************************************************************************/
/* ledc                                                                       */
/******************************************************************************/
/* LEDC Software Fade */
// use first channel of 16 channels (started from zero)
#define LEDC_CHANNEL_0     0
// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13
// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000
// fade LED PIN (replace with LED_BUILTIN constant for built-in LED)
#define LED_PIN            5
#define BRIGHTNESS         255    // how bright the LED is

static const char *index_html PROGMEM = R"XX(
<html><head><meta charset='UTF-8'><title>esp32forth</title>
<style>body{font-family:'Courier New',monospace;font-size:12px;}</style>
</head>
<body>
    <div id='log' style='float:left;overflow:auto;height:600px;width:600px;
         background-color:#f8f0f0;'>ESP32Forth 8.02</div>
    <textarea id='tib' style='height:600px;width:400px;'
        onkeydown='if (13===event.keyCode) forth()'>words</textarea>
</body>
<script>
let log = document.getElementById('log')
let tib = document.getElementById('tib')
function httpPost(url, items, callback) {
    let fd = new FormData()
    for (k in items) { fd.append(k, items[k]) }
    let r = new XMLHttpRequest()
    r.onreadystatechange = function() {
        if (this.readyState != XMLHttpRequest.DONE) return
        callback(this.status===200 ? this.responseText : null) }
    r.open('POST', url)
    r.send(fd) }
function chunk(ary, d) {                        // recursive call to sequence POSTs
    req = ary.slice(0,30).join('\n')            // 30*(average 50 byte/line) ~= 1.5K
    if (req=='' || d>20) return                 // bail looping, just in case
    log.innerHTML+='<font color=blue>'+req.replace(/\n/g, '<br/>')+'</font>'
    httpPost('/input', { cmd: req }, rsp=>{
        if (rsp !== null) {
            log.innerHTML += rsp.replace(/\n/g, '<br/>').replace(/\s/g,'&nbsp;')
            log.scrollTop=log.scrollHeight      // scroll down
            chunk(ary.splice(30), d+1) }})}     // next 30 lines
function forth() { 
    let str = tib.value.replace(/\\.*\n/g,'').split(/(\(\s[^\)]+\))/)
    let cmd = str.map(v=>v[0]=='(' ? v.replaceAll('\n',' ') : v).join('')
    chunk(cmd.split('\n'), 1); tib.value = '' }
window.onload = ()=>{ tib.focus() }
</script></html>
)XX";
///==========================================================================
/// ForthVM front-end handlers
///==========================================================================
istringstream forth_in;
ostringstream forth_out;
ForthVM *forth_vm =  new ForthVM(forth_in, forth_out);
///
/// Forth Command processor
///
static String process_command(String cmd) {
    forth_out.str("");               // clean output buffer, ready for next run
    forth_in.clear();                // clear input stream error bit if any
    forth_in.str(cmd.c_str());       // feed user command into input stream
    forth_vm->outer();               // invoke outer interpreter (ForthVM backend)
    return String(forth_out.str().c_str());  // return response as a String object
}
///
/// Forth bootstrap loader (from Flash)
///
static int forth_load(const char *fname) {
    if (!SPIFFS.begin()) {
        Serial.println("Error mounting SPIFFS"); return 1; }
    File file = SPIFFS.open(fname, "r");
    if (!file) {
        Serial.print("Error opening file:"); Serial.println(fname); return 1; }
    Serial.print("Loading file: "); Serial.print(fname); Serial.print("...");
    while (file.available()) {
        // retrieve command from Flash memory
        String cmd = file.readStringUntil('\n');
        Serial.println("<< "+cmd);  // display bootstrap command on console
        // send it to Forth command processor
        process_command(cmd); }
    Serial.println("Done loading.");
    file.close();
    SPIFFS.end();
    return 0;
}
///
/// memory statistics dump - for heap and stack debugging
///
static void mem_stat() {
    Serial.print("Core:");          Serial.print(xPortGetCoreID());
    Serial.print(" heap[maxblk=");  Serial.print(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.print(", avail=");        Serial.print(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    Serial.print(", maxbss=");       Serial.print(Code::maxbss);
    Serial.print("], lowest[heap="); Serial.print(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    Serial.print(", stack=");        Serial.print(uxTaskGetStackHighWaterMark(NULL));
    Serial.println("]");
    if (!heap_caps_check_integrity_all(true)) {
//        heap_trace_dump();     // dump memory, if we have to
        abort();                 // bail, on any memory error
    }
}
///==========================================================================
/// Web Server handlers
///==========================================================================
static void handleInput() {
    // receive POST from web browser
    if (!server.hasArg("cmd")) {  // make sure parameter contains "cmd" property
        server.send(500, "text/plain", "Missing Input\r\n");
        return;
    }
    // retrieve command from web server
    String cmd = server.arg("cmd");
    Serial.print("\n>> "+cmd);          // display requrest on console
    // send requrest command to Forth command processor, and receive response
    String rsp = process_command(cmd);
    Serial.print(rsp);                  // display response on console
    mem_stat();
    // send response back to web browser
    server.setContentLength(rsp.length());
    server.send(200, "text/plain; charset=utf-8", rsp);
}
///==========================================================================
/// ESP32 routines
///==========================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    //  WiFi.config(ip, gateway, subnet);
    WiFi.mode(WIFI_STA);
    // attempt to connect to Wifi network:
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("."); }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    // if you get a connection, report back via serial:
    server.begin();
    Serial.println("Booting esp32Forth v8.2 ...");
    // Setup timer and attach timer to a led pin
    ledcSetup(0, 100, LEDC_TIMER_13_BIT);
    ledcAttachPin(5, 0);
    analogWrite(0, 250, BRIGHTNESS);
    pinMode(2,OUTPUT);
    digitalWrite(2, HIGH);   // turn the LED2 on
    pinMode(16,OUTPUT);
    digitalWrite(16, LOW);   // motor1 forward
    pinMode(17,OUTPUT);
    digitalWrite(17, LOW);   // motor1 backward
    pinMode(18,OUTPUT);
    digitalWrite(18, LOW);   // motor2 forward
    pinMode(19,OUTPUT);
    digitalWrite(19, LOW);   // motor2 bacward
    // Setup web server handlers
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", index_html); });
    server.on("/input", HTTP_POST, handleInput);
    server.begin();
    Serial.println("HTTP server started");
    ///
    /// ForthVM initalization
    ///
    forth_vm->init();
    forth_load("/load.txt");    // compile \data\load.txt
}

void loop(void) {
    server.handleClient(); // ESP32 handle web requests
    delay(2);              // yield to background tasks (interrupt, timer,...)
    ///
    /// while Web requests come in from handleInput asynchronously,
    /// we also take user input from console (for debugging mostly)
    ///
    if (Serial.available()) {
        String cmd = Serial.readString();
        Serial.print(process_command(cmd));
        mem_stat();
        delay(2); }         // check! might not be thread-safe
}
