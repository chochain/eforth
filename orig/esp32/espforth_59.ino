/******************************************************************************/
/* espForth, Version 5.6 : for NodeMCU ESP32S                                 */
/******************************************************************************/
/* 13mar19cht  _58                                                            */
/* motor, speaker, adc                                                     */
/* 13mar19cht  _57                                                            */
/* buttons and text box                                                       */
/* 13mar19cht  _54,55,56                                                      */
/* audio demo from Whisker                                                    */
/* HTTP server, NOP returns from Forth to loop()                              */
/* Load.txt compiled on boot                                                  */
/* 21jan19cht  _51                                                            */
/* 8 channel electronic organ                                                 */
/* 15jan19cht  _50                                                            */
/* Clean up for AIR robot                                                     */
/* 03jan19cht  _47-49                                                         */
/* Move to ESP32                                                              */
/* 07jan19cht  _46                                                            */
/* delete UDP                                                                 */
/* 03jan19cht  _45                                                            */
/* Move to NodeMCU ESP32S Kit                                                 */
/* 18jul17cht  _44                                                            */
/* Byte code sequencer                                                        */
/* 14jul17cht  _43                                                            */
/* Stacks in circular buffers                                                 */
/* 01jul17cht  _42                                                            */
/* Compiled as an Arduino sketch                                              */
/* 20mar17cht  _41                                                            */
/* Compiled as an Arduino sketch                                              */
/* Follow the ceForth model with 64 primitives                                */
/* Serial Monitor at 115200 baud                                              */
/* Send and receive UDP packets in parallel with Serial Monitor               */
/* Case insensitive interpreter                                               */
/* data[] must be filled with rom42.h eForth dictionary                       */
/* 22jun17cht                                                                 */
/* Stacks are 256 cell circular buffers, with byte pointers R and S           */
/* All references to R and S are forced to (unsigned char)                    */
/* All multiply-divide words cleaned up                                       */
/******************************************************************************/

/******************************************************************************/
/* ledc                                                                       */
/******************************************************************************/
/*
 LEDC Software Fade
 This example shows how to software fade LED
 using the ledcWrite function.
 Code adapted from original Arduino Fade example:
 https://www.arduino.cc/en/Tutorial/Fade
 This example code is in the public domain.
 */

// use first channel of 16 channels (started from zero)
#define LEDC_CHANNEL_0     0
// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13
// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000
// fade LED PIN (replace with LED_BUILTIN constant for built-in LED)
#define LED_PIN            5
int brightness = 255;    // how bright the LED is

// Arduino like analogWrite
// value has to be between 0 and valueMax
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / valueMax) * min(value, valueMax);
  // write duty to LEDC
  ledcWrite(channel, duty);
}

#include <WiFi.h>
#include <WebServer.h>
#include "SPIFFS.h"

const char* ssid = "FIG-2019";//type your ssid
const char* pass = "GotFORTH!!";//type your password
// static ip address
IPAddress ip(192,168,2,202); 
IPAddress gateway(192,168,2,1);
IPAddress subnet(255,255,255,0);

WebServer server(80);

/******************************************************************************/
/* esp32Forth_51                                                              */
/******************************************************************************/

# define  FALSE 0
# define  TRUE  -1
# define  LOGICAL ? TRUE : FALSE
# define  LOWER(x,y) ((unsigned long)(x)<(unsigned long)(y))
# define  pop top = stack[(unsigned char)S--]
# define  push stack[(unsigned char)++S] = top; top =

long rack_main[256] = {0};
long stack_main[256] = {0};
long rack_background[256] = {0};
long stack_background[256] = {0};
__thread long *rack;
__thread long *stack;
__thread unsigned char R, S, bytecode ;
__thread long* Pointer ;
__thread long  P, IP, WP, top, len ;
uint8_t* cData ;
__thread long long int d, n, m ;
String HTTPin;
String HTTPout;
TaskHandle_t background_thread;

#include "rom_56.h" /* load dictionary */

/******************************************************************************/
/* PRIMITIVES                                                                 */
/******************************************************************************/

void next(void)
{ P = data[IP>>2];
  IP += 4;
  WP = P+4;  }

void accep()
/* WiFiClient */
{ while (Serial.available()) {
    len = Serial.readBytes(cData, top); }
  Serial.write(cData, len);
  top = len;
}
void qrx(void)
  { while (Serial.available() == 0) {};
    push Serial.read();
    push -1; }

