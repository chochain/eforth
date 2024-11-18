\page 5
### Why
On recent systems and micro-controllers, multiple cores CPUs are common. We have chosen to add multi-threading to eForth that can be enabled by conditional compilation.

### Synopsis
C++ pthread library is readily aviable for most of the platforms, false-sharing and affinity are verifed by 'perf stat'.
System functions are refactored into separated file ceforth_sys.cpp

### Experimental Changes - what have we tested on ceForth_50

** 1: States are created for machine status transition
<pre>
+ STOP
+ HOLD
+ QUERY
+ NEST
+ MSG
</pre>

** 2: Forth VM are created to host individual task**
<pre>
+ ip
+ tos
+ ss
+ rs
+ state
+ compile
+ base
</pre>

** 3: Thread Pool are created
<pre>
+ based on hardware cores
</pre>
   
** 4: Methods follows MPI message passing, i.e. Actor, model
<pre>
    void send(int tid, int n);     ///< send onto destination VM's stack (blocking)
    void recv(int tid, int n);     ///< receive from source VM's stack (blocking)
    void bcast(int n);             ///< broadcast to all receivers
    void join(int tid);            ///< wait for the given task to end
</pre>

** 5: IO/Memory mutex and condition variables for synchronization
</pre>
