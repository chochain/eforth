///
/// @file
/// @brief eForth platform for WASM
///
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

int main(int ac, char* av[]) {
    dict_compile();               // initialize dictionary
    dict_dump();
    mem_stat();
    
    forth_core("words");
    
    return 0;
}
