///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
///
/// version info
///
#define APP_NAME         "eForth"
#define MAJOR_VERSION    "8"
#define MINOR_VERSION    "6"

const char *vm_version(){
    static string ver = string(APP_NAME) + " " + MAJOR_VERSION + "." + MINOR_VERSION;
    return ver.c_str();
}
void mem_stat() {
	LOGS("memory blocks:");
    LOG_KX("\n  mm.max= 0x", E4_PMEM_SZ);
    LOG_KX("\n  here  = 0x", HERE);
    LOG_KX("\n  avail = 0x", E4_PMEM_SZ - HERE);
    LOG_KV("\n  ss.sz = ",   E4_SS_SZ);
    LOG_KV("\n  ss.idx= ",   ss.idx);
    LOG_KV("\n  ss.max= ",   ss.max);
    LOG_KV("\n  rs.sz = ",   E4_RS_SZ);
    LOG_KV("\n  rs.idx= ",   rs.idx);
    LOG_KV("\n  rs.max= ",   rs.max);
    LOGS("\n");
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

    cout << vm_version() << endl;
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
