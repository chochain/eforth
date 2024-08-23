# Forth - is it still relevant?
With all the advantages, it is unfortunate that Forth lost out to C language over the years and have been reduced to a niche. Per ChatGPT: *due to C's broader appeal, standardization, and support ecosystem likely contributed to its greater adoption and use in mainstream computing*.

So, the question is, how to encourage today's world of C programmers to take a look at Forth. How do we convince them that Forth can be 10 times more productive? Well, we do know that by keep saying how elegant Forth is or even bashing how bad C can be probably won't get us anywhere.

Dr. Ting, a pillar of Forth community, created eForth along with Bill Munich for educational purpose. He described Forth in his well-written eForth [genesis](https://chochain.github.io/eforth/docs/eForthAndZen.pdf) and [overview](https://chochain.github.io/eforth/docs/eForthOverviewv5.pdf)

> The language consists of a collection of words, which reside in the memory of a computer and can be executed by entering their names on the computer keyboard. A list of words can be compiled, given a new name and made a new word. In fact, most words in Forth are defined as lists of existing words. A small set of primitive words are defined in machine code of the native CPU. All other words are built from this primitive words and eventually refer to them when executed.

> Forth is a computer model which can be implemented on any real CPU with reasonable resources. This model is often called a virtual Forth computer. The minimal components of a virtual Forth computer are:
> 1. A dictionary in memory to hold all the execution procedures.
> 2. A return stack to hold return addresses of procedures yet to be executed.
> 3. A data stack to hold parameters passing between procedures.
> 4. A user area in RAM memory to hold all the system variables.
> 5. A CPU to move date among stacks and memory, and to do ALU operations to parameters stored on the data stack.

## eForth now - What have we done!

1. <b>100% C/C++ with multi-platform support</b>. Though classic implementation of primitives in assembly language and scripted high-level words gave the power to Forth, it also became the hurtle for newbies. Because they have to learn the assembly and Forth syntax before peeking into the internal beauty of Forth.
2. <b>Dictionary is just an array</b>. It's remodeled from linear memory linked-list to an array (or a vector in C++'s term) of words.
    + To search for a word, simply scan the name string of dictionary entries. So, to define a new word during compile time is just to append those found word pointers to the its parameter array one by one.
    + To execute become just a walk of the word pointers in the array. This is our inner interpreter.
    
3. <b>Data and Return Stacks are also arrays</b>. With push, pop and [] methods to clarify intentions.
4. <b>No vocabulary, multi-tasking, or meta-compilation</b>. These black-belt skills of Forth greatness are dropped to keep the focus on core concepts.

## eForth Internals
The core of current implementation of eForth is the dictionary composed of an array of Code objects that represent each of Forth words.

1. <b>Code</b> - the heart of eForth, depends on the constructor called, the following fields are populated accordingly
    <pre>
    + name - a string that holds primitive word's name, i.e. NFA in classic FORTH,
             can also holds branching mnemonic for compound words which classic FORTH keeps on parameter memory
    + xt   - pointer to a lambda function for primitive words i.e. XT in classic FORTH
    + pf, p1, p2 - parameter arrays of Code objects for compound words, i.e. PFA in classic FORTH
    + q    - holds the literal value which classic FORTH keep on parameter memory
    </pre>

2. <b>Lit, Var, Str, Bran, Tmp</b> - the polymorphic classes extended from the base class Code.
    <pre>
    + Lit  - numeric literals
    + Var  - variable or constant
    + Str  - string for dostr or dotstr
    + Bran - Branching opcode
    + Tmp  - temp storage for branching word
    </pre>

