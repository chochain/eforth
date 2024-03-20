///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
///
#include <iostream>      // cin, cout
#include <fstream>       // ifstream

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
///> include external Forth script
///
void forth_include(const char *fn) {
    auto send_to_con = [](int len, const char *rst) { cout << rst; };
    string   cmd;
    ifstream ifile(fn);                 ///> open input stream
    while (getline(ifile, cmd)) {
        forth_vm(cmd.c_str(), send_to_con);
    }
    ifile.close();
}
///====================================================================
///
/// main program - Note: Arduino and ESP32 is have their own main-loop
///
int main(int ac, char* av[]) {
    dict_compile();                            ///> initialize dictionary
    mem_stat();

    auto send_to_con = [](int len, const char *rst) { cout << rst; };
    string cmd;
    while (getline(cin, cmd)) {                ///> fetch user input
        // printf("cmd=<%s>\n", cmd.c_str());
        forth_vm(cmd.c_str(), send_to_con);    ///> execute outer interpreter
    }
    cout << "Done!" << endl;
    return 0;
}
///====================================================================
