\page 5
### Why
Even with vocabulary, multi-tasking, and metacompilation (the black-belt stuffs of Forth greatness) delibrately dropped to reduce the complexity, eForth traditionally uses linear memory to host words of the entire dictionary, including codes and their parameters with a backward linked-list and hence the well-known term threading. This model is crucial when memory is scarce or compiler resource is just underwhelming. It, however, does create extra hurdle that sometimes hinder the learning of newbies.

### Synopsis
With C++ implementation of Forth as the back drop, I experimented with verious changes to the classic Forth linear-memory model detailed in next section. By manually turning the rocks under each memory management scenarions to find out what could we do better.
At the end of the day, the simplest vector-based token-threading implementation with just C++ compiler optimizer actually out perform all the following tuning, 10% on AMD and 25% on ESP32.
So, respect Dr. Ting's original goal of creating an educational platform, I have decided to stick with the simplest, shortest code-base.

### Experimental Changes - what have we tested on ceForth_40

** 1: Separation of parameter memory and dictionary**
<pre>
+ it makes dictionary uniform size which eliminates the need for link field
- the down side is that it requires manual array size tuning
</pre>
   
eForth_33 uses functions i.g. CODE, LABEL, HEADER, ... as macros to assemble dictionary which just mimicing how classic Forth creates the dictionary. Visually, it is not that different from using Forth which is challenging for new comers.

** 2: Use struct to host a dictionary entry**
<pre>
struct Code {
    string name;
    void   (*xt)(void);
    int    immd;
};
+ it simpify the classic field management of Forth
- extra space to store the name and code pointers
</pre>

** 3: Build array-based dictionary**
<pre>
Code* primitives[] = {
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

C language on modern OS have good libraries for I/O interfaces, the classic way of TX/RX bytes or using block to manage files are no more essential. They were necessary, but no more for today's Forth.

** 4: Use Streams for input/output**
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

Dr. Ting latest ceForth uses the token indirect threading model. It is great for learning as wll as being portable. The extra lookup for token to function pointer makes it slower (at about 50%) of a subroutine indirect threading model.

** 5: Using 16-bit xt offset in parameter field (instead of full 32 or 64 bits)**
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
