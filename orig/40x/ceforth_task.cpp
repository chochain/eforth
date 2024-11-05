///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"

extern void CALL(VM &vm, IU w);

List<VM, E4_VM_POOL_SZ> _vm;

VM& vm_get(int id) {
    return (id >= 0 && id < E4_VM_POOL_SZ) ? _vm[id] : _vm[0];
}

#if DO_MULTITASK
int vm_create(VM& vm0, IU xt) {
    int i = 1;
    while (i < E4_VM_POOL_SZ && _vm[i].state != STOP) i++;
    if (i >= E4_VM_POOL_SZ) return 0;
    
    VM &vm1 = _vm[i];
//    memcpy(&vm[i], &vm0, sizeof(VM));   /// duplicate stacks?
    vm1._ip = xt;

    return i;
}
void vm_exec(int id) {
    VM &vm = vm_get(id);
    IU w   = vm._ip;
    printf(">> vm[%d] started, vm.state=%d\n", id, vm.state);
    vm.state = QUERY;
    CALL(vm, w);
    printf(">> vm[%d] done state=%d\n", id, vm.state);
    vm.state = STOP;
    vm._ip   = w;                        /// restore 
}
void vm_start(int id) {
    thread t(vm_exec, id);
    t.detach();
}
#endif // DO_MULTITASK
