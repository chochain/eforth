///
/// @file
/// @brief eForth - System dependent functions (C-Style Optimization)
///
#include <cstdio>                             /// snprintf, sprintf, printf
#include <cstring>                            /// strlen, strcmp, strchr
#include <cstdarg>                            /// va_list, va_start, va_end
#include "ceforth.h"

// ==================== STREAM REPLACEMENTS ====================
static const char *fpi = nullptr;             ///< Replaces istringstream (Tracks remaining input)
static char *fpo = nullptr;                   ///< Cursor for continuous appending
static char obuf[E4_OBUF_SZ];                 ///< Replaces ostringstream (Scratch formatting buffer)

static int  _e4_base  = 10;                   /// Replaces <iomanip> formatting parameters state machine
static char _e4_fill  = ' ';

void (*fout_cb)(int, const char*) = nullptr;  ///< forth output callback function

/// Clear and flush the custom output buffer layout straight down to the callback
static void fout_flush() {
    if (fpo == obuf) return;
    
    *fpo = '\0';                              /// Null-terminate
    if (fout_cb) fout_cb(strlen(obuf), obuf); /// callback
    fpo = obuf;                               /// Reset pointer position
}

/// Appends formatted data onto our string block safely
static void fout(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int sz0 = E4_OBUF_SZ - (fpo - obuf) - 1;
    if (sz0 > 0) {
        int sz = vsnprintf(fpo, sz0, fmt, args);
        if (sz > 0) fpo += (sz < sz0) ? sz : sz0;
    }
    va_end(args);
}

/// Fixed standalone static helper for radix operations
static const char* _radix_helper(DU v, int b, char* buf, int buf_sz) {
    int i   = buf_sz - 1;
    buf[i]  = '\0';
    
    int dec = (b == 10);
    U32 n   = dec ? UINT(v < 0 ? -v : v) : UINT(v);

    do {
        U8 d = (U8)(n % b);
        n /= b;
        buf[--i] = (d > 9) ? ((d - 10) + 'a') : (d + '0');
    } while (n && i > 0);
    
    if (dec && v < 0 && i > 0) buf[--i] = '-';

    return &buf[i];
}
/// =============================================================
extern List<Code*> dict;                   ///< dictionary
extern List<U8>    pmem;                   ///< parameter memory (for colon definitions)
extern U8          *MEM0;                  ///< base of parameter memory block

#define TOS       (vm.tos)                 /**< Top of stack                            */
#define SS        (vm.ss)                  /**< parameter stack (per task)              */
#define RS        (vm.rs)                  /**< return stack (per task)                 */
#define MEM(a)    (MEM0 + (IU)UINT(a))     /**< pointer to address fetched from pmem    */
#define TONAME(w) (dict[w]->pfa - STRLEN(dict[w]->name))

///====================================================================
///
///> IO functions
///
void fin_setup(const char *line) {
    obuf[0] = '\0';                   /// * clean output buffer safely
    fpo = obuf;
    fpi = line;                           /// * reload pointer reference directly
}

void fout_setup(void (*hook)(int, const char*)) {
    auto cb = [](int, const char *rst) { printf("%s", rst); };
    fout_cb = hook ? hook : cb;           ///< serial output hook up
}

const char *scan(char c) {
    static char pad[E4_IBUF_SZ];          /// Fixed: Added explicit literal string buffer sizes
    if (!fpi || *fpi == '\0') { pad[0] = '\0'; return pad; }

    const char *next = strchr(fpi, c);
    if (next) {
        size_t len = next - fpi;
        if (len >= sizeof(pad)) len = sizeof(pad) - 1;
        strncpy(pad, fpi, len);
        pad[len] = '\0';
        fpi = next + 1; 
    }
    else {
        strncpy(pad, fpi, sizeof(pad) - 1);
        pad[sizeof(pad) - 1] = '\0';
        fpi += strlen(fpi); 
    }
    return pad;
}

int fetch(char *idiom, int max) {
    if (!fpi) return 0;
    
    while (*fpi == ' ' || *fpi == '\t' || *fpi == '\r' || *fpi == '\n') {
        fpi++;
    }
    if (*fpi == '\0') return 0;

    int idx = 0;
    while (*fpi != '\0' && *fpi != ' ' && *fpi != '\t' && 
           *fpi != '\r' && *fpi != '\n' && idx < max - 1) {
        idiom[idx++] = *fpi++;
    }
    idiom[idx] = '\0';
    return (idx > 0);
}

