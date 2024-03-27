#include <iostream>         // cin, cout
#include <iomanip>          // setbase
#include <vector>           // vector
#include <chrono>

using namespace std;
template<typename T>
struct ForthList : public vector<T> {     ///< our super-vector class
    ForthList()                        : vector<T>()    {}
    ForthList(initializer_list<T> lst) : vector<T>(lst) {}
    ForthList *merge(ForthList<T> v) {
        this->insert(this->end(), v.begin(), v.end()); return this;
    }
    T    dec_i()   { return (this->back() -= 1); }
    void push(T n) { this->push_back(n); }
    T    pop()     { T n = this->back(); this->pop_back(); return n; }
    T    operator[](int i) { return this->at(i < 0 ? (this->size() + i) : i); }
};
///
/// Forth VM state variables
///
ForthList<int> rs;           ///< return stack
ForthList<int> ss;           ///< parameter stack
bool compile = false;        ///< compiling flag
int  top     = -1;           ///< cached top of stack
///
/// data structure for dictionary entry
///
struct Code;                 ///< forward declaration
typedef void (*XT)(Code*);   ///< function pointer
Code *find(string s);        ///< fwd declared for Code
void  words();               ///< fwd declared for dict

struct Code {
    static int idx;          ///< token incremental counter
    string name;             ///< name of word
    int    token = 0;        ///< dict index, 0=param word
    bool   immd  = false;    ///< immediate flag
    XT     xt    = NULL;     ///< execution token
    string str;              ///< string literal
    int    stage = 0;        ///< branching stage (looping condition)
    ForthList<Code*> pf;     ///< parameter field
    ForthList<Code*> p1;     ///< parameter field - if..else, aft..then
    ForthList<Code*> p2;     ///< parameter field - then..next
    ForthList<int>   q;      ///< parameter field - literal
    Code(string n, XT fp, bool im=false)  ///> primitive
        : name(n), xt(fp), immd(im), token(idx++) {}
    Code(string n, bool f=false)          ///> colon word
        : name(n), token(f ? idx++ : 0) {
        Code *w = find(n); xt = w ? w->xt : NULL;
    }
    Code(string n, int d) : name(""), xt(find(n)->xt) { q.push(d); }  ///> dolit, dovar
    Code(string n, string s) : name("_$"), xt(find(n)->xt), str(s) {} ///> dostr, dotstr
    Code *immediate()  { immd = true; return this; }  ///> set immediate flag
    Code *add(Code *w) { pf.push(w);  return this; }  ///> append colon word
    void exec() {                         ///> inner interpreter
        if (xt) { xt(this); return; }     /// * run primitive word
        for (Code *w : pf) {              /// * run colon word
            try { w->exec(); }            /// * execute recursively
            catch (...) { break; }        /// * also handle exit
        }
    }
};
int Code::idx = 0;                        ///< initialize static var
///
///> IO functions
///
string next_idiom(char delim=0) {         ///> read next idiom form input stream
    string s; delim ? getline(cin, s, delim) : cin >> s; return s;
}
void ss_dump() {                          ///> display data stack and ok promt
    cout << "< "; for (int i : ss) cout << i << " ";
    cout << top << " > ok" << endl;
}
void see(Code *c, int dp) {               ///> disassemble a colon word
    auto pp = [](int dp, string s, ForthList<Code*> v) { ///> recursive dump with indent
        int i = dp; cout << endl; while (i--) cout << "  "; cout << s;
        for (Code *w : v) if (dp < 2) see(w, dp + 1);    /// * depth controlled
    };
    pp(dp, (c->name=="_$" ? ".\" "+c->str+"\"" : c->name), c->pf);
    if (c->p1.size() > 0) pp(dp, "( 1-- )", c->p1);
    if (c->p2.size() > 0) pp(dp, "( 2-- )", c->p2);
    if (c->q.size()  > 0) for (int i : c->q) cout << i << " ";
}
///> macros to reduce verbosity (but harder to single-step debug)
inline  int POP()    { int n=top; top=ss.pop(); return n; }
#define PUSH(v)      (ss.push(top), top=(v))
#define BOOL(f)      ((f) ? -1 : 0)
#define VAR(i)       (*dict[i]->pf[0]->q.data())
#define BASE         (VAR(0))   /* borrow dict[0] to store base (numeric radix) */
#define millis()     chrono::duration_cast<chrono::milliseconds>( \
                     chrono::steady_clock::now().time_since_epoch()).count()
