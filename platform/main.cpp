///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
///
#include <cstdint>         // U64
#include <cstdio>          // standard IO

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif _WIN32 || _WIN64
#include <windows.h>
#include <string>
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
#if _WIN32 || _WIN64
#include <conio.h>         // getchar
char qkey() {
    char c = _kbhit() ? _getche() : '\0';
    switch (c) {
    case 0x8:
    case 0x7f: putchar(' ');  putchar(c); break;
    case '\r': putchar('\n'); break;
    }
    return c;
}

#else // _WIN32 || _WIN64
#include <termios.h>       // tcgetattr
#include <unistd.h>        // STDIN_FILENO

char qkey() {                                 ///< get one unbuffered char with timeout
    struct termios t0, t1;

    fflush(stdout);                           /// * flush output buffer before wait
    tcgetattr(STDIN_FILENO, &t0);             /// * backup stdin attributes
    t1 = t0;
    t1.c_lflag &= ~(ICANON | ECHO);           /// * non-buffered, and echo
    t1.c_cc[VMIN]  = 0;                       /// * capture 0 or more char
    t1.c_cc[VTIME] = 0;                       /// * 0: no wait, 1:timeout on 0.1 second (returns '\0')
    tcsetattr(STDIN_FILENO, TCSANOW, &t1);    /// * set to non-buffered

    char c;
    int n = read(STDIN_FILENO, &c, 1);        /// * fetch one char from given input file

    tcsetattr(STDIN_FILENO, TCSANOW, &t0);    /// * restore stdin attributes

    return n ? c : '\0';
}
#endif // _WIN32 || _WIN64

#define TIB_SZ 128                            /// * 128-byte line buffer
void outer(FILE *fp) {
    char cmd[TIB_SZ+1];
    int  idx  = 0;
    int  done = 0;
    int  term = fp==stdin;                    ///< input from terminal
    while (!done) {
        char c = term ? qkey() : fgetc(fp);   ///< ?key or stream from file
        //        fprintf(stderr, ".%c%x", c, c);
        switch (c) {
        case '\0':
            forth_vm(NULL);                   /// * handle timer interrupt
            break;
        case 0x8:                             /// * backspace
        case 0x7f: --idx;   break;            /// * erase
        case EOF: done = 1; break;            /// * done with input file
        case '\n': case '\r':
            cmd[idx] = '\0';
//            if (!term) fprintf(stdout, "%s\n", cmd);
            done = forth_vm(cmd);
            idx  = 0;
            break;
        default:                              /// * capture input char
            cmd[idx < TIB_SZ ? idx++ : idx] = c;
            break;
        }
    }
}

void forth_include(const char *fn) {
    FILE *fp = fopen(fn, "r");

    if (fp) outer(fp);
    else    fprintf(stderr, "failed to open file %s\n", fn);

    fclose(fp);
}
///====================================================================
///
/// main program - Note: Arduino and ESP32 have their own main-loop
///
#include <cstdlib>                            /// srand
#include <ctime>                              /// time
#include <iostream>
int main(int ac, char* av[]) {
    std::ios_base::sync_with_stdio(true);     /// * sync C++ iostream with C stdio
    forth_init();                             ///> initialize dictionary

    mem_stat();                               ///> show memory status
    srand((int)time(0));                      ///> seed random generator
    outer(stdin);                             ///> Forth outer interpreter

    forth_teardown();                         ///> clean up before we go
    fprintf(stdout, "%s Done!\n", APP_VERSION);
    return 0;
}
///====================================================================
