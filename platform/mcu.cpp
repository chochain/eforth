///
/// @file
/// @brief eForth implemented for micro controllers (Aruino & ESP)
///
const char *APP_VERSION = "eForth v8.6";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat()  {
    LOGS(APP_VERSION);
    LOG_KV("\n  dict: ",   dict.idx); LOG_KV("/", E4_DICT_SZ);
    LOG_KV(", ",           sizeof(Code));  LOGS("-byte/entry");
    LOG_KV("\n  ss  : ",   ss.idx);   LOG_KV("/", E4_SS_SZ);
    LOG_KV(" (max ",       ss.max);   LOGS(")");
    LOG_KV("\n  rs  : ",   rs.idx);   LOG_KV("/", E4_RS_SZ);
    LOG_KV(" (max ",       rs.max);   LOGS(")");
    LOG_KX("\n  here: 0x", HERE);     LOG_KX("/0x", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    LOG_KX(" (free 0x",    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)); LOGS(")");
    LOG_KV("\n  core_id     : ", xPortGetCoreID());
    LOG_KV("\n  lowest_heap : ", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    LOG_KV("\n  stack hwmark: ",  uxTaskGetStackHighWaterMark(NULL));
    LOGS("\n");
    if (!heap_caps_check_integrity_all(true)) {
//        heap_trace_dump();     // dump memory, if we have to
        abort();                 // bail, on any memory error
    }
}
///====================================================================
///
///> Arduino/ESP32 SPIFFS interfaces
///
/// Arduino extra string handlers
int  find(string &s)  { return find(s.c_str()); }
void colon(string &s) { colon(s.c_str()); }
///
///> eForth turn-key code loader (from Flash memory)
///
#include <SPIFFS.h>
int forth_load(const char *fname) {
    auto dummy = [](int, const char *) { /* do nothing */ };
    if (!SPIFFS.begin()) {
        LOGS("Error mounting SPIFFS"); return 1;
    }
    File file = SPIFFS.open(fname, "r");
    if (!file) {
        LOGS("Error opening file:"); LOG(fname); return 1;
    }
    LOGS("Loading file: "); LOG(fname); LOGS("...");
    while (file.available()) {
        char cmd[256], *p = cmd, c;
        while ((c = file.read())!='\n') *p++ = c;   // one line a time
        *p = '\0';
        LOGS("\n<< "); LOG(cmd);                    // show bootstrap command
        forth_outer(cmd, dummy);
    }
    LOGS("Done loading.\n");
    file.close();
    SPIFFS.end();
    return 0;
}
///
/// main program - Note: Arduino&ESP32 has their own main-loop
///
///====================================================================