void txsto(void)
{  Serial.write( (unsigned char) top);
   char c=top;
   HTTPout += c ;
   pop;
}

void docon(void)
{  push data[WP>>2]; }

void dolit(void)
{   push data[IP>>2];
  IP += 4;
  next(); }

void dolist(void)
{   rack[(unsigned char)++R] = IP;
  IP = WP;
  next(); }

void exitt(void)
{   IP = (long) rack[(unsigned char)R--];
  next(); }

void execu(void)
{  P = top;
  WP = P + 4;
  pop; }

void donext(void)
{   if(rack[(unsigned char)R]) {
    rack[(unsigned char)R] -= 1 ;
    IP = data[IP>>2];
  } else { IP += 4;  (unsigned char)R-- ;  }
  next(); }

void qbran(void)
{   if(top == 0) IP = data[IP>>2];
  else IP += 4;  pop;
  next(); }

void bran(void)
{   IP = data[IP>>2];
  next(); }

void store(void)
{   data[top>>2] = stack[(unsigned char)S--];
  pop;  }

void at(void)
{   top = data[top>>2];  }

void cstor(void)
{   cData[top] = (unsigned char) stack[(unsigned char)S--];
  pop;  }

void cat(void)
{   top = (long) cData[top];  }

void rpat(void) {}
void rpsto(void) {}

void rfrom(void)
{   push rack[(unsigned char)R--];  }

void rat(void)
{   push rack[(unsigned char)R];  }

void tor(void)
{   rack[(unsigned char)++R] = top;  pop;  }

void spat(void) {}
void spsto(void) {}

void drop(void)
{   pop;  }

void dup(void)
{   stack[(unsigned char)++S] = top;  }

void swap(void)
{   WP = top;
  top = stack[(unsigned char)S];
  stack[(unsigned char)S] = WP;  }

void over(void)
{  push stack[(unsigned char)(S-1)];  }

void zless(void)
{   top = (top < 0) LOGICAL;  }

void andd(void)
{   top &= stack[(unsigned char)S--];  }

void orr(void)
{   top |= stack[(unsigned char)S--];  }

void xorr(void)
{   top ^= stack[(unsigned char)S--];  }

void uplus(void)
{   stack[(unsigned char)S] += top;
  top = LOWER(stack[(unsigned char)S], top);  }

void nop(void)
{   next(); }

void qdup(void)
{   if(top) stack[(unsigned char)++S] = top ;  }

void rot(void)
{   WP = stack[(unsigned char)(S-1)];
  stack[(unsigned char)(S-1)] = stack[(unsigned char)S];
  stack[(unsigned char)S] = top;
  top = WP;  }

void ddrop(void)
{   drop(); drop();  }

void ddup(void)
{   over(); over();  }

void plus(void)
{   top += stack[(unsigned char)S--];  }

void inver(void)
{   top = -top-1;  }

void negat(void)
{   top = 0 - top;  }

void dnega(void)
{   inver();
  tor();
  inver();
  push 1;
  uplus();
  rfrom();
  plus(); }

void subb(void)
{   top = stack[(unsigned char)S--] - top;  }

void abss(void)
{   if(top < 0)
    top = -top;  }

void great(void)
{   top = (stack[(unsigned char)S--] > top) LOGICAL;  }

void less(void)
{   top = (stack[(unsigned char)S--] < top) LOGICAL;  }

void equal(void)
{   top = (stack[(unsigned char)S--] == top) LOGICAL;  }

void uless(void)
{   top = LOWER(stack[(unsigned char)S], top) LOGICAL; S--;  }

void ummod(void)
{  d = (long long int)((unsigned long)top);
  m = (long long int)((unsigned long)stack[(unsigned char) S]);
  n = (long long int)((unsigned long)stack[(unsigned char) (S - 1)]);
  n += m << 32;
  pop;
  top = (unsigned long)(n / d);
  stack[(unsigned char) S] = (unsigned long)(n%d); }
void msmod(void)
{ d = (signed long long int)((signed long)top);
  m = (signed long long int)((signed long)stack[(unsigned char) S]);
  n = (signed long long int)((signed long)stack[(unsigned char) S - 1]);
  n += m << 32;
  pop;
  top = (signed long)(n / d);
  stack[(unsigned char) S] = (signed long)(n%d); }
void slmod(void)
{ if (top != 0) {
    WP = stack[(unsigned char) S] / top;
    stack[(unsigned char) S] %= top;
    top = WP;
  } }
