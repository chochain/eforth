///
/// @file ceforth_coop.cpp
/// @brief eForth - cooperative multitasking: PAUSE + TASK + START, web sessions as tasks
///
#include "ceforth.h"

#if DO_MULTITASK && !USE_PTHREAD
enum {                                      ///< forth_run() results
    RUN_WAIT =  2,                          ///< sleeping (ms), call again later
    RUN_DONE =  1,                          ///< line consumed
    RUN_MORE =  0,                          ///< paused, runnable now: call again with line=NULL
    RUN_BUSY = -1,                          ///< engine/compiler taken: call again, SAME arguments
    RUN_GONE = -2                           ///< session closed or aborted: drop it
};

#if E4_FREERTOS
static R_MUTEX _lock = nullptr;
static bool _lock_ready()  { return _lock != nullptr; }
static void _lock_create() { if (!_lock) _lock = xSemaphoreCreateRecursiveMutex(); }
static bool _lock_take(int ms) {
    return xSemaphoreTakeRecursive(
        _lock, ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(ms)) == pdTRUE;
}
static void _lock_give()   { xSemaphoreGiveRecursive(_lock); }

#else // !E4_FREERTOS: x86, ARM Linux, macOS...
static R_MUTEX _lock;                  ///< same semantics as FreeRTOS recursive mutex
static bool _lock_on = false;          ///< armed by vm_pool_init, like _lock != nullptr

static bool _lock_ready()  { return _lock_on; }
static void _lock_create() { _lock_on = true; }
static bool _lock_take(int ms) {
    if (ms < 0)  { _lock.lock(); return true; }
    if (ms == 0) return _lock.try_lock();
    return _lock.try_lock_for(std::chrono::milliseconds(ms));
}
static void _lock_give()   { _lock.unlock(); }

#endif // E4_FREERTOS

extern List<VM, E4_VM_POOL_SZ> _vm;    ///< VM pool defined in ceforth_task.cpp
extern IU gStop[];                     ///< defined in ceforth.cpp

static int g_compiler = -1;            ///< tid with an open ':' definition (-1 = none, 0 = shell)
static int g_cur      = 0;             ///< tid of the slice now running

VM& vm_get(int id) {
    return _vm[(id >= 0 && id < E4_VM_POOL_SZ) ? id : 0];
}
///
///> engine lock: one slice of Forth at a time (dictionary, pmem, tib and obuf are shared)
///
bool forth_lock(int ms) {
    if (!_lock_ready()) return true;   /// * before vm_pool_init: single threaded
    return _lock_take(ms);
}
void forth_unlock(bool vm0) {
    g_compiler = vm0 ? 0 : -1;         /// * shell owns the compiler while ':' is open
    if (_lock_ready()) _lock_give();
}

uint32_t forth_cur_tid()        { return (uint32_t)g_cur; }
uint32_t forth_cur_sid()        { return _vm[g_cur].sid; }
int      forth_fb_add()         { return ++_vm[g_cur].fb; }
int      forth_fb(uint32_t tid) { return vm_get(tid).fb; }

///====================================================================
///
///> Web session = task. One slice of a session's input line.
///
///  The session VM is a shell (like VM 0) that owns a private input line.
///  A slice runs until the line is consumed, or the VM pauses/sleeps, then
///  parks its input cursor and returns, so other sessions get the engine.
///
///  @return RUN_DONE  line consumed (stack printed)
///          RUN_MORE  paused, runnable now          => call again with line=NULL
///          RUN_WAIT  sleeping (ms), not due yet    => call again later
///          RUN_BUSY  engine/compiler taken          => call again, SAME arguments
///          RUN_GONE  session closed or aborted      => drop it
///
static inline bool due(const VM &v) {
    return !v.wake || (S32)(millis() - v.wake) >= 0;
}

