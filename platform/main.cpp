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
#include <ctime>         // time
#include <thread>        // this_thread
#include <chrono>        // sleep_for

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif _WIN32 || _WIN64
#include <windows.h>
#else // Linux || Cygwin
#include <sys/sysinfo.h>
#endif

using namespace std;
///
/// Note: declared here, not by including ceforth.h, so that the macros in
///       config.h (MAX, ABS, LOG, ERR, ...) stay out of the system headers above.
///       Keep these in step with the definitions in ceforth.cpp.
///
extern void forth_init();
extern int  forth_vm(const char *cmd, void(*)(int, const char*)=NULL);
extern void forth_teardown();
extern bool io_rx_ready();
extern int  io_rx_get();
extern bool io_rx_eof();
extern bool is_forth_idle();

const char* APP_VERSION = "eForth v6.0";
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
///> Forth outer loop: one line at a time into the shell VM (vm 0)
///
/// Note: forth_vm keeps a pointer to the line (no copy), so 'cmd' must stay
///       alive, unchanged, for the whole call. It does here.
///       Cooperative tasks (USE_PTHREAD=0) only advance while the shell runs
///       'pause' or waits in 'rx@'; nothing schedules them between lines.
///
/// @return true if the session ended (bye), false on end of input
///
static bool read_line(string &line) {       ///< non-blocking, assembles one line
    while (io_rx_ready()) {
        int c = io_rx_get();
        if (c == '\n') return true;
        if (c != '\r') line += (char)c;
    }
    return false;
}

bool outer_poll() {
    string line;
    int drain = 1000;                            ///< rounds allowed after end of input
    for (;;) {
        if (read_line(line)) {
            bool bye = forth_vm(line.c_str());   ///< line must outlive this call (tib points into it)
            line.clear();
            if (bye) return true;
        }
        else if (io_rx_eof()) {
            if (!line.empty() && forth_vm(line.c_str())) return true;   /// last line, no newline
            while (drain-- > 0 && is_forth_idle());                     /// let finite tasks finish (piped scripts)
            return false;
        }
        else if (!is_forth_idle()) {           /// nothing ran: don't spin the CPU
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }
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
    string cmd;                                ///< input command; TODO: static pool
    while (getline(ifile, cmd)) {              ///> fetch user input
        // printf("cmd=<%s>\n", cmd.c_str());
        if (forth_vm(cmd.c_str())) return true;     ///> outer interpreter, if STOP
    }
    return false;                              ///< no STOP
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
    if (!bye) outer_poll();                   ///> Forth outer interpreter

    forth_teardown();                         ///> clean up before we go
    cout << APP_VERSION << " Done!" << endl;
    return 0;
}
///====================================================================
