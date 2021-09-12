#include <stdint.h>		// uintxx_t
#include <stdlib.h>		// strtol
#include <string.h>		// strcmp
#include <exception>	// try...catch, throw
///
/// macro for alignment and standard units
///
#define ALIGN2(sz)		((sz) + (-(sz) & 0x1))
#define ALIGN4(sz)		((sz) + (-(sz) & 0x3))
typedef uint32_t U32;
typedef uint16_t U16;
typedef uint8_t  U8;
///
/// array class template (so we don't have dependency on C++ STL)
/// Note:
///   * using decorator pattern
///   * this is similar to vector class but much simplified
///
template<class T, int N>
struct List {
    T   v[N];           /// fixed array storage
    int idx = 0;		/// current index of array

    T& operator[](int i)   { return i < 0 ? v[idx + i] : v[i]; }
    T pop() {
    	if (idx>0) return v[idx--];
    	throw "ERR: List empty";
    }
    T push(T t) {
    	if (idx<N) return v[idx++] = t;
    	throw "ERR: List full";
    }
    void push(T *a, int n)  { for (int i=0; i<n; i++) push(*(a+i)); }
    void merge(List& a)     { for (int i=0; i<a.idx; i++) push(a[i]);}
    void clear(int i=0)     { idx=i; }
};
///
/// functor implementation - for lambda support (without STL)
///
struct fop { virtual void operator()(int) = 0; };
template<typename F>
struct XT : fop {			// universal functor
    F fp;
    XT(F &f) : fp(f) {}
    void operator()(int c) { fp(c); }
};
///
/// universal Code class
/// Note:
///   * 12-byte on 32-bit machine, 24-byte on 64-bit machine
///
struct Code {
    const char *name = 0;	// name field
    fop *xt = 0;			// lambda pointer
    U16 def:  1;			// colon defined word
    U16 immd: 1;			// immediate flag
    U16 len:  14;			// len of pf
    U16 pf = 0;				// offset to heap space