#define CODE(s, g)   new Code(s, [](Code *c){ g; })
#define IMMD(s, g)   new Code(s, [](Code *c){ g; }, true)
///
///> Forth dictionary assembler
///
ForthList<Code*> dict = {
    CODE("bye",    exit(0)),                           // exit to OS
    // stack ops
    CODE("dup",    PUSH(top)),
    CODE("drop",   top=ss.pop()),
    CODE("swap",   int n = ss.pop(); PUSH(n)),
    CODE("over",   PUSH(ss[-2])),
    CODE("rot",    int n = ss.pop(); int m = ss.pop(); ss.push(n); PUSH(m)),
    CODE("-rot",   int n = ss.pop(); int m = ss.pop(); PUSH(m);  PUSH(n)),
    CODE("pick",   top = ss[-top]),
    CODE("nip",    ss.pop()),
    CODE("2dup",   PUSH(ss[-2]); PUSH(ss[-2])),
    CODE("2drop",  ss.pop(); top=ss.pop()),
    CODE("2swap",  int n = ss.pop(); int m = ss.pop(); int l = ss.pop();
                   ss.push(n); PUSH(l); PUSH(m)),
    CODE("2over",  PUSH(ss[-4]); PUSH(ss[-4])),
    CODE(">r",     rs.push(POP())),
    CODE("r>",     PUSH(rs.pop())),
    CODE("r@",     PUSH(rs[-1])),
    // ALU ops
    CODE("+",      top += ss.pop()),       // note: ss.pop() is different from POP()
    CODE("-",      top =  ss.pop() - top),
    CODE("*",      top *= ss.pop()),
    CODE("/",      top =  ss.pop() / top),
    CODE("mod",    top =  ss.pop() % top),
    CODE("*/",     top =  ss.pop() * ss.pop() / top),
    CODE("*/mod",  int n = ss.pop() * ss.pop();
                   ss.push(n % top); top = (n / top)),
    CODE("and",    top &= ss.pop()),
    CODE("or",     top |= ss.pop()),
    CODE("xor",    top ^= ss.pop()),
    CODE("negate", top = -top),
    CODE("abs",    top = abs(top)),
    // logic ops
    CODE("0=",     top = BOOL(top == 0)),
    CODE("0<",     top = BOOL(top <  0)),
    CODE("0>",     top = BOOL(top >  0)),
    CODE("=",      top = BOOL(ss.pop() == top)),
    CODE(">",      top = BOOL(ss.pop() >  top)),
    CODE("<",      top = BOOL(ss.pop() <  top)),
    CODE("<>",     top = BOOL(ss.pop() != top)),
    CODE(">=",     top = BOOL(ss.pop() >= top)),
    CODE("<=",     top = BOOL(ss.pop() <= top)),
    // IO ops
    CODE("base",   PUSH(0)),   // dict[0]->pf[0]->q[0] used for base
    CODE("hex",    cout << setbase(BASE = 16)),
    CODE("decimal",cout << setbase(BASE = 10)),
    CODE("cr",     cout << endl),
    CODE(".",      cout << setbase(BASE) << POP() << " "),
    CODE(".r",     cout << setbase(BASE) << setw(POP()) << POP()),
    CODE("u.r",    cout << setbase(BASE) << setw(POP()) << abs(POP())),
    CODE("key",    PUSH(next_idiom()[0])),
    CODE("emit",   cout << (char)POP()),
    CODE("space",  cout << " "),
    CODE("spaces", cout << setw(POP()) << ""),
    // literals
    CODE("_str",   cout << c->str),
    CODE("_lit",   PUSH(c->q[0])),
    CODE("_var",   PUSH(c->token)),
    IMMD(".\"",
         string s = next_idiom('"');
         dict[-1]->add(new Code("_str", s.substr(1)))),
    IMMD("(",      next_idiom(')')),
    IMMD(".(",     cout << next_idiom(')')),
    IMMD("\\",     string s; getline(cin, s, '\n')), // flush input
    // branching ops - if...then, if...else...then
    CODE("_bran",
         for (Code *w : (POP() ? c->pf : c->p1)) w->exec()),
    IMMD("if",
         dict[-1]->add(new Code("_bran"));
         dict.push(new Code("tmp"))),                // scratch pad
    IMMD("else",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         tmp->pf.clear();
         last->stage = 1),
    IMMD("then",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         if (last->stage == 0) {               // if...then
             last->pf.merge(tmp->pf);
             dict.pop();                       // CC: memory leak?
         }
         else {                                // if..else..then, or
             last->p1.merge(tmp->pf);          // for..aft..then..next
             if (last->stage == 1) dict.pop(); // CC: memory leak?
             else tmp->pf.clear();
         }),
    // loop ops - begin..again, begin..f until, begin..f while..repeat
    CODE("_loop", int b = c->stage;            ///< stage=looping type
         while (true) {
             for (Code *w : c->pf) w->exec();  // begin..
             if (b==0 && POP()!=0) break;      // ..until
             if (b==1)             continue;   // ..again
             if (b==2 && POP()==0) break;      // ..while..repeat
             for (Code *w : c->p1) w->exec();
         }),
    IMMD("begin",
         dict[-1]->add(new Code("_loop"));
         dict.push(new Code("tmp"))),
    IMMD("while",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         tmp->pf.clear(); last->stage = 2),
    IMMD("repeat",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->p1.merge(tmp->pf); dict.pop()),
    IMMD("again",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         last->stage = 1; dict.pop()),
    IMMD("until",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf); dict.pop()),
    // loops ops - for...next, for...aft...then...next
    CODE("_for",
         do { for (Code *w : c->pf) w->exec(); }
         while (c->stage==0 && rs.dec_i() >=0);   // for...next only
         while (c->stage > 0) {                   // aft
             for (Code *w : c->p2) w->exec();     // then...next
             if (rs.dec_i() < 0) break;
             for (Code *w : c->p1) w->exec();     // aft...then
         }
         rs.pop()),
    IMMD("for",
         dict[-1]->add(new Code(">r"));
         dict[-1]->add(new Code("_for"));
         dict.push(new Code("tmp"))),
    IMMD("aft",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         last->pf.merge(tmp->pf);
         tmp->pf.clear(); last->stage = 3),
    IMMD("next",
         Code *last = dict[-2]->pf[-1]; Code *tmp = dict[-1];
         if (last->stage == 0) last->pf.merge(tmp->pf);
         else last->p2.merge(tmp->pf);
         dict.pop()),                             // CC: memory leak?
    // compiler ops
    CODE("[",      compile = false),
    CODE("]",      compile = true),
    CODE("exec",   dict[top]->exec()),            // xt --
    CODE("exit",   throw length_error("")),       // --
    CODE(":",
         dict.push(new Code(next_idiom(), true)); // create new word
         compile = true),
    IMMD(";", compile = false),
    CODE("immediate", dict[-1]->immediate()),
    CODE("variable",
         dict.push(new Code(next_idiom(), true));
         Code *last = dict[-1]->add(new Code("_var", 0));
         last->pf[0]->token = last->token),
    CODE("create",
         dict.push(new Code(next_idiom(), true));
         Code *last = dict[-1]->add(new Code("_var", 0));
         last->pf[0]->token = last->token;
         last->pf[0]->q.pop()),
    CODE("constant",
         dict.push(new Code(next_idiom(), true));
         Code *last = dict[-1]->add(new Code("_lit", POP()));
         last->pf[0]->token = last->token),
    CODE("@",      int w=POP(); PUSH(VAR(w))),                     // w -- n
    CODE("!",      int w=POP(); VAR(w) = POP()),                   // n w --
    CODE("+!",     int w=POP(); VAR(w) += POP()),                  // n w --
    CODE("?",      int w=POP(); cout << VAR(w) << " "),            // w --
    CODE("array@", int i=POP(); int w=POP(); PUSH(*(&VAR(w)+i))),  // w i -- n
    CODE("array!", int i=POP(); int w=POP(); *(&VAR(w)+i)=POP()),  // n w i --
    CODE(",",      dict[-1]->pf[0]->q.push(POP())),
    CODE("allot",                                     // n --
         int n = POP();
         for (int i=0; i<n; i++) dict[-1]->pf[0]->q.push(0)),
    CODE("_does",
         bool hit = false;
         for (Code *w : dict[c->token]->pf) {
             if (hit) dict[-1]->add(w);               // copy rest of pf
             if (w->name=="_does") hit = true;
         }
         throw length_error("")),                     // exit caller
    IMMD("does>",
         dict[-1]->add(new Code("_does"));
         dict[-1]->pf[-1]->token = dict[-1]->token),  // keep WP
    CODE("to",                                        // n --
         Code *w=find(next_idiom()); if (!w) return;
         VAR(w->token) = POP()),                      // update value
    CODE("is",                                        // w --
         dict.push(new Code(next_idiom()));           // create word
         int n = POP();                               // like this word
         dict[-1]->xt = dict[n]->xt;                  // if primitive
         dict[-1]->pf = dict[n]->pf),                 // or colon word
    // debugging ops
    CODE("here",  PUSH(dict[-1]->token)),
    CODE("words", words()),
    CODE(".s",    ss_dump()),
    CODE("'",     Code *w = find(next_idiom()); if (w) PUSH(w->token)),
    CODE("see",   Code *w = find(next_idiom()); if (w) see(w, 0); cout << endl),
    CODE("ms",    PUSH(millis())),
    CODE("forget",
         Code *w = find(next_idiom()); if (!w) return;
         int t = max(w->token, find("boot")->token);
         for (int i=dict.size(); i>t; i--) dict.pop()),
    CODE("boot",
         int t = find("boot")->token + 1;
         for (int i=dict.size(); i>t; i--) dict.pop())
};
void dict_setup() {           ///> setup dictionary
    dict[0]->add(new Code("_var", 10));  /// * borrow dict[0] for base
}
Code *find(string s) {        ///> search dictionary from last to first
    for (int i = dict.size() - 1; i >= 0; --i) {
        if (s == dict[i]->name) return dict[i];
    }
    return NULL;              /// * word not found
}
void words() {                ///> display word list
    const int WIDTH = 60;
    int i = 0, x = 0;
    cout << setbase(16) << setfill('0');
    for (Code *w : dict) {
#if DEBUG
        cout << setw(4) << w->token << "> "
             << setw(8) << (uintptr_t)w->xt
             << ":" << (w->immd ? '*' : ' ')
             << w->name << "  " << endl;
#else
        if (w->name[0]=='_') continue;
        cout << w->name << "  ";
        x += (w->name.size() + 2);
#endif
        if (x > WIDTH) { cout << endl; x = 0; }
    }
    cout << setbase(BASE) << endl;
}
///
///> main - Forth outer interpreter
///
int main(int ac, char* av[]) {
    dict_setup();
    cout << "ceForth v4.1" << endl;
    string idiom;
    while (cin >> idiom) {                ///> read from input stream
        try {
            Code *w = find(idiom);        /// * search through dictionary
            if (w) {                      /// * word found?
                if (compile && !w->immd)
                    dict[-1]->add(w);     /// * add to colon word
                else w->exec();           /// * execute forth word
            }
            else {
                int n = stoi(idiom, nullptr, BASE);     /// * convert to integer
                if (compile)
                    dict[-1]->add(new Code("_lit", n)); /// * add to current word
                else PUSH(n);                           /// * add value to data stack
            }
        }
        catch (...) {                     /// * failed to parse number
            cout << idiom << "? " << endl;
            compile = false;
            getline(cin, idiom, '\n');    /// * flush to end-of-line
        }
        if (cin.peek()=='\n' && !compile) ss_dump();    /// * dump stack and display ok prompt
    }
    cout << "Done!" << endl;
    return 0;
}
