#include <iomanip>          // setbase, setw, setfill
#include "ceforth.h"
#define APP_NAME         "eForth"
#define MAJOR_VERSION    "8"
#define MINOR_VERSION    "1"
///==============================================================================
///
/// global memory blocks
///
List<DU,   E4_SS_SZ>   rs;                  /// return stack
List<DU,   E4_RS_SZ>   ss;                  /// parameter stack
List<Code, E4_DICT_SZ> dict;                /// dictionary
List<U8,   E4_PMEM_SZ> pmem;                /// parameter memory (for colon definitions)
U8  *MEM0   = &pmem[0];                     /// base of parameter memory block
UFP DICT0;                                  /// base of dictionary
///==============================================================================
///
/// colon word compiler
/// Note:
///   * we separate dict and pmem space to make word uniform in size
///   * if they are combined then can behaves similar to classic Forth
///   * with an addition link field added.
///
string &ForthVM::next_idiom(char delim) {
    delim ? getline(fin, idiom, delim) : fin >> idiom; return idiom;
}
void  ForthVM::colon(const char *name) {
    char *nfa = (char*)&pmem[HERE];         // current pmem pointer
    int sz = STRLEN(name);                  // string length, aligned
    pmem.push((U8*)name,  sz);              // setup raw name field
#if LAMBDA_OK
    Code c(nfa, [](){});                    // create a new word on dictionary
#else  // LAMBDA_OK
    Code c(nfa, NULL);
#endif // LAMBDA_OK
    c.def = 1;                              // specify a colon word
    c.len = 0;                              // advance counter (by number of U16)
    c.pfa = HERE;                           // capture code field index
    dict.push(c);                           // deep copy Code struct into dictionary
    printf("%3d> pfa=%x, name=%4x:%p %s\n", dict.idx-1,
        dict[-1].pfa,
        (U16)(dict[EXIT].name - (const char*)MEM0),
        dict[-1].name, dict[-1].name);
}
///==============================================================================
///
/// dictionary search functions - can be adapted for ROM+RAM
///
int ForthVM::pfa2word(U8 *ip) {
    IU   ipx = *(IU*)ip;
    U8   *fp = (U8*)(DICT0 + ipx);
    for (int i = dict.idx - 1; i >= 0; --i) {
        if (ipx & 1) {
            if (dict[i].pfa == (ipx & ~1)) return i;
        }
        else if ((U8*)dict[i].xt == fp) return i;
    }
    return -1;
}
int ForthVM::streq(const char *s1, const char *s2) {
    return ucase ? strcasecmp(s1, s2)==0 : strcmp(s1, s2)==0;
}
int ForthVM::find(const char *s) {
    for (int i = dict.idx - (compile ? 2 : 1); i >= 0; --i) {
        if (streq(s, dict[i].name)) return i;
    }
    return -1;
}
///
/// VM ops
///
void ForthVM::call(IU w) {
    if (dict[w].def) {
        WP = w;
        IP = MEM0 + dict[w].pfa;
        nest();
    }
    else {
#if LAMBDA_OK
        (*(fop*)((UFP)dict[w].xt & ~0x3))();
#else  // LAMBDA_OK
        (*(fop)(DICT0 + (*(IU*)IP & ~0x3))();
#endif // LAMBDA_OK
    }
}
void ForthVM::nest() {
    int dp = 0;                                      /// iterator depth control
    while (dp >= 0) {
        /// function core
        auto ipx = *(IU*)IP;                         /// hopefully use register than cached line
        while (ipx) {
            if (ipx & 1) {
                rs.push(WP);                         /// * setup callframe (ENTER)
                rs.push(OFF(IP) + sizeof(IU));
                IP = MEM0 + (ipx & ~0x1);            /// word pfa (def masked)
                dp++;
            }
            else {
                UFP xt = DICT0 + (ipx & ~0x3);       /// * function pointer
                IP += sizeof(IU);                    /// advance to next pfa
                (*(fop*)xt)();
            }
            ipx = *(IU*)IP;
        }
        if (dp-- > 0) {
            IP = MEM0 + rs.pop();                    /// * restore call frame (EXIT)
            WP = rs.pop();
        }
    }
    yield();                                ///> give other tasks some time
}
///==============================================================================
///
/// debug functions
///
void ForthVM::dot_r(int n, int v) {
    fout << setw(n) << setfill(' ') << v;
}
void ForthVM::to_s(IU c) {
    fout << dict[c].name << " " << c << (dict[c].immd ? "* " : " ");
}
///
/// recursively disassemble colon word
///
void ForthVM::see(U8 *ip, int dp) {
    while (*(IU*)ip) {
        fout << ENDL; for (int i=dp; i>0; i--) fout << "  ";        // indentation
        fout << setw(4) << OFF(ip) << "[ " << setw(-1);
        IU c = pfa2word(ip);
        to_s(c);                                                    // name field
        if (dict[c].def && dp <= 2) {                               // is a colon word
            see(PFA(c), dp+1);                                      // recursive into child PFA
        }
        ip += sizeof(IU);
        switch (c) {
        case DOVAR: case DOLIT:
            fout << "= " << *(DU*)ip; ip += sizeof(DU); break;
        case DOSTR: case DOTSTR:
            fout << "= \"" << (char*)ip << '"';
            ip += STRLEN((char*)ip); break;
        case BRAN: case ZBRAN: case DONEXT:
            fout << "j" << *(IU*)ip; ip += sizeof(IU); break;
        }
        fout << "] ";
    }
}
void ForthVM::words() {
    fout << setbase(16);
    for (int i=0; i<dict.idx; i++) {
        if ((i%10)==0) { fout << ENDL; yield(); }
        to_s(i);
    }
    fout << setbase(base);
}
void ForthVM::ss_dump() {
    fout << " <"; for (int i=0; i<ss.idx; i++) { fout << ss[i] << " "; }
    fout << top << "> ok" << ENDL;
}
void ForthVM::mem_dump(IU p0, DU sz) {
    fout << setbase(16) << setfill('0') << ENDL;
    for (IU i=ALIGN16(p0); i<=ALIGN16(p0+sz); i+=16) {
        fout << setw(4) << i << ": ";
        for (int j=0; j<16; j++) {
            U8 c = pmem[i+j];
            fout << setw(2) << (int)c << (j%4==3 ? "  " : " ");
        }
        for (int j=0; j<16; j++) {   // print and advance to next byte
            U8 c = pmem[i+j] & 0x7f;
            fout << (char)((c==0x7f||c<0x20) ? '_' : c);
        }
        fout << ENDL;
        yield();
    }
    fout << setbase(base);
}
///
/// ForthVM Outer interpreter
///
void ForthVM::outer() {
    while (fin >> idiom) {
        //printf("%s=>", idiom.c_str());
        int w = find(idiom);                        /// * search through dictionary
        if (w >= 0) {                               /// * word found?
            //printf("%s(%ld)\n", w->to_s().c_str(), w.use_count());
            if (compile && !dict[w].immd)           /// * in compile mode?
                add_w(w);                           /// * add to colon word
            else call(w);                           /// * execute forth word
            continue;
        }
        // try as a number
        char *p;
#if DU==float
        DU n = (base==10)
            ? static_cast<DU>(strtof(idiom.c_str(), &p))
            : static_cast<DU>(strtol(idiom.c_str(), &p, base));
#else
        DU n = static_cast<DU>(strtol(idiom.c_str(), &p, base));
#endif
        //printf("%d\n", n);
        if (*p != '\0') {                           /// * not number
            fout << idiom << "? " << ENDL;          ///> display error prompt
            compile = false;                        ///> reset to interpreter mode
            break;                                  ///> skip the entire input buffer
        }
        // is a number
        if (compile) {                              /// * a number in compile mode?
            add_w(DOLIT);                           ///> add to current word
            add_du(n);
        }
        else PUSH(n);                               ///> or, add value onto data stack
    }
    if (!compile) ss_dump();   /// * dump stack and display ok prompt
}

#if !_WIN32 && !_WIN64 && !ARDUINO
#include <iostream>
/// main program
int main(int ac, char* av[]) {
    istringstream forth_in;
    ostringstream forth_out;
    string cmd;

    ForthVM *vm = new ForthVM(forth_in, forth_out);     // create FVM instance
    vm->init();                                         // initialize dictionary

    cout << APP_NAME << " " << MAJOR_VERSION << "." << MINOR_VERSION << ENDL;
    while (getline(cin, cmd)) {                         // fetch user input
        //printf("cmd=<%s>\n", line.c_str());
        forth_in.clear();                               // clear any input stream error bit
        forth_in.str(cmd);                              // send command to FVM
        vm->outer();                                    // execute outer interpreter
        cout << forth_out.str();                        // send VM result to output
        forth_out.str(string());                        // clear output buffer
    }
    cout << "done!" << ENDL;
    return 0;
}
#endif // !_WIN32 && !_WIN64 && !ARDUINO
