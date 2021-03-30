#ifndef __GOOF_SRC_TINYFORTH_H
#define __GOOF_SRC_TINYFORTH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint16_t U16;
typedef uint8_t  U8;

#define BUF_SZ     10       /* 8 - 255    */
#define STK_SZ     (64)     /* 8 - 65536  */
#define DIC_SZ     (512)    /* 8 - 8*1024 */

#define putchr(c)  putchar(c)
#define getchr()   getchar()
//
// length + space delimited 3-char string
//
#define LST_RUN    "\x04" ":  " "VAR" "FGT" "BYE"
#define LST_COM    "\x0b" ";  " "IF " "ELS" "THN" "BGN" "UTL" "WHL" "RPT" "DO " "LOP" "I  "
#define LST_PRM    "\x19" \
	"DRP" "DUP" "SWP" ">R " "R> " "+  " "-  " "*  " "/  " "MOD" \
	"AND" "OR " "XOR" "=  " "<  " ">  " "<= " ">= " "<> " "NOT" \
	"@  " "!  " "C@ " "C! " ".  "
//
// branch flags
//
#define JMP_SGN    0x1000
#define PFX_UDJ    0x80
#define PFX_CDJ    0xa0
#define PFX_CALL   0xc0
#define PFX_PRM    0xe0
//
// alloc for 2 extra opcode, borrow 2 blocks (2*256 bytes) from offset field
//
#define I_LIT      0xff
#define I_RET      0xfe
//
// append 5 more opcode to the end of primitives
//
#define I_LOOP     (PFX_PRM | 25)
#define I_RD2      (PFX_PRM | 26)
#define I_I        (PFX_PRM | 27)
#define I_P2R2     (PFX_PRM | 28)
#define BYE        (PFX_PRM | 29)
//
// dictionary address<=>pointer translation macros
//
#define PTR(n)     (dic + (n))
#define IDX(p)     ((U16)((U8*)(p) - dic))
//
// Forth opcode macros
//
#define TOS         (*psp)
#define PUSH(v)     (*(--psp)=(U16)(v))
#define POP()       ((U16)(*(psp++)))
#define RPUSH(v)    (*(rsp++)=(U16)(v))
#define RPOP()      ((U16)(*(--rsp)))
#define SET8(p, c)  (*((p)++)=(U8)(c))
#define SET16(p, n) do { U16 x=(n); SET8(p, (x)>>8); SET8(p, (x)&0xff); } while(0)
#define SETNM(p, s) do { SET8(p, (s)[0]); SET8(p, (s)[1]); SET8(p, ((s)[1]!=' ') ? (s)[2] : ' '); } while(0)
#define GET16(p)    (((U16)*((U8*)(p))<<8) + *((U8*)(p)+1))
#define JMP000(p,j) SET16(p, (j)<<8)
#define JMPSET(idx, p1) do {             \
    U8  *p = PTR(idx);                   \
    U8  f8 = *(p);                       \
    U16 a  = ((U8*)(p1) - p) + JMP_SGN;  \
    SET16(p, (a | (U16)f8<<8));          \
    } while(0)
#define JMPBCK(idx, f) do {              \
    U8  *p = PTR(idx);                   \
    U16 a  = (U16)(p - dptr) + JMP_SGN;  \
    SET16(dptr, a | (f<<8));             \
    } while(0)
//
// IO functions
//
void putmsg(char *msg);
void putnum(U16 n);
void puthex(U8 c);
void putadr(U16 a);
U8   *gettkn(void);
//
// dictionary, string list scanners
//
U8 lookup(U8 *key, U16 *adr);
U8 find(U8 *key, char *lst, U16 *id);
//
// Forth VM core functions
//
void compile(void);
void variable(void);
void forget(void);
U8   literal(U8 *str, U16 *num);
void execute(U16 adrs);
void primitive(U8 ic);

#endif // __GOOF_SRC_TINYFORTH_H
