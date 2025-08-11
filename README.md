# Forth - is it still relevant?
With all the advantages, it is unfortunate that Forth lost out to C language over the years and have been reduced to a niche. Per ChatGPT: *due to C's broader appeal, standardization, and support ecosystem likely contributed to its greater adoption and use in mainstream computing*.

So, the question is, how to encourage today's world of C programmers to take a look at Forth. How do we convince them that Forth can be 10 times more productive? Well, we do know that by keep saying how elegant Forth is or even bashing how bad C can be probably won't get us anywhere.

Bill Munich created eForth for simplicity and educational purpose. Dr. Ting, ported to many processors, described Forth in his well-written eForth [genesis](https://chochain.github.io/eforth/docs/eForthAndZen.pdf) and [overview](https://chochain.github.io/eforth/docs/eForthOverviewv5.pdf). I like the idea and decided to pick it up.

## eForth now - What did I change!

1. <b>100% C/C++ with multi-platform support</b>. Though classic implementation of primitives in assembly language and scripted high-level words gave the power to Forth, it also became the hurtle for newbies. Because they have to learn the assembly and Forth syntax before peeking into the internal beauty of Forth.
2. <b>Dictionary is just an array</b>. It's remodeled from linear memory linked-list to an array (or a vector in C++'s term) of words.
    + To search for a word, simply scan the name string of dictionary entries. So, to define a new word during compile time is just to append those found word pointers to the its parameter array one by one.
    + To execute become just a walk of the word pointers in the array. This is our inner interpreter.
    + Hashtables might go even faster but we'll try that later.
    
3. <b>Data and Return Stacks are also arrays</b>. With push, pop and [] methods to clarify intentions.
4. <b>Parameter fields are all arrays</b>. Why not! They can be dynamically expanded while compiling. Or changed on the fly in runtime i.e. self-morphing code. This can be a "scary" feature for future Forth.
5. <b>No vocabulary, or meta-compilation</b>. These black-belt skills of Forth greatness are dropped to keep the focus on core concepts.
6. <b>Multi-threading and message passing are available</b> From v5.0 and on, multi-core platform can utilize Forth VMs running in parallel. see the multi-threading section below for details
    + A thread pool is built-in. Size is defaults to number of cores.
    + Message Passing send/recv with pthread mutex waiting.
    + IO and memory update can be synchronized with lock/unlock.

## Rolling your own C-based Forth?

If you are fluent in C/C++ and in the process of building your own Forth, skipping the verbage, the easiest path to gain understanding of how things work together is to download [release v4.2](https://github.com/chochain/eforth/releases/tag/v4.2.2) and work from there.

In the release, a heavily commented *ceforth.cpp*, the companion *ceforth.h*, and a *config.h*. Altogether, about 800 lines. Check them out!

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
    
### Linux, MacOS, Cygwin, or Raspberry Pi

    > make
    > ./tests/eforth             # to bring up the Forth interpreter
    > type> words⏎               \ to see available Forth words
    > type> 1 2 +⏎               \ see Forth in action
    > type> bye⏎  or Ctrl-C      \ to exit eForth

    Once you get pass the above, try the lessons by Dr. Ting.
    > ./tests/eforth < ./tests/demo.fs

    Pretty amazing stuffs! To grasp how they were done, study the
    individual files (*.fs) under ~/tests/demo.
    
    Note: MacOS added, thanks to Kristopher Johnson's work.

### Windows  (Console App)

I haven't develop anything useful on Windows for a long time. Just bearly got this compiled on an 2007 Windows7 box. So, take it with a grain of salt. I'm hoping someone can make it more streamlined.

    > install and run Visual Studio on your box
    > under the root directory, open the solution file eforth.sln (which points to project platform/eforth.vcxproj)
    > Menu bar -> Build -> Build Solution   (default to Debug/64-bit)
    > in a Command window, find and run eforth.exe under tests sub-directory
    > type> words⏎               \ to see available Forth words
    > type> 1 2 +⏎               \ see Forth in action
    > type> bye⏎  or Ctrl-C      \ to exit eForth

    Note: Windows multi-threading seems to work but 2x slower. 
        * I only have a 2-core Win box. Do let me know if it goes further. 8-)
        * No CPU affinity. The code might need to be namespaced to avoid conflicts with Windows include files.
    
