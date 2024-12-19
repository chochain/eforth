# Forth - is it still relevant?
With all the advantages, it is unfortunate that Forth lost out to C language over the years and have been reduced to a niche. Per ChatGPT: *due to C's broader appeal, standardization, and support ecosystem likely contributed to its greater adoption and use in mainstream computing*.

So, the question is, how to encourage today's world of C programmers to take a look at Forth. How do we convince them that Forth can be 10 times more productive? Well, we do know that by keep saying how elegant Forth is or even bashing how bad C can be probably won't get us anywhere.

Bill Munich created eForth for simplicity and educational purpose. Dr. Ting, ported to many processors, described Forth in his well-written eForth [genesis](https://chochain.github.io/eforth/docs/eForthAndZen.pdf) and [overview](https://chochain.github.io/eforth/docs/eForthOverviewv5.pdf)

> The language consists of a collection of words, which reside in the memory of a computer and can be executed by entering their names on the computer keyboard. A list of words can be compiled, given a new name and made a new word. In fact, most words in Forth are defined as lists of existing words. A small set of primitive words are defined in machine code of the native CPU. All other words are built from this primitive words and eventually refer to them when executed.

> Forth is a computer model which can be implemented on any real CPU with reasonable resources. This model is often called a virtual Forth computer. The minimal components of a virtual Forth computer are:
> 1. A dictionary in memory to hold all the execution procedures.
> 2. A return stack to hold return addresses of procedures yet to be executed.
> 3. A data stack to hold parameters passing between procedures.
> 4. A user area in RAM memory to hold all the system variables.
> 5. A CPU to move date among stacks and memory, and to do ALU operations to parameters stored on the data stack.

## eForth now - What has been done!

1. <b>100% C/C++ with multi-platform support</b>. Though classic implementation of primitives in assembly language and scripted high-level words gave the power to Forth, it also became the hurtle for newbies. Because they have to learn the assembly and Forth syntax before peeking into the internal beauty of Forth.
2. <b>Dictionary is just an array</b>. It's remodeled from linear memory linked-list to an array (or a vector in C++'s term) of words.
    + To search for a word, simply scan the name string of dictionary entries. So, to define a new word during compile time is just to append those found word pointers to the its parameter array one by one.
    + To execute become just a walk of the word pointers in the array. This is our inner interpreter.
    + Hashtables might go even faster but we'll try that later.
    
3. <b>Data and Return Stacks are also arrays</b>. With push, pop and [] methods to clarify intentions.
4. <b>Parameter fields are all arrays</b>. Why not! They can be dymatically expanded while compiling. Or changed on the fly in runtime i.e. self-morphing code. This can be a "scrary" feature for future Forth.
5. <b>No vocabulary, or meta-compilation</b>. These black-belt skills of Forth greatness are dropped to keep the focus on core concepts.
6. <b>Multi-threading and message passing are available</b> From v5.0 and on, multi-core platform can utilize Forth VMs running in parallel. see the multi-threading section below for details
    + A thread pool is built-in. Size is defaults to number of cores.
    + Message Passing send/rec with pthread mutex waiting.
    + IO and memory update can be synchronized with lock/unlock.

## Rolling your own C-based Forth?

If you are fluent in C/C++ and in the process of building your own Forth, skipping the verbage, the easiest path to gain understanding of how things work together is go straight to ~/orig/40x.

