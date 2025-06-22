///
/// @file
/// @brief eForth - System Dependent Interface
///
///====================================================================
#include <sstream>                     /// iostream, stringstream
#include <cstring>
#include "ceforth.h"
using namespace std;

extern FV<Code*> dict;
///
///> I/O streaming interface
///
istringstream   fin;                   ///< forth_in
ostringstream   fout;                  ///< forth_out
void (*fout_cb)(int, const char*);     ///< forth output callback functi
int load_dp = 0;                       ///< load depth control
///====================================================================
///
///> IO functions
///
void fin_setup(const char *line) {
    fout.str("");                      /// * clean output buffer
    fin.clear();                       /// * clear input stream error bit if any
    fin.str(line);                     /// * reload user command into input stream
}
void fout_setup(void (*hook)(int, const char*)) {
    auto cb = [](int, const char *rst) { printf("%s", rst); };
    fout_cb = hook ? hook : cb;        ///< serial output hook up
}
const char *scan(char c) {
    static string s;                   ///< temp str, static prevents reclaim
    getline(fin, s, c);                ///< scan fin for char c
    return s.c_str();                  ///< return the reference
}
const char *word(char delim) {         ///> read next idiom form input stream
    static string s;                   ///< temp str, static prevents reclaim
    delim ? getline(fin, s, delim) : (fin >> s);

    if (s.size()) return s.c_str();    ///< return a new copy of string

    pstr(" ?str");
    return NULL;
}
int fetch(string &idiom) {             ///> read an idiom from input stream
    return !(fin >> idiom)==0;
}
char key() { return word()[0]; }
void load(VM &vm, const char *fn) {    ///> include script from stream
    load_dp++;                         /// * increment depth counter
    void (*cb)(int, const char*) = fout_cb;  ///< keep output function
    string in; getline(fin, in);             ///< keep input buffers
    fout << ENDL;                      /// * flush output

    vm.rs.push(vm.state);              /// * save context
    vm.set_state(NEST);
    forth_include(fn);                 /// * send script to VM
    vm.set_state(static_cast<vm_state>(vm.rs.pop()));
    
    fout_cb = cb;                      /// * restore output cb
    fin.clear(); fin.str(in);          /// * restore input
    --load_dp;                         /// * decrement depth counter
}
void spaces(int n) { for (int i = 0; i < n; i++) fout << " "; }
void dot(io_op op, DU v) {
    switch (op) {
    case RDX:   fout << setbase(UINT(v));               break;
    case CR:    fout << ENDL;                           break;
    case DOT:   fout << v << " ";                       break;
    case UDOT:  fout << static_cast<U32>(v) << " ";     break;
    case EMIT:  { char b = (char)UINT(v); fout << b; }  break;
    case SPCS:  spaces(UINT(v));                        break;
    default:    fout << "unknown io_op=" << op << ENDL; break;
    }
}
void dotr(int w, DU v, int b, bool u) {
    fout << setbase(b) << setw(w)
         << (u ? static_cast<U32>(v) : v);
}
void pstr(const char *str, io_op op) {
    fout << str;
    if (op==CR) { fout << ENDL; }
}
///====================================================================
///
///> Debug functions
///
void ss_dump(VM &vm, bool forced) {       ///> display data stack and ok promt
    if (load_dp) return;                  /// * skip when including file
#if DO_WASM    
    if (!forced) { fout << "ok" << ENDL; return; }
#endif // DO_WASM
    char buf[34];
    auto rdx = [&buf](DU v, int b) {      ///> display v by radix
#if USE_FLOAT
        DU t, f = modf(v, &t);            ///< integral, fraction
        if (ABS(f) > DU_EPS) {
		    snprintf(buf, 32, "%0.6g", v);
            return buf;
        }
#endif // USE_FLOAT
        int i = 33;  buf[i]='\0';         /// * C++ can do only 8,10,16
        int dec = b==10;
        U32 n   = dec ? UINT(ABS(v)) : UINT(v);  ///< handle negative
        do {
            U8 d = (U8)MOD(n, b);  n /= b;
            buf[--i] = d > 9 ? (d-10)+'a' : d+'0';
        } while (n && i);
        if (dec && v < DU0) buf[--i]='-';
        return &buf[i];
    };
    SS.push(TOS);
    for (DU v : SS) { fout << rdx(v, *vm.base) << ' '; }
    TOS = SS.pop();
    fout << "-> ok" << ENDL;
}
void _see(const Code &c, int dp) {       ///> disassemble a colon word
    string sn(c.name);
    if (c.is_str) sn = (c.token ? "s\" " : ".\" ") + sn + "\"";
    else if (sn=="zbran" || sn=="bran" || sn=="next" || sn=="loop") {
        sn += " ( j" + to_string(c.token) + " )";
    }
    
    fout << " " << sn;                   ///> print name
    for (DU i : c.q) fout << i << " ";   ///> print if value
    fout << ENDL;
    if (c.xt || dp > 1) return;          /// * depth control
    
    int j = 0;
    for (auto w : ((Colon*)&c)->pf) {
        int i=dp;
        while (i--) fout << "  ";        ///> indent control
        fout << "( " << setfill('0')
             << setw(3) << j++
             << '['
             << setw(3) << w->token
             << "] )";
        _see(*w, dp + 1);                ///> walk recursively
    }
}
void see(const Code &c, int base) {
    if (c.xt) fout << c.name << " ->{ " << c.desc << "; }";
    else {
        fout << ":"; _see(c, 0); fout << ";";
    }
}
void words(int base) {                    ///> display word list
    const int WIDTH = 60;
    int x = 0;
    fout << setbase(16) << setfill('0');
    for (auto w : dict) {
#if CC_DEBUG > 1
        fout << setw(4) << w->token << "> "
             << (UFP)w << ' '
             << setw(8) << static_cast<U32>((UFP)w->xt)
             << (w->is_str ? '"' : ':') << (w->immd ? '*' : ' ')
             << w->name << "  " << ENDL;
#else // !CC_DEBUG
        fout << "  " << w->name;
        x += ((int)strlen(w->name) + 2);
        if (x > WIDTH) { fout << ENDL; x = 0; }
#endif // CC_DEBUG
    }
    fout << setfill(' ') << setbase(base) << ENDL;
}
///====================================================================
///
///> System statistics - for heap, stack, external memory debugging
///
void dict_dump(int base) {
    fout << setbase(16) << ENDL;
    for (auto i = dict.begin(); i != dict.end(); i++) {
        fout << setfill('0') << setw(3) << (int)(i - dict.begin())
             << "> name=" << setw(8) << (UFP)(*i)->name
             << ", xt="   << setw(8) << (UFP)(*i)->xt
             << ", attr=" << setw(8) << (*i)->attr
             << " "       << (*i)->name << ENDL;
    }
    fout << setbase(base) << setfill(' ') << setw(-1);
}
void mem_dump(IU w0, IU w1, int base) {
    auto show_pf = [](const char *nm, FV<Code*> pf) {
        if (pf.size() == 0) return;
        fout << "  " << nm << ": ";
        for (auto p : pf) { fout << p->token << " "; }
        fout << ENDL;
    };
    fout << setbase(16) << setfill('0');
    auto cx = dict.begin() + w1 + 1;
    for (auto i = dict.begin() + w0; i != cx; i++) {

        fout << setw(4) << (int)(i - dict.begin()) << ": ";
        Code *w = *i;
        if (w->xt) { fout << "built-in" << ENDL; continue; }
        
        fout << w->name << ENDL;
        
        show_pf("pf", ((Colon*)w)->pf);
        
        if (w->q.size()==0) continue;
        fout << "  q:";
        for (auto v : w->q) { fout << v << " "; }
        fout << ENDL;
        
    }
    fout << setbase(base) << setfill(' ');
}
///====================================================================
