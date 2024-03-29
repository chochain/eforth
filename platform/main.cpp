///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
///
#include <iostream>      // cin, cout
#include <fstream>       // ifstream
#include <sys/sysinfo.h>
using namespace std;

extern void forth_init();
extern void forth_vm(const char *cmd, void(*callback)(int, const char*));

const char* APP_VERSION = "eForth v4.2";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat() {
	cout << APP_VERSION;
	struct sysinfo si;
	if (sysinfo(&si)!=-1) {
        int64_t p = 1000L * si.freeram / si.totalram;
		cout << ", RAM "   << static_cast<float>(p) * 0.1
             << "% free (" << (si.freeram>>20)
             << " / "      << (si.totalram>>20) << " MB)";
	}
	cout << endl;
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
    forth_init();                       ///> initialize dictionary
	mem_stat();                         ///> show memory status
    
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
