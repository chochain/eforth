#include <iostream>
#include <vector>
#include <functional>
#include <map>

using namespace std;

int top = 0;                                  // cached top of stack
int P, IP, WP;

#define FALSE   0
#define TRUE    -1

#define pop()    (top=ss.back(),ss.pop_back(),top)
#define push(v)  (ss.push_back(top),top=(v))
#define popR()   ({int r=rs.back(); rs.pop_back(); r;})
#define pushR(v) (rs.push_back(v))

class Code {                                 // one size fits all objects
public:
    static int fence;
    string name;
    int    token = 0;
    int    stage = 0;
    bool   immd  = false;
    string literal;
    vector<Code*> pf;
    vector<Code*> pf1;
    vector<Code*> pf2;
    vector<int>   qf;

    Code(string n);
    Code(string n, bool f);
    Code(string n, int d);
    Code(string n, string l);

    Code *immediate();
    Code *addCode(Code *w);
    
    void xt();
};

vector<int>   rs;                               // return stack
vector<int>   ss;                               // parameter stack
vector<Code*> dict;                             // dictionary

#define VT(s, g) { s, [](Code *c){ g; }}

void _sub(Code *c) {
    top = ss.back() - top;
    ss.pop_back();
}
void _over(Code *c) {
	int v = ss.back();
	push(v);
}
void _ddup(Code *c) {
	_over(c); _over(c);
}

typedef void (*op)(Code*);
map<string, op> lookUp {
    VT("bye",   exit(0)),					// lambda using macro to shorten
    VT("qrx",   push(getchar()); if (top!=0) push(TRUE)),
    VT("txsto", putchar((char)top); pop()),
    VT("dup",   push(top)),
    VT("drop",  pop()),
    VT("+",     top+=pop()),
    VT("if",	dict.push_back(new Code("branch", false)); \
    			dict.push_back(new Code("temp", false)) ),
    VT("dolit", push(c->qf.front())),
    VT("dovar", push(c->token)),
    VT("hi",    cout << "hi\n"),
    { "sub",    _sub },                    // direct functions
    { "over",   _over},
    { "2dup",   _ddup}                     // compiled
};
//
// Code class implementation
//
int Code::fence = 0;
Code::Code(string n)           { name=n; token=fence++;        }
Code::Code(string n, bool f)   { name=n; if (f) token=fence++; }
Code::Code(string n, int d)    { name=n; qf.push_back(d);      }
Code::Code(string n, string l) { name=n; literal=l;            }

Code *Code::immediate()        { immd=true; return this;       }
Code *Code::addCode(Code *w)   { pf.push_back(w); return this; }
void  Code::xt() {
    auto itr=lookUp.find(name);
    if (itr != lookUp.end()) {         // check if primitive word found
        itr->second(this);             // pass this Code object to the lambda function found
        return;
    }
    pushR(WP); pushR(IP);              // prep call frame for colon word
    WP=token; IP=0;                    // point to current dolist
    for (Code *w: pf) {
        try { w->xt(); IP++; }         // inner interpreter
        catch (int e) {}
    }
    IP=popR(); WP=popR();              // return to caller
}
//
// main class
//
void dict_setup() {
	map<string, int> immd { {"bye",1},{"if",1} };
    for (pair<string, op> p:lookUp) {                           // map iterator
        string n  = p.first;                                    // get the key
        Code   *c = new Code(n);                                // create Code object
        if (immd.find(n) != immd.end()) c->immediate();         // mark immediate flag
        dict.push_back(c);                                      // add to dictionary
    }
}
void ss_dump() {
	for (int i: ss) {
		cout << i << "_";
	}
	cout << top << "_ok\n";
}
void words() {
    for (Code *w:dict) {                                        // vector iterator
        cout << w->name << " " << w->token << (w->immd ? "*" : " ") << " ";
    }
    cout << "\n";
}
//
// Main Program
//
int main(int ac, char* av[])
{
    dict_setup();
    words();

    // unit tests
    lookUp.find("hi")->second(NULL);	// OK to pass a NULL because Code *c is not used in the function

    push(123);
    push(100);
    push(12);
    ss_dump();
    lookUp.find("sub")->second(NULL);
    ss_dump();

    lookUp.find("2dup")->second(NULL);
    ss_dump();

    lookUp.find("if")->second(NULL);
    words();

    return 0;
}
