///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"

FV<VM> _vm;                        ///< VM pool
///
///> VM pool
///
VM& vm_get(int id) {
    return _vm[(id >= 0 && id < E4_VM_POOL_SZ) ? id : 0];
}

#if DO_MULTITASK
extern List<U8, 0> pmem;           ///< parameter memory block
extern U8          *MEM0;          ///< base pointer of pmem
extern void add_du(DU v);          ///< add data unit to pmem
extern void nest(VM &vm);          ///< Forth inner loop
///
///> VM messaging and IO control variables
///
int  VM::NCORE   = thread::hardware_concurrency();  ///< number of cores
bool VM::io_busy = false;
mutex              VM::tsk;
mutex              VM::io;
condition_variable VM::cv_tsk;
condition_variable VM::cv_io;

///============================================================
///
///> Thread pool
///
/// Note: Thread pool is universal and singleton,
///       so we keep them in C. Hopefully can be reused later
#include <queue>
bool               _done    = 0;   ///< thread pool exit flag
FV<thread>         _pool;          ///< thread pool
queue              _que;           ///< event queue
mutex              _mtx;           ///< mutex for queue access
condition_variable _cv_mtx;        ///< for pool exit

void _event_loop(int rank) {
    VM *vm;
    while (true) {
        {
            unique_lock<mutex> lck(_mtx);   ///< lock queue
            _cv_mtx.wait(lck,      ///< release lock and wait
                []{ return _que.idx > 0 || _done; });
            if (_done) return;     ///< lock reaccquired
            vm = _que.pop();       ///< get next event
        }
        _cv_mtx.notify_one();

        VM_LOG(vm, ">> started on T%d", rank);
        vm->rs.push(DU0);          /// exit token
        while (vm->state==HOLD) nest(*vm);
        VM_LOG(vm, ">> finished on T%d", rank);
        
        vm->stop();                /// * release any lock
    }
}

void t_pool_init() {
    /// setup VM and it's user area (base pointer)
    _vm.reserve(E4_VM_POOL_SZ);
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        dict[0]->append(new Var(10));             /// * borrow dict[0] for base
        _vm[i].id   = i;                          /// * VM id
        _vm[i].base = (U8*)&dict[0]->pf[0]->q[i]; /// * set pointer
    }
    
    /// setup threads
    _pool.reserve(E4_VM_POOL_SZ);
    
    cpu_set_t set;
    CPU_ZERO(&set);                               /// * clear affinity
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {     ///< loop thru ranks
        _pool[i] = thread(_event_loop, i);        /// * closure with rank id
        CPU_SET(i % VM::NCORE, &set);             /// * CPU affinity
        int rc = pthread_setaffinity_np(          /// * set core affinity
            _pool[i].native_handle(),
            sizeof(cpu_set_t), &set
        );
        if (rc !=0) {
            printf("thread[%d] failed to set affinity: %d\n", i, rc);
        }
    }
    printf("thread pool[%d] initialized\n", E4_VM_POOL_SZ);
}

void t_pool_stop() {
    {
        lock_guard<mutex> lck(_mtx);        ///< lock queue
        _done = true;
    }
    _cv_mtx.notify_all();

    printf("joining thread...");
    for (thread t: _pool) {
        t.join();
    }
    _pool.clear();
    
    printf(" done!\n");
}

int task_create(IU pfa) {
    int i = E4_VM_POOL_SZ - 1;
    {
        lock_guard<mutex> lck(VM::tsk);
        while (i > 0 && _vm[i].state != STOP) --i;
        if (i > 0) {
            _vm[i].reset(pfa, HOLD);   /// ready to run
        }
    }
    VM::cv_tsk.notify_one();
    
    return i;
}

