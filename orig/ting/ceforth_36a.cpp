/******************************************************************************/
/* ceForth_36.cpp, Version 3.6 : Forth in C                                   */
/******************************************************************************/
/* 20sep21cht   version 3.5                                                   */
/* Primitives/outer coded in C                                                */
/* 01jul19cht   version 3.3                                                   */
/* Macro assembler, Visual Studio 2019 Community                              */
/* 13jul17cht   version 2.3                                                   */
/* True byte code machine with bytecode                                       */
/* Change w to WP, pointing to parameter field                                */
/* 08jul17cht  version 2.2                                                    */
/* Stacks are 256 cell circular buffers                                       */
/* Clean up, delete SP@, SP!, RP@, RP!                                        */
/* 13jun17cht  version 2.1                                                    */
/* Compiled as a C++ console project in Visual Studio Community 2017          */
/* Follow the eForth model with 64 primitives                                 */
/* Kernel                                                                     */
/* Use long long int to implement multipy and divide primitives               */
/* Case insensitive interpreter                                               */
/* data[] must be filled with rom_21.h eForth dictionary                      */
/*   from c:/F#/ceforth_21                                                    */
/* C compiler must be reminded that S and R are (char)                        */
/******************************************************************************/
#include <string>       // string class
#include <iostream>     // cin, cout
#include <iomanip>      // setw, setbase, ...
using namespace std;

#define  FALSE 0
#define  TRUE  -1
#define  LOGICAL ? -1 : 0
#define  LOWER(x,y) ((unsigned long)(x)<(unsigned long)(y))
#define  pop top = stack[(unsigned char)S--]
#define  push stack[(unsigned char)++S] = top; top =
#define  popR rack[(unsigned char)R--]
#define  pushR rack[(unsigned char)++R]
#define  ALIGN(sz) ((sz) + (-(sz) & 0x3))
#define  IMEDD    0x80

int rack[256] = { 0 };
int stack[256] = { 0 };
unsigned char R = 0, S = 0, bytecode, c;	// CC: bytecode should be U32, c is not used
int* Pointer;								// CC: Pointer is unused
int  P, IP, WP, top, len;					// CC: len can be local
int  lfa, nfa, cfa, pfa;					// CC: pfa is unused; lfa, cfa can be local
int  DP, link, context;						// CC: DP, context and link usage are interchangable
int  ucase = 1, compile = 0, base = 16;
string idiom, s;                            // CC: s is unused; idiom can be local

uint32_t data[16000] = {};
uint8_t  *cData = (uint8_t*)data;

