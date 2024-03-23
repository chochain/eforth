/*
 * ESP32forth v7.0.5
 * Revision: 33cf8aaa6fe3e0bc4abf3e4cd5c496a3071b9171
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef intptr_t cell_t;
typedef uintptr_t ucell_t;

#define Y(op, code) X(#op, id ## op, code)
#define NIP (--sp)
#define NIPn(n) (sp -= (n))
#define DROP (tos = *sp--)
#define DROPn(n) (NIPn(n-1), DROP)
#define DUP (*++sp = tos)
#define PUSH DUP; tos = (cell_t)
#define COMMA(n) *g_sys.heap++ = (n)
#define DOIMMEDIATE() (*g_sys.current)[-1] |= IMMEDIATE
#define UNSMUDGE() (*g_sys.current)[-1] &= ~SMUDGE
#define DOES(ip) **g_sys.current = (cell_t) ADDR_DODOES; (*g_sys.current)[1] = (cell_t) ip
#define PARK DUP; *++rp = (cell_t) sp; *++rp = (cell_t) ip

#ifndef SSMOD_FUNC
# if __SIZEOF_POINTER__ == 8
typedef __int128_t dcell_t;
# elif __SIZEOF_POINTER__ == 4 || defined(_M_IX86)
typedef int64_t dcell_t;
# else
#  error "unsupported cell size"
# endif
# define SSMOD_FUNC dcell_t d = (dcell_t) *sp * (dcell_t) sp[-1]; \
                    --sp; cell_t a = (cell_t) (d < 0 ? ~(~d / tos) : d / tos); \
                    *sp = (cell_t) (d - ((dcell_t) a) * tos); tos = a
#endif

#define OPCODE_LIST \
  X("0=", ZEQUAL, tos = !tos ? -1 : 0) \
  X("0<", ZLESS, tos = (tos|0) < 0 ? -1 : 0) \
  X("+", PLUS, tos += *sp--) \
  X("U/MOD", USMOD, w = *sp; *sp = (ucell_t) w % (ucell_t) tos; \
                    tos = (ucell_t) w / (ucell_t) tos) \
  X("*/MOD", SSMOD, SSMOD_FUNC) \
  Y(LSHIFT, tos = (*sp-- << tos)) \
  Y(RSHIFT, tos = (*sp-- >> tos)) \
  Y(AND, tos &= *sp--) \
  Y(OR, tos |= *sp--) \
  Y(XOR, tos ^= *sp--) \
  Y(DUP, DUP) \
  Y(SWAP, w = tos; tos = *sp; *sp = w) \
  Y(OVER, DUP; tos = sp[-1]) \
  Y(DROP, DROP) \
  X("@", AT, tos = *(cell_t *) tos) \
  X("L@", LAT, tos = *(int32_t *) tos) \
  X("C@", CAT, tos = *(uint8_t *) tos) \
  X("!", STORE, *(cell_t *) tos = *sp--; DROP) \
  X("L!", LSTORE, *(int32_t *) tos = *sp--; DROP) \
  X("C!", CSTORE, *(uint8_t *) tos = *sp--; DROP) \
  X("SP@", SPAT, DUP; tos = (cell_t) sp) \
  X("SP!", SPSTORE, sp = (cell_t *) tos; DROP) \
  X("RP@", RPAT, DUP; tos = (cell_t) rp) \
  X("RP!", RPSTORE, rp = (cell_t *) tos; DROP) \
  X(">R", TOR, *++rp = tos; DROP) \
  X("R>", FROMR, DUP; tos = *rp; --rp) \
  X("R@", RAT, DUP; tos = *rp) \
  Y(EXECUTE, w = tos; DROP; JMPW) \
  Y(BRANCH, ip = (cell_t *) *ip) \
  Y(0BRANCH, if (!tos) ip = (cell_t *) *ip; else ++ip; DROP) \
  Y(DONEXT, *rp = *rp - 1; if (~*rp) ip = (cell_t *) *ip; else (--rp, ++ip)) \
  Y(DOLIT, DUP; tos = *ip++) \
  Y(ALITERAL, COMMA(g_sys.DOLIT_XT); COMMA(tos); DROP) \
  Y(CELL, DUP; tos = sizeof(cell_t)) \
  Y(FIND, tos = find((const char *) *sp, tos); --sp) \
  Y(PARSE, DUP; tos = parse(tos, sp)) \
  X("S>NUMBER?", CONVERT, tos = convert((const char *) *sp, tos, sp); \
                          if (!tos) --sp) \
  Y(CREATE, DUP; DUP; tos = parse(32, sp); \
            create((const char *) *sp, tos, 0, ADDR_DOCREATE); \
            COMMA(0); DROPn(2)) \
  X("DOES>", DOES, DOES(ip); ip = (cell_t *) *rp; --rp) \
  Y(IMMEDIATE, DOIMMEDIATE()) \
  X("'SYS", SYS, DUP; tos = (cell_t) &g_sys) \
  Y(YIELD, PARK; return rp) \
  X(":", COLON, DUP; DUP; tos = parse(32, sp); \
                create((const char *) *sp, tos, SMUDGE, ADDR_DOCOLON); \
                g_sys.state = -1; --sp; DROP) \
  Y(EVALUATE1, DUP; sp = evaluate1(sp); w = *sp--; DROP; if (w) JMPW) \
  Y(EXIT, ip = (cell_t *) *rp--) \
  X(";", SEMICOLON, UNSMUDGE(); COMMA(g_sys.DOEXIT_XT); g_sys.state = 0) \


#define SET tos = (cell_t)

#define n0 tos
#define n1 (*sp)
#define n2 sp[-1]
#define n3 sp[-2]
#define n4 sp[-3]
#define n5 sp[-4]
#define n6 sp[-5]
#define n7 sp[-6]
#define n8 sp[-7]
#define n9 sp[-8]
#define n10 sp[-9]

#define a0 ((void *) tos)
#define a1 (*(void **) &n1)
#define a2 (*(void **) &n2)
#define a3 (*(void **) &n3)
#define a4 (*(void **) &n4)
#define a5 (*(void **) &n5)
#define a6 (*(void **) &n6)

#define b0 ((uint8_t *) tos)
#define b1 (*(uint8_t **) &n1)
#define b2 (*(uint8_t **) &n2)
#define b3 (*(uint8_t **) &n3)
#define b4 (*(uint8_t **) &n4)
#define b5 (*(uint8_t **) &n5)
#define b6 (*(uint8_t **) &n6)

#define c0 ((char *) tos)
#define c1 (*(char **) &n1)
#define c2 (*(char **) &n2)
#define c3 (*(char **) &n3)
#define c4 (*(char **) &n4)
#define c5 (*(char **) &n5)
#define c6 (*(char **) &n6)



// For now, default on several options.
#define ENABLE_SPIFFS_SUPPORT
#define ENABLE_WIFI_SUPPORT
#define ENABLE_MDNS_SUPPORT
#define ENABLE_WEBSERVER_SUPPORT
#define ENABLE_SDCARD_SUPPORT
#define ENABLE_I2C_SUPPORT
#define ENABLE_SOCKETS_SUPPORT
#define ENABLE_FREERTOS_SUPPORT
#define ENABLE_INTERRUPTS_SUPPORT

// Uncomment this #define for OLED Support.
// You will need to install these libraries from the Library Manager:
//   Adafruit SSD1306
//   Adafruit GFX Library
//   Adafruit BusIO
// #define ENABLE_OLED_SUPPORT

// For now assume only boards with PSRAM (ESP32-CAM)
// will want SerialBluetooth (very large) and camera support.
// Other boards can support these if they're set to a larger
// parition size. But it's unclear the best way to configure this.
#ifdef BOARD_HAS_PSRAM
# define ENABLE_CAMERA_SUPPORT
# define ENABLE_SERIAL_BLUETOOTH_SUPPORT
#endif

#ifdef ENABLE_WEBSERVER_SUPPORT
# include "WebServer.h"
#endif

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#if defined(ESP32)
# define HEAP_SIZE (100 * 1024)
# define STACK_SIZE 512
#elif defined(ESP8266)
# define HEAP_SIZE (40 * 1024)
# define STACK_SIZE 512
#else
# define HEAP_SIZE 2 * 1024
# define STACK_SIZE 32
#endif
#define INTERRUPT_STACK_CELLS 64

#define PLATFORM_OPCODE_LIST \
  /* Memory Allocation */ \
  Y(MALLOC, SET malloc(n0)) \
  Y(SYSFREE, free(a0); DROP) \
  Y(REALLOC, SET realloc(a1, n0); NIP) \
  Y(heap_caps_malloc, SET heap_caps_malloc(n1, n0); NIP) \
  Y(heap_caps_free, heap_caps_free(a0); DROP) \
  Y(heap_caps_realloc, \
      tos = (cell_t) heap_caps_realloc(a2, n1, n0); NIPn(2)) \
  /* Serial */ \
  X("Serial.begin", SERIAL_BEGIN, Serial.begin(tos); DROP) \
  X("Serial.end", SERIAL_END, Serial.end()) \
  X("Serial.available", SERIAL_AVAILABLE, PUSH Serial.available()) \
  X("Serial.readBytes", SERIAL_READ_BYTES, n0 = Serial.readBytes(b1, n0); NIP) \
  X("Serial.write", SERIAL_WRITE, n0 = Serial.write(b1, n0); NIP) \
  X("Serial.flush", SERIAL_FLUSH, Serial.flush()) \
  /* Pins and PWM */ \
  Y(pinMode, pinMode(n1, n0); DROPn(2)) \
  Y(digitalWrite, digitalWrite(n1, n0); DROPn(2)) \
  Y(digitalRead, n0 = digitalRead(n0)) \
  Y(analogRead, n0 = analogRead(n0)) \
  Y(pulseIn, n0 = pulseIn(n2, n1, n0); NIPn(2)) \
  Y(dacWrite, dacWrite(n1, n0); DROPn(2)) \
  Y(ledcSetup, \
      n0 = (cell_t) (1000000 * ledcSetup(n2, n1 / 1000.0, n0)); NIPn(2)) \
  Y(ledcAttachPin, ledcAttachPin(n1, n0); DROPn(2)) \
  Y(ledcDetachPin, ledcDetachPin(n0); DROP) \
  Y(ledcRead, n0 = ledcRead(n0)) \
  Y(ledcReadFreq, n0 = (cell_t) (1000000 * ledcReadFreq(n0))) \
  Y(ledcWrite, ledcWrite(n1, n0); DROPn(2)) \
  Y(ledcWriteTone, \
      n0 = (cell_t) (1000000 * ledcWriteTone(n1, n0 / 1000.0)); NIP) \
  Y(ledcWriteNote, \
      tos = (cell_t) (1000000 * ledcWriteNote(n2, (note_t) n1, n0)); NIPn(2)) \
  /* General System */ \
  X("MS-TICKS", MS_TICKS, PUSH millis()) \
  X("RAW-YIELD", RAW_YIELD, yield()) \
  Y(TERMINATE, exit(n0)) \
  /* File words */ \
  X("R/O", R_O, PUSH O_RDONLY) \
  X("R/W", R_W, PUSH O_RDWR) \
  X("W/O", W_O, PUSH O_WRONLY) \
  Y(BIN, ) \
  X("CLOSE-FILE", CLOSE_FILE, tos = close(tos); tos = tos ? errno : 0) \
  X("FLUSH-FILE", FLUSH_FILE, fsync(tos); /* fsync has no impl and returns ENOSYS :-( */ tos = 0) \
  X("OPEN-FILE", OPEN_FILE, cell_t mode = n0; DROP; cell_t len = n0; DROP; \
    memcpy(filename, a0, len); filename[len] = 0; \
    n0 = open(filename, mode, 0777); PUSH n0 < 0 ? errno : 0) \
  X("CREATE-FILE", CREATE_FILE, cell_t mode = n0; DROP; cell_t len = n0; DROP; \
    memcpy(filename, a0, len); filename[len] = 0; \
    n0 = open(filename, mode | O_CREAT | O_TRUNC); PUSH n0 < 0 ? errno : 0) \
  X("DELETE-FILE", DELETE_FILE, cell_t len = n0; DROP; \
    memcpy(filename, a0, len); filename[len] = 0; \
    n0 = unlink(filename); n0 = n0 ? errno : 0) \
  X("WRITE-FILE", WRITE_FILE, cell_t fd = n0; DROP; cell_t len = n0; DROP; \
    n0 = write(fd, a0, len); n0 = n0 != len ? errno : 0) \
  X("READ-FILE", READ_FILE, cell_t fd = n0; DROP; cell_t len = n0; DROP; \
    n0 = read(fd, a0, len); PUSH n0 < 0 ? errno : 0) \
  X("FILE-POSITION", FILE_POSITION, \
    n0 = (cell_t) lseek(n0, 0, SEEK_CUR); PUSH n0 < 0 ? errno : 0) \
  X("REPOSITION-FILE", REPOSITION_FILE, cell_t fd = n0; DROP; \
    n0 = (cell_t) lseek(fd, tos, SEEK_SET); n0 = n0 < 0 ? errno : 0) \
  X("RESIZE-FILE", RESIZE_FILE, cell_t fd = n0; DROP; n0 = ResizeFile(fd, tos)) \
  X("FILE-SIZE", FILE_SIZE, struct stat st; w = fstat(n0, &st); \
    n0 = (cell_t) st.st_size; PUSH w < 0 ? errno : 0) \
  OPTIONAL_SPIFFS_SUPPORT \
  OPTIONAL_WIFI_SUPPORT \
  OPTIONAL_MDNS_SUPPORT \
  OPTIONAL_WEBSERVER_SUPPORT \
  OPTIONAL_SDCARD_SUPPORT \
  OPTIONAL_I2C_SUPPORT \
  OPTIONAL_SERIAL_BLUETOOTH_SUPPORT \
  OPTIONAL_CAMERA_SUPPORT \
  OPTIONAL_SOCKETS_SUPPORT \
  OPTIONAL_FREERTOS_SUPPORT \
  OPTIONAL_INTERRUPTS_SUPPORT \
  OPTIONAL_OLED_SUPPORT \

