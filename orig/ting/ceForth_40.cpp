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

typedef void (*op)(Code*);
map<string, op> lookUp {
    { "bye",    [](Code *c){ exit(0); }},
    { "qrx",    [](Code *c){ push(getchar()); if (top!=0) push(TRUE); }},
    { "txsto",  [](Code *c){ putchar((char)top); pop(); }},
    { "dup",    [](Code *c){ push(top); }},
    { "drop",   [](Code *c){ pop(); }},
    { "+",      [](Code *c){ top+=pop(); }},
    { "dolit",  [](Code *c){ push(c->qf.front()); }},
    { "if",     [](Code *c){
            dict.push_back(new Code("branch", false));
            dict.push_back(new Code("temp", false)); }}
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
    for (Code *w:pf) {
        try { w->xt(); IP++; }         // inner interpreter
        catch (int e) {}
    }
    IP=popR(); WP=popR();              // return to caller
}

void dict_setup() {
    map<string, int> immd { {"bye",1},{"if",1} };
    for (pair<string, op> p:lookUp) {                           // map iterator
        string n  = p.first;                                    // get the key
        Code   *c = new Code(n);                                // create Code object
        if (immd.find(n) != immd.end()) c->immediate();         // mark immediate flag
        dict.push_back(c);                                      // add to dictionary
    }
}

void words() {
    for (Code *w:dict) {                                        // vector iterator
        cout << w->name << " " << w->token << (w->immd ? "*" : " ") << "\n";
    }
}
/*
* Main Program
*/
int main(int ac, char* av[])
{
    dict_setup();

    words();

    return 0;
}



