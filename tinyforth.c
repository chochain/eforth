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
U8 find(U8 *key, char *lst, U16 *id) {
    for (U8 n=0, m=*(lst++); n < m; n++, lst += 3) {
        if (lst[0]==key[0] && lst[1]==key[1] && (key[1]==' ' || lst[2]==key[2])) {
            *id = (U16)n;
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
    U16 tmp;

    /* get the identifier */
    tkn = gettkn();

    /* Write the header */
    tmp  = IDX(dmax);
    dmax = dptr;
    ADDU16(tmp);
	ADDU8(tkn[0]);
    ADDU8(tkn[1]);
	ADDU8((tkn[1] != ' ') ? tkn[2] : ' ');   // ensure 3-char name

    for (;;) {
        U8 *p, f8;

        // dump token
        dump(p0, dptr, 0);

        tkn = gettkn();
        p0  = dptr;
        if (find(tkn, LST_COM, &tmp)) {
            if (tmp==0) {	/* ; */
                ADDU8(I_RET);
                break;
            }
            switch (tmp) {
            case 1:	/* IF */
                RPUSH(IDX(dptr));
                ADDU8(PFX_CDJ);
                ADDU8(0);
                break;
            case 2:	/* ELS */
                tmp   = RPOP();
                p     = PTR(tmp);
                f8    = *(p);
                tmp   = IDX(dptr + 2) - tmp + JMP_SGN;
                *(p++)    = f8 | (tmp / 256U);
                *(p++)    = tmp % 256U;
                RPUSH(IDX(dptr));
                ADDU8(PFX_UDJ);
                ADDU8(0);
                break;
            case 3:	/* THN */
                tmp  = RPOP();
                p    = PTR(tmp);
                f8   = *(p);
                tmp  = IDX(dptr) - tmp + JMP_SGN;
                *(p++) = f8 | (tmp / 256U);
                *(p++) = tmp % 256U;
                break;
            case 4:	/* BGN */
                RPUSH(IDX(dptr));
                break;
            case 5:	/* END */
                tmp = RPOP() - IDX(dptr) + JMP_SGN;
                ADDU8(PFX_CDJ | (tmp / 256U));
                ADDU8(tmp % 256U);
                break;
            case 6:	/* WHL */
                RPUSH(IDX(dptr));
                dptr += 2;                     // allocate branch addr
                break;
            case 7:	/* RPT */
                tmp = RPOP();
                p   = PTR(tmp);
                tmp = IDX(dptr + 2) - tmp + JMP_SGN;
                *(p++)    = PFX_CDJ | (tmp / 256U);
                *(p++)    = tmp % 256U;
                tmp = RPOP() - IDX(dptr) + JMP_SGN;
                ADDU8(PFX_UDJ | (tmp / 256U));
                ADDU8(tmp % 256U);
                break;
            case 8:	/* DO */
                RPUSH(IDX(dptr+1));
                ADDU8(I_P2R2);
                break;
            case 9:	/* LOP */
                ADDU8(I_LOOP);
                tmp = RPOP() - IDX(dptr) + JMP_SGN;
                ADDU8(PFX_CDJ | (tmp / 256U));
                ADDU8(tmp % 256U);
                ADDU8(I_RDROP2);
                break;
            case 10:	/* I */
                ADDU8(I_I);
                break;
            }
        }
        else if (lookup(tkn, &tmp)) {
            tmp += 2 + 3 - IDX(dptr) + JMP_SGN;
            ADDU8(PFX_CALL | (tmp / 256U));
            ADDU8(tmp % 256U);
        }
        else if (find(tkn, LST_PRM, &tmp)) {
            ADDU8(PFX_PRM | (U8)tmp);
        }
        else if (literal(tkn, &tmp)) {
            if (tmp < 128U) {
                ADDU8((U8)tmp);
            }
			else {
                ADDU8(I_LIT);
                ADDU8(tmp % 256U);
                ADDU8(tmp / 256U);
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
    U16 tmp;
    if (!lookup(gettkn(), &tmp)) {
        putmsg("??");
        return;
    }
    U8 *p = PTR(tmp);
    dmax  = PTR(*p + *(p+1) * 256U);
    dptr  = p;
}
//
//  VARIABLE instruction
//
void variable(void) {
    /* get an identifier */
    U8 *tkn = gettkn();

    /* Write the header */
    U16 tmp = IDX(dmax);
    dmax = dptr;
    ADDU16(tmp);
    ADDU8(tkn[0]);
	ADDU8(tkn[1]);
    ADDU8((tkn[1] != ' ') ? tkn[2] : ' ');

    tmp = IDX(dptr + 2);
    if (tmp < 128U) {
        ADDU8((U8)tmp);
    }
	else {
        tmp = IDX(dptr + 4);
        ADDU8(I_LIT);
        ADDU16(tmp);
    }
    ADDU8(I_RET);
    ADDU16(0);	/* data area */
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
            U16 tmp = *(pc++);
            tmp += *(pc++) * 256U;
            PUSH(tmp);
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
        U8 *tkn = gettkn();

        U16 tmp;
        /* keyword */
        if (find(tkn, LST_RUN, &tmp)) {
            switch (tmp) {
            case 0:	/* :   */ compile();     break;
            case 1:	/* VAR */ variable();    break;
            case 2:	/* FGT */ forget();      break;
            case 3: /* BYE */ exit(0);
            }
        }
        else if (lookup(tkn, &tmp)) {
            execute(tmp + 2 + 3);
        }
        else if (find(tkn, LST_PRM, &tmp)) {
            primitive((U8)tmp);
        }
        else if (literal(tkn, &tmp)) {
            PUSH(tmp);
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