#ifndef ENABLE_SPIFFS_SUPPORT
// Provide a default failing SPIFFS.begin
# define OPTIONAL_SPIFFS_SUPPORT \
  X("SPIFFS.begin", SPIFFS_BEGIN, NIPn(2); n0 = 0)
#else
# include "SPIFFS.h"
# define OPTIONAL_SPIFFS_SUPPORT \
  X("SPIFFS.begin", SPIFFS_BEGIN, \
      tos = SPIFFS.begin(n2, c1, n0); NIPn(2)) \
  X("SPIFFS.end", SPIFFS_END, SPIFFS.end()) \
  X("SPIFFS.format", SPIFFS_FORMAT, PUSH SPIFFS.format()) \
  X("SPIFFS.totalBytes", SPIFFS_TOTAL_BYTES, PUSH SPIFFS.totalBytes()) \
  X("SPIFFS.usedBytes", SPIFFS_USED_BYTES, PUSH SPIFFS.usedBytes())
#endif

#ifndef ENABLE_FREERTOS_SUPPORT
# define OPTIONAL_FREERTOS_SUPPORT
#else
# include "freertos/FreeRTOS.h"
# include "freertos/task.h"
# define OPTIONAL_FREERTOS_SUPPORT \
  Y(vTaskDelete, vTaskDelete((TaskHandle_t) n0); DROP) \
  Y(xTaskCreatePinnedToCore, n0 = xTaskCreatePinnedToCore((TaskFunction_t) a6, c5, n4, a3, (UBaseType_t) n2, (TaskHandle_t *) a1, (BaseType_t) n0); NIPn(6)) \
  Y(xPortGetCoreID, PUSH xPortGetCoreID())
#endif

#ifndef ENABLE_INTERRUPTS_SUPPORT
# define OPTIONAL_INTERRUPTS_SUPPORT
#else
# include "esp_intr_alloc.h"
# include "driver/timer.h"
# include "driver/gpio.h"
# define OPTIONAL_INTERRUPTS_SUPPORT \
  Y(gpio_config, n0 = gpio_config((const gpio_config_t *) a0)) \
  Y(gpio_reset_pin, n0 = gpio_reset_pin((gpio_num_t) n0)) \
  Y(gpio_set_intr_type, n0 = gpio_set_intr_type((gpio_num_t) n1, (gpio_int_type_t) n0); NIP) \
  Y(gpio_intr_enable, n0 = gpio_intr_enable((gpio_num_t) n0)) \
  Y(gpio_intr_disable, n0 = gpio_intr_disable((gpio_num_t) n0)) \
  Y(gpio_set_level, n0 = gpio_set_level((gpio_num_t) n1, n0); NIP) \
  Y(gpio_get_level, n0 = gpio_get_level((gpio_num_t) n0)) \
  Y(gpio_set_direction, n0 = gpio_set_direction((gpio_num_t) n1, (gpio_mode_t) n0); NIP) \
  Y(gpio_set_pull_mode, n0 = gpio_set_pull_mode((gpio_num_t) n1, (gpio_pull_mode_t) n0); NIP) \
  Y(gpio_wakeup_enable, n0 = gpio_wakeup_enable((gpio_num_t) n1, (gpio_int_type_t) n0); NIP) \
  Y(gpio_wakeup_disable, n0 = gpio_wakeup_disable((gpio_num_t) n0)) \
  Y(gpio_pullup_en, n0 = gpio_pullup_en((gpio_num_t) n0)) \
  Y(gpio_pullup_dis, n0 = gpio_pullup_dis((gpio_num_t) n0)) \
  Y(gpio_pulldown_en, n0 = gpio_pulldown_en((gpio_num_t) n0)) \
  Y(gpio_pulldown_dis, n0 = gpio_pulldown_dis((gpio_num_t) n0)) \
  Y(gpio_hold_en, n0 = gpio_hold_en((gpio_num_t) n0)) \
  Y(gpio_hold_dis, n0 = gpio_hold_dis((gpio_num_t) n0)) \
  Y(gpio_deep_sleep_hold_en, gpio_deep_sleep_hold_en()) \
  Y(gpio_deep_sleep_hold_dis, gpio_deep_sleep_hold_dis()) \
  Y(gpio_install_isr_service, n0 = gpio_install_isr_service(n0)) \
  Y(gpio_uninstall_isr_service, gpio_uninstall_isr_service()) \
  Y(gpio_isr_handler_add, n0 = GpioIsrHandlerAdd(n2, n1, n0); NIPn(2)) \
  Y(gpio_isr_handler_remove, n0 = gpio_isr_handler_remove((gpio_num_t) n0)) \
  Y(gpio_set_drive_capability, n0 = gpio_set_drive_capability((gpio_num_t) n1, (gpio_drive_cap_t) n0); NIP) \
  Y(gpio_get_drive_capability, n0 = gpio_get_drive_capability((gpio_num_t) n1, (gpio_drive_cap_t *) a0); NIP) \
  Y(esp_intr_alloc, n0 = EspIntrAlloc(n4, n3, n2, n1, a0); NIPn(4)) \
  Y(esp_intr_free, n0 = esp_intr_free((intr_handle_t) n0)) \
  Y(timer_isr_register, n0 = TimerIsrRegister(n5, n4, n3, n2, n1, a0); NIPn(5))
#endif

#ifndef ENABLE_CAMERA_SUPPORT
# define OPTIONAL_CAMERA_SUPPORT
#else
# include "esp_camera.h"
# define OPTIONAL_CAMERA_SUPPORT \
  Y(esp_camera_init, n0 = esp_camera_init((camera_config_t *) a0)) \
  Y(esp_camera_deinit, PUSH esp_camera_deinit()) \
  Y(esp_camera_fb_get, PUSH esp_camera_fb_get()) \
  Y(esp_camera_fb_return, esp_camera_fb_return((camera_fb_t *) a0); DROP) \
  Y(esp_camera_sensor_get, PUSH esp_camera_sensor_get())
#endif

#ifndef ENABLE_SOCKETS_SUPPORT
# define OPTIONAL_SOCKETS_SUPPORT
#else
# include <errno.h>
# include <sys/select.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <sys/types.h>
# include <sys/un.h>
# include <sys/poll.h>
# define OPTIONAL_SOCKETS_SUPPORT \
  Y(socket, n0 = socket(n2, n1, n0); NIPn(2)) \
  Y(bind, n0 = bind(n2, (struct sockaddr *) a1, n0); NIPn(2)) \
  Y(listen, n0 = listen(n1, n0); NIP) \
  Y(connect, n0 = connect(n2, (struct sockaddr *) a1, n0); NIPn(2)) \
  Y(accept, n0 = accept(n2, (struct sockaddr *) a1, (socklen_t *) a0); NIPn(2)) \
  Y(select, n0 = select(n4, (fd_set *) a3, (fd_set *) a2, (fd_set *) a1, (struct timeval *) a0); NIPn(4)) \
  Y(poll, n0 = poll((struct pollfd *) a2, (nfds_t) n1, n0); NIPn(2)) \
  Y(errno, PUSH errno)
#endif

#ifndef ENABLE_SDCARD_SUPPORT
# define OPTIONAL_SDCARD_SUPPORT
#else
# include "SD_MMC.h"
# define OPTIONAL_SDCARD_SUPPORT \
  X("SD_MMC.begin", SD_MMC_BEGIN, tos = SD_MMC.begin(c1, n0); NIP) \
  X("SD_MMC.end", SD_MMC_END, SD_MMC.end()) \
  X("SD_MMC.cardType", SD_MMC_CARD_TYPE, PUSH SD_MMC.cardType()) \
  X("SD_MMC.totalBytes", SD_MMC_TOTAL_BYTES, PUSH SD_MMC.totalBytes()) \
  X("SD_MMC.usedBytes", SD_MMC_USED_BYTES, PUSH SD_MMC.usedBytes())
#endif

#ifndef ENABLE_I2C_SUPPORT
# define OPTIONAL_I2C_SUPPORT
#else
# include <Wire.h>
# define OPTIONAL_I2C_SUPPORT \
  X("Wire.begin", WIRE_BEGIN, n0 = Wire.begin(n1, n0); NIP) \
  X("Wire.setClock", WIRE_SET_CLOCK, Wire.setClock(n0); DROP) \
  X("Wire.getClock", WIRE_GET_CLOCK, PUSH Wire.getClock()) \
  X("Wire.setTimeout", WIRE_SET_TIMEOUT, Wire.setTimeout(n0); DROP) \
  X("Wire.getTimeout", WIRE_GET_TIMEOUT, PUSH Wire.getTimeout()) \
  X("Wire.lastError", WIRE_LAST_ERROR, PUSH Wire.lastError()) \
  X("Wire.getErrorText", WIRE_GET_ERROR_TEXT, PUSH Wire.getErrorText(n0)) \
  X("Wire.beginTransmission", WIRE_BEGIN_TRANSMISSION, Wire.beginTransmission(n0); DROP) \
  X("Wire.endTransmission", WIRE_END_TRANSMISSION, SET Wire.endTransmission(n0)) \
  X("Wire.requestFrom", WIRE_REQUEST_FROM, n0 = Wire.requestFrom(n2, n1, n0); NIPn(2)) \
  X("Wire.writeTransmission", WIRE_WRITE_TRANSMISSION, \
      n0 = Wire.writeTransmission(n3, b2, n1, n0); NIPn(3)) \
  X("Wire.readTransmission", WIRE_READ_TRANSMISSION, \
      n0 = Wire.readTransmission(n4, b3, n2, n1, (uint32_t *) a0); NIPn(4)) \
  X("Wire.write", WIRE_WRITE, n0 = Wire.write(b1, n0); NIP) \
  X("Wire.available", WIRE_AVAILABLE, PUSH Wire.available()) \
  X("Wire.read", WIRE_READ, PUSH Wire.read()) \
  X("Wire.peek", WIRE_PEEK, PUSH Wire.peek()) \
  X("Wire.busy", WIRE_BUSY, PUSH Wire.busy()) \
  X("Wire.flush", WIRE_FLUSH, Wire.flush())
#endif