### WASM

    > ensure you have Emscripten (WASM compiler) installed and configured
    > or, alternatively, you can utilize docker image from emscripten/emsdk
    > type> make wasm
    > type> python3 tests/cors.py        # supports COOP
    > from your browser, open http://localhost:8000/tests/eforth.html

Note: For multi-threading to work, browser needs to receive Cross-Origin policies [here for detail](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Opener-Policy) in the response header. A Python script *~/tests/cors.py* is provided to solve the issue. The same needed to be provided if you use other web server.

### ESP32

    > ensure your Arduino IDE have ESP32 libraries installed
    > update ESP32 compiler.optimization flags in ~/hardware/platform.txt to -O3 (default -Os)
    > open eforth.ino with Arduino IDE
    > inside eforth.ino, modify WIFI_SSID and WIFI_PASS to point to your router
    > open Arduino Serial Monitor, set baud 115200 and linefeed to 'Both NL & CR'
    > compile and load
    > if successful, web server IP address/port and eForth prompt shown in Serial Monitor
    > from your browser, enter the IP address to access the ESP32 web server
    
Note: Most ESP32 are dual-core. However core0 is dedicated to WiFi and FreeRTOS house keeping. Forth tasks will be tied to core1 only. So, multi-threading is possible but no performance gain. Actually, singled-threaded v4.2 does a bit better.

### Experimental - back to classic linear-memory model.

Instead of using vectors (i.e. pf, p1, p2) to keep codes and parameters, this implementation follows classic Forth's model using one big block of parameter memory with words laid down contiguoursly. With 32-bit data, subroutine threaded but hybrid with 16-bit xt offset (to reduce one lookup).

It is stable but tweaked from time to time and works better with WASM's memory model. It is used as the foundation for [weForth](https://github.com/chochain/weForth). 

    > make 50x
    > ./tests/eforth50x

### Experimental - An effort to modernize Forth.

