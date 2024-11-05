///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"

extern void CALL(VM &vm, IU w);

VM    *_vm;
int   _pool_sz = 0;

int vm_pool_init(int n) {
    if (_pool_sz) return 0;             ///< already initailized

    _vm = new VM[n];
    if (!_vm) {
        LOGS("VM and thread pool allocation failed\n");
        return 1;
    }
    _pool_sz = n;

    return 0;
}
VM& vm_get(int id) {
    return (id >= 0 && id < _pool_sz) ? _vm[id] : _vm[0];
}

#if DO_MULTITASK
int vm_create(VM& vm0, IU xt) {
    int i = 1;
    while (i < _pool_sz && _vm[i].state != STOP) i++;
    if (i >= _pool_sz) return 0;
    
    VM &vm1 = _vm[i];
//    memcpy(&vm[i], &vm0, sizeof(VM));   /// duplicate stacks
    vm1._ip = xt;

    return i;
}
void vm_exec(int id) {
    VM &vm = vm_get(id);
    printf(">> thread[%d] started, vm.state=%d\n", id, vm.state);
    vm.state = QUERY;
    CALL(vm, vm._ip);
    printf(">> thread[%d] done state=%d\n", id, vm.state);
    vm.state = STOP;
}
void vm_start(int id) {
    thread t(vm_exec, id);
    t.detach();
}
#endif // DO_MULTITASK
