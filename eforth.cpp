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

extern "C" int  assemble(U8 *cdata, dicState *st);
extern "C" void vm_init(U8 *rom, U8 *cdata, dicState *st);
extern "C" void vm_run();

extern U32 rom[];
static U8  _mem[FORTH_ROM_SZ];        		  // default 4K forth ROM block

void dump_data(U8* cdata, int len) {
#if ASM_TRACE
	printf("\n");
    for (int p=0; p<len+0x20; p+=0x20) {
        U32 *x = (U32*)&cdata[p];
        for (int i=0; i<0x8; i++, x++) {
            printf("0x%08x,", *x);
        }
        printf(" // %04x ", p);
        for (int i=0; i<0x20; i++) {
            U8 c = cdata[p+i];
            printf("%c", c ? ((c!=0x5c && c>0x1f && c<0x7f) ? c : '_') : '.');
        }
        printf("\n");
    }
#endif // ASM_TRACE
    printf("\nPrimitives=%d, Addr=%d-bit, CELL=%d", FORTH_PRIMITIVES, (int)sizeof(XA)*8, CELLSZ);
    printf("\nROM = x%x", FORTH_ROM_SZ);
    printf("\nRAM = x%x", FORTH_RAM_SZ);
    printf("\n  STACK x%04x+%04x", FORTH_STACK_ADDR, FORTH_STACK_SZ);
    printf("\n  TIB   x%04x+%04x", FORTH_TIB_ADDR,   FORTH_TIB_SZ);
    printf("\n  USER  x%04x+%04x (flash here)", FORTH_TVAR_ADDR,  FORTH_DIC_ADDR-FORTH_TVAR_ADDR);
    printf("\n  DIC   x%04x+%04x", FORTH_DIC_ADDR,   FORTH_RAM_ADDR+FORTH_RAM_SZ-FORTH_DIC_ADDR);
    printf("\nHERE x%04x", len);
    printf("\neForth16 v1.0");
}

int main(int ac, char* av[])
{
	setvbuf(stdout, NULL, _IONBF, 0);		// autoflush (turn STDOUT buffering off)

	U8 *cdata = _mem;
    dicState st;
	int sz = assemble(cdata, &st);
	dump_data(cdata, sz);

	vm_init((U8*)rom, cdata, &st);
	vm_run();

	return 0;
}




