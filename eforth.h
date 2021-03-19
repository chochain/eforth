#ifndef __EFORTH_SRC_EFORTH_H
#define __EFORTH_SRC_EFORTH_H
#include <stdio.h>
#include <string.h>
#include <stdint.h>
//
// debugging flags
//
#define ASSEM_DUMP   0
#define FORTH_TRACE  1
//
// portable types
//
typedef uint64_t  U64;
typedef uint32_t  U32;
typedef uint16_t  U16;
typedef uint8_t   U8;
typedef int64_t   S64;

typedef int32_t   S32;
typedef int16_t   S16;
typedef int8_t    S8;
//
// logic and stack op macros
//
#define FORTH_BOOT_ADDR  0x0
#define FORTH_TIB_ADDR   0x80
#define FORTH_UVAR_ADDR  0x90
#define FORTH_DIC_ADDR   0x200
#define FORTH_BUF_ADDR   0x100
#define FORTH_BUF_SIZE   0x100

#define	FALSE	         0
#define	TRUE	         -1
//
// tracing/logging macros
//
#if    ASSEM_DUMP
#define DEBUG(s, v)     printf(s, v)
#define SHOWOP(op)      printf("\n%04x: %s\t", P, op)
#else  // ASSEM_DUMP
#define DEBUG(s, v)
#define SHOWOP(op)
#endif // ASSEM_DUMP
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
    opDOLIST,     // 6
    opEXIT,       // 7
    opEXECU,      // 8
    opDONEXT,     // 9
    opQBRAN,      // 10
    opBRAN,       // 11
    opSTORE,      // 12
    opAT,         // 13
    opCSTOR,      // 14
    opCAT,        // 15
    opRPAT,       // 16   borrow for trc_on
    opRPSTO,      // 17   borrow for trc_off
    opRFROM,      // 18
    opRAT,        // 19
    opTOR,        // 20
    opSPAT,       // 21
    opSPSTO,      // 22
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
    opINVER,      // 38
    opNEGAT,      // 39
    opDNEGA,      // 40
    opSUB,        // 41
    opABS,        // 42
    opEQUAL,      // 43
    opULESS,      // 44
    opLESS,       // 45
    opUMMOD,      // 46
    opMSMOD,      // 47
    opSLMOD,      // 48
    opMOD,        // 49
    opSLASH,      // 50
    opUMSTA,      // 51
    opSTAR,       // 52
    opMSTAR,      // 53
    opSSMOD,      // 54
    opSTASL,      // 55
    opPICK,       // 56
    opPSTOR,      // 57
    opDSTOR,      // 58
    opDAT,        // 59
    opCOUNT,      // 60
    opDOVAR,      // 61
    opMAX,        // 62
    opMIN         // 63
};

struct {
	U8  R, S;              // return stack index, data stack index
	U32 P, IP, WP;         // P (program counter), IP (intruction pointer), WP (parameter pointer)
	U32 thread;            // pointer to previous word
	S32 top0;              // stack top value (cache)
	void (*vtbl[64])();    // opcode vtable
} vmState;

struct {
	U32 rack[256];         // return stack
	S32 stack[256];        // data stack
	U32	data[16000];       // main memory block
	U8  *byte;             // byte stream pointer to data[]
} vmHeap;
//
// shared variables
//
extern U8  R, S;           // return stack and data stack indices
extern U32 P, IP, WP;      // program counter, instruction pointer, and parameter pointer
extern U32 thread;         // previous word pointer
extern S32 top;            // value on top of stack (cache)

extern U32 data[];         // main heap
extern U32 rack[];         // return stack
extern S32 stack[];        // data stack
extern U8  *byte;          // heap byte pointer

extern void (*primitives[])();  // virtual machine vtable

#endif // __EFORTH_SRC_EFORTH_H
