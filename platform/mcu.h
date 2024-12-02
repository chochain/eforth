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
///     665ms src/ceforth vector-based, object-threaded
///     534ms src/ceforth, multi-threading, vector-based, object-threaded (with gcc -O3)
///
const char *APP_VERSION = "eForth v5.0";
///
///> interface to core module
///
#include "../src/ceforth.h"
extern void forth_init();
extern int  forth_vm(const char *cmd, void(*hook)(int, const char*));
extern FV<Code*> dict;
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat()  {
    size_t  t = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t  f = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int64_t p = 1000L * f / t;
    LOGS(APP_VERSION);
    LOGS(" on core[");           LOG(xPortGetCoreID());
    LOGS("] at ");               LOG(getCpuFrequencyMhz());      
    LOGS(" MHz, RAM ");          LOG(static_cast<float>(p) * 0.1);
    LOGS("% free (");            LOG(f>>10);
    LOGS(" / ");                 LOG(t>>10); LOGS(" KB)\n");
    LOG_KV("  pinMode INPUT|OUTPUT|PULLUP|PULLDOWN=", INPUT);
    LOG_KV("|", OUTPUT);         LOG_KV("|", INPUT_PULLUP);
    LOG_KV("|", INPUT_PULLDOWN);
    LOG_KV(", digitalWrite HIGH|LOW=", HIGH);
    LOG_KV("|", LOW);            LOGS("\n");
}
///====================================================================
///
///> Arduino/ESP32 SPIFFS interfaces
///  @brief eForth external file loader from Flash memory
///         can be called in setup() to become a turn-key system
///
#include <SPIFFS.h>
void forth_include(const char *fname) {
    auto dumb = [](int, const char *) { /* silent output */ };
    if (!SPIFFS.begin()) {
        LOGS("Error mounting SPIFFS"); return;
    }
    File file = SPIFFS.open(fname, "r");
    if (!file) {
        LOGS("Error opening file:"); LOG(fname); return;
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
}
///
///> add ESP32 specific opcodes
///
#define PEEK(a)      (U32)(*(U32*)((UFP)(a)))
#define POKE(a, c)   (*(U32*)((UFP)(a))=(U32)(c))

const Code ops[] = {
    CODE("pinmode",IU p = POPI(); pinMode(p, POPI())),          // n p --
    CODE("in",     IU p = POPI(); PUSH(digitalRead(p))),        // p -- n
    CODE("out",    IU p = POPI(); digitalWrite(p, POPI())),     // n p --
    CODE("adc",    IU p = POPI(); PUSH(analogRead(p))),         // p -- n
    CODE("duty",   IU p = POPI(); analogWrite(p, POPI(), 255)), // n ch
    CODE("attach", IU p = POPI(); ledcAttachPin(p, POPI())),    // ch p --
    CODE("setup",  IU ch= POPI(); IU freq=POPI();               // res freq ch --
                       ledcSetup(ch, freq, POPI())),
    CODE("tone",   IU ch= POPI(); ledcWriteTone(ch, POPI())),   // duty ch --
    CODE("peek",   IU a = POPI(); PUSH(PEEK(a))),               // a -- n
    CODE("poke",   IU a = POPI(); POKE(a, POPI())),             // n a --
};

void mcu_init() {
    forth_init();
    
    const int sz = (int)(sizeof(ops))/(sizeof(Code));
    for (const Code &c : ops) dict.push((Code*)&c);
}
#endif // __EFORTH_PLATFORM_MCU_H

