///
/// @file
/// @brief eForth platform for WASM
///
#include <stdio.h>
#include <emscripten.h>

extern void forth_init();
extern void forth_vm(const char *cmd, void(*)(int, const char*)=NULL);

const char* APP_VERSION = "eForth v4.2";
///====================================================================
///
///> Memory statistics - for heap, stack, external memory debugging
///
void mem_stat()                    { printf("%s\n", APP_VERSION); }
void forth_include(const char *fn) {
    const int BUF = HERE + 64;
    const int rst = EM_ASM_INT({
        const xhr = new XMLHttpRequest();
        xhr.responseType = 'text';                /// Forth script
        xhr.open('GET', UTF8ToString($0), false); /// synchronized GET
        xhr.send(null);
        if (xhr.status!=200) {
            console.log(xhr.statusText);
            return 0;
        }
        const wa  = wasmExports;               ///< WASM export block
        const adr = wa.vm_mem() + $1;          ///< memory address
        const len = xhr.responseText.length+1; ///< script + '\0'
        const buf = new Uint8Array(wa.memory.buffer, adr, len);
        for (var i=0; i < len; i++) {
            buf[i] = xhr.responseText.charCodeAt(i);
        }
        buf[len-1] = '\0';                     /// * \0 terminated str
        return len;
        }, fn, BUF);
    if (rst==0) return 0;                      /// * fetch failed, bail
    ///
    /// preserve I/O states, call VM, restore IO states
    ///
    void (*cb)(int, const char*) = fout_cb;    ///< keep output function
    string in; getline(fin, in);               ///< keep input buffers
    fout << ENDL;                              /// * flush output
    
    forth_vm((const char*)&pmem[BUF]);         /// * send script to VM
    
    fout_cb = cb;                              /// * restore output cb
    fin.clear(); fin.str(in);                  /// * restore input
    return rst;
}

///====================================================================
///
/// main program
///
int main(int ac, char* av[]) {
    forth_init();
    return 0;
}
///====================================================================
///
/// WASM/Emscripten ccall interfaces
///
extern "C" {
    void forth(int n, char *cmd) { forth_vm(cmd); }
}
