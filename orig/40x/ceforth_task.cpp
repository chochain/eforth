///
/// @file
/// @brief eForth - multi-tasking support
///
#include "ceforth.h"

extern void nest(VM &vm);

List<VM, E4_VM_POOL_SZ> _vm;

VM& vm_get(int id) {
    return (id >= 0 && id < E4_VM_POOL_SZ) ? _vm[id] : _vm[0];
}

#if DO_MULTITASK
int vm_create(IU pfa) {
    int i = 1;
    while (i < E4_VM_POOL_SZ && _vm[i].state != STOP) i++;
    if (i >= E4_VM_POOL_SZ) return 0;
    
    VM &vm1 = _vm[i];
//    memcpy(&vm[i], &vm0, sizeof(VM));   /// duplicate stacks?
    vm1.state = HOLD;                     /// STOP=>HOLD, fake resume, CC: lock?
    vm1._ip   = pfa;                      /// at given pfa (of colon word)

    return i;
}
void vm_exec(int id) {
    VM &vm = vm_get(id);
    IU pfa = vm._ip;
    
    printf(">> vm[%d] started, vm.state=%d\n", id, vm.state);
    vm._rs.push(DU0);                    /// exit token
    while (vm.state==HOLD) nest(vm);
    printf(">> vm[%d] done state=%d\n", id, vm.state);
    vm._ip   = pfa;                      /// keep w for restart
}
void vm_start(int id) {
    thread t(vm_exec, id);
    t.detach();
}
#endif // DO_MULTITASK
