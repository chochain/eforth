#include <iostream>			// cin, cout
#include <iomanip>			// setbase
#include <vector>
#include <functional>
#include <cstring>
#include <string>
#include <exception>

using namespace std;
///
/// .hpp - macros and class prototypes
///
#define FALSE   0
#define TRUE    -1

template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern
    T operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    T operator<<(T t) { v.push_back(t); }
    T pop() {
        if (v.empty()) throw length_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t;
    }
    T dec_i() { int i=(v.back()-=1); if (i<=0) v.pop_back(); return i; }		// decrement rs.top
    void push(T t) { v.push_back(t); }
    void clear() { v.clear(); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
};
#define POP()    (top=ss.pop())
#define PUSH(v)  (ss.push(top),top=(v))

class Code;                                 /// forward declaration
typedef void (*fop)(Code*);                 /// Forth operator

class Code {
public:
    static int fence;                       /// token incremental counter
    string name;                            /// name of word
    int    token = 0;                       /// dictionary order token
    bool   immd = false;                   /// immediate flag
    fop    xt = NULL;                    /// primitive function
    string literal;                         /// string literal
    int    stage = 0;                       /// branching stage
    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<int>   qf;

    Code(string n, fop fn, bool im = false);  /// primitive
    Code(string n, bool f = false);           /// new colon word or temp
    Code(string n, int d);                  /// dolit
    Code(string n, string l);               /// dostr

    Code* immediate();                    /// set immediate flag
    Code* addcode(Code* w);               /// append colon word
    void   exec();                          /// execute word
    string to_s();                          /// debugging
};
///
/// .cpp - Code class implementation
///
/// Forth virtual machine variables
///
ForthList<int>   rs;                        /// return stack
ForthList<int>   ss;                        /// parameter stack
ForthList<Code*> dict;                      /// dictionary

