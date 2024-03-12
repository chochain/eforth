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
    LOG_KV("\n  ss  : ",   ss.idx);   LOG_KV("/", E4_SS_SZ);
    LOG_KV(" (max ",       ss.max);   LOGS(")");
    LOG_KV("\n  rs  : ",   rs.idx);   LOG_KV("/", E4_RS_SZ);
    LOG_KV(" (max ",       rs.max);   LOGS(")");
    LOG_KX("\n  here: 0x", HERE);     LOG_KX("/0x", E4_PMEM_SZ);
    LOG_KX(" (free 0x",    E4_PMEM_SZ - HERE); LOGS(")\n");
}
///====================================================================
///
/// main program - Note: Arduino and ESP32 is have their own main-loop
///
int main(int ac, char* av[]) {
    dict_compile();               // initialize dictionary
    dict_dump();
    mem_stat();
    
    forth_core("mstat");
    
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
int  vm_base()       { return base;     }
int  vm_ss_idx()     { return ss.idx;   }
int  vm_dict_idx()   { return dict.idx; }
DU   *vm_ss()        { return &ss[0];   }
char *vm_dict(int i) { return (char*)dict[i].name; }
char *vm_mem()       { return (char*)&pmem[0]; }
}
