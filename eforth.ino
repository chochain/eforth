///
/// @file
/// @brief eForth implemented for ESP32
///
///====================================================================
#include "soc/soc.h"                   /// * for brown out detector
#include "soc/rtc_cntl_reg.h"          /// * RTC control registers
///
///> ESP32 WiFi setup
///
#include "platform/mcu.h"              ///< Forth VM interfaces
#include "platform/server.h"           ///< ESP32 Web Server

const char *WIFI_SSID = "Amitofo-NV";  ///< use your own SSID
const char *WIFI_PASS = "AlseTron";    ///< and the password
///
///> ESP32 main loop
///
String tib;                            ///< terminal input buffer

void setup() {
    ///> disable brown-out detector
    /// Note: for prototyping only, turn it back on for production
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    Serial.begin(115200);
    delay(100);
    
    ForthServer::setup(WIFI_SSID, WIFI_PASS);
    mcu_init();                     ///> initialize Forth VM
    mem_stat();

    tib.reserve(256);                 ///> reserve 256 bytes for input
}

void loop(void) {
    ForthServer::handle_client();
    delay(2);    // yield to background tasks (interrupt, timer,...)
    ///
    /// while Web requests come in from handleInput asynchronously,
    /// we also take user input from console (for debugging mostly)
    ///
    auto rsp_to_con =        ///> redirect Forth response to console
        [](int len, const char *rst) { Serial.print(rst); };
    
    if (Serial.available()) {
        tib = Serial.readString();
        Serial.print(tib);
        
        forth_vm(tib.c_str(), rsp_to_con);
        delay(2);
    }
}