bool compile = false;                          /// compiling flag
int  base = 10;                             /// numeric radix
int  top = -1;                              /// cached top of stack
int  IP, WP;                                /// instruction and parameter pointers
///
/// dictionary search function
///
Code* find(string s) {                   /// search dictionary reversely
    for (int i = dict.v.size() - 1; i >= 0; --i) {
        if (s == dict.v[i]->name) return dict.v[i];
    }
    return NULL;
}
///
/// constructors
///
int Code::fence = 0;
Code::Code(string n, fop fn, bool im) { name = n; token = fence++; xt = fn; immd = im; }
Code::Code(string n, bool f) { Code* c = find(name = n); if (c) xt = c->xt; if (f) token = fence++; }
Code::Code(string n, int d) { xt = find(name = n)->xt; qf.push(d); }
Code::Code(string n, string l) { xt = find(name = n)->xt; literal = l; }
///
/// public methods
///
Code* Code::immediate() { immd = true;  return this; }
Code* Code::addcode(Code* w) { pf.push(w); return this; }
void  Code::exec() {
    if (xt) { xt(this); return; }       /// * execute primitive word and return
    rs.push(WP); rs.push(IP);           /// * execute colon word
    WP = token; IP = 0;                     /// * setup dolist call frame
    for (Code* w : pf.v) {               /// * inner interpreter
        try { w->exec(); IP++; }        /// * pass Code object to xt
        catch (...) {}
    }
    IP = rs.pop(); WP = rs.pop();           /// * return to caller
}
string Code::to_s() {
    return name + " " + to_string(token) + (immd ? "*" : " ");
}
//
// external function examples (instead of inline)
//
void _over() {
    int v = ss[-1];
    PUSH(v);
}
void _2dup() {
    _over(); _over();
}
void ss_dump() {
    cout << "< ";
    for (int i : ss.v) { cout << i << " "; }
    cout << top << " >ok" << endl;
}
void see(Code* c, int dp) {
    auto tab = [](int i, string s) { cout << endl; while (i--) cout << "  "; cout << s; };          // lambda for indentation
    auto qf = [](vector<int> v) { cout << "="; for (int i : v) cout << i << " "; };

    tab(dp, "[ " + c->to_s());
    for (Code* w : c->pf.v)  see(w, dp + 1);    /// call recursively
    if (c->pf2.v.size() > 0) {
        tab(dp, "2--"); for (Code* w : c->pf2.v) see(w, dp + 1);
    }
    if (c->pf1.v.size() > 0) {
        tab(dp, "1--"); for (Code* w : c->pf1.v) see(w, dp + 1);
    }
    if (c->qf.v.size() > 0) qf(c->qf.v);
    cout << "]";
}
void words() {
    int i = 0;
    for (Code* w : dict.v) {
        cout << w->to_s() << " ";
        if ((++i % 10) == 0) cout << endl;
    }
    cout << endl;
}
void _cycles(Code *c) {
	do {
		for (Code * w : c->pf.v) w->exec();
	    cout << "rs:"; for (int i : rs.v) { cout << i << " "; }
		if (rs.dec_i() < 0) break;
    } while (c->stage == 0);
	while (c->stage > 0) {
        for (Code * w : c->pf2.v) w->exec();
	    cout << "rs:" << endl; for (int i : rs.v) { cout << i << " "; }
        if (rs.dec_i() < 0) break;
        for (Code * w : c->pf1.v) w->exec();
    }
}
///
/// macros to reduce verbosity (but harder to single-step debug)
///
#define CODE(s, g) new Code(s, [](Code *c){ g; })
#define IMMD(s, g) new Code(s, [](Code *c){ g; }, true)
///
/// primitives (mostly use lambda but can use external as well)
///
vector<Code*> prim = {
    // IO examples
	CODE("bye",   exit(0)),
    CODE("hi",    cout << "Hello, World!" << endl),
    CODE("qrx",   PUSH(getchar()); if (top != 0) PUSH(TRUE)),
    CODE("txsto", putchar((char)top); POP()),
    // stack op examples
    CODE("dup", PUSH(top)),
    CODE("over", PUSH(ss[-1])),
    CODE("2dup", PUSH(ss[-1]); PUSH(ss[-1])),
    CODE("2over", PUSH(ss[-4]); PUSH(ss[-4])),
    CODE("4dup", PUSH(ss[-4]); PUSH(ss[-4]); PUSH(ss[-4]); PUSH(ss[-4])),
    CODE("swap", int n = ss.pop(); PUSH(n)),
    CODE("rot", int n = ss.pop(); int m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("-rot", int n = ss.pop(); int m = ss.pop(); PUSH(n); PUSH(m)),
    CODE("2swap", int n = ss.pop(); int m = ss.pop(); int l = ss.pop(); ss.push(n); PUSH(l); PUSH(m)),
    CODE("pick", int i=top; top=ss[-i]),
    //    CODE("roll", int i=top; top=ss[-i]),
    CODE("drop", POP()),
    CODE("nip", ss.pop()),
    CODE("2drop", POP(); POP()),
    CODE(">r", rs.push(top); POP()),
    CODE("r>", PUSH(rs.pop())),
    CODE("r@", PUSH(rs.pop());rs.push(top)),
    CODE("push", rs.push(top); POP()),
    CODE("pop", PUSH(rs.pop())),
    // ALU examples
    CODE("hex", base=16),
    CODE("decimal",   base=10),
    CODE("+",  top += ss.pop()),       // note: ss.pop() is different from POP()
    CODE("-",  top = ss.pop() - top),
    CODE("*",  top *= ss.pop()),
    CODE("/",  top = ss.pop() / top),
    CODE("*/", int n = ss.pop(); ss.push(ss.pop()* ss.pop() / n)),
    CODE("*/mod", int n = ss.pop(); int m = ss.pop() * ss.pop();
        ss.push(m% n); ss.push(m / n)),
    CODE("mod", int n = ss.pop(); ss.push(ss.pop() % n)),
    CODE("and", ss.push(ss.pop()& ss.pop())),
    CODE("or", ss.push(ss.pop() | ss.pop())),
    CODE("xor", ss.push(ss.pop() ^ ss.pop())),
    CODE("negate", ss.push(-ss.pop())),
    CODE("abs", ss.push(abs(ss.pop()))),
    // logic
    CODE("0=", top=(top == 0) ? -1 : 0),                     		// CC:
    CODE("0<", ss.push((ss.pop() < 0) ? -1 : 0)),
    CODE("0>", ss.push((ss.pop() > 0) ? -1 : 0)),
    CODE("=",  top=(ss.pop() == top) ? -1 : 0),						// CC:
    CODE(">",  top=(ss.pop() > top) ? -1 : 0),                      // CC:
    CODE("<",  int n = ss.pop(); ss.push((ss.pop() < n) ? -1 : 0)),
    CODE("<>", int n = ss.pop(); ss.push((ss.pop() != n) ? -1 : 0)),
    CODE(">=", int n = ss.pop(); ss.push((ss.pop() >= n) ? -1 : 0)),
    CODE("<=", int n = ss.pop(); ss.push((ss.pop() <= n) ? -1 : 0)),
    // output
    CODE("base@", ss.push(base)),
    CODE("base!", base = ss.pop()),
    CODE("hex", cout << setbase(base=16)),
    CODE("decimal", cout << setbase(base=10)),
    CODE("cr",  cout << ("\n")),
    CODE(".",   cout << top; POP()),
    CODE(".r", int n = ss.pop(); string s = to_string(ss.pop());
        for (int i = 0; (i+s.size())<n; i++) cout << (" ");
        cout << (s + " ")),
    CODE("u.r", int n = ss.pop(); string s = to_string(ss.pop() & 0x7fffffff);
        for (int i = 0; (i+s.size())<n; i++) cout << (" ");
        cout << (s + " ")),
    CODE("key", string s; cin >> s; PUSH(s[0])),
    CODE("emit", char b = (char)ss.pop(); cout << ("" + b)),
    CODE("space", cout << (" ")),
    CODE("spaces", int n = ss.pop(); for (int i = 0; i < n; i++) cout << (" ")),
    // literals
    CODE("dotstr",cout << c->literal),
    CODE("dostr", PUSH(c->token)),
    CODE("dolit", PUSH(c->qf[0])),
    CODE("dovar", PUSH(c->token)),
    CODE("docon", PUSH(c->qf[0])),          		// integer literal
    CODE("[", compile = false),
    CODE("]", compile = true),
    CODE("'", 
        string s; cin >> s;            /// * fetch next token
        Code* w = find(s);
        PUSH(w->token)),
    CODE("$\"", 
        string s; getline(cin, s, '"'); /// * copy string upto delimiter
        dict[-1]->addcode(new Code("dostr", s))),
    IMMD(".\"",
        string s; getline(cin, s, '"'); /// * copy string upto delimiter
        dict[-1]->addcode(new Code("dotstr",s))),
    IMMD("(", 
        string s; getline(cin, s, ')')), 
    IMMD(".(", 
        string s; getline(cin, s, '"'); 
        cout << s),
    CODE("\\", 
        string s; getline(cin, s, '\n')),
    // branching examples
    IMMD("branch",
        bool f = top != 0; POP();           // check flag then update top
        for (Code* w : (f ? c->pf.v : c->pf1.v)) w->exec()),
    IMMD("if",
        dict[-1]->addcode(new Code("branch"));    	// bran=word->pf
        dict.push(new Code("temp"))),           	// use last cell of dictionay as scratch pad
    IMMD("else",
        Code *temp = dict[-1];
        Code *last = dict[-2]->pf[-1];    // branching node
        last->pf.merge(temp->pf.v);
        temp->pf.clear();
        last->stage = 1),
    IMMD("then",
        Code *temp = dict[-1];
        Code *last = dict[-2]->pf[-1];
        if (last->stage == 0) {                 // if...then
            last->pf.merge(temp->pf.v);
            dict.pop();}
        else {                                  // if...else...then
            last->pf1.merge(temp->pf.v);
            if (last->stage == 1) dict.pop();
            else temp->pf.clear();}
        ),
    // loops
    CODE("loops",
    	while (true) {
    		for (Code * w : c->pf.v) w->exec();
    		int f = top;
    		if (c->stage == 0) {				// ...until
    			POP(); if (f != 0) break;
    		}
    		if (c->stage == 2) {				// while...repeat
    			POP(); if (f == 0) break;
    			for (Code * w : c->pf1.v) w->exec();
    		}
    	}),
    IMMD("begin", 
        dict[-1]->addcode(new Code("loops"));
        dict.push(new Code("temp"))),
    IMMD("while", 
        Code* last = dict[-2]->pf[-1];
        Code* temp = dict[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear();
        last->stage = 2),
    IMMD("repeat", 
        Code* last = dict[-2]->pf[-1];
        Code* temp = dict[-1];
        last->pf1.merge(temp->pf.v);
        dict.pop()),
    IMMD("again", 
        Code* last = dict[-2]->pf[-1];
        Code* temp = dict[-1];
        last->pf.merge(temp->pf.v);
        last->stage = 1;
        dict.pop()),
    IMMD("until", 
        Code* last = dict[-2]->pf[-1];
        Code* temp = dict[-1];
        last->pf.merge(temp->pf.v);
        dict.pop()),
    // for next
    CODE("cycles", _cycles(c)),
    	/*
		do {
			for (Code * w : c->pf.v) w->exec();
			if (rs.top_dec() < 0) break;
        } while (c->stage == 0);
		while (c->stage > 0) {
            for (Code * w : c->pf2.v) w->exec();
            if (rs.top_dec() < 0) break;
            for (Code * w : c->pf1.v) w->exec();
        }),
        */
    IMMD("for",
        dict[-1]->addcode(new Code(">r"));
        dict[-1]->addcode(new Code("cycles"));
        dict.push(new Code("temp"))),
    IMMD("aft", 
        Code* last = dict[-2]->pf[-1];
        Code* temp = dict[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear();
        last->stage = 3),
    IMMD("next", 
        Code* last = dict[-2]->pf[-1];
        Code* temp = dict[-1];
        if (last->stage == 0) last->pf.merge(temp->pf.v);
        else last->pf2.merge(temp->pf.v);
        dict.pop()),
    // compiler examples
    CODE("exit", throw length_error(" ")), 	// exit interpreter
    CODE("exec", int n = top; dict[n]->exec()),
    CODE(":",
        string s; cin >> s;            /// * get next token
        dict.push(new Code(s, true));  /// * create new word
        compile = true),
    IMMD(";",     compile = false),
    CODE("variable",
        string s; cin >> s;            /// * create new variable
        dict.push(new Code(s, true));
        Code * last = dict[-1]->addcode(new Code("dovar", 0));
        last->pf[0]->token = last->token),
    CODE("create",
        string s; cin >> s;            /// * create new variable
        dict.push(new Code(s, true));
        Code* last = dict[-1]->addcode(new Code("dovar", 0));
        last->pf[0]->token = last->token;
        last->pf[0]->qf.pop()),
    CODE("constant",   // n --
        string s; cin >> s;            /// * create new constant
        dict.push(new Code(s, true));
        Code* last = dict[-1]->addcode(new Code("docon", top));
        last->pf[0]->token = last->token; POP()),
    CODE("@",   // w -- n
        Code* last = dict[top]; POP();
        PUSH(last->pf[0]->qf[0])),
    CODE("!",   // n w -- 
        Code* last = dict[top]; POP();
        last->pf[0]->qf.clear(); 
        last->pf[0]->qf.push(top); POP()),
    CODE("+!",   // n w -- 
        Code* last = dict[top]; POP();
        int n = last->pf[0]->qf[0] + top; POP();
        last->pf[0]->qf.clear();
        last->pf[0]->qf.push(n)),
    CODE("?",   // w -- 
        Code* last = dict[top]; POP();
        cout << last->pf[0]->qf[0] << " "),
    CODE("array@",   // w a -- n
        int a = top; POP();
        Code* last = dict[top]; POP();
        PUSH(last->pf[0]->qf[a])),
    CODE("array!",   // n w a -- 
        int a = top; POP();
        Code* last = dict[top]; POP();
        last->pf[0]->qf.v[a] =top; POP()),
    CODE(",",  // n --
        Code* last = dict[-1];
        last->pf[0]->qf.push(top); POP()),
    CODE("allot",   // n --
        int n = top; POP();
        Code* last = dict[-1];
        for (int i = 0; i < n; i++) last->pf[0]->qf.push(0)),
    CODE("does",  // n --
        Code * last = dict[-1];
        Code * source = dict[WP];
        last->pf = source->pf; 
        ),
    CODE("to",    // n -- , compile only 
        Code* last = dict[WP]; IP++;               	// current colon word
        last->pf[IP++]->pf[0]->qf.push(top); POP()),// next constant
    CODE("is",     // w -- , execute only
        Code* source = dict[top]; POP();            	// source word
        string s; cin >> s;
        Code* w = find(s);
        if (w == NULL) throw length_error(" ");
        dict[w->token]->pf = source->pf),
    // tools
    CODE("here", PUSH(dict[-1]->token)),
    CODE("boot", for (int i = dict[-1]->token; i > 104; i--) dict.pop();),
    CODE("forget", 
        string s; cin >> s;
        Code* w = find(s);
        if (w == NULL) throw length_error(" ");
        for (int i = dict[-1]->token; i >= max(w->token, 104); i--) dict.pop()),
        // debugging
    CODE("words", words()),
    CODE(".s",  ss_dump()),
    CODE("see",
        string s; cin >> s;            /// * fetch next token
        Code * w = find(s);
        if (w) see(w, 0);
        cout << endl)
};
///
/// main class
///
void dict_setup() {
    dict.merge(prim);                   /// * populate dictionary
    prim.clear();                       /// * reduce memory footprint
}
void outer() {
    string idiom;
    while (cin >> idiom) {
        Code* w = find(idiom);            /// * search through dictionary
        if (w) {                        /// * word found?
            if (compile && !w->immd) {     /// * in compile mode?
                dict[-1]->addcode(w);   /// * add to colon word
            }
            else {
                try { w->exec(); }      /// * execute forth word
                catch (exception& e) {
                    cout << e.what() << endl;
                }
            }
        }
        else {                          /// * try as numeric
            try {
                int n = stoi(idiom, nullptr, base);   /// * convert to integer
                if (compile) {
                    dict[-1]->addcode(new Code("dolit", n)); /// * add to current word
                }
                else PUSH(n);           /// * add value onto data stack
            }
            catch (...) {                /// * failed to parse number
                cout << idiom << "? " << endl;
                ss.clear(); top = -1;
                compile = false;
            }
        }
        if (!compile) ss_dump();           /// * stack dump and display ok prompt
    }
}
//
// Main Program
//
int main(int ac, char* av[]) {
    dict_setup();
    cout << "ceforth 4.02" << endl;
    outer();
    cout << "done!" << endl;
    return 0;
}
