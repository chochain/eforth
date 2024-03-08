///
/// @file
/// @brief eForth main program for testing on Desktop PC (Linux and Cygwin)
///
/// version info
///
#define APP_NAME         "eForth"
#define MAJOR_VERSION    "8"
#define MINOR_VERSION    "6"

const char *vm_version(){
    static string ver = string(APP_NAME) + " " + MAJOR_VERSION + "." + MINOR_VERSION;
    return ver.c_str();
}
///====================================================================
///
///> System statistics - for heap, stack, external memory debugging
///
#if CC_DEBUG
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
        printf("%3d> xt=%4x:%p name=%4x:%p %s\n",
            i, c.xtoff(), c.xt,
            (U16)((UFP)c.name - Code::NM0),
            c.name, c.name);
    }
}
#else  // CC_DEBUG
void mem_stat()   {}
void dict_dump()  {}

#endif // CC_DEBUG
///
/// ForthVM outer interpreter
///
void vm_outer(const char *cmd, void(*callback)(int, const char*)) {
    fin.clear();               ///> clear input stream error bit if any
    fin.str(cmd);              ///> feed user command into input stream
    fout_cb = callback;        ///> setup callback function
    fout.str("");              ///> clean output buffer, ready for next run
    while (fin >> strbuf) {    ///> outer interpreter loop
        const char *idiom = strbuf.c_str();
        forth_core(idiom);     ///> single command to Forth core
    }
    if (!compile) ss_dump();   /// * dump stack and display ok prompt
}
///====================================================================
///
/// main program - Note: Arduino and ESP32 is have their own main-loop
///
#include <iostream>                            // cin, cout
int main(int ac, char* av[]) {
    dict_compile();                            ///> initialize dictionary
    dict_dump();
    mem_stat();
    
    cout << vm_version() << endl;

    static auto send_to_con = [](int len, const char *rst) { cout << rst; };
    string cmd;
    while (getline(cin, cmd)) {                ///> fetch user input
        // printf("cmd=<%s>\n", cmd.c_str());
        vm_outer(cmd.c_str(), send_to_con);    ///> execute outer interpreter
    }
    cout << "Done!" << endl;
    return 0;
}
///====================================================================
