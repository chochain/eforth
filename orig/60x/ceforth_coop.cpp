///
/// @file ceforth_coop.cpp
/// @brief eForth - cooperative multitasking: PAUSE + TASK + START
///
#include "ceforth.h"

#if DO_MULTITASK && !USE_PTHREAD
extern List<VM, E4_VM_POOL_SZ> _vm;           ///< VM pool defined in ceforth_task.cpp
extern IU gStop[];                            ///< defined in ceforth.cpp

VM& vm_get(int id) {
    return _vm[(id >= 0 && id < E4_VM_POOL_SZ) ? id : 0];
}

void t_pool_init() { LOG("[SYSTEM] cooperative tasker, %d slots\n", E4_VM_POOL_SZ); }
void t_pool_stop() {}

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

int task_create(IU pfa) {                     ///< returns tid, or 0 if the pool is full
    int i = E4_VM_POOL_SZ - 1;
    while (i > 0 && _vm[i].state != STOP) --i;
    if (i > 0) _vm[i].reset(pfa, HOLD);       /// * created, not yet running
    else ERR("task slots full\n");

    return i;
}

void task_start(int tid) {
    if (tid > 0 && tid < E4_VM_POOL_SZ && _vm[tid].state == HOLD) {
        _vm[tid].state = NEST;                /// * now in the ring
    }
}

/// One turn of the ring: run each runnable task whose wake time has passed.
/// Returns true if a task ran, or if only sleepers remain (after a 1ms nap),
/// so the drain loop in outer_poll() keeps going until they finish.
static bool one_turn() {
    auto due = [](U32 wake) { return (S32)(millis() - wake) >= 0; };   /// * rollover-safe
    bool ran = false, sleep = false;
    for (int i = 1; i < E4_VM_POOL_SZ; i++) {
        VM &t = _vm[i];
        if (t.state != NEST) continue;
        if (!due(t.wake)) { sleep = true; continue; }
        nest(t);                              /// * returns at its pause/ms, or at doSTOP
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
