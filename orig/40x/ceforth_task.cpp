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
#include <atomic>
#include <mutex>
#include <condition_variable>

int                _nthread = 0;   ///< max # of threads hardware supports
bool               _done = false;  ///< pool exit flag
List<thread, 0>    _pool;          ///< thread pool
List<VM*,    0>    _que;           ///< event queue
mutex              _mtx;           ///< mutex for multithreading
condition_variable _cv_mtx;        ///< for pool exit
condition_variable _cv_msg;        ///< for messaging
atomic<int>        _io(1);         ///< for io control

void _event_loop(int pid) {
    VM *vm;
    while (true) {
        {
            unique_lock<mutex> lck(_mtx);
            _cv_mtx.wait(lck,      ///< release lock and wait
                     []{ return _que.idx > 0 || _done; });
            if (_done) return;     ///< lock reaccquired
            vm = _que.pop();       ///< get next event
        }
        printf(">> vm[%d] started on thread[%d], vm.state=%d\n", vm->_id, pid, vm->state);
        vm->_rs.push(DU0);         /// exit token
        while (vm->state==HOLD) nest(*vm);
        printf(">> vm[%d] on thread[%d] done, vm.state=%d\n", vm->_id, pid, vm->state);
    }
}

void t_pool_init() {
    const int NT = _nthread = thread::hardware_concurrency();
    
    _pool = new thread[NT];
    _que  = new VM*[NT*2];
    
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
    for (int i = 0; i < NT; i++) {
        _pool[i] = thread(_event_loop, i);   /// * closure
        CPU_SET(i, &set);
        int rc = pthread_setaffinity_np(     /// * set core affinity
            _pool[i].native_handle(),
            sizeof(cpu_set_t), &set
        );
        if (rc !=0) {
            printf("thread[%d] failed to set affinity: %d\n", i, rc);
        }
    }
    printf("thread pool[%d] initialized\n", NT);
}

void t_pool_stop() {
    {
        lock_guard<mutex> lck(_mtx);
        _done = true;
    }
    _cv_mtx.notify_all();

    printf("joining thread...");
    for (int i = 0; i < _nthread; i++) {
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
///> Messaging control
///
void _ss_dup(VM &vm1, VM &vm0, int n) {
    for (int i = n; i > 1; --i) {
        vm1._ss.push(vm0._ss[-i]);
    }
    vm1._tos = vm0._ss[-1];
    vm0._tos = vm0._ss[-(n+1)];
    vm0._ss.idx -= n;
}
void task_send(VM &vm0, int id) {    ///< ( v1 v2 .. vn n -- )
    VM &vm1 = vm_get(id);
    {
        unique_lock<mutex> lck(_mtx);
        _cv_msg.wait(lck,            ///< release lock and wait
                     [&vm0]{ return vm0.state!=HOLD || _done; });
        vm0.state = HOLD;
        IU n = UINT(vm0._tos);       ///< number of elements
        _ss_dup(vm1, vm0, n);        ///< message passing
        vm0.state = NEST;
    }
    _cv_msg.notify_one();
}

void task_recv(VM &vm0, int id) {
    VM &vm1 = vm_get(id);
    {
        unique_lock<mutex> lck(_mtx);
        _cv_msg.wait(lck,            ///< release lock and wait
                     [&vm1]{ return vm1.state==STOP || _done; });
        IU n = vm1._ss.idx;
        _ss_dup(vm0, vm1, n);
    }
    _cv_msg.notify_one();
}
///
///> IO control
///
void task_wait() {
    while (!_io) delay(1);
    --_io;
}

void task_signal() {
    _io++;
}
#endif // DO_MULTITASK
