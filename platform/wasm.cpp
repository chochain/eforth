///
/// @file
/// @brief eForth platform for WASM
///
#include <stdio.h>

extern void forth_init();
extern void forth_vm(const char *cmd, void(*callback)(int, const char*));

const char* APP_VERSION = "eForth v4.2";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat()                    { printf("%s\n", APP_VERSION); }
void forth_include(const char *fn) { /* not implemented */ }

///====================================================================
///
/// main program
///
int main(int ac, char* av[]) {
    forth_init();
    return 0;
}
///====================================================================
///
/// WASM/Emscripten ccall interfaces
///
#include <emscripten.h>
extern "C" {
void forth(int n, char *cmd) {
    auto rsp_to_con = [](int len, const char *rst) { printf("%s", rst); };
    forth_vm(cmd, rsp_to_con);
}
}
