/*
  Tiny FORTH
  T. NAKAGAWA
  2004/07/04-10,7/29,8/5-6
*/
#include "tinyforth.h"
//
// allocate, initialize stack pointers
//
U16  stk[STK_SZ];
U16  *rsp  = &stk[0];            // return stack pointer
U16  *psp  = &stk[STK_SZ];       // parameter stack pointer
//
// allocate, initialize dictionary pointers
//
U8   dic[DIC_SZ];
U8   *dptr = dic;                // dictionary pointer
U8   *dmax = PTR(0xffff);        // end of dictionary
//
// IO functions ============================================================================
//
//  Put a Number
//
void putnum(U16 num) {
    if (num / (U16)10 != 0) putnum(num / (U16)10);
    putchr((char)(num % (U16)10) + '0');
}
//
// print a 8-bit hex
//
void puthex(U8 c) {
    U8 h = c>>4, l = c&0xf;
    putchr(h>9 ? 'A'+h-10 : '0'+h);
    putchr(l>9 ? 'A'+l-10 : '0'+l);
}
//
//  Put a message
//
void putmsg(char *msg) {
    while (*msg != '\0') putchr(*(msg++));
}
//
//  Get a Token
//
U8 *gettkn(void) {
    static U8 buf[BUF_SZ] = " ";	/*==" \0\0\0..." */
    U8 ptr;

    /* remove leading non-delimiters */
    while (*buf != ' ') {
        for (ptr = 0; ptr < BUF_SZ - 1; ptr++) buf[ptr] = buf[ptr + 1];
        buf[ptr] = '\0';
    }
    for (;;) {
        /* remove leading delimiters */
        while (*buf==' ') {
            for (ptr = 0; ptr < BUF_SZ - 1; ptr++) buf[ptr] = buf[ptr + 1];
            buf[ptr] = '\0';
        }
        if (*buf) {
            for (int i=0; i<4; i++) putchr(buf[i]<0x20 ? '_' : buf[i]);
            return buf;
        }
        
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
            else if (ptr < BUF_SZ - 1) { buf[ptr++] = c; }
            else {
                putchr('\b');
                putchr(' ');
                putchr('\b');
            }
        }
    }
}
//
// memory dumper with delimiter option
// 
void dump(U8 *p0, U8 *p1, U8 d)
{
	U16 n = (U16)(p0 - dic);
	puthex((U8)(n>>8)); puthex((U8)(n&0xff)); putchr(':');
	for (; p0<p1; n++, p0++) {
		if (d && (n&0x3)==0) putchr(d);
		puthex(*p0);
	}
	if (d) putchr('\n');
}

