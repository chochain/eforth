#ifndef __EFORTH_SRC_EFORTH_H
#define __EFORTH_SRC_EFORTH_H
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#define EFORTH_8BIT     1
#define ROM_DUMP        0
//
// debugging flags
//
#define PRINTF(s, ...)  printf(s, ##__VA_ARGS__)
#define GETCHAR()       getchar()
#define ASM_TRACE       0
#define EXE_TRACE       0
//
// portable types
//
typedef uint32_t  U32;   // unsigned integer
typedef uint16_t  U16;
typedef uint8_t   U8;

typedef int32_t   S32;   // signed integers
typedef int16_t   S16;   // 16-bit data 
typedef int8_t    S8;

typedef U16       XA;	 // 16-bit address
#define CELLSZ	  2
//
// capacity and sizing
//
#define FORTH_PRIMITIVES 64
#define FORTH_ROM_SZ     0x1000
#define FORTH_RAM_SZ     0x500
#define FORTH_STACK_SZ   0x60*CELLSZ
#define FORTH_TIB_SZ     0x40
#define FORTH_PAD_SZ     0x20
//
// logic and stack op macros (processor dependent)
//
#define FORTH_BOOT_ADDR  0x0
#define FORTH_RAM_ADDR   0x1000
#define FORTH_STACK_ADDR (FORTH_RAM_ADDR+0x0)
#define FORTH_STACK_TOP  (FORTH_STACK_ADDR+FORTH_STACK_SZ)
#define FORTH_TIB_ADDR   (FORTH_STACK_TOP)
#define FORTH_TVAR_ADDR  (FORTH_TIB_ADDR+FORTH_TIB_SZ)
#define FORTH_UVAR_ADDR  (FORTH_TVAR_ADDR+0x10)
#define FORTH_DIC_ADDR   (FORTH_UVAR_ADDR+0x10)
//
// TRUE cannot use 1 because NOT(ffffffff)==0 while NOT(1)==ffffffff
// which does not need boolean op (i.e. in C)
//
#define	TRUE	         -1
#define	FALSE	         0
//
// Forth VM Opcodes (for Bytecode Assembler)
//
enum {
    opNOP = 0,    // 0
    opBYE,        // 1
    opQRX,        // 2
    opTXSTO,      // 3
    
    opDOCON,      // 4
    opDOLIT,      // 5
    opENTER,      // 6
    opEXIT,       // 7
    opEXECU,      // 8
    opDONEXT,     // 9
    opQBRAN,      // 10
    opBRAN,       // 11
    
    opSTORE,      // 12
    opAT,         // 13
    opCSTOR,      // 14
    opCAT,        // 15
    opONEP,       // 16 Dr. Ting's opRPAT
    opONEM,       // 17 Dr. Ting's opRPSTO
    opRFROM,      // 18
    opRAT,        // 19
    opTOR,        // 20
    opDELAY,      // 21 Dr. Ting's opPSPAT 
    opCLOCK,      // 22 Dr. Ting's opSPSTO
    
    opDROP,       // 23
    opDUP,        // 24
    opSWAP,       // 25
    opOVER,       // 26
    
    opZLESS,      // 27
    opAND,        // 28
    opOR,         // 29
    opXOR,        // 30
    opUPLUS,      // 31
    opNEXT,       // 32
    opQDUP,       // 33
    opROT,        // 34
    opDDROP,      // 35
    opDDUP,       // 36
    
    opPLUS,       // 37
    opNOT,        // 38 =opINVERT
    opNEGATE,     // 39
    opGREAT,      // 40 Dr. Ting' opDNEGA
    opSUB,        // 41
    opABS,        // 42
    opEQUAL,      // 43
    opULESS,      // 44
    opLESS,       // 45
    
    opUMMOD,      // 46
    opPIN,        // 47 Dr. Ting's opMSMOD
    opMAP,        // 48 Dr. Ting's opSLMOD
    
    opMOD,        // 49
    opSLASH,      // 50
    opUMSTAR,     // 51 =opUMSTA
    opSTAR,       // 52
    opMSTAR,      // 53
    opDIN,        // 54 Dr. Ting's opSSMOD
    opDOUT,       // 55 Dr. Ting's opSTASL
    opPICK,       // 56
    opPSTOR,      // 57
    opAIN,        // 58 Dr. Ting's opDSTOR
    opAOUT,       // 59 Dr. Ting's opDAT
    opCOUNT,      // 60
    opDOVAR,      // 61
    opMAX,        // 62
    opMIN         // 63
};

#endif // __EFORTH_SRC_EFORTH_H
