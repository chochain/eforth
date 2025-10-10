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
    Var *v = new Var(10);
    dict[BASE_NODE]->append(v);    /// * borrow dict[0]->pf[0]->q[vm.id] for VM's user area

    _vm0.id    = 0;                /// * VM id
    _vm0.state = HOLD;             /// * VM ready to run
    _vm0.base  = (U8*)&v->q[0];    /// * set base pointer
    *_vm0.base = 10;
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
MUTEX    VM::io;
MUTEX    VM::tsk;
COND_VAR VM::cv_io;
COND_VAR VM::cv_tsk;

///============================================================
///
///> Thread pool
///
/// Note: Thread pool is universal and singleton,
///       so we keep them in C. Hopefully can be reused later
#include <queue>
vector<THREAD> _pool;                             ///< thread pool
queue<VM*>     _que;                              ///< event queue, thread-safe?
MUTEX          _evt;                              ///< mutex for queue access
COND_VAR       _cv_evt;                           ///< for pool exit
bool           _quit = 0;                         ///< thread pool exit flag

void _event_loop(int rank) {
    VM *vm = NULL;
    while (true) {
        {
            XLOCK(_evt);                          ///< lock queue
            WAIT(_cv_evt, []{ return !_que.empty() || _quit; });
            
            if (!_que.empty()) {                  ///< lock reaccquired
                vm = _que.front();
                _que.pop();
            }
            else if (_quit) break;                /// * bail
            
            NOTIFY(_cv_evt);                      /// * notify one
        }
        VM_LOG(vm, ">> started on T%d", rank);
        dict[vm->wp]->nest(*vm);
        VM_LOG(vm, ">> finished on T%d", rank);

        vm->stop();                               /// * release any lock
    }
}

void t_pool_init() {
    VM::NCORE = thread::hardware_concurrency();   ///< number of cores
    
    /// setup thread pool and CPU affinity
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {     ///< loop thru ranks
        _pool.emplace_back(_event_loop, (int)_vm[i].id);

#if __has_include(<sched.h>) && !defined(__CYGWIN__) && !defined(__ANDROID__)
        pthread_t t   = _pool.back().native_handle();
        cpu_set_t set;
        CPU_ZERO(&set);                           /// * clear affinity
        CPU_SET(i % VM::NCORE, &set);             /// * set CPU affinity
        int rc = pthread_setaffinity_np(          /// * set core affinity
            t, sizeof(cpu_set_t), &set
        );
        if (rc != 0) {
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
    int i = (int)_pool.size();
    for (auto &t : _pool) {
        printf("%d ", --i);
        t.join();
    }
    _pool.clear();
    
    printf("done!\n");
}
///
///> setup/teardown user area (base pointer)
///
void uvar_init() {
    Var *v = new Var(10);
    dict[BASE_NODE]->append(v);  /// * borrow dict[0]->pf[0]->q[vm.id] for VM's user area

    v->q.reserve(E4_VM_POOL_SZ);
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        if (i > 0) v->q.push(10);                 /// * allocate next base storage
        _vm[i].base = (U8*)&v->q[i];              /// * set base pointer
        _vm[i].id   = i;                          /// * VM id
        _vm[i].reset(0, STOP);
    }
    _vm[0].state = HOLD;
}

int task_create(IU w) {
    int i = E4_VM_POOL_SZ - 1;

    GUARD(VM::tsk);
    
    while (i > 0 && _vm[i].state != STOP) --i;
    if (i > 0) {
        _vm[i].reset(w, HOLD);                    /// ready to run
    }
    NOTIFY(VM::cv_tsk);
    
    return i;
}

void task_start(int tid) {
    if (tid == 0) {
        printf("main task (tid=0) already running.\n");
        return;
    }
    VM &vm = vm_get(tid);                        /// fetch VM[id]
    
    GUARD(_evt);
    _que.push(&vm);                              /// create event
    NOTIFY(_cv_evt);
}
///==================================================================
///
///> VM methods
///
void VM::set_state(vm_state st) {
    GUARD(tsk);
    state = st;
    NOTIFY(cv_tsk);
}
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
    dst.ss.push(dst.tos);                         /// * push dest TOS
    dst.tos = src.tos;                            /// * set dest TOS
    src.tos = src.ss[-n];                         /// * set src TOS
    for (int i = n - 1; i > 0; --i) {
        dst.ss.push(src.ss[-i]);                  /// * passing stack elements
    }
    src.ss.erase(src.ss.end() - n);               /// * pop src by n items
}
void VM::reset(IU w, vm_state st) {
    rs.clear();
    ss.clear();
    tos        = -DU1;
    wp         = w;                               /// * task word
    *base      = 10;                              /// * default decimal
    state      = st;
    compile    = false;
}
void VM::stop() { set_state(STOP); }              /// * and release lock
///
///> send to destination VM's stack (blocking)
///
void VM::send(int tid, int n) {                   ///< ( v1 v2 .. vn -- )
    VM& vm = vm_get(tid);                         ///< destination VM
    
    XLOCK(tsk);
    WAIT(cv_tsk, [vm]{ return vm.state==HOLD || _quit; });

    if (_quit) return;                            /// * nothing to do, bail

    VM_LOG(&vm, ">> sending %d items to VM%d.%d", n, tid, vm.state);
    _ss_dup(vm, *this, n);                        /// * pass n params as a msg queue
    vm.state = NEST;                              /// * unblock target task

    NOTIFY(cv_tsk);
}
///
///> receive from source VM's stack (blocking)
///
void VM::recv() {                                 ///< ( -- v1 v2 .. vn )
    vm_state st = state;                          ///< keep current VM state
    {
        GUARD(tsk);                               /// * lock tasker
        state = HOLD;                             /// * pending state for message
        NOTIFY(cv_tsk);
    }
    VM_LOG(this, ">> waiting");
    {
        XLOCK(tsk);
        WAIT(cv_tsk, [this]{ return state!=HOLD || _quit; }); /// * block until msg arrive
        state = st;                                /// * restore VM state
    
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
    VM& vm = vm_get(tid);                         ///< source VM
    
    XLOCK(tsk);
    WAIT(cv_tsk, [vm]{ return vm.state==STOP || _quit; });
    
    if (!_quit) {
        _ss_dup(*this, vm, n);                    /// * retrieve from completed task
        printf(">> pulled %d items from VM%d.%d\n", n, vm.id, vm.state);
    }
    
    NOTIFY(cv_tsk);
}
///
///> IO control (can use atomic _io after C++20)
///
/// Note: after C++20, _io can be atomic.wait
void VM::io_lock() {
    XLOCK(io);                                    ///< wait for IO
    WAIT(cv_io, []{ return !io_busy; });
    
    io_busy = true;                               /// * lock
    
    NOTIFY(cv_io);
}

void VM::io_unlock() {
    GUARD(io);
    io_busy = false;                              /// * unlock
    NOTIFY(cv_io);
}
#endif // DO_MULTITASK
