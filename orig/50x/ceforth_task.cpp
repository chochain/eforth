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
    pmem.idx = DALIGN(pmem.idx);   /// DU aligned
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
int      VM::NCORE   = thread::hardware_concurrency();  ///< number of cores
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
MUTEX    _mtx    = PTHREAD_MUTEX_INITIALIZER;     ///< mutex for queue access
COND_VAR _cv_mtx = PTHREAD_COND_INITIALIZER;      ///< for pool exit

bool               _done    = 0;   ///< thread pool exit flag
List<THREAD, 0>    _pool;          ///< thread pool
List<VM*,    0>    _que;           ///< event queue

void *_event_loop(void *arg) {
    int rank = *(int*)arg;                        ///< dup argument
    VM *vm   = NULL;
    while (true) {
        pthread_mutex_lock(&_mtx);                ///< lock queue
        {
            while (!_done && _que.idx==0) {       /// * condition wait
                pthread_cond_wait(&_cv_mtx, &_mtx);
            }
            if (!_done) vm = _que.pop();          ///< lock reaccquired
        }
        pthread_cond_signal(&_cv_mtx);            /// * notify one
        pthread_mutex_unlock(&_mtx);

        if (_done) return NULL;

        VM_LOG(vm, ">> started on T%d", rank);
        vm->rs.push(DU0);                         /// exit token
        nest(*vm);
        VM_LOG(vm, ">> finished on T%d", rank);

        vm->stop();                               /// * release any lock
    }
}

void t_pool_init() {
    _pool = new THREAD[E4_VM_POOL_SZ];            ///< a thread each core
    _que  = new VM*[E4_VM_POOL_SZ];               ///< queue with spare 

    if (!_pool.v || !_que.v) {
        printf("thread_pool_init allocation failed\n");
        exit(-1);
    }
    /// setup thread pool and CPU affinity
#ifdef __CYGWIN__
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {     ///< loop thru ranks
        pthread_create(&_pool[i], NULL, _event_loop, (void*)&i);
    }
#else // !__CYGWIN__
    cpu_set_t set;
    CPU_ZERO(&set);                               /// * clear affinity
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {     ///< loop thru ranks
        pthread_create(&_pool[i], NULL, _event_loop, (void*)&i);
/*
        CPU_SET(i % VM::NCORE, &set);             /// * CPU affinity
        int rc = pthread_setaffinity_np(          /// * set core affinity
            _pool[i].native_handle(),
            sizeof(cpu_set_t), &set
        );
        if (rc !=0) {
            printf("thread[%d] failed to set affinity: %d\n", i, rc);
        }
*/
    }
#endif // __CYGWIN__
    printf("thread pool[%d] initialized\n", E4_VM_POOL_SZ);
}

void t_pool_stop() {
    pthread_mutex_lock(&_mtx);
    {
        _done = true;                          /// * stop event queue
    }
    pthread_cond_signal(&_cv_mtx);
    pthread_mutex_unlock(&_mtx);

    printf("joining thread...");
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        pthread_join(_pool[i], NULL);
        printf("%d ", i);
    }
    _pool.clear();

    printf(" done!\n");
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

    pthread_mutex_lock(&VM::tsk);
    {
        while (i > 0 && _vm[i].state != STOP) --i;
        if (i > 0) {
            _vm[i].reset(pfa, HOLD);              /// ready to run
        }
    }
    pthread_cond_signal(&VM::cv_tsk);
    pthread_mutex_unlock(&VM::tsk);

    return i;
}

void task_start(int tid) {
    if (tid == 0) {
        printf("main task (tid=0) already running.\n");
        return;
    }
    VM &vm = vm_get(tid);
    pthread_mutex_lock(&_mtx);
    {
        _que.push(&vm);                          /// create event
    }
    pthread_cond_signal(&_cv_mtx);
    pthread_mutex_unlock(&_mtx);
}
///==================================================================
///
///> VM methods
///
void VM::join(int tid) {
    VM &vm = vm_get(tid);
    VM_LOG(this, ">> joining VM%d", vm.id);

    pthread_mutex_lock(&tsk);
    {
        while (vm.state != STOP) {
            pthread_cond_wait(&cv_tsk, &tsk);
        }
    }
    pthread_cond_signal(&cv_tsk);
    pthread_mutex_unlock(&tsk);

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
    pthread_mutex_lock(&tsk);       /// * lock tasker
    {
        state = STOP;
    }
    pthread_cond_signal(&cv_tsk);   /// * release join lock if any
    pthread_mutex_unlock(&tsk);
}
///
///> send to destination VM's stack (blocking)
///
void VM::send(int tid, int n) {      ///< ( v1 v2 .. vn -- )
    VM& vm = vm_get(tid);            ///< destination VM

    pthread_mutex_lock(&tsk);        /// * lock tasker
    {
        while (!_done && vm.state!=HOLD) {
            pthread_cond_wait(&cv_tsk, &tsk);
        }
        VM_LOG(&vm, " >> sending %d items to VM%d.%d", n, tid, vm.state);
        _ss_dup(vm, *this, n);       /// * pass n variables as a queue
        vm.state = NEST;             /// * unblock target task
    }
    pthread_cond_signal(&cv_tsk);
    pthread_mutex_unlock(&tsk);
}
///
///> receive from source VM's stack (blocking)
///
void VM::recv() {                    ///< ( -- v1 v2 .. vn )
    vm_state st = state;             ///< keep current VM state

    pthread_mutex_lock(&tsk);        /// * lock tasker
    {
        state = HOLD;
    }
    pthread_cond_signal(&cv_tsk);
    pthread_mutex_unlock(&tsk);

    VM_LOG(this, " >> waiting");

    pthread_mutex_lock(&tsk);        ///< lock tasker
    {
        while (!_done && state==HOLD) {  /// * block til msg arrival
            pthread_cond_wait(&cv_tsk, &tsk);
        }
        VM_LOG(this, " >> received => state=%d", st);
        state = st;                  /// * restore VM state
    }
    pthread_cond_signal(&cv_tsk);
    pthread_mutex_unlock(&tsk);
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

    pthread_mutex_lock(&tsk);        /// * lock tasker
    {
        while (!_done && vm.state != STOP) {
            pthread_cond_wait(&cv_tsk, &tsk);
        }
        if (!_done) {
            _ss_dup(*this, vm, n);   /// * retrieve from completed task
            printf(">> pulled %d items from VM%d.%d\n", n, vm.id, vm.state);
        }
    }
    pthread_cond_signal(&cv_tsk);
    pthread_mutex_unlock(&tsk);
}
///
///> IO control (can use atomic _io after C++20)
///
/// Note: after C++20, _io can be atomic.wait
void VM::io_lock() {
    pthread_mutex_lock(&io);
    {
        while (io_busy) {
            pthread_cond_wait(&cv_io, &io);
        }
        io_busy = true;            /// * lock
    }
    pthread_cond_signal(&cv_io);
    pthread_mutex_unlock(&io);
}

void VM::io_unlock() {
    pthread_mutex_lock(&io);
    {
        io_busy = false;           /// * unlock
    }
    pthread_cond_signal(&cv_io);
    pthread_mutex_unlock(&io);
}
#endif // DO_MULTITASK
