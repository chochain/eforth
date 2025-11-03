///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
///
#include <fcntl.h>         // O_NONBLOCK
#include <error.h>         // EAGAIN, EWOUDLBLOCK
#include <unistd.h>        // read (low-level)
#include <cstdint>         // U64
#include <cstdio>          // standard IO
#include <string>

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif _WIN32 || _WIN64
#include <windows.h>
#else // Linux || Cygwin
#include <sys/sysinfo.h>   // memory info
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
    fprintf(stdout, "%s", APP_VERSION);

#ifdef __APPLE__
    int64_t memsize;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, NULL, 0) == 0) {
        fprintf(stdout, ", RAM %ld MB", memsize >> 20);
    }
    
#elif _WIN32 || _WIN64
    MEMORYSTATUSEX si;                        ///< Windows Memory Status
    si.dwLength = sizeof(si);                 /// * Initialize the structure

    if (GlobalMemoryStatusEx(&si)) {          /// * fetch from system
        int p = si.dwMemoryLoad;              /// * percentage of memory in use
        U64 f = (U64)si.ullAvailPhys;         /// * available physical memory
        U64 t = (U64)si.ullTotalPhys;         /// * total physical memory
        fprintf(stdout,
            ", RAM %d%% free (%ld / %ld MB)",
            100 - p, static_cast<long>(f >> 20), static_cast<long>(t >> 20));
    }
    else fprintf(stderr, "ERR: Windows memory status fetch failed!");
    
#else // Linux, Cygwin
    struct sysinfo si;
    if (sysinfo(&si) != -1) {
        U64 f = (U64)si.freeram * si.mem_unit;
        U64 t = (U64)si.totalram * si.mem_unit;
        U64 p = f * 1000L / t;
        fprintf(stdout, 
            ", RAM %.1f%% free (%ld / %ld MB)",
            static_cast<float>(p * 0.1),
            static_cast<long>(f >> 20), static_cast<long>(t >> 20));
    }
    
#endif
    fprintf(stdout, "\n");
}
///
///> include external Forth script
///
int getline_async(const int& fno, string& cmd, char delim='\n') {
    cmd = "";
    int n = 1;
    while (n > 0) {
        char buf[2] = { 0 };
        n = (int)read(fno, buf, 1);                      /// * can return -1
        if (n) {
            if (*buf == delim) return 1;                 /// * EOL
            cmd.append(buf);                             /// * expend string
        } else {
            n = errno==EAGAIN || errno==EWOULDBLOCK;     /// * reverted back to blocking
            if (!n) break;                               /// * bail
        }
    }
    return n;
}

void outer(FILE *fp) {
    int fno = fileno(fp);                               ///< capture file number
    auto noblock = [fno]() {                            ///< set input to non-blocking
        int flags = fcntl(fno, F_GETFL, 0);
        fcntl(fno, F_SETFL, flags | O_NONBLOCK);
    };
    int    stop = 0;
    string cmd;
    while (!stop) {
        int n = getline_async(fno, cmd);
        if (n < 0) { noblock(); n = 0; }               /// * handle input error
        stop = forth_vm(n ? cmd.c_str() : nullptr);    /// * send cmd to Forth VM
        fflush(stdout);                                /// * flush output buffer before wait
    }
}

void forth_include(const char *fn) {
    FILE *fp = fopen(fn, "r");

    if (fp) {
        outer(fp);
        fclose(fp);
    }
    else fprintf(stderr, "failed to open file %s\n", fn);
}

///====================================================================
///
/// main program - Note: Arduino and ESP32 have their own main-loop
///
#include <cstdlib>                            /// srand
#include <ctime>                              /// time
#include <iostream>                           /// stdio
int main(int ac, char* av[]) {
    std::ios_base::sync_with_stdio(true);     /// * sync C++ iostream with C stdio
    forth_init();                             /// * initialize dictionary

    mem_stat();                               /// * show memory status
    srand((int)time(0));                      /// * seed random generator

    outer(stdin);                             /// * Forth outer interpreter (non-blocking input)

    forth_teardown();                         /// * clean up before we go
    fprintf(stdout, "%s Done!\n", APP_VERSION);
    
    return 0;
}
///====================================================================
