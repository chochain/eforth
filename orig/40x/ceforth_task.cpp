///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"
///
///> VM pool
///
List<VM, E4_VM_POOL_SZ> _vm;       ///< VM pool
    
VM& vm_get(int id) {
    return (id >= 0 && id < E4_VM_POOL_SZ) ? _vm[id] : _vm[0];
}

#if DO_MULTITASK
///
///> Thread pool
///
#include <atomic>
#include <mutex>
#include <condition_variable>

extern void nest(VM &vm);

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
            _cv.wait(lck,
                     []{ return _que.idx > 0 || _done; });
            if (_done) return;
        
            vm = _que.pop();       ///< get next event
        }
        IU pfa = vm->_ip;
    
        printf(">> vm[%d] started, vm.state=%d\n", id, vm->state);
        vm->_rs.push(DU0);         /// exit token
        while (vm->state==HOLD) nest(*vm);
        printf(">> vm[%d] done state=%d\n", id, vm->state);
        
        vm->reset(pfa);            /// keep w for restart
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
    
    VM &vm1 = _vm[i];
    vm1.state = HOLD;              /// STOP=>HOLD, fake resume
    vm1._ip   = pfa;               /// at given pfa (of colon word)

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