#ifndef ENABLE_SERIAL_BLUETOOTH_SUPPORT
# define OPTIONAL_SERIAL_BLUETOOTH_SUPPORT
#else
# include "esp_bt_device.h"
# include "BluetoothSerial.h"
# define bt0 ((BluetoothSerial *) a0)
# define OPTIONAL_SERIAL_BLUETOOTH_SUPPORT \
  X("SerialBT.new", SERIALBT_NEW, PUSH new BluetoothSerial()) \
  X("SerialBT.delete", SERIALBT_DELETE, delete bt0; DROP) \
  X("SerialBT.begin", SERIALBT_BEGIN, n0 = bt0->begin(c2, n1); NIPn(2)) \
  X("SerialBT.end", SERIALBT_END, bt0->end(); DROP) \
  X("SerialBT.available", SERIALBT_AVAILABLE, n0 = bt0->available()) \
  X("SerialBT.readBytes", SERIALBT_READ_BYTES, n0 = bt0->readBytes(b2, n1); NIPn(2)) \
  X("SerialBT.write", SERIALBT_WRITE, n0 = bt0->write(b2, n1); NIPn(2)) \
  X("SerialBT.flush", SERIALBT_FLUSH, bt0->flush(); DROP) \
  X("SerialBT.hasClient", SERIALBT_HAS_CLIENT, n0 = bt0->hasClient()) \
  X("SerialBT.enableSSP", SERIALBT_ENABLE_SSP, bt0->enableSSP(); DROP) \
  X("SerialBT.setPin", SERIALBT_SET_PIN, n0 = bt0->setPin(c1); NIP) \
  X("SerialBT.unpairDevice", SERIALBT_UNPAIR_DEVICE, \
      n0 = bt0->unpairDevice(b1); NIP) \
  X("SerialBT.connect", SERIALBT_CONNECT, n0 = bt0->connect(c1); NIP) \
  X("SerialBT.connectAddr", SERIALBT_CONNECT_ADDR, n0 = bt0->connect(b1); NIP) \
  X("SerialBT.disconnect", SERIALBT_DISCONNECT, n0 = bt0->disconnect()) \
  X("SerialBT.connected", SERIALBT_CONNECTED, n0 = bt0->connected(n1); NIP) \
  X("SerialBT.isReady", SERIALBT_IS_READY, n0 = bt0->isReady(n2, n1); NIPn(2)) \
  /* Bluetooth */ \
  Y(esp_bt_dev_get_address, PUSH esp_bt_dev_get_address())
#endif

#ifndef ENABLE_WIFI_SUPPORT
# define OPTIONAL_WIFI_SUPPORT
#else
# include <WiFi.h>
# include <WiFiClient.h>

static IPAddress ToIP(cell_t ip) {
  return IPAddress(ip & 0xff, ((ip >> 8) & 0xff), ((ip >> 16) & 0xff), ((ip >> 24) & 0xff));
}

static cell_t FromIP(IPAddress ip) {
  cell_t ret = 0;
  ret = (ret << 8) | ip[3];
  ret = (ret << 8) | ip[2];
  ret = (ret << 8) | ip[1];
  ret = (ret << 8) | ip[0];
  return ret;
}

# define OPTIONAL_WIFI_SUPPORT \
  /* WiFi */ \
  X("WiFi.config", WIFI_CONFIG, \
      WiFi.config(ToIP(n3), ToIP(n2), ToIP(n1), ToIP(n0)); DROPn(4)) \
  X("WiFi.begin", WIFI_BEGIN, WiFi.begin(c1, c0); DROPn(2)) \
  X("WiFi.disconnect", WIFI_DISCONNECT, WiFi.disconnect()) \
  X("WiFi.status", WIFI_STATUS, PUSH WiFi.status()) \
  X("WiFi.macAddress", WIFI_MAC_ADDRESS, WiFi.macAddress(b0); DROP) \
  X("WiFi.localIP", WIFI_LOCAL_IPS, PUSH FromIP(WiFi.localIP())) \
  X("WiFi.mode", WIFI_MODE, WiFi.mode((wifi_mode_t) n0); DROP) \
  X("WiFi.setTxPower", WIFI_SET_TX_POWER, WiFi.setTxPower((wifi_power_t) n0); DROP) \
  X("WiFi.getTxPower", WIFI_GET_TX_POWER, PUSH WiFi.getTxPower())
#endif

#ifndef ENABLE_MDNS_SUPPORT
# define OPTIONAL_MDNS_SUPPORT
#else
# include <ESPmDNS.h>
# define OPTIONAL_MDNS_SUPPORT \
  /* mDNS */ \
  X("MDNS.begin", MDNS_BEGIN, n0 = MDNS.begin(c0))
#endif

#ifndef ENABLE_WEBSERVER_SUPPORT
# define OPTIONAL_WEBSERVER_SUPPORT
#else
# include <WebServer.h>
# define ws0 ((WebServer *) a0)
# define OPTIONAL_WEBSERVER_SUPPORT \
  /* WebServer */ \
  X("WebServer.new", WEBSERVER_NEW, PUSH new WebServer(tos)) \
  X("WebServer.delete", WEBSERVER_DELETE, delete ws0; DROP) \
  X("WebServer.begin", WEBSERVER_BEGIN, ws0->begin(n1); DROPn(2)) \
  X("WebServer.stop", WEBSERVER_STOP, ws0->stop(); DROP) \
  X("WebServer.on", WEBSERVER_ON, InvokeWebServerOn(ws0, c2, n1); DROPn(3)) \
  X("WebServer.hasArg", WEBSERVER_HAS_ARG, n0 = ws0->hasArg(c1); DROP) \
  X("WebServer.arg", WEBSERVER_ARG, \
      string_value = ws0->arg(c1); \
      c1 = &string_value[0]; n0 = string_value.length()) \
  X("WebServer.argi", WEBSERVER_ARGI, \
      string_value = ws0->arg(n1); \
      c1 = &string_value[0]; n0 = string_value.length()) \
  X("WebServer.argName", WEBSERVER_ARG_NAME, \
      string_value = ws0->argName(n1); \
      c1 = &string_value[0]; n0 = string_value.length()) \
  X("WebServer.args", WEBSERVER_ARGS, n0 = ws0->args()) \
  X("WebServer.setContentLength", WEBSERVER_SET_CONTENT_LENGTH, \
      ws0->setContentLength(n1); DROPn(2)) \
  X("WebServer.sendHeader", WEBSERVER_SEND_HEADER, \
      ws0->sendHeader(c3, c2, n1); DROPn(4)) \
  X("WebServer.send", WEBSERVER_SEND, ws0->send(n3, c2, c1); DROPn(4)) \
  X("WebServer.sendContent", WEBSERVER_SEND_CONTENT, \
      ws0->sendContent(c1); DROPn(2)) \
  X("WebServer.method", WEBSERVER_METHOD, n0 = ws0->method()) \
  X("WebServer.handleClient", WEBSERVER_HANDLE_CLIENT, ws0->handleClient(); DROP)
#endif

#ifndef ENABLE_OLED_SUPPORT
# define OPTIONAL_OLED_SUPPORT
#else
#  include <Adafruit_GFX.h>
#  include <Adafruit_SSD1306.h>
static Adafruit_SSD1306 *oled_display = 0;
# define OPTIONAL_OLED_SUPPORT \
  Y(OledAddr, PUSH &oled_display) \
  Y(OledNew, oled_display = new Adafruit_SSD1306(n2, n1, &Wire, n0); DROPn(3)) \
  Y(OledDelete, delete oled_display) \
  Y(OledBegin, n0 = oled_display->begin(n1, n0); NIP) \
  Y(OledHOME, oled_display->setCursor(0,0); DROP) \
  Y(OledCLS, oled_display->clearDisplay()) \
  Y(OledTextc, oled_display->setTextColor(n0); DROP) \
  Y(OledPrintln, oled_display->println(c0); DROP) \
  Y(OledNumln, oled_display->println(n0); DROP) \
  Y(OledNum, oled_display->print(n0); DROP) \
  Y(OledDisplay, oled_display->display()) \
  Y(OledPrint, oled_display->write(c0); DROP) \
  Y(OledInvert, oled_display->invertDisplay(n0); DROP) \
  Y(OledTextsize, oled_display->setTextSize(n0); DROP) \
  Y(OledSetCursor, oled_display->setCursor(n1,n0); DROPn(2)) \
  Y(OledPixel, oled_display->drawPixel(n2, n1, n0); DROPn(2)) \
  Y(OledDrawL, oled_display->drawLine(n4, n3, n2, n1, n0); DROPn(4)) \
  Y(OledCirc, oled_display->drawCircle(n3,n2, n1, n0); DROPn(3)) \
  Y(OledCircF, oled_display->fillCircle(n3, n2, n1, n0); DROPn(3)) \
  Y(OledRect, oled_display->drawRect(n4, n3, n2, n1, n0); DROPn(4)) \
  Y(OledRectF, oled_display->fillRect(n4, n3, n2, n1, n0); DROPn(3)) \
  Y(OledRectR, oled_display->drawRoundRect(n5, n4, n3, n2, n1, n0); DROPn(5)) \
  Y(OledRectRF, oled_display->fillRoundRect(n5, n4, n3, n2, n1, n0 ); DROPn(5))
#endif

static char filename[PATH_MAX];
static String string_value;

static cell_t EspIntrAlloc(cell_t source, cell_t flags, cell_t xt, cell_t arg, cell_t *ret);
static cell_t GpioIsrHandlerAdd(cell_t pin, cell_t xt, cell_t arg);
static cell_t TimerIsrRegister(cell_t group, cell_t timer, cell_t xt, cell_t arg, void *ret);

#define PRINT_ERRORS 0

#define CELL_LEN(n) (((n) + sizeof(cell_t) - 1) / sizeof(cell_t))
#define FIND(name) find(name, sizeof(name) - 1)
#define LOWER(ch) ((ch) & 0x5F)
#define IMMEDIATE 1
#define SMUDGE 2
#define VOCABULARY_DEPTH 16

#if PRINT_ERRORS
#include <unistd.h>
#endif

static struct {
  const char *tib;
  cell_t ntib, tin, state, base;
  cell_t *heap, **current, ***context, notfound;
  int argc;
  char **argv;
  cell_t *(*runner)(cell_t *rp);  // pointer to forth_run
  cell_t *rp;  // spot to park main thread
  cell_t DOLIT_XT, DOEXIT_XT, YIELD_XT;
} g_sys;

static cell_t convert(const char *pos, cell_t n, cell_t *ret) {
  *ret = 0;
  cell_t negate = 0;
  cell_t base = g_sys.base;
  if (!n) { return 0; }
  if (pos[0] == '-') { negate = -1; ++pos; --n; }
  if (pos[0] == '$') { base = 16; ++pos; --n; }
  for (; n; --n) {
    uintptr_t d = pos[0] - '0';
    if (d > 9) {
      d = LOWER(d) - 7;
      if (d < 10) { return 0; }
    }
    if (d >= base) { return 0; }
    *ret = *ret * base + d;
    ++pos;
  }
  if (negate) { *ret = -*ret; }
  return -1;
}

static cell_t same(const char *a, const char *b, cell_t len) {
  for (;len && LOWER(*a) == LOWER(*b); --len, ++a, ++b);
  return len;
}

static cell_t find(const char *name, cell_t len) {
  for (cell_t ***voc = g_sys.context; *voc; ++voc) {
    cell_t *pos = **voc;
    cell_t clen = CELL_LEN(len);
    while (pos) {
      if (!(pos[-1] & SMUDGE) && len == pos[-3] &&
          same(name, (const char *) &pos[-3 - clen], len) == 0) {
        return (cell_t) pos;
      }
      pos = (cell_t *) pos[-2];  // Follow link
    }
  }
  return 0;
}

static void create(const char *name, cell_t length, cell_t flags, void *op) {
  char *pos = (char *) g_sys.heap;
  for (cell_t n = length; n; --n) { *pos++ = *name++; }  // name
  g_sys.heap += CELL_LEN(length);
  *g_sys.heap++ = length;  // length
  *g_sys.heap++ = (cell_t) *g_sys.current;  // link
  *g_sys.heap++ = flags;  // flags
  *g_sys.current = g_sys.heap;
  *g_sys.heap++ = (cell_t) op;  // code
}

static int match(char sep, char ch) {
  return sep == ch || (sep == ' ' && (ch == '\t' || ch == '\n' || ch == '\r'));
}

static cell_t parse(cell_t sep, cell_t *ret) {
  while (g_sys.tin < g_sys.ntib &&
         match(sep, g_sys.tib[g_sys.tin])) { ++g_sys.tin; }
  *ret = (cell_t) (g_sys.tib + g_sys.tin);
  while (g_sys.tin < g_sys.ntib &&
         !match(sep, g_sys.tib[g_sys.tin])) { ++g_sys.tin; }
  cell_t len = g_sys.tin - (*ret - (cell_t) g_sys.tib);
  if (g_sys.tin < g_sys.ntib) { ++g_sys.tin; }
  return len;
}

