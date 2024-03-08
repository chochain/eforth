///
/// eForth - Configuration and Cross Platform macros
///
#ifndef __EFORTH_SRC_CONFIG_H
#define __EFORTH_SRC_CONFIG_H
///
/// Benchmark: 10K*10K cycles on desktop (3.2G AMD)
///    RANGE_CHECK     0 cut 100ms
///    INLINE            cut 545ms
///
///@name Conditional compililation options
///@}
#define RANGE_CHECK     0               /**< vector range check     */
#define USE_FLOAT       0               /**< support floating point */
#define DO_WASM         __EMSCRIPTEN__  /**< for WASM output        */
#define CC_DEBUG        1               /**< debug tracing flag     */
///@}
///@name Memory block configuation
///@{
#define E4_RS_SZ        64
#define E4_SS_SZ        64
#define E4_DICT_SZ      1024
#define E4_PMEM_SZ      (32*1024)
///@}
///
///@name Logical units (instead of physical) for type check and portability
///@{
typedef uint32_t        U32;   ///< unsigned 32-bit integer
typedef int32_t         S32;   ///< signed 32-bit integer
typedef uint16_t        U16;   ///< unsigned 16-bit integer
typedef uint8_t         U8;    ///< byte, unsigned character
typedef uintptr_t       UFP;   ///< function pointer as integer

#if USE_FLOAT
typedef double          DU2;
typedef float           DU;
#define DU0             0.0f
#define UINT(v)         (fabs(v)))

#else // !USE_FLOAT
typedef int64_t         DU2;
typedef int32_t         DU;
#define DU0             0
#define UINT(v)         (abs(v))

#endif // USE_FLOAT
typedef uint16_t        IU;    ///< instruction pointer unit
///@}
///@name Inline & Alignment macros
///@{
#pragma GCC optimize("align-functions=4")    // we need fn alignment
#define INLINE          __attribute__((always_inline))
#define ALIGN2(sz)      ((sz) + (-(sz) & 0x1))
#define ALIGN4(sz)      ((sz) + (-(sz) & 0x3))
#define ALIGN16(sz)     ((sz) + (-(sz) & 0xf))
#define ALIGN32(sz)     ((sz) + (-(sz) & 0x1f))
#define ALIGN(sz)       ALIGN2(sz)
#define STRLEN(s)       (ALIGN(strlen(s)+1))  /** calculate string size with alignment */
///@}
///@name Multi-platform support
///@{
#if    _WIN32 || _WIN64
    #define ENDL "\r\n"
#else  // !(_WIN32 || _WIN64)
    #define ENDL endl; fout_cb(fout.str().length(), fout.str().c_str()); fout.str("")
#endif // _WIN32 || _WIN64

#if    ARDUINO
    #include <Arduino.h>
    #define to_string(i)    string(String(i).c_str())
    #define LOGS(s)         Serial.print(F(s))
    #define LOG(v)          Serial.print(v)
    #define LOGX(v)         Serial.print(v, HEX)
    #if    ESP32
        #define analogWrite(c,v,mx) ledcWrite((c),(8191/mx)*min((int)(v),mx))
    #endif // ESP32

#elif  DO_WASM
    #include <emscripten.h>
    #define millis()        EM_ASM_INT({ return Date.now(); })
    #define delay(ms)       EM_ASM({ let t = setTimeout(()=>clearTimeout(t), $0); }, ms)
    #define yield()         /* JS is async */

#else  // !ARDUINO && !DO_WASM
    #include <chrono>
    #include <thread>
    #define millis()        chrono::duration_cast<chrono::milliseconds>( \
                            chrono::steady_clock::now().time_since_epoch()).count()
    #define delay(ms)       this_thread::sleep_for(chrono::milliseconds(ms))
    #define yield()         this_thread::yield()
    #define PROGMEM

#endif // ARDUINO && DO_WASM
///@}
///@name Logging support
///@{
#if CC_DEBUG
    #define LOG_KV(k, v)
    #define LOG_KX(k, x)
    #define LOG_HDR(f, s)
    #define LOG_DIC(i)
    #define LOG_NA()
#else  // CC_DEBUG
#if ARDUINO
    #define LOG_KV(k, v)    LOGS(k); LOG(v)
    #define LOG_KX(k, x)    LOGS(k); LOGX(v)
    #define LOG_HDR(f, s)   LOGS(f); LOGS("("); LOGS(s); LOGS(") => ")
    #define LOG_DIC(i)      LOGS("dict["); LOG(i); LOGS("] "); \
                            LOG(dict[i].name); LOGS(" attr="); LOGX(dict[i].attr); \
                            LOGS("\n")
    #define LOG_NA()        LOGS("not found\n")

#else  // !ARDUINO
    #define LOG_KV(k, v)    printf("%s%d", k, v)
    #define LOG_KX(k, x)    printf("%s%x", k, x)
    #define LOG_HDR(f, s)   printf("%s(%s) => ", f, s)
    #define LOG_DIC(i)      printf("dict[%d] %s attr=%x\n", i, dict[i].name, dict[i].attr)
    #define LOG_NA()        printf("not found\n")
#endif // ARDUINO
#endif // CC_DEBUG
///@}
#endif // __EFORTH_SRC_CONFIG_H
