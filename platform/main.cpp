///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
///
#include <iostream>      // cin, cout
#include <fstream>       // ifstream
#include <sys/sysinfo.h>

const char* APP_VERSION = "eForth v4.2";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat() {
	struct sysinfo si;
	LOGS(APP_VERSION);
	if (sysinfo(&si)!=-1) {
		LOG_KX(", RAM free 0x", si.freeram);
		LOG_KX("/0x", si.totalram);
	}
    LOG_KV("\n  dict: ",   dict.size());
    LOG_KV(", ",           sizeof(Code)); LOGS("-byte/entry");
    LOG_KV("\n  ss  : ",   ss.size());    LOGS("\n");
	
}
///
///> include external Forth script
///
int forth_include(const char *fn) {
    auto send_to_con = [](int len, const char *rst) { cout << rst; };
    string   cmd;
    ifstream ifile(fn);                 ///> open input stream
    while (getline(ifile, cmd)) {
        forth_vm(cmd.c_str(), send_to_con);
    }
    ifile.close();
    return 0;
}
///====================================================================
///
/// main program - Note: Arduino and ESP32 have their own main-loop
///
int main(int ac, char* av[]) {
	mem_stat();
    forth_init();                              ///> initialize dictionary
    
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
