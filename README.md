# eForth - continuation of Dr. Ting's final work

### eForth next-gen, 100% C/C++ (portable) implementation, i.e. Forth without Forth
* ~/src cross-platform, common source for eforth, esp32forth, wineForth, and weForth
* ~/orig/33b refactored 32-bit asm+vm implementation.
  > Originated from <a href="http://forth.org/OffeteStore/OffeteStore.html" target="_blank">Dr. Ting's eForth site</a>
* shared source with Dr. Ting in ~/orig/ting

### Benchmark
* Desktop PC - 10K*10K cycles on 3.2GHz AMD
   > + 1200ms - ~/esp32forth/orig/esp32forth8_1, token threading
   > +  981ms - subroutine threading, inline list methods
   > +  937ms - subroutine threading, with 16-bit offset

* ESP32 - 1K*1K cycles on NodeMCU
   > + 1440ms - Dr. Ting's ~/esp32forth/orig/esp32forth_82
   > + 1045ms - ~/esp32forth/orig/esp32forth8_1, token threading
   > +  839ms - subroutine threading, inline list methods

### To Compile on Linux and Cygwin
> g++ -O2 -Isrc -o tests/eforth src/ceforth.cpp

### To Run on Linux
> ./tests/eforth

### TODO
* WASM - review wasmtime (CLI), perf+hotspot (profiling)

### Revision History
* Dr. Ting's work on eForth between 1995~2011
  <a href="http://forth.org/library/eforth_SOC" target="_blank">[eForth references]</a> and their <a href="http://forth.org/library/eforth_SOC/eforth_SOC_source" target="_blank">[Source Code Repo]</a>
* CC 20210314: Init
  > Started with ~orig/33b code-base, refactor with enum and VA_ARGS macros targeting 100% C/C++.
* CC 20210707: Refactor
  > Incorporated list-based dict, ss, rs (i.e. ~orig/ting/ceForth40 and ~orig/802) which I proposed to Dr. Ting in our email exchanges.
* CC 20210816: Code Merge
  > Targeting multi-platform. Common source by consolidating ceForth, wineForth, ESP32forth (kept in ~/orig/*). Offically version 8.0
* CC 20220512: Refactor
  >  Though the goal of Dr. Ting's eForth is to demonstrate how a Forth can be easily understood and cleanly constructed. However, the token threading method used is costly (slow) because each call needs 2 indirect lookups (token->dict, dict->xt). On top of that, C/C++ callframe needs to be setup/teardown. It is worsen by the branch prediction missing every call stalling the CPU pipeline. Bad stuffs!
  > Refactor to subroutine threading. It's not portable but does speed up (see benchmark above). Using 16-bit offsets for pointer arithmatics in order to speed up while maintaining 16-bit parameter space consumption. Since C++ code is at least 4-byte aligned and parameter is 2-byte aligned, the LSB of a given parameter is utilized for colon word identification.
* CC 20221118: Refactor
  > WASM function pointer is U32 (index). Token-indirect worked but the two indirect look-up is even slower. Since WASM uses 64K linear memory block, 16-bit pointer offset is a better option. However, the 32-bit xt function pointer in code space is not aligned, with or without LAMBDA_OK option. So, we are forced to use MSB for parameter field (primitive word pointer and colon pfa), there are only 16K offset.
* CC 20231011: Review
  > Since the original intention of having a pre-compiled ROM dictionary still end up in C++ static initialization run before main(), moved dictionary compilation into vm_init as function calls gives a little more debugging control and opportunity for fine tuning.

