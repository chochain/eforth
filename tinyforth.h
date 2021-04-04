#ifndef __GOOF_SRC_TINYFORTH_H
#define __GOOF_SRC_TINYFORTH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define EXE_TRACE  1

typedef uint8_t    U8;
typedef uint16_t   U16;
typedef int16_t    S16;

#define BUF_SZ     10       /* 8 - 255    */
#define STK_SZ     (64)     /* 8 - 65536  */
#define DIC_SZ     (512)    /* 8 - 8*1024 */

#define getchr()   getchar()
#define putchr(c)  putchar(c)
//
// length + space delimited 3-char string
//
enum {
    TKN_EXE = 1,
    TKN_DIC,
    TKN_EXT,
    TKN_PRM,
    TKN_NUM,
    TKN_ERR
};
#define LST_CMD    "\x05" \
    ":  " "VAR" "FGT" "DMP" "BYE"
#define LST_JMP    "\x0b" \
    ";  " "IF " "ELS" "THN" "BGN" "UTL" "WHL" "RPT" "DO " "LOP" "I  "
#define LST_PRM    "\x19" \
	"DRP" "DUP" "SWP" ">R " "R> " "+  " "-  " "*  " "/  " "MOD" \
	"AND" "OR " "XOR" "=  " "<  " ">  " "<= " ">= " "<> " "NOT" \
    "@  " "!  " "C@ " "C! " ".  " 
#define LST_EXT    "\x09" \
    "HRE" "CP " "OVR" "INV" "CEL" "ALO" "WRD" "SAV" "LD "
//
// ============================================================================================
// Opcode formats
//     primitive:  111c cccc                      (32 primitive)
//     branching:  1BBa aaaa aaaa aaaa            (+- 12-bit address)
//     1-byte lit: 0nnn nnnn                      (0..127)
//     3-byte lit: 1111 1111 nnnn nnnn nnnn nnnn
// ============================================================================================
//
// branch flags (1BBx)
//
#define JMP_BIT    0x1000          /* 0001 0000 0000 0000 12-bit offset */
#define PFX_UDJ    0x80            /* 1000 0000 */
#define PFX_CDJ    0xa0            /* 1010 0000 */
#define PFX_CALL   0xc0            /* 1100 0000 */
#define PFX_PRM    0xe0            /* 1110 0000 */
//
// opcodes for loop control, literal, and extensions
//
#define I_LOOP     (PFX_PRM | 25)  /* 19 1111 1001 */
#define I_RD2      (PFX_PRM | 26)  /* 1a 1111 1010 */
#define I_I        (PFX_PRM | 27)  /* 1b 1111 1011 */
#define I_P2R2     (PFX_PRM | 28)  /* 1c 1111 1100 */
#define I_RET      (PFX_PRM | 29)  /* 1d 1111 1101 */
#define I_LIT      (PFX_PRM | 30)  /* 1e 1111 1110 */
#define I_EXT      (PFX_PRM | 31)  /* 1f 1111 1111 */
//
// dictionary address<=>pointer translation macros
//
#define PTR(n)     (dic + (U16)(n))
#define IDX(p)     ((U16)((U8*)(p) - dic))
//
// Forth stack opcode macros
//
#define TOS        (*psp)
#define TOS1       (*(psp+1))
#define PUSH(v)    (*(--psp)=(S16)(v))
#define POP()      (*(psp++))
#define RPUSH(v)   (*(rsp++)=(U16)(v))
#define RPOP()     (*(--rsp))
//
// memory access opcodes
//
#define SET8(p, c)  (*(U8*)(p)++=(U8)(c))
#define SET16(p, n) do { U16 x=(U16)(n); SET8(p, (x)>>8); SET8(p, (x)&0xff); } while(0)
#define GET16(p)    (((U16)*((U8*)(p))<<8) + *((U8*)(p)+1))
#define SETNM(p, s) do {                   \
    SET8(p, (s)[0]);                       \
    SET8(p, (s)[1]);                       \
    SET8(p, ((s)[1]!=' ') ? (s)[2] : ' '); \
    } while(0)
//
// branching opcodes
//
#define JMP000(p,j) SET16(p, (j)<<8)
#define JMPSET(idx, p1) do {               \
    U8  *p = PTR(idx);                     \
    U8  f8 = *(p);                         \
    U16 a  = ((U8*)(p1) - p) + JMP_BIT;    \
    SET16(p, (a | (U16)f8<<8));            \
    } while(0)
#define JMPBCK(idx, f) do {                \
    U8  *p = PTR(idx);                     \
    U16 a  = (U16)(p - dptr) + JMP_BIT;    \
    SET16(dptr, a | (f<<8));               \
    } while(0)
//
// IO functions
//
void putmsg(char *msg);
void putnum(S16 n);
U8   getnum(U8 *str, U16 *num);
U8   *gettkn(void);
//
// dictionary, string list scanners
//
U8 lookup(U8 *key, U16 *adr);
U8 find(U8 *key, const char *lst, U16 *id);
//
// For compiler functions
//
void variable(void);
void compile(void);
//
// Forth VM core functions
//
void forget(void);
void primitive(U8 op);
void execute(U16 adr);
void extended(U8 op);

#endif // __GOOF_SRC_TINYFORTH_H