3. <b>Dictionary</b> - an array of *Code* objects
    <pre>
    + primitive words - constructed by initializer_list at start up, befor main is called, degeneated lambdas becomea function pointers stored in Code.xt
        dict[0].xt ------> lambda[0]         <== These function pointers can be converted
        dict[1].xt ------> lambda[1]             into indices to a jump table
        ...                                      which is exactly what WASM does
        dict[N-1].xt ----> lambda[N-1]       <== N is number of primitive words
        
    + colon (user defined) words - collection of word pointers during compile time
        dict[N].pf   = [ *Code, *Code, ... ] <== These are called the 'threads' in Forth's term
        dict[N+1].pf = [ *Code, *Code, ... ]     So, instead of subroutine threading
        ...                                      this is 'object' threading.
        dict[-1].pf  = [ *Code, *Code, ... ]     It can be further compacted into
                                                 token (i.e. dict index) threading if desired
    </pre>
    
4. <b>Inner Interpreter</b> - *Code.exec()* is self-explanatory
    ```C
    if (xt) { xt(this); return; }         // run primitive word
    for (Code *w : pf) {                  // run colon word
        try { w->exec(); }                // execute recursively
        catch (...) { break; }            // handle exception if any
    }
    ```
    
    i.e. either we call a primitive word's lambda function or walk the Code.pf array recursively like a depth-first tree search.
    
5. <b>Outer Interpreter</b> - *forth_core()* is self-explanatory
    ```C
    Code *c = find(idiom);                // search dictionary
    if (c) {                              // word found?
        if (compile && !c->immd)          // are we compiling a new word?
            dict[-1]->add(c);             // then append found code to it
        else c->exec();                   // or, execute the code
        return;
    }
    DU n = parse_number(idiom);           // word not found, try as a number
    if (compile)                          // are we compiling a new word?
        dict[-1]->add(new Lit(n));        // append numeric literal to it
    else PUSH(n);                         // push onto data stack
    ```
    
## ceForth - Where we came from

