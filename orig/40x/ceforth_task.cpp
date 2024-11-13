///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"

List<VM, E4_VM_POOL_SZ> _vm;                 ///< VM pool
///
///> VM pool
///
VM& vm_get(int id) {
    return _vm[(id >= 0 && id < E4_VM_POOL_SZ) ? id : 0];
}

#if DO_MULTITASK
extern List<U8, 0> pmem;
extern void add_du(DU v);
extern void nest(VM &vm);
///
///> Thread pool
///
/// Note: Thread pool is universal and singleton,
///       so we keep them in C. Hopefully can be reused later
bool               _done    = 0;   ///< thread pool exit flag
List<thread, 0>    _pool;          ///< thread pool
List<VM*,    0>    _que;           ///< event queue
mutex              _mtx;           ///< mutex for memory access
condition_variable _cv_mtx;        ///< for pool exit

void _event_loop(int rank) {
    VM *vm;
    while (true) {
        {
            unique_lock<mutex> lck(_mtx);
            _cv_mtx.wait(lck,      ///< release lock and wait
                     []{ return _que.idx > 0 || _done; });
            if (_done) return;     ///< lock reaccquired
            vm = _que.pop();       ///< get next event
        }
        printf(">> T%d=VM%d.%d started IP=%4x\n", rank, vm->_id, vm->state, vm->_ip);
        vm->_rs.push(DU0);         /// exit token
        while (vm->state==HOLD) nest(*vm);
        printf(">> T%d=VM%d.%d done\n", rank, vm->_id, vm->state);
    }
}

void t_pool_init() {
    _pool = new thread[VM::NCORE];           ///< a thread each core
    _que  = new VM*[VM::NCORE * 2];          ///< queue with spare 
    
    if (!_pool.v || !_que.v) {
        printf("thread_pool_init allocation failed\n");
        exit(-1);
    }
    /// setup VMs user area
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        _vm[i].base = &pmem[pmem.idx];       /// * HERE
        _vm[i]._id  = i;                     /// * VM id
        add_du(10);                          /// * default base=10
    }
    /// setup threads
    cpu_set_t set;
    CPU_ZERO(&set);                          /// * clear affinity
    for (int i = 0; i < VM::NCORE; i++) {    ///< loop thru ranks
        _pool[i] = thread(_event_loop, i);   /// * closure with rank id
        CPU_SET(i, &set);
        int rc = pthread_setaffinity_np(     /// * set core affinity
            _pool[i].native_handle(),
            sizeof(cpu_set_t), &set
        );
        if (rc !=0) {
            printf("thread[%d] failed to set affinity: %d\n", i, rc);
        }
    }
    printf("thread pool[%d] initialized\n", VM::NCORE);
}

void t_pool_stop() {
    {
        lock_guard<mutex> lck(_mtx);
        _done = true;
    }
    _cv_mtx.notify_all();

    printf("joining thread...");
    for (int i = 0; i < VM::NCORE; i++) {
        _pool[i].join();
        printf("%d ", i);
    }
    _pool.clear();
    
    printf(" done!\n");
}

int task_create(IU pfa) {
    int i = E4_VM_POOL_SZ - 1;
    while (i > 0 && _vm[i].state != STOP) --i;
    
    if (i > 0) _vm[i].reset(pfa, HOLD);
    
    return i;
}

void task_start(int tid) {
    if (tid == 0) {
        printf("skip, main task (tid=0) running.\n");
        return;
    }
    VM &vm = vm_get(tid);
    {
        lock_guard<mutex> lck(_mtx);
        _que.push(&vm);
    }
    _cv_mtx.notify_one();
}
///
///> Messaging control
///
int  VM::NCORE   = thread::hardware_concurrency();  ///< number of cores
bool VM::io_busy = false;
mutex              VM::msg;
mutex              VM::io;
condition_variable VM::cv_msg;
condition_variable VM::cv_io;

void VM::join(int tid) {
    VM& vm = vm_get(tid);
    {
        unique_lock<mutex> lck(msg);
        cv_msg.wait(lck, [&vm]{ return vm.state==STOP; });
    }
    cv_msg.notify_one();
}
void VM::_ss_dup(VM &dst, VM &src, int n) {
    dst._ss.push(dst._tos);          /// * push dest TOS
    dst._tos = src._tos;             /// * set dest TOS
    src._tos = src._ss[-n];          /// * set src TOS
    for (int i = n - 1; i > 0; --i) {
        dst._ss.push(src._ss[-i]);   /// * passing stack elements
    }
    src._ss.idx -= n;                /// * pop src by n items
}

void VM::reset(IU ip, vm_state st) {
    _rs.idx    = _ss.idx = 0;
    _ip        = ip;
    _tos       = -DU1;
    state      = st;
    compile    = false;
    *(DU*)base = 10;
}
///
///> send to destination VM's stack (blocking)
///
void VM::send(int tid, int n) {      ///< ( v1 v2 .. vn -- )
    VM& vm = vm_get(tid);            ///< destination VM
    {
        unique_lock<mutex> lck(msg);
        cv_msg.wait(lck,             ///< release lock and wait
            [&vm]{ return _done ||   /// * Forth exit
                vm.state==HOLD  ||   /// * init before task start
                vm.state==MSG;       /// * block on dest task here
            });
        printf(">> VM%d.%d send %d items to VM%d.%d\n", _id, state, n, tid, vm.state);
        _ss_dup(vm, *this, n);       /// * passing n variables
        
        if (vm.state==MSG) {         /// * messaging completed
            vm.state = NEST;         /// * unblock target task
        }
    }
    cv_msg.notify_one();
}
///
///> receive from source VM's stack (blocking)
///
void VM::recv(int tid, int n) {      ///< ( -- v1 v2 .. vn )
    VM& vm = vm_get(tid);            ///< source VM
    {
        unique_lock<mutex> lck(msg);
        auto st = state;             ///< keep current state
        if (st==STOP) {              ///< forced fetch from completed VM
            printf(">> forced recv from VM%d.%d\n", vm._id, vm.state);
            _ss_dup(*this, vm, n);   /// * retrieve from completed task
            return;                  /// * no notify needed
        }
        state = MSG;                 /// * ready for messaging
        cv_msg.wait(lck,             ///< release lock and wait
            [this, &vm]{ return
                _done           ||   /// * Forth exit
                vm.state==STOP ||    /// * until task finished or
                state!=MSG;          /// * until messaging complete
            });
        printf(">> VM%d.%d recv %d items from VM%d.%d\n", _id, state, n, tid, vm.state);
        state = st;                  /// * restore VM state
    }
    cv_msg.notify_one();
}
///
///> broadcasting to all receving VMs
///
void VM::bcast(int n) {
    ///< TODO
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