static cell_t *evaluate1(cell_t *sp) {
  cell_t call = 0;
  cell_t name;
  cell_t len = parse(' ', &name);
  if (len == 0) { *++sp = 0; return sp; }  // ignore empty
  cell_t xt = find((const char *) name, len);
  if (xt) {
    if (g_sys.state && !(((cell_t *) xt)[-1] & IMMEDIATE)) {
      *g_sys.heap++ = xt;
    } else {
      call = xt;
    }
  } else {
    cell_t n;
    cell_t ok = convert((const char *) name, len, &n);
    if (ok) {
      if (g_sys.state) {
        *g_sys.heap++ = g_sys.DOLIT_XT;
        *g_sys.heap++ = n;
      } else {
        *++sp = n;
      }
    } else {
#if PRINT_ERRORS
      write(2, (void *) name, len);
      write(2, "\n", 1);
#endif
      *++sp = name;
      *++sp = len;
      *++sp = -1;
      call = g_sys.notfound;
    }
  }
  *++sp = call;
  return sp;
}

static cell_t *forth_run(cell_t *initrp);

static void forth_init(int argc, char *argv[], void *heap,
                         const char *src, cell_t src_len) {
  g_sys.heap = ((cell_t *) heap) + 4;  // Leave a little room.
  cell_t *sp = g_sys.heap + 1; g_sys.heap += STACK_SIZE;
  cell_t *rp = g_sys.heap + 1; g_sys.heap += STACK_SIZE;

  // FORTH vocabulary
  *g_sys.heap++ = 0; cell_t *forth = g_sys.heap;
  *g_sys.heap++ = 0;  *g_sys.heap++ = 0;  *g_sys.heap++ = 0;
  // Vocabulary stack
  g_sys.current = (cell_t **) forth;
  g_sys.context = (cell_t ***) g_sys.heap;
  *g_sys.heap++ = (cell_t) forth;
  for (int i = 0; i < VOCABULARY_DEPTH; ++i) { *g_sys.heap++ = 0; }

  forth_run(0);
  (*g_sys.current)[-1] = IMMEDIATE;  // Make last word ; IMMEDIATE
  g_sys.DOLIT_XT = FIND("DOLIT");
  g_sys.DOEXIT_XT = FIND("EXIT");
  g_sys.YIELD_XT = FIND("YIELD");
  g_sys.notfound = FIND("DROP");
  cell_t *start = g_sys.heap;
  *g_sys.heap++ = FIND("EVALUATE1");
  *g_sys.heap++ = FIND("BRANCH");
  *g_sys.heap++ = (cell_t) start;
  g_sys.argc = argc;
  g_sys.argv = argv;
  g_sys.base = 10;
  g_sys.tib = src;
  g_sys.ntib = src_len;
  *++rp = (cell_t) sp;
  *++rp = (cell_t) start;
  g_sys.rp = rp;
  g_sys.runner = forth_run;
}

#define JMPW goto **(void **) w
#define NEXT w = *ip++; JMPW
#define ADDR_DOCOLON && OP_DOCOLON
#define ADDR_DOCREATE && OP_DOCREATE
#define ADDR_DODOES && OP_DODOES

static cell_t *forth_run(cell_t *init_rp) {
  if (!init_rp) {
#define X(name, op, code) create(name, sizeof(name) - 1, name[0] == ';', && OP_ ## op);
    PLATFORM_OPCODE_LIST
    OPCODE_LIST
#undef X
    return 0;
  }
  register cell_t *ip, *rp, *sp, tos, w;
  rp = init_rp;  ip = (cell_t *) *rp--;  sp = (cell_t *) *rp--;
  DROP; NEXT;
#define X(name, op, code) OP_ ## op: { code; } NEXT;
  PLATFORM_OPCODE_LIST
  OPCODE_LIST
#undef X
  OP_DOCOLON: ++rp; *rp = (cell_t) ip; ip = (cell_t *) (w + sizeof(cell_t)); NEXT;
  OP_DOCREATE: DUP; tos = w + sizeof(cell_t) * 2; NEXT;
  OP_DODOES: DUP; tos = w + sizeof(cell_t) * 2;
             ++rp; *rp = (cell_t) ip; ip = (cell_t *) *(cell_t *) (w + sizeof(cell_t)); NEXT;
}