const char *word() {                     ///< get next idiom
    static char tmp[64];                 /// Fixed: Set explicit bounds to ensure stable compilation
    if (!fetch(tmp, sizeof(tmp))) tmp[0] = '\0';
    return tmp;
}

char key() { return word()[0]; }
void spaces(int n) { for (int i = 0; i < n; i++) fout(" "); }
void dot(io_op op, DU v) {
    switch (op) {
    case RDX:   _e4_base = UINT(v);                        break;
    case CR:    fout("\n"); fout_flush();                  break; 
    case DOT:   fout("%d ", (int)v);                       break;
    case UDOT:  fout("%u ", static_cast<U32>(v));          break;
    case EMIT:  { char b = (char)UINT(v); fout("%c", b); } break;
    case SPCS:  spaces(UINT(v));                           break;
    default:    fout("unknown io_op=%d\n", op);            break;
    }
}

void dotr(int w, DU v, int b, bool u) {
    char buf[66]; // Large enough to hold a 64-bit binary string + null terminator
    char *fmt = (char*)_radix_helper(v, b, buf, sizeof(buf));
    int  len  = (int)strlen(fmt);
    if (w > len) {
        int spcs = w - len;
        for (int i = 0; i < spcs; i++) { // padding
            fout("%c", (_e4_fill == '0') ? '0' : ' ');
        }
    }
    fout("%s", fmt);
    _e4_fill = ' ';     /// Reset layout alignment states to safe defaults for subsequent print jobs
}

void pstr(const char *str, io_op op) {
    fout("%s", str);
    if (op == CR) { fout("\n"); fout_flush(); }
}

///====================================================================
///
///> Debug functions
///
int pfa2didx(IU ix) {                          ///> reverse lookup
    if (IS_PRIM(ix)) return (int)ix;           ///> primitives
    IU pfa = ix & ~EXT_FLAG;                   ///< pfa (mask colon word)
    for (int i = dict.idx - 1; i > 0; --i) {
        Code *c = dict[i];
        if (pfa == (c->is_udf() ? c->pfa : c->xtoff())) return i;
    }
    return 0;                                  /// * not found
}

int  pfa2nvar(IU pfa) {
    IU  w  = *(IU*)MEM(pfa);
    if (w != VAR && w != VBRAN) return 0;
    
    IU  i0 = pfa2didx(pfa | EXT_FLAG);
    if (!i0) return 0;
    IU  p1 = (i0+1) < dict.idx ? TONAME(i0+1) : pmem.idx;
    int n  = p1 - pfa - sizeof(IU) * (w==VAR ? 1 : 2);    ///> CC: calc # of elements
    return n;
}

void to_s(IU w, U8 *ip, int base) {
#if CC_DEBUG
    fout("( %04x[%4d] ) ", (unsigned int)(ip - MEM0), w);
#endif // CC_DEBUG
    
    ip += sizeof(IU);                   ///> calculate next ip
    switch (w) {
    case LIT:  fout("%d ( lit )", (int)*(DU*)ip); break;
    case STR:  fout("s\" %s\"", (char*)ip);       break;
    case DOTQ: fout(".\" %s\"", (char*)ip);       break;
    case VAR:
    case VBRAN: {
        int n  = pfa2nvar(UINT(ip - MEM0 - sizeof(IU)));
        IU  ix = (IU)(ip - MEM0 + (w==VAR ? 0 : sizeof(IU)));
        for (int i = 0, a=DALIGN(ix); i < n; i+=sizeof(DU)) {
            fout("%d ", (int)*(DU*)MEM(a + i));
        }
    }                                   /// no break, fall through
    default: fout("%s", dict[w]->name);           break;
    }
    switch (w) {
    case NEXT: case LOOP:
    case BRAN: case ZBRAN: case VBRAN:  ///> display jmp target
        fout(" $%04x", *(IU*)ip);
        break;
    default: /* do nothing */ break;
    }
    _e4_fill = ' ';
}

void see(IU pfa, int base) {
    U8 *ip = MEM(pfa);                  ///< memory pointer
    int i=0;
    while (1) {
        IU w = pfa2didx(*(IU*)ip);      ///< fetch word index by pfa
        if (!w) break;                  ///> loop guard
        
        fout("\n  ");                   /// * indent
        to_s(w, ip, base);              /// * display opcode
        if (w==EXIT || w==VAR) break;   /// * end of word

        ip += sizeof(IU);               ///> advance ip (next opcode)
        switch (w) {                    ///> extra bytes to skip
        case LIT:   ip += sizeof(DU);                    break; 
        case STR:   case DOTQ:  ip += STRLEN((char*)ip); break;
        case BRAN:  case ZBRAN:
        case NEXT:  case LOOP:  ip += sizeof(IU);        break;
        case VBRAN: ip = MEM(*(IU*)ip);                  break;
        }
        fout("( %04x[%4d] ) ", (unsigned int)(ip - MEM0), w);
        break;
    }
    fout_flush();
}

