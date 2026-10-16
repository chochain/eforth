// host test harness: stubs for the I/O + user-area functions that live in other files
#include "ceforth.h"
#include <string>
#include <cstdlib>
extern U8 *MEM0;
static std::string g_in; static size_t g_pos;
static VM g_vm;
VM& vm_get(int) { return g_vm; }
void uvar_init() { IU off = pmem.idx; add_iu(10); g_vm.base = off; }   // radix cell in user area
void fin_setup(const char *l) { g_in = l; g_pos = 0; }
void fout_setup(void(*)(int,const char*)) {}
int  fetch(char *b, int mx) {
    while (g_pos < g_in.size() && isspace((unsigned char)g_in[g_pos])) g_pos++;
    if (g_pos >= g_in.size()) return 0;
    int n = 0; while (g_pos < g_in.size() && !isspace((unsigned char)g_in[g_pos]) && n < mx-1) b[n++] = g_in[g_pos++];
    b[n] = 0; return 1;
}
const char *word(char *b, int mx) { return fetch(b, mx) ? b : ""; }
const char *scan(char c, char *b, int mx) {
    int n = 0; while (g_pos < g_in.size() && g_in[g_pos] != c && n < mx-1) b[n++] = g_in[g_pos++];
    if (g_pos < g_in.size()) g_pos++;     // eat delimiter
    b[n] = 0; return b;
}
char key() { return 'K'; }
void dot(io_op op, DU v, int base) {
    if (op == CR) { printf("\n"); return; }
    if (base == 16) printf("%x ", (unsigned)v); else printf("%d ", (int)v);
}
void dotr(int, DU v, int base, bool) { dot(DOT, v, base); }
void pstr(const char *s, io_op op) { printf("%s", s); if (op == CR) printf("\n"); }
void spaces(int n) { while (n-- > 0) printf(" "); }
void see(IU, int) {} void words() {} void dict_dump() {} void mem_stat() {}
void load(VM&, const char*) {}
void ss_dump(VM &vm, bool) { }
void mem_dump(U32 a, IU n, int) { for (IU i = 0; i < n; i++) printf("%02x ", MEM0[a+i]); printf("\n"); }
int main() {
    forth_init();
    char line[512];
    while (fgets(line, sizeof line, stdin)) {
        line[strcspn(line, "\n")] = 0;
        if (!*line) continue;
        printf("> %s\n", line);
        forth_vm(line);
        printf("\n");
        VM &vm = vm_get(0); vm.state = QUERY;
    }
    VM &vm = vm_get(0);
    printf("[depth=%d rdepth=%d]\n", vm.ss.idx, vm.rs.idx);
    return 0;
}
