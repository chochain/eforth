#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <map>

using namespace std;

int top = 0, r = 0;                                  // cached top of ss
int w, ip, wp, ntib, base = 10;
bool compiling = false;
string cmd = ": sq dup * ; : qd sq sq ; 8 qd . \n";
string tib = "", idiom = "";
char delimit = ' ';

#define pop()    (top=ss.back(),ss.pop_back(),top)
#define push(v)  (ss.push_back(top),top=(v))
#define popR()   (r=rs.back(),rs.pop_back(),r)
#define pushR(v) (rs.push_back(v),r=v)


class Code {                                 // one size fits all objects
public:
    static int fence;
    string name;
    int    idiom = 0;
    int    stage = 0;
    bool   immd = false;
    string literal;
    vector<Code*> pf;
    vector<Code*> pf1;
    vector<Code*> pf2;
    vector<int>   qf;
    Code(string n);
    Code(string n, bool f);
    Code(string n, int d);
    Code(string n, string l);
    Code* immediate();
    Code* addCode(Code* w);
    void xt();
};

vector<int>   rs;                               // return ss
vector<int>   ss;                               // parameter ss
vector<Code*> dict;                             // dictionary

void dump() {
    cout << "< ";
    for (int i : ss) { cout << i << " "; }
    cout << top << " >ok\n";
}
void words() {
    int i = 0;
    cout << "\n";
    for (Code* w : dict) {                                        // vector iterator
        cout << w->name << " ";
        if (i > 9) { cout << "\n"; i = 0; }
        i++;
    }
    cout << "\n";
}

#define VT(s, g) { s, [](Code *c){ g; }}
typedef void (*op)(Code*);
map<string, op> lookUp{
    VT("dup",   ss.push_back(top)),
    VT("drop",  top = ss.back(); ss.pop_back();),
    VT("+",     top += ss.back(); ss.pop_back();),
    VT("*"    , top *= ss.back(); ss.pop_back();),
    VT("dolit", ss.push_back(c->qf.front())),
    VT("hi",    cout << "hi\n"),
    VT("here" , ss.push_back(top);top=(dict.size())),
    VT(","    , Code("dolit",top);top=ss.back();ss.pop_back()),
    VT("exit"  , throw(" ");),
    //    VT(":"    ,
    //    parse(); compiling = true;
    //    dict.push(VT(idiom, this.pf : []
    //    ),
    //    VT(";"    , compiling = false; Code("exit"); this.immd : true}
    VT("."    , cout << top + " ";top=ss.back();ss.pop_back();),
    VT(".s"    , dump()),
    VT("words"    , words())
};
//
// Code class implementation
//
int Code::fence = 0;
Code::Code(string n) { name = n; idiom = fence++; }
Code::Code(string n, bool f) { name = n; if (f) idiom = fence++; }
Code::Code(string n, int d) { name = n; qf.push_back(d); }
Code::Code(string n, string l) { name = n; literal = l; }

Code* Code::immediate() { immd = true; return this; }
Code* Code::addCode(Code* w) { pf.push_back(w); return this; }
void  Code::xt() {
    auto itr = lookUp.find(name);
    if (itr != lookUp.end()) {         // check if primitive word found
        itr->second(this);             // pass this Code object to the lambda void found
        return;
    }
    pushR(wp); pushR(ip);              // prep call frame for colon word
    wp = idiom; ip = 0;                    // point to current dolist
    for (Code* w : pf) {
        try { w->xt(); ip++; }         // inner interpreter
        catch (int e) {}
    }
    ip = popR(); wp = popR();              // return to caller
}
//
// main class
//
void dict_setup() {
    map<string, int> immd{ {";",1} };
    for (pair<string, op> p : lookUp) {                           // map iterator
        string n = p.first;                                    // get the key
        Code* c = new Code(n);                                // create Code object
        if (immd.find(n) != immd.end()) c->immediate();         // mark immediate flag
        dict.push_back(c);                                      // add to dictionary
    }
}
//
// Main Program
//
void parse(char limit) {
    idiom = "";
    while (tib[ntib] == ' ') ntib++;
    while (ntib < tib.size() && tib[ntib] != limit) idiom += tib[ntib++];
    return;
}
Code* findword(string name) {
    for (Code* w : dict) { if (w->name == name) return w; }
    return NULL;
}
void evaluate(string idiom) {               // outer loop
    Code* nword = findword(idiom);
    int n;
    if (nword != NULL) {
        if (compiling && !nword->immd) { Code(idiom,0); }
        else { nword->xt(); }
    }               // docol, docon, dovar need pf
    else {
        try {
            n =atoi(idiom.c_str());                     // if the idiom is a number
            if (compiling) {
                Code("dolit", n);            // compile an literal
            }
            else { ss.push_back(top); top = n; }
        }
        catch (string str) { cmd = ""; (cout << str+" " + idiom + "?"); }
    }
}
void spush(int n) { ss.push_back(top); top = n; }

int main(int ac, char* av[])
{
    cout << "ceforth_401\n";
    dict_setup();
    words();
    spush(123);
    spush(100);
    spush(120);
    dump();
    while (true) {
        getline(cin,tib);
        ntib = 0;
        while (true) {
            parse(' ');
            if (idiom.length() == 0) {
                 dump(); break;
            }
            evaluate(idiom);
        }
    }
}
