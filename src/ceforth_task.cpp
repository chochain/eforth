///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"

extern FV<Code*> dict;             ///< Forth dictionary

#if !DO_MULTITASK
VM _vm0;                           ///< singleton, no VM pooling

VM& vm_get(int id) { return _vm0; }/// * return the singleton
void uvar_init() {
    dict[0]->append(new Var(10));  /// * borrow dict[0]->pf[0]->q[vm.id] for VM's user area

    _vm0.id    = 0;                           /// * VM id
    _vm0.base  = (U8*)&dict[0]->pf[0]->q[0];  /// * set base pointer
    *_vm0.base = 10;
    _vm0.state = HOLD;
}

#else // DO_MULTITASK
VM _vm[E4_VM_POOL_SZ];             ///< VMs for multitasks
///
///> VM pool
///
VM& vm_get(int id) {
    return _vm[(id >= 0 && id < E4_VM_POOL_SZ) ? id : 0];
}
///
///> VM messaging and IO control variables
///
int      VM::NCORE   = 1;          ///< default to 1, updated in init
bool     VM::io_busy = false;
MUTEX    VM::tsk     = PTHREAD_MUTEX_INITIALIZER;
MUTEX    VM::io      = PTHREAD_MUTEX_INITIALIZER;
COND_VAR VM::cv_tsk  = PTHREAD_COND_INITIALIZER;
COND_VAR VM::cv_io   = PTHREAD_COND_INITIALIZER;

///============================================================
///
///> Thread pool
///
/// Note: Thread pool is universal and singleton,
///       so we keep them in C. Hopefully can be reused later
#include <queue>
THREAD     _pool[E4_VM_POOL_SZ];                  ///< thread pool
queue<VM*> _que;                                  ///< event queue
MUTEX      _mtx     = PTHREAD_MUTEX_INITIALIZER;  ///< mutex for queue access
COND_VAR   _cv_mtx  = PTHREAD_COND_INITIALIZER;   ///< for pool exit
bool       _done    = 0;                          ///< thread pool exit flag

void *_event_loop(void *arg) {
    int rank = *(int*)arg;                        ///< dup argument
    VM *vm   = NULL;
    while (true) {
        LOCK(&_mtx);                              ///< lock queue
        {
            while (!_done && _que.size()==0) {    /// * condition wait
                WAIT_FOR(&_cv_mtx, &_mtx);
            }
            if (!_done) {                         ///< lock reaccquired
                vm = _que.front();
                _que.pop();
            }
        }
        NOTIFY(&_cv_mtx);                         /// * notify one
        UNLOCK(&_mtx);

        if (_done) return NULL;

        VM_LOG(vm, ">> started on T%d", rank);
        dict[vm->wp]->nest(*vm);
        VM_LOG(vm, ">> finished on T%d", rank);

        vm->stop();                               /// * release any lock
    }
}

void t_pool_init() {
    VM::NCORE = sysconf(_SC_NPROCESSORS_ONLN);    ///< number of cores
    
    /// setup thread pool and CPU affinity
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {     ///< loop thru ranks
        pthread_create(&_pool[i], NULL, _event_loop, (void*)&_vm[i].id);
        
#if !(defined(__CYGWIN__) || (ARDUINO || ESP32 || __APPLE__) || DO_WASM)
        cpu_set_t set;
        CPU_ZERO(&set);                           /// * clear affinity
        CPU_SET(i % VM::NCORE, &set);             /// * set CPU affinity
        int rc = pthread_setaffinity_np(          /// * set core affinity
            _pool[i], sizeof(cpu_set_t), &set
        );
        if (rc !=0) {
            printf("thread[%d] failed to set affinity: %d\n", i, rc);
        }
#endif // defined(__CYGWIN__) || (ARDUINO || ESP32 || __APPLE__) || DO_WASM
    }
    printf("CPU cores=%d, thread pool[%d] initialized\n", VM::NCORE, E4_VM_POOL_SZ);
}

void t_pool_stop() {
    LOCK(&_mtx);
    {
        _done = true;                          /// * stop event queue
    }
    NOTIFY(&_cv_mtx);
    UNLOCK(&_mtx);

    printf("joining thread...");
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        pthread_join(_pool[i], NULL);
        printf("%d ", i);
    }
    printf(" done!\n");
}
///
///> setup user area (base pointer)
///
void uvar_init() {
    dict[0]->append(new Var(10));  /// * borrow dict[0]->pf[0]->q[vm.id] for VM's user area

    FV<DU> &q = dict[0]->pf[0]->q;
    q.reserve(E4_VM_POOL_SZ);
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        if (i > 0) q.push(10);                    /// * allocate next base storage
        _vm[i].base = (U8*)&q[i];                 /// * set base pointer
        _vm[i].id   = i;                          /// * VM id
        _vm[i].reset(0, STOP);
    }
    _vm[0].state = HOLD;
}

