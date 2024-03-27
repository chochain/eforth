///
/// @file
/// @brief eForth platform for WASM
///
const char* APP_VERSION = "eForth v4.2";
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

int forth_include(const char *fn) { /* do nothing */ return 0; }
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
void forth(int n, char *cmd) {
    auto rsp_to_con = [](int len, const char *rst) { printf("%s", rst); };
    forth_vm(cmd, rsp_to_con);
}
}
