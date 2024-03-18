///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
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
///
/// ForthVM outer interpreter
///
void vm_outer(const char *cmd, void(*callback)(int, const char*)) {
    fin.clear();               ///> clear input stream error bit if any
    fin.str(cmd);              ///> feed user command into input stream
    fout_cb = callback;        ///> setup callback function
    fout.str("");              ///> clean output buffer, ready for next run
    while (fin >> strbuf) {    ///> outer interpreter loop
        const char *idiom = strbuf.c_str();
        forth_core(idiom);     ///> single command to Forth core
    }
    if (!compile) ss_dump();   /// * dump stack and display ok prompt
}
///====================================================================
///
/// main program - Note: Arduino and ESP32 is have their own main-loop
///
#include <iostream>                            // cin, cout
int main(int ac, char* av[]) {
    dict_compile();                            ///> initialize dictionary
    mem_stat();

    static auto send_to_con = [](int len, const char *rst) { cout << rst; };
    string cmd;
    while (getline(cin, cmd)) {                ///> fetch user input
        // printf("cmd=<%s>\n", cmd.c_str());
        vm_outer(cmd.c_str(), send_to_con);    ///> execute outer interpreter
    }
    cout << "Done!" << endl;
    return 0;
}
///====================================================================
