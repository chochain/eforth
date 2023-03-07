# eForth - continuation of Dr. Ting's final work

### next generation of ceForth
* ~/src common source for esp32forth and wineForth
* ~/orig/33b refactored 32-bit asm+vm implementation.
  > Originated from <a href="http://forth.org/OffeteStore/OffeteStore.html" target="_blank">Dr. Ting's eForth site</a>
* shared source with Dr. Ting in ~/orig/ting

### Benchmark
* 10K*10K cycles on desktop (3.2GHz AMD)
   > + 1200ms - ~/esp32forth/orig/esp32forth8_1, token call threading
   > +  981ms - call threading, inline list methods

* 1K*1K cycles on NodeMCU ESP32S
   > + 1440ms - Dr. Ting's ~/esp32forth/orig/esp32forth_82
   > + 1045ms - ~/esp32forth/orig/esp32forth8_1, token call threading
   > +  839ms - call threading, inline list methods

### Compile
#### Linux
> g++ -O3 -Isrc -o tests/eforth src/ceforth.cpp

###

### Version History
* Dr. Ting's work on eForth between 1995~2011
  <a href="http://forth.org/library/eforth_SOC" target="_blank">[eForth references]</a> and their <a href="http://forth.org/library/eforth_SOC/eforth_SOC_source" target="_blank">[Source Code Repo]</a>
* CC 20220512:
  >  Though the goal of eForth is to show how a Forth can be easily understood and cleanly constructed. However, the threading method used is very inefficient (slow) because each call needs 2 indirect lookups (token->dict, dict->xt) and a callframe needs to be setup/teardown. Plus, every call will miss the branch prediction stalling CPU pipeline. Bad stuffs!
* CC 20220514:
  > Refactor to call threading (not portable). Using 16-bit offsets for pointer arithmatics in order to speed up while maintaining space consumption. Refactor to subroutine indirect threading. Using 16-bit offsets for pointer arithmatics in order to speed up while maintaining space consumption
* CC 20221118
  > WASM function pointer is U32 (index). So, internally, it brought us back to token-indirect again. Though enabling LAMBDA_OK, the anonymous struct is created with 32-bit offset (as an object), it adds two extra indirect look-up. We are back to square one. So, by disabling LAMBDA_OK, xt is a 32-bit index, we use lower 14 bits (16K entries) to identify word while keeping U16 pfa (parameter field) with 64K linear memory block.

