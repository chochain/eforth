/******************************************************************************/
/* ceForth_36.cpp, Version 3.6 : Forth in C                                   */
/******************************************************************************/
#include <iostream>		// cin, cout
#include <string>       // string class
#include <iomanip>      // setw, setbase, ...
#include <sstream>      // iostringstream
using namespace std;

typedef uint8_t   U8;
typedef uint32_t  U32;
typedef uint32_t  IU;
typedef int32_t   DU;

#define FALSE     0
#define TRUE      -1
#define BOOL(f)   ((f) ? TRUE : FALSE)
#define ALIGN(sz) ((sz) + (-(sz) & 0x3))
#define IMMD_FLAG 0x80
///
/// ForthVM IO streams and input buffer
///
istringstream fin;
ostringstream fout;
string idiom;
#define ENDL      endl; cout << fout.str().c_str(); fout.str("")
///
/// ForthVM global variables
///
DU rack[256] = { 0 };
DU stack[256] = { 0 };
U8  R = 0, S = 0, bytecode;
IU  P, IP, WP, nfa;
DU  top;
IU  DP, thread, context;					// CC: DP, context and thread usage are interchangable
U32 ucase = 1, compile = 0, base = 16;

IU iData[16000] = {};
U8 *cData = (U8*)iData;

int  pop()        { int n = top; top = stack[(unsigned char)S--]; return n; }
void push(int v)  { stack[(unsigned char)++S] = top; top = v; }
int  popR()       { return rack[(unsigned char)R--]; }
void pushR(int v) { rack[(unsigned char)++R] = v; }
void next()       { P = iData[IP >> 2]; WP = P; IP += 4; }
void nest()       { pushR(IP); IP = WP + 4; next(); }
void unnest()     { IP = popR(); next(); }
void comma(int n) { iData[DP >> 2] = n; DP += 4; }
void comma_s(int lex, string s) {
	int len = s.length();
	comma(lex);
	cData[DP++] = len;
	for (int i = 0; i < len; i++) { cData[DP++] = s[i]; }
	while (DP & 3) { cData[DP++] = 0; }
}
string next_idiom(char delim = 0) {
	delim ? getline(fin, idiom, delim) : fin >> idiom; return idiom;
}
void dot_r(int n, int v) {
	fout <<setw(n) << setfill(' ') << v;
}
///
/// scan through dictionary, return found cfa or 0
///
int find(string s) {				// CC: nfa, lfa, cfa, len modified
	int len_s = s.length();
	nfa = context;								// searching from tail
	while (nfa) {								// break on (nfa == 0)
        int lfa = nfa - sizeof(int);           	// CC: 4 = sizeof(IU)
        int len = (int)cData[nfa] & 0x1f;		// CC: 0x1f = ~IMMD_FLAG
        if (len_s == len) {
        	bool ok = true;
        	const char *c = (const char*)&cData[nfa+1], *p = s.c_str();
        	for (int i=0; ok && i<len; i++, c++, p++) {
        		ok = (ucase && *c > 0x40)
        			? ((*c & 0x5f) == (*p & 0x5f))
        			: (*c == *p);
        	}
            if (ok) return ALIGN(nfa + len + 1);
        }
        nfa = iData[lfa >> 2];
	}
	return 0;						// not found
}
void words() {
	int i = 0;
	int n = context; 				// CONTEXT, from tail of the dictionary
	while (n) {
        int lfa = n - sizeof(int);
        int len = (int)cData[n] & 0x1f;
        for (int i = 0; i < len; i++)
            fout << cData[n + 1 + i];
        if ((++i % 10) == 0) { fout << ENDL; }
        else                 { fout << ' ';  }
        n = iData[lfa >> 2];
	}
	fout << ENDL;
}
void dump(int a, int n) {
	fout << setbase(16) << ENDL;
	for (int r = 0, sz = ((n+15)/16); r < sz; r++) {
		int p = a + r * 16;
		char sum = 0;
		fout <<setw(4) << p << ": ";
		for (int i = 0; i < 16; i++) {
	        sum += cData[p];
	        fout <<setw(2) << (int)cData[p++] << ' ';
		}
		fout <<setw(4) << (sum & 0xff) << "  ";
		p = a + r * 16;
		for (int i = 0; i < 16; i++) {
	        sum = cData[p++] & 0x7f;
	        fout <<(char)((sum < 0x20) ? '_' : sum);
		}
		fout << ENDL;
	}
	fout << setbase(base) << ENDL;
}
void ss_dump() {
	fout << "< "; for (int i = 1; S>0 && i <= S; i++) { fout <<stack[i] << " "; }
	fout << top << " >ok" << ENDL;
}
///
/// Code - data structure to keep primitive definitions
///
struct Code {
    string name;
    void   (*xt)(void);
    int    immd;
};
#define CODE(s, g)  { s, []{ g; }, 0 }
#define IMMD(s, g)  { s, []{ g; }, IMMD_FLAG }
const static struct Code primitives[] = {
    /// Execution flow ops
	CODE("ret",   next()),
	CODE("nop",   {}),
	CODE("nest",  nest()),
	CODE("unnest",unnest()),
	/// Stack ops
	CODE("dup",   stack[++S] = top),
	CODE("drop",  pop()),
	CODE("over",  push(stack[S])),
	CODE("swap",  int n = top; top = stack[S]; stack[S] = n),
	CODE("rot",
         int n = stack[(S - 1)];
         stack[(S - 1)] = stack[S];
         stack[S] = top; top = n),
	CODE("pick",  top = stack[(S - top)]),
	CODE(">r",    rack[++R] = pop()),
	CODE("r>",    push(rack[R--])),
	CODE("r@",    push(rack[R])),
	/// Stack ops - double
	CODE("2dup",  push(stack[S]); push(stack[S])),
	CODE("2drop", pop(); pop()),
	CODE("2over", push(stack[S - 2]); push(stack[S - 2])),
	CODE("2swap", 
         int n = pop(); int m = pop(); int l = pop(); int i = pop();
         push(m); push(n); push(i); push(l)),
	/// ALU ops
	CODE("+",     int n = pop(); top += n),
	CODE("-",     int n = pop(); top -= n),
	CODE("*",     int n = pop(); top *= n),
	CODE("/",     int n = pop(); top /= n),
	CODE("mod",   int n = pop(); top %= n),
	CODE("*/",    int n = pop(); int m = pop(); int l = pop(); push(m*l/n)),
	CODE("/mod",
         int n = pop(); int m = pop();
         push(m % n); push(m / n)),
	CODE("*/mod",
         int n = pop(); int m = pop(); int l = pop();
         push((m * l) % n); push((m * l) / n)),
	CODE("and",   top &= stack[S--]),
	CODE("or",    top |= stack[S--]),
	CODE("xor",   top ^= stack[S--]),
	CODE("abs",   top = abs(top)),
	CODE("negate",top = -top),
	CODE("max",   int n = pop(); top = max(top, n)),
	CODE("min",   int n = pop(); top = min(top, n)),
	CODE("2*",    top *= 2),
	CODE("2/",    top /= 2),
	CODE("1+",    top += 1),
	CODE("1-",    top -= 1),
	/// Logic ops
	CODE("0=",    top = BOOL(top == 0)),
    CODE("0<",    top = BOOL(top < 0)),
	CODE("0>",    top = BOOL(top > 0)),
	CODE("=",     int n = pop(); top = BOOL(top == n)),
	CODE(">",     int n = pop(); top = BOOL(top > n)),
	CODE("<",     int n = pop(); top = BOOL(top < n)),
	CODE("<>",    int n = pop(); top = BOOL(top != n)),
	CODE(">=",    int n = pop(); top = BOOL(top >= n)),
	CODE("<=",    int n = pop(); top = BOOL(top <= n)),
	/// IO ops
	CODE("base@", push(base)),
	CODE("base!", fout <<setbase(base = pop())),
	CODE("hex",   fout <<setbase(base = 16)),
	CODE("decimal",fout <<setbase(base = 10)),
	CODE("cr",    fout << ENDL),
	CODE(".",     fout << pop() << " "),
	CODE(".r",    int n = pop(); dot_r(n, pop())),
	CODE("u.r",   int n = pop(); dot_r(n, abs(pop()))),
	CODE(".s",    ss_dump()),
	CODE("key",   push(next_idiom()[0])),
	CODE("emit",  fout << (char)pop()),
	CODE("space", fout << ' '),
	CODE("spaces",for (int n = pop(), i = 0; i < n; i++) fout << ' '),
	/// Literal ops
	CODE("dostr",
         int p = IP; push(p); int n = cData[p];
         p += (n + 1); p += (-p & 3); IP = p),
	CODE("dotstr",
         int p = IP; int n = cData[p++];
         for (int i = 0; i < n; i++) fout <<cData[p++];
         p += (-p & 3); IP = p),
	CODE("dolit", push(iData[IP >> 2]); IP += 4),
	CODE("dovar", push(WP + 4)),
	IMMD("[",     compile = 0),
	CODE("]",     compile = 1),
    IMMD("(",     next_idiom(')')),
    IMMD(".(",    fout <<next_idiom(')')),
    IMMD("\\",    next_idiom('\n')),
    IMMD("$*",    comma_s(find("dostr"), next_idiom('"'))),
    IMMD(".\"",   comma_s(find("dotstr"), next_idiom('"'))),
	/// Branching ops
	CODE("branch", IP = iData[IP >> 2]; next()),
	CODE("0branch",
         IP = top ? IP + 4 : iData[IP >> 2];
         pop(); next()),
	CODE("donext",
         if (rack[R]) {
             rack[R] -= 1; IP = iData[IP >> 2];
         }
         else { IP += 4;  R--; }
         next()),
	IMMD("if",    comma(find("0branch")); push(DP); comma(0)),
    IMMD("else",
         comma(find("branch")); iData[top >> 2] = DP + 4;
         top = DP; comma(0)),
    IMMD("then",  iData[pop() >> 2] = DP),
	/// Loops
	IMMD("begin", push(DP)),
    IMMD("while", comma(find("0branch")); push(DP); comma(0)),
    IMMD("repeat",
         int n = pop();
         comma(find("branch")); comma(pop()); iData[n >> 2] = DP),
	IMMD("again", comma(find("branch")); comma(pop())),
    IMMD("until", comma(find("0branch")); comma(pop())),
	///  For loops
	IMMD("for",   comma((find(">r"))); push(DP)),
	IMMD("aft",
         pop();
         comma((find("branch"))); comma(0); push(DP); push(DP - 4)),
    IMMD("next",  comma(find("donext")); comma(pop())),
	///  Compiler ops
	CODE("exit",  IP = popR(); next()),
	CODE("docon", push(iData[(WP + 4) >> 2])),
    CODE(":",
         thread = DP + 4; comma_s(context, next_idiom());
         comma(cData[find("nest")]); compile = 1),
	IMMD(";",
         context = thread; compile = 0;
         comma(find("unnest"))),
	CODE("variable", 
         thread = DP + 4; comma_s(context, next_idiom());
         context = thread;
         comma(cData[find("dovar")]); comma(0)),
	CODE("constant",
         thread = DP + 4; comma_s(context, next_idiom());
         context = thread;
         comma(cData[find("docon")]); comma(pop())),
	CODE("@",  top = iData[top >> 2]),
	CODE("!",  int a = pop(); iData[a >> 2] = pop()),
	CODE("?",  fout <<iData[pop() >> 2] << " "),
	CODE("+!", int a = pop(); iData[a >> 2] += pop()),
	CODE("allot",
         for (int n=pop(), i = 0; i < n; i++) cData[DP++] = 0),
	CODE(",",  comma(pop())),
	/// metacompiler
	CODE("create",
         thread = DP + 4; comma_s(context, next_idiom());
         context = thread;
         comma(find("nest")); comma(find("dovar"))),
	CODE("does", comma(find("nest"))), // copy words after "does" to new the word
	CODE("to",   int n = find(next_idiom()); iData[(n + 4) >> 2] = pop()),
	CODE("is",   int n = find(next_idiom()); iData[n >> 2] = pop()),
	CODE("[to]", int n = iData[IP >> 2]; iData[(n + 4) >> 2] = pop()),
	/// Debug ops
	CODE("bye",   exit(0)),
	CODE("here",  push(DP)),
	CODE("words", words()),
	CODE("dump",  int n = pop(); int a = pop(); dump(a, n)),
	CODE("'" ,    push(find(next_idiom()))),
	CODE("see",
         int n = find(next_idiom());
         for (int i = 0; i < 20; i++) fout <<iData[(n >> 2) + i];
         fout << ENDL),
	CODE("ucase", ucase = pop()),
    CODE("boot",  DP = find("boot") + sizeof(int); thread = nfa)
};
// Macro Assembler
void encode(const struct Code *prim) {
    const char *seq = prim->name.c_str();
	int sz = prim->name.length();
	comma(thread);                    // CC: thread field (U32 now)
	thread = DP;
	cData[DP++] = sz | prim->immd;	// CC: attribute byte = length(0x1f) + immediate(0x80)
	for (int i = 0; i < sz; i++) { cData[DP++] = seq[i]; }
	while (DP & 3) { cData[DP++] = 0; }
	comma(P++); 					/// CC: cfa = sequential bytecode (U32 now)
	fout << P - 1 << ':' << DP - 4 << ' ' << seq << ENDL;
}
void run(int n) {					/// inner interpreter, CC: P, WP, IP, R, bytecode modified
	P = n; WP = n; IP = 0; R = 0;
	do {
        bytecode = cData[P++];		/// CC: bytecode is U8, storage is U32, using P++ is incorrect
        primitives[bytecode].xt();	/// execute colon
	} while (R != 0);
}
void forth_outer(const char *cmd) {
	fin.clear();
	fin.str(cmd);
	fout.str("");
	while (fin >> idiom) {
		int cf = find(idiom);
        if (cf) {
            if (compile && ((cData[nfa] & IMMD_FLAG)==0))
                comma(cf);
            else run(cf);
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
                else { push(n); }
            }
        }
	}
    if (!compile) ss_dump();  ///  * dump stack and display ok prompt
}
void forth_init() {
	cData = (unsigned char*)iData;
	IP = 0; thread = 0; P = 0;
	S = 0; R = 0;

	fout << setbase(16);
	for (int i=0; i<sizeof(primitives)/sizeof(Code); i++) encode(&primitives[i]);

	context = DP - 12;
	fout <<"\nhere=" << DP << " link=" << context << " words=" << P << ENDL;

	dump(0, DP);

	P = 0; WP = 0; IP = 0; S = 0; R = 0;
	top = -1;
	fout << setbase(base);
}
int main(int ac, char* av[]) {
	///
	/// setup ForthVM
	///
	forth_init();
	words();
	///
	/// enter main loop
	///
	string buf;
	buf.reserve(256);
	fout <<"\nceForth v3.6, 22sep21cht\n";
	while (getline(cin, buf, '\n')) {
		forth_outer(buf.c_str());
	}
}

