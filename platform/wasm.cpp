///
/// @file
/// @brief eForth platform for WASM
///
#include <stdio.h>
#include <emscripten.h>

extern void forth_init();
extern void forth_vm(const char *cmd, void(*)(int, const char*)=NULL);

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
extern "C" {
    void forth(int n, char *cmd) { forth_vm(cmd); }
}
