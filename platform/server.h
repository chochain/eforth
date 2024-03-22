///
/// @file
/// @brief eForth ESP32 Web Serer connection and index
/// 
///====================================================================
#ifndef __EFORTH_PLATFORM_SERVER_H
#define __EFORTH_PLATFORM_SERVER_H
#include <WiFi.h>

const char *HTML_INDEX PROGMEM = R"XX(
HTTP/1.1 200 OK
Content-type:text/html

<html><head><meta charset='UTF-8'><title>eForth on ESP32</title>
<style>body{font-family:'Courier New',monospace;font-size:14px;}</style>
</head>
<body>
    <div id='log' style='float:left;overflow:auto;height:600px;width:600px;
         background-color:#f8f0f0;'>eForth 4.2</div>
    <textarea id='tib' style='height:600px;width:400px;'
        onkeydown='if (13===event.keyCode) forth()'>words</textarea>
</body>
<script>
let log = document.getElementById('log')
let tib = document.getElementById('tib')
let idx = 0
function send_post(url, ary) {
    let id  = '_'+(idx++).toString()
    let cmd = '\n---CMD'+id+'\n'
    let req = ary.slice(0,30).join('\n')
    log.innerHTML += '<div id='+id+'><font color=blue>'+req.replace(/\n/g,'<br/>')+'</font><br/></div>'
    fetch(url, {
        method: 'POST', headers: { 'Context-Type': 'text/plain' },
        body: cmd+req+cmd
     }).then(rsp=>rsp.text()).then(txt=>{
        document.getElementById(id).innerHTML +=
            txt.replace(/\n/g,'<br/>').replace(/\s/g,'&nbsp;')
        log.scrollTop=log.scrollHeight
        ary.splice(0,30)
        if (ary.length > 0) send_post(url, ary)
    })
}
function forth() {
    let ary = tib.value.split('\n')
    send_post('/input', ary)
    tib.value = ''
}
window.onload = ()=>{ tib.focus() }
</script></html>

)XX";

const char *HTML_CHUNKED PROGMEM = R"XX(
HTTP/1.1 200 OK
Content-type:text/plain
Transfer-Encoding: chunked

)XX";
    
namespace ForthServer {
    WiFiServer server;
    WiFiClient client;
    String     http_req;
    
    void setup(const char *ssid, const char *pass) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, pass);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        server.begin(80);
        Serial.print("WiFi Connected. ForthServer IP=");
        Serial.print(WiFi.localIP());
        Serial.println(":80");
        // reserve string space
        http_req.reserve(256);
    }
    int readline() {
        http_req.clear();
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                if (c == '\n') return 1;
                if (c != '\r') http_req += c;
            }
        }
        return 0;
    }
    void handle_index() {
        client.println(HTML_INDEX);                 ///
        delay(30);                   // give browser sometime to receive
    }
    void send_chunk(int len, const char *msg) {
        Serial.print(msg);
        client.println(len, HEX);
        client.println(msg);
        yield();
    }
    void handle_input() {
        while (readline() && http_req.length()>0);  /// skip HTTP header
        for (int i=0; i<4 && readline(); i++) {     /// find Forth command token
            if (http_req.startsWith("---CMD")) break;
        }
        // process Forth command, return in chunks
        client.println(HTML_CHUNKED);               /// send HTTP chunked header
        for (int i=0; readline(); i++) {
            if (http_req.startsWith("---CMD")) break;
            if (http_req.length() > 0) {
                Serial.println(http_req);           /// echo on console
                forth_vm(http_req.c_str(), send_chunk);
            }
        }
        send_chunk(0, "\r\n");                      /// close HTTP chunk stream
    }
    void handle_client() {                          /// uri router
        if (!(client = server.available())) return;
        while (readline()) {
            if (http_req.startsWith("GET /")) {
                handle_index();
                break;
            }
            else if (http_req.startsWith("POST /input")) {
                handle_input();
                break;
            }
        }
        client.stop();
        yield();
    }
};
#endif // __EFORTH_PLATFORM_SERVER_H
