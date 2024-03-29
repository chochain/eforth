#ifndef __EFORTH_PLATFORM_MCU_H
#define __EFORTH_PLATFORM_MCU_H
///
/// @file
/// @brief eForth implemented for ESP32
/// @note
///    benchmark: 1K*1K test case
///    1440ms Dr. Ting's orig/esp32forth_82
///    1240ms ~/Download/forth/esp32/esp32forth8_exp9
///    1045ms orig/esp32forth8_1
///     999ms orig/40x/ceforth subroutine-threaded, 16-bit xt offset
///     940ms orig/40x/ceforth use cached xt offsets in nest()
///     665ms src/ceforth vector-based token-threaded
///
extern void forth_init();
extern void forth_vm(const char *cmd, void(*callback)(int, const char*));

#define LOGS(s) Serial.print(F(s))
#define LOG(s)  Serial.print(s)

const char *APP_VERSION = "eForth v4.2";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat()  {
    size_t  t = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t  f = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int64_t p = 1000L * f / t;
    LOGS("eForth 4.2 on Core "); LOG(xPortGetCoreID());
    LOGS(", RAM ");   LOG(static_cast<float>(p) * 0.1);
    LOGS("% free ("); LOG(f>>10);
    LOGS(" / ");      LOG(t>>10); LOGS(" KB)\n");
}
///====================================================================
///
///> Arduino/ESP32 SPIFFS interfaces
///  @brief eForth external file loader from Flash memory
///         can be called in setup() to become a turn-key system
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
#endif // __EFORTH_PLATFORM_MCU_H

