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
bool               _done    = 0;   ///< thread pool exit flag
bool               _io_busy = 0;   ///< io control
List<thread, 0>    _pool;          ///< thread pool
List<VM*,    0>    _que;           ///< event queue
mutex              _mtx;           ///< mutex for memory access
mutex              _io;            ///< mutex for io access
condition_variable _cv_mtx;        ///< for pool exit
condition_variable _cv_io;         ///< for io control

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
        printf(">> vm[%d] started on thread[%d], vm.state=%d\n", vm->_id, rank, vm->state);
        vm->_rs.push(DU0);         /// exit token
        while (vm->state==HOLD) nest(*vm);
        printf(">> vm[%d] on thread[%d] done, vm.state=%d\n", vm->_id, rank, vm->state);
    }
}

void t_pool_init() {
    _pool = new thread[VM::RANK];
    _que  = new VM*[VM::RANK*2];
    
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
    for (int i = 0; i < VM::RANK; i++) {     ///< loop thru ranks
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
    printf("thread pool[%d] initialized\n", VM::RANK);
}

void t_pool_stop() {
    {
        lock_guard<mutex> lck(_mtx);
        _done = true;
    }
    _cv_mtx.notify_all();

    printf("joining thread...");
    for (int i = 0; i < VM::RANK; i++) {
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

void task_start(int id) {
    if (id == 0) {
        printf("skip, main task occupied.\n");
        return;
    }
    VM &vm = vm_get(id);
    {
        lock_guard<mutex> lck(_mtx);
        _que.push(&vm);
    }
    _cv_mtx.notify_one();
}
///
///> IO control (can use atomic _io after C++20)
///
/// Note: after C++20, _io can be atomic.wait
void task_wait() {
    unique_lock<mutex> lck(_io);
    _cv_io.wait(lck, []{ return !_io_busy; });
    _io_busy = true;               /// * lock
    _cv_io.notify_one();
}

void task_signal() {
    {
        lock_guard<mutex> lck(_io);
        _io_busy = false;          /// * unlock
    }
    _cv_io.notify_one();
}
///
///> Messaging control
///
int VM::RANK = thread::hardware_concurrency();  ///< 
mutex              VM::mtx;
condition_variable VM::msg;

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
    VM& dst = vm_get(tid);           ///< destination VM
    {
        unique_lock<mutex> lck(mtx);
        msg.wait(lck,                ///< release lock and wait
            [&dst]{ return _done ||  /// * Forth exit
                dst.state==HOLD  ||  /// * init before task start
                dst.state==MSG;      /// * block on dest task here
            });

        _ss_dup(dst, *this, n);      /// * passing n variables
        
        if (dst.state==MSG) {        /// * messaging completed
            dst.state = NEST;        /// * unblock dest task
        }
    }
    msg.notify_one();
}
///
///> receive from source VM's stack (blocking)
///
void VM::recv(int tid, int n) {      ///< ( -- v1 v2 .. vn )
    VM& src = vm_get(tid);           ///< source VM
    {
        unique_lock<mutex> lck(mtx);
        auto st = state;             ///< keep current state
        state = MSG;                 /// * ready for messaging
        msg.wait(lck,                ///< release lock and wait
            [this, &src]{ return
                _done           ||   /// * Forth exit
                src.state==STOP ||   /// * until task finished or
                state!=MSG;          /// * until messaging complete
            });
        
        if (src.state==STOP) {       ///< forced fetch from completed VM
            _ss_dup(*this, src, n);  /// * retrieve from completed task
        }
        state = st;                  /// * restore VM state
    }
    msg.notify_one();
}
///
///> broadcasting to all receving VMs
///
void VM::bcast(int n) {
    ///< TODO
}
#endif // DO_MULTITASK
