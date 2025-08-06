///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"

extern List<U8, 0> pmem;           ///< parameter memory block
extern U8          *MEM0;          ///< base pointer of pmem

#if !DO_MULTITASK
VM _vm0;                           ///< singleton, no VM pooling
VM& vm_get(int id) { return _vm0; }/// * return the singleton
void uvar_init() {
    U8 *b = &pmem[pmem.idx++];     ///< *base
    *b = 10;                       /// * default 10
    _vm0.id   = 0;                 /// * VM id
    _vm0.base = (IU)(b - MEM0);    /// * base idx
    pmem.idx  = ALIGN16(pmem.idx); /// 16-byte aligned
}

#else // DO_MULTITASK
List<VM, E4_VM_POOL_SZ> _vm;       ///< VM pool
///
///> VM pool
///
VM& vm_get(int id) {
    return _vm[(id >= 0 && id < E4_VM_POOL_SZ) ? id : 0];
}

extern void add_du(DU v);          ///< add data unit to pmem
extern void nest(VM &vm);          ///< Forth inner loop
///
///> VM messaging and IO control variables
///
int      VM::NCORE   = 1;          ///< default 1, updated in init
bool     VM::io_busy = false;
MUTEX    VM::tsk;
MUTEX    VM::io;
COND_VAR VM::cv_tsk;
COND_VAR VM::cv_io;

///============================================================
///
///> Thread pool
///
/// Note: Thread pool is universal and singleton,
///       so we keep them in C. Hopefully can be reused later
MUTEX    _evt;                     ///< mutex for queue access
COND_VAR _cv_evt;                  ///< for pool exit

bool            _quit = 0;         ///< thread pool exit flag
List<THREAD, 0> _pool;             ///< thread pool
List<VM*,    0> _que;              ///< event queue

void _event_loop(int rank) {
    VM *vm = NULL;
    while (true) {
        {
            XLOCK(_evt);                          ///< lock queue
            WAIT(_cv_evt, []{ return _que.idx || _quit; });
            
            if (_que.idx) vm = _que.pop();        ///< lock reaccquired
            else if (_quit) break;                /// * bail
            
            NOTIFY(_cv_evt);                      /// * notify one
        }
        VM_LOG(vm, ">> started on T%d", rank);
        vm->rs.push(DU0);                         /// exit token
        nest(*vm);
        VM_LOG(vm, ">> finished on T%d", rank);

        vm->stop();                               /// * release any lock
    }
}

void t_pool_init() {
    VM::NCORE = thread::hardware_concurrency();   /// * number of cores
    
    _pool = new THREAD[E4_VM_POOL_SZ];            ///< a thread each core
    _que  = new VM*[E4_VM_POOL_SZ];               ///< queue with spare 

    if (!_pool.v || !_que.v) {
        printf("thread_pool_init allocation failed\n");
        exit(-1);
    }
    
    /// setup thread pool and CPU affinity
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {     ///< loop thru ranks
        THREAD t(_event_loop, (int)_vm[i].id);    ///< create thread
        _pool[i] = std::move(t);                  /// * transfer thread ownership

#if __has_include(<sched.h>) && !defined(__CYGWIN__)
        pthread_t pt = _pool[i].native_handle();
        cpu_set_t set;
        CPU_ZERO(&set);                           /// * clear affinity
        CPU_SET(i % VM::NCORE, &set);             /// * set CPU affinity
        int rc = pthread_setaffinity_np(          /// * set core affinity
            pt, sizeof(cpu_set_t), &set
        );
        if (rc !=0) {
            printf("thread[%d] failed to set affinity: %d\n", i, rc);
        }
#endif // __has_include(<sched.h>)
    }
    printf("CPU cores=%d, thread pool[%d] initialized\n", VM::NCORE, E4_VM_POOL_SZ);
}

void t_pool_stop() {
    {
        GUARD(_evt);
        _quit = true;                             /// * stop event queue
        NOTIFY_ALL(_cv_evt);
    }
    printf("joining thread ");
    for (int i = E4_VM_POOL_SZ - 1; i >= 0; i--) {
        printf("%d ", i);
        _pool[i].join();
    }
    printf("done!\n");
}

void uvar_init() {
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        U8 *b = &pmem[pmem.idx++];                ///< *base
        *b = 10;                                  /// * default 10
        _vm[i].id   = i;                          /// * VM id
        _vm[i].base = (IU)(b - MEM0);             /// * base idx
    }
    pmem.idx = DALIGN(pmem.idx);                  /// DU aligned
}