void next()       { P = data[IP >> 2]; WP = P; IP += 4; }
void nest()       { pushR = IP; IP = WP + 4; next(); }
void unnest()     { IP = popR; next(); }
void comma(int n) { data[DP >> 2] = n; DP += 4; }
void comma_s(int lex, string s) {
	comma(lex); len = s.length(); cData[DP++] = len;
	for (int i = 0; i < len; i++) { cData[DP++] = s[i]; }
	while (DP & 3) { cData[DP++] = 0; }
}
string next_idiom(char delim = 0) {
	string s; delim ? getline(cin, s, delim) : cin >> s; return s;
}
void dot_r(int n, int v) {
	cout << setw(n) << setfill(' ') << v;
}
int find(string s) {						// CC: nfa, lfa, cfa, len modified
	int len_s = s.length();
	nfa = context;
	while (nfa) {
        lfa = nfa - 4;           				// CC: 4 = sizeof(IU)
        len = (int)cData[nfa] & 0x1f;			// CC: 0x1f = ~IMEDD
        if (len_s == len) {
            int success = 1;                    // CC: memcmp
            for (int i = 0; i < len; i++) {
                if (s[i] != cData[nfa + 1 + i])
                {
                    success = 0; break;
                }
            }
            if (success) { return cfa = ALIGN(nfa + len + 1); }
        }
        nfa = data[lfa >> 2];
	}
	return 0;
}
void words() {
	int n = 0;
	nfa = context; // CONTEXT
	while (nfa) {
        lfa = nfa - 4;
        len = (int)cData[nfa] & 0x1f;
        for (int i = 0; i < len; i++)
            cout << cData[nfa + 1 + i];
        cout << ((++n%10==0) ? '\n' : ' ');
        nfa = data[lfa >> 2];
	}
	cout << endl;
}
void CheckSum() {                            // CC: P updated, but used as a local variable
	pushR = P; char sum = 0;
	cout << setw(4) << setbase(16) << P << ": ";
	for (int i = 0; i < 16; i++) {
        sum += cData[P];
        cout << setw(2) << (int)cData[P++] << ' ';
	}
	cout << setw(4) << (sum & 0XFF) << "  ";
	P = popR;
	for (int i = 0; i < 16; i++) {
        sum = cData[P++] & 0x7f;
        cout << (char)((sum < 0x20) ? '_' : sum);
	}
	cout << endl;
}
void dump() {// a n --                       // CC: P updated, but used as a local variable
	cout << endl;
	len = top / 16; pop; P = top; pop;
	for (int i = 0; i < len; i++) { CheckSum(); }
}
void ss_dump() {
	cout << "< "; for (int i = S - 4; i < S + 1; i++) { cout << stack[i] << " "; }
	cout << top << " >ok" << endl;
}
///
/// Code - data structure to keep primitive definitions
///
typedef struct {
    string name;
    void   (*xt)(void);
    int    immd;
} Code;
#define CODE(s, g)  { s, []{ g; }, 0 }
#define IMMD(s, g)  { s, []{ g; }, IMEDD }
const static Code primitives[] = {
	CODE("ret",   next()),
	/// Stack ops
	CODE("nop",   {}),
	CODE("nest",  nest()),
	CODE("unnest",unnest()),
	CODE("dup",   stack[++S] = top),
	CODE("drop",  pop),
	CODE("over",  push stack[(S - 1)]),
	CODE("swap",
         int n = top;
         top = stack[S];
         stack[S] = n),
	CODE("rot",
         int n = stack[(S - 1)];
         stack[(S - 1)] = stack[S];
         stack[S] = top; top = n),
	CODE("pick",  top = stack[(S - top)]),
	CODE(">r",    rack[++R] = top; pop),
	CODE("r>",    push rack[R--]),
	CODE("r@",    push rack[R]),
	/// Stack ops - double
	CODE("2dup",  push stack[(S - 1)]; push stack[(S - 1)]),
	CODE("2drop", pop; pop),
	CODE("2over", push stack[(S - 3)]; push stack[(S - 3)]),
	CODE("2swap", 
         int n = top; pop; int m = top; pop; int l = top; pop; int i = top; pop;
         push m; push n; push i; push l),
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
	CODE("base!", base = top; pop; cout << setbase(base)),
	CODE("hex",   base = 16; cout << setbase(base)),
	CODE("decimal", base = 10; cout << setbase(base)),
	CODE("cr",    cout << endl),
	CODE(".",     cout << top << " "; pop),
	CODE(".r",    int n = top; pop; dot_r(n, top); pop),
	CODE("u.r",   int n = top; pop; dot_r(n, abs(top)); pop),
	CODE(".s",    ss_dump()),
	CODE("key",   push(next_idiom()[0])),
	CODE("emit",  char b = (char)top; pop; cout << b),
	CODE("space", cout << " "),
	CODE("spaces",int n = top; pop; for (int i = 0; i < n; i++) cout << " "),
	/// Literal ops
	CODE("dostr",
         int p = IP; push p; len = cData[p];
         p += (len + 1); p += (-p & 3); IP = p),
	CODE("dotstr",
         int p = IP; len = cData[p++];
         for (int i = 0; i < len; i++) cout << cData[p++];
         p += (-p & 3); IP = p),
	CODE("dolit", push data[IP >> 2]; IP += 4),
	CODE("dovar", push WP + 4),
	IMMD("[",     compile = 0),
	CODE("]",     compile = 1),
    IMMD("(",     next_idiom(')')),
    IMMD(".(",    cout << next_idiom(')')),
    IMMD("\\",    next_idiom('\n')),
    IMMD("$*", 
         int n = find("dostr");
         comma_s(n, next_idiom('"'))),
    IMMD(".\"",
         int n = find("dotstr");
         comma_s(n, next_idiom('"'))),
	/// Branching ops
	CODE("branch", IP = data[IP >> 2]; next()),
	CODE("0branch",
        if (top == 0) IP = data[IP >> 2];
        else IP += 4;  pop; next()),
	CODE("donext",
         if (rack[R]) {
             rack[R] -= 1; IP = data[IP >> 2];
         }
         else { IP += 4;  R--; }
         next()),
	IMMD("if", 
         comma(find("0branch")); push DP;
         comma(0)),
    IMMD("else",
         comma(find("branch")); data[top >> 2] = DP + 4;
         top = DP; comma(0)),
    IMMD("then", data[top >> 2] = DP; pop),
	/// Loops
	IMMD("begin",  push DP),
    IMMD("while",
         comma(find("0branch")); push DP;
         comma(0)),
    IMMD("repeat",
         comma(find("branch")); int n = top; pop;
         comma(top); pop; data[n >> 2] = DP),
	IMMD("again", 
         comma(find("branch"));
         comma(top); pop),
    IMMD("until", 
         comma(find("0branch"));
         comma(top); pop),
	///  For loops
	IMMD("for", comma((find(">r"))); push DP),
	IMMD("aft",
         pop;
         comma((find("branch"))); comma(0); push DP; push DP - 4),
    IMMD("next",
         comma(find("donext")); comma(top); pop),
	///  Compiler ops
	CODE("exit",  IP = popR; next()),
	CODE("docon", push data[(WP + 4) >> 2]),
    CODE(":",
         string s = next_idiom();
         link = DP + 4; comma_s(context, s);
         comma(cData[find("nest")]); compile = 1),
	IMMD(";",
         context = link; compile = 0;
         comma(find("unnest"))),
	CODE("variable", 
         string s = next_idiom();
         link = DP + 4; comma_s(context, s);
         context = link;
         comma(cData[find("dovar")]); comma(0)),
	CODE("constant",
         string s = next_idiom();
         link = DP + 4; comma_s(context, s);
         context = link;
         comma(cData[find("docon")]); comma(top); pop),
	CODE("@",  top = data[top >> 2]),
	CODE("!",  int a = top; pop; data[a >> 2] = top; pop),
	CODE("?",  cout << data[top >> 2] << " "; pop),
	CODE("+!", int a = top; pop; data[a >> 2] += top; pop),
	CODE("allot",
         int n = top; pop;
         for (int i = 0; i < n; i++) cData[DP++] = 0),
	CODE(",",  comma(top); pop),
	/// metacompiler
	CODE("create",
         string s = next_idiom();
         link = DP + 4; comma_s(context, s);
         context = link;
         comma(find("nest")); comma(find("dovar"))),
	CODE("does", comma(find("nest"))), // copy words after "does" to new the word
	CODE("to",                         // n -- , compile only
         int n = find(next_idiom());
         data[(cfa + 4) >> 2] = top; pop),
	CODE("is",                         // w -- , execute only
         int n = find(next_idiom());
         data[cfa >> 2] = top; pop),
	CODE("[to]",
         int n = data[IP >> 2]; data[(n + 4) >> 2] = top; pop),
	/// Debug ops
	CODE("bye",   exit(0)),
	CODE("here",  push DP),
	CODE("words", words()),
	CODE("dump",  dump()),
	CODE("'" ,    push find(next_idiom())),
	CODE("see",
         int n = find(next_idiom());
         for (int i = 0; i < 20; i++) cout << data[(n >> 2) + i];
         cout << endl),
	CODE("ucase", ucase = top; pop),
    CODE("boot",  DP = find("boot") + 4; link = nfa)
};

