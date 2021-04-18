/******************************************************************************/
/* ceForth_33.cpp, Version 3.3 : Forth in C                                   */
/******************************************************************************/
/* Chen-Hanson Ting                                                           */
/* 01jul19cht   version 3.3                                                   */
/* Macro assembler, Visual Studio 2019 Community                              */
/* 13jul17cht   version 2.3                                                   */
/* True byte code machine with bytecode                                       */
/* Change w to WP, pointing to parameter field                                */
/* 08jul17cht  version 2.2                                                    */
/* Stacks are 256 cell circular buffers                                       */
/* Clean up, delete SP@, SP!, RP@, RP!                                        */
/* 13jun17cht  version 2.1                                                    */
/* Compiled as a C++ console project in Visual Studio Community 2017          */
/* Follow the eForth model with 64 primitives                                 */
/* Kernel                                                                     */
/* Use long long int to implement multipy and divide primitives               */
/* Case insensitive interpreter                                               */
/* data[] must be filled with rom_21.h eForth dictionary                      */
/*   from c:/F#/ceforth_21                                                    */
/* C compiler must be reminded that S and R are (char)                        */
/******************************************************************************/
#include <stdio.h>
#include "eforth.h"

extern "C" int  assemble(U8 *rom);
extern "C" void vm_init(U8 *rom);
extern "C" void vm_run();

U32 data[FORTH_DATA_SZ] = {};           		// 64K forth memory block

void dump_data(U8* cdata, int len) {
#if EXE_TRACE
    for (int p=0; p<len; p+=0x20) {
        PRINTF("\n%04x: ", p);
        for (int i=0; i<0x20; i++) {
        	U8 c = cdata[p+i];
            PRINTF("%02x", c);
            PRINTF("%s", (i%4)==3 ? " " : "");
        }
        for (int i=0; i<0x20; i++) {
            U8 c = cdata[p+i];
            PRINTF("%c", c ? ((c>32 && c<127) ? c : '_') : '.');
        }
    }
#endif // EXE_TRACE
}

int main(int ac, char* av[])
{
	U8 *cdata = (U8*)data;
	setvbuf(stdout, NULL, _IONBF, 0);		// autoflush (turn STDOUT buffering off)

	int sz  = assemble(cdata);
	dump_data(cdata, sz+0x20);

	printf("\nceForth v4.0 ROM[%04x]\n", sz);
	vm_init(cdata);
	vm_run();

	return 0;
}

