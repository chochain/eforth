///
/// @file
/// @brief eForth platform for WASM
///
const char* APP_VERSION = "eForth v8.6";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat() {
    LOGS(APP_VERSION);
    LOG_KV("\n  dict: ",   dict.idx); LOG_KV("/", E4_DICT_SZ);
    LOG_KV(", ",           sizeof(Code));  LOGS("-byte/entry");
    LOG_KV("\n  ss  : ",   ss.idx);   LOG_KV("/", E4_SS_SZ);
    LOG_KV(" (max ",       ss.max);   LOGS(")");
    LOG_KV("\n  rs  : ",   rs.idx);   LOG_KV("/", E4_RS_SZ);
    LOG_KV(" (max ",       rs.max);   LOGS(")");
    LOG_KX("\n  here: 0x", HERE);     LOG_KX("/0x", E4_PMEM_SZ);
    LOG_KX(" (free 0x",    E4_PMEM_SZ - HERE); LOGS(")\n");
}
///====================================================================
///
/// main program
///
void run_once() {
    const char *cmd[] = {
        "dict",
        "mstat",
        "1",
        "2",
        "+",
        ": xx 123 ;",
        "words",
        "bye"
    };
    for (int i=0; i < sizeof(cmd)/sizeof(const char*); i++) {
        forth_core(cmd[i]);
    }
}

int main(int ac, char* av[]) {
    dict_compile();               // initialize dictionary
    mem_stat();

    emscripten_set_main_loop(run_once, 0, false);
    return 0;
}
///====================================================================
///
/// WASM/Emscripten ccall interfaces
///
extern "C" {
void forth(int n, char *cmd) {
    forth_core(cmd);
}
}