Most classic Forth systems are build with a few low-level primitives in assembly language and bootstrap the high-level words in Forth itself. Over the years, Dr. Ting have implemented many Forth systems using the same model. See [here](https://www.forth.org/OffeteStore/OffeteStore.html) for the detailed list. However, he eventually stated that it was silly trying to explain Forth in Forth to new comers. There are just not many people know Forth, period.

Utilizing modern OS and tool chains, a new generation of Forths implemented in just a few hundreds lines of C code can help someone who did not know Forth to gain the core understanding much quickly. He called the insight **Forth without Forth**.

In 2021-07-04, I got in touched with Dr. Ting mentioning that he taught at the university when I attended. He, as the usual kind and generous him, included me in his last projects all the way till his passing. I am honored that he considered me one of the frogs living in the bottom of the deep well with him looking up to the small opening of the sky together. With cross-platform portability as our guild-line, we built ooeForth in Java, jeForth in Javascript, wineForth for Windows, and esp32forth for ESP micro-controllers using the same code-base. With his last breath in the hospital, he attempted to build it onto an FPGA using Verilog. see [ceForth_403](https://chochain.github.io/eforth/docs/ceforth_403.pdf) and [eJsv32](https://github.com/chochain/eJsv32) for details.

We hope it can serve as a stepping stone for learning Forth to even building their own, one day.

## How To Build and Run

    > git clone https://github.com/chochain/eforth to your local machine
    > cd eforth
    
### Linux or Cygwin

    > make
    > ./tests/eforth             # to bring up the Forth interpreter
    > type> words⏎               \ to see available Forth words
    > type> 1 2 +⏎               \ see Forth in action
    > type> bye⏎  or Ctrl-C      \ to exit eForth

    Once you get pass the above, try the lessons by Dr. Ting.
    > ./tests/eforth < ./tests/lessons420.txt

    Pretty amazing stuffs! To grasp how they were done, study the
    individual files under ~/tests/demo.

### WASM

    > ensure you have Emscripten (WASM compiler) installed and configured
    > type> make wasm
    > type> python3 -m http.server
    > from your browser, open http://localhost:80/tests/eforth.html

### ESP32

    > ensure your Arduino IDE have ESP32 libraries installed
    > open eforth.ino with Arduino IDE
    > inside eforth.ino, modify WIFI_SSID and WIFI_PASS to point to your router
    > open Arduino Serial Monitor, set baud 115200 and linefeed to 'Both NL & CR'
    > compile and load
    > if successful, web server IP address/port and eForth prompt shown in Serial Monitor
    > from your browser, enter the IP address to access the ESP32 web server

### Legacy ceForth, 32-bit token-threaded

    > make 36b
    > ./tests/ceforth36b

### Experimental ceForth, Linear-memory, 32-bit subroutine-threaded

    > make 40x
    > ./tests/eforth40x

## Source Code Directories

    + ~/src       - common source code for all supported platforms
    + ~/platform  - platform specific code for C++, ESP32, Windows, and WASM
    + ~/orig      - archive from Dr. Ting and my past works
    +    /33b     - refactored ceForth_33, separate ASM from VM (used in eForth1 for Adruino UNO)
    +    /ting    - ceForth source codes collaborated with Dr. Ting
    +    /esp32   - esp32forth source codes collaborated with Dr. Ting
    +    /40x     - my experiments, refactor _40 into vector-based subroutine-threaded, with 16-bit offset

## Evolution - continuation of Dr. Ting's final work

Kept under ~/orig and details [here](https://chochain.github.io/eforth/orig/index.html)

### ~/orig/ting - Dr. Ting's original ceForth

    + ting/_23                 - cross-compiled ROM, token-threaded
    + ting/_33                 - assembler with C functions as macro, token-threaded
    + 33b/                     - from _33, I refactored assembler from inner interpreter
    + ting/_40, _40b, _40c     - my input, array-based, token-threaded
    + ting/_401, _402, _403    - email exchange on _40 with Dr. Ting
    + ting/_36, _36b, _36x     - from _33, Dr. Ting updated his linear memory, subroutine-threaded
    + ting/_410                - from _403, I refactored with initializer_list, and bug fixes

### ~/orig/esp32 - the origin of ESP32Forth

    + esp32/_54, _59           - cross-compiled ROM, token-threaded
    + esp32/_62, _63           - assembler with C functions as macros, from ceForth_33
    + esp32/_705               - ESP32Forth v7, C macro-based assembler with Brad and Peter
    + esp32/_802               - my interim work shown to Dr. Ting, sync ceForth_403
    + esp32/_82, _83, _84, _85 - from _63, Dr. Ting adapted array-based, token-threaded

### ~/orig/40x - experiments on _40 for various implementation and tuning

    + 40x/ceforth - array-based subroutine-threaded, with 16-bit offset enhanced

### life after _410 and 40x - my current work

    + src/ceforth - multi-platform supporting code base
    + platform/   - platform specific for C++, ESP32, WASM

## Benchmark and Tuning

### Desktop PC - 10K*10K cycles on 3.2GHz AMD**

    + 4452ms: ~/orig/ting/ceforth_36b, linear memory, 32-bit, token threading
    + 1450ms: ~/orig/ting/ceForth_403, dict/pf array-based, subroutine threading
    + 1050ms: ~/orig/40x/ceforth, subroutine indirect threading, with 16-bit offset
    +  890ms: ~/orig/40x/ceforth, inner interpreter with cached xt offsets
    +  780ms: ~/src/ceforth, dynamic vector, token threading

### ESP32 - 1K*1K cycles on 240MHz NodeMCU**

    + 1440ms: Dr. Ting's ~/esp32forth/orig/esp32forth_82
    + 1045ms: ~/orig/esp32/ceforth802, array-based, token threading
    +  990ms: ~/orig/40x/ceforth, linear-memory, subroutine threading, with 16-bit offset
    +  930ms: ~/orig/40x/ceforth, inner interpreter with cached xt offsets
    +  644ms: ~/src/ceforth dynamic vector, token threading

### Dictionary of Pointers vs Objects

What is the performance difference?
1. Code *dict[] - where words are dynamically allocated as a collection of pointers, or
2. Code dict[]  - where words are statically created as an array of objects.

I have created a git branch 'static' to compare to the 'master. The static version is about 10% slower on 64-bit machine and about 5% slower on 32-bits. This hasn't been carefully analyzed but my guess is because Code is big at 144-bytes on 64-bit. They might get pushed off L1 cache too often.

### Memory Consumption Consideration
Though the use of C++ standard libraries helps us understanding what Forth does but, even on machines with GBs, we still need to be mindful of the followings. It gets expensive especially on MCUs.

    + A pointer takes 8-byte on a 64-bit machine,
    + A C++ string, needs 3 to 4 pointers, will require 24-32 bytes,
    + A vector, takes 3 pointers, is 24 bytes

The current implementation of ~/src/ceforth.h, a Code node takes 144 bytes on a 64-bit machine. On the other extreme, my ~/orig/40x experimental version, a vector linear-memory hybrid, takes only 16 bytes [here](https://chochain.github.com/eforth/orig/40x/ceforth.h). Go figure how the classic Forths needs only 2 or 4 bytes per node via linked-field and the final executable in a just a few KB. You might start to understand why the old Forth builders see C/C++ like plaque.

## Revision History

* Dr. Ting's work on eForth between 1995~2011
    [eForth references](http://forth.org/library/eforth_SOC) and their [Source Code Repo](http://forth.org/library/eforth_SOC/eforth_SOC_source)
  
* CC 20210314: Initial
    + Started with ~orig/33b code-base, refactor with enum and VA_ARGS macros targeting 100% C/C++.
    
* CC 20210707: Refactor
    + Incorporated list-based dict, ss, rs (i.e. ~orig/ting/ceForth40 and ~orig/802) which I proposed to Dr. Ting in our email exchanges.
    
* CC 20210816: Code Merge
    + Targeting multi-platform. Common source by consolidating ceForth, wineForth, ESP32forth (kept in ~/orig/*). Officially version 8.0
    
* CC 20220512: Refactor
    +  Though the goal of Dr. Ting's eForth is to demonstrate how a Forth can be easily understood and cleanly constructed. However, the token threading method used is costly (slow) because each call needs 2 indirect lookups (token->dict, dict->xt). On top of that, C/C++ callframe needs to be setup/teardown. It is worsen by the branch prediction missing every call stalling the CPU pipeline. Bad stuffs!
    + Refactor to subroutine indirect threading. It's not portable but does speed up 25% (see benchmark above).
    + Using 16-bit offsets for pointer arithmetic which speed up another 5% while maintaining 16-bit parameter space consumption.
    + Since C++ code is at least 4-byte aligned and parameter is 2-byte aligned, the LSB of a given parameter is utilized for colon word identification.
    
* CC 20221118: Refactor
    + WASM function pointer is U32 (index). Token-indirect worked but the two indirect look-up is even slower. Since WASM uses 64K linear memory block, 16-bit pointer offset is a better option. However, the xt "function pointer" in code space is simply an index to the shared _indirect_function_table. Since LSB is used, so we are forced to use MSB to differentiate primitive word from colon word. This left us 15-bit, i.e. 32K, parameter offset available.
    
* CC 20231011: Review
    + Since the original intention of having a pre-compiled ROM dictionary still end up in C++ static initialization run before main(), moved dictionary compilation into dict_compile as function calls gives a little more debugging control and opportunity for fine tuning.
    + LAMBDA_OK option was originally intended for full VM implementation but 2x slower. Dropped to reduce source clutter.
    
* CC 20240308: Refactor for multi-platform, accept dynamic vectors
    + Experiment various threading and memory pointer models, archive into ~/orig/40x
    + To support cross-platform, i.g. Linux/Cygwin, Arduino/ESP32, Win32, and WASM, there were many conditional compilation branches which make the code really messy. The following were done
        - Separate cross-platform and configuration into ~/src/config.h
        - Separate platform specific code into ~/platform
        - add included opcode for Forth script loading
        - rename 'next_idiom' to 'word', per Forth standard

