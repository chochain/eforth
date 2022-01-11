#include <iostream>         // cin, cout
#include <iomanip>          // setbase
#include <vector>           // vector
using namespace std;
/// .hpp - macros and class prototypes
template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern
    T& operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    T operator<<(T t) { v.push_back(t); }
    T pop() {
        if (v.empty()) throw length_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t;
    }
    T dec_i() { return v.back() -= 1; }     /// decrement stack top
    void push(T t) { v.push_back(t); }
    void clear() { v.clear(); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
    void erase(int i) { v.erase(v.begin() + i, v.end()); }
};
class Code;                                 /// forward declaration
typedef void (*fop)(Code*);                 /// Forth operator
/// .cpp - Code class implementation
class Code {
public:
    static int fence;                       /// token incremental counter
    string name;                            /// name of word
    int    token = 0;                       /// dictionary order token
    bool   immd = false;                    /// immediate flag
    fop    xt = NULL;                       /// primitive function
    string literal;                         /// string literal
    int    stage = 0;                       /// branching stage
    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<int>   qf;
    Code(string n, fop fn, bool im = false);/// primitive
    Code(string n, bool f = false);         /// new colon word or temp
    Code(string n, int d);                  /// dolit
    Code(string n, string l);               /// dostr
    Code* immediate();                      /// set immediate flag
    Code* addcode(Code* w);                 /// append colon word
    void   exec();                          /// execute word
    string to_s();                          /// debugging
};
/// Forth virtual machine variables
ForthList<int>   rs;                        /// return stack
ForthList<int>   ss;                        /// parameter stack
ForthList<Code*> dict;                      /// dictionary
bool compile = false;                       /// compiling flag
int  base = 10;                             /// numeric radix
int  top = -1;                              /// cached top of stack
int  IP, WP;                                /// instruction and parameter pointers
/// dictionary and input stream search functions
Code *find(string s) {                      /// search dictionary reversely
    for (int i = dict.v.size() - 1; i >= 0; --i) {
        if (s == dict.v[i]->name) return dict.v[i];
    }
    return NULL;
}
string next_idiom(char delim=0) { string s; delim ? getline(cin, s, delim) : cin >> s; return s; }
/// Code class constructors
int Code::fence = 0;
Code::Code(string n, fop fn, bool im) { name = n; token = fence++; xt = fn; immd = im; }
Code::Code(string n, bool f) { Code* c = find(name = n); if (c) xt = c->xt; if (f) token = fence++; }
Code::Code(string n, int d) { xt = find(name = n)->xt; qf.push(d); }
Code::Code(string n, string l) { xt = find(name = n)->xt; literal = l; }
/// Code class public methods
Code* Code::immediate() { immd = true;  return this; }
Code* Code::addcode(Code* w) { pf.push(w); return this; }
void  Code::exec() {
    if (xt) { xt(this); return; }           /// * execute primitive word and return
    rs.push(WP); rs.push(IP);               /// * execute colon word
    WP = token; IP = 0;                     /// * setup dolist call frame
    for (Code* w : pf.v) {                  /// * inner interpreter
        try { w->exec(); IP++; }            /// * pass Code object to xt
        catch (...) {}
    }
    IP = rs.pop(); WP = rs.pop();           /// * return to caller
}
string Code::to_s() { return name + " " + to_string(token) + (immd ? "*" : ""); }
// external function (instead of inline)
void dot_r(int n, string s) {
	for (int i = 0, m = n-s.size(); i < m; i++) cout << " ";
	cout << s;
}
void ss_dump() {
    cout << "< "; for (int i : ss.v) { cout << i << " "; }
    cout << top << " >ok" << endl;
}
void see(Code* c, int dp) {
    auto pf = [](int dp, string s, vector<Code*> v) {   // lambda for indentation and recursive dump
        int i = dp; cout << endl; while (i--) cout << "  "; cout << s;
        for (Code* w : v) see(w, dp + 1);
    };
    auto qf = [](vector<int> v) { cout << "="; for (int i : v) cout << i << " "; };
    pf(dp, "[ " + c->to_s(), c->pf.v);
    if (c->pf1.v.size() > 0) pf(dp, "1--", c->pf1.v);
    if (c->pf2.v.size() > 0) pf(dp, "2--", c->pf2.v);
    if (c->qf.v.size() > 0)  qf(c->qf.v);
    cout << "]";
}
void words() {
    int i = 0;
    for (Code* w : dict.v) {
        cout << w->to_s() << " ";
        if ((++i % 10) == 0) cout << endl;
    }
}
/// macros to reduce verbosity (but harder to single-step debug)
inline int POP() { int n=top; top=ss.pop(); return n; }
#define PUSH(v)  (ss.push(top),top=(v))
#define CODE(s, g) new Code(s, [](Code *c){ g; })
#define IMMD(s, g) new Code(s, [](Code *c){ g; }, true)
/// primitives (mostly use lambda but can use external as well)
vector<Code*> prim = {
    // stack op
    CODE("dup",  PUSH(top)),
    CODE("over", PUSH(ss[-2])),
    CODE("2dup", PUSH(ss[-2]); PUSH(ss[-2])),
    CODE("2over",PUSH(ss[-4]); PUSH(ss[-4])),
    CODE("4dup", PUSH(ss[-4]); PUSH(ss[-4]); PUSH(ss[-4]); PUSH(ss[-4])),
    CODE("swap", int n = ss.pop(); PUSH(n)),
    CODE("rot",  int n = ss.pop(); int m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("-rot", int n = ss.pop(); int m = ss.pop(); PUSH(n); PUSH(m)),
    CODE("2swap",int n = ss.pop(); int m = ss.pop(); int l = ss.pop(); ss.push(n); PUSH(l); PUSH(m)),
    CODE("pick", int i = top; top = ss[-i]),
    //    CODE("roll", int i=top; top=ss[-i]),
    CODE("drop", top=ss.pop()),
    CODE("nip",  ss.pop()),
    CODE("2drop",ss.pop(); top=ss.pop()),
    CODE(">r",   rs.push(POP())),
    CODE("r>",   PUSH(rs.pop())),
    CODE("r@",   PUSH(rs[-1])),
    CODE("push", rs.push(POP())),
    CODE("pop",  PUSH(rs.pop())),
    // ALU ops
    CODE("+",  top += ss.pop()),       // note: ss.pop() is different from POP()
    CODE("-",  top = ss.pop() - top),
    CODE("*",  top *= ss.pop()),
    CODE("/",  top = ss.pop() / top),
    CODE("mod",top = ss.pop() % top),
    CODE("*/", top = ss.pop() * ss.pop() / top),
    CODE("*/mod", int n = ss.pop() * ss.pop();
        ss.push(n% top); top = (n / top)),
    CODE("and",top &= ss.pop()),
    CODE("or", top |= ss.pop()),
    CODE("xor",top ^= ss.pop()),
    CODE("negate", top = -top),
    CODE("abs",top = abs(top)),
    // logic ops
    CODE("0=", top = (top == 0) ? -1 : 0),
    CODE("0<", top = (top <  0) ? -1 : 0),
    CODE("0>", top = (top >  0) ? -1 : 0),
    CODE("=",  top = (ss.pop() == top) ? -1 : 0),
    CODE(">",  top = (ss.pop() >  top) ? -1 : 0),
    CODE("<",  top = (ss.pop() <  top) ? -1 : 0),
    CODE("<>", top = (ss.pop() != top) ? -1 : 0),
    CODE(">=", top = (ss.pop() >= top) ? -1 : 0),
    CODE("<=", top = (ss.pop() <= top) ? -1 : 0),
    // output
    CODE("base@",  PUSH(base)),
    CODE("base!",  cout << setbase(base = POP())),
    CODE("hex",    cout << setbase(base = 16)),
    CODE("decimal",cout << setbase(base = 10)),
    CODE("cr",     cout << endl),
    CODE(".",      cout << POP() << " "),
    CODE(".r",     int n = POP(); string s = to_string(POP()); dot_r(n, s)),
    CODE("u.r",    int n = POP(); string s = to_string(abs(POP())); dot_r(n, s)),
    CODE("key",    PUSH(next_idiom()[0])),
    CODE("emit",   char b = (char)POP(); cout << b),
    CODE("space",  cout << (" ")),
    CODE("spaces", for (int n = POP(), i = 0; i < n; i++) cout << " "),
    // literals
    CODE("dotstr", cout << c->literal),
    CODE("dolit",  PUSH(c->qf[0])),
    CODE("dovar",  PUSH(c->token)),
    CODE("[", compile = false),
    CODE("]", compile = true),
    CODE("$\"",
        string s = next_idiom('"').substr(1);
        dict[-1]->addcode(new Code("dovar", s))),
    IMMD(".\"",
        string s = next_idiom('"').substr(1);
        dict[-1]->addcode(new Code("dotstr", s))),
    IMMD("(", next_idiom(')')),
    IMMD(".(", cout << next_idiom(')')),
    CODE("\\", cout << next_idiom('\n')),
    // branching - if...then, if...else...then
    IMMD("branch",
        bool f = POP() != 0;                        // check flag
        for (Code* w : (f ? c->pf.v : c->pf1.v)) w->exec()),
    IMMD("if",
        dict[-1]->addcode(new Code("branch"));
        dict.push(new Code("temp"))),               // use last cell of dictionay as scratch pad
    IMMD("else",
        Code *temp = dict[-1]; Code *last = dict[-2]->pf[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear();
        last->stage = 1),
    IMMD("then",
        Code *temp = dict[-1]; Code *last = dict[-2]->pf[-1];
        if (last->stage == 0) {                     // if...then
            last->pf.merge(temp->pf.v);
            dict.pop();
        }
        else {                                      // if...else...then, or
             last->pf1.merge(temp->pf.v);           // for...aft...then...next
             if (last->stage == 1) dict.pop();
             else temp->pf.clear();
        }),
    // loops - begin...again, begin...f until, begin...f while...repeat
    CODE("loops",
        while (true) {
            for (Code* w : c->pf.v) w->exec();                   // begin...
            int f = top;
            if (c->stage == 0 && (top=ss.pop(), f != 0)) break;  // ...until
            if (c->stage == 1) continue;                         // ...again
            if (c->stage == 2 && (top=ss.pop(), f == 0)) break;  // while...repeat
            for (Code* w : c->pf1.v) w->exec();
        }),
    IMMD("begin",
        dict[-1]->addcode(new Code("loops"));
        dict.push(new Code("temp"))),
    IMMD("while",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear(); last->stage = 2),
    IMMD("repeat",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf1.merge(temp->pf.v); dict.pop()),
    IMMD("again",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf.merge(temp->pf.v);
        last->stage = 1; dict.pop()),
    IMMD("until",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf.merge(temp->pf.v); dict.pop()),
    // loops - for...next, for...aft...then...next
    CODE("cycles",
        do { for (Code* w : c->pf.v) w->exec(); }
        while (c->stage == 0 && rs.dec_i() >= 0);   // for...next only
        while (c->stage > 0) {                      // aft
            for (Code* w : c->pf2.v) w->exec();     // then...next
            if (rs.dec_i() < 0) break;
            for (Code* w : c->pf1.v) w->exec();     // aft...then
        }
        rs.pop()),
    IMMD("for",
        dict[-1]->addcode(new Code(">r"));
        dict[-1]->addcode(new Code("cycles"));
        dict.push(new Code("temp"))),
    IMMD("aft",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        last->pf.merge(temp->pf.v);
        temp->pf.clear(); last->stage = 3),
    IMMD("next",
        Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
        if (last->stage == 0) last->pf.merge(temp->pf.v);
        else last->pf2.merge(temp->pf.v); dict.pop()),
    // compiler
    CODE("exit", exit(0)),                          // exit interpreter
    CODE("exec", int n = top; dict[n]->exec()),
    CODE(":",
        dict.push(new Code(next_idiom(), true));    // create new word
        compile = true),
    IMMD(";", compile = false),
    CODE("variable",
        dict.push(new Code(next_idiom(), true));
        Code *last = dict[-1]->addcode(new Code("dovar", 0));
        last->pf[0]->token = last->token),
    CODE("create",
        dict.push(new Code(next_idiom(), true));
        Code *last = dict[-1]->addcode(new Code("dovar", 0));
        last->pf[0]->token = last->token;
        last->pf[0]->qf.pop()),
    CODE("constant",
        dict.push(new Code(next_idiom(), true));
        Code *last = dict[-1]->addcode(new Code("dolit", POP()));
        last->pf[0]->token = last->token),
    CODE("@", int n=POP(); PUSH(dict[n]->pf[0]->qf[0])),              // w -- n
    CODE("!", int n=POP(); dict[n]->pf[0]->qf[0] = POP()),            // n w --
    CODE("+!",int n=POP(); dict[n]->pf[0]->qf[0] += POP()),           // n w --
    CODE("?", int n=POP(); cout << dict[n]->pf[0]->qf[0] << " "),     // w --
    CODE("array@", int a=POP(); PUSH(dict[POP()]->pf[0]->qf[a])),     // w a -- n
    CODE("array!", int a=POP(); dict[POP()]->pf[0]->qf[a] = POP()),   // n w a --
    CODE(",", dict[-1]->pf[0]->qf.push(POP())),
    CODE("allot",                                     // n --
        int n = POP();
        for (int i = 0; i < n; i++) dict[-1]->pf[0]->qf.push(0)),
    CODE("does", dict[-1]->pf.merge(dict[WP]->pf.v)),
    CODE("to",                                        // n -- , compile only
        IP++;                                         // current colon word
        dict[WP]->pf[IP++]->pf[0]->qf.push(POP())),   // next constant
    CODE("is",                                        // w -- , execute only
        Code *source = dict[POP()];                   // source word
        Code *w = find(next_idiom());
        if (w == NULL) throw length_error(" ");
        dict[w->token]->pf = source->pf),
    // tools
    CODE("here",  PUSH(dict[-1]->token)),
    CODE("words", words()),
    CODE(".s",    ss_dump()),
    CODE("'", Code *w = find(next_idiom()); PUSH(w->token)),
    CODE("see",
        Code *w = find(next_idiom());
        if (w) see(w, 0); cout << endl),
    CODE("forget",
        Code *w = find(next_idiom());
         if (w == NULL) return;
         dict.erase(max(w->token, find("boot")->token))),
    CODE("boot", dict.erase(find("boot")->token + 1))
};
/// core functions (use Python indentation to save a few lines)
void outer() {
    string idiom;
    while (cin >> idiom) {
        Code *w = find(idiom);                          /// * search through dictionary
        if (w) {                                        /// * word found?
            if (compile && !w->immd)                    /// * in compile mode?
                dict[-1]->addcode(w);                   /// * add to colon word
            else {
                try { w->exec(); }                      /// * execute forth word
                catch (exception& e) {
                    cout << e.what() << endl; }}}
        else {
        	try {                                       /// * try as numeric
        		int n = stoi(idiom, nullptr, base);     /// * convert to integer
        		if (compile) 
        			dict[-1]->addcode(new Code("dolit", n)); /// * add to current word
                else PUSH(n); }                         /// * add value onto data stack
            catch (...) {                               /// * failed to parse number
                cout << idiom << "? " << endl;
                ss.clear(); top = -1; compile = false;
                getline(cin, idiom, '\n'); }}           /// * skip the entire line
    	if (cin.peek()=='\n' && !compile) ss_dump(); }} /// * dump stack and display ok prompt
void dict_setup() {
    dict.merge(prim);                                   /// * populate dictionary
    prim.clear(); }                                     /// * reduce memory footprint
/// main program
int main(int ac, char* av[]) {
    dict_setup();
    cout << "ceforth 4.03" << endl;
    outer();
    cout << "done!" << endl;
    return 0; }
