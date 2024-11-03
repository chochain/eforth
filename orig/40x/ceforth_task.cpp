///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"

VM      _vm0;                        ///< default task (if no pool)
VM      *_vm;
thread  *_pool;                      ///< std::thread
int      _pool_sz = 0;

VM& vm_instance(int id) {
    return id < _pool_sz ? _vm[id] : _vm0;
}
int vm_pool(int n) {
    if (_pool_sz) return 0;             ///< already initailized

    _vm = n > 1 ? (_pool = new thread[n], new VM[n]) : &_vm0;
    if (!_vm || (n > 1 && !_pool)) {
        LOGS("VM and thread pool allocation failed\n");
        return 1;
    }
    _pool_sz = n;

    return 0;
}
