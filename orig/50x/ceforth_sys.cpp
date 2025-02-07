///
/// @file
/// @brief eForth - System dependent functions
///
///====================================================================
///
/// utilize C++ standard template libraries for core IO functions only
/// Note:
///   * we use STL for its convinence, but
///   * if it takes too much memory for target MCU,
///   * these functions can be replaced with our own implementation
///
#include <iomanip>                         /// setbase, setw, setfill
#include <sstream>                         /// iostream, stringstream
#include "ceforth.h"

istringstream     fin;                     ///< forth_in
ostringstream     fout;                    ///< forth_out
void (*fout_cb)(int, const char*);         ///< forth output callback function (see ENDL macro)
int    load_dp    = 0;
///
///@name Primitive words to help printing
///@{
Code prim[] = {
    Code(";",    EXIT),  Code("next", NEXT),  Code("loop", LOOP),  Code("lit", LIT),
    Code("var",  VAR),   Code("str",  STR),   Code("dotq", DOTQ),  Code("bran",BRAN),
    Code("0bran",ZBRAN), Code("for",  FOR),   Code("do",   DO),    Code("key", KEY)
};
///@}
extern List<Code> dict;                    ///< dictionary
extern List<U8>   pmem;                    ///< parameter memory (for colon definitions)
extern U8         *MEM0;                   ///< base of parameter memory block

#define TOS       (vm.tos)                 /**< Top of stack                            */
#define SS        (vm.ss)                  /**< parameter stack (per task)              */
#define RS        (vm.rs)                  /**< return stack (per task)                 */
#define MEM(a)    (MEM0 + (IU)UINT(a))     /**< pointer to address fetched from pmem    */
#define IS_PRIM(w) (!IS_UDF(w) && (w < MAX_OP))
#define TONAME(w) (dict[w].pfa - STRLEN(dict[w].name))

