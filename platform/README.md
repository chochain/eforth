\page 1 - Platform Specific Code directory
will be included at the end of ~/src/ceforth.cpp

##
+ wasm.cpp - Web Assembly (Emscripten)
+ mcu.cpp  - Micro controllers (Arduino && ESP32)
+ main.cpp - Linux or Cygwin
+ server.h - ESP32 Web Server interface
+ main_mqtt_dev, main_mqtt_ctl - MQTT device and console
  Note for Android (TermUX)
  1. wget timeb.h from googlesource for Android bionic into $PREFIX/include/sys
  2. config paho.mqtt.c build with cmake -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_SAMPLES=OFF -DPAHO_BUILD_DOCUMENTATION=OFF -D -DCMAKE_INSTALL_PREFIX=$PREFIX

  