void mod(void)
{ top = (top) ? stack[(unsigned char) S--] % top : stack[(unsigned char) S--]; }
void slash(void)
{ top = (top) ? stack[(unsigned char) S--] / top : (stack[(unsigned char) S--], 0); }
void umsta(void)
{ d = (unsigned long long int)top;
  m = (unsigned long long int)stack[(unsigned char) S];
  m *= d;
  top = (unsigned long)(m >> 32);
  stack[(unsigned char) S] = (unsigned long)m; }
void star(void)
{ top *= stack[(unsigned char) S--]; }
void mstar(void)
{ d = (signed long long int)top;
  m = (signed long long int)stack[(unsigned char) S];
  m *= d;
  top = (signed long)(m >> 32);
  stack[(unsigned char) S] = (signed long)m; }
void ssmod(void)
{ d = (signed long long int)top;
  m = (signed long long int)stack[(unsigned char) S];
  n = (signed long long int)stack[(unsigned char) (S - 1)];
  n *= m;
  pop;
  top = (signed long)(n / d);
  stack[(unsigned char) S] = (signed long)(n%d); }
void stasl(void)
{ d = (signed long long int)top;
  m = (signed long long int)stack[(unsigned char) S];
  n = (signed long long int)stack[(unsigned char) (S - 1)];
  n *= m;
  pop; pop;
  top = (signed long)(n / d); }

void pick(void)
{   top = stack[(unsigned char)(S-top)];  }

void pstor(void)
{   data[top>>2] += stack[(unsigned char)S--], pop;  }

void dstor(void)
{   data[(top>>2)+1] = stack[(unsigned char)S--];
  data[top>>2] = stack[(unsigned char)S--];
  pop;  }

void dat(void)
{   push data[top>>2];
  top = data[(top>>2)+1];  }

void count(void)
{   stack[(unsigned char)++S] = top + 1;
  top = cData[top];  }

void dovar(void)
{   push WP; }

void maxx(void)
{   if (top < stack[(unsigned char)S]) pop;
  else (unsigned char)S--; }

void minn(void)
{   if (top < stack[(unsigned char)S]) (unsigned char)S--;
  else pop; }

void audio(void)
{  WP=top; pop;
   ledcWriteTone(WP,top);
   pop;
}

void sendPacket(void)
{}

void poke(void)
{   Pointer = (long*)top; *Pointer = stack[(unsigned char)S--];
    pop;  }

void peeek(void)
{   Pointer = (long*)top; top = *Pointer;  }

void adc(void)
{  top= (long) analogRead(top); }

void pin(void)
{  WP=top; pop;
   ledcAttachPin(top,WP);
   pop;
}

void duty(void)
{  WP=top; pop;
   ledcAnalogWrite(WP,top,255);
   pop;
}

void freq(void)
{  WP=top; pop;
   ledcSetup(WP,top,13);
   pop;
}

void (*primitives[72])(void) = {
    /* case 0 */ nop,
    /* case 1 */ accep,
    /* case 2 */ qrx,
    /* case 3 */ txsto,
    /* case 4 */ docon,
    /* case 5 */ dolit,
    /* case 6 */ dolist,
    /* case 7 */ exitt,
    /* case 8 */ execu,
    /* case 9 */ donext,
    /* case 10 */ qbran,
    /* case 11 */ bran,
    /* case 12 */ store,
    /* case 13 */ at,
    /* case 14 */ cstor,
    /* case 15 */ cat,
    /* case 16 */ nop,
    /* case 17 */ nop,
    /* case 18 */ rfrom,
    /* case 19 */ rat,
    /* case 20 */ tor,
    /* case 21 */ nop,
    /* case 22 */ nop,
    /* case 23 */ drop,
    /* case 24 */ dup,
    /* case 25 */ swap,
    /* case 26 */ over,
    /* case 27 */ zless,
    /* case 28 */ andd,
    /* case 29 */ orr,
    /* case 30 */ xorr,
    /* case 31 */ uplus,
    /* case 32 */ next,
    /* case 33 */ qdup,
    /* case 34 */ rot,
    /* case 35 */ ddrop,
    /* case 36 */ ddup,
    /* case 37 */ plus,
    /* case 38 */ inver,
    /* case 39 */ negat,
    /* case 40 */ dnega,
    /* case 41 */ subb,
    /* case 42 */ abss,
    /* case 43 */ equal,
    /* case 44 */ uless,
    /* case 45 */ less,
    /* case 46 */ ummod,
    /* case 47 */ msmod,
    /* case 48 */ slmod,
    /* case 49 */ mod,
    /* case 50 */ slash,
    /* case 51 */ umsta,
    /* case 52 */ star,
    /* case 53 */ mstar,
    /* case 54 */ ssmod,
    /* case 55 */ stasl,
    /* case 56 */ pick,
    /* case 57 */ pstor,
    /* case 58 */ dstor,
    /* case 59 */ dat,
    /* case 60 */ count,
    /* case 61 */ dovar,
    /* case 62 */ maxx,
    /* case 63 */ minn,
    /* case 64 */ audio,
    /* case 65 */ sendPacket,
    /* case 66 */ poke,
    /* case 67 */ peeek,
    /* case 68 */ adc,
    /* case 69 */ pin,
    /* case 70 */ duty,
    /* case 71 */ freq };