const char boot[] =
": (   41 parse drop drop ; immediate\n"
"\n"
"( Useful Basic Compound Words )\n"
": nip ( a b -- b ) swap drop ;\n"
": rdrop ( r: n n -- ) r> r> drop >r ;\n"
": */ ( n n n -- n ) */mod nip ;\n"
": * ( n n -- n ) 1 */ ;\n"
": /mod ( n n -- n n ) 1 swap */mod ;\n"
": / ( n n -- n ) /mod nip ;\n"
": mod ( n n -- n ) /mod drop ;\n"
": invert ( n -- ~n ) -1 xor ;\n"
": negate ( n -- -n ) invert 1 + ;\n"
": - ( n n -- n ) negate + ;\n"
": rot ( a b c -- c a b ) >r swap r> swap ;\n"
": -rot ( a b c -- b c a ) swap >r swap r> ;\n"
": < ( a b -- a<b ) - 0< ;\n"
": > ( a b -- a>b ) swap - 0< ;\n"
": <= ( a b -- a>b ) swap - 0< 0= ;\n"
": >= ( a b -- a<b ) - 0< 0= ;\n"
": = ( a b -- a!=b ) - 0= ;\n"
": <> ( a b -- a!=b ) = 0= ;\n"
": 0<> ( n -- n) 0= 0= ;\n"
": bl 32 ;   : nl 10 ;\n"
": 1+ 1 + ;   : 1- 1 - ;\n"
": 2* 2 * ;   : 2/ 2 / ;\n"
": 4* 4 * ;   : 4/ 4 / ;\n"
": +! ( n a -- ) swap over @ + swap ! ;\n"
"\n"
"( Line Comments )\n"
": \\   nl parse drop drop ; immediate\n"
"\n"
"( Cells )\n"
": cell+ ( n -- n ) cell + ;\n"
": cells ( n -- n ) cell * ;\n"
": cell/ ( n -- n ) cell / ;\n"
"\n"
"( Double Words )\n"
": 2drop ( n n -- ) drop drop ;\n"
": 2dup ( a b -- a b a b ) over over ;\n"
": 2@ ( a -- lo hi ) dup @ swap cell+ @ ;\n"
": 2! ( lo hi a -- ) dup >r cell+ ! r> ! ;\n"
"\n"
"( System Variables )\n"
": 'tib ( -- a ) 'sys 0 cells + ;\n"
": #tib ( -- a ) 'sys 1 cells + ;\n"
": >in ( -- a ) 'sys 2 cells + ;\n"
": state ( -- a ) 'sys 3 cells + ;\n"
": base ( -- a ) 'sys 4 cells + ;\n"
": 'heap ( -- a ) 'sys 5 cells + ;\n"
": current ( -- a ) 'sys 6 cells + ;\n"
": 'context ( -- a ) 'sys 7 cells + ;  : context 'context @ cell+ ;\n"
": 'notfound ( -- a ) 'sys 8 cells + ;\n"
"\n"
"( Dictionary )\n"
": here ( -- a ) 'heap @ ;\n"
": allot ( n -- ) 'heap +! ;\n"
": aligned ( a -- a ) cell 1 - dup >r + r> invert and ;\n"
": align   here aligned here - allot ;\n"
": , ( n --  ) here ! cell allot ;\n"
": c, ( ch -- ) here c! 1 allot ;\n"
"\n"
"( Compilation State )\n"
": [ 0 state ! ; immediate\n"
": ] -1 state ! ; immediate\n"
"\n"
"( Quoting Words )\n"
": ' bl parse 2dup find dup >r -rot r> 0= 'notfound @ execute 2drop ;\n"
": ['] ' aliteral ; immediate\n"
": char bl parse drop c@ ;\n"
": [char] char aliteral ; immediate\n"
": literal aliteral ; immediate\n"
"\n"
"( Core Control Flow )\n"
": begin   here ; immediate\n"
": again   ['] branch , , ; immediate\n"
": until   ['] 0branch , , ; immediate\n"
": ahead   ['] branch , here 0 , ; immediate\n"
": then   here swap ! ; immediate\n"
": if   ['] 0branch , here 0 , ; immediate\n"
": else   ['] branch , here 0 , swap here swap ! ; immediate\n"
": while   ['] 0branch , here 0 , swap ; immediate\n"
": repeat   ['] branch , , here swap ! ; immediate\n"
": aft   drop ['] branch , here 0 , here swap ; immediate\n"
"\n"
"( Compound words requiring conditionals )\n"
": min 2dup < if drop else nip then ;\n"
": max 2dup < if nip else drop then ;\n"
": abs ( n -- +n ) dup 0< if negate then ;\n"
"\n"
"( Dictionary Format )\n"
": >name ( xt -- a n ) 3 cells - dup @ swap over aligned - swap ;\n"
": >link& ( xt -- a ) 2 cells - ;   : >link ( xt -- a ) >link& @ ;\n"
": >flags ( xt -- flags ) cell - ;\n"
": >body ( xt -- a ) dup @ [ ' >flags @ ] literal = 2 + cells + ;\n"
"\n"
"( Postpone - done here so we have ['] and IF )\n"
": immediate? ( xt -- f ) >flags @ 1 and 0= 0= ;\n"
": postpone ' dup immediate? if , else aliteral ['] , , then ; immediate\n"
"\n"
"( Constants and Variables )\n"
": constant ( n \"name\" -- ) create , does> @ ;\n"
": variable ( \"name\" -- ) create 0 , ;\n"
"\n"
"( Stack Convience )\n"
"sp@ constant sp0\n"
"rp@ constant rp0\n"
": depth ( -- n ) sp@ sp0 - cell/ ;\n"
"\n"
"( FOR..NEXT )\n"
": for   postpone >r postpone begin ; immediate\n"
": next   postpone donext , ; immediate\n"
"\n"
"( DO..LOOP )\n"
"variable leaving\n"
": leaving,   here leaving @ , leaving ! ;\n"
": leaving(   leaving @ 0 leaving ! ;\n"
": )leaving   leaving @ swap leaving !\n"
"             begin dup while dup @ swap here swap ! repeat drop ;\n"
": (do) ( n n -- .. ) swap r> -rot >r >r >r ;\n"
": do ( lim s -- ) leaving( postpone (do) here ; immediate\n"
": (?do) ( n n -- n n f .. ) 2dup = if 2drop 0 else -1 then ;\n"
": ?do ( lim s -- ) leaving( postpone (?do) postpone 0branch leaving,\n"
"                   postpone (do) here ; immediate\n"
": unloop   postpone rdrop postpone rdrop ; immediate\n"
": leave   postpone unloop postpone branch leaving, ; immediate\n"
": (+loop) ( n -- f .. ) dup 0< swap r> r> rot + dup r@ < -rot >r >r xor 0= ;\n"
": +loop ( n -- ) postpone (+loop) postpone until\n"
"                 postpone unloop )leaving ; immediate\n"
": loop   1 aliteral postpone +loop ; immediate\n"
": i ( -- n ) postpone r@ ; immediate\n"
": j ( -- n ) rp@ 3 cells - @ ;\n"
"\n"
"( Exceptions )\n"
"variable handler\n"
": catch ( xt -- n )\n"
"  sp@ >r handler @ >r rp@ handler ! execute r> handler ! r> drop 0 ;\n"
": throw ( n -- )\n"
"  dup if handler @ rp! r> handler ! r> swap >r sp! drop r> else drop then ;\n"
"' throw 'notfound !\n"
"\n"
"( Values )\n"
": value ( n -- ) create , does> @ ;\n"
": to ( n -- )\n"
"   ' >body state @ if aliteral postpone ! else ! then ; immediate\n"
": +to ( n -- )\n"
"   ' >body state @ if aliteral postpone +! else +! then ; immediate\n"
"\n"
"( Deferred Words )\n"
": defer ( \"name\" -- ) create 0 , does> @ dup 0= throw execute ;\n"
": is ( xt \"name -- ) postpone to ; immediate\n"
"\n"
"( Defer I/O to platform specific )\n"
"defer type\n"
"defer key\n"
"defer key?\n"
"defer bye\n"
": emit ( n -- ) >r rp@ 1 type rdrop ;\n"
": space bl emit ;   : cr nl emit ;\n"
"\n"
"( Numeric Output )\n"
"variable hld\n"
": pad ( -- a ) here 80 + ;\n"
": digit ( u -- c ) 9 over < 7 and + 48 + ;\n"
": extract ( n base -- n c ) u/mod swap digit ;\n"
": <# ( -- ) pad hld ! ;\n"
": hold ( c -- ) hld @ 1 - dup hld ! c! ;\n"
": # ( u -- u ) base @ extract hold ;\n"
": #s ( u -- 0 ) begin # dup while repeat ;\n"
": sign ( n -- ) 0< if 45 hold then ;\n"
": #> ( w -- b u ) drop hld @ pad over - ;\n"
": str ( n -- b u ) dup >r abs <# #s r> sign #> ;\n"
": hex ( -- ) 16 base ! ;   : octal ( -- ) 8 base ! ;\n"
": decimal ( -- ) 10 base ! ;   : binary ( -- ) 2 base ! ;\n"
": u. ( u -- ) <# #s #> type space ;\n"
": . ( w -- ) base @ 10 xor if u. exit then str type space ;\n"
": ? ( a -- ) @ . ;\n"
": n. ( n -- ) base @ swap decimal <# #s #> type base ! ;\n"
"\n"
"( Strings )\n"
": parse-quote ( -- a n ) [char] \" parse ;\n"
": $place ( a n -- ) for aft dup c@ c, 1+ then next drop 0 c, align ;\n"
": $@   r@ dup cell+ swap @ r> dup @ 1+ aligned + cell+ >r ;\n"
": s\"   parse-quote state @ if postpone $@ dup , $place\n"
"       else dup here swap >r >r $place r> r> then ; immediate\n"
": .\"   postpone s\" state @ if postpone type else type then ; immediate\n"
": z\"   postpone s\" state @ if postpone drop else drop then ; immediate\n"
": r\"   parse-quote state @ if swap aliteral aliteral then ; immediate\n"
": r|   [char] | parse state @ if swap aliteral aliteral then ; immediate\n"
": s>z ( a n -- z ) here >r $place r> ;\n"
": z>s ( z -- a n ) 0 over begin dup c@ while 1+ swap 1+ swap repeat drop ;\n"
"\n"
"( Fill, Move )\n"
": cmove ( a a n -- ) for aft >r dup c@ r@ c! 1+ r> 1+ then next 2drop ;\n"
": cmove> ( a a n -- ) for aft 2dup swap r@ + c@ swap r@ + c! then next 2drop ;\n"
": fill ( a a n -- ) swap for swap aft 2dup c! 1 + then next 2drop ;\n"
"\n"
"( Better Errors )\n"
": notfound ( a n n -- )\n"
"   if cr .\" ERROR: \" type .\"  NOT FOUND!\" cr -1 throw then ;\n"
"' notfound 'notfound !\n"
"\n"
"( Input )\n"
": raw.s   depth 0 max for aft sp@ r@ cells - @ . then next ;\n"
"variable echo -1 echo !   variable arrow -1 arrow !\n"
": ?echo ( n -- ) echo @ if emit else drop then ;\n"
": ?arrow.   arrow @ if >r >r raw.s r> r> .\" --> \" then ;\n"
": accept ( a n -- n ) ?arrow. 0 swap begin 2dup < while\n"
"     key\n"
"     dup nl = over 13 = or if ?echo drop nip exit then\n"
"     dup 8 = over 127 = or if\n"
"       drop over if rot 1- rot 1- rot 8 ?echo bl ?echo 8 ?echo then\n"
"     else\n"
"       dup ?echo\n"
"       >r rot r> over c! 1+ -rot swap 1+ swap\n"
"     then\n"
"   repeat drop nip ;\n"
"200 constant input-limit\n"
": tib ( -- a ) 'tib @ ;\n"
"create input-buffer   input-limit allot\n"
": tib-setup   input-buffer 'tib ! ;\n"
": refill   tib-setup tib input-limit accept #tib ! 0 >in ! -1 ;\n"
"\n"
"( REPL )\n"
": prompt   .\"  ok\" cr ;\n"
": evaluate-buffer   begin >in @ #tib @ < while evaluate1 repeat ;\n"
": evaluate ( a n -- ) 'tib @ >r #tib @ >r >in @ >r\n"
"                      #tib ! 'tib ! 0 >in ! evaluate-buffer\n"
"                      r> >in ! r> #tib ! r> 'tib ! ;\n"
": quit    begin ['] evaluate-buffer catch\n"
"          if 0 state ! sp0 sp! rp0 rp! .\" ERROR\" cr then\n"
"          prompt refill drop again ;\n"
": ok   .\" uEForth\" cr prompt refill drop quit ;\n"
"( Interpret time conditionals )\n"
"\n"
": DEFINED? ( \"name\" -- xt|0 )\n"
"   bl parse find state @ if aliteral then ; immediate\n"
"defer [SKIP]\n"
": [THEN] ;   : [ELSE] [SKIP] ;   : [IF] 0= if [SKIP] then ;\n"
": [SKIP]' 0 begin postpone defined? dup if\n"
"    dup ['] [IF] = if swap 1+ swap then\n"
"    dup ['] [ELSE] = if swap dup 0 <= if 2drop exit then swap then\n"
"    dup ['] [THEN] = if swap 1- dup 0< if 2drop exit then swap then\n"
"  then drop again ;\n"
"' [SKIP]' is [SKIP]\n"
"( Implement Vocabularies )\n"
"variable last-vocabulary\n"
"current @ constant forth-wordlist\n"
": forth   forth-wordlist context ! ;\n"
": vocabulary ( \"name\" ) create 0 , current @ 2 cells + , current @ @ last-vocabulary !\n"
"                        does> cell+ context ! ;\n"
": definitions   context @ current ! ;\n"
"\n"
"( Make it easy to transfer words between vocabularies )\n"
": xt-find& ( xt -- xt& ) context @ begin 2dup @ <> while @ >link& repeat nip ;\n"
": xt-hide ( xt -- ) xt-find& dup @ >link swap ! ;\n"
": xt-transfer ( xt --  ) dup xt-hide   current @ @ over >link& !   current @ ! ;\n"
": transfer ( \"name\" ) ' xt-transfer ;\n"
": }transfer ;\n"
": transfer{ begin ' dup ['] }transfer = if drop exit then xt-transfer again ;\n"
"\n"
"( Watered down versions of these )\n"
": only   forth 0 context cell+ ! ;\n"
": voc-stack-end ( -- a ) context begin dup @ while cell+ repeat ;\n"
": also   context context cell+ voc-stack-end over - 2 cells + cmove> ;\n"
": sealed   0 last-vocabulary @ >body cell+ ! ;\n"
"\n"
"( Hide some words in an internals vocabulary )\n"
"vocabulary internals   internals definitions\n"
"\n"
"( Vocabulary chain for current scope, place at the -1 position )\n"
"variable scope   scope context cell - !\n"
"\n"
"transfer{\n"
"  xt-find& xt-hide xt-transfer\n"
"  voc-stack-end forth-wordlist\n"
"  last-vocabulary\n"
"  branch 0branch donext dolit\n"
"  'context 'notfound notfound\n"
"  immediate? input-buffer ?echo ?arrow. arrow\n"
"  evaluate1 evaluate-buffer\n"
"  'sys 'heap aliteral\n"
"  leaving( )leaving leaving leaving,\n"
"  (do) (?do) (+loop)\n"
"  parse-quote digit $@ raw.s\n"
"  tib-setup input-limit\n"
"  [SKIP] [SKIP]'\n"
"}transfer\n"
"forth definitions\n"
"\n"
"( Make DOES> switch to compile mode when interpreted )\n"
"(\n"
"forth definitions internals\n"
"' does>\n"
": does>   state @ if postpone does> exit then\n"
"          ['] constant @ current @ @ dup >r !\n"
"          here r> cell+ ! postpone ] ; immediate\n"
"xt-hide\n"
"forth definitions\n"
")\n"
"( Cooperative Tasks )\n"
"\n"
"vocabulary tasks   tasks definitions\n"
"\n"
"variable task-list\n"
"\n"
"forth definitions tasks also internals\n"
"\n"
": pause\n"
"  rp@ sp@ task-list @ cell+ !\n"
"  task-list @ @ task-list !\n"
"  task-list @ cell+ @ sp! rp!\n"
";\n"
"\n"
": task ( xt dsz rsz \"name\" )\n"
"   create here >r 0 , 0 , ( link, sp )\n"
"   swap here cell+ r@ cell+ ! cells allot\n"
"   here r@ cell+ @ ! cells allot\n"
"   dup 0= if drop else\n"
"     here r@ cell+ @ @ ! ( set rp to point here )\n"
"     , postpone pause ['] branch , here 3 cells - ,\n"
"   then rdrop ;\n"
"\n"
": start-task ( t -- )\n"
"   task-list @ if\n"
"     task-list @ @ over !\n"
"     task-list @ !\n"
"   else\n"
"     dup task-list !\n"
"     dup !\n"
"   then\n"
";\n"
"\n"
"DEFINED? ms-ticks [IF]\n"
"  : ms ( n -- ) ms-ticks >r begin pause ms-ticks r@ - over >= until rdrop drop ;\n"
"[THEN]\n"
"\n"
"tasks definitions\n"
"0 0 0 task main-task   main-task start-task\n"
"forth definitions\n"
"( Add a yielding task so pause yields )\n"
"internals definitions\n"
"transfer{ yield raw-yield }transfer\n"
"' raw-yield 100 100 task yield-task\n"
"yield-task start-task\n"
"forth definitions\n"
"\n"
"( Set up Basic I/O )\n"
"internals definitions\n"
": esp32-bye   0 terminate ;\n"
"' esp32-bye is bye\n"
": serial-type ( a n -- ) Serial.write drop ;\n"
"' serial-type is type\n"
": serial-key ( -- n )\n"
"   begin pause Serial.available until 0 >r rp@ 1 Serial.readBytes drop r> ;\n"
"' serial-key is key\n"
": serial-key? ( -- n ) Serial.available ;\n"
"' serial-key? is key?\n"
"forth definitions\n"
"\n"
"( Map Arduino / ESP32 things to shorter names. )\n"
": pin ( n pin# -- ) swap digitalWrite ;\n"
": adc ( n -- n ) analogRead ;\n"
": duty ( n n -- ) 255 min 8191 255 */ ledcWrite ;\n"
": freq ( n n -- ) 1000 * 13 ledcSetup drop ;\n"
": tone ( n n -- ) 1000 * ledcWriteTone drop ;\n"
"\n"
"( Utilities )\n"
": page   30 for cr next ;\n"
"\n"
"( Basic Ardiuno Constants )\n"
"0 constant LOW\n"
"1 constant HIGH\n"
"1 constant INPUT\n"
"2 constant OUTPUT\n"
"2 constant LED\n"
"\n"
"( Startup Setup )\n"
"-1 echo !\n"
"115200 Serial.begin\n"
"100 ms\n"
"-1 z\" /spiffs\" 10 SPIFFS.begin drop\n"
"led OUTPUT pinMode\n"
"high led pin\n"
"\n"
"( Setup entry )\n"
": ok   .\" ESP32forth v7.0.5 - rev 33cf8aaa6fe3e0bc4abf3e4cd5c496a3071b9171\" cr prompt refill drop quit ;\n"
"( Words with OS assist )\n"
": allocate ( n -- a ior ) malloc dup 0= ;\n"
": free ( a -- ior ) sysfree drop 0 ;\n"
": resize ( a n -- a ior ) realloc dup 0= ;\n"
"\n"
"( Migrate various words to separate vocabularies, and constants )\n"
"\n"
"vocabulary Wire   Wire definitions\n"
"transfer{\n"
"  Wire.begin Wire.setClock Wire.getClock\n"
"  Wire.setTimeout Wire.getTimeout\n"
"  Wire.lastError Wire.getErrorText\n"
"  Wire.beginTransmission Wire.endTransmission\n"
"  Wire.requestFrom Wire.writeTransmission\n"
"  Wire.readTransmission Wire.write\n"
"  Wire.available Wire.read\n"
"  Wire.peek Wire.busy Wire.flush\n"
"}transfer\n"
"forth definitions\n"
"\n"
"vocabulary WebServer   WebServer definitions\n"
"transfer{\n"
"  WebServer.arg WebServer.argi WebServer.argName\n"
"  WebServer.new WebServer.delete\n"
"  WebServer.begin WebServer.stop\n"
"  WebServer.on WebServer.hasArg\n"
"  WebServer.sendHeader WebServer.send WebServer.sendContent\n"
"  WebServer.method WebServer.handleClient\n"
"  WebServer.args WebServer.setContentLength\n"
"}transfer\n"
"forth definitions\n"
"\n"
"vocabulary WiFi   WiFi definitions\n"
"\n"
"transfer{\n"
"  WiFi.config\n"
"  WiFi.begin WiFi.disconnect\n"
"  WiFi.status\n"
"  WiFi.macAddress WiFi.localIP\n"
"  WiFi.mode\n"
"  WiFi.setTxPower WiFi.getTxPower\n"
"}transfer\n"
"\n"
"( WiFi Modes )\n"
"0 constant WIFI_MODE_NULL\n"
"1 constant WIFI_MODE_STA\n"
"2 constant WIFI_MODE_AP\n"
"3 constant WIFI_MODE_APSTA\n"
"\n"
"forth definitions\n"
"\n"
"vocabulary SD_MMC   SD_MMC definitions\n"
"( SD_MMC.begin - TODO: causing issues pulled in )\n"
"transfer{\n"
"  SD_MMC.cardType\n"
"  SD_MMC.end\n"
"  SD_MMC.totalBytes SD_MMC.usedBytes\n"
"}transfer\n"
"forth definitions\n"
"\n"
"vocabulary SPIFFS   SPIFFS definitions\n"
"transfer{\n"
"  SPIFFS.begin SPIFFS.end\n"
"  SPIFFS.format\n"
"  SPIFFS.totalBytes SPIFFS.usedBytes\n"
"}transfer\n"
"forth definitions\n"
"\n"
"vocabulary ledc  ledc definitions\n"
"transfer{\n"
"  ledcSetup ledcAttachPin ledcDetachPin\n"
"  ledcRead ledcReadFreq\n"
"  ledcWrite ledcWriteTone ledcWriteNote\n"
"}transfer\n"
"forth definitions\n"
"\n"
"vocabulary Serial   Serial definitions\n"
"transfer{\n"
"  Serial.begin Serial.end\n"
"  Serial.available Serial.readBytes\n"
"  Serial.write Serial.flush\n"
"}transfer\n"
"forth definitions\n"
"\n"
"vocabulary sockets   sockets definitions\n"
"transfer{\n"
"  socket bind listen connect accept select poll errno\n"
"}transfer\n"
"1 constant SOCK_STREAM\n"
"2 constant AF_INET\n"
"16 constant sizeof(sockaddr_in)\n"
"forth definitions\n"
"\n"
"vocabulary interrupts   interrupts definitions\n"
"transfer{\n"
"  gpio_config\n"
"  gpio_reset_pin gpio_set_intr_type\n"
"  gpio_intr_enable gpio_intr_disable\n"
"  gpio_set_level gpio_get_level\n"
"  gpio_set_direction\n"
"  gpio_set_pull_mode\n"
"  gpio_wakeup_enable gpio_wakeup_disable\n"
"  gpio_pullup_en gpio_pullup_dis\n"
"  gpio_pulldown_en gpio_pulldown_dis\n"
"  gpio_hold_en gpio_hold_dis\n"
"  gpio_deep_sleep_hold_en gpio_deep_sleep_hold_dis\n"
"  gpio_install_isr_service gpio_uninstall_isr_service\n"
"  gpio_isr_handler_add gpio_isr_handler_remove\n"
"  gpio_set_drive_capability gpio_get_drive_capability\n"
"  esp_intr_alloc esp_intr_free\n"
"}transfer\n"
"\n"
"0 constant ESP_INTR_FLAG_DEFAULT\n"
": ESP_INTR_FLAG_LEVELn ( n=1-6 -- n ) 1 swap lshift ;\n"
"1 7 lshift constant ESP_INTR_FLAG_NMI\n"
"1 8 lshift constant ESP_INTR_FLAG_SHARED\n"
"1 9 lshift constant ESP_INTR_FLAG_EDGE\n"
"1 10 lshift constant ESP_INTR_FLAG_IRAM\n"
"1 11 lshift constant ESP_INTR_FLAG_INTRDISABLED\n"
"\n"
"0 constant GPIO_INTR_DISABLE\n"
"1 constant GPIO_INTR_POSEDGE\n"
"2 constant GPIO_INTR_NEGEDGE\n"
"3 constant GPIO_INTR_ANYEDGE\n"
"4 constant GPIO_INTR_LOW_LEVEL\n"
"5 constant GPIO_INTR_HIGH_LEVEL\n"
"\n"
"( Easy word to trigger on any change to a pin )\n"
"ESP_INTR_FLAG_DEFAULT gpio_install_isr_service drop\n"
": pinchange ( xt pin ) dup GPIO_INTR_ANYEDGE gpio_set_intr_type throw\n"
"                       swap 0 gpio_isr_handler_add throw ;\n"
"\n"
"forth definitions\n"
"\n"
"vocabulary rtos   rtos definitions\n"
"transfer{\n"
"  xPortGetCoreID xTaskCreatePinnedToCore vTaskDelete\n"
"}transfer\n"
"forth definitions\n"
"\n"
"DEFINED? SerialBT.new [IF]\n"
"vocabulary bluetooth   bluetooth definitions\n"
"transfer{\n"
"  SerialBT.new SerialBT.delete SerialBT.begin SerialBT.end\n"
"  SerialBT.available SerialBT.readBytes SerialBT.write\n"
"  SerialBT.flush SerialBT.hasClient\n"
"  SerialBT.enableSSP SerialBT.setPin SerialBT.unpairDevice\n"
"  SerialBT.connect SerialBT.connectAddr SerialBT.disconnect SerialBT.connected\n"
"  SerialBT.isReady esp_bt_dev_get_address\n"
"}transfer\n"
"forth definitions\n"
"[THEN]\n"
"\n"
"DEFINED? OledNew [IF]\n"
"vocabulary oled   oled definitions\n"
"transfer{\n"
"  OledNew OledDelete\n"
"  OledHOME OledCLS\n"
"  OledTextc OledPrintln OledNumln OledNum\n"
"  OledDisplay OledPrint\n"
"  OledInvert OledTextsize OledSetCursor\n"
"  OledPixel OledDrawL OledCirc OledCircF\n"
"  OledRect OledRectF OledRectR OledRectrf\n"
"}transfer\n"
"\n"
"128 constant WIDTH\n"
"64 constant HEIGHT\n"
"-1 constant OledReset\n"
"0 constant BLACK\n"
"1 constant WHITE\n"
"1 constant SSD1306_EXTERNALVCC\n"
"2 constant SSD1306_SWITCHCAPVCC\n"
": OledInit\n"
"  OledAddr @ 0= if\n"
"    WIDTH HEIGHT OledReset OledNew\n"
"    SSD1306_SWITCHCAPVCC $3C OledBegin drop\n"
"  then\n"
"  OledCLS\n"
"  2 OledTextsize  ( Draw 2x Scale Text )\n"
"  WHITE OledTextc  ( Draw white text )\n"
"  0 0 OledSetCursor  ( Start at top-left corner )\n"
"  z\" Esp32forth\" OledPrintln OledDisplay\n"
";\n"
"forth definitions\n"
"[THEN]\n"
"\n"
"internals definitions\n"
"transfer{\n"
"  malloc sysfree realloc\n"
"  heap_caps_malloc heap_caps_free heap_caps_realloc\n"
"}transfer\n"
"\n"
"( Heap Capabilities )\n"
"binary\n"
"0001 constant MALLOC_CAP_EXEC\n"
"0010 constant MALLOC_CAP_32BIT\n"
"0100 constant MALLOC_CAP_8BIT\n"
"1000 constant MALLOC_CAP_DMA\n"
": MALLOC_CAP_PID ( n -- ) 10000 over 11 ( 3 ) - for 2* next ;\n"
"000010000000000 constant MALLOC_CAP_SPIRAM\n"
"000100000000000 constant MALLOC_CAP_INTERNAL\n"
"001000000000000 constant MALLOC_CAP_DEFAULT\n"
"010000000000000 constant MALLOC_CAP_IRAM_8BIT\n"
"010000000000000 constant MALLOC_CAP_RETENTION\n"
"decimal\n"
"forth definitions\n"
"\n"
"( Including Files )\n"
": included ( a n -- )\n"
"   r/o open-file dup if nip throw else drop then\n"
"   dup file-size throw\n"
"   dup allocate throw\n"
"   swap 2dup >r >r\n"
"   rot dup >r read-file throw drop\n"
"   r> close-file throw\n"
"   r> r> over >r evaluate\n"
"   r> free throw ;\n"
": include ( \"name\" -- ) bl parse included ; \n"
": dump-file ( a n a n -- )\n"
"  w/o create-file if drop .\" failed create-file\" exit then\n"
"  >r r@ write-file if r> drop .\" failed write-file\" exit then\n"
"  r> close-file drop\n"
";\n"
"\n"
"internals definitions\n"
"( Leave some room for growth of starting system. )\n"
"$4000 constant growth-gap\n"
"here growth-gap + growth-gap 1- + growth-gap 1- invert and constant saving-base\n"
": park-heap ( -- a ) saving-base ;\n"
": park-forth ( -- a ) saving-base cell+ ;\n"
": 'cold ( -- a ) saving-base 2 cells + ;   0 'cold !\n"
"\n"
": save-name\n"
"  'heap @ park-heap !\n"
"  forth-wordlist @ park-forth !\n"
"  w/o create-file throw >r\n"
"  saving-base here over - r@ write-file throw\n"
"  r> close-file throw ;\n"
"\n"
": restore-name ( \"name\" -- )\n"
"  r/o open-file throw >r\n"
"  saving-base r@ file-size throw r@ read-file throw drop\n"
"  r> close-file throw\n"
"  park-heap @ 'heap !\n"
"  park-forth @ forth-wordlist !\n"
"  'cold @ dup if execute else drop then ;\n"
"\n"
"defer remember-filename\n"
": default-remember-filename   s\" myforth\" ;\n"
"' default-remember-filename is remember-filename\n"
"\n"
"forth definitions also internals\n"
"\n"
": save ( \"name\" -- ) bl parse save-name ;\n"
": restore ( \"name\" -- ) bl parse restore-name ;\n"
": remember   remember-filename save-name ;\n"
": startup: ( \"name\" ) ' 'cold ! remember ;\n"
": revive   remember-filename restore-name ;\n"
": reset   remember-filename delete-file throw ;\n"
"\n"
"only forth definitions\n"
"( Words built after boot )\n"
"\n"
"( For tests and asserts )\n"
": assert ( f -- ) 0= throw ;\n"
"\n"
"( Examine Memory )\n"
": dump ( a n -- )\n"
"   cr 0 do i 16 mod 0= if cr then dup i + c@ . loop drop cr ;\n"
"\n"
"( Remove from Dictionary )\n"
": forget ( \"name\" ) ' dup >link current @ !  >name drop here - allot ;\n"
"\n"
"2 constant SMUDGE\n"
": :noname ( -- xt ) 0 , current @ @ , SMUDGE , here dup current @ ! ['] = @ , postpone ] ;\n"
"\n"
"internals definitions\n"
": mem= ( a a n -- f)\n"
"   for aft 2dup c@ swap c@ <> if 2drop rdrop 0 exit then 1+ swap 1+ then next 2drop -1 ;\n"
"forth definitions also internals\n"
": str= ( a n a n -- f) >r swap r@ <> if rdrop 2drop 0 exit then r> mem= ;\n"
": startswith? ( a n a n -- f ) >r swap r@ < if rdrop 2drop 0 exit then r> mem= ;\n"
": .s   .\" <\" depth n. .\" > \" raw.s cr ;\n"
"only forth definitions\n"
"\n"
"( Definitions building to SEE and ORDER )\n"
"internals definitions\n"
": see. ( xt -- ) >name type space ;\n"
": see-one ( xt -- xt+1 )\n"
"   dup cell+ swap @\n"
"   dup ['] DOLIT = if drop dup @ . cell+ exit then\n"
"   dup ['] $@ = if drop ['] s\" see.\n"
"                   dup @ dup >r >r dup cell+ r> type cell+ r> aligned +\n"
"                   [char] \" emit space exit then\n"
"   dup  ['] BRANCH =\n"
"   over ['] 0BRANCH = or\n"
"   over ['] DONEXT = or\n"
"       if see. cell+ exit then\n"
"   see. ;\n"
": exit= ( xt -- ) ['] exit = ;\n"
": see-loop   >body begin dup @ exit= 0= while see-one repeat drop ;\n"
": see-xt ( xt -- )\n"
"        dup @ ['] see-loop @ <>\n"
"        if .\" Unsupported word type: \" see. cr exit then\n"
"        ['] : see.  dup see.  space see-loop   ['] ; see. cr ;\n"
": see-all   0 context @ @ begin dup while dup see-xt >link repeat 2drop cr ;\n"
": voc. ( voc -- ) dup forth-wordlist = if .\" FORTH \" drop exit then 3 cells - see. ;\n"
"forth definitions also internals\n"
": see   ' see-xt ;\n"
": order   context begin dup @ while dup @ voc. cell+ repeat drop cr ;\n"
"only forth definitions\n"
"\n"
"( List words in Dictionary / Vocabulary )\n"
"internals definitions\n"
"75 value line-width\n"
": onlines ( n xt -- n xt )\n"
"   swap dup line-width > if drop 0 cr then over >name nip + 1+ swap ;\n"
": >name-length ( xt -- n ) dup 0= if exit then >name nip ;\n"
"forth definitions also internals\n"
": vlist   0 context @ @ begin dup >name-length while onlines dup see. >link repeat 2drop cr ;\n"
": words   0 context @ @ begin dup while onlines dup see. >link repeat 2drop cr ;\n"
"only forth definitions\n"
"\n"
"( Extra Task Utils )\n"
"tasks definitions also internals\n"
": .tasks   task-list @ begin dup 2 cells - see. @ dup task-list @ = until drop ;\n"
"only forth definitions\n"
"( Local Variables )\n"
"\n"
"( NOTE: These are not yet gforth compatible )\n"
"\n"
"internals definitions\n"
"\n"
"variable scope-depth\n"
": scope-doer   create does> @ rp@ + @ ;\n"
"scope-doer scope-template\n"
": scope-clear\n"
"   begin scope-depth @ while postpone rdrop cell scope-depth +! repeat\n"
"   0 scope ! ;\n"
": scope-create ( a n -- )\n"
"   dup >r $place r> , ( name )\n"
"   scope @ , 0 , here scope ! ( link, flags )\n"
"   ['] scope-template dup @ , cell+ @ ,\n"
"   cell negate scope-depth +!   scope-depth @ , ;\n"
"\n"
"( NOTE: This is not ANSForth compatible )\n"
": (local) ( a n -- )\n"
"   >r >r postpone >r postpone ahead r> r> scope-create postpone then ;\n"
": }? ( a n -- ) 1 <> if drop 0 exit then c@ [char] } = ;\n"
"\n"
"also forth definitions\n"
"\n"
": {   begin bl parse 2dup }? if 2drop exit then (local) again ; immediate\n"
": ;   scope-clear postpone ; ; immediate\n"
"\n"
"only forth definitions\n"
"( Byte Stream / Ring Buffer )\n"
"\n"
"vocabulary streams   streams definitions\n"
"\n"
": stream ( n \"name\" ) create 1+ dup , 0 , 0 , allot align ;\n"
": >write ( st -- wr ) cell+ ;   : >read ( st -- rd ) 2 cells + ;\n"
": >offset ( n st -- a ) 3 cells + + ;\n"
": stream# ( sz -- n ) >r r@ >write @ r@ >read @ - r> @ mod ;\n"
": full? ( st -- f ) dup stream# swap @ 1- = ;\n"
": empty? ( st -- f ) stream# 0= ;\n"
": wait-write ( st -- ) begin dup full? while pause repeat drop ;\n"
": wait-read ( st -- ) begin dup empty? while pause repeat drop ;\n"
": ch>stream ( ch st -- )\n"
"   dup wait-write\n"
"   >r r@ >write @ r@ >offset c!\n"
"   r@ >write @ 1+ r@ @ mod r> >write ! ;\n"
": stream>ch ( st -- ch )\n"
"   dup wait-read\n"
"   >r r@ >read @ r@ >offset c@\n"
"   r@ >read @ 1+ r@ @ mod r> >read ! ;\n"
": >stream ( a n st -- )\n"
"   swap 0 do over c@ over ch>stream swap 1+ swap loop 2drop ;\n"
": stream> ( a n st -- )\n"
"   begin over 1 > over empty? 0= and while\n"
"   dup stream>ch >r rot dup r> swap c! 1+ rot 1- rot repeat 2drop 0 swap c! ;\n"
"\n"
"forth definitions\n"
"( Server Terminal )\n"
"\n"
"also streams also WebServer also WiFi\n"
"vocabulary web-interface   also web-interface definitions\n"
"\n"
": ip# dup 255 and n. [char] . emit 256 / ;\n"
": ip. ( n -- ) ip# ip# ip# 255 and . ;\n"
"\n"
"r|\n"
"<!html>\n"
"<head>\n"
"<title>esp32forth</title>\n"
"<style>\n"
"body {\n"
"  padding: 5px;\n"
"  background-color: #111;\n"
"  color: #2cf;\n"
"  overflow: hidden;\n"
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
"  overflow-y: scroll;\n"
"  word-break: break-all;\n"
"}\n"
"</style>\n"
"<link rel=\"icon\" href=\"data:,\">\n"
"</head>\n"
"<body>\n"
"<h2>ESP32forth v7</h2>\n"
"Upload File: <input id=\"filepick\" type=\"file\" name=\"files[]\"></input><br/>\n"
"<button onclick=\"ask('hex')\">hex</button>\n"
"<button onclick=\"ask('decimal')\">decimal</button>\n"
"<button onclick=\"ask('words')\">words</button>\n"
"<button onclick=\"ask('low led pin')\">LED OFF</button>\n"
"<button onclick=\"ask('high led pin')\">LED ON</button>\n"
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
"      var parts = e.target.result.replace(/[\\r]/g, '').split('\\n');\n"
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
"| s>z constant index-html\n"
"\n"
"variable webserver\n"
"20000 constant out-size\n"
"200 stream input-stream\n"
"out-size stream output-stream\n"
"create out-string out-size 1+ allot align\n"
"\n"
": handle-index\n"
"   index-html z>s nip webserver @ WebServer.setContentLength\n"
"   200 z\" text/html\" index-html webserver @ WebServer.send\n"
";\n"
"\n"
": handle-input\n"
"   z\" cmd\" webserver @ WebServer.hasArg if\n"
"     z\" cmd\" webserver @ WebServer.arg input-stream >stream pause\n"
"     out-string out-size output-stream stream>\n"
"     200 z\" text/plain\" out-string webserver @ WebServer.send\n"
"   else\n"
"     500 z\" text/plain\" z\" Missing Input\" webserver @ WebServer.send\n"
"   then\n"
";\n"
"\n"
": serve-type ( a n -- ) output-stream >stream ;\n"
": serve-key ( -- n ) input-stream stream>ch ;\n"
"\n"
": do-serve\n"
"   80 WebServer.new webserver !\n"
"   z\" /\" ['] handle-index webserver @ WebServer.on\n"
"   z\" /input\" ['] handle-input webserver @ WebServer.on\n"
"   webserver @ WebServer.begin\n"
"   begin\n"
"     webserver @ WebServer.handleClient\n"
"     pause\n"
"   again\n"
";\n"
"\n"
"' do-serve 1000 1000 task webserver-task\n"
"\n"
": serve\n"
"   ['] serve-type is type\n"
"   ['] serve-key is key\n"
"   webserver-task start-task\n"
";\n"
"\n"
"also forth definitions\n"
"\n"
": login ( z z -- )\n"
"   WIFI_MODE_STA Wifi.mode\n"
"   WiFi.begin begin WiFi.localIP 0= while 100 ms repeat WiFi.localIP ip. cr\n"
"   z\" forth\" MDNS.begin if .\" MDNS started\" else .\" MDNS failed\" then cr ;\n"
": webui ( z z -- ) login serve ;\n"
"\n"
"only forth definitions\n"
"vocabulary registers   registers definitions\n"
"\n"
"( Tools for working with bit masks )\n"
": m! ( val shift mask a -- )\n"
"   dup >r @ over invert and >r >r lshift r> and r> or r> ! ;\n"
": m@ ( shift mask a -- val ) @ and swap rshift ;\n"
"\n"
"only forth definitions\n"
"vocabulary timers   timers definitions   also registers also interrupts\n"
"\n"
"$3ff5f000 constant TIMG_BASE\n"
"( group n = 0/1, timer x = 0/1, watchdog m = 0-5 )\n"
": TIMGn ( n -- a ) $10000 * TIMG_BASE + ;\n"
": TIMGn_Tx ( n x -- a ) $24 * swap TIMGn + ;\n"
": TIMGn_TxCONFIG_REG ( n x -- a ) TIMGn_Tx 0 cells + ;\n"
": TIMGn_TxLOHI_REG ( n x -- a ) TIMGn_Tx 1 cells + ;\n"
": TIMGn_TxUPDATE_REG ( n x -- a ) TIMGn_Tx 3 cells + ;\n"
": TIMGn_TxALARMLOHI_REG ( n x -- a ) TIMGn_Tx 4 cells + ;\n"
": TIMGn_TxLOADLOHI_REG ( n x -- a ) TIMGn_Tx 6 cells + ;\n"
": TIMGn_TxLOAD_REG ( n x -- a ) TIMGn_Tx 8 cells + ;\n"
"\n"
": TIMGn_Tx_WDTCONFIGm_REG ( n m -- a ) swap TIMGn cells + $48 + ;\n"
": TIMGn_Tx_WDTFEED_REG ( n -- a ) TIMGn $60 + ;\n"
": TIMGn_Tx_WDTWPROTECT_REG ( n -- a ) TIMGn $6c + ;\n"
"\n"
": TIMGn_RTCCALICFG_REG ( n -- a ) TIMGn $68 + ;\n"
": TIMGn_RTCCALICFG1_REG ( n -- a ) TIMGn $6c + ;\n"
"\n"
": TIMGn_Tx_INT_ENA_REG ( n -- a ) TIMGn $98 + ;\n"
": TIMGn_Tx_INT_RAW_REG ( n -- a ) TIMGn $9c + ;\n"
": TIMGn_Tx_INT_ST_REG ( n -- a ) TIMGn $a0 + ;\n"
": TIMGn_Tx_INT_CLR_REG ( n -- a ) TIMGn $a4 + ;\n"
"\n"
": t>nx ( t -- n x ) dup 2/ 1 and swap 1 and ;\n"
"\n"
": timer@ ( t -- lo hi )\n"
"   dup t>nx TIMGn_TxUPDATE_REG 0 swap !\n"
"       t>nx TIMGn_TxLOHI_REG 2@ ;\n"
": timer! ( lo hi t -- )\n"
"   dup >r t>nx TIMGn_TxLOADLOHI_REG 2!\n"
"       r> t>nx TIMGn_TxLOAD_REG 0 swap ! ;\n"
": alarm ( t -- a ) t>nx TIMGn_TxALARMLOHI_REG ;\n"
"\n"
": enable! ( v t ) >r 31 $80000000 r> t>nx TIMGn_TxCONFIG_REG m! ;\n"
": increase! ( v t ) >r 30 $40000000 r> t>nx TIMGn_TxCONFIG_REG m! ;\n"
": autoreload! ( v t ) >r 29 $20000000 r> t>nx TIMGn_TxCONFIG_REG m! ;\n"
": divider! ( v t ) >r 13 $1fffc000 r> t>nx TIMGn_TxCONFIG_REG m! ;\n"
": edgeint! ( v t ) >r 12 $1000 r> t>nx TIMGn_TxCONFIG_REG m! ;\n"
": levelint! ( v t ) >r 11 $800 r> t>nx TIMGn_TxCONFIG_REG m! ;\n"
": alarm-enable! ( v t ) >r 10 $400 r> t>nx TIMGn_TxCONFIG_REG m! ;\n"
": alarm-enable@ ( v t ) >r 10 $400 r> t>nx TIMGn_TxCONFIG_REG m@ ;\n"
"\n"
": int-enable! ( f t -- )\n"
"   t>nx swap >r dup 1 swap lshift r> TIMGn_Tx_INT_ENA_REG m! ;\n"
"\n"
": onalarm ( xt t ) swap >r t>nx r> 0 ESP_INTR_FLAG_EDGE 0\n"
"                   timer_isr_register throw ;\n"
": interval ( xt usec t ) 80 over divider!\n"
"                         swap over 0 swap alarm 2!\n"
"                         1 over increase!\n"
"                         1 over autoreload!\n"
"                         1 over alarm-enable!\n"
"                         1 over edgeint!\n"
"                         0 over 0 swap timer!\n"
"                         dup >r onalarm r>\n"
"                         1 swap enable! ;\n"
": rerun ( t -- ) 1 swap alarm-enable! ;\n"
"\n"
"only forth definitions\n"
"( Lazy loaded Bluetooth Serial Terminal )\n"
"\n"
": bterm r|\n"
"vocabulary bterm  bterm definitions\n"
"also bluetooth also internals\n"
"SerialBT.new constant bt\n"
"z\" forth\" 0 bt SerialBT.begin drop\n"
"esp_bt_dev_get_address hex 6 dump cr\n"
": bt-type bt SerialBT.write drop ;\n"
": bt-key\n"
"   begin bt SerialBT.available until 0 >r rp@ 1 bt SerialBT.readBytes drop r> ;\n"
": bt-on ['] bt-type is type ['] bt-key is key ;\n"
": bt-off ['] serial-type is type ['] serial-key is key ;\n"
"only forth definitions\n"
"bterm 500 ms bt-on\n"
"| evaluate ;\n"
"( Telnet )\n"
"vocabulary telnetd   telnetd definitions also sockets also internals\n"
"\n"
"23 constant port\n"
"-1 value sockfd   -1 value clientfd\n"
": bs, ( n -- ) dup 256 / c, c, ;\n"
": s, ( n -- ) dup c, 256 / c, ;\n"
": l, ( n -- ) dup s, 65536 / s, ;\n"
"create telnet-port  16 c, AF_INET c, port bs, 0 l, 0 l, 0 l,\n"
"create client   sizeof(sockaddr_in) allot   variable client-len\n"
"\n"
"defer broker\n"
"\n"
": telnet-emit' ( ch -- ) >r rp@ 1 clientfd write-file rdrop if broker then ;\n"
": telnet-emit ( ch -- ) dup nl = if 13 telnet-emit' then telnet-emit' ;\n"
": telnet-type ( a n -- ) for aft dup c@ telnet-emit 1+ then next drop ;\n"
": telnet-key ( -- n ) 0 >r rp@ 1 clientfd read-file if drop rdrop broker else drop then r> ;\n"
"\n"
": connection ( n -- )\n"
"  dup 0< if drop exit then to clientfd\n"
"  0 echo !\n"
"  ['] telnet-key is key\n"
"  ['] telnet-type is type quit ;\n"
"\n"
": broker-connection\n"
"  rp0 rp! sp0 sp!\n"
"  begin\n"
"    ['] serial-key is key\n"
"    ['] serial-type is type\n"
"    -1 echo !\n"
"    .\" Listening on port \" port . cr\n"
"    sockfd client client-len accept\n"
"    .\" Connected: \" dup . cr connection\n"
"  again ;\n"
"' broker-connection is broker\n"
"\n"
": server\n"
"  AF_INET SOCK_STREAM 0 socket to sockfd\n"
"  sockfd telnet-port sizeof(sockaddr_in) bind throw\n"
"  sockfd 10 listen throw   broker ;\n"
"\n"
"only forth definitions\n"
"( Handling for ESP32-CAM )\n"
"\n"
"DEFINED? esp_camera_init [IF]\n"
"\n"
"vocabulary camera   camera definitions\n"
"\n"
"transfer{\n"
"  esp_camera_init esp_camera_deinit\n"
"  esp_camera_fb_get esp_camera_fb_return\n"
"  esp_camera_sensor_get\n"
"}transfer\n"
"\n"
"0 constant PIXFORMAT_RGB565\n"
"1 constant PIXFORMAT_YUV422\n"
"2 constant PIXFORMAT_GRAYSCALE\n"
"3 constant PIXFORMAT_JPEG\n"
"4 constant PIXFORMAT_RGB888\n"
"5 constant PIXFORMAT_RAW\n"
"6 constant PIXFORMAT_RGB444\n"
"7 constant PIXFORMAT_RGB555\n"
"\n"
"5 constant FRAMESIZE_QVGA\n"
"8 constant FRAMESIZE_VGA\n"
"\n"
"( See https://github.com/espressif/esp32-camera/blob/master/driver/include/esp_camera.h )\n"
"( Settings for AI_THINKER )\n"
"create camera-config\n"
"  32 , ( pin_pwdn ) -1 , ( pin_reset ) 0 , ( pin_xclk )\n"
"  26 , ( pin_sscb_sda ) 27 , ( pin_sscb_scl )\n"
"  35 , 34 , 39 , 36 , 21 , 19 , 18 , 5 , ( pin_d7 - pin_d0 )\n"
"  25 , ( pin_vsync ) 23 , ( pin_href ) 22 , ( pin_pclk )\n"
"  20000000 , ( xclk_freq_hz )\n"
"  0 , ( ledc_timer ) 0 , ( ledc_channel )\n"
"  here\n"
"  PIXFORMAT_JPEG , ( pixel_format )\n"
"  FRAMESIZE_VGA , ( frame_size ) 12 , ( jpeg_quality 0-63 low good )\n"
"  here\n"
"  1 , ( fb_count )\n"
"constant camera-fb-count\n"
"constant camera-format\n"
"\n"
"forth definitions\n"
"\n"
"[THEN]\n"
"( Block Files )\n"
"internals definitions\n"
": clobber-line ( a -- a' ) dup 63 bl fill 63 + nl over c! 1+ ;\n"
": clobber ( a -- ) 15 for clobber-line next drop ;\n"
"0 value block-dirty\n"
"create block-data 1024 allot\n"
"forth definitions internals\n"
"\n"
"-1 value block-fid   variable scr   -1 value block-id\n"
": open-blocks ( a n -- )\n"
"   block-fid 0< 0= if block-fid close-file throw -1 to block-fid then\n"
"   2dup r/w open-file if drop r/w create-file throw else nip nip then to block-fid ;\n"
": use ( \"name\" -- ) bl parse open-blocks ;\n"
"defer default-use\n"
"internals definitions\n"
": common-default-use s\" blocks.fb\" open-blocks ;\n"
"' common-default-use is default-use\n"
": use?!   block-fid 0< if default-use then ;\n"
": grow-blocks ( n -- ) 1024 * block-fid file-size throw max block-fid resize-file throw ;\n"
"forth definitions internals\n"
": save-buffers\n"
"   block-dirty if\n"
"     block-id grow-blocks block-id 1024 * block-fid reposition-file throw\n"
"     block-data 1024 block-fid write-file throw\n"
"     block-fid flush-file throw\n"
"     0 to block-dirty\n"
"   then ;\n"
": block ( n -- a ) use?! dup block-id = if drop block-data exit then\n"
"                   save-buffers dup grow-blocks\n"
"                   dup 1024 * block-fid reposition-file throw\n"
"                   block-data clobber\n"
"                   block-data 1024 block-fid read-file throw drop\n"
"                   to block-id block-data ;\n"
": buffer ( n -- a ) use?! dup block-id = if drop block-data exit then\n"
"                    save-buffers to block-id block-data ;\n"
": empty-buffers   -1 to block-id ;\n"
": update   -1 to block-dirty ;\n"
": flush   save-buffers empty-buffers ;\n"
"\n"
"( Loading )\n"
": load ( n -- ) block 1024 evaluate ;\n"
": thru ( a b -- ) over - 1+ for aft dup >r load r> 1+ then next drop ;\n"
"\n"
"( Utility )\n"
": copy ( from to -- )\n"
"   swap block pad 1024 cmove pad swap block 1024 cmove update ;\n"
"\n"
"( Editing )\n"
": list ( n -- ) scr ! .\" Block \" scr @ . cr scr @ block\n"
"   15 for dup 63 type [char] | emit space 15 r@ - . cr 64 + next drop ;\n"
"internals definitions\n"
": @line ( n -- ) 64 * scr @ block + ;\n"
": e' ( n -- ) @line clobber-line drop update ;\n"
"forth definitions internals\n"
"vocabulary editor   also editor definitions\n"
": l    scr @ list ;   : n    1 scr +! l ;  : p   -1 scr +! l ;\n"
": wipe   15 for r@ e' next l ;   : e   e' l ;\n"
": d ( n -- ) dup 1+ @line swap @line 15 @line over - cmove 15 e ;\n"
": r ( n \"line\" -- ) 0 parse 64 min rot dup e @line swap cmove l ;\n"
": a ( n \"line\" -- ) dup @line over 1+ @line 16 @line over - cmove> r ;\n"
"only forth definitions\n"
"internals definitions\n"
"\n"
"( Change default block source on arduino )\n"
": arduino-default-use s\" /spiffs/blocks.fb\" open-blocks ;\n"
"' arduino-default-use is default-use\n"
"\n"
"( Setup remember file )\n"
": arduino-remember-filename   s\" /spiffs/myforth\" ;\n"
"' arduino-remember-filename is remember-filename\n"
"\n"
"( Check for autoexec.fs and run if present.\n"
"  Failing that, try to revive save image. )\n"
": autoexec\n"
"   300 for key? if rdrop exit then 10 ms next\n"
"   s\" /spiffs/autoexec.fs\" ['] included catch 2drop drop\n"
"   ['] revive catch drop ;\n"
"' autoexec ( leave on the stack for fini.fs )\n"
"\n"
"forth definitions\n"
"internals\n"
"( Bring a forth to the top of the vocabulary. )\n"
"transfer forth\n"
"( Move heap to save point, with a gap. )\n"
"saving-base 16 cells + 'heap !\n"
"forth\n"
"execute ( assumes an xt for autoboot is on the dstack )\n"
"ok\n"
"\n";


