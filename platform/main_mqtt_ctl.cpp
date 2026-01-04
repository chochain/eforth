///
/// @file
/// @brief - MQTT enabled eForth
/// @note
///    1. build and install paho.mqtt.c i.e. make; sudo make install
///    2. sudo apt install openssl libssl-dev
///
#include <cstdio>
#include <cstdlib>         /// srand
#include <string>

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

using namespace std;

const char* APP_VERSION = "eForth_MQTT_ctl v5.0";
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
int getline_async(int fno, string &cmd, char delim='\n') {
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
int getline_async(int fno, string& cmd, char delim='\n') {
    int n = 1;
    while (n > 0) {
        char buf[2] = { 0 };
        n = (int)read(fno, buf, 1);                    /// * can return -1
        if (n==1) {                                    /// * got char
            if (*buf == delim) return 1;               /// * EOL
            cmd.append(buf);
        } else {
            n = errno==EAGAIN || errno==EWOULDBLOCK;   /// * reverted back to blocking
            if (n) return -1;                          /// * bail
        }
    }
    return n;
}
#endif // _WIN32 || _WIN64

///====================================================================
///
/// MQTT receiver
///
#include "mqtt.h"

#define  MQTT_URI  "tcp://test.mosquitto.org:1883"
#define  TOPIC_CMD "gnii/mqtt/cmd"
#define  TOPIC_RST "gnii/mqtt/rst"

MQTT gMqtt;

void outer(FILE *fp) {
#if _WIN32 || _WIN64
    int fno = 0;
#define NOBLOCK
#else
    int fno = fileno(fp);                              ///< capture file number
#define NOBLOCK fcntl(fno, F_SETFL, fcntl(fno, F_GETFL, 0) | O_NONBLOCK)   /**< set input to non-blocking */
#endif
    auto get_cmd = [](int fno) {
        string cmd("");
        fflush(stdout);                                /// * flush output buffer before wait
        int n = getline_async(fno, cmd);
        if (n < 0) { NOBLOCK; return 1; }              /// * handle input error
        if (n) {
            fprintf(stderr, "cmd=<%s>\n", cmd.c_str());
            gMqtt.publish(cmd.c_str());                /// * call Forth VM (or trigger ticker)
            
            if (cmd == "bye") return 0;                /// * shutdown console
        }
        return 1;
    };
    NOBLOCK;
    while (get_cmd(fno)) gMqtt.yield();                /// * sleep 10ms
}

int onRst(void *ctx, char *topic, int len, mqtt_msg_t *msg) {
    printf("%s[%d] >> %s",
           topic, msg->payloadlen, (char*)msg->payload);

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
    printf("Usage:> %s client_id [topic_rst [topic_cmd]]\n", argv[0]);
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return usage(argv);
    
    gMqtt.init(
        argv[1],
        MQTT_URI,
        argc > 2 ? argv[2] : TOPIC_RST,
        argc > 3 ? argv[3] : TOPIC_CMD,
        onRst);

    std::ios_base::sync_with_stdio(true);       /// * sync C++ iostream with C stdio

    mem_stat();                                 /// * show memory status

    outer(stdin);                               /// * Forth outer interpreter (non-blocking input)

    fprintf(stdout, "%s Done!\n", APP_VERSION);
    
    return 0;
}
