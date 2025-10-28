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
#include <conio.h>         // getchar
#else // Linux || Cygwin
#include <sys/sysinfo.h>   // memory info
#include <termios.h>       // getchar
#include <unistd.h>        // STDIN_FILENO
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
                100 - p, f >> 20, t >> 20)
    }
	else fprintf(stderr, "ERR: Windows memory status fetch failed!");
#else // Linux, Cygwin
    struct sysinfo si;
    if (sysinfo(&si) != -1) {
      U64 f = (U64)si.freeram * si.mem_unit;
      U64 t = (U64)si.totalram * si.mem_unit;
      U64 p = f * 1000L / t;
      fprintf(stdout, 
              ", RAM %3.1f%% free (%ld / %ld MB)",
              static_cast<float>(p * 0.1), f >> 20, t >> 20);
    }
#endif

    fprintf(stdout, "\n");
}
///
///> include external Forth script
///
#if _WIN32 || _WIN64
#else
char getc(int file_no) {                      ///< get one unbuffered char with timeout
	struct termios t0, t1;

	tcgetattr(file_no, &t0);                  /// * backup stdin attributes
	t1 = t0;
	t1.c_lflag &= ~(ICANON | ECHO);           /// * non-buffered, and echo
    t1.c_cc[VMIN]  = 0;                       /// * no waiting
    t1.c_cc[VTIME] = 1;                       /// * timeout on 0.1 second (returns '\0')
	tcsetattr(file_no, TCSANOW, &t1);         /// * set to non-buffered
    
	char ch;
    int n = read(file_no, &ch, 1);            /// * fetch one char from given input file
    
	tcsetattr(file_no, TCSANOW, &t0);         /// * restore stdin attributes

	return n ? ch: '\0';
}
#endif

#define TIB_SZ 128                            /// * 128-byte line buffer
void outer(int file_no) {
	char  cmd[TIB_SZ+1];
	int   idx  = 0;
    int   done = 0;
	while (!done) {
        char c = getc(file_no);
        switch (c) {
        case '\0':
            forth_vm("clock . cr 1000 ms");                   /// * handle timer interrupt
            break;
        case 0x8: --idx; break;               /// * backspace
        case '\n': case '\r':
            cmd[idx] = '\0';
            done = forth_vm(cmd);             ///> run outer interpreter (single task)
            idx  = 0;
            break;
        case EOF: 
            printf(".EOF");
            done = 1;
            break;
        default:                              /// * capture input char
            printf(".%c", c);
            cmd[idx < TIB_SZ ? idx++ : idx] = c;
            break;
        }
    }
}

void forth_include(const char *fn) {
    FILE *fp = fopen(fn, "r");
    
    if (fp) outer(fp->_fileno);
    else    fprintf(stderr, "failed to open file %s\n", fn);
    
    fclose(fp);
}
///====================================================================
///
/// main program - Note: Arduino and ESP32 have their own main-loop
///
#include <cstdlib>                            /// srand
#include <ctime>                              /// time
int main(int ac, char* av[]) {
    forth_init();                             ///> initialize dictionary
    
    mem_stat();                               ///> show memory status
    srand((int)time(0));                      ///> seed random generator
    outer(STDIN_FILENO);                      ///> Forth outer interpreter
    
    forth_teardown();                         ///> clean up before we go
    fprintf(stdout, "%s Done!\n", APP_VERSION);
    return 0;
}
///====================================================================