int task_create(IU w) {
    int i = E4_VM_POOL_SZ - 1;
    
    LOCK(&VM::tsk);
    {
        while (i > 0 && _vm[i].state != STOP) --i;
        if (i > 0) {
            _vm[i].reset(w, HOLD);               /// ready to run
        }
    }
    NOTIFY(&VM::cv_tsk);
    UNLOCK(&VM::tsk);
    
    return i;
}

void task_start(int tid) {
    if (tid == 0) {
        printf("main task (tid=0) already running.\n");
        return;
    }
    VM &vm = vm_get(tid);
    LOCK(&_mtx);
    {
        _que.push(&vm);                          /// create event
    }
    NOTIFY(&_cv_mtx);
    UNLOCK(&_mtx);
}
///==================================================================
///
///> VM methods
///
void VM::join(int tid) {
    VM &vm = vm_get(tid);
    VM_LOG(this, ">> joining VM%d", vm.id);
    
    LOCK(&tsk);
    {
        while (vm.state != STOP) {
            pthread_cond_wait(&cv_tsk, &tsk);
        }
    }
    NOTIFY(&cv_tsk);
    UNLOCK(&tsk);
    
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
    src.ss.erase(src.ss.end() - n); /// * pop src by n items
}
void VM::reset(IU w, vm_state st) {
    rs.clear();
    ss.clear();
    tos        = -DU1;
    wp         = w;                 /// * task word
    *base      = 10;                /// * default decimal
    state      = st;
    compile    = false;
}
void VM::stop() {
    LOCK(&tsk);                     /// * lock tasker
    {
        state = STOP;
    }
    NOTIFY(&cv_tsk);                /// * release join lock if any
    UNLOCK(&tsk);
}
///
///> send to destination VM's stack (blocking)
///
void VM::send(int tid, int n) {      ///< ( v1 v2 .. vn -- )
    VM& vm = vm_get(tid);            ///< destination VM

    LOCK(&tsk);                      /// * lock tasker
    {
        while (!_done && vm.state!=HOLD) {
            WAIT_FOR(&cv_tsk, &tsk);
        }
        VM_LOG(&vm, " >> sending %d items to VM%d.%d", n, tid, vm.state);
        _ss_dup(vm, *this, n);       /// * pass n variables as a queue
        vm.state = NEST;             /// * unblock target task
    }
    NOTIFY(&cv_tsk);
    UNLOCK(&tsk);
}
///
///> receive from source VM's stack (blocking)
///
void VM::recv() {                    ///< ( -- v1 v2 .. vn )
    vm_state st = state;             ///< keep current VM state
    
    LOCK(&tsk);                      /// * lock tasker
    {
        state = HOLD;
    }
    NOTIFY(&cv_tsk);
    UNLOCK(&tsk);

    VM_LOG(this, " >> waiting");

    LOCK(&tsk);                      ///< lock tasker
    {
        while (!_done && state==HOLD) {  /// * block til msg arrival
            WAIT_FOR(&cv_tsk, &tsk);
        }
        VM_LOG(this, " >> received => state=%d", st);
        state = st;                  /// * restore VM state
    }
    NOTIFY(&cv_tsk);
    UNLOCK(&tsk);
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

    LOCK(&tsk);                      /// * lock tasker
    {
        while (!_done && vm.state != STOP) {
            WAIT_FOR(&cv_tsk, &tsk);
        }
        if (!_done) {
            _ss_dup(*this, vm, n);   /// * retrieve from completed task
            printf(">> pulled %d items from VM%d.%d\n", n, vm.id, vm.state);
        }
    }
    NOTIFY(&cv_tsk);
    UNLOCK(&tsk);
}
///
///> IO control (can use atomic _io after C++20)
///
/// Note: after C++20, _io can be atomic.wait
void VM::io_lock() {
    LOCK(&io);
    {
        while (io_busy) {
            WAIT_FOR(&cv_io, &io);
        }
        io_busy = true;            /// * lock
    }
    NOTIFY(&cv_io);
    UNLOCK(&io);
}

void VM::io_unlock() {
    LOCK(&io);
    {
        io_busy = false;           /// * unlock
    }
    NOTIFY(&cv_io);
    UNLOCK(&io);
}
#endif // DO_MULTITASK
