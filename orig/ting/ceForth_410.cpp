#include <iostream>         // cin, cout
#include <iomanip>          // setbase
#include <vector>           // vector

using namespace std;
template<typename T>
struct ForthList : public vector<T> {     ///< our super-vector class
    ForthList()                        : vector<T>()    {}
    ForthList(initializer_list<T> lst) : vector<T>(lst) {}
    ForthList *merge(ForthList<T> v) {
        this->insert(this->end(), v.begin(), v.end()); return this;
    }
    void push(T n) { this->push_back(n); }
    T    pop()     { T n = this->back(); this->pop_back(); return n; }
};
///
/// Forth VM state variables
///
ForthList<int> rs;                        ///< return stack
ForthList<int> ss;                        ///< parameter stack
int  base    = 10;                        ///< numeric radix
bool compile = false;                     ///< compiling flag
int  top     = -1;                        ///< cached top of stack
int  IP, WP;                              ///< instruction and param ptrs
///
/// data structure for dictionary entry
///
struct Code;                              ///< forward declaration
typedef void (*fop)(Code*);               ///< Forth operator
Code *find(string s);                     ///< forward declaration
void  words(); 

struct Code {
    static int fence;                     ///< token incremental counter
    string name;                          ///< name of word
    int    token = 0;                     ///< dictionary order token
    bool   immd  = false;                 ///< immediate flag
    fop    xt    = NULL;                  ///< primitive function
    string lit;                           ///< string literal
    int    stage = 0;                     ///< branching stage
    ForthList<Code*> pf;                  ///< parameter field
    ForthList<Code*> p1;                  ///< parameter field - if
    ForthList<Code*> p2;                  ///< parameter field - else
    ForthList<int>   q;                   ///< parameter field - literal
    Code(const char *n) {}
    Code(string n, fop fn, bool im=false) /// primitive
        : name(n), xt(fn), immd(im), token(fence++) {}
    Code(string n, bool f=false) {        /// new colon word or temp
        Code* c = find(name=n);
        if (c) xt = c->xt;
        if (f) token = fence++;
    }
    Code(string n, int d) :  name(n), xt(find(n)->xt) { q.push(d); } /// dolit
    Code(string n, string l) : name(n), xt(find(n)->xt), lit(l) {}    /// dostr
    Code*  immediate()  { immd = true;  return this; } ///> set immediate flag
    Code*  add(Code* w) { pf.push(w); return this; }   ///> append colon word
    string to_s() { return name + " " + to_string(token) + (immd ? "*" : ""); }
    void   exec() {                      ///> execute word
        if (xt) { xt(this); return; }    /// * execute primitive word
        rs.push(WP); rs.push(IP);        /// * execute colon word
        WP = token; IP = 0;              /// * setup dolist call frame
        for (Code* c : pf) {             /// * inner interpreter
            try { c->exec(); IP++; }     /// * pass Code object to xt
            catch (...) {}
        }
        IP = rs.pop(); WP = rs.pop();    /// * return to caller
    }
};
int Code::fence = 0;                     ///< initialize static var
///
/// IO functions
///
string next_idiom(char delim=0) {
    string s; delim ? getline(cin, s, delim) : cin >> s; return s;
}
void dot_r(int n, string s) {
	for (int i = 0, m = n-s.size(); i < m; i++) cout << " ";
	cout << s;
}
void ss_dump() {
    cout << "< "; for (int i : ss) { cout << i << " "; }
    cout << top << " >ok" << endl;
}
void see(Code* c, int dp) {
    auto pp = [](int dp, string s, vector<Code*> v) {   // lambda for indentation and recursive dump
        int i = dp; cout << endl; while (i--) cout << "  "; cout << s;
        for (Code* w : v) see(w, dp + 1);
    };
    auto pq = [](vector<int> v) { cout << "="; for (int i : v) cout << i << " "; };
    pp(dp, "[ " + c->name, c->pf);
    if (c->p1.size() > 0) pp(dp, "1--", c->p1);
    if (c->p2.size() > 0) pp(dp, "2--", c->p2);
    if (c->q.size()  > 0) pq(c->q);
    cout << "]";
}
/// macros to reduce verbosity (but harder to single-step debug)
inline  int POP()    { int n=top; top=ss.pop(); return n; }
#define PUSH(v)      (ss.push(top), top=(v))
#define CODE(s, g)   new Code(s, [](Code *c){ g; })
#define IMMD(s, g)   new Code(s, [](Code *c){ g; }, true)
///
/// Forth dictionary assembler
///
ForthList<Code*> dict = {
    // stack op
    CODE("dup",  PUSH(top)),
    CODE("over", PUSH(ss[-2])),
    CODE("2dup", PUSH(ss[-2]); PUSH(ss[-2])),
    CODE("2over",PUSH(ss[-4]); PUSH(ss[-4])),
    CODE("4dup", PUSH(ss[-4]); PUSH(ss[-4]); PUSH(ss[-4]); PUSH(ss[-4])),
    CODE("swap", int n = ss.pop(); PUSH(n)),
    CODE("rot",  int n = ss.pop(); int m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("-rot", int n = ss.pop(); int m = ss.pop(); PUSH(n);  PUSH(m)),
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
    CODE("+",    top += ss.pop()),       // note: ss.pop() is different from POP()
    CODE("-",    top =  ss.pop() - top),
    CODE("*",    top *= ss.pop()),
    CODE("/",    top =  ss.pop() / top),
    CODE("mod",  top =  ss.pop() % top),
    CODE("*/",   top =  ss.pop() * ss.pop() / top),
    CODE("*/mod",
         int n = ss.pop() * ss.pop();
         ss.push(n % top); top = (n / top)),
    CODE("and",  top &= ss.pop()),
    CODE("or",   top |= ss.pop()),
    CODE("xor",  top ^= ss.pop()),
    CODE("negate", top = -top),
    CODE("abs",  top = abs(top)),
    // logic ops
    CODE("0=",   top = (top == 0) ? -1 : 0),
    CODE("0<",   top = (top <  0) ? -1 : 0),
    CODE("0>",   top = (top >  0) ? -1 : 0),
    CODE("=",    top = (ss.pop() == top) ? -1 : 0),
    CODE(">",    top = (ss.pop() >  top) ? -1 : 0),
    CODE("<",    top = (ss.pop() <  top) ? -1 : 0),
    CODE("<>",   top = (ss.pop() != top) ? -1 : 0),
    CODE(">=",   top = (ss.pop() >= top) ? -1 : 0),
    CODE("<=",   top = (ss.pop() <= top) ? -1 : 0),
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
    CODE("dotstr", cout << c->lit),
    CODE("dolit",  PUSH(c->q[0])),
    CODE("dovar",  PUSH(c->token)),
    CODE("[", compile = false),
    CODE("]", compile = true),
    CODE("$\"",
        string s = next_idiom('"').substr(1);
        dict[-1]->add(new Code("dovar", s))),
    IMMD(".\"",
        string s = next_idiom('"').substr(1);
        dict[-1]->add(new Code("dotstr", s))),
    IMMD("(", next_idiom(')')),
    IMMD(".(", cout << next_idiom(')')),
    CODE("\\", cout << next_idiom('\n')),
    // branching - if...then, if...else...then
    IMMD("branch",
        bool f = POP() != 0;                        // check flag
        for (Code* w : (f ? c->pf : c->p1)) w->exec()),
    IMMD("if",
        dict[-1]->add(new Code("branch"));
        dict.push(new Code("temp"))),               // use last cell of dictionay as scratch pad
    IMMD("else",
        Code *tmp = dict[-1]; Code *last = dict[-2]->pf[-1];
        last->pf.merge(tmp->pf);
        tmp->pf.clear();
        last->stage = 1),
    IMMD("then",
        Code *tmp = dict[-1]; Code *last = dict[-2]->pf[-1];
        if (last->stage == 0) {                     // if...then
            last->pf.merge(tmp->pf);
            dict.pop_back();
        }
        else {                                      // if...else...then, or
            last->p1.merge(tmp->pf);           // for...aft...then...next
             if (last->stage == 1) dict.pop_back();
             else tmp->pf.clear();
        }),
    // loops - begin...again, begin...f until, begin...f while...repeat
    CODE("loops",
        while (true) {
            for (Code* w : c->pf) w->exec();                   // begin...
            int f = top;
            if (c->stage == 0 && (top=ss.pop(), f != 0)) break;  // ...until
            if (c->stage == 1) continue;                         // ...again
            if (c->stage == 2 && (top=ss.pop(), f == 0)) break;  // while...repeat
            for (Code* w : c->p1) w->exec();
        }),
    IMMD("begin",
        dict[-1]->add(new Code("loops"));
        dict.push(new Code("tmp"))),
    IMMD("while",
        Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
        tmp->pf.clear(); last->stage = 2),
    IMMD("repeat",
        Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->p1.merge(tmp->pf); dict.pop_back()),
    IMMD("again",
        Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
        last->stage = 1; dict.pop_back()),
    IMMD("until",
        Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf); dict.pop_back()),
    // loops - for...next, for...aft...then...next
    CODE("cycles",
        do { for (Code* w : c->pf) w->exec(); }
        while (c->stage==0 && (rs[-1]-=1) >=0);  // for...next only
        while (c->stage > 0) {                      // aft
            for (Code* w : c->p2) w->exec();     // then...next
            if ((rs[-1]-=1) < 0) break;
            for (Code* w : c->p1) w->exec();     // aft...then
        }
        rs.pop_back()),
    IMMD("for",
        dict[-1]->add(new Code(">r"));
        dict[-1]->add(new Code("cycles"));
        dict.push(new Code("tmp"))),
    IMMD("aft",
        Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
        tmp->pf.clear(); last->stage = 3),
    IMMD("next",
        Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         if (last->stage == 0) last->pf.merge(tmp->pf);
         else last->p2.merge(tmp->pf); dict.pop_back()),
    // compiler
    CODE("exit", exit(0)),                          // exit interpreter
    CODE("exec", int n = top; dict[n]->exec()),
    CODE(":",
        dict.push(new Code(next_idiom(), true));    // create new word
        compile = true),
    IMMD(";", compile = false),
    CODE("variable",
        dict.push(new Code(next_idiom(), true));
        Code *last = dict[-1]->add(new Code("dovar", 0));
        last->pf[0]->token = last->token),
    CODE("create",
        dict.push(new Code(next_idiom(), true));
        Code *last = dict[-1]->add(new Code("dovar", 0));
        last->pf[0]->token = last->token;
        last->pf[0]->q.pop_back()),
    CODE("constant",
        dict.push(new Code(next_idiom(), true));
        Code *last = dict[-1]->add(new Code("dolit", POP()));
        last->pf[0]->token = last->token),
    CODE("@", int n=POP(); PUSH(dict[n]->pf[0]->q[0])),              // w -- n
    CODE("!", int n=POP(); dict[n]->pf[0]->q[0] = POP()),            // n w --
    CODE("+!",int n=POP(); dict[n]->pf[0]->q[0] += POP()),           // n w --
    CODE("?", int n=POP(); cout << dict[n]->pf[0]->q[0] << " "),     // w --
    CODE("array@", int a=POP(); PUSH(dict[POP()]->pf[0]->q[a])),     // w a -- n
    CODE("array!", int a=POP(); dict[POP()]->pf[0]->q[a] = POP()),   // n w a --
    CODE(",", dict[-1]->pf[0]->q.push(POP())),
    CODE("allot",                                     // n --
        int n = POP();
        for (int i = 0; i < n; i++) dict[-1]->pf[0]->q.push(0)),
    CODE("does", dict[-1]->pf.merge(dict[WP]->pf)),
    CODE("to",                                        // n -- , compile only
        IP++;                                         // current colon word
        dict[WP]->pf[IP++]->pf[0]->q.push(POP())),   // next constant
    CODE("is",                                        // w -- , execute only
         Code *src = dict[POP()];                     // source word
         c = find(next_idiom());
         if (c == NULL) throw length_error(" ");
         dict[c->token]->pf = src->pf),
    // tools
    CODE("here",  PUSH(dict[-1]->token)),
    CODE("words", words()),
    CODE(".s",    ss_dump()),
    CODE("'",     c = find(next_idiom()); PUSH(c->token)),
    CODE("see",
         c = find(next_idiom());
         if (c) see(c, 0); cout << endl),
    CODE("forget",
         c = find(next_idiom());
         if (c == NULL) return;
         int t = max(c->token, find("boot")->token);
         for (int i=dict.size(); i>t; i--) dict.pop_back()),
    CODE("boot",
         int t = find("boot")->token;
         for (int i=dict.size(); i>t; i--) dict.pop_back())
};
Code *find(string s) {                      /// search dictionary reversely
    for (int i = dict.size() - 1; i >= 0; --i) {
        if (s == dict[i]->name) return dict[i];
    }
    return NULL;
}
void words() {
    const int WIDTH = 60;
    int i = 0, w = 0;
    for (Code* c : dict) {
        cout << c->name << "  ";
        w += (c->name.size() + 2);
        if (w > WIDTH) { cout << endl; w = 0; }
    }
}
/// 
/// main - outer interpreter
///
int main(int ac, char* av[]) {
    cout << "ceForth 4.1" << endl;
    words();
    return 0;
    string idiom;
    while (cin >> idiom) {               /// outer interpreter
        Code *w = find(idiom);           /// * search through dictionary
        if (w) {                         /// * word found?
            if (compile && !w->immd) dict[-1]->add(w);  /// * add to colon word
            else {
                try { w->exec(); }       /// * execute forth word
                catch (exception& e) { cout << e.what() << endl; }
            }
            break;
        }
        try {                                   /// * try as numeric
            int n = stoi(idiom, nullptr, base); /// * convert to integer
            if (compile) dict[-1]->add(new Code("dolit", n)); /// * add to current word
            else PUSH(n);                 /// * add value to data stack
        }                         
        catch (...) {                     /// * failed to parse number
            cout << idiom << "? " << endl;
            ss.clear(); top = -1; compile = false;
            getline(cin, idiom, '\n');
        }
        if (cin.peek()=='\n' && !compile) ss_dump(); /// * dump stack and display ok prompt
    }           /// * skip the entire line
    cout << "Done!" << endl;
    return 0;
}