__thread int counter = 0;
void evaluate()
{ while (true){
    if (counter++ > 10000) {
      delay(1);
      counter = 0;
    }
    bytecode=(unsigned char)cData[P++];
    if (bytecode) {primitives[bytecode]();}
    else {break;}
  }                 // break on NOP
}

static const char *index_html =
"<!html>\n"
"<head>\n"
"<title>esp32forth</title>\n"
"<style>\n"
"body {\n"
"  padding: 5px;\n"
"  background-color: #111;\n"
"  color: #2cf;\n"
"}\n"
"#prompt {\n"
"  width: 100%;\n"
"  padding: 5px;\n"
"  font-family: monospace;\n"
"  background-color: #ff8;\n"
"}\n"
"#output {\n"
"  width: 100%;\n"
"  height: 80%;\n"
"  resize: none;\n"
"}\n"
"</style>\n"
"</head>\n"
"<h2>esp32forth</h2>\n"
"<link rel=\"icon\" href=\"data:,\">\n"
"<body>\n"
"Upload File: <input id=\"filepick\" type=\"file\" name=\"files[]\"></input><br/>\n"
"<button onclick=\"ask('hex')\">hex</button>\n"
"<button onclick=\"ask('decimal')\">decimal</button>\n"
"<button onclick=\"ask('words')\">words</button>\n"
"<button onclick=\"ask('$100 init hush')\">init</button>\n"
"<button onclick=\"ask('ride')\">ride</button>\n"
"<button onclick=\"ask('blow')\">blow</button>\n"
"<button onclick=\"ask('$50000 p0')\">fore</button>\n"
"<button onclick=\"ask('$a0000 p0')\">back</button>\n"
"<button onclick=\"ask('$10000 p0')\">left</button>\n"
"<button onclick=\"ask('$40000 p0')\">right</button>\n"
"<button onclick=\"ask('$90000 p0')\">spin</button>\n"
"<button onclick=\"ask('0 p0')\">stop</button>\n"
"<button onclick=\"ask('4 p0s')\">LED</button>\n"
"<button onclick=\"ask('$24 ADC . $27 ADC . $22 ADC . $23 ADC .')\">ADC</button>\n"
"<br/>\n"
"<textarea id=\"output\" readonly></textarea>\n"
"<input id=\"prompt\" type=\"prompt\"></input><br/>\n"
"<script>\n"
"var prompt = document.getElementById('prompt');\n"
"var filepick = document.getElementById('filepick');\n"
"var output = document.getElementById('output');\n"
"function httpPost(url, items, callback) {\n"
"  var fd = new FormData();\n"
"  for (k in items) {\n"
"    fd.append(k, items[k]);\n"
"  }\n"
"  var r = new XMLHttpRequest();\n"
"  r.onreadystatechange = function() {\n"
"    if (this.readyState == XMLHttpRequest.DONE) {\n"
"      if (this.status === 200) {\n"
"        callback(this.responseText);\n"
"      } else {\n"
"        callback(null);\n"
"      }\n"
"    }\n"
"  };\n"
"  r.open('POST', url);\n"
"  r.send(fd);\n"
"}\n"
"function ask(cmd, callback) {\n"
"  httpPost('/input',\n"
"           {cmd: cmd + '\\n'}, function(data) {\n"
"    if (data !== null) { output.value += data; }\n"
"    output.scrollTop = output.scrollHeight;  // Scroll to the bottom\n"
"    if (callback !== undefined) { callback(); }\n"
"  });\n"
"}\n"
"prompt.onkeyup = function(event) {\n"
"  if (event.keyCode === 13) {\n"
"    event.preventDefault();\n"
"    ask(prompt.value);\n"
"    prompt.value = '';\n"
"  }\n"
"};\n"
"filepick.onchange = function(event) {\n"
"  if (event.target.files.length > 0) {\n"
"    var reader = new FileReader();\n"
"    reader.onload = function(e) {\n"
"      var parts = e.target.result.split('\\n');\n"
"      function upload() {\n"
"        if (parts.length === 0) { filepick.value = ''; return; }\n"
"        ask(parts.shift(), upload);\n"
"      }\n"
"      upload();\n"
"    }\n"
"    reader.readAsText(event.target.files[0]);\n"
"  }\n"
"};\n"
"window.onload = function() {\n"
"  ask('');\n"
"  prompt.focus();\n"
"};\n"
"</script>\n"
;

