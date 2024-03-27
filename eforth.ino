///
/// @file
/// @brief eForth implemented for ESP32
///
///====================================================================
#include "src/ceforth.h"               ///< Forth VM interface
#include "platform/server.h"           ///< ESP32 Web Server

const char *WIFI_SSID = "Amitofo-NV";       ///< use your own SSID
const char *WIFI_PASS = "AlseTron";       ///< and the password

String tib;                            ///< terminal input buffer

void setup() {
    Serial.begin(115200);
    delay(100);

    ForthServer::setup(WIFI_SSID, WIFI_PASS);
    forth_init();                     ///> initialize Forth VM

    tib.reserve(256);                 ///> reserve 256 bytes for input
}

void loop(void) {
    ForthServer::handle_client();
    delay(2);    // yield to background tasks (interrupt, timer,...)
    ///
    /// while Web requests come in from handleInput asynchronously,
    /// we also take user input from console (for debugging mostly)
    ///
    auto rsp_to_con = ///> redirect Forth response to console
        [](int len, const char *rst) { Serial.print(rst); };
    
    if (Serial.available()) {
        tib = Serial.readString();
        Serial.print(tib);
        
        forth_vm(tib.c_str(), rsp_to_con);
        delay(2);
    }
}