    template<typename F>
    Code(const char *n, F f, bool im=false) : name(n), def(0), len(0) {
    	xt = new XT<F>(f);
    	immd = im ? 1 : 0;
    }
};
///
/// main storages in RAM
/// Note:
///   * by separating heap from dictionary, it makes dictionary uniform size
///   * (i.e. the RISC vs CISC debate) which eliminate the need for link field
///   * however, it require size tuning manually
///
List<int,   64>      rs;
List<int,   64>      ss;
List<Code*, 1024>    dict;	// fixed sized dictionary (RISC vs CISC)
List<U8,    48*1024> heap;	// storage for all colon definitions
///
/// system variables
///
bool compile = false;
int  top = 0, base = 10;
int  WP = 0, IP = 0;
///
/// dictionary search functions - can be adapted for ROM+RAM
///
int find(const char *s) {
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (strcmp(s, dict[i]->name)==0) return i;
    }
    return -1;
}
///
/// colon word compiler
/// Note:
///   * we separate dict and heap space to make dict uniform in size
///   * if they are combined then can behaves similar to classic Forth
/// 
void colon(const char *name) {
	const char *nfd = (const char*)&heap[heap.idx];      // current heap pointer
    dict.push(new Code(nfd, [](int){}));    // create a new word on dictionary
    dict[-1]->def = 1;                      // specify a colon word
    dict[-1]->len = 0;                		// advance counter (by number of U16)
    int sz  = strlen(name)+1;				// string length
    int asz = ALIGN2(sz);        			// 16-bit alignment
    heap.push((U8*)name, asz); 		        // copy string into heap space
    dict[-1]->pf  = heap.idx;               // capture starting index
};
void addcode(U16 c) {                       // add an opcode to pf 
	heap.push((U8*)&c, sizeof(U16));
	dict[-1]->len += sizeof(U16);           // advance for 16-bit integer
}
void addvar(int n) {                        // add a dovar
    addcode(find("dovar"));                 // dovar,
    heap.push((U8*)&n, sizeof(int));        // int
    dict[-1]->len += size(U32);           	// advance for a 32-bit integer
}
void addstr(const char *s) {                // add a string
    int sz = ALIGN2(strlen(s)+1);           // padding to 2-byte align
    addcode(find("dotstr"));                // dostr,
    heap.push((U8*)s, sz);                  // byte0, byte1, byte2, ..., byteN
    dict[-1]->len += sz;                    // skip to next opcode
}    
void nest(int c) {
    Code *w = dict[c];						// get word on dictionary
    // primitives
    if (!w->def) { (*(w->xt))(c); return; }
    // colon words
    rs.push(WP); rs.push(IP); WP = c; IP = 0;
    try {
        int n = w->len;						// how many
    	while (IP < n) {
    		int i = heap[w->pf + IP];		// fetch instruction from heap space
    		nest(i);
            IP += sizeof(U16);              // advance to next instruction
    	}
    }
    catch(...) {}
    IP = rs.pop(); WP = rs.pop();
}
///
/// utilize C++ standard template libraries for core functions only
///
#include <iostream>     // cin, cout
#include <string>		// string class
using namespace std;
#define ENDL    endl
string strbuf;
///
/// debug functions
///
void ss_dump() {
	cout << " <"; for (int i=0; i<ss.idx; i++) { cout << ss[i] << " "; }
	cout << top << "> ok" << ENDL;
}
void see(int c) {
	// TODO
}
void words() {
	for (int i=dict.idx - 1; i>=0; i--) {
		Code *w = dict[i];
		cout << w->name << " " << i << (w->immd ? "* " : " ");
		if ((i%10)==0) cout << ENDL;
	}
}
///
/// macros
///
#define PUSH(v) { ss.push(top); return top = v; }
#define POP()   { int n=top; top=ss.pop(); return n; }
#define CODE(s, g) { s, [&](int c){ g; }, 0 }
#define IMMD(s, g) { s, [&](int c){ g; }, 1 }
///
/// primitives (ROMable)
/// Note:
///   * we merge prim into dictionary in main()
///   * However, since primitive is statically compiled
///   * it can be stored in ROM, and only
///   * find() needs to be modified to support ROM+RAM
///
auto _colon = [&](int c) {
	cin >> strbuf;
	colon(strbuf.c_str());
	compile=true;
};
static Code prim[] = {
    CODE("dup",  PUSH(top)),
    CODE("drop", top = ss.pop()),
    CODE("over", PUSH(ss[-1])),
    CODE("swap", int n = ss.pop(); PUSH(n)),
    CODE("+",    top += ss.pop()),
    CODE("-",    top =  ss.pop() - top),
    CODE(".",       cout << POP() << " "),
    CODE("dotstr",
         int x = dict[WP]->pf+IP+sizeof(U16);
         const char *s = (char*)&heap[i];
         cout << s;
         IP += ALIGN2(strlen(s)+1)),
    CODE("dolit",
         int x  = dict[WP]->pf+IP+sizeof(U16);
         int *i = (int*)&heap[x];
         PUSH(*i); IP += sizeof(int)),
    CODE("dovar",
         int x  = dict[WP]->pf+IP+sizeof(U16);
         PUSH(x); IP += sizeof(int)),
    IMMD(".\"",
         getline(cin, strbuf, '"');
         addstr(strbuf.substr(1).c_str())),
    CODE(":",
    	cin >> strbuf;
    	colon(strbuf.c_str());
    	compile=true),
    IMMD(";", compile = false),
    CODE("words", words())
};
const int PSZ = sizeof(prim)/sizeof(Code);
///
/// outer interpreter
///
int outer() {
	while (cin >> strbuf) {
		const char *idiom = strbuf.c_str();
		printf("%s=>", idiom);
		if (strcmp("bye", idiom)==0) return 0;
		int w = find(idiom);           /// * search through dictionary
		if (w>=0) {                                 /// * word found?
			printf("%s\n", dict[w]->name);
			if (compile && !dict[w]->immd) {        /// * in compile mode?
				addcode(w);                         /// * add to colon word
	        }
	        else nest(w);                           /// * execute forth word
	        continue;
	    }
	    // try as a number
	    char *p;
	    int n = static_cast<int>(strtol(idiom, &p, base));
	    printf("%d\n", n);
	    if (*p != '\0') {                           /// * not number
	    	cout << idiom << "? " << ENDL;          ///> display error prompt
	        compile = false;                        ///> reset to interpreter mode
	        break;                                  ///> skip the entire input buffer
	    }
	    // is a number
	    if (compile) addvar(n);                     /// * a number in compile mode?
	    else PUSH(n);                               ///> or, add value onto data stack
	}
	if (!compile) ss_dump();
	return 1;
}
void init() {
	for (int i=0; i<PSZ; i++) {						/// copy prim(ROM) into RAM dictionary,
		dict.push(&prim[i]);		                /// we don't need to do this if modify
	}                                               /// find to support both
	words();
}
int main(int ac, char* av[]) {
	init();
	cout << unitbuf << "eForth8" << ENDL;
	while (outer());
	cout << "Done." << ENDL;
	return 0;
}

