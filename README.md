# Forth - is it still relevant?
With all the advantages, it is unfortunate that Forth lost out to C language over the years and have been reduced to a niche. Per ChatGPT: *due to C's border appeal, standardization, and support ecosystem likely contributed to its greater adoption and use in mainstream computing*.

So, the question is, how to encourage today's world of C programmers to take a look at Forth. How do we convince them that Forth can be 10 times more productive? Well, we do know that by keep saying how elegant Forth is or even bashing how bad C can be probably won't get us anywhere.

Dr. Ting, a pillar of Forth community, created eForth along with Bill Muench for educational purpose. He described Forth in his well-written eForth review [here](http://chochain.github.io/eforth/docs/eForthReview.pdf)

> The language consists of a collection of words, which reside in the memory of a computer and can be executed by entering their names on the computer keyboard. A list of words can be compiled, given a new name and made a new word. In fact, most words in Forth are defined as lists of existing words. A small set of primitive words are defined in machine code of the native CPU. All other words are built from this primitive words and eventually refer to them when executed.

> Forth is a computer model which can be implemented on any real CPU with reasonable resources. This model is often called a virtual Forth computer. The minimal components of a virtual Forth computer are:
> 1. A dictionary in memory to hold all the execution procedures.
> 2. A return stack to hold return addresses of procedures yet to be executed.
> 3. A data stack to hold parameters passing between procedures.
> 4. A user area in RAM memory to hold all the system variables.
> 5. A CPU to move date among stacks and memory, and to do ALU operations to parameters stored on the data stack.

### ceForth - on the shoulder of a giant
Most classic Forth systems are build with a few low-level primitives in assembly language and bootstrap the high-level words in Forth itself. Over the years, Dr. Ting have implemented many Forth systems using the same model. See [here](https://www.forth.org/OffeteStore/OffeteStore.html) for the detailed list. However, he eventually stated that it was silly trying to explain Forth in Forth to new comers. There are just not many people know Forth, period.

Utilizing modern OS and tool chains, a new generation of Forths implemented in just a few hundreds lines of C code can help someone who did not know Forth to gain the core understanding much quickly. He called the insight **Forth without Forth**.

In 2021-07-04, I got in touched with Dr. Ting mentioning that he taught at the university when I attended. He, as the usual kind and generous him, included me in his last projects all the way till his passing. I am honored that he considered me one of the frogs living in the bottom of the deep well with him looking up to the small opening of the sky together. With cross-platform portability as our guild-line, we built ooeForth in Java, jeForth in Javascript, wineForth for Windows, and esp32forth for ESP micro-controllers using the same code-base. With his last breath in the hospital, he attempted to build it onto an FPGA using Verilog. see [ceForth_403](https://chochain.github.io/eforth/docs/ceForth_403.pdf) and [eJsv32](https://github.com/chochain/eJsv32) for details.

### Evolution - continuation of Dr. Ting's final work
Source codes kept under ~/orig/ting
<pre>
ceForth_10 - 2009       Dr. Ting first attempt of Forth in C
ceForth_23 - 2017-07-13 Dr. Ting last version of ceForth with pre-built ROM (compiled in F#)
ceForth_33 - 2019-07-01 Dr. Ting used CODE/LABEL/... functions as the macro assembler, 100% in C

ceForth_40 - 2021-07-27 Lee suggested Dr. Ting to use
                        + struct for dictionary entry with name and lambda pointers,
                        + std::vector for dict/ss/rs, and
                        + std::map to host dictionary
ceForth_40a- 2021-07-28 Lee suggested using VT macros to build dictionary entries (struct)
ceForth_40b- 2021-07-31 Lee replaced std::vector with ForthList struct for dict/ss/rs
ceForth_401- 2021-08-01 Dr. Ting adopted VT macro
ceForth_402- 2021-08-03 Dr. Ting adopted ForthList
ceForth_403- 2021-08-06 Lee refined _402
                        Dr. Ting add docs and presented it on Forth2020

ceForth_36 - 2021-09-27 Dr. Ting, learnt from _40x, upgraded his _33 to _36 (retained linear memory model)
ceForth_36a- 2021-10-03 Lee added CODE/IMMD macros
ceForth_36b- 2021-10-03 Dr. Ting added Code struct and lambda,
                        ported to esp32forth_85 and presented in Forth2020
ceForth_36x- 2022-01-13 Dr. Ting final archive, great for understanding Forth building
</pre>

### Changes - what did we do?
Even with metacompilation (the black-belt stuffs of Forth greatness) delibrately dropped to reduce the complexity, eForth traditionally uses linear memory to host words of the entire dictionary, including codes and their parameters with a backward linked-list and hence the well-known term threading. This model is crucial when memory is scarce or compiler resource is just underwhelming. It, however, does create extra hurdle that sometimes hinder the learning of newbies.

**Change 1: Separation of parameter memory and dictionary**
<pre>
+ it makes dictionary uniform size which eliminates the need for link field
- the down side is that it requires manual array size tuning
</pre>
   
eForth_33 uses functions i.g. CODE, LABEL, HEADER, ... as macros to assemble dictionary which just mimicing how classic Forth creates the dictionary. Visually, it is not that different from using Forth which is challenging for new comers.

**Change 2: Use struct to host a dictionary entry**
<pre>
struct Code {
    string name;
    void   (*xt)(void);
    int    immd;
};
+ it simpify the classic field management of Forth
- extra space to store the name and code pointers
</pre>

**Change 3: Build array-based dictionary**
<pre>
const struct Code primitives[] = {
    CODE("dup",  stack[++S] = top),
    CODE("drop", pop()),
    CODE("over", push(stack[S])),
    ...
    CODE("+",    top += pop()),
    CODE("-",    top -= pop()),
    CODE("*",    top *= pop()),
    CODE("/",    top /= pop()),
    ...
    CODE("delay", sleep(pop())),
    ...
};
+ it makes the size of dictionary entries uniform
+ it removes the need for threading the linked-list (and link field)
+ using lambda syntax makes the intention easy to understand
+ OS library functions can be called directly by opcode
</pre>

C language on modern OS have good libraries for I/O interfaces,
the classic way of TX/RX bytes or using block to manage files are no more essential. They were necessary, but no more for today's Forth.

**Change 4: Use Streams for input/output**
<pre>
CODE(".",  cout << pop() << " "),
CODE("cr", cout << ENDL),
+ the I/O opcodes are easy to write and understand
+ they can be easily redirected to/from various interfaces/devices
</pre>
or
<pre>
while (cin >> idiom) {
    int w = find_word(idiom);
    if (w) {
        if (compile && !dict[w].immd) comma(w);
        else                          run(w);
    }
    else {
        int n = get_number(idiom);
        if (compile) comma(n);
        else         push(n);
    }
}
+ our outer-interpreter is understandable even without any comment
</pre>

Dr. Ting latest ceForth uses the token indirect threading model. It is great for learning as wll
as being portable. The extra lookup for token to function pointer makes it slower (at about 50%) of
a subroutine indirect threading model.

**Change 5: Using 16-bit xt offset in parameter field (instead of full 32 or 64 bits)**
<pre>
+ it avoids the double lookup of token threaded indexing
+ it reduces parameter storage requirement from 32-bit to 16-bit
+ it speeds up by reading only 2-bytes from RAM instead of 4-bytes (cache hit more)
+ it unifies xt/pfa parameter storage
+ it uses the LSB for id flag (2-byte aligned, so LSB is free)
- it limits function pointer spread to 64KB range
- the words created are not binary portable anymore
</pre>

### Memory Structures
**Struct to host a dictionary entry**
<pre>
Universal functor (no STL) and Code class
  Code class on 64-bit systems (expand pfa possible)
  +-------------------+-------------------+
  |    *name          |       xt          |
  +-------------------+----+----+---------+
                      |attr|pfa |xxxxxxxxx|
                      +----+----+---------+
  Code class on 32-bit systems (memory best utilized)
  +---------+---------+
  |  *name  |   xt    |
  +---------+----+----+
            |attr|pfa |
            +----+----+
  Code class on WASM/32-bit (a bit wasteful)
  +---------+---------+---------+
  |  *name  |   xt    |attr|xxxx|
  +---------+----+----+---------+
            |pfa |xxxx|
            +----+----+
</pre>

**Macros to build dictionary entry*8
<pre>
#define ADD_CODE(n, g, im) {    \
    Code c(n, []{ g; }, im);	\
    dict.push(c);               \
    }
#define CODE(n, g) ADD_CODE(n, g, false)
#define IMMD(n, g) ADD_CODE(n, g, true)
</pre>

**Global memory blocks**
<pre>
  Dictionary structure (N=E4_DICT_SZ in config.h)
     dict[0].xt ---------> pointer to primitive word lambda[0]
     dict[1].xt ---------> pointer to primitive word lambda[1]
     ...
     dict[N-1].xt -------> pointer to last primitive word lambda[N-1]

  Parameter memory structure (memory block=E4_PMEM_SZ in config.h)
     dict[N].xt ----+ user defined colon word)    dict[N+1].xt------+
                    |                                               |
     +--MEM0        v                                               v
     +--------------+--------+--------+-----+------+----------------+-----
     | str nameN \0 |  parm1 |  parm2 | ... | ffff | str nameN+1 \0 | ...
     +--------------+--------+--------+-----+------+----------------+-----
     ^              ^        ^        ^     ^      ^
     | strlen+1     | 2-byte | 2-byte |     |      |
     +--------------+--------+--------+-----+------+---- 2-byte aligned

  Parameter structure - 16-bit aligned (use LSB for colon word flag)
     primitive word (16-bit xt offset with LSB set to 0)
     +--------------+-+
     | dict.xtoff() |0|   call (XT0 + *IP)() to execute
     +--------------+-+   this save the extra memory lookup for xt

     colon (user defined) word (16-bit pmem offset with LSB set to 1)
     +--------------+-+
     |  dict.pfa    |1|   next IP = *(MEM0 + (*IP & ~1))
     +--------------+-+
 </pre>
 
### Source Code directories
<pre>
+ ~/src           common source for ceforth, esp32forth, wineForth, and weForth
+ ~/orig/33b      refactored ceForth_33 with asm+vm
+ ~/orig/ting     ceForth source codes collaborated with Dr. Ting
+ ~/orig/802      esp32forth source codes collaborated with Dr. Ting
</pre>

### Benchmark
**Desktop PC - 10K*10K cycles on 3.2GHz AMD**
<pre>
+ 4452ms: ceForth_36x, linear memory, 32-bit, token indirect threading
+ 1450ms: ceForth_403, dict/pf vector<*Code>, subroutine indirect threading
+ 1050ms: src/ceforth, subroutine indirect threading, with 16-bit offset
+  890ms: src/ceforth, inner interpreter with cached xt offsets
</pre>

**ESP32 - 1K*1K cycles on NodeMCU**
<pre>
+ 1440ms: Dr. Ting's ~/esp32forth/orig/esp32forth_82
+ 1045ms: ~/esp32forth/orig/esp32forth8_1, token indirect threading
+  990ms: src/ceforth, subroutine indirect threading, with 16-bit offset
+  930ms: src/ceforth, inner interpreter with cached xt offsets
</pre>

### To Compile on Linux and Cygwin
> g++ -O3 -Isrc -o tests/eforth src/ceforth.cpp

### To Run on Linux
> ./tests/eforth

### Revision History
* Dr. Ting's work on eForth between 1995~2011
  [eForth references](http://forth.org/library/eforth_SOC) and their [Source Code Repo](http://forth.org/library/eforth_SOC/eforth_SOC_source)
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
  + add include opcode for Forth script loading