int task_create(IU pfa) {
    int i = E4_VM_POOL_SZ - 1;

    GUARD(VM::tsk);
    
    while (i > 0 && _vm[i].state != STOP) --i;
    if (i > 0) {
        _vm[i].reset(pfa, HOLD);                  /// ready to run
    }
    NOTIFY(VM::cv_tsk);

    return i;
}

void task_start(int tid) {
    if (tid == 0) {
        printf("main task (tid=0) already running.\n");
        return;
    }
    VM &vm = vm_get(tid);
    
    GUARD(_evt);
    _que.push(&vm);                              /// create event
    NOTIFY(_cv_evt);
}
///==================================================================
///
///> VM methods
///
void VM::join(int tid) {
    VM &vm = vm_get(tid);
    VM_LOG(this, ">> joining VM%d", vm.id);
    {
        XLOCK(tsk);
        WAIT(cv_tsk, [&vm]{ return vm.state==STOP; });
        NOTIFY(cv_tsk);
    }
    VM_LOG(this, ">> VM%d joint", vm.id);
}
///
///> hard copying data stacks, behaves like a message queue
///
void VM::_ss_dup(VM &dst, VM &src, int n) {
    dst.ss.push(dst.tos);            /// * push dest TOS
    dst.tos = src.tos;               /// * set dest TOS
    src.tos = src.ss[-n];            /// * set src TOS
    for (int i = n - 1; i > 0; --i) {
        dst.ss.push(src.ss[-i]);     /// * passing stack elements
    }
    src.ss.idx -= n;                 /// * pop src by n items
}
void VM::reset(IU cfa, vm_state st) {
    rs.idx     = ss.idx = 0;
    ip         = cfa;
    tos        = -DU1;
    state      = st;
    compile    = false;
    *(MEM0 + base) = 10;             /// * default radix = 10
}
void VM::stop() {
    GUARD(tsk);
    state = STOP;
    NOTIFY(cv_tsk);
}
///
///> send to destination VM's stack (blocking)
///
void VM::send(int tid, int n) {      ///< ( v1 v2 .. vn -- )
    VM& vm = vm_get(tid);            ///< destination VM

    XLOCK(tsk);
    WAIT(cv_tsk, [vm]{ return vm.state==HOLD || _quit; });

    if (_quit) return;               /// * nothing to do, bail

    VM_LOG(&vm, ">> sending %d items to VM%d.%d", n, tid, vm.state);
    _ss_dup(vm, *this, n);           /// * pass n params as a msg queue
    vm.state = NEST;                 /// * unblock target task

    NOTIFY(cv_tsk);
}
///
///> receive from source VM's stack (blocking)
///
void VM::recv() {                    ///< ( -- v1 v2 .. vn )
    vm_state st = state;             ///< keep current VM state
    {
        GUARD(tsk);                  /// * lock tasker
        state = HOLD;                /// * pending state for message
        NOTIFY(cv_tsk);
    }
    VM_LOG(this, ">> waiting");
    {
        XLOCK(tsk);
        WAIT(cv_tsk, [this]{ return state!=HOLD || _quit; }); /// * block until msg arrive
        state = st;                  /// * restore VM state
    
        NOTIFY(cv_tsk);
    }
    VM_LOG(this, ">> received => state=%d", st);
}
///
///> broadcasting to all receving VMs
///
void VM::bcast(int n) {
    /// CC: TODO
}
///
///> pull n items from stopped/completed task
///
void VM::pull(int tid, int n) {
    VM& vm = vm_get(tid);            ///< source VM

    XLOCK(tsk);
    WAIT(cv_tsk, [vm]{ return vm.state==STOP || _quit; });
    
    if (!_quit) {
        _ss_dup(*this, vm, n);       /// * retrieve from completed task
        printf(">> pulled %d items from VM%d.%d\n", n, vm.id, vm.state);
    }
    
    NOTIFY(cv_tsk);
}
///
///> IO control (can use atomic _io after C++20)
///
/// Note: after C++20, _io can be atomic.wait
void VM::io_lock() {
    XLOCK(io);                       ///< wait for IO
    WAIT(cv_io, []{ return !io_busy; });
    
    io_busy = true;                  /// * lock
    
    NOTIFY(cv_io);
}

void VM::io_unlock() {
    GUARD(io);
    io_busy = false;                 /// * unlock
    NOTIFY(cv_io);
}
#endif // DO_MULTITASK
