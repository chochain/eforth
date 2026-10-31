///
/// @file ceforth_coop.cpp
/// @brief eForth - cooperative multitasking: PAUSE + TASK + START
///
#include "ceforth.h"

#if DO_MULTITASK && !USE_PTHREAD

List<VM, E4_VM_POOL_SZ> _vm;                  ///< VM pool; slot 0 is the shell

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
    return i;
}

void task_start(int tid) {
    if (tid > 0 && tid < E4_VM_POOL_SZ && _vm[tid].state == HOLD) {
        _vm[tid].state = NEST;                /// * now in the ring
    }
}

/// One turn of the ring: each runnable task runs until its next PAUSE or its end.
/// Returns true if any task ran.
bool task_pause() {
    bool ran = false;
    for (int i = 1; i < E4_VM_POOL_SZ; i++) {
        VM &t = _vm[i];
        if (t.state != NEST) continue;
        nest(t);                              /// * returns at its PAUSE, or at doSTOP
        ran = true;
    }
    return ran;
}

#endif // DO_MULTITASK && !USE_PTHREAD
