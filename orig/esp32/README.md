\page 3 - Evolution of ESP32Forth

## ~/orig/esp32 - the origin of ESP32Forth

    + esp32/_54, _59           - cross-compiled ROM, token-threaded
    + esp32/_62, _63           - assembler with C functions as macros, from ceForth_33
    + esp32/_705               - ESP32Forth v7, C macro-based assembler with Brad and Peter
    + esp32/_802               - my interim work shown to Dr. Ting, sync ceForth_403
    + esp32/_82, _83, _84, _85 - from _63, Dr. Ting adapted array-based, token-threaded
    
### Dr. Ting's original esp32forth
  + _54
  + _59
  + _62
  + _63
  + _64
  
### Brad Nelson & Peter Forth's
  + ESP32forth705

### My original injection from ~/eforth/orig/ceforth802
  + _82 20210831~20210909

### My hack to show Dr. Ting a javascript-based Forth interact with ESP32 server
  + esp32jeforth_615 ~20210913
  
### My token call threaded shown to Dr. Ting (didn't seem to like it)
  + esp32forth8_1 ~20210919

### Dr. Ting's modification
  + _83 ~20210910
  + _84 ~20211002
  + _85 ~20211014 released to Peter Forth, Don Golding



