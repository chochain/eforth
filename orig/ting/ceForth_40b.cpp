#include <iostream>
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
    T operator[](int i) { return i<0 ? v[v.size()+i] : v[i]; }
    T operator<<(T t)   { v.push_back(t); }
    T pop()             {
        if (v.empty()) throw length_error("ERR: stack empty");
        T t=v.back(); v.pop_back(); return t;
    }
    void push(T t)      { v.push_back(t); }
    void clear()        { v.clear(); }
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
    bool   immd  = false;                   /// immediate flag
    fop    xt    = NULL;                    /// primitive function
    string literal;                         /// string literal
    int    stage = 0;                       /// branching stage
    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<int>   qf;

    Code(string n, fop fn, bool im=false);  /// primitive
    Code(string n, bool f=false);           /// new colon word or temp
    Code(string n, int d);                  /// dolit
    Code(string n, string l);               /// dostr

    Code   *immediate();                    /// set immediate flag
    Code   *addcode(Code *w);               /// append colon word
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

bool cmpi = false;                          /// compiling flag
int  base = 10;                             /// numeric radix
int  top  = 0;                              /// cached top of stack
int  IP, WP;                                /// instruction and parameter pointers
///
/// dictionary search function
///
Code *find(string s)    {                   /// search dictionary reversely
    for (int i=dict.v.size()-1; i>=0; --i) {
        if (s==dict.v[i]->name) return dict.v[i];
    }
    return NULL;
}
///
/// constructors
///
int Code::fence = 0;
Code::Code(string n, fop fn, bool im) { name=n; token=fence++; xt=fn; immd=im; }
Code::Code(string n, bool f)   { Code *c=find(name=n); if (c) xt=c->xt; if (f) token=fence++; }
Code::Code(string n, int d)    { xt=find(name=n)->xt; qf.push(d); }
Code::Code(string n, string l) { xt=find(name=n)->xt; literal=l;  }
///
/// public methods
///
Code *Code::immediate()      { immd=true;  return this; }
Code *Code::addcode(Code *w) { pf.push(w); return this; }
void  Code::exec() {
    if (xt) { xt(this); return; }       /// * execute primitive word and return
    rs.push(WP); rs.push(IP);           /// * execute colon word
    WP=token; IP=0;                     /// * setup dolist call frame
    for (Code *w: pf.v) {               /// * inner interpreter
        try { w->xt(w); IP++; }         /// * pass Code object to xt
        catch (int e) {}
    }
    IP=rs.pop(); WP=rs.pop();           /// * return to caller
}
string Code::to_s() {
    return name+" "+to_string(token)+(immd ? "*" : " ");
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
    for (int i:ss.v) { cout << i << "_"; }
    cout << top << "_ok" << endl;
}
void see(Code *c, int dp) {
    auto tab = [](int i)         { cout<<endl; while(i--) cout << "  "; };          // lambda for indentation
auto qf  = [](vector<int> v) { cout<<"="; for (int i:v) cout << i << " "; };
    tab(dp); cout << "[ " << c->to_s();
    for (Code *w: c->pf.v)  see(w, dp+1);    /// call recursively
    if (c->pf1.v.size()>0) {
        tab(dp); cout << "---";
        for (Code *w: c->pf1.v) see(w, dp+1);
    }
    if (c->qf.v.size()>0) qf(c->qf.v);
    cout << "]";
}
void words() {
    int i=0;
    for (Code *w:dict.v) {
        cout << w->to_s() << " ";
        if ((++i%10)==0) cout << endl;
    }
    cout << endl;
}
void _then() {
    Code *temp=dict[-1], *bran=dict[-2]->pf[-1];
    if (bran->stage==0) {                   // if...then
        bran->pf.merge(temp->pf.v);
        dict.pop();
    }
    else {                                  // if...else...then
        bran->pf1.merge(temp->pf.v);
        if (bran->stage==1) dict.pop();
        else temp->pf.clear();
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
    CODE("hi",    cout << "---->hi!" << endl),
    // IO examples
    IMMD("bye",   exit(0)),             // lambda using macro to shorten
    CODE("qrx",   PUSH(getchar()); if (top!=0) PUSH(TRUE)),
    CODE("txsto", putchar((char)top); POP()),
    // stack op examples
    CODE("dup",   PUSH(top)),
    CODE("drop",  POP()),
    // ALU examples
    CODE("+",     top+=ss.pop()),       // note: ss.pop() is different from POP()
    CODE("-",     top=ss.pop()-top),
    // external function examples
    CODE("over",  _over()),
    CODE("2dup",  _2dup()),
    CODE("words", words()),
    // branching examples
    IMMD("bran",
        bool f=top!=0; POP();           // check flag then update top
        for (Code *w:(f ? c->pf.v : c->pf1.v)) w->exec()),
    IMMD("if",
        dict[-1]->addcode(new Code("bran"));    // bran=word->pf
        dict.push(new Code("temp"))),           // use last cell of dictionay as scratch pad
    IMMD("else",
        Code *temp=dict[-1];
        Code *bran=dict[-2]->pf[-1];    // branching node
        bran->pf.merge(temp->pf.v);
        temp->pf.clear();
        bran->stage=1),
    IMMD("then", _then()),              // externalize to help single-step debugging
    // string and numeric examples
    IMMD(".\"",
        string s; getline(cin, s, '"'); /// * copy string upto delimiter
        dict[-1]->addcode(new Code("dotstr",s))),
    CODE("dotstr",cout << c->literal),
    CODE("dolit", PUSH(c->qf[0])),
    // compiler examples
    CODE(":",
        string s; cin >> s;            /// * get next token
        dict.push(new Code(s, true));  /// * create new word
        cmpi=true),
    IMMD(";",     cmpi=false),
    CODE("dovar", PUSH(c->token)),
    IMMD("variable",
        string s; cin >> s;            /// * create new variable
        dict.push(new Code(s, true));
        Code *last=dict[-1]->addcode(new Code("dovar", 0));
        last->pf[0]->token=last->token),
    // debugging
    CODE(".s",  ss_dump()),
    IMMD("see",
        string s; cin >> s;            /// * fetch next token
        Code *w = find(s);
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
    string tok;
    while (cin >> tok) {
        // cout << tok << endl;
        if (tok=="bye") break;
        Code *w = find(tok);            /// * search through dictionary
        if (w) {                        /// * word found?
            if (cmpi && !w->immd) {     /// * in compile mode?
                dict[-1]->addcode(w);   /// * add to colon word
            }
            else {
                try { w->exec(); }      /// * execute forth word
                catch (exception &e) {
                    cout << e.what() << endl;
                }
            }
        }
        else {                          /// * try as numeric
            try {
                int n = stoi(tok, nullptr, base);   /// * convert to integer
                if (cmpi) {
                    dict[-1]->addcode(new Code("dolit",n)); /// * add to current word
                }
                else PUSH(n);           /// * add value onto data stack
            }
            catch(...) {                /// * failed to parse number
                cout << tok << "? " << endl;
                cmpi = false;
            }
        }
        if (!cmpi) ss_dump();           /// * stack dump and display ok prompt
    }
}
//
// Main Program
//
int main(int ac, char* av[]) {
    dict_setup();
    cout << "ceforth v4" << endl;
    outer();
    cout << "done!" << endl;
    return 0;
}
