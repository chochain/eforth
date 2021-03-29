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
//  put a 16-bit integer
//
void putnum(U16 n)
{
	U16 t = n/10;
    if (t != 0) putnum(t);
    putchr('0' + (n%10));
}
//
// print a 8-bit hex
//
void puthex(U8 c)
{
    U8 h = c>>4, l = c&0xf;
    putchr(h>9 ? 'A'+h-10 : '0'+h);
    putchr(l>9 ? 'A'+l-10 : '0'+l);
}
void putadr(U16 a)
{
	puthex((U8)(a>>8)); puthex((U8)(a&0xff)); putchr(':');
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
	U8 p;

    /* remove leading non-delimiters */
    while (*buf != ' ') {
        for (p=0; p<BUF_SZ-1; p++) buf[p] = buf[p+1];
        buf[p] = '\0';
    }
    for (;;) {
        /* remove leading delimiters */
        while (*buf==' ') {
            for (p=0; p<BUF_SZ-1; p++) buf[p] = buf[p+1];
            buf[p] = '\0';
        }
        if (*buf) {
            for (int i=0; i<4; i++) putchr(buf[i]<0x20 ? '_' : buf[i]);
            return buf;
        }
        for (p=0;;) {
            U8 c = getchr();
            if (c=='\n') {
                putchr('\n');
                buf[p] = ' ';
                break;
            }
            else if (c=='\b') {
                if (p==0) continue;
                buf[--p] = '\0';
                putchr(' ');
                putchr('\b');
            }
            else if (c <= 0x1fU)         {}
            else if (p < BUF_SZ-1) { buf[p++] = c; }
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
	putadr(n);
	for (; p0<p1; n++, p0++) {
		if (d && (n&0x3)==0) putchr(d);
		puthex(*p0);
	}
	if (d) putchr('\n');
}

//
// Lookup the Keyword from the Dictionary
//
U8 lookup(U8 *key, U16 *adr) {
    for (U8 *p=dmax; p != PTR(0xffff); p=PTR(*p + *(p+1) * 256)) {
        if (p[2]==key[0] && p[3]==key[1] && (p[3]==' ' || p[4]==key[2])) {
            *adr = IDX(p);
            return 1;
        }
    }
    return 0;
}
//
// Find the Keyword in a List
//
U8 find(U8 *key, char *lst, U8 *id) {
    for (U8 n=0, m=*(lst++); n < m; n++, lst += 3) {
        if (lst[0]==key[0] && lst[1]==key[1] && (key[1]==' ' || lst[2]==key[2])) {
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
        U8 *p;

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
                RPUSH(IDX(dptr));
                *(dptr++) = PFX_CDJ;
                dptr++;
                break;
            case 2:	/* ELS */
                tmp16 = RPOP();
                p     = PTR(tmp16);
                tmp8  = *(p);
                tmp16 = IDX(dptr + 2) - tmp16 + JMP_SGN;
                *(p++)    = tmp8 | (tmp16 / 256U);
                *(p++)    = tmp16 % 256U;
                RPUSH(IDX(dptr));
                *(dptr++) = PFX_UDJ;
                dptr++;
                break;
            case 3:	/* THN */
                tmp16  = RPOP();
                p      = PTR(tmp16);
                tmp8   = *(p);
                tmp16  = IDX(dptr) - tmp16 + JMP_SGN;
                *(p++) = tmp8 | (tmp16 / 256U);
                *(p++) = tmp16 % 256U;
                break;
            case 4:	/* BGN */
                RPUSH(IDX(dptr));
                break;
            case 5:	/* END */
                tmp16 = RPOP() - IDX(dptr) + JMP_SGN;
                *(dptr++) = PFX_CDJ | (tmp16 / 256U);
                *(dptr++) = tmp16 % 256U;
                break;
            case 6:	/* WHL */
                RPUSH(IDX(dptr));
                dptr += 2;                     // allocate branch addr
                break;
            case 7:	/* RPT */
                tmp16 = RPOP();
                p     = PTR(tmp16);
                tmp16 = IDX(dptr + 2) - tmp16 + JMP_SGN;
                *(p++)    = PFX_CDJ | (tmp16 / 256U);
                *(p++)    = tmp16 % 256U;
                tmp16 = RPOP() - IDX(dptr) + JMP_SGN;
                *(dptr++) = PFX_UDJ | (tmp16 / 256U);
                *(dptr++) = tmp16 % 256U;
                break;
            case 8:	/* DO */
                RPUSH(IDX(dptr+1));
                *(dptr++) = I_P2R2;
                break;
            case 9:	/* LOP */
                *(dptr++) = I_LOOP;
                tmp16 = RPOP() - IDX(dptr) + JMP_SGN;
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
    if (!lookup(gettkn(), &tmp16)) {
        putmsg("??");
        return;
    }
    U8 *p = PTR(tmp16);
    dmax  = PTR(*p + *(p+1) * 256U);
    dptr  = p;
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
    }
	else {
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
    RPUSH(0xffff);

    for (pc = PTR(adrs); pc != PTR(0xffffU); ) {
        U8  ir;	/* instruction register */
        U16 n = (U16)(pc - dic);

        putadr(n);
        ir = *(pc++);
        puthex(ir); putchr(' ');

        if ((ir & 0x80U)==0) {
            /* literal(0-127) */
            PUSH(ir);
        }
        else if (ir==I_LIT) {
            /* literal(128-65535) */
            U16 tmp16;
            tmp16 = *(pc++);
            tmp16 += *(pc++) * 256U;
            PUSH(tmp16);
        }
        else if (ir==I_RET) {
            /* RET: return */
            pc = PTR(RPOP());
        }
        else if ((ir & 0xe0U)==PFX_UDJ) {
            /* UDJ: unconditional direct jump */
            pc = PTR(IDX(pc-1) + (ir & 0x1fU) * 256U + *pc - JMP_SGN);  // JMP_SGN ensure backward jump
            putchr('\n');
        }
        else if ((ir & 0xe0U)==PFX_CDJ) {
            /* CDJ: conditional direct jump */
            pc = POP()
                ? pc+1
                : PTR(IDX(pc-1) + (ir & 0x1fU) * 256U + *pc - JMP_SGN);
        }
        else if ((ir & 0xe0U)==PFX_CALL) {
            /* CALL: subroutine call */
            RPUSH(IDX(pc+1));
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
        x0 = TOS;
        PUSH(x0);
        break;
    case 2:	/* SWP */
        x1 = POP();
        x0 = POP();
        PUSH(x1);
        PUSH(x0);
        break;
    case 3:	/* >R */
        RPUSH(POP());
        break;
    case 4:	/* R> */
        PUSH(RPOP());
        break;
    case 5:	/* + */
        x0 = POP();
        TOS += x0;
        break;
    case 6:	/* - */
        x0 = POP();
        TOS -= x0;
        break;
    case 7:	/* * */
        x0 = POP();
        TOS *= x0;
        break;
    case 8:	/* / */
        x0 = POP();
        TOS /= x0;
        break;
    case 9:	/* MOD */
        x0 = POP();
        TOS %= x0;
        break;
    case 10:	/* AND */
        x0 = POP();
        TOS &= x0;
        break;
    case 11:	/* OR */
        x0 = POP();
        TOS |= x0;
        break;
    case 12:	/* XOR */
        x0 = POP();
        TOS ^= x0;
        break;
    case 13:	/* = */
        x1 = POP();
        x0 = POP();
        PUSH(x0==x1);
        break;
    case 14:	/* < */
        x1 = POP();
        x0 = POP();
        PUSH(x0 < x1);
        break;
    case 15:	/* > */
        x1 = POP();
        x0 = POP();
        PUSH(x0 > x1);
        break;
    case 16:	/* <= */
        x1 = POP();
        x0 = POP();
        PUSH(x0 <= x1);
        break;
    case 17:	/* >= */
        x1 = POP();
        x0 = POP();
        PUSH(x0 >= x1);
        break;
    case 18:	/* <> */
        x1 = POP();
        x0 = POP();
        PUSH(x0 != x1);
        break;
    case 19:	/* NOT */
        TOS = (TOS==0);
        break;
    case 20:	/* @ */
        x0 = POP();
        x1 = *(PTR(x0));
        x1 += *(PTR(x0 + 1)) * 256U;
        PUSH(x1);
        break;
    case 21:	/* @@ */
        x0 = POP();
        x1 = *(PTR(x0));
        PUSH(x1);
        break;
    case 22:	/* ! */
        x1 = POP();
        x0 = POP();
        *(PTR(x1)) = x0 % 256U;
        *(PTR(x1 + 1)) = x0 / 256U;
        break;
    case 23:	/* !! */
        x1 = POP();
        x0 = POP();
        *(PTR(x1)) = (U8)x0;
        break;
    case 24:	/* . */
        putnum(POP());
        putchr(' ');
        break;
    case 25:	/* LOOP */
        (*(rsp - 2))++;
        x1 = *(rsp - 2);
        x0 = *(rsp - 1);
        PUSH(x0 <= x1);
        putchr('\n');
        break;
    case 26:	/* RDROP2 */
        rsp -= 2;
        break;
    case 27:	/* I */
        PUSH(*(rsp - 2));
        break;
    case 28:	/* P2R2 */
        RPUSH(POP());
        RPUSH(POP());
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
            PUSH(tmp16);
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