static void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

static void handleInput() {
  if (!server.hasArg("cmd")) {
    return returnFail("Missing Input");
  }
  HTTPin = server.arg("cmd");
  HTTPout = "";
  Serial.println(HTTPin);  // line cleaned up
  len = HTTPin.length();
  HTTPin.getBytes(cData, len);
  //Serial.println("Enter Forth.");
  data[0x66] = 0;                   // >IN
  data[0x67] = len;                 // #TIB
  data[0x68] = 0;                   // 'TIB
  if (len > 3 && memcmp(cData, "bg ", 3) == 0) {
    if (background_thread) {
      vTaskDelete(background_thread);
      background_thread = 0;
    }
    data[0x66] = 3; // Skip "bg "
    // Start background thread 1024 byte stack.
    xTaskCreate(background, "background", 1024, &IP, tskIDLE_PRIORITY, &background_thread);
  } else {
    P = 0x180;                        // EVAL
    WP = 0x184;
    evaluate();
  }
//  Serial.println();
//  Serial.println("Return from Forth.");           // line cleaned up
//  Serial.print("Returning ");
  Serial.print(HTTPout.length());
//  Serial.println(" characters");
  server.setContentLength(HTTPout.length());
  server.send(200, "text/plain", HTTPout);
}

void background(void *ipp) {
  long *ipv = (long*) ipp;
  rack = rack_background;
  stack = stack_background;
  Serial.println("background!!");
  IP = *ipv;
  S = 0;
  R = 0;
  top = 0;
  P = 0x180;                        // EVAL
  WP = 0x184;
  evaluate();
  for(;;) {
  }
}

void setup()
{
  rack = rack_main;
  stack = stack_main;
  P = 0x180;
  WP = 0x184;
  IP = 0;
  S = 0;
  R = 0;
  top = 0;
  cData = (uint8_t *) data;
  Serial.begin(115200);
  delay(100);
  WiFi.config(ip, gateway, subnet);
//  WiFi.mode(WIFI_STA);
// attempt to connect to Wifi network:
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  // if you get a connection, report back via serial:
  server.begin();
  Serial.println("Booting esp32Forth v5.8 ...");

// Setup timer and attach timer to a led pin
  ledcSetup(0, 100, LEDC_TIMER_13_BIT);
  ledcAttachPin(5, 0);
  ledcAnalogWrite(0, 250, brightness);
  pinMode(2,OUTPUT);
  digitalWrite(2, HIGH);   // turn the LED2 on
  pinMode(16,OUTPUT);
  digitalWrite(16, HIGH);   // motor1 forward
  pinMode(17,OUTPUT);
  digitalWrite(17, HIGH);   // motor1 backward
  pinMode(18,OUTPUT);
  digitalWrite(18, HIGH);   // motor2 forward
  pinMode(19,OUTPUT);
  digitalWrite(19, HIGH);   // motor2 bacward

  if(!SPIFFS.begin(true)){Serial.println("Error mounting SPIFFS"); }
  File file = SPIFFS.open("/load.txt");
  if(file) {
    Serial.println("Load file.");
    len = file.read(cData+0x8000,0x7000);
    data[0x66] = 0;                   // >IN
    data[0x67] = len;                 // #TIB
    data[0x68] = 0x8000;              // 'TIB
    P = 0x180;                        // EVAL
    WP = 0x184;
    evaluate();
    Serial.println(" Done loading.");
    file.close();
    SPIFFS.end();
  }
  // Setup web server handlers
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });
  server.on("/input", HTTP_POST, handleInput);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
