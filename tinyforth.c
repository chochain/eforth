/*
  Tiny FORTH
  T. NAKAGAWA
  2004/07/04-10,7/29,8/5-6
*/
#include <stdio.h>
#include <stdint.h>

typedef uint16_t U16;
typedef uint8_t  U8;

#define BUF_SIZE            10       /* 8 - 255    */
#define STACK_SIZE          (64)     /* 8 - 65536  */
#define DIC_SIZE            (512)    /* 8 - 8*1024 */

#define putchr(c)           putchar(c)
#define getchr()            getchar()
#define V2R(base, n)        ((U8*)((base) + (n)))
#define R2V(base, p)        ((U16)((p) - (base)))
//
// length + space delimited 3-char string
//
#define KEY_RUNMODE         "\x03" ":  " "VAR" "FGT"
#define KEY_COMPILEMODE     "\x0b" ";  " "IF " "ELS" "THN" "BGN" "END" "WHL" "RPT" "DO " "LOP" "I  "
#define KEY_PRIMITIVE       "\x19" "DRP" "DUP" "SWP" ">R " "R> " "+  " "-  " "*  " "/  " "MOD" "AND" "OR " "XOR" "=  " "<  " ">  " "<= " ">= " "<> " "NOT" "@  " "@@ " "!  " "!! " ".  "

#define JMP_FLG             0x1000
#define PFX_UDJ             0x80U
#define PFX_CDJ             0xa0U
#define PFX_CALL            0xc0U
#define PFX_PRIMITIVE       0xe0U

#define I_LIT               0xffU
#define I_RET               0xfeU
#define I_LOOP              (PFX_PRIMITIVE | 25U)
#define I_RDROP2            (PFX_PRIMITIVE | 26U)
#define I_I                 (PFX_PRIMITIVE | 27U)
#define I_P2R2              (PFX_PRIMITIVE | 28U)

static U16  stack[STACK_SIZE];
static U16  *retstk;
static U16  *parstk;
static U8   dic[DIC_SIZE];
static U8   *dicptr;
static U8   *dicent;

static void putmsg(char *msg);
static U8   *gettkn(void);
static char literal(U8 *str, U16 *num);
static char lookup(U8 *key, U16 *adrs);
static char find(U8 *key, char *list, U8 *id);
static void compile(void);
static void variable(void);
static void forget(void);
static void execute(U16 adrs);
static void primitive(U8 ic);
static void putnum(U16 num);

