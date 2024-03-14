# Forth - is it still relavant?
With all the advantages, it is unfortunate that Forth lost out to C language over the years and have become a niche. Per ChatGPT: *due to C's border appeal, standardization, and support ecosystem likely contributed to its greater adoption and use in mainstream computing*. So, the question is, how to encurage today's world of C programmers to take a look at Forth. How do we convince them that Forth can be 10 times more productive? Well, we do know that by keep saying how elegant Forth is or even bashing how bad C can be probably won't get us anywhere.

Dr. Ting, a pillar of Forth community, created eForth along with Bill Muench for educational purpose. He described Forth in his well-written eForth review [here](http://chochain.github.io/eforth/docs/eForthReview.pdf)

> The language consists of a collection of words, which reside in the memory of a computer and can be executed by entering their names on the computer keyboard. A list of words can be compiled, given a new name and made a new word. In fact, most words in Forth are defined as lists of existing words. A small set of primitive words are defined in machine code of the native CPU. All other words are built from this primitive words and eventually refer to them when executed.

> Forth is a computer model which can be implemented on any real CPU with reasonable resources. This model is often called a virtual Forth computer. The minimal components of a virtual Forth computer are:
> 1. A dictionary in memory to hold all the execution procedures.
> 2. A return stack to hold return addresses of procedures yet to be executed.
> 3. A data stack to hold parameters passing between procedures.
> 4. A user area in RAM memory to hold all the system variables.
> 5. A CPU to move date among stacks and memory, and to do ALU operations to parameters stored on the data stack.

### ceForth - on the shoulder of a giant
Most classic Forth systems are build with a few low-level primitives in assembly language and bootstrap the high-level words in Forth itself. Over the years, Dr. Ting have created many versions of Forth system using the same model. However, he eventually stated that it was silly trying to explain Forth in Forth to new comers. There are just not many people know Forth, period.
Utilizing morden OS and tool chains to construct new generation of Forths in just a few hundreds lines of C code, it is much easier for someone who did not know Forth to gain the core understanding. He called the insight **Forth without Forth**.
In 2021-07-04, I reconnected with Dr. Ting. He included me in his last projects all the way till his passing. I am honored that he considered me one of the frogs living in the bottom of the same well looking up to the small opening of the sky. Together, with cross-platform portability as our guildline, we built ooeForth in Java, jeForth in Javascript, wineForth for Windows, and esp32forth for ESP micro-controllers using the same code-base. With his last breath in the hospital, he even attempt to build it onto an FPGA using Verilog. see [ceForth_403 here](https://chochain.github.io/eforth/docs/ceForth_403.pdf) and [eJsv32 here](https://github.com/chochain/eJsv32).

### Evolution of ceForth - continuation of Dr. Ting's final work
* ceForth_10 - 2009       Dr. Ting first attempt of Forth in C
* ceForth_23 - 2017-07-13 Dr. Ting last version of ceForth with pre-built ROM (compiled in F#)
* ceForth_33 - 2019-07-01 Dr. Ting used CODE/LABEL/... functions as the macro assembler, 100% in C
* ceForth_33b- 2021-03-14 Lee refactored _33, separated asm and vm

* ceForth_40 - 2021-07-27 Lee proposed to Dr. Ting, to use
                          + struct for dictionary entry with name and lambda pointers,
                          + std::vector for dict/ss/rs, and
                          + std::map to host dictionary
* ceForth_40a- 2021-07-28 Lee use VT macros to build dictionary entries (struct)
* ceForth_40b- 2021-07-31 Lee replace std::vector with ForthList struct for dict/ss/rs
* ceForth_401- 2021-08-01 Dr. Ting adopted VT macro
* ceForth_402- 2021-08-03 Dr. Ting adopted ForthList
* ceForth_403- 2021-08-06 Lee refined _402
                          Dr. Ting add docs and presented it on Forth2020
                          
* ceForth_36 - 2021-09-27 Dr. Ting, utilized the experience from _40x, upgraded his _33 to _36
* ceForth_36a- 2021-10-03 Lee added CODE/IMMD macros
* ceForth_36b- 2021-10-03 Dr. Ting introduced Code struct and lambda
                          ported to esp32forth_85 and presented in Forth2020
* ceForth_36x- 2022-01-13 Lee pretty-print, sent to Masa Kasahara

### eForth next-gen, 100% C/C++ for cross-platform
* ~/src common source for eforth, esp32forth, wineForth, and weForth
* ~/orig/33b refactored 32-bit asm+vm implementation originated from <a href="http://forth.org/OffeteStore/OffeteStore.html" target="_blank">Dr. Ting's eForth site</a>
* ~/orig/ting, ~/orig/802 source codes collaborated with Dr. Ting

### Benchmark
* Desktop PC - 10K*10K cycles on 3.2GHz AMD
   > + 1200ms: ~/esp32forth/orig/esp32forth8_1, token indirect threading
   > + 1134ms: subroutine indirect threading, inline list methods
   > + 1050ms: subroutine indirect threading, with 16-bit offset
   > +  890ms: inner interpreter with cached xt offsets

* ESP32 - 1K*1K cycles on NodeMCU
   > + 1440ms: Dr. Ting's ~/esp32forth/orig/esp32forth_82
   > + 1045ms: ~/esp32forth/orig/esp32forth8_1, token indirect threading
   > +  839ms: subroutine indirect threading, inline list methods

### To Compile on Linux and Cygwin
> g++ -O2 -Isrc -o tests/eforth src/ceforth.cpp

### To Run on Linux
> ./tests/eforth

### TODO
* WASM - review wasmtime (CLI), perf+hotspot (profiling)

### Revision History
* Dr. Ting's work on eForth between 1995~2011
  <a href="http://forth.org/library/eforth_SOC" target="_blank">[eForth references]</a> and their <a href="http://forth.org/library/eforth_SOC/eforth_SOC_source" target="_blank">[Source Code Repo]</a>
* CC 20210314: Initial
  > Started with ~orig/33b code-base, refactor with enum and VA_ARGS macros targeting 100% C/C++.
* CC 20210707: Refactor
  > Incorporated list-based dict, ss, rs (i.e. ~orig/ting/ceForth40 and ~orig/802) which I proposed to Dr. Ting in our email exchanges.
* CC 20210816: Code Merge
  > Targeting multi-platform. Common source by consolidating ceForth, wineForth, ESP32forth (kept in ~/orig/*). Officially version 8.0
* CC 20220512: Refactor
  >  Though the goal of Dr. Ting's eForth is to demonstrate how a Forth can be easily understood and cleanly constructed. However, the token threading method used is costly (slow) because each call needs 2 indirect lookups (token->dict, dict->xt). On top of that, C/C++ callframe needs to be setup/teardown. It is worsen by the branch prediction missing every call stalling the CPU pipeline. Bad stuffs!
  > Refactor to subroutine indirect threading. It's not portable but does speed up 25% (see benchmark above).
  > Using 16-bit offsets for pointer arithmetic which speed up another 5% while maintaining 16-bit parameter space consumption.
  > Since C++ code is at least 4-byte aligned and parameter is 2-byte aligned, the LSB of a given parameter is utilized for colon word identification.
* CC 20221118: Refactor
  > WASM function pointer is U32 (index). Token-indirect worked but the two indirect look-up is even slower. Since WASM uses 64K linear memory block, 16-bit pointer offset is a better option. However, the xt "function pointer" in code space is simply an index to the shared _indirect_function_table. Since LSB is used, so we are forced to use MSB to differentiate primitive word from colon word. This left us 15-bit, i.e. 32K, parameter offset available.
* CC 20231011: Review
  > Since the original intention of having a pre-compiled ROM dictionary still end up in C++ static initialization run before main(), moved dictionary compilation into dict_compile as function calls gives a little more debugging control and opportunity for fine tuning.
  > LAMBDA_OK option was originally intended for full VM implementation but 2x slower. Dropped to reduce source clutter.
* CC 20240308: Refactor for multi-platform
  > To support cross-platform, i.g. WIN32, Arduino/ESP, Linux, and WASM, there were many conditional compilation branches which make the code really messy.
  + Separate cross-platform and configuation into ~/src/config.h
  + Separate platform specific code into ~/platform/*.cpp
  + Use ffff as word exit, instead of 0000, to preserve XT0 offset and helps debugging

