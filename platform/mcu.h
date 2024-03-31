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
const char *APP_VERSION = "eForth v4.2";
///
///> interface to core module
///
#include "../src/ceforth.h"
extern void forth_init();
extern void forth_vm(const char *cmd, void(*callback)(int, const char*));
extern DU       top;
extern FV<Code*>dict;
extern FV<DU>   ss;
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat()  {
    size_t  t = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t  f = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int64_t p = 1000L * f / t;
    LOGS("eForth 4.2 on Core["); LOG(xPortGetCoreID());
    LOGS("] at ");               LOG(getCpuFrequencyMhz());      
    LOGS(" MHz, RAM ");          LOG(static_cast<float>(p) * 0.1);
    LOGS("% free (");            LOG(f>>10);
    LOGS(" / ");                 LOG(t>>10); LOGS(" KB)\n");
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
inline  DU POP() { DU n=top; top=ss.pop(); return n; }
#define PUSH(v)  (ss.push(top), top=(v))
#define PEEK(a)    (U32)(*(U32*)((UFP)(a)))
#define POKE(a, c) (*(U32*)((UFP)(a))=(U32)(c))

FV<Code*> ops = {
    CODE("pinmode",DU p = POP(); pinMode(p, POP())),          // n p --
    CODE("in",     DU p = POP(); PUSH(digitalRead(p))),       // p -- n
    CODE("out",    DU p = POP(); digitalWrite(p, POP())),     // n p --
    CODE("adc",    DU p = POP(); PUSH(analogRead(p))),        // p -- n
    CODE("duty",   DU p = POP(); analogWrite(p, POP(), 255)), // n ch
    CODE("attach", DU p = POP(); ledcAttachPin(p, POP())),    // ch p --
    CODE("setup",  DU ch= POP(); DU freq=POP();               // res freq ch --
                       ledcSetup(ch, freq, POP())),
    CODE("tone",   DU ch= POP(); ledcWriteTone(ch, POP())),   // duty ch --
    CODE("peek",   DU a = POP(); PUSH(PEEK(a))),              // a -- n
    CODE("poke",   DU a = POP(); POKE(a, POP())),             // n a --
};

void mcu_init() {
    forth_init();
    dict.merge(ops);
}
#endif // __EFORTH_PLATFORM_MCU_H