// Macro Assembler
void encode(const Code *prim) {
    string seq = prim->name;
    int immd = prim->immd;
	len = seq.length();
	comma(link);                    // CC: link field (U32 now)
	link = DP;
	cData[DP++] = len | immd;		// CC: attribute byte = length(0x1f) + immediate(0x80)
	for (int i = 0; i < len; i++) { cData[DP++] = seq[i]; }
	while (DP & 3) { cData[DP++] = 0; }
	comma(P++); 					/// CC: cfa = sequential bytecode (U32 now)
	cout << P - 1 << ':' << DP - 4 << ' ' << seq << endl;
}

void run(int n) {					/// inner interpreter, CC: P, WP, IP, R, bytecode modified
	P = n; WP = n; IP = 0; R = 0;
	do {
        bytecode = cData[P++];		/// CC: bytecode is U8, storage is U32, using P++ is incorrect
        primitives[bytecode].xt();	/// execute colon
	} while (R != 0);
}

void outer() {
	cout << "outer interpreter v3.6" << setbase(base) << endl;
	while (cin >> idiom) {
        if (find(idiom)) {
            if (compile && ((cData[nfa] & IMEDD)==0))
                comma(cfa);
            else  run(cfa);
        }
        else {
            char* p;
            int n = (int)strtol(idiom.c_str(), &p, base);
            if (*p != '\0') {///  not number
                cout << idiom << "? " << endl;///  display error prompt
                compile = 0;///  reset to interpreter mode
                getline(cin, idiom, '\n');///  skip the entire line
            }
            else {
                if (compile) { comma(find("dolit")); comma(n); }
                else { push n; }
            }
        }
        if (cin.peek() == '\0' && !compile) ss_dump();
	} ///  * dump stack and display ok prompt
}

///  Main Program
int main(int ac, char* av[]) {
	cData = (unsigned char*)data;
	IP = 0; link = 0; P = 0;
	S = 0; R = 0;
	cout << "Build dictionary" << setbase(16) << endl;
	for (int i=0; i<sizeof(primitives)/sizeof(Code); i++) encode(&primitives[i]);

	context = DP - 12;
	cout << "\n\nPointers DP=" << DP << " Link=" << context << " Words=" << P << endl;
	// dump dictionary
	cout << "\nDump dictionary\n" << setbase(16);
	P = 0;
	for (len = 0; len < 100; len++) { CheckSum(); }
	// Boot Up
	P = 0; WP = 0; IP = 0; S = 0; R = 0;
	top = -1;
	cout << "\nceForth v3.6, 22sep21cht\n" << setbase(16);
	words();
	outer();
}
/* End of ceforth_36.cpp */

