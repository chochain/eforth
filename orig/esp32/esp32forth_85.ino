/******************************************************************************/
/* ESP32 ceForth, Version 8.5 10/10/2021                                               */
/******************************************************************************/
#include <sstream>      // iostream, stringstream
#include <string>       // string class
#include <iomanip>      // setw, setbase, ...
#include "SPIFFS.h"     // flash memory
#include <WebServer.h>
using namespace std;

///==========================================================================
/// macros to simplify coding
///==========================================================================
typedef uint8_t    U8;
typedef uint32_t   U32;
#define IMMD_FLAG  0x80
#define FALSE      0
#define TRUE       -1
#define LOGICAL    ? TRUE : FALSE
#define pop        top = stack[(U8)S--]
#define push       stack[(U8)++S] = top; top =
#define popR       rStack[(U8)R--]
#define pushR      rStack[(U8)++R]
#define ALIGN(sz)  ((sz) + (-(sz) & 0x3))
#define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
#define PEEK(a)    (U32)(*(U32*)((uintptr_t)(a)))
#define POKE(a, c) (*(U32*)((uintptr_t)(a))=(U32)(c))
#define LOGF(s)    Serial.print(F(s))
#define LOG(v)     Serial.print(v)
#define LOGH(v)    Serial.print(v, HEX)
#define ENDL       endl; yield()

///==========================================================================
/// ForthVM global variables
///==========================================================================
istringstream fin;                    /// ForthVM input stream
ostringstream fout;                   /// ForthVM output stream
int rStack[256] = { 0 };                /// return stack
int stack[256] = { 0 };               /// parameter stack
int top;
U8  R = 0, S = 0, bytecode;
U32 P, IP, WP, lfa, nfa, cfa, pfa;    /// dictionary pointers
U32 DP, thread, context, does, fence; /// manage dictionary
int ucase = 1, compile = 0, base = 16;
string idiom;

U32 iDict[16000] = {};                /// integer array
U8  *cDict = (U8*)iDict;              /// byte array alias

