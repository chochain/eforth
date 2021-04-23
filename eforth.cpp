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

extern "C" int  assemble(U8 *cdata, U8 *stack);
extern "C" void vm_init(U8 *cdata, U8 *stack);
extern "C" void vm_run();

static U8 _mem[FORTH_MEM_SZ];        		  // default 8K forth memory block

void dump_data(U8* cdata, int len) {
#if ASM_TRACE
    for (int p=0; p<len+0x20; p+=0x20) {
        printf("\n%04x: ", p);
        for (int i=0; i<0x20; i++) {
        	U8 c = cdata[p+i];
            printf("%02x", c);
            printf("%s", (i%4)==3 ? " " : "");
        }
        for (int i=0; i<0x20; i++) {
            U8 c = cdata[p+i];
            printf("%c", c ? ((c>0x1f && c<0x7f) ? c : '_') : '.');
        }
    }
    printf("\nPrimitives=%d, Addr=%d-bit, CELL=%d", FORTH_PRIMITIVES, (int)sizeof(XA)*8, CELLSZ);
    printf("\nHEAP = x%x", FORTH_MEM_SZ);
    printf("\n  BOOT x%04x", FORTH_BOOT_ADDR);
    printf("\n  USER x%04x+%04x", FORTH_TVAR_ADDR, FORTH_TIB_ADDR-FORTH_TVAR_ADDR);
    printf("\n  TIB  x%04x+%04x", FORTH_TIB_ADDR, FORTH_TIB_SZ);
    printf("\n  STK  x%04x+%04x", FORTH_STACK_ADDR, FORTH_STACK_SZ*CELLSZ);
    printf("\n  DIC  x%04x+%04x", FORTH_DIC_ADDR, FORTH_MEM_SZ-FORTH_DIC_ADDR);
    printf("\n  HERE x%04x", len);
    printf("\neForth16 v1.0");
#endif // ASM_TRACE
}

int main(int ac, char* av[])
{
	U8  *cdata = _mem;
    U8  *stack = &_mem[FORTH_STACK_ADDR];
                                                   
	setvbuf(stdout, NULL, _IONBF, 0);		// autoflush (turn STDOUT buffering off)

	int sz  = assemble(cdata, stack);
	dump_data(cdata, sz);

	vm_init(cdata, stack);
	vm_run();

	return 0;
}

