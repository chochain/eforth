///
/// @file
/// @brief eForth implemented for ESP32
/// @note
///    benchmark: 1K*1K test case
///    1440ms Dr. Ting's orig/esp32forth_82
///    1240ms ~/Download/forth/esp32/esp32forth8_exp9
///    1045ms orig/esp32forth8_1
///     999ms src/ceforth subroutine indirect threading, with 16-bit offset
///     940ms src/ceforth use cached xt offsets in nest()
///
const char *APP_VERSION = "eForth v4.2";
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
    LOG_KX("\n  here: 0x", HERE);     LOG_KX("/0x", E4_PMEM_SZ);
    LOG_KV("\nForth VM on Core ", xPortGetCoreID());
    LOG_KX(", RAM avail/free 0x", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    LOG_KX("/0x",      heap_caps_get_free_size(MALLOC_CAP_8BIT));
    LOG_KX(" (min 0x", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
//    LOG_KX(", watermark high 0x", uxTaskGetStackHighWaterMark(NULL));
    LOGS(")\n");
    if (!heap_caps_check_integrity_all(true)) {
//        heap_trace_dump();     // dump memory, if we have to
        abort();                 // bail, on any memory error
    }
}
/// Arduino extra string handlers
int  find(string &s)  { return find(s.c_str()); }
void colon(string &s) { colon(s.c_str()); }
///====================================================================
///
///> Arduino/ESP32 SPIFFS interfaces
///
///
///> eForth turn-key code loader (from Flash memory)
///
#include <SPIFFS.h>
int forth_include(const char *fname) {
    auto dumb = [](int, const char *) { /* silent output */ };
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
        forth_vm(cmd, dumb);
    }
    LOGS("Done loading.\n");
    file.close();
    SPIFFS.end();
    return 0;
}