///==========================================================================
/// Named functions
///==========================================================================
void next()       { P = iDict[IP >> 2]; WP = P; IP += 4; }
void nest()       { pushR = IP; IP = WP + 4; next(); }
void unnest()     { IP = popR; next(); }            // inner interpreter
void comma(int n) { iDict[DP >> 2] = n; DP += 4; }  /// compiler
void comma_s(int lex, string s) {                   /// string compiler
    comma(lex);
    int len = cDict[DP++] = s.length();
    for (int i = 0; i < len; i++) { cDict[DP++] = s[i]; }
    while (DP & 3) { cDict[DP++] = 0; }
}
string next_idiom(char delim = 0) {
    delim ? getline(fin, idiom, delim) : fin >> idiom; return idiom;
}
void dot_r(int n, int v) {
    fout << setw(n) << setfill(' ') << v;
}
int find(string s) {            /// scan dictionary, return cfa or 0
    int len_s = s.length();
    nfa = context; lfa = DP;    /// return cfa, nfa, pfa and prior lfa
    while (nfa) {
        int len = (int)cDict[nfa] & 0x1f;
        if (len_s == len) {
            bool ok = true;
            const char *c = (const char*)&cDict[nfa+1], *p = s.c_str();
            for (int i=0; ok && i<len; i++, c++, p++) {
                ok = (ucase && *c > 0x40)
                    ? ((*c & 0x5f) == (*p & 0x5f))  /// case senitive
                    : (*c == *p);                   /// case insensitive
            }
            if (ok) {
                yield();
                cfa = ALIGN(nfa + len + 1);
                pfa = cfa + 4;
                return cfa;
            }
        }
        lfa = nfa -4;            /// need to see
        nfa = iDict[lfa >> 2];   /// link field to previous word
    }
    yield();
    return 0;
}
void printName(int n) {           /// print name from cfa
    nfa = context;
    while (nfa) {
        int len = (int)cDict[nfa] & 0x1f;
        cfa = ALIGN(nfa + len + 1);
        if (n == cfa) {
            for (int i=0; i<len; i++) {
              fout << (char)cDict[nfa+1+i];
            }
            fout << ' ';
            return;
        }
    nfa = iDict[(nfa-4) >> 2];      /// link field to previous word
    }
    fout << n << ' ';
}
void words() {
    int n = 0;
    nfa = context;                   /// search dictionary
    fout << ENDL;
    while (nfa) {
        int len = (int)cDict[nfa] & 0x1f;
        for (int i = 0; i < len; i++)
            fout << cDict[nfa + 1 + i];
        if ((++n % 10) == 0) { fout << ENDL; }
        else                 { fout << ' ';  }
        nfa = iDict[(nfa - 4) >> 2];  /// link field to previous word
    }
    fout << ENDL;
}
void dump(int a, int n) {             /// dump dictionary
    fout << setbase(16) << ENDL;
    for (int r = 0, sz = ((n+15)/16); r < sz; r++) {
        int p0 = a + r * 16, p = p0;
        char sum = 0;
        fout <<setw(4) << p << ": ";
        for (int i = 0; i < 16; i++) {
            sum += cDict[p];
            fout <<setw(2) << (int)cDict[p++] << ' ';
        }
        fout << setw(4) << (sum & 0xff) << "  ";
        p = p0;
        for (int i = 0; i < 16; i++) {
            sum = cDict[p++] & 0xff;
            fout <<(char)((sum < 0x20) ? '_' : sum);
        }
        fout << ENDL;
    }
    fout << setbase(base) << ENDL;
}
void ss_dump() {    /// dump parameter stack
    fout << "< "; 
    for (int i = 0; i < 5; i++) fout << stack[(U8)(S - 4 + i)] << " ";
    fout << top << " >ok" << ENDL;
}
///==========================================================================
/// Code - data structure to keep primitive definitions
///==========================================================================
struct Code{
    string name;
    void   (*xt)(void);
    int    immd;
} ;
#define CODE(s, g)  { s, []{ g; }, 0 }
#define IMMD(s, g)  { s, []{ g; }, IMMD_FLAG }
static struct Code primitives[] = {
    /// Execution control 
    CODE("ret",   next()),
    CODE("nop",   {}),
    CODE("nest",  nest()),
    CODE("unnest",unnest()),
    CODE("exit",  IP = popR; next()),
    CODE("dolit", push iDict[IP >> 2]; IP += 4),
    CODE("dovar", push WP + 4),
    CODE("docon", push iDict[(WP + 4) >> 2]),
    CODE("dostr", push IP; IP = ALIGN(IP + cDict[IP] + 1)),
    CODE("dotstr", 
         int len = cDict[IP++];
         for (int i = 0; i < len; i++) fout << cDict[IP++];
         IP = ALIGN(IP)),
    CODE("dodoes", 
        pushR =IP; IP = iDict[(WP + 4) >> 2]; push (WP+8); next(); ),
    CODE("branch", IP = iDict[IP >> 2]; next()),
    CODE("0branch",
         if (top == 0) IP = iDict[IP >> 2];
         else IP += 4;  pop; next()),
    CODE("donext",
         if (rStack[R]) {
             rStack[R] -= 1; IP = iDict[IP >> 2];
         }
         else { IP += 4;  R--; }
         next()),
    /// Stack 
    CODE("dup",   stack[++S] = top),
    CODE("drop",  pop),
    CODE("over",  push stack[(S - 1)]),
    CODE("swap",  int n = top; top = stack[S]; stack[S] = n),
    CODE("rot",
         int n = stack[(S - 1)];
         stack[(S - 1)] = stack[S];
         stack[S] = top; top = n),
    CODE("2dup",  push stack[(S - 1)]; push stack[(S - 1)]),
    CODE("2drop", pop; pop),
    CODE("2over", push stack[(S - 3)]; push stack[(S - 3)]),
    CODE("2swap",
         int n = top; pop; int m = top; pop; int l = top; pop; int i = top; pop;
         push m; push n; push i; push l),
    CODE("pick",  top = stack[(S - top)]),
    /// Return stack
    CODE(">r",    rStack[++R] = top; pop),
    CODE("r>",    push rStack[R--]),
    CODE("r@",    push rStack[R]),
    /// ALU ops
    CODE("+",     int n = top; pop; top += n),
    CODE("-",     int n = top; pop; top -= n),
    CODE("*",     int n = top; pop; top *= n),
    CODE("/",     int n = top; pop; top /= n),
    CODE("mod",   int n = top; pop; top %= n),
    CODE("*/",
         int n = top; pop; int m = top; pop;
         int l = top; pop; push(m * l) / n),
    CODE("/mod",
         int n = top; pop; int m = top; pop;
         push(m % n); push(m / n)),
    CODE("*/mod",
         int n = top; pop; int m = top; pop;
         int l = top; pop; push((m * l) % n); push((m * l) / n)),
    CODE("and",   top &= stack[S--]),
    CODE("or",    top |= stack[S--]),
    CODE("xor",   top ^= stack[S--]),
    CODE("abs",   top = abs(top)),
    CODE("negate",top = -top),
    CODE("max",   int n = top; pop; top = max(top, n)),
    CODE("min",   int n = top; pop; top = min(top, n)),
    CODE("2*",    top *= 2),
    CODE("2/",    top /= 2),
    CODE("1+",    top += 1),
    CODE("1-",    top += -1),
    /// Logic ops
    CODE("0=",    top = (top == 0) LOGICAL),
    CODE("0<",    top = (top < 0) LOGICAL),
    CODE("0>",    top = (top > 0) LOGICAL),
    CODE("=",     int n = top; pop; top = (top == n) LOGICAL),
    CODE(">",     int n = top; pop; top = (top > n) LOGICAL),
    CODE("<",     int n = top; pop; top = (top < n) LOGICAL),
    CODE("<>",    int n = top; pop; top = (top != n) LOGICAL),
    CODE(">=",    int n = top; pop; top = (top >= n) LOGICAL),
    CODE("<=",    int n = top; pop; top = (top <= n) LOGICAL),
    /// IO ops
    CODE("base@", push base),
    CODE("base!", base = top; pop; fout << setbase(base)),
    CODE("hex",   base = 16; fout << setbase(base)),
    CODE("decimal", base = 10; fout << setbase(base)),
    CODE("cr",    fout << ENDL),
    CODE(".",     fout << top << " "; pop),
    CODE(".r",    int n = top; pop; dot_r(n, top); pop),
    CODE("u.r",   int n = top; pop; dot_r(n, abs(top)); pop),
    CODE(".s",    ss_dump()),
    CODE("key",   push(next_idiom()[0])),
    CODE("emit",  char b = (char)top; pop; fout << b),
    CODE("space", fout << " "),
    CODE("spaces",int n = top; pop; for (int i = 0; i < n; i++) fout << " "),
    /// Literal ops
    IMMD("[",     compile = 0),
    CODE("]",     compile = 1),
    IMMD("(",     next_idiom(')')),
    IMMD(".(",    fout << next_idiom(')')),
    IMMD("\\",    next_idiom('\n')),
    IMMD("$*",    comma_s(find("dostr"), next_idiom('"'))),
    IMMD(".\"",   comma_s(find("dotstr"), next_idiom('"'))),
    /// Branching ops
    IMMD("if",    comma(find("0branch")); push DP; comma(0)),
    IMMD("else",
         comma(find("branch")); iDict[top >> 2] = DP + 4;
         top = DP; comma(0)),
    IMMD("then", iDict[top >> 2] = DP; pop),
    /// Loops
    IMMD("begin",  push DP),
    IMMD("while", comma(find("0branch")); push DP; comma(0)),
    IMMD("repeat",
         comma(find("branch")); int n = top; pop;
         comma(top); pop; iDict[n >> 2] = DP),
    IMMD("again", comma(find("branch")); comma(top); pop),
    IMMD("until", comma(find("0branch")); comma(top); pop),
    ///  Cycles
    IMMD("for", comma((find(">r"))); push DP),
    IMMD("aft",
         pop;
         comma((find("branch"))); comma(0); push DP; push DP - 4),
    IMMD("next",  comma(find("donext")); comma(top); pop),
    ///  Compiler ops
    CODE(":",
         thread = DP + 4; comma_s(context, next_idiom());
         comma(cDict[find("nest")]); compile = 1),
    IMMD(";",
         context = thread; compile = 0;
         comma(find("unnest"))),
    CODE("variable",
         thread = DP + 4; comma_s(context, next_idiom());
         context = thread;
         comma(cDict[find("dovar")]); comma(0)),
    CODE("constant",
         thread = DP + 4; comma_s(context, next_idiom());
         context = thread;
         comma(cDict[find("docon")]); comma(top); pop),
    CODE("create",
         thread = DP + 4; comma_s(context, next_idiom());
         context = thread;
         comma(cDict[find("dovar")]); ),
    ///  Memory ops
    CODE("@",  top = iDict[top >> 2]),
    CODE("!",  int a = top; pop; iDict[a >> 2] = top; pop),
    CODE("?",  fout << iDict[top >> 2] << " "; pop),
    CODE("+!", int a = top; pop; iDict[a >> 2] += top; pop),
    CODE("allot",
         int n = top; pop;
         for (int i = 0; i < n; i++) cDict[DP++] = 0),
    CODE(",",  comma(top); pop),
    /// defining word 
    CODE("<builds",
         thread = DP + 4; comma_s(context, next_idiom());
         context = thread; does = DP;
         comma(cDict[find("dovar")]); comma(0)),
    CODE("does>", 
         iDict[does>>2] = cDict[find("dodoes")];  /// change target to dodoes
         iDict[(does+4)>>2] = IP; unnest(); ),    /// instal target interpreter
    /// Re-vector
    CODE("to",                         /// change value of a constant
         int n = find(next_idiom());
         iDict[(n + 4) >> 2] = top; pop; ),
    CODE("is",                         /// re-vector a colon word
         int n = find(next_idiom());
         iDict[(n + 4) >> 2] = top; pop;
         iDict[(n + 8) >> 2] = find("unnest"); ),
    CODE("[to]",                        /// change constants in a colon word
         int n = iDict[IP >> 2]; iDict[(n + 4) >> 2] = top; pop;
         IP += 4; ),
    /// Tools 
    CODE("bye",   exit(0)),
    CODE("here",  push DP),
    CODE("words", words()),
    CODE("dump",  int n = top; pop; int a = top; pop; dump(a, n)),
    CODE("'" ,    push find(next_idiom())),
    CODE("see",
         int n = find(next_idiom());  // n=cfa, pfa and lfa bracket token list
         for (int i = pfa; i < lfa; i+=4) printName(iDict[i >> 2]);
         fout << ENDL),
    CODE("ucase", ucase = top; pop),
    CODE("forget", int n = find(next_idiom()); 
         if (n > fence) { context = iDict[(nfa-4) >> 2]; DP = nfa - 4;} 
         else fout << idiom << "? below fence" << ENDL; ),
    /// ESP32 specific ops
    CODE("clock", push millis()),
    CODE("peek",  int a = top; pop; push PEEK(a)),
    CODE("poke",  int a = top; pop; POKE(a, top); pop),
    CODE("delay", delay(top); pop),
    CODE("pin",   int p = top; pop; pinMode(p, top); pop),
    CODE("in",    int p = top; pop; push digitalRead(p)),
    CODE("out",   int p = top; pop; digitalWrite(p, top); pop),
    CODE("adc",   int p = top; pop; push analogRead(p)),
    CODE("duty",  int p = top; pop; analogWrite(p, top, 255); pop),
    CODE("attach",int p = top; pop; ledcAttachPin(p, top); pop),
    CODE("setup", int ch = top; pop; int freq=top; pop; ledcSetup(ch, freq, top); pop),
    CODE("tone",  int ch = top; pop; ledcWriteTone(ch, top); pop),
    CODE("boot",  DP = find("boot") + 4; context = nfa)
};
///==========================================================================
/// Primitives assembler
///==========================================================================
void encode(struct Code* prim) {    /// DP, thread, and P updated
    string seq = prim->name;
    int immd = prim->immd;
    int len  = seq.length();
    comma(thread);                  /// lfa: link field (U32)
    thread = DP;
    cDict[DP++] = len | immd;        /// nfa: word length + immediate bit
    for (int i = 0; i < len; i++) { cDict[DP++] = seq[i]; }
    while (DP & 3) { cDict[DP++] = 0; }
    comma(P++);                      /// cfa: sequential bytecode (U32 now)
}
void run(int n) { /// inner interpreter, P, WP, IP, R, bytecode modified
    P = n; WP = n; IP = 0; R = 0;
    do {
        bytecode = cDict[P++];       /// fetch bytecode
        primitives[bytecode].xt();   /// execute colon byte-by-byte
    } while (R != 0);
}
void forth_init() {
    DP = thread = P = 0; S = R = 0;
    for (int i = 0; i < sizeof(primitives) / sizeof(Code); i++) {
        encode(&primitives[i]);
    }
    context = DP - 12;               /// lfa: 12 = ALIGN("boot"+1)+sizeof(U32)
    LOG("\nDP=0x");     LOGH(DP);
    LOG(" link=0x");    LOGH(context);
    LOG(" Words=0x");   LOGH(P);
    LOG("\n");
    // Boot Up
    P = WP = IP = 0; fence = DP;
    top = -1;
    fout << setbase(base);
}
///==========================================================================
/// Outer interpreter
///==========================================================================
void forth_outer() {
    while (fin >> idiom) {
        int cfa = find(idiom);
        if (cfa) {
            if (compile && ((cDict[nfa] & IMMD_FLAG) == 0))
                comma(cfa);
            else run(cfa);
        }
        else {
            char* p;
            int n = (int)strtol(idiom.c_str(), &p, base);
            if (*p != '\0') {                   ///  not number
                fout << idiom << "? " << ENDL;  ///  display error prompt
                compile = 0;                    ///  reset to interpreter mode
                getline(fin, idiom, '\n');      ///  skip the entire line
            }
            else {
                if (compile) { comma(find("dolit")); comma(n); }
                else { push n; }
            }
        }
    }
    if (!compile) ss_dump();      /// dump stack and display ok prompt
}
///==========================================================================
/// ESP32 Web Serer connection and html page
///==========================================================================
//const char *ssid = "Sonic-6af4";
//const char *pass = "7b369c932f";
const char *ssid = "Frontier7008";
const char *pass = "8551666595";