void words(int base) {
    const int WIDTH = 56;
    int sz = 0;
    for (int i=0; i<dict.idx; i++) {
        const char *nm = dict[i]->name;
        const int  len = strlen(nm);
#if CC_DEBUG > 1
        if (nm[0]) {
#else  //  CC_DEBUG > 1
        if (nm[len-1] != ' ') {
#endif // CC_DEBUG > 1
            sz += len + 2;
            fout("  %s", nm);
        }
        if (sz > WIDTH) {
            sz = 0;
            fout("\n");
            yield();
        }
    }
    fout("\n");
    fout_flush();
}

static int load_dp = 0;
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

void ss_dump(VM &vm, bool forced) {
    if (load_dp) return;    /// * skip when including file
    static char buf[64];    /// Fixed: Added explicit character buffer sizing
    SS.push(TOS);
    for (int i=0; i<SS.idx; i++) {
        const char* fmt_val = _radix_helper(SS[i], *MEM(vm.base), buf, sizeof(buf));
        fout("%s ", fmt_val);
    }
    TOS = SS.pop();
    fout("ok\n");
    fout_flush();
}
void mem_dump(U32 p0, IU sz, int base) {
    for (IU i=p0 & ~15; i<=(p0+sz); i+=16) {
        fout("%04x: ", i);
        for (int j=0; j<16; j++) {
            U8 c = pmem[i+j];
            fout("%02x%s", (int)c, (j % 4 == 3 ? " " : ""));
        }
        for (int j=0; j<16; j++) {
            U8 c = pmem[i+j] & 0x7f;
            fout("%c", ((c==0x7f||c<0x20) ? '_' : c));
        }
        fout("\n");
        yield();
    }
    fout_flush();
}

void dict_dump(int base) {
    printf("XT0=%08x\n", (U32)Code::XT0);
    for (int i=0; i<dict.idx; i++) {
        Code *c = dict[i];
        printf("%03d> name=%-8s, xt=%p, attr=%x, xtoff=%04x %s\n",
               i, c->name, c->xt, (c->attr & 0x3),
               (c->is_udf() ? c->pfa : c->xtoff()), c->name);
    }
}
///====================================================================
///
///> LVGL / Native Web Formatter API
///
#if DO_WASM
extern "C" { void js_call(const char *ops); }
void native_api(VM &vm) {                  ///> ( n addr u -- )
    POP();                                 /// * strlen, not used
    char fmt_template = (char)MEM(POP());
    char pad[512];                         /// Fixed: Correctly sized buffer for parameter translation
    strncpy(pad, fmt_template, sizeof(pad)-1);
    pad[sizeof(pad)-1] = '\0';
    for (int i = (int)strlen(pad) - 2; i >= 0; i--) {
        if (pad[i] == '%') {
            char type_char = pad[i + 1];
            char tmp[256] = {0};           /// Fixed: Clean character array allocation block
            switch (type_char) {
            case 'd': snprintf(tmp, sizeof(tmp), "%d", (int)UINT(POP())); break;
            case 'f': snprintf(tmp, sizeof(tmp), "%g", (double)(DU)POP()); break;
            case 'x': snprintf(tmp, sizeof(tmp), "0x%x", (unsigned int)UINT(POP())); break;
            case 's': {POP();snprintf(tmp, sizeof(tmp), "%s", (char*)MEM(POP()));} break;
            case 'p': {
                unsigned int len = (unsigned int)UINT(POP());
                unsigned int addr = (unsigned int)UINT(POP());
                snprintf(tmp, sizeof(tmp), "p %u %u", addr, len);
            } break;
            case '%':
                tmp[0] = '%';
                tmp[1] = '\0';
                memmove(&pad[i+1], &pad[i+2], strlen(&pad[i+2])+1);
                break;
            default:
                snprintf(tmp, sizeof(tmp), "%c?", type_char); break;
            }
            size_t orig_len = strlen(pad);
            size_t insert_len = strlen(tmp);
            if (orig_len - 2 + insert_len < sizeof(pad) - 1) {
                memmove(&pad[i + insert_len], &pad[i + 2], strlen(&pad[i + 2]) + 1);
                memcpy(&pad[i], tmp, insert_len);
            }
        }
    }
    js_call(pad);
}
#endif // DO_WASM

