# eforth 
### next generation of ceForth
* ~/src common source for esp32forth and wineForth
* ~/orig/33b refactored 32-bit asm+vm implementation. Originated from Dr. Ting's eForth site http://forth.org/OffeteStore/OffeteStore.html
* shared source with Dr. Ting in ~/orig/ting

### Compile
#### Linux
> g++ -Isrc -o tests/eforth src/ceforth.cpp