void task_start(int tid) {
    if (tid == 0) {
        printf("main task (tid=0) already running.\n");
        return;
    }
    VM &vm = vm_get(tid);
    {
        lock_guard<mutex> lck(_mtx);   ///< lock queue
        _que.push(&vm);
    }
    _cv_mtx.notify_one();
}
///==================================================================
///
///> VM methods
///
void VM::join(int tid) {
    VM &vm = vm_get(tid);
    VM_LOG(this, ">> joining VM%d", vm.id);
    {
        unique_lock<mutex> lck(tsk);   ///< lock tasker
        cv_tsk.wait(lck, [&vm]{ return vm.state==STOP; });
    }
    cv_tsk.notify_one();
    VM_LOG(this, ">> VM%d joint", vm.id);
}
///
///> hard copying data stacks, behaves like a message queue
///
void VM::_ss_dup(VM &dst, VM &src, int n) {
    dst.ss.push(dst.tos);           /// * push dest TOS
    dst.tos = src.tos;              /// * set dest TOS
    src.tos = src.ss[-n];           /// * set src TOS
    for (int i = n - 1; i > 0; --i) {
        dst.ss.push(src.ss[-i]);    /// * passing stack elements
    }
    src.ss.idx -= n;                /// * pop src by n items
}
void VM::reset(IU cfa, vm_state st) {
    rs.idx     = ss.idx = 0;
    ip         = cfa;
    tos        = -DU1;
    state      = st;
    compile    = false;
    *(MEM0 + base) = 10;            /// * default radix = 10
}
void VM::stop() {
    {
        lock_guard<mutex> lck(tsk); /// * lock tasker
        state = STOP;
    }
    cv_tsk.notify_one();            /// * release join lock if any
}
///
///> send to destination VM's stack (blocking)
///
void VM::send(int tid, int n) {      ///< ( v1 v2 .. vn -- )
    VM& vm = vm_get(tid);            ///< destination VM
    {
        unique_lock<mutex> lck(tsk); ///< lock tasker
        cv_tsk.wait(lck,             ///< release lock and wait
            [&vm]{ return _done ||   /// * Forth exit, or
                vm.state==HOLD;      /// * waiting for messaging
            });
        VM_LOG(&vm, " >> sending %d items to VM%d.%d", n, tid, vm.state);
        _ss_dup(vm, *this, n);       /// * pass n variables as a queue
        
        vm.state = NEST;             /// * unblock target task
    }
    cv_tsk.notify_one();
}
///
///> receive from source VM's stack (blocking)
///
void VM::recv() {                    ///< ( -- v1 v2 .. vn )
    vm_state st = state;             ///< keep current VM state
    {
        lock_guard<mutex> lck(tsk);  ///< lock tasker
        state = HOLD;
    }
    cv_tsk.notify_one();

    VM_LOG(this, " >> waiting");
    {
        unique_lock<mutex> lck(tsk); ///< lock tasker
        cv_tsk.wait(lck,             /// * block until message arrival
            [this]{ return _done ||  /// * wait till Forth exit, or
                state != HOLD;       /// * message arrived
            });
        VM_LOG(this, " >> received => state=%d", st);
        state = st;                  /// * restore VM state
    }
    cv_tsk.notify_one();
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
    {
        unique_lock<mutex> lck(tsk); ///< lock tasker
        cv_tsk.wait(lck,             ///< release lock and wait
            [&vm]{ return _done ||   /// * Forth exit
                 vm.state==STOP;     /// * init before task start
            });
        if (_done) return;
        
        _ss_dup(*this, vm, n);       /// * retrieve from completed task
    
        printf(">> pulled %d items from VM%d.%d\n", n, vm.id, vm.state);
    }
    cv_tsk.notify_one();
}
///
///> IO control (can use atomic _io after C++20)
///
/// Note: after C++20, _io can be atomic.wait
void VM::io_lock() {
    unique_lock<mutex> lck(io);
    cv_io.wait(lck, []{ return !io_busy; });
    io_busy = true;                /// * lock
    cv_io.notify_one();
}

void VM::io_unlock() {
    {
        lock_guard<mutex> lck(io);
        io_busy = false;           /// * unlock
    }
    cv_io.notify_one();
}
#endif // DO_MULTITASK