int forth_run(
    uint32_t tid, uint32_t sid,
    const char *line, void (*hook)(int, const char*)) {
    if (tid <= 0)       return RUN_GONE;
    if (!forth_lock(0)) return RUN_BUSY;                    /// * never block a dispatcher worker
    VM &vm = vm_get(tid);
    if (!vm.ses || vm.sid != sid) {                         /// * stale msg: closed, or slot reused
        forth_unlock(); return RUN_GONE;
    }
    if (g_compiler >= 0 && g_compiler != (int)tid) {        /// * someone else has an open ':' definition
        forth_unlock(); return RUN_BUSY;
    }
    
    fout_setup(hook);
    if (line) {
        free(vm.tbuf);
        vm.tbuf  = strdup(line);                            /// * msg buffers die; keep our own copy
        vm.tib   = vm.tbuf;
        vm.fb    = 0;
        vm.state = QUERY;
    }
    g_cur = tid;
    io_attach(vm);

    int  rc = RUN_DONE;
    char idiom[E4_IBUF_SZ];
    for (;;) {
        if (vm.wake) {
            if (!due(vm)) { rc = RUN_WAIT; break; }         /// * still sleeping
            vm.wake = 0;
        }
        if (vm.state == NEST) {
            nest(vm);                                       /// * resume the word that paused
        }
        else {
            if (!fetch(idiom, E4_IBUF_SZ)) break;           /// * line consumed
            forth_core(vm, idiom);
        }
        if (vm.state == NEST || vm.wake) { rc = RUN_MORE; break; }
    }
    if (rc == RUN_DONE && !vm.compile) { ss_dump(vm); pstr("ok\n"); }
    
    g_compiler = vm.compile ? tid : -1;
    io_detach(vm);                                          /// * park tib, flush output

    if (rc == RUN_DONE) vm.tib = nullptr;
    g_cur = 0;

    forth_unlock();
    return rc;
}

void vm_pool_init() {
    _lock_create();
    LOG("[SYSTEM] cooperative tasker, %d slots\n", E4_VM_POOL_SZ);
}
void vm_pool_stop() {}

