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

#if !(__APPLE__ || _WIN32 || _WIN64) 
#include <sys/sysinfo.h>   // memory info
#endif

using namespace std;

extern void forth_init();
extern int  forth_vm(const char *cmd, void(*)(int, const char*)=NULL);
extern void forth_teardown();

const char* APP_VERSION = "eForth_MQTT_dev v5.0";
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
#include <cstring>
#include "mqtt.h"
#define  MQTT_URI   "tcp://test.mosquitto.org:1883"
#define  TOPIC_CMD  "gnii/mqtt/cmd"
#define  TOPIC_RST  "gnii/mqtt/rst"

int gStop = 0;

int onCmd(void *ctx, char *topic, int len, mqtt_msg_t *msg) {
    char *cmd = (char*)msg->payload;
    
    printf("Cmd arrived\n");
    printf("  topic: %s\n", topic);
    printf("  msg: %.*s\n", msg->payloadlen, cmd);

    forth_vm(cmd);
    if (strcmp(cmd, "bye")==0) gStop = 1;

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
    printf("Usage:> %s [topic_cmd [topic_rst]]\n", argv[0]);
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 1) return usage(argv);
    
    MQTT mqtt(
        argv[1],
        MQTT_URI,
        argc > 2 ? argv[2] : TOPIC_CMD,
        argc > 3 ? argv[3] : TOPIC_RST,
        onCmd);

    std::ios_base::sync_with_stdio(true);       /// * sync C++ iostream with C stdio
    forth_init();                               /// * initialize dictionary

    mem_stat();                                 /// * show memory status
    srand((int)time(0));                        /// * seed random generator
    
    while (!gStop);

    forth_teardown();                           /// * clean up before we go
    fprintf(stdout, "%s Done!\n", APP_VERSION);
    
    return 0;
}