//
// Lookup the Keyword from the Dictionary
//
char lookup(U8 *key, U16 *adrs) {
    for (U8 *ptr = dmax; ptr != PTR(0xffffU); ptr = PTR(*ptr + *(ptr+1) * 256U)) {
        if (ptr[2]==key[0] && ptr[3]==key[1] && (ptr[3]==' ' || ptr[4]==key[2])) {
            *adrs = IDX(ptr);
            return 1;
        }
    }
    return 0;
}
//
// Find the Keyword in a List
//
char find(U8 *key, char *list, U8 *id) {
    for (U8 n=0, m=*(list++); n < m; n++, list += 3) {
        if (list[0]==key[0] && list[1]==key[1] && (key[1]==' ' || list[2]==key[2])) {
            *id = n;
            return 1;
        }
    }
    return 0;
}
//= Forth VM core ======================================================================
//
//  Compile Mode
//
void compile(void) {
    U8  *tkn, *p0 = dptr;
    U8  tmp8;
    U16 tmp16;

    /* get the identifier */
    tkn = gettkn();

    /* Write the header */
    tmp16 = IDX(dmax);
    dmax = dptr;
    *(dptr++) = tmp16 % 256U;
    *(dptr++) = tmp16 / 256U;
    *(dptr++) = tkn[0];
    *(dptr++) = tkn[1];
    *(dptr++) = (tkn[1] != ' ') ? tkn[2] : ' ';   // ensure 3-char name

    for (;;) {
        U8 *ptr;

        // dump token
        dump(p0, dptr, 0);

        tkn = gettkn();
        p0  = dptr;
        if (find(tkn, LST_COM, &tmp8)) {
            if (tmp8==0) {	/* ; */
                *(dptr++) = I_RET;
                break;
            }
            switch (tmp8) {
            case 1:	/* IF */
                *(rsp++) = IDX(dptr);
                *(dptr++) = PFX_CDJ;
                dptr++;
                break;
            case 2:	/* ELS */
                tmp16 = *(--rsp);
                ptr = PTR(tmp16);
                tmp8 = *(ptr);
                tmp16 = IDX(dptr + 2) - tmp16 + JMP_SGN;
                *(ptr++) = tmp8 | (tmp16 / 256U);
                *(ptr++) = tmp16 % 256U;
                *(rsp++) = IDX(dptr);
                *(dptr++) = PFX_UDJ;
                dptr++;
                break;
            case 3:	/* THN */
                tmp16 = *(--rsp);
                ptr = PTR(tmp16);
                tmp8 = *(ptr);
                tmp16 = IDX(dptr) - tmp16 + JMP_SGN;
                *(ptr++) = tmp8 | (tmp16 / 256U);
                *(ptr++) = tmp16 % 256U;
                break;
            case 4:	/* BGN */
                *(rsp++) = IDX(dptr);
                break;
            case 5:	/* END */
                tmp16 = *(--rsp) - IDX(dptr) + JMP_SGN;
                *(dptr++) = PFX_CDJ | (tmp16 / 256U);
                *(dptr++) = tmp16 % 256U;
                break;
            case 6:	/* WHL */
                *(rsp++) = IDX(dptr);
                dptr += 2;                     // allocate branch addr
                break;
            case 7:	/* RPT */
                tmp16 = *(--rsp);
                ptr = PTR(tmp16);
                tmp16 = IDX(dptr + 2) - tmp16 + JMP_SGN;
                *(ptr++) = PFX_CDJ | (tmp16 / 256U);
                *(ptr++) = tmp16 % 256U;
                tmp16 = *(--rsp) - IDX(dptr) + JMP_SGN;
                *(dptr++) = PFX_UDJ | (tmp16 / 256U);
                *(dptr++) = tmp16 % 256U;
                break;
            case 8:	/* DO */
                *(rsp++) = IDX(dptr+1);
                *(dptr++) = I_P2R2;
                break;
            case 9:	/* LOP */
                *(dptr++) = I_LOOP;
                tmp16 = *(--rsp) - IDX(dptr) + JMP_SGN;
                *(dptr++) = PFX_CDJ | (tmp16 / 256U);
                *(dptr++) = tmp16 % 256U;
                *(dptr++) = I_RDROP2;
                break;
            case 10:	/* I */
                *(dptr++) = I_I;
                break;
            }
        }
        else if (lookup(tkn, &tmp16)) {
            tmp16 += 2 + 3 - IDX(dptr) + JMP_SGN;
            *(dptr++) = PFX_CALL | (tmp16 / 256U);
            *(dptr++) = tmp16 % 256U;
        }
        else if (find(tkn, LST_PRM, &tmp8)) {
            *(dptr++) = PFX_PRM | tmp8;
        }
        else if (literal(tkn, &tmp16)) {
            if (tmp16 < 128U) {
                *(dptr++) = (U8)tmp16;
            } else {
                *(dptr++) = I_LIT;
                *(dptr++) = tmp16 % 256U;
                *(dptr++) = tmp16 / 256U;
            }
        }
        else /* error */ putmsg("!\n");
    }
    dump(dic, dptr, ' ');
}
//
//  Forget Words in the Dictionary
//
void forget(void) {
    U16 tmp16;
    U8 *ptr;

    /* get a word */
    if (!lookup(gettkn(), &tmp16)) {
        putmsg("??");
        return;
    }

    ptr = PTR(tmp16);
    dmax = PTR(*ptr + *(ptr+1) * 256U);
    dptr = ptr;

    return;
}
//
//  VARIABLE instruction
//
void variable(void) {
    U8 *tkn;
    U16 tmp16;

    /* get an identifier */
    tkn = gettkn();

    /* Write the header */
    tmp16 = IDX(dmax);
    dmax = dptr;
    *(dptr++) = tmp16 % 256U;
    *(dptr++) = tmp16 / 256U;
    *(dptr++) = tkn[0];
    *(dptr++) = tkn[1];
    *(dptr++) = (tkn[1] != ' ') ? tkn[2] : ' ';

    tmp16 = IDX(dptr + 2);
    if (tmp16 < 128U) {
        *(dptr++) = (U8)tmp16;
    } else {
        tmp16 = IDX(dptr + 4);
        *(dptr++) = I_LIT;
        *(dptr++) = tmp16 % 256U;
        *(dptr++) = tmp16 / 256U;
    }
    *(dptr++) = I_RET;
    *(dptr++) = 0;	/* data area */
    *(dptr++) = 0;	/* data area */

    return;
}
//
// Process a Literal
//
char literal(U8 *str, U16 *num) {
    if (*str=='$') {
        U16 n = 0;
        for (str++; *str != ' '; str++) {
            n *= 16;
            n += *str - (*str<='9' ? '0' : 'A' - 10);
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
//
//  Virtual Code Execution
//
void execute(U16 adrs) {
    U8 *pc;
    *(rsp++) = 0xffffU;

    for (pc = PTR(adrs); pc != PTR(0xffffU); ) {
        U8  ir;	/* instruction register */
        U16 n = (U16)(pc - dic);

        puthex((U8)(n>>8)); puthex((U8)(n&0xff)); putchr(':');
        ir = *(pc++);
        puthex(ir); putchr(' ');

        if ((ir & 0x80U)==0) {
            /* literal(0-127) */
            *(--psp) = ir;
        }
        else if (ir==I_LIT) {
            /* literal(128-65535) */
            U16 tmp16;
            tmp16 = *(pc++);
            tmp16 += *(pc++) * 256U;
            *(--psp) = tmp16;
        }
        else if (ir==I_RET) {
            /* RET: return */
            pc = PTR(*(--rsp));
        }
        else if ((ir & 0xe0U)==PFX_UDJ) {
            /* UDJ: unconditional direct jump */
            pc = PTR(IDX(pc-1) + (ir & 0x1fU) * 256U + *pc - JMP_SGN);  // JMP_SGN ensure backward jump
            putchr('\n');
        }
        else if ((ir & 0xe0U)==PFX_CDJ) {
            /* CDJ: conditional direct jump */
            pc = *(psp++)
                ? pc+1
                : PTR(IDX(pc-1) + (ir & 0x1fU) * 256U + *pc - JMP_SGN);
        }
        else if ((ir & 0xe0U)==PFX_CALL) {
            /* CALL: subroutine call */
            *(rsp++) = IDX(pc+1);
            pc = PTR(IDX(pc-1) + (ir & 0x1fU) * 256U + *pc - JMP_SGN);
        }
        else primitive(ir & 0x1fU);             /* primitive functions */
    }

    return;
}
//
//  Execute a Primitive Instruction
//
void primitive(U8 ic) {
    U16 x0, x1;

    switch (ic) {
    case 0:	/* DRP */
        psp++;
        break;
    case 1:	/* DUP */
        x0 = *psp;
        *(--psp) = x0;
        break;
    case 2:	/* SWP */
        x1 = *(psp++);
        x0 = *(psp++);
        *(--psp) = x1;
        *(--psp) = x0;
        break;
    case 3:	/* >R */
        *(rsp++) = *(psp++);
        break;
    case 4:	/* R> */
        *(--psp) = *(--rsp);
        break;
    case 5:	/* + */
        x0 = *(psp++);
        *psp += x0;
        break;
    case 6:	/* - */
        x0 = *(psp++);
        *psp -= x0;
        break;
    case 7:	/* * */
        x0 = *(psp++);
        *psp *= x0;
        break;
    case 8:	/* / */
        x0 = *(psp++);
        *psp /= x0;
        break;
    case 9:	/* MOD */
        x0 = *(psp++);
        *psp %= x0;
        break;
    case 10:	/* AND */
        x0 = *(psp++);
        *psp &= x0;
        break;
    case 11:	/* OR */
        x0 = *(psp++);
        *psp |= x0;
        break;
    case 12:	/* XOR */
        x0 = *(psp++);
        *psp ^= x0;
        break;
    case 13:	/* = */
        x1 = *(psp++);
        x0 = *(psp++);
        *(--psp) = (x0==x1);
        break;
    case 14:	/* < */
        x1 = *(psp++);
        x0 = *(psp++);
        *(--psp) = (x0 < x1);
        break;
    case 15:	/* > */
        x1 = *(psp++);
        x0 = *(psp++);
        *(--psp) = (x0 > x1);
        break;
    case 16:	/* <= */
        x1 = *(psp++);
        x0 = *(psp++);
        *(--psp) = (x0 <= x1);
        break;
    case 17:	/* >= */
        x1 = *(psp++);
        x0 = *(psp++);
        *(--psp) = (x0 >= x1);
        break;
    case 18:	/* <> */
        x1 = *(psp++);
        x0 = *(psp++);
        *(--psp) = (x0 != x1);
        break;
    case 19:	/* NOT */
        *psp = (*psp==0);
        break;
    case 20:	/* @ */
        x0 = *(psp++);
        x1 = *(PTR(x0));
        x1 += *(PTR(x0 + 1)) * 256U;
        *(--psp) = x1;
        break;
    case 21:	/* @@ */
        x0 = *(psp++);
        x1 = *(PTR(x0));
        *(--psp) = x1;
        break;
    case 22:	/* ! */
        x1 = *(psp++);
        x0 = *(psp++);
        *(PTR(x1)) = x0 % 256U;
        *(PTR(x1 + 1)) = x0 / 256U;
        break;
    case 23:	/* !! */
        x1 = *(psp++);
        x0 = *(psp++);
        *(PTR(x1)) = (U8)x0;
        break;
    case 24:	/* . */
        putnum(*(psp++));
        putchr(' ');
        break;
    case 25:	/* LOOP */
        (*(rsp - 2))++;
        x1 = *(rsp - 2);
        x0 = *(rsp - 1);
        *(--psp) = (x0 <= x1);
        putchr('\n');
        break;
    case 26:	/* RDROP2 */
        rsp -= 2;
        break;
    case 27:	/* I */
        *(--psp) = *(rsp - 2);
        break;
    case 28:	/* P2R2 */
        *(rsp++) = *(psp++);
        *(rsp++) = *(psp++);
        break;
    }
    return;
}

int main(void) {
    putmsg("Tiny FORTH\n");
    
    for (;;) {
        U8 tmp8;
        U16 tmp16;
        U8 *tkn;

        tkn = gettkn();

        /* keyword */
        if (find(tkn, LST_RUN, &tmp8)) {
            switch (tmp8) {
            case 0:	/* :   */ compile();     break;
            case 1:	/* VAR */ variable();    break;
            case 2:	/* FGT */ forget();      break;
            case 3: /* BYE */ exit(0);
            }
        }
        else if (lookup(tkn, &tmp16)) {
            execute(tmp16 + 2 + 3);
        }
        else if (find(tkn, LST_PRM, &tmp8)) {
            primitive(tmp8);
        }
        else if (literal(tkn, &tmp16)) {
            *(--psp) = tmp16;
        }
        else {
            /* error */
            putmsg("?\n");
            continue;
        }
        if (psp > &(stk[STK_SZ])) {
            putmsg("OVF\n");
            psp = &(stk[STK_SZ]);
        }
        else {
        	putchr('[');
            for (U16 *p=&stk[STK_SZ]-1; p>=psp; p--) {
                putchr(' '); putnum(*p);
            }
            putmsg(" ] OK ");
        }
    }
}