// Work around lack of ftruncate
static cell_t ResizeFile(cell_t fd, cell_t size) {
  struct stat st;
  char buf[256];
  cell_t t = fstat(fd, &st);
  if (t < 0) { return errno; }
  if (size < st.st_size) {
    // TODO: Implement truncation
    return ENOSYS;
  }
  cell_t oldpos = lseek(fd, 0, SEEK_CUR);
  if (oldpos < 0) { return errno; }
  t = lseek(fd, 0, SEEK_END);
  if (t < 0) { return errno; }
  memset(buf, 0, sizeof(buf));
  while (st.st_size < size) {
    cell_t len = sizeof(buf);
    if (size - st.st_size < len) {
      len = size - st.st_size;
    }
    t = write(fd, buf, len);
    if (t != len) {
      return errno;
    }
    st.st_size += t;
  }
  t = lseek(fd, oldpos, SEEK_SET);
  if (t < 0) { return errno; }
  return 0;
}

#ifdef ENABLE_WEBSERVER_SUPPORT
static void InvokeWebServerOn(WebServer *ws, const char *url, cell_t xt) {
  ws->on(url, [xt]() {
    cell_t code[2];
    code[0] = xt;
    code[1] = g_sys.YIELD_XT;
    cell_t stack[INTERRUPT_STACK_CELLS];
    cell_t rstack[INTERRUPT_STACK_CELLS];
    cell_t *rp = rstack;
    *++rp = (cell_t) (stack + 1);
    *++rp = (cell_t) code;
    forth_run(rp);
  });
}
#endif