A heavily commented [ceforth.cpp](https://github.com/chochain/eforth/blob/master/orig/40x/ceforth.cpp) and the companion [ceforth.h](https://github.com/chochain/eforth/blob/master/orig/40x/ceforth.h) are all you need. Check them out!

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

2. <b>Lit, Var, Str, Bran, Tmp</b> - the polymorphic classes extended from the base class Code which serve the functionalities of primitive words of classic Forth.
    <pre>
    + Lit  - numeric literals
    + Var  - variable or constant
    + Str  - string for dostr or dotstr
    + Bran - Branching opcode
    + Tmp  - temp storage for branching word
    </pre>

3. <b>Dictionary</b> - an array of *Code* objects
    <pre>
    + build-it words - constructed by initializer_list at start up, before main is called, degenerated lambdas become function pointers stored in Code.xt
        dict[0].xt ------> lambda[0]         <== These function pointers can be converted
        dict[1].xt ------> lambda[1]             into indices to a jump table
        ...                                      which is exactly what WASM does
        dict[N-1].xt ----> lambda[N-1]       <== N is number of built-in words
        
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
    
    i.e. either we call a built-in word's lambda function or walk the Code.pf array recursively like a depth-first tree search.
    
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

There are two major versions current. eForth. v4 is single-threaded only and v5 default single-threaded but also supports multi-threaded.

Checkout the version you are interested in.

    > git checkout v42           # for version 4.2 (latest), or
    > git checkout master        # for version 5 and on

To enable multi-threading, of v5, update the followings in ~/src/config.h
    
    > #define DO_MULTITASK   1
    > #define E4_VM_POOL_SZ  8
    
### Linux, Cygwin, or Raspberry Pi

    > make
    > ./tests/eforth             # to bring up the Forth interpreter
    > type> words⏎               \ to see available Forth words
    > type> 1 2 +⏎               \ see Forth in action
    > type> bye⏎  or Ctrl-C      \ to exit eForth

    Once you get pass the above, try the lessons by Dr. Ting.
    > ./tests/eforth < ./tests/demo.fs

    Pretty amazing stuffs! To grasp how they were done, study the
    individual files (*.fs) under ~/tests/demo.

### WASM

For multi-threading to work, browser needs to receive Cross-Origin policies [here for detail](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Opener-Policy) in the response header. A Python script *~/tests/cors.py* is provided to solve the issue. The same needed to be provided if you use other web server.

    > ensure you have Emscripten (WASM compiler) installed and configured
    > type> make wasm
    > type> python3 tests/cors.py        # supports COOP
    > from your browser, open http://localhost:8000/tests/eforth.html

### ESP32
* Note: Most ESP32 are dual-core. However core0 is dedicated to WiFi and FreeRTOS house keeping. Forth tasks will be tied to core1 only. No performance gain running in parallel. So, singled-threaded does better.

    > ensure your Arduino IDE have ESP32 libraries installed
    > update ESP32 compiler.optimization flags in ~/hardware/platform.txt to -O3 (default -Os)
    > open eforth.ino with Arduino IDE
    > inside eforth.ino, modify WIFI_SSID and WIFI_PASS to point to your router
    > open Arduino Serial Monitor, set baud 115200 and linefeed to 'Both NL & CR'
    > compile and load
    > if successful, web server IP address/port and eForth prompt shown in Serial Monitor
    > from your browser, enter the IP address to access the ESP32 web server

### Experimental eForth - Linear-memory, 32-bit, subroutine-threaded

    > make 50x
    > ./tests/eforth50x

## Multi-threading - for release v5.0 and after
Forth has been supporting multi-tasking since the 70's. They are single-CPU round-robin/time-slicing systems mostly. Modern system has multiple cores and Forth can certainly take advantage of them. However, unlike most of the matured Forth word sets, multi-threading/processing words are yet to be standardized and there are many ways to do it.

### Design & Implementation

    > each VM has it's own private ss, rs, tos, ip, and state
    > multi-threading, instead of multi-processing, with shared dictionary and parameter memory blocks.
    > pthread.h is used. It is a common POSIXish library supported by most platforms. I have only tried the handful on hands, your milage may vary.
    > Message Passing interface for inter-task communication.

### Life-cycle

    > 1. We have the VM array, sized by E4_VM_POOL_SZ, which defines the max tasks you want to have. Typically, anything more than your CPU core count does not help completing the job faster.
    > 2. Each VM is associated with a thread, i.e. our thread-pool.
    > 3. The event_queue, a C++ queue takes in "ready to run" tasks.
    > 4. Lastly, event_loop picks up "ready to run" tasks and kicks start them one by one.

    The following VM states manage the life-cycle of a task
    
    > QUERY - interpreter mode - only the main thread can do this
    > HOLD  - ready to execute, or waiting for message to arrive
    > NEST  - in execution
    > STOP  - free for next task

Before we go too far, make sure the following are updated before your build

    > pthread.h is installed. 
    > DO_MULTITASK, E4_VM_POOL_SZ are updated in ~/src/config.h

### Built-in words (available only when DO_MULTITASK is enabled)
    
|word|stack|desc|state|
|----|-----|----|-----|
|task|( xt -- t )|create a task (tid is index to thread pool entry)<br/>a free VM from pool is chosen for the task|STOP=>HOLD|
|rank|( -- t )|fetch current task id|NEST|
|start|( t -- )|start a task<br/>The VM is added to event_queue and kick started when picked up by event_loop|HOLD=>NEST|
|join|( t -- )|wait until the given task is completed|NEST=>STOP|
|lock|( -- )|lock (semaphore) IO or memory|NEST|
|unlock|( -- )|release IO or memory lock|NEST|
|send|( v1 v2 .. vn n t -- )|send n elements on current stack to designated task's stack (use stack as message queue)|sender NEST<br/>receiver HOLD|
|recv|( -- v1 v2 .. vn )|wait, until message to arraive|HOLD=>NEST|
|pull|( n t -- )|forced fetch stack elements from a completed task|current NEST<br/>target STOP|
|bcast|( n -- )|not implemented yet, TODO|sender NEST<br/>receivers HOLD|

#### Example1 - parallel jobs

    > : once 999999 for rank drop next ;      \ 1M cycles
    > : run ms negate once ms + . ." ms" cr ; \ benchmark
    > ' run constant xt                       \ keep the xt
    > : jobs 1- for xt task start next ;      \ tasks in parallel
    > 4 jobs

<pre><font color="#4E9A06">[06.1]&gt;&gt; started on T2</font>
<font color="#C4A000">[05.1]&gt;&gt; started on T4</font>
<font color="#3465A4">[04.1]&gt;&gt; started on T6</font>
<font color="#CC0000">[07.1]&gt;&gt; started on T0</font>
18 ms
<font color="#4E9A06">[06.3]&gt;&gt; finished on T2</font>
18 ms
<font color="#C4A000">[05.3]&gt;&gt; finished on T4</font>
18 ms
<font color="#3465A4">[04.3]&gt;&gt; finished on T6</font>
18 ms
<font color="#CC0000">[07.3]&gt;&gt; finished on T0</font>
</pre>

#### Example2 - producer-consumer

    > 0 constant pp                           \ producer task id
    > 0 constant cc                           \ consumer task id
    > : sndr
        1000 delay                            \ delay to simulate some processing
        1 2 3 4 4 cc send                     \ send 4 items from stack
        lock ." sent " unlock ;               \ locked IO before write
    > : rcvr
        recv                                  \ wait for sender
        + + +                                 \ sum received 4 items
        lock ." sum=" . unlock ;              \ locked IO before write
    > ' sndr task to pp
    > ' rcvr task to cc
    > cc start                                \ start receiver task
    > pp start                                \ start sender task
    > pp join cc join                         \ wait for completion

#### Example3 - fetch result(s) from completed task

    > : sum 0 1000000 for i + next ;          \ add 0 to 1M
    > ' sum task constant tt                  \ create the task
    > tt start tt join                        \ run and wait for completion
    > 1 tt pull ." total=" .                  \ pull the sum

## Source Code Directories

    + ~/src       - common source code for all supported platforms
    + ~/platform  - platform specific code for C++, ESP32, Windows, and WASM
    + ~/orig      - archive from Dr. Ting and my past works
    +    /33b     - refactored ceForth_33, separate ASM from VM (used in eForth1 for Adruino UNO)
    +    /ting    - ceForth source codes collaborated with Dr. Ting
    +    /esp32   - esp32forth source codes collaborated with Dr. Ting
    +    /40x     - my experiments, refactor _40 into vector-based subroutine-threaded, with 16-bit offset
    +    /50x     - my experiments, add multi-threading to _40

## Evolution - my experiments on various implementation and tuning

    + ~/platform/   - platform specific for C++, ESP32, WASM
    + ~/orig/40x/ceforth - array-based subroutine-threaded, with 16-bit offset enhanced (released as v4.2)
    + ~/orig/50x/ceforth - add multi-threading support to 40x
    + ~/src/eforth  - multi-threaded, dynamic vector-based, object threading

## Benchmark and Tuning

### Desktop PC - 10K*10K cycles on 3.2GHz AMD**

    + 4452ms: ~/orig/ting/ceforth_36b, linear memory, 32-bit, token threading
    + 1450ms: ~/orig/ting/ceForth_403, dict/pf array-based, subroutine threading
    + 1050ms: ~/orig/40x/ceforth, subroutine indirect threading, with 16-bit offset
    +  890ms: ~/orig/40x/ceforth, inner interpreter with cached xt offsets
    +  780ms: ~/src/eforth, v4.2 dynamic vector, object threading
    +  810ms: ~/src/eforth, v5.0 multi-threaded, dynamic vector, object threading

### ESP32 - 1K*1K cycles on 240MHz NodeMCU**

    + 1440ms: Dr. Ting's ~/esp32forth/orig/esp32forth_82
    + 1045ms: ~/orig/esp32/ceforth802, array-based, token threading
    +  990ms: ~/orig/40x/ceforth, linear-memory, subroutine threading, with 16-bit offset
    +  930ms: ~/orig/40x/ceforth, inner interpreter with cached xt offsets
    +  644ms: ~/src/eforth, v4.2 dynamic vector, token threading
    +  534ms: ~/src/eforth, v5.0 multi-threaded, dynamic vector, object threading (with gcc -O3)

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

## References
    + perf   - [multithreaded](https://easyperf.net/blog/2019/10/05/Performance-Analysis-Of-MT-apps)
    + coding -
        [optimizing](http://www.agner.org/optimize/optimizing_cpp.pdf)
        [false-sharing](https://medium.com/distributed-knowledge/optimizations-for-c-multi-threaded-programs-33284dee5e9c)
        [affinity](https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/)
        [occlusion](https://fgiesen.wordpress.com/2013/02/17/optimizing-sw-occlusion-culling-index/)
        [perf c2c](https://coffeebeforearch.github.io/2020/03/27/perf-c2c.html)

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
    +  Though the goal of Dr. Ting's is to demonstrate how a Forth can be easily understood and cleanly constructed. However, the token threading method used is costly (slow) because each call needs 2 indirect lookups (token->dict, dict->xt). On top of that, C/C++ callframe needs to be setup/teardown. It is worsen by the branch prediction missing every call stalling the CPU pipeline. Bad stuffs!
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

* CC 20241001: Add multi-threading support
    + Shared dictionary and code space amount threads.
    + Refactor source into ceforth, ceforth_sys, and ceforth_task for their specific functions.
    + Introduce VM, states
        - local ss, rs, tos, and user area
        - align to cache-line width
        - pass VM& to all lambda and static functions
    + Add thread pool and event_loop with affinity to physical cores.
        - task, start, stop, join for thread life-cycle management
        - add general multi-threading demo
    + Add Inter-task communication
        - pthread mutex and condition variables are used for synchronization
        - rank for task id
        - send, recv, and pull. Use local stack, as queue, for message passing.
        - add producer/consumer demo
    + Add IO sequencing
        - ANSI-Color trace/logging for different cores
        - mutex guard used
        - lock, unlock for output stream synchronization