WebServer server(80);

static const char *index_html =
"<html><head><meta charset='UTF-8'><title>esp32forth</title>\n"
"<style>body{font-family:'Courier New',monospace;font-size:12px;}</style>\n"
"</head>\n"
"<body>\n"
"    <div id='log' style='float:left;overflow:auto;height:600px;width:600px;\n"
"         background-color:#f8f0f0;'>ESP32Forth 8.02</div>\n"
"    <textarea id='tib' style='height:600px;width:400px;'\n"
"        onkeydown='if (13===event.keyCode) forth()'>words</textarea>\n"
"</body>\n"
"<script>\n"
"let log = document.getElementById('log')\n"
"let tib = document.getElementById('tib')\n"
"function httpPost(url, items, callback) {\n"
"    let fd = new FormData()\n"
"    for (k in items) { fd.append(k, items[k]) }\n"
"    let r = new XMLHttpRequest()\n"
"    r.onreadystatechange = function() {\n"
"        if (this.readyState != XMLHttpRequest.DONE) return\n"
"        callback(this.status===200 ? this.responseText : null)\n"
"    }\n"
"    r.open('POST', url)\n"
"    r.send(fd)\n"
"}\n"
"function forth() {\n"
"    log.innerHTML+='<font color=blue>'+tib.value+'<br/></font>'\n"
"    httpPost('/input', { cmd: tib.value + '\\n' },function(rsp) {\n"
"        if (rsp !== null) {\n"
"            log.innerHTML += rsp.replace(/\\n/g, '<br/>').replace(/\\s/g,'&nbsp;')\n"
"            log.scrollTop=log.scrollHeight }})\n"
"    tib.value = ''\n"
"}\n"
"window.onload = ()=>{ tib.focus() }\n"
"</script></html>\n"
;
///==========================================================================
/// ForthVM front-end handlers
///==========================================================================
static String process_command(String cmd) {
    fout.str("");                       // clean output buffer, ready for next run
    fin.clear();                        // clear input stream error bit if any
    fin.str(cmd.c_str());               // feed user command into input stream
    forth_outer();                      // invoke outer interpreter
    return String(fout.str().c_str());  // return response as a String object
}
///==========================================================================
/// Forth SPIFFS loader
///==========================================================================
static int forth_load(const char *fname) {
    if (!SPIFFS.begin()) {
        LOG("Error mounting SPIFFS\n"); return 1; }
    File file = SPIFFS.open(fname, "r");
    if (!file) {
        LOG("Error opening file:"); LOG(fname); return 1; }
    LOG("Loading file: /data"); LOG(fname); LOG("...\n");
    String cmd = file.readStringUntil('\0');
    process_command(cmd);
    LOG("Done loading.\n");
    file.close();
    SPIFFS.end();
    return 0;
}
///==========================================================================
/// Memory statistics dump - for heap and stack debugging
///==========================================================================
static void mem_stat() {
    LOGF("Core:");           LOG(xPortGetCoreID());
    LOGF(" heap[maxblk=");   LOG(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    LOGF(", avail=");        LOG(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    LOGF(", pmem=");         LOG(context);
    LOGF("], lowest[heap="); LOG(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    LOGF(", stack=");        LOG(uxTaskGetStackHighWaterMark(NULL));
    LOGF("]\n");
    if (!heap_caps_check_integrity_all(true)) {
        abort();                 // bail, on any memory error
    }
}
///==========================================================================
/// Web Server handlers
///==========================================================================
static void handleInput() {
    // receive POST from web browser
    if (!server.hasArg("cmd")) { // make sure parameter contains "cmd" property
        server.send(500, "text/plain", "Missing Input\r\n");
        return;
    }
    // retrieve command from web server
    String cmd = server.arg("cmd");
    LOG("\n>> "+cmd+"\n");       // display requrest on console
    // send requrest command to Forth command processor, and receive response
    String rsp = process_command(cmd);
    LOG(rsp);                    // display response on console
    mem_stat();
    // send response back to web browser
    server.setContentLength(rsp.length());
    server.send(200, "text/plain; charset=utf-8", rsp);
}
///==========================================================================
/// Arduino setup()
///==========================================================================
String console_cmd;          /// ESP32 input buffer
void setup() {
    Serial.begin(115200);
    delay(100);
    ledcSetup(0, 100, 13);
    ledcAttachPin(5, 0);
    analogWrite(0, 250, 255);// turn speaker on
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
    //  WiFi.config(ip, gateway, subnet);
    WiFi.mode(WIFI_STA);
    // attempt to connect to Wifi network:
    WiFi.begin(ssid, pass);
    LOG("\nStart ESP32 \nWiFi connecting ");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        LOG("."); }
    LOG("\nWiFi connected \nIP Address: ");
    LOG(WiFi.localIP());
    server.begin();
    // Setup web server handlers
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", index_html); });
    server.on("/input", HTTP_POST, handleInput);
    LOG("\nHTTP server started\n");
    /// ForthVM initalization
    console_cmd.reserve(16000);
    idiom.reserve(256);
    forth_init();
    forth_load("/load.txt");
    mem_stat();
    LOG("\nesp32Forth v8.5\n");
    analogWrite(0, 0, 255);// turn speaker off
}
///==========================================================================
/// Arduino loop()
///==========================================================================
void loop(void) {
    server.handleClient(); // ESP32 handle web requests
    delay(2);              // yield to background tasks (interrupt, timer,...)
    if (Serial.available()) {
        console_cmd = Serial.readString();
        LOG(console_cmd);
        LOG(process_command(console_cmd));
        delay(2);
    }
}