struct handle_interrupt_args {
  cell_t xt;
  cell_t arg;
};

static void IRAM_ATTR HandleInterrupt(void *arg) {
  struct handle_interrupt_args *args = (struct handle_interrupt_args *) arg;
  cell_t code[2];
  code[0] = args->xt;
  code[1] = g_sys.YIELD_XT;
  cell_t stack[INTERRUPT_STACK_CELLS];
  cell_t rstack[INTERRUPT_STACK_CELLS];
  stack[0] = args->arg;
  cell_t *rp = rstack;
  *++rp = (cell_t) (stack + 1);
  *++rp = (cell_t) code;
  forth_run(rp);
}

static cell_t EspIntrAlloc(cell_t source, cell_t flags, cell_t xt, cell_t arg, void *ret) {
  // NOTE: Leaks memory.
  struct handle_interrupt_args *args = (struct handle_interrupt_args *) malloc(sizeof(struct handle_interrupt_args));
  args->xt = xt;
  args->arg = arg;
  return esp_intr_alloc(source, flags, HandleInterrupt, args, (intr_handle_t *) ret);
}

static cell_t GpioIsrHandlerAdd(cell_t pin, cell_t xt, cell_t arg) {
  // NOTE: Leaks memory.
  struct handle_interrupt_args *args = (struct handle_interrupt_args *) malloc(sizeof(struct handle_interrupt_args));
  args->xt = xt;
  args->arg = arg;
  return gpio_isr_handler_add((gpio_num_t) pin, HandleInterrupt, args);
}

static cell_t TimerIsrRegister(cell_t group, cell_t timer, cell_t xt, cell_t arg, cell_t flags, void *ret) {
  // NOTE: Leaks memory.
  struct handle_interrupt_args *args = (struct handle_interrupt_args *) malloc(sizeof(struct handle_interrupt_args));
  args->xt = xt;
  args->arg = arg;
  return timer_isr_register((timer_group_t) group, (timer_idx_t) timer, HandleInterrupt, args, flags, (timer_isr_handle_t *) ret);
}

void setup() {
  cell_t *heap = (cell_t *) malloc(HEAP_SIZE);
  forth_init(0, 0, heap, boot, sizeof(boot));
}

void loop() {
  g_sys.rp = forth_run(g_sys.rp);
}
