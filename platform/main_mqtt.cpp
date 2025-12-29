#include <cstdio>
#include <cstdlib>         /// srand
#include <cstring>

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif _WIN32 || _WIN64
#include <windows.h>
extern char qkey();
#else // Linux || Cygwin
#include <fcntl.h>         // O_NONBLOCK
#include <unistd.h>        // read (low-level)
#include <error.h>         // EAGAIN, EWOUDLBLOCK
#include <sys/sysinfo.h>   // memory info
#endif

extern void forth_init();
extern int  forth_vm(const char *cmd, void(*)(int, const char*)=NULL);
extern void forth_teardown();

const char* APP_VERSION = "eForth_MQTT v5.0";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
#include <cstdint>         // U64
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
int getline_async(const int fno, char *cmd, char delim) {
    int idx = 0;
    while (1) {
        char ch = qkey();
        switch (ch) {
        case '\0': case EOF:  return 0;                /// * no input, skip
        case '\r': case '\n': return 1;                /// * line captured
        case 0x8:  case 0x7f: --idx; break;            /// * remove previous char
        default:   cmd[idx++] = ch; break;             /// * capture input char
        }
    }
    return idx;
}
#else // !(_WIN32 || _WIN64)
#include <errno.h>
int getline_async(const int fno, char *cmd, char delim) {
    int idx = 0, n = 1;
    while (n > 0) {
        char buf[2] = { 0 };
        n = (int)read(fno, buf, 1);                    /// * can return -1
        if (n==1) {                                    /// * got char
            if (*buf == delim) return 1;               /// * EOL
            cmd[idx++] = *buf;
        } else {
            n = errno==EAGAIN || errno==EWOULDBLOCK;   /// * reverted back to blocking
            if (n) return -1;                          /// * bail
        }
    }
    return idx;
}
#endif // _WIN32 || _WIN64

#define TIB_SZ 256
void forth_include(const char *fn) {
    FILE *fp = fopen(fn, "r");
    char buf[TIB_SZ];
    if (!fp) {
        fprintf(stderr, "failed to open file %s\n", fn);
        return;
    }
    int stop = 0;
    while (!stop &&fgets(buf, TIB_SZ, fp) != nullptr) {
        stop = forth_vm(buf, NULL);
    }
    fclose(fp);
}

///====================================================================
///
/// MQTT receiver
///
#include "mqtt.h"

#define  MQTT_URI "tcp://test.mosquitto.org:1883"

void outer(FILE *fp, MQTT *sndr) {
#if _WIN32 || _WIN64
    int fno = 0;
    auto noblock = []() {};
#else
    int fno = fileno(fp);                              ///< capture file number
    auto noblock = [fno]() {                           ///< set input to non-blocking
        int flags = fcntl(fno, F_GETFL, 0);
        fcntl(fno, F_SETFL, flags | O_NONBLOCK);
    };
#endif
    char cmd[128];
    int  stop = 0;
    noblock();
    while (!stop) {
        fflush(stdout);                                /// * flush output buffer before wait
        int n = getline_async(fno, cmd, '\n');
        if (n < 0) { noblock(); n = 0; }               /// * handle input error
        if (n) {
            fprintf(stderr, "cmd=<%s>\n", cmd);
            stop = sndr->publish(cmd);                 /// * call Forth VM (or trigger ticker)
        }
        else sndr->publish(nullptr);
    }
}

int onCmd(void *ctx, char *topic, int len, mqtt_msg_t *msg) {
    printf("Cmd arrived\n");
    printf("  topic: %s\n", topic);
    printf("  msg: %.*s\n", msg->payloadlen, (char*)msg->payload);

    forth_vm((char*)msg->payload);

    MQTTAsync_freeMessage(&msg);
    MQTTAsync_free(topic);

    return 1;
}

int onRst(void *ctx, char *topic, int len, mqtt_msg_t *msg) {
    printf("Rst arrived\n");
    printf("  topic: %s\n", topic);
    printf("  msg: %.*s\n", msg->payloadlen, (char*)msg->payload);

    printf("%s\n", (char*)msg->payload);

    MQTTAsync_freeMessage(&msg);
    MQTTAsync_free(topic);

    return 1;
}

///====================================================================
///
/// main program - Note: Arduino and ESP32 have their own main-loop
///
#include <ctime>                                /// time
#include <iostream>                             /// stdio
int usage(char *argv[]) {
    printf("Usage:> %s topic_get topic_put\n", argv[0]);
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 3) return usage(argv);
    
    MQTT mqtt( MQTT_URI, argv[1], onCmd, argv[2], onRst);

    std::ios_base::sync_with_stdio(true);       /// * sync C++ iostream with C stdio
    forth_init();                               /// * initialize dictionary

    mem_stat();                                 /// * show memory status
    srand((int)time(0));                        /// * seed random generator

//    outer(stdin, &mqtt);                      /// * Forth outer interpreter (non-blocking input)

    forth_teardown();                           /// * clean up before we go
    fprintf(stdout, "%s Done!\n", APP_VERSION);
    
    return 0;
}