int main(void) {
    /* Initialize the stack and dictionary */
    retstk = &(stack[0]);
    parstk = &(stack[STACK_SIZE]);
    dicptr = dic;
    dicent = V2R(dic, 0xffffU);

    putmsg("Tiny FORTH");

    for (;;) {
        U8 tmp8;
        U16 tmp16;
        U8 *tkn;

        tkn = gettkn();

        /* keyword */
        if (find(tkn, KEY_RUNMODE, &tmp8)) {
            switch (tmp8) {
            case 0:	/* :   */ compile();     break;
            case 1:	/* VAR */ variable();    break;
            case 2:	/* FGT */ forget();      break;
            }
        }
        else if (lookup(tkn, &tmp16)) {
            execute(tmp16 + 2 + 3);
        }
        else if (find(tkn, KEY_PRIMITIVE, &tmp8)) {
            primitive(tmp8);
        }
        else if (literal(tkn, &tmp16)) {
            *(--parstk) = tmp16;
        }
        else {
            /* error */
            putmsg("?");
            continue;
        }
        if (parstk > &(stack[STACK_SIZE])) {
            putmsg("OVF");
            parstk = &(stack[STACK_SIZE]);
        }
        else putmsg("OK");
    }
}
/*
  Put a message
*/
static void putmsg(char *msg) {
    while (*msg != '\0') putchr(*(msg++));
    putchr('\r');
    putchr('\n');
    return;
}
/*
  Get a Token
*/
static U8 *gettkn(void) {
    static U8 buf[BUF_SIZE] = " ";	/*==" \0\0\0..." */
    U8 ptr;

    /* remove leading non-delimiters */
    while (*buf != ' ') {
        for (ptr = 0; ptr < BUF_SIZE - 1; ptr++) buf[ptr] = buf[ptr + 1];
        buf[ptr] = '\0';
    }
    for (;;) {
        /* remove leading delimiters */
        while (*buf==' ') {
            for (ptr = 0; ptr < BUF_SIZE - 1; ptr++) buf[ptr] = buf[ptr + 1];
            buf[ptr] = '\0';
        }
        if (*buf) return buf;
        
        for (ptr=0;;) {
            U8 c = getchr();
            if (c=='\n') {
                putchr('\n');
                buf[ptr] = ' ';
                break;
            }
            else if (c=='\b') {
                if (ptr==0) continue;
                buf[--ptr] = '\0';
                putchr(' ');
                putchr('\b');
            }
            else if (c <= 0x1fU)         {}
            else if (ptr < BUF_SIZE - 1) { buf[ptr++] = c; }
            else {
                putchr('\b');
                putchr(' ');
                putchr('\b');
            }
        }
    }
}
/*
  Process a Literal
*/
static char literal(U8 *str, U16 *num) {
    if (*str=='$') {
        U16 n = 0;
        for (str++; *str != ' '; str++) {
            n *= 16;
            n += *str - (*str<='9' '0' : 'A' - 10);
        }
        *num = n;
        return 1;
    }
    if ('0' <= *str && *str <= '9') {
        U16 n = 0;
        for (; *str != ' '; str++) {
            n *= 10;
            n += *str - '0';
        }
        *num = n;
        return 1;
    }
    return 0;
}
/*
  Lookup the Keyword from the Dictionary
*/
static char lookup(U8 *key, U16 *adrs) {
    for (U8 *ptr = dicent; ptr != V2R(dic, 0xffffU); ptr = V2R(dic, *ptr + *(ptr+1) * 256U)) {
        if (ptr[2]==key[0] && ptr[3]==key[1] && (ptr[3]==' ' || ptr[4]==key[2])) {
            *adrs = R2V(dic, ptr);
            return 1;
        }
    }
    return 0;
}
/*
  Find the Keyword in a List
*/
static char find(U8 *key, char *list, U8 *id) {
    for (U8 n=0, m=*(list++); n < m; n++, list += 3) {
        if (list[0]==key[0] && list[1]==key[1] && (key[1]==' ' || list[2]==key[2])) {
            *id = n;
            return 1;
        }
    }
    return 0;
}
/*
  Compile Mode
*/
static void compile(void) {
    U8 *tkn;
    U8  tmp8;
    U16 tmp16;

    /* get the identifier */
    tkn = gettkn();

    /* Write the header */
    tmp16 = R2V(dic, dicent);
    dicent = dicptr;
    *(dicptr++) = tmp16 % 256U;
    *(dicptr++) = tmp16 / 256U;
    *(dicptr++) = tkn[0];
    *(dicptr++) = tkn[1];
    *(dicptr++) = (tkn[1] != ' ') ? tkn[2] : ' ';

    for (;;) {
        U8 *ptr;

        putmsg(">");    // in compile mode
        tkn = gettkn();

        if (find(tkn, KEY_COMPILEMODE, &tmp8)) {
            if (tmp8==0) {	/* ; */
                *(dicptr++) = I_RET;
                break;
            }
            switch (tmp8) {
            case 1:	/* IF */
                *(retstk++) = R2V(dic, dicptr);
                *(dicptr++) = PFX_CDJ;
                dicptr++;
                break;
            case 2:	/* ELS */
                tmp16 = *(--retstk);
                ptr = V2R(dic, tmp16);
                tmp8 = *(ptr);
                tmp16 = R2V(dic, dicptr + 2) - tmp16 + JMP_FLG;
                *(ptr++) = tmp8 | (tmp16 / 256U);
                *(ptr++) = tmp16 % 256U;
                *(retstk++) = R2V(dic, dicptr);
                *(dicptr++) = PFX_UDJ;
                dicptr++;
                break;
            case 3:	/* THN */
                tmp16 = *(--retstk);
                ptr = V2R(dic, tmp16);
                tmp8 = *(ptr);
                tmp16 = R2V(dic, dicptr) - tmp16 + JMP_FLG;
                *(ptr++) = tmp8 | (tmp16 / 256U);
                *(ptr++) = tmp16 % 256U;
                break;
            case 4:	/* BGN */
                *(retstk++) = R2V(dic, dicptr);
                break;
            case 5:	/* END */
                tmp16 = *(--retstk) - R2V(dic, dicptr) + JMP_FLG;
                *(dicptr++) = PFX_CDJ | (tmp16 / 256U);
                *(dicptr++) = tmp16 % 256U;
                break;
            case 6:	/* WHL */
                *(retstk++) = R2V(dic, dicptr);
                dicptr += 2;
                break;
            case 7:	/* RPT */
                tmp16 = *(--retstk);
                ptr = V2R(dic, tmp16);
                tmp16 = R2V(dic, dicptr + 2) - tmp16 + JMP_FLG;
                *(ptr++) = PFX_CDJ | (tmp16 / 256U);
                *(ptr++) = tmp16 % 256U;
                tmp16 = *(--retstk) - R2V(dic, dicptr) + JMP_FLG;
                *(dicptr++) = PFX_UDJ | (tmp16 / 256U);
                *(dicptr++) = tmp16 % 256U;
                break;
            case 8:	/* DO */
                *(retstk++) = R2V(dic, dicptr+1);
                *(dicptr++) = I_P2R2;
                break;
            case 9:	/* LOP */
                *(dicptr++) = I_LOOP;
                tmp16 = *(--retstk) - R2V(dic, dicptr) + JMP_FLG;
                *(dicptr++) = PFX_CDJ | (tmp16 / 256U);
                *(dicptr++) = tmp16 % 256U;
                *(dicptr++) = I_RDROP2;
                break;
            case 10:	/* I */
                *(dicptr++) = I_I;
                break;
            }
        }
        else if (lookup(tkn, &tmp16)) {
            tmp16 += 2 + 3 - R2V(dic, dicptr) + JMP_FLG;
            *(dicptr++) = PFX_CALL | (tmp16 / 256U);
            *(dicptr++) = tmp16 % 256U;
        }
        else if (find(tkn, KEY_PRIMITIVE, &tmp8)) {
            *(dicptr++) = PFX_PRIMITIVE | tmp8;
        }
        else if (literal(tkn, &tmp16)) {
            if (tmp16 < 128U) {
                *(dicptr++) = (U8)tmp16;
            } else {
                *(dicptr++) = I_LIT;
                *(dicptr++) = tmp16 % 256U;
                *(dicptr++) = tmp16 / 256U;
            }
        }
        else /* error */ putmsg("!");
    }
}
/*
  VARIABLE instruction
*/
static void variable(void) {
    U8 *tkn;
    U16 tmp16;

    /* get an identifier */
    tkn = gettkn();

    /* Write the header */
    tmp16 = R2V(dic, dicent);
    dicent = dicptr;
    *(dicptr++) = tmp16 % 256U;
    *(dicptr++) = tmp16 / 256U;
    *(dicptr++) = tkn[0];
    *(dicptr++) = tkn[1];
    *(dicptr++) = (tkn[1] != ' ') ? tkn[2] : ' ';

    tmp16 = R2V(dic, dicptr + 2);
    if (tmp16 < 128U) {
        *(dicptr++) = (U8)tmp16;
    } else {
        tmp16 = R2V(dic, dicptr + 4);
        *(dicptr++) = I_LIT;
        *(dicptr++) = tmp16 % 256U;
        *(dicptr++) = tmp16 / 256U;
    }
    *(dicptr++) = I_RET;
    *(dicptr++) = 0;	/* data area */
    *(dicptr++) = 0;	/* data area */

    return;
}
/*
  Forget Words in the Dictionary
*/
static void forget(void) {
    U16 tmp16;
    U8 *ptr;

    /* get a word */
    if (!lookup(gettkn(), &tmp16)) {
        putmsg("??");
        return;
    }

    ptr = V2R(dic, tmp16);
    dicent = V2R(dic, *ptr + *(ptr+1) * 256U);
    dicptr = ptr;

    return;
}
/*
  Virtual Code Execution
*/
static void execute(U16 adrs) {
    U8 *pc;
    *(retstk++) = 0xffffU;

    for (pc = V2R(dic, adrs); pc != V2R(dic, 0xffffU); ) {
        U8 ir;	/* instruction register */

        ir = *(pc++);

        if ((ir & 0x80U)==0) {
            /* literal(0-127) */
            *(--parstk) = ir;
        }
        else if (ir==I_LIT) {
            /* literal(128-65535) */
            U16 tmp16;
            tmp16 = *(pc++);
            tmp16 += *(pc++) * 256U;
            *(--parstk) = tmp16;
        }
        else if (ir==I_RET) {
            /* RET: return */
            pc = V2R(dic, *(--retstk));
        }
        else if ((ir & 0xe0U)==PFX_UDJ) {
            /* UDJ: unconditional direct jump */
            pc = V2R(dic, R2V(dic, pc-1) + (ir & 0x1fU) * 256U + *pc - JMP_FLG);
        }
        else if ((ir & 0xe0U)==PFX_CDJ) {
            /* CDJ: conditional direct jump */
            pc += (*(parstk++))
                ? pc+1
                : V2R(dic, R2V(dic, pc-1) + (ir & 0x1fU) * 256U + *pc - JMP_FLG);
        }
        else if ((ir & 0xe0U)==PFX_CALL) {
            /* CALL: subroutine call */
            *(retstk++) = R2V(dic, pc+1);
            pc = V2R(dic, R2V(dic, pc-1) + (ir & 0x1fU) * 256U + *pc - JMP_FLG);
        }
        else primitive(ir & 0x1fU);             /* primitive functions */
    }

    return;
}
/*
  Execute a Primitive Instruction
*/
static void primitive(U8 ic) {
    U16 x0, x1;

    switch (ic) {
    case 0:	/* DRP */
        parstk++;
        break;
    case 1:	/* DUP */
        x0 = *parstk;
        *(--parstk) = x0;
        break;
    case 2:	/* SWP */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(--parstk) = x1;
        *(--parstk) = x0;
        break;
    case 3:	/* >R */
        *(retstk++) = *(parstk++);
        break;
    case 4:	/* R> */
        *(--parstk) = *(--retstk);
        break;
    case 5:	/* + */
        x0 = *(parstk++);
        *parstk += x0;
        break;
    case 6:	/* - */
        x0 = *(parstk++);
        *parstk -= x0;
        break;
    case 7:	/* * */
        x0 = *(parstk++);
        *parstk *= x0;
        break;
    case 8:	/* / */
        x0 = *(parstk++);
        *parstk /= x0;
        break;
    case 9:	/* MOD */
        x0 = *(parstk++);
        *parstk %= x0;
        break;
    case 10:	/* AND */
        x0 = *(parstk++);
        *parstk &= x0;
        break;
    case 11:	/* OR */
        x0 = *(parstk++);
        *parstk |= x0;
        break;
    case 12:	/* XOR */
        x0 = *(parstk++);
        *parstk ^= x0;
        break;
    case 13:	/* = */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(--parstk) = (x0==x1);
        break;
    case 14:	/* < */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(--parstk) = (x0 < x1);
        break;
    case 15:	/* > */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(--parstk) = (x0 > x1);
        break;
    case 16:	/* <= */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(--parstk) = (x0 <= x1);
        break;
    case 17:	/* >= */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(--parstk) = (x0 >= x1);
        break;
    case 18:	/* <> */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(--parstk) = (x0 != x1);
        break;
    case 19:	/* NOT */
        *parstk = (*parstk==0);
        break;
    case 20:	/* @ */
        x0 = *(parstk++);
        x1 = *(V2R(dic, x0));
        x1 += *(V2R(dic, x0 + 1)) * 256U;
        *(--parstk) = x1;
        break;
    case 21:	/* @@ */
        x0 = *(parstk++);
        x1 = *(V2R(dic, x0));
        *(--parstk) = x1;
        break;
    case 22:	/* ! */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(V2R(dic, x1)) = x0 % 256U;
        *(V2R(dic, x1 + 1)) = x0 / 256U;
        break;
    case 23:	/* !! */
        x1 = *(parstk++);
        x0 = *(parstk++);
        *(V2R(dic, x1)) = (U8)x0;
        break;
    case 24:	/* . */
        putnum(*(parstk++));
        putchr(' ');
        break;
    case 25:	/* LOOP */
        (*(retstk - 2))++;
        x1 = *(retstk - 2);
        x0 = *(retstk - 1);
        *(--parstk) = (x0 <= x1);
        break;
    case 26:	/* RDROP2 */
        retstk -= 2;
        break;
    case 27:	/* I */
        *(--parstk) = *(retstk - 2);
        break;
    case 28:	/* P2R2 */
        *(retstk++) = *(parstk++);
        *(retstk++) = *(parstk++);
        break;
    }
    return;
}
/*
  Put a Number
*/
static void putnum(U16 num) {
    if (num / (U16)10 != 0) putnum(num / (U16)10);
    putchr((char)(num % (U16)10) + '0');
    return;
}