Hinted by Sean Pringle's [Rethinking Forth](https://github.com/seanpringle/reforth) and Travis Bemann's wornderful [zeptoforth](https://github.com/tabemann/zeptoforth). Nested module (or sub-words), simplified control structures are attemped. Still very much a work in progress.

    > git checkout reforth
    > make
    
Note:
    Code:: FV<Code*> vt[] - virtual table for current namespace
    
    FV<Code*> nspace - namespace stack

    Code Node: 

    +------+-----+-----+------+----------+-----------------+
    | LINK | PFA | NSA | LAST | name-str | code/parameters |
    +------+-----+-----+------+----------+-----------------+

     0          0          0    <= NSA (namespace address)
      \          \          \
    <--[ W1 ] <-- [ W2 ] <-- [ W3 ] <-- LAST (word linked-list)
             \          \          \
              \         NSA         [ A ] <-- [ B ] <-- [ C ] <-- W3.LAST
              NSA         \
                \          [ A ] <-- [ B ] <-- [ X ] <-- W2.LAST
                 \
                  [ A ] <-- [ B ] <-- W1.LAST
                                 \
                                  [ A ] <-- [ X ] <-- [ Y ] <-- W1B.LAST
                                  

## Multi-threading - for release v5.0 and after
Forth has been supporting multi-tasking since the 70's. They are single-CPU round-robin/time-slicing systems mostly. Modern system has multiple cores and Forth can certainly take advantage of them. However, unlike most of the matured Forth word sets, multi-threading/processing words are yet to be standardized and there are many ways to do it.

### Design & Implementation

    > each VM has it's own private ss, rs, tos, ip, and state
    > multi-threading, instead of multi-processing, with shared dictionary and parameter memory blocks.
    > pthread.h is used. It is a common POSIXish library supported by most platforms. I have only tried the handful on hands, your mileage may vary.
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
|recv|( -- v1 v2 .. vn )|wait, until message to arrive|HOLD=>NEST|
|pull|( n t -- )|forced fetch stack elements from a completed task|current NEST<br/>target STOP|
|bcast|( n -- )|not implemented yet, TODO|sender NEST<br/>receivers HOLD|
|clock|( -- n )|fetch microsecond since Epoch, useful for timing|

#### Example1 - parallel jobs (~/tests/demo/mtask.fs)

    > : once 999999 for rank drop next ;            \ 1M cycles
    > : run clock negate once clock + . ." ms" cr ; \ benchmark
    > ' run constant xt                             \ keep the xt
    > : jobs 1- for xt task start next ;            \ tasks in parallel
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

#### Example2 - producer-consumer (~/tests/demo/mpi.fs)

    > 0 constant pp                           \ producer task id
    > 0 constant cc                           \ consumer task id
    > : sndr
        1000 ms                               \ delay to simulate some processing
        1 2 3 4 4 cc send                     \ send 4 items from stack
        lock ." sent " cr unlock ;            \ locked IO before write
    > : rcvr
        recv                                  \ wait for sender
        + + +                                 \ sum received 4 items
        lock ." sum=" . cr unlock ;           \ locked IO before write
    > ' sndr task to pp
    > ' rcvr task to cc
    > cc start                                \ start receiver task
    > pp start                                \ start sender task
    > pp join cc join                         \ wait for completion

<pre>
[06.1]>> started on T1
[06.1]>> waiting
[07.1]>> started on T2
[06.1]>> sending 4 items to VM6.1
sent 
[07.3]>> finished on T2
[00.3]>> VM7 joint
[06.3]>> received => state=3
sum=10
[06.3]>> finished on T1
[00.3]>> VM6 joint
</pre>

#### Example3 - fetch result(s) from completed task (~/tests/demo/mpi_pull.fs)

    > : sum 0 1000000 for i + next ;          \ add 0 to 1M
    > ' sum task constant tt                  \ create the task
    > tt start tt join                        \ run and wait for completion
    > 1 tt pull ." total=" .                  \ pull the sum

<pre>
[00.3]>> joining VM7
[07.1]>> started on T1
[07.3]>> finished on T1
[00.3]>> VM7 joint
pulled 1 items from VM7.0
total= 1784293664 -1 -> ok
</pre>

## Source Code Directories

    + ~/src       - multi-threaded, dynamic vector-based, object threading
    + ~/platform  - platform specific code for C++, ESP32, Windows, and WASM
    + ~/orig      - archive from Dr. Ting and my past works
    +    /33b     - refactored ceForth_33, separate ASM from VM (used in eForth1 for Adruino UNO)
    +    /ting    - ceForth source codes collaborated with Dr. Ting
    +    /esp32   - esp32forth source codes collaborated with Dr. Ting
    +    /40x     - my experiments, refactor _40 into vector-based subroutine-threaded, with 16-bit offset
    +    /50x     - my experiments, add multi-threading to _40

## Benchmark and Tuning

### Desktop PC - 10K*10K cycles on 3.2GHz AMD**
#### v4.x single-threaded
    + 4452ms: ~/orig/ting/ceforth_36b, linear memory, 32-bit, token threading
    + 1450ms: ~/orig/ting/ceForth_403, dict/pf array-based, subroutine threading
    + 1050ms: ~/orig/40x/ceforth, subroutine indirect threading, with 16-bit offset
    +  890ms: ~/orig/40x/ceforth, inner interpreter with cached xt 16-bit offsets
    +  780ms: ~/src/eforth, v4.2 dynamic vector, object threading (gcc -O2)
    
#### v5.x ~/src/ceforth, multi-threading capable, dynamic vector, object threading
    +  812ms: v5.0, multi-threaded (gcc -O2)
    +  732ms: v5.0, multi-threaded (gcc -O3)
    +  731ms: v5.0, single-threaded (gcc -O3) => not much overhead with MT
    
#### experimental ~/orig/50x/ceforth multi-threading capable, linear-memory, 32-bit IU
    +  843ms: v5.0 50x32 branch (gcc -O2)
       * program spent >50% in nest() - gprof/valgrind/cachegrind
       * 16-bit IU fetch + dispatch: Ir/Dr = 2.3M/0.5M (810ms)
       * 32-bit Param hardcopy     : Ir/Dr = 3.8M/1.1M (930ms)
       * 32-bit Param reference    : Ir/Dr = 3.1M/0.8M (843ms) <== 32-bit best
       * 32-bit Param pointer      : Ir/Dr = 3.2M/0.9M (899ms)
    +  873ms: v5.0 50x32 branch (gcc -O3)
       * slower, due to inline find() into forth_core() which crowded cache.
         Note: this doesn't seem to bother WASM.
                
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

I try to release allocated blocks before exiting, however due to the dynamic alloc and resizing of std::vector, eForth dictionary hold on to many Code objects and the names string generated with them, valgrind (or similar tool) could reports lost (or leak). Though these memory blocks should all be reclaimed by the OS, it is something to be mindful of.

### Multiple or Unified Parameter Field Consideration
Current implementation utilize C++ vector as the core storage. Inside a Code object, there are pf, p1, p2 vectors to store branching words similar to that of an AST (Abstract Syntax Tree). The alternative is to stick all words into a single parameter field as done in classic Forth. I have created a branch **one_pf** doing exactly the same just to check it out. Also, tried polymorphic inner interpreter. So, are they better?

    + Branching microcode look cleaner. 2-bit **VM.stage** flag can be replaced by a 1-bit **VM.jmp** status. No big deal.
    + dump and see are easier to implement, but
    + Runs 4~8x slower using recursive nest() i.e. Forth inner interpreter,
    + Improved to 2x slower using iterative nest()
    + Also, polymorphic slows down additional 5%. Most likely due to extra vtable lookup.
    
So, what **cachegrind** said for **100M loop** tight loops and **chacha.fs** a CPU intensive?

    | Op          | 100M loop | chacha.fs |
    |-------------|-----------|-----------|
    | Data Read   | +30%      | +32%      |
    | Branches    | +25%      | +30%      |
    | Mispred     | similar   | similar   |
    | Instruction | +20%      | +40%      |
    
Apparently, grown ~30% in all aspects. I think because having branching primitives, i.e. **_if/_else/_then**, **for/next**, in C++ prevent the extra fetch of VM branches. Sort of the difference between having hardware and software branchers. However, my gut feeling is the difference shouldn't be so dramatic especially with the recursive nest(). More research on this...

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
    +  Though the goal of Dr. Ting's is to demonstrate how a Forth can be easily understood and cleanly constructed. However, the token threading method used is costly (slow) because each call needs 2 indirect lookups (token->dict, dict->xt). On top of that, C/C++ call-frame needs to be setup/teardown. It is worsen by the branch prediction missing every call stalling the CPU pipeline. Bad stuffs!
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
* CC: 20250610: maintenance and memory leak check
    + Refactor
        - Macros to reduce verbosity i.e. VM referenced TOS, SS, RS, BRAN, BTGT
        - Group IO functions to forth_sys module
        - Macros to clarify intention, i.e. NEST, BASE, ADD_W
        - Code references replace Code pointers
        - Rename ms=>clock, delay=>ms (adhere to Forth Standard)
        - Add destructors to deallocate (reduce valgrind's complaints)
    + Enhance multi-threading
        - Use std::thread instead of pthread (except device specific CPU affinity)
        - Handle recursive include - Save/Restore WP
        - Refined forth_vm state machine transition (QUERY, HOLD, NEST, STOP)
    + Enhance debugging
        - Add dict() to detail dictionary entries
        - Add dump() to show memory/parameter field's content