///====================================================================
///
///> IO functions
///
void fin_setup(const char *line) {
    fout.str("");                        /// * clean output buffer
    fin.clear();                         /// * clear input stream error bit if any
    fin.str(line);                       /// * reload user command into input stream
}
void fout_setup(void (*hook)(int, const char*)) {
    auto cb = [](int, const char *rst) { printf("%s", rst); };
    fout_cb = hook ? hook : cb;          ///< serial output hook up
}
char *scan(char c) {
    static string pad;                   ///< temp storage
    getline(fin, pad, c);                ///< scan fin for char c
    return (char*)pad.c_str();           ///< return found string
}
int  fetch(string &idiom) { return !(fin >> idiom)==0; }
char *word() {                           ///< get next idiom
    static string tmp;                   ///< temp string holder
    if (!fetch(tmp)) tmp.clear();        /// * input buffer exhausted?
    return (char*)tmp.c_str();
}
char key() { return word()[0]; }
void load(VM &vm, const char* fn) {
    load_dp++;                           /// * increment depth counter
    RS.push(vm.ip);                      /// * save context
    RS.push(vm.state);
    vm.state = NEST;                     /// * +recursive
    forth_include(fn);                   /// * include file
    vm.state = static_cast<vm_state>(RS.pop());
    vm.ip   = UINT(RS.pop());            /// * context restored
    --load_dp;                           /// * decrement depth counter
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
int pfa2didx(IU ix) {                          ///> reverse lookup
/*    
    if (IS_PRIM(ix)) return (int)ix;           ///> primitives
    IU pfa = ix & ~EXT_FLAG;                   ///< pfa (mask colon word)
    for (int i = dict.idx - 1; i > 0; --i) {
        Code &c = dict[i];
        if (pfa == (IS_UDF(i) ? c.pfa : c.xtoff())) return i;
    }
*/
    return 0;                                  /// * not found
}
int  pfa2nvar(IU pfa) {
    IU  w  = *(IU*)MEM(pfa);
    if (w != VAR) return 0;
/*    
    IU  i0 = pfa2didx(pfa | EXT_FLAG);
    if (!i0) return 0;
    IU  p1 = (i0+1) < dict.idx ? TONAME(i0+1) : pmem.idx;
    int n  = p1 - pfa - sizeof(IU) * (w==VAR ? 1 : 2);    ///> CC: calc # of elements
    return n;
*/
    return 0;
}
void to_s(IU w, U8 *ip, int base) {
#if CC_DEBUG
    fout << setbase(16) << "( ";
    fout << setfill('0') << setw(4) << (ip - MEM0);       ///> addr
    fout << '[' << setfill(' ') << setw(4) << w << ']';   ///> word ref
    fout << " ) " << setbase(base);
#endif // CC_DEBUG
    
    ip += sizeof(IU);                   ///> calculate next ip
    switch (w) {
    case LIT:  fout << ip  << " ( lit )";      break;
    case STR:  fout << "s\" " << (char*)ip << '"';  break;
    case DOTQ: fout << ".\" " << (char*)ip << '"';  break;
    case VAR: {
        int n  = pfa2nvar(UINT(ip - MEM0 - sizeof(IU)));
        IU  ix = (IU)(ip - MEM0 + (w==VAR ? 0 : sizeof(IU)));
        for (int i = 0, a=DALIGN(ix); i < n; i+=sizeof(DU)) {
            fout << *(DU*)MEM(a + i) << ' ';
        }
    }                                   /// no break, fall through
    default:
        Code &c = IS_PRIM(w) ? prim[w] : dict[w];
        fout << c.name; break;
    }
    switch (w) {
    case NEXT: case LOOP:
    case BRAN: case ZBRAN:              ///> display jmp target
        fout << " $" << setbase(16)
             << setfill('0') << setw(4) << *(IU*)ip;
        break;
    default: /* do nothing */ break;
    }
    fout << setfill(' ') << setw(-1);   ///> restore output format settings
}
void see(IU pfa, int base) {
    U8 *ip = MEM(pfa);                  ///< memory pointer
    while (1) {
        IU w = pfa2didx(*(IU*)ip);      ///< fetch word index by pfa
        if (!w) break;                  ///> loop guard
        
        fout << ENDL; fout << "  ";     /// * indent
        to_s(w, ip, base);              /// * display opcode
        if (w==EXIT || w==VAR) return;  /// * end of word
        
        ip += sizeof(IU);               ///> advance ip (next opcode)
        switch (w) {                    ///> extra bytes to skip
        case LIT:   ip += sizeof(DU);                    break; /// alignment?
        case STR:   case DOTQ:  ip += STRLEN((char*)ip); break;
        case BRAN:  case ZBRAN:
        case NEXT:  case LOOP:  ip += sizeof(IU);        break;
        }
    }
}
void words(int base) {
    const int WIDTH = 60;
    int sz = 0;
    fout << setbase(10);
    for (int i=0; i<dict.idx; i++) {
        const char *nm = dict[i].name;
        const int  len = strlen(nm);
#if CC_DEBUG > 1
        if (nm[0]) {
#else  //  CC_DEBUG > 1
        if (nm[len-1] != ' ') {
#endif // CC_DEBUG > 1
            sz += len + 2;
            fout << "  " << nm;
        }
        if (sz > WIDTH) {
            sz = 0;
            fout << ENDL;
            yield();
        }
    }
    fout << setbase(base) << ENDL;
}
void ss_dump(VM &vm, bool forced) {
    if (load_dp) return;                  /// * skip when including file
#if DO_WASM    
    if (!forced) { fout << "ok" << ENDL; return; }
#endif // DO_WASM
    static char buf[34];                  ///< static buffer
    auto rdx = [](DU v, int b) {          ///< display v by radix
#if USE_FLOAT
        DU t, f = modf(v, &t);            ///< integral, fraction
        if (ABS(f) > DU_EPS) {
            sprintf(buf, "%0.6g", v);
            return buf;
        }
#endif // USE_FLOAT
        int i = 33;  buf[i]='\0';         /// * C++ can do only base=8,10,16
        int dec = b==10;
        U32 n   = dec ? UINT(ABS(v)) : UINT(v);  ///< handle negative
        do {                              ///> digit-by-digit
            U8 d = (U8)MOD(n,b);  n /= b;
            buf[--i] = d > 9 ? (d-10)+'a' : d+'0';
        } while (n && i);
        if (dec && v < DU0) buf[--i]='-';
        return &buf[i];
    };
    SS.push(TOS);
    for (int i=0; i<SS.idx; i++) {
        fout << rdx(SS[i], *MEM(vm.base)) << ' ';
    }
    TOS = SS.pop();
    fout << "-> ok" << ENDL;
}
void mem_dump(U32 p0, IU sz, int base) {
    fout << setbase(16) << setfill('0');
    for (IU i=ALIGN16(p0); i<=ALIGN16(p0+sz); i+=16) {
        fout << setw(4) << i << ": ";
        for (int j=0; j<16; j++) {
            U8 c = pmem[i+j];
            fout << setw(2) << (int)c << (MOD(j,4)==3 ? " " : "");
        }
        for (int j=0; j<16; j++) {   // print and advance to next byte
            U8 c = pmem[i+j] & 0x7f;
            fout << (char)((c==0x7f||c<0x20) ? '_' : c);
        }
        fout << ENDL;
        yield();
    }
    fout << setbase(base) << setfill(' ');
}
///====================================================================
///
///> System statistics - for heap, stack, external memory debugging
///
void dict_dump(int base) {
    fout << setbase(16) << setfill('0') << "XT0=" << Code::XT0 << ENDL;
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        fout << setfill('0') << setw(3) << i
             << (IS_UDF(i) ? " U" : "  ")
			 << (IS_IMM(i) ? "I " : "  ")
             << setw(8) << ((UFP)c.xt & MSK_XT)
             << ":" << setw(6) << c.ip()
             << " " << c.name << ENDL;
    }
    fout << setbase(base) << setfill(' ') << setw(-1);
}
///====================================================================
///
///> Javascript/WASM interface
///
#if DO_WASM
#define POP() ({ DU n=TOS; TOS=SS.pop(); n; })
EM_JS(void, js_call, (const char *ops), {
        const req = UTF8ToString(ops).split(/\\s+/);
        const wa  = wasmExports;
        const mem = wa.vm_mem();
        let msg = [], tfr = [];
        for (let i=0, n=req.length; i < n; i++) {
            if (req[i]=='p') {
                const a = new Float32Array(     ///< create a buffer ref
                    wa.memory.buffer,           /// * WASM ArrayBuffer
                    mem + (req[i+1]|0),         /// * pointer address
                    req[i+2]|0                  /// * length
                );
                i += 2;                         /// *  skip over addr, len
                const t = new Float64Array(a);  ///< create a transferable
                msg.push(t);                    /// * which speeds postMessage
                tfr.push(t.buffer);             /// * from 20ms => 5ms
            }
            else msg.push(req[i]);
        }
        msg.push(Date.now());                   /// * t0 anchor for performance check
        postMessage(['js', msg], tfr);
});
///
///> Javascript calling, before passing to js_call()
///
///  String substitude similar to printf
///    %d - integer
///    %f - float
///    %x - hex
///    %s - string
///    %p - pointer (memory block)
///
void native_api(VM &vm) {                  ///> ( n addr u -- )
    static stringstream n;                 ///< string processor
    static string       pad;               ///< tmp storage
    auto t2s = [&vm](char c) {             ///< template to string
        n.str("");                         /// * clear stream
        switch (c) {
        case 'd': n << UINT(POP());                break;
        case 'f': n << (DU)POP();                  break;
        case 'x': n << "0x" << hex << UINT(POP()); break;
        case 's': POP(); n << (char*)MEM(POP());   break;  /// also handles raw stream
        case 'p':
            n << "p " << UINT(POP());
            n << ' '  << UINT(POP());              break;
        default : n << c << '?';                   break;
        }
        return n.str();
    };
    POP();                                 /// * strlen, not used
    pad.clear();                           /// * init pad
    pad.append((char*)MEM(POP()));         /// * copy string on stack
    for (size_t i=pad.find_last_of('%');   ///> find % from back
         i!=string::npos;                  /// * until not found
         i=pad.find_last_of('%',i?i-1:0)) {
        if (i && pad[i-1]=='%') {          /// * double %%
            pad.replace(--i,1,"");         /// * drop one %
        }
        else pad.replace(i, 2, t2s(pad[i+1]));
    }
    js_call(pad.c_str());    /// * call Emscripten js function
}
#endif // DO_WASM