void uvar_init() {
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        U8 *b = &pmem[pmem.idx++];
        *b = 10;                              /// * default radix
        _vm[i].id    = i;
        _vm[i].base  = (IU)(b - MEM0);
        _vm[i].state = STOP;                  /// * free slot
    }
    _vm[0].state = QUERY;                     /// * the shell
    pmem.idx = ALIGN4(pmem.idx);              /// * was DALIGN, a no-op
}
///
///> slot allocation: lock-free, the httpd thread, dispatcher workers and Forth words all call in
///
static int vm_slot_get(vm_state to) {        ///< returns slot (1..N-1) or 0 if full
    for (int i = E4_VM_POOL_SZ - 1; i > 0; i--) {
        int expect = STOP;
        if (__atomic_compare_exchange_n(
            (int*)&_vm[i].state, &expect, (int)to,
            false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return i;
    }
    return 0;
}

int task_create(IU pfa) {                     ///< returns tid, or 0 if the pool is full
    int i = vm_slot_get(HOLD);                /// * created, not yet running
    if (i) _vm[i].reset(pfa, HOLD);
    else   ERR("task slots full\n");
    return i;
}

void task_start(int tid) {
    if (tid > 0 && tid < E4_VM_POOL_SZ &&
        _vm[tid].state == HOLD && !_vm[tid].ses) {
        _vm[tid].state = NEST;                /// * now in the ring
    }
}
///
///> web session = task
///  A session slot is a shell VM (state QUERY) that is never in the ring: it is only
///  ever driven by forth_run(), one slice per ForthActor message.
///
int session_open(uint32_t sid) {              ///< returns tid, or 0 if the pool is full
    int i = vm_slot_get(HOLD);
    if (!i) return 0;

    VM &v = _vm[i];
    v.ss.idx  = v.rs.idx = 0;
    v.ip      = 0;
    v.state   = QUERY;                        /// * owned, idle
    v.compile = false;
    v.wake    = 0;
    v.sid     = sid;
    v.fb      = 0;
    v.ses     = true;
    v.tib     = nullptr;
    v.tbuf    = nullptr;
    v.key     = 0;
    v.busy    = false;
    v.last    = millis();
    
    *(MEM0 + v.base) = 10;                    /// * default radix
    return i;
}

bool session_close(uint32_t tid) {
    if (tid <= 0 || tid >= E4_VM_POOL_SZ) return false;
    
    VM &v = _vm[tid];
    if (!v.ses) return false;
    
    if (!forth_lock(250)) {                   /// * a slice of this VM may be running
        LOG("[SYSTEM] session_close(%d) timed out\n", tid);
        return false;
    }
    if (g_compiler==(int)tid) g_compiler = -1;/// * release an abandoned ':' definition
    free(v.tbuf);
    
    v.ss.idx  = v.rs.idx = 0;
    v.state   = STOP;                         /// * last: slot is free again
    v.compile = false;
    v.wake    = 0;
    v.sid     = 0;                            /// * stale RESUME msgs now fail the sid check
    v.ses     = false;
    v.tib     = nullptr;
    v.tbuf    = nullptr;
    v.key     = 0;
    v.busy    = false;
    
    forth_unlock();
    
    return true;
}

static int session_lru() {                    ///< oldest idle persistent session, or 0
    int best = 0;
    for (int i = 1; i < E4_VM_POOL_SZ; i++) {
        VM &v = _vm[i];
        if (!v.ses || v.busy) continue;
        if (!best || (S32)(_vm[best].last - v.last) > 0) best = i;
    }
    return best;
}

///> one VM per browser tab: find it (stack intact) or create it; rebind to this request's sid
int session_attach(uint32_t key, uint32_t sid) {
    int t = 0;
    for (int i = 1; i < E4_VM_POOL_SZ; i++) {
        if (_vm[i].ses && _vm[i].key == key) { t = i; break; }
    }
    if (t) {
        if (_vm[t].busy) return -1;           /// * previous line of this tab still running
    }
    else {
        t = session_open(sid);
        if (!t) {                             /// * pool full: evict least-recently-used idle tab
            int v = session_lru();
            if (v && session_close(v)) t = session_open(sid);
        }
        if (!t) return 0;
        _vm[t].key = key;
    }
    VM &v = _vm[t];
    v.sid  = sid;                             /// * new sid: stale RESUMEs of old requests now fail
    v.fb   = 0;
    v.busy = true;
    return t;
}

///> request finished: free the HTTP side, keep ss (and an open ':' unless aborted)
bool session_release(uint32_t tid, bool abort) {
    if (tid <= 0 || tid >= E4_VM_POOL_SZ) return false;
    VM &v = _vm[tid];
    if (!v.ses) return false;

    v.sid  = 0;                               /// * first: in-flight slices drop themselves
    v.busy = false;
    v.last = millis();
    if (!forth_lock(250)) {
        LOG("[SYSTEM] session_release(%d) timed out\n", (int)tid);
        return false;
    }
    free(v.tbuf);  v.tbuf = nullptr;
    v.tib = nullptr;
    v.wake = 0;
    if (abort) {                              /// * interrupted mid-word: clean control state only
        v.rs.idx  = 0;
        v.ip      = 0;
        v.state   = QUERY;
        v.compile = false;
        if (g_compiler == (int)tid) g_compiler = -1;
    }
    forth_unlock();
    return true;
}

/// One turn of the ring: run each runnable task whose wake time has passed.
/// Returns true if a task ran, or if only sleepers remain (after a 1ms nap),
/// so the drain loop in outer_poll() keeps going until they finish.
static bool one_turn() {
    auto due = [](U32 wake) { return (S32)(millis() - wake) >= 0; };   /// * rollover-safe
    bool ran = false, sleep = false;
    for (int i = 1; i < E4_VM_POOL_SZ; i++) {
        VM &t = _vm[i];
        if (t.state != NEST || t.ses) continue;  /// * sessions are driven by forth_run()
        if (!due(t.wake)) { sleep = true; continue; }
        nest(t);                           /// * returns at its pause/ms, or at doSTOP
        ran = true;
    }
    if (!ran && sleep) { delay(1); return true; }
    return ran;
}

/// ms <= 0: one turn of the ring (pause, PAUSE(), handle_idle).
/// ms >  0: shell wait, keep the ring turning for ms milliseconds.
bool task_pause(int ms) {
    if (ms <= 0) return one_turn();

    bool busy = false;
    U32  t0   = millis();
    do {
        if (one_turn()) busy = true;
        else            delay(1);         /// * no tasks at all, don't spin
    } while ((S32)(millis() - t0) < ms);
    return busy;
}
#endif // DO_MULTITASK && !USE_PTHREAD
