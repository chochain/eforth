///
/// @file ceforth_task.cpp
/// @brief eForth - 1970s Chuck Moore Cooperative Multi-tasking Engine for ESP32
///
#include "ceforth.h"

#if DO_MULTITASK
#if !USE_PTHREAD
///
///> VM messaging and IO control variables
///
int  VM::curr_vm_id = 0;
bool VM::io_busy    = false;

List<VM, E4_VM_POOL_SZ> _vm;       ///< Pool of distinct VM contexts

VM& vm_get(int id) {
    return _vm[(id >= 0 && id < E4_VM_POOL_SZ) ? id : 0];
}

// =====================================================================
// 🏢 1970s Cooperative Scheduler Ring Execution
// =====================================================================

void t_pool_init() {
    // Overriding the hardware thread calculation; everything runs cooperatively on 1 core.
    VM::curr_vm_id = 0;           /// * Main task starts as active leader
    LOG("[SYSTEM] Cooperative Tasker initialized across [%d] slots\n", E4_VM_POOL_SZ);
}

void t_pool_stop() {
    // No physical threads to join or cleanup anymore
}

void uvar_init() {
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        U8 *b = &pmem[pmem.idx++];                ///< *base
        *b = 10;                                  /// * default 10
        _vm[i].id   = i;                          /// * VM id
        _vm[i].base = (IU)(b - MEM0);             /// * base idx
        _vm[i].state = STOP;                      /// * Start out dead
    }
    _vm[0].state = QUERY;                         /// * Main loop task is alive
    pmem.idx = DALIGN(pmem.idx);                  /// DU aligned
}

int task_create(IU pfa) {
    // Find an empty task slot available backwards through the pool ring
    int i = E4_VM_POOL_SZ - 1;
    while (i > 0 && _vm[i].state != STOP) --i;
    
    if (i > 0) {
        _vm[i].reset(pfa, HOLD);                  /// Initialized and ready to be started
    }
    return i;
}

void task_start(int tid) {
    if (tid == 0) return;
    if (tid > 0 && tid < E4_VM_POOL_SZ) {
        _vm[tid].state = NEST;                    /// Mark it ready for scheduler traversal passes
    }
}

/// ===================================================================
/// 🔄 The Round-Robin Dispatch Loop (The Core 'PAUSE' Manager)
/// ===================================================================

/// This replaces your old thread-blocking `nest(vm)` architecture. 
/// Calling this shifts focus smoothly to the next task in line.
void pause(void) {
    int tid = VM::curr_vm_id;                    ///< keep current tid
    
    for (int i = 0; i < E4_VM_POOL_SZ; i++) {
        tid = (tid + 1) % E4_VM_POOL_SZ;         /// * round-robin
        if (_vm[tid].state == NEST || _vm[tid].state == QUERY) {
            VM::curr_vm_id = tid;
            return; 
        }
    }
    
    // Fallback: If everything else is sleeping/holding, return back to the main user shell task
    VM::curr_vm_id = 0;
    
    // Crucial ESP32 Guardrail: feed the watchdog when nothing else is demanding cycles
    delay(1); 
}

void VM::stop() {
    state = STOP;
    pause();
}

void VM::join(int tid) {
    VM &target = vm_get(tid);
    // Keep giving away our execution slice until the target finishes completely
    while (target.state != STOP) {
        pause();
    }
}

void VM::send(int tid, int n) {
    VM& dest = vm_get(tid);
    // Yield loops until destination goes to HOLD state (ready to absorb values)
    while (dest.state != HOLD) {
        pause();
    }
    
    ss_dup(dest, *this, n);
    dest.state = NEST;           /// Wake it up
    pause();
}

void VM::recv() {
    state = HOLD;                /// Block ourselves waiting for an influx of variables
    pause();         /// Let other items run until someone rewrites our state back to NEST
}

void VM::bcast(int n) {
    // Intentional stub
}

void VM::pull(int tid, int n) {
    VM& src = vm_get(tid);
    while (src.state != STOP) {
        pause();
    }
    ss_dup(*this, src, n);
}

void VM::io_lock() {
    // Cooperatively wait for the IO lock to become completely vacant
    while (VM::io_busy) {
        pause();
    }
    VM::io_busy = true;
}

void VM::io_unlock() {
    VM::io_busy = false;
    pause();
}

void VM::reset(IU cfa, vm_state st) {
    rs.idx     = ss.idx = 0;
    *ip        = cfa;
    state      = st;
    compile    = false;
    *(MEM0 + base) = 10;
}

#endif // !USE_PTHREAD
#endif // DO_MULTITASK
