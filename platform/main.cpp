///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
///
/// usage: ceforth [file ...]    load each file, then enter the interactive shell
///                              (a file that executes 'bye' ends the session)
///
#include <iostream>      // cin, cout, cerr
#include <fstream>       // ifstream
#include <string>        // string, getline
#include <cstdint>
#include <cstdlib>       // srand

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif _WIN32 || _WIN64
#include <windows.h>
#include <string>
#else // Linux || Cygwin
#include <sys/sysinfo.h>
#endif

using namespace std;

extern void forth_init();
extern int  forth_vm(const char *cmd, void(*)(int, const char*)=NULL);
extern void forth_teardown();

const char* APP_VERSION = "eForth v5.0";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
typedef uint64_t U64;
void mem_stat() {
    cout << APP_VERSION;

#ifdef __APPLE__
    int64_t memsize;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, NULL, 0) == 0) {
        cout << ", RAM " << (memsize >> 20) << " MB";
    }
#elif _WIN32 || _WIN64
    MEMORYSTATUSEX si;                        ///< Windows Memory Status
    si.dwLength = sizeof(si);                 /// * Initialize the structure

    if (GlobalMemoryStatusEx(&si)) {          /// * fetch from system
        int p = si.dwMemoryLoad;              /// * percentage of memory in use
        U64 f = (U64)si.ullAvailPhys;         /// * available physical memory
        U64 t = (U64)si.ullTotalPhys;         /// * total physical memory
        cout << ", RAM " << (100 - p) << "% free (" << (f >> 20)
             << " / " << (t >> 20) << " MB)";
    }
    else cerr << "ERR: Windows memory status fetch failed!";
#else // Linux, Cygwin
    struct sysinfo si;
    if (sysinfo(&si) != -1) {
        U64 f = (U64)si.freeram * si.mem_unit;
        U64 t = (U64)si.totalram * si.mem_unit;
        U64 p = f * 1000L / t;
        cout << ", RAM " << static_cast<float>(p) * 0.1 << "% free (" << (f >> 20)
             << " / " << (t >> 20) << " MB)";
    }
#endif

    cout << endl;
}
///====================================================================
///
///> include external Forth script
///> Forth outer loop: one line at a time into the shell VM (vm 0)
///
bool outer(istream &in) {
    string cmd;                               ///< input command; TODO: static pool
    while (getline(in, cmd)) {                ///> fetch user input
        // printf("cmd=<%s>\n", cmd.c_str());
        if (forth_vm(cmd.c_str())) return true;     ///> run outer interpreter (single task)
    }
    return false;
}
///
/// @return true if the script ended the session (bye)
///
bool forth_include(const char *fn) {
    ifstream ifile(fn);                       ///< open input stream
    if (!ifile.is_open()) {
        cerr << "file " << fn << " open failed!" << endl;
        return false;
    }
    return outer(ifile);
}        
///====================================================================
///
/// main program - Note: Arduino and ESP32 have their own main-loop
///
int main(int ac, char* av[]) {
    srand((unsigned)time(0));                 ///> seed random generator
    forth_init();                             ///> initialize dictionary and VM pool

    mem_stat();                               ///> show memory status

    bool bye = false;
    for (int i = 1; i < ac && !bye; i++) {    ///> load scripts named on the command line
        bye = forth_include(av[i]);
    }
    if (!bye) outer(cin);                     ///> Forth outer interpreter

    forth_teardown();                         ///> clean up before we go
    cout << APP_VERSION << " Done!" << endl;
    return 0;
}
///====================================================================
