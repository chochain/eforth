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
List<thread, 0>    _pool;          ///< thread pool
List<VM*,    0>    _que;           ///< event queue
bool               _done = false;
mutex              _mtx;
condition_variable _cv;

void _event_loop(int id) {
    VM *vm;
    while (true) {
        {
            unique_lock<mutex> lck(_mtx);
            _cv.wait(lck,          ///< release lock and wait
                     []{ return _que.idx > 0 || _done; });
            if (_done) return;     ///< lock reaccquired
            vm = _que.pop();       ///< get next event
        }
        printf(">> vm[%d] started, vm.state=%d\n", id, vm->state);
        vm->_rs.push(DU0);         /// exit token
        while (vm->state==HOLD) nest(*vm);
        printf(">> vm[%d] done state=%d\n", id, vm->state);
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
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        _vm[i].base = &pmem[pmem.idx];       /// * HERE
        add_du(10);                          /// * default base=10
    }
    for (int i = 0; i < NT; i++) {
        _pool[i] = thread(_event_loop, i);
    }
    printf("thread pool[%d] initialized\n", NT);
}

void t_pool_stop() {
    {
        lock_guard<mutex> lck(_mtx);
        _done = true;
    }
    _cv.notify_all();

    printf("joining thread...");
    for (int i = 0; i < _nthread; i++) {
        _pool[i].join();
        printf("%d ", i);
    }
    _pool.clear();
    
    printf(" done!\n");
}

int task_create(IU pfa) {
    int i = 1;
    while (i < E4_VM_POOL_SZ && _vm[i].state != STOP) i++;
    if (i >= E4_VM_POOL_SZ) return 0;
    
    _vm[i].reset(pfa, HOLD);

    return i;
}

void task_start(int id) {
    VM &vm = vm_get(id);
    {
        lock_guard<mutex> lck(_mtx);
        _que.push(&vm);
    }
    _cv.notify_one();
}
///
///> IO control
///
atomic<int> _io(1);

void task_wait() {
    while (!_io) delay(1);
    --_io;
}

void task_signal() {
    _io++;
}
#endif // DO_MULTITASK
