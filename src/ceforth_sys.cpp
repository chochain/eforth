///
/// @file
/// @brief eForth - System Dependent Interface
///
///====================================================================
#include <sstream>                     /// iostream, stringstream
#include <cstring>
#include "ceforth.h"

using namespace std;
///
///> I/O streaming interface
///
istringstream   fin;                   ///< forth_in
ostringstream   fout;                  ///< forth_out
string          pad;                   ///< string buffers
void (*fout_cb)(int, const char*);     ///< forth output callback functi
///====================================================================
///
///> IO functions
///
string word(char delim) {            ///> read next idiom form input stream
    string s; delim ? getline(fin, s, delim) : fin >> s; return s;
}
void ss_dump(DU base) {              ///> display data stack and ok promt
    char buf[34];
    auto rdx = [&buf](DU v, int b) {      ///> display v by radix
#if USE_FLOAT
        sprintf(buf, "%0.6g", v);
        return buf;
#else // !USE_FLOAT
        int i = 33;  buf[i]='\0';         /// * C++ can do only 8,10,16
        DU  n = ABS(v);                   ///< handle negative
        do {
            U8 d = (U8)MOD(n, b);  n /= b;
            buf[--i] = d > 9 ? (d-10)+'a' : d+'0';
        } while (n && i);
        if (v < 0) buf[--i]='-';
        return &buf[i];
#endif // USE_FLOAT
    };
    ss.push(tos);
    for (DU v : ss) { fout << rdx(v, base) << ' '; }
    tos = ss.pop();
    fout << "-> ok" << ENDL;
}
void _see(Code *c, int dp) {         ///> disassemble a colon word
    auto pp = [](string s, FV<Code*> v, int dp) {  ///> recursive dump with indent
        int i = dp;
        if (dp && s!="\t") fout << ENDL;           ///> newline control
        while (i--) { fout << "  "; } fout << s;   ///> indentation control
        if (dp < 2) for (Code *w : v) _see(w, dp + 1);
    };
    auto pq = [](FV<DU> q) {
        for (DU i : q) fout << i << (q.size() > 1 ? " " : "");
    };
    const FV<Code*> zz = {};
    string sn(c->name);
    if (c->is_str) sn = (c->token ? "s\" " : ".\" ") + sn + "\"";
    pp(sn, c->pf, dp);
    if (sn=="if")    {
        if (c->stage==1) pp("else", c->p1, dp);
        pp("then", zz, dp);
    }
    else if (sn=="begin") {
        switch (c->stage) {
        case 0: pp("until", zz, dp); break;
        case 1: pp("again", zz, dp); break;
        case 2:
            pp("while",  c->p1, dp);
            pp("repeat", zz,    dp);
            break;
        }
    }
    else if (sn=="for") {
        if (c->stage==3) {
            pp("aft",  c->p1, dp);
            pp("then", c->p2, dp);
        }
        pp("next", zz, dp);
    }
    else if (sn=="do") {
        pp("loop", zz, dp);
    }
    else pq(c->q);
}
void see(Code *c) {
    if (c->xt) fout << "  ->{ " << c->desc << "; }";
    else {
        fout << ": "; _see(c, 0); fout << " ;";
    }
}
void words() {              ///> display word list
    const int WIDTH = 60;
    int x = 0;
    fout << setbase(16) << setfill('0');
    for (Code *w : dict) {
#if CC_DEBUG > 1
        fout << setw(4) << w->token << "> "
             << (UFP)w << ' '
             << setw(8) << static_cast<U32>((UFP)w->xt)
             << (w->is_str ? '"' : ':') << (w->immd ? '*' : ' ')
             << w->name << "  " << ENDL;
#else // !CC_DEBUG
        fout << "  " << w->name;
        x += (strlen(w->name) + 2);
        if (x > WIDTH) { fout << ENDL; x = 0; }
#endif // CC_DEBUG
    }
    fout << setfill(' ') << setbase(BASE) << ENDL;
}
void load(const char *fn) {          ///> include script from stream
    void (*cb)(int, const char*) = fout_cb;  ///< keep output function
    string in; getline(fin, in);             ///< keep input buffers
    fout << ENDL;                            /// * flush output
    
    forth_include(fn);                       /// * send script to VM
    
    fout_cb = cb;                            /// * restore output cb
    fin.clear(); fin.str(in);                /// * restore input
}
