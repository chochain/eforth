///
/// @file
/// @brief eForth platform for WASM
///
#define APP_NAME         "weForth"
#define MAJOR_VERSION    "1"
#define MINOR_VERSION    "0"

const char *vm_version(){
    static string ver = string(APP_NAME) + " " + MAJOR_VERSION + "." + MINOR_VERSION;
    return ver.c_str();
}

void mem_stat()  {
    LOG_KX("heap[maxblk=", E4_PMEM_SZ);
    LOG_KX(", avail=",     E4_PMEM_SZ - HERE);
    LOG_KX(", ss_max=",    ss.max);
    LOG_KX(", rs_max=",    rs.max);
    LOG_KX(", pmem=",      HERE);
    LOG_KX("], stack_sz=", E4_SS_SZ);
}
void dict_dump() {
    printf("XT0=%lx, NM0=%lx, sizeof(Code)=%ld bytes\n",
           Code::XT0, Code::NM0, sizeof(Code));
    for (int i=0; i<dict.idx; i++) {
        Code &c = dict[i];
        printf("%3d> xt=%x:%8lx name=%4x:%p %s\n",
               i, c.xtoff(), (uintptr_t)c.xt,
            (U16)((UFP)c.name - Code::NM0),
            c.name, c.name);
    }
}

extern "C" {
void forth(int n, char *cmd) {
    forth_core(cmd);
}
int  vm_base()       { return base;     }
int  vm_ss_idx()     { return ss.idx;   }
int  vm_dict_idx()   { return dict.idx; }
DU   *vm_ss()        { return &ss[0];   }
char *vm_dict(int i) { return (char*)dict[i].name; }
char *vm_mem()       { return (char*)&pmem[0]; }
}

#include <iostream>
int main(int ac, char* av[]) {
    dict_compile();               // initialize dictionary
    dict_dump();
    mem_stat();
    
    cout << endl << vm_version() << endl;
    
    forth_core("words");
    
    cout << "Done!" << endl;
    return 0;
}
