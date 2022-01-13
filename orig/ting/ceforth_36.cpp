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

//Preamble

#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <stdarg.h>
#include <string>
#include <iostream>
#include <exception> 
#include <iomanip>      // setw, setbase, ...
//using namespace std;

# define  FALSE 0
# define  TRUE  -1
# define  LOGICAL ? -1 : 0
# define  LOWER(x,y) ((unsigned long)(x)<(unsigned long)(y))
# define  pop top = stack[(unsigned char)S--]
# define  push stack[(unsigned char)++S] = top; top =
# define  popR rack[(unsigned char)R--]
# define  pushR rack[(unsigned char)++R]
# define  ALIGN(sz) ((sz) + (-(sz) & 0x3))
int  IMEDD = 0x80;

int rack[256] = { 0 };
int stack[256] = { 0 };
unsigned char R = 0, S = 0, bytecode, c;
int* Pointer;
int  P, IP, WP, top, len;
int  lfa, nfa, cfa, pfa;
int  DP, link, context;
int  ucase = 1, compile = 0, base = 16;
std::string idiom, s;

int data[16000] = {};
unsigned char* cData = (unsigned char*)data;

void next() { P = data[IP >> 2]; WP = P; IP += 4; }
void nest() { pushR = IP; IP = WP + 4; next(); }
void unnest() { IP = popR; next(); }
void comma(int n) { data[DP >> 2] = n; DP += 4; }
void comma_s(int lex, std::string s) {
	comma(lex); len = s.length(); cData[DP++] = len;
	for (int i = 0; i < len; i++) { cData[DP++] = s[i]; }
	while (DP & 3) { cData[DP++] = 0; }
}
std::string next_idiom(char delim = 0) {
	std::string s; delim ? std::getline(std::cin, s, delim) : std::cin >> s; return s;
}
void dot_r(int n, int v) {
	std::cout << std::setw(n) << std::setfill(' ') << v;
}
int find(std::string s) {
	int len_s = s.length();
	nfa = context;
	while (nfa) {
	lfa = nfa - 4;
	len = (int)cData[nfa] & 0x1f;
	if (len_s == len) {
	int success = 1;
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
	nfa = context; // CONTEXT
	while (nfa) {
	lfa = nfa - 4;
	len = (int)cData[nfa] & 0x1f;
	for (int i = 0; i < len; i++)
	std::cout << cData[nfa + 1 + i];
	std::cout << ' ';
	nfa = data[lfa >> 2];
	}
	std::cout << std::endl;
}
void CheckSum() {
	int i; pushR = P; char sum = 0;
	std::cout << std::setw(4) << std::setbase(16) << P << ": ";
	for (i = 0; i < 16; i++) {
	sum += cData[P];
	std::cout << std::setw(2) << (int)cData[P++] << ' ';
	}
	std::cout << std::setw(4) << (sum & 0XFF) << "  ";
	P = popR;
	for (i = 0; i < 16; i++) {
	sum = cData[P++] & 0x7f;
	std::cout << (char)((sum < 0x20) ? '_' : sum);
	}
	std::cout << std::endl;
}
void dump() {// a n --
	std::cout << std::endl;
	len = top / 16; pop; P = top; pop;
	for (int i = 0; i < len; i++) { CheckSum(); }
}
void ss_dump() {
	std::cout << "< "; for (int i = S - 4; i < S + 1; i++) { std::cout << stack[i] << " "; }
	std::cout << top << " >ok" << std::endl;
}
void(*primitives[120])(void) = {
	/// Stack ops
	/* -1 "ret" */ [] {next(); },
	/* 0 "opn" */ [] {},
	/* 1 "nest" */ [] {nest(); },
	/* 2 "unnest" */ [] {unnest(); },
	/* 3 "dup" */ [] {stack[++S] = top; },
	/* 4 "drop" */ [] {pop; },
	/* 5 "over" */ [] {push stack[(S - 1)]; },
	/* 6 "swap" */ [] {int n = top; top = stack[S];
	stack[S] = n; },
	/* 7 "rot" */ [] {int n = stack[(S - 1)];
	stack[(S - 1)] = stack[S];
	stack[S] = top; top = n; },
	/* 8 "pick" */ [] {top = stack[(S - top)]; },
	/* 9 ">r" */ [] {rack[++R] = top; pop; },
	/* 10 "r>" */ [] {push rack[R--]; },
	/* 11 "r@" */ [] {push rack[R]; },
	/// Stack ops - double
	/* 12 "2dup" */ [] {push stack[(S - 1)]; push stack[(S - 1)]; },
	/* 13 "2drop" */ [] {pop; pop; },
	/* 14 "2over"*/ [] {push stack[(S - 3)]; push stack[(S - 3)]; },
	/* 15 "2swap" */ [] {
	int n = top; pop; int m = top; pop; int l = top; pop; int i = top; pop;
	push m; push n; push i; push l; },
	/// ALU ops
	/* 16 "+" */ [] {int n = top; pop; top += n; },
	/* 17 "-" */ [] {int n = top; pop; top -= n; },
	/* 18 "*" */ [] {int n = top; pop; top *= n; },
	/* 19 "/" */ [] {int n = top; pop; top /= n; },
	/* 20 "mod" */ [] {int n = top; pop; top %= n; },
	/* 21 "* /" */ [] {int n = top; pop; int m = top; pop;
	int l = top; pop; push(m * l) / n; },
	/* 22 "/mod" */ [] {int n = top; pop; int m = top; pop;
	push(m % n); push(m / n); },
	/* 23 "* /mod" */ [] {int n = top; pop; int m = top; pop;
	int l = top; pop; push((m * l) % n); push((m * l) / n); },
	/* 24 "and" */ [] {top &= stack[S--]; },
	/* 25 "or"  */ [] {top |= stack[S--]; },
	/* 26 "xor" */ [] {top ^= stack[S--]; },
	/* 27 "abs" */ [] {top = abs(top); },
	/* 28 "negate" */ [] {top = -top; },
	/* 29 "max" */ [] {int n = top; pop; top = std::max(top, n); },
	/* 30 "min" */ [] {int n = top; pop; top = std::min(top, n); },
	/* 31 "2*"  */ [] {top *= 2; },
	/* 32 "2/"  */ [] {top /= 2; },
	/* 33 "1+"  */ [] {top += 1; },
	/* 34 "1-"  */ [] {top += -1; },
	/// Logic ops
	/* 35 "0=" */ [] {top = (top == 0) LOGICAL; },
	/* 36 "0<" */ [] {top = (top < 0) LOGICAL; },
	/* 37 "0>" */ [] {top = (top > 0) LOGICAL; },
	/* 38 "="  */ [] {int n = top; pop; top = (top == n) LOGICAL; },
	/* 39 ">"  */ [] {int n = top; pop; top = (top > n) LOGICAL; },
	/* 40 "<"  */ [] {int n = top; pop; top = (top < n) LOGICAL; },
	/* 41 "<>" */ [] {int n = top; pop; top = (top != n) LOGICAL; },
	/* 42 ">=" */ [] {int n = top; pop; top = (top >= n) LOGICAL; },
	/* 43 "<=" */ [] {int n = top; pop; top = (top <= n) LOGICAL; },
	/// IO ops
	/* 44 "base@" */ [] {push base; },
	/* 45 "base!" */ [] {base = top; pop; std::cout << std::setbase(base); },
	/* 46 "hex" */ [] {base = 16; std::cout << std::setbase(base); },
	/* 47 "decimal" */ [] {base = 10; std::cout << std::setbase(base); },
	/* 48 "cr" */ [] {std::cout << std::endl; },
	/* 49 "." */ [] {std::cout << top << " "; pop; },
	/* 50 ".r" */ [] {int n = top; pop; dot_r(n, top); pop; },
	/* 51 "u.r" */ [] {int n = top; pop; dot_r(n, abs(top)); pop; },
	/* 52 ".s" */ [] {ss_dump(); },
	/* 53 "key" */ [] {push(next_idiom()[0]); },
	/* 54 "emit" */ [] {char b = (char)top; pop; std::cout << b; },
	/* 55 "space" */ [] {std::cout << " "; },
	/* 56 "spaces" */ [] {int n = top; pop; for (int i = 0; i < n; i++) std::cout << " "; },
	/// Literal ops
	/* 57 "dostr" */ [] {int p = IP; push p; len = cData[p];
	p += (len + 1); p += (-p & 3); IP = p; },
	/* 58 "dotstr" */ [] {int p = IP; len = cData[p++];
	for (int i = 0; i < len; i++) std::cout << cData[p++];
	p += (-p & 3); IP = p; },
	/* 59 "dolit" */ [] {push data[IP >> 2]; IP += 4; },
	/* 60 "dovar" */ [] {push WP + 4; },
	/* 61 [  */ [] {compile = 0; },
	/* 62 ]  */ [] {compile = 1; },
	/* 63 (  */ [] {next_idiom(')'); },
	/* 64 .( */ [] {std::cout << next_idiom(')'); },
	/* 65 \  */ [] {next_idiom('\n'); },
	/* 66 $" */ [] {
	std::string s = next_idiom('"'); len = s.length();
	int n = find("dostr");
	comma_s(n, s); },
	/* 67 ." */ [] {
	std::string s = next_idiom('"'); len = s.length();
	int n = find("dotstr");
	comma_s(n, s); },
	/// Branching ops
	/* 68 "branch" */ [] { IP = data[IP >> 2]; next(); },
	/* 69 "0branch" */ [] {
	if (top == 0) IP = data[IP >> 2];
	else IP += 4;  pop; next(); },
	/* 70 "donext" */ [] {
	if (rack[R]) {
	rack[R] -= 1; IP = data[IP >> 2];
	}
	else { IP += 4;  R--; }
	next(); },
	/* 71 "if" */ [] {
	comma(find("0branch")); push DP;
	comma(0); },
	/* 72 "else" */ [] {
	comma(find("branch")); data[top >> 2] = DP + 4;
	top = DP; comma(0);  },
	/* 73 "then" */ [] {
	data[top >> 2] = DP; pop; },
	/// Loops
	/* 74 "begin" */ [] { push DP; },
	/* 75 "while" */ [] {
	comma(find("0branch")); push DP;
	comma(0); },
	/* 76 "repeat" */ [] {
	comma(find("branch")); int n = top; pop;
	comma(top); pop; data[n >> 2] = DP; },
	/* 77 "again" */ [] {
	comma(find("branch"));
	comma(top); pop; },
	/* 78 "until" */ [] {
	comma(find("0branch"));
	comma(top); pop; },
	///  For loops
	/* 79 "for" */ [] {comma((find(">r"))); push DP; },
	/* 80 "aft" */ [] {pop;
	comma((find("branch"))); comma(0); push DP; push DP - 4; },
	/* 81 "next" */ [] {
	comma(find("donext")); comma(top); pop; },
	///  Compiler ops
	/* 82 "exit" */ [] {IP = popR; next(); },
	/* 83 "docon" */ [] {push data[(WP + 4) >> 2]; },
	/* 84 ":" */ [] {
	std::string s = next_idiom();
	link = DP + 4; comma_s(context, s);
	comma(cData[find("nest")]); compile = 1; },
	/* 85 ";" */ [] {
	context = link; compile = 0;
	comma(find("unnest")); },
	/* 86 "variable" */ [] {
	std::string s = next_idiom();
	link = DP + 4; comma_s(context, s);
	context = link;
	comma(cData[find("dovar")]); comma(0);
	},
	/* 87 "constant" */ [] {
	std::string s = next_idiom();
	link = DP + 4; comma_s(context, s);
	context = link;
	comma(cData[find("docon")]); comma(top); pop; },
	/* 88 "@" */ [] {top = data[top >> 2]; },
	/* 89 "!" */ [] {int a = top; pop; data[a >> 2] = top; pop; },
	/* 90 "?" */ [] {std::cout << data[top >> 2] << " "; pop; },
	/* 91 "+!" */ [] {int a = top; pop; data[a >> 2] += top; pop; },
	/* 92 "allot" */ [] {int n = top; pop;
	for (int i = 0; i < n; i++) cData[DP++] = 0; },
	/* 93 "," */ [] {comma(top); pop; },
	/// metacompiler
	/* 94 "create" */ [] {
	std::string s = next_idiom();
	link = DP + 4; comma_s(context, s);
	context = link;
	comma(find("nest")); comma(find("dovar")); },
	/* 95 "does" */ [] {
	comma(find("nest")); }, // copy words after "does" to new the word
	/* 96 "to" */ [] {// n -- , compile only
	int n = find(next_idiom());
	data[(cfa + 4) >> 2] = top; pop; },
	/* 97 "is" */ [] {// w -- , execute only
	int n = find(next_idiom());
	data[cfa >> 2] = top; pop; },
	/* 98 "[to]" */ [] {
	int n = data[IP >> 2]; data[(n + 4) >> 2] = top; pop; },
	/// Debug ops
	/* 99 "bye" */ [] {exit(0); },
	/* 100 "here" */ [] {push DP; },
	/* 101 "words" */ [] {words(); },
	/* 102 "dump" */ [] {dump(); },
	/* 103 "'" */ [] {push find(next_idiom()); },
	/* 104 "see" */ [] {
	int n = find(next_idiom());
	for (int i = 0; i < 20; i++) std::cout << data[(n >> 2) + i];
	std::cout << std::endl; },
	/* 105 "ucase" */ [] {ucase = top; pop; },
		/* 106 "boot" */ [] {DP = find("boot") + 4; link = nfa; }
};

// Macro Assembler

void CODE(int lex, const char seq[]) {
	len = lex & 31;
	comma(link); link = DP; cData[DP++] = lex;
	for (int i = 0; i < len; i++) { cData[DP++] = seq[i]; }
	while (DP & 3) { cData[DP++] = 0; }
	comma(P++); /// sequential bytecode
	std::cout << seq << ":" << P - 1 << ',' << DP - 4 << ' ';
}

void run(int n) {
	P = n; WP = n; IP = 0; R = 0;
	do {
	bytecode = cData[P++];
	primitives[bytecode]();	/// execute colon
	} while (R != 0);
}

void outer() {
	std::cout << "outer interpreter v3.6" << std::setbase(base) << std::endl;
	while (std::cin >> idiom) {
	if (find(idiom)) {
	if (compile && (((int)cData[nfa] & 0x80) == 0))
	comma(cfa);
	else  run(cfa);
	}
	else {
	char* p;
	int n = (int)strtol(idiom.c_str(), &p, base);
	if (*p != '\0') {///  not number
	std::cout << idiom << "? " << std::endl;///  display error prompt
	compile = 0;///  reset to interpreter mode
	std::getline(std::cin, idiom, '\n');///  skip the entire line
	}
	else {
	if (compile) { comma(find("dolit")); comma(n); }
	else { push n; }
	}
	}
	if (std::cin.peek() == '\0' && !compile) ss_dump();
	} ///  * dump stack and display ok prompt
}

///  Main Program
int main(int ac, char* av[]) {
	cData = (unsigned char*)data;
	IP = 0; link = 0; P = 0;
	S = 0; R = 0;
	std::cout << "Build dictionary" << std::setbase(16) << std::endl;

	// Kernel
	CODE(3, "ret");
	CODE(3, "nop");
	CODE(4, "nest");
	CODE(6, "unnest");
	CODE(3, "dup");
	CODE(4, "drop");
	CODE(4, "over");
	CODE(4, "swap");
	CODE(3, "rot");
	CODE(4, "pick");
	CODE(2, ">r");
	CODE(2, "r>");
	CODE(2, "r@");
	CODE(4, "2dup");
	CODE(5, "2drop");
	CODE(5, "2over");
	CODE(5, "2swap");
	CODE(1, "+");
	CODE(1, "-");
	CODE(1, "*");
	CODE(1, "/");
	CODE(3, "mod");
	CODE(2, "*/");
	CODE(4, "/mod");
	CODE(5, "*/mod");
	CODE(3, "and");
	CODE(2, "or");
	CODE(3, "xor");
	CODE(3, "abs");
	CODE(6, "negate");
	CODE(3, "max");
	CODE(3, "min");
	CODE(2, "2*");
	CODE(2, "2/");
	CODE(2, "1+");
	CODE(2, "1-");
	CODE(2, "0=");
	CODE(2, "0<");
	CODE(2, "0>");
	CODE(1, "=");
	CODE(1, ">");
	CODE(1, "<");
	CODE(2, "<>");
	CODE(2, ">=");
	CODE(2, "<=");
	CODE(5, "base@");
	CODE(5, "base!");
	CODE(3, "hex");
	CODE(7, "decimal");
	CODE(2, "cr");
	CODE(1, ".");
	CODE(2, ".r");
	CODE(3, "u.r");
	CODE(2, ".s");
	CODE(3, "key");
	CODE(4, "emit");
	CODE(5, "space");
	CODE(6, "spaces");
	CODE(5, "dostr");
	CODE(6, "dotstr");
	CODE(5, "dolit");
	CODE(5, "dovar");
	CODE(1 + IMEDD, "[");
	CODE(1, "]");
	CODE(1 + IMEDD, "(");
	CODE(2 + IMEDD, ".(");
	CODE(1 + IMEDD, "\\");
	CODE(2 + IMEDD, "$\"");
	CODE(2 + IMEDD, ".\"");
	CODE(6, "branch");
	CODE(7, "0branch");
	CODE(6, "donext");
	CODE(2 + IMEDD, "if");
	CODE(4 + IMEDD, "else");
	CODE(4 + IMEDD, "then");
	CODE(5 + IMEDD, "begin");
	CODE(5 + IMEDD, "while");
	CODE(6 + IMEDD, "repeat");
	CODE(5 + IMEDD, "again");
	CODE(5 + IMEDD, "until");
	CODE(3 + IMEDD, "for");
	CODE(3 + IMEDD, "aft");
	CODE(4 + IMEDD, "next");
	CODE(4, "exit");
	CODE(5, "docon");
	CODE(1, ":");
	CODE(1 + IMEDD, ";");
	CODE(8, "variable");
	CODE(8, "constant");
	CODE(1, "@");
	CODE(1, "!");
	CODE(1, "?");
	CODE(2, "+!");
	CODE(5, "allot");
	CODE(1, ",");
	CODE(6, "create");
	CODE(4, "does");
	CODE(2, "to");
	CODE(2, "is");
	CODE(4, "[to]");
	CODE(3, "bye");
	CODE(4, "here");
	CODE(5, "words");
	CODE(4, "dump");
	CODE(1, "'");
	CODE(3, "see");
	CODE(5, "ucase");
	CODE(4, "boot");
	context = DP - 12;
	std::cout << "\n\nPointers DP=" << DP << " Link=" << context << " Words=" << P << std::endl;
	// dump dictionary
	std::cout << "\nDump dictionary\n" << std::setbase(16);
	P = 0;
	for (len = 0; len < 100; len++) { CheckSum(); }
	// Boot Up
	P = 0; WP = 0; IP = 0; S = 0; R = 0;
	top = -1;
	std::cout << "\nceForth v3.6, 22sep21cht\n" << std::setbase(16);
	words();
	outer();
}
/* End of ceforth_36.cpp */

