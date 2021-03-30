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
    if (t) putnum(t);
    putchr('0' + (n - t*10));
}
//
// print a 8-bit hex
//
void puthex(U8 c)
{
    U8 n0 = c>>4, n1 = c&0xf;
    putchr(n0 + (n0>9 ? 'A'-10 : '0'));
    putchr(n1 + (n1>9 ? 'A'-10 : '0'));
}
void putadr(U16 a)
{
	puthex((U8)(a>>8)); puthex((U8)(a&0xff)); putchr(':');
}
//
//  Put a message
//
void putmsg(char *msg) {
    while (*msg) putchr(*(msg++));
}
//
//  Get a Token
//
U8 *gettkn(void) {
    static U8 buf[BUF_SZ] = " ";	/*==" \0\0\0..." */
	U8 p;

    while (*buf != ' ') {     // remove leading non-delimiters
        for (p=0; p<BUF_SZ-1; p++) buf[p] = buf[p+1];
        buf[p] = '\0';
    }
    for (;;) {                // remove leading delimiters
        while (*buf==' ') {
            for (p=0; p<BUF_SZ-1; p++) buf[p] = buf[p+1];
            buf[p] = '\0';
        }
        if (*buf) {           // debug output
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
	U16 n = IDX(p0);
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
    for (U8 *p=dmax; p != PTR(0xffff); p=PTR(GET16(p))) {
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
    U8  *tkn = gettkn();
    U8  *p0  = dptr;
    U16 tmp  = IDX(dmax);
    
    dmax = dptr;
    SET16(dptr, tmp);         // link to previous word
    SETNM(dptr, tkn);         // 3-byte name

    for (;;) {
        dump(p0, dptr, 0);

        tkn = gettkn();
        p0  = dptr;
        if (find(tkn, LST_COM, &tmp)) {
            if (tmp==0) {	/* ; */
                SET8(dptr, I_RET);
                break;
            }
            switch (tmp) {
            case 1:	/* IF */
                RPUSH(IDX(dptr));               // save current dptr A1
                JMP000(dptr, PFX_CDJ);          // alloc addr with jmp_flag
                break;
            case 2:	/* ELS */
                JMPSET(RPOP(), dptr+2);         // update A1 with next addr
                RPUSH(IDX(dptr));               // save current dptr A2
                JMP000(dptr, PFX_UDJ);          // alloc space with jmp_flag
                break;
            case 3:	/* THN */
                JMPSET(RPOP(), dptr);           // update A2 with current addr
                break;
            case 4:	/* BGN */
                RPUSH(IDX(dptr));               // save current dptr A1
                break;
            case 5:	/* UTL */
                JMPBCK(RPOP(), PFX_CDJ);        // conditional jump back to A1
                break;
            case 6:	/* WHL */
                RPUSH(IDX(dptr));               // save WHILE addr A2
				JMP000(dptr, PFX_CDJ);          // allocate branch addr A2 with jmp flag
                break;// add found primitive opcode
            case 7:	/* RPT */
                JMPSET(RPOP(), dptr+2);         // update A2 with next addr
                JMPBCK(RPOP(), PFX_UDJ);        // unconditional jump back to A1
                break;
            case 8:	/* DO */
                RPUSH(IDX(dptr+1));             // save current addr A1
                SET8(dptr, I_P2R2);
                break;
            case 9:	/* LOP */
                SET8(dptr, I_LOOP);
                JMPBCK(RPOP(), PFX_CDJ);        // conditionally jump back to A1
                SET8(dptr, I_RD2);
                break;
            case 10: /* I */
                SET8(dptr, I_I);
                break;
            }
        }
        else if (lookup(tkn, &tmp)) {           // scan dictionary
        	JMPBCK(2+3, PFX_CALL);              // add found word addr, adr(2), name(3)
        }
        else if (find(tkn, LST_PRM, &tmp)) {    // scan primitives
        	SET8(dptr, PFX_PRM | (U8)tmp);      // add found primitive opcode
        }
        else if (literal(tkn, &tmp)) {
            if (tmp < 128) {
                SET8(dptr, (U8)tmp);            // 1-byte literal
            }
			else {
                SET8(dptr, I_LIT);              // 3-byte literal
                SET16(dptr, tmp);
            }
        }
        else putmsg("!\n");                     // error
    }
    // debug memory dump
    dump(dmax, dptr, ' ');
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
    //
    // word found, rollback dptr
    //
    U8 *p = PTR(tmp);       // address of word
    dmax  = PTR(GET16(p));  
    dptr  = p;
}
//
//  VARIABLE instruction
//
void variable(void) {
    U8 *tkn = gettkn();        // get token
    U16 tmp = IDX(dmax);
    
    dmax = dptr;
    SET16(dptr, tmp);          // link addr of previous word
    SETNM(dptr, tkn);          // 3-byte variable name

    tmp = IDX(dptr + 2);       // next addr
    if (tmp < 128) {           // 1-byte immediate
        SET8(dptr, (U8)tmp);
    }
	else {
        tmp = IDX(dptr + 4);   // alloc LIT(1)+storage_addr(2)+RET(1)
        SET8(dptr, I_LIT);
        SET16(dptr, tmp);
    }
    SET8(dptr, I_RET);
    SET16(dptr, 0);	           // actual storage area
}
//
// Process a Literal
//
U8 literal(U8 *str, U16 *num) {
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
void execute(U16 adr) {
    RPUSH(0xffff);

    for (U8 *pc=PTR(adr); pc != PTR(0xffff); ) {
        U16 a  = IDX(pc);                                 // current program counter
        U8  ir = *(pc++);                                 // fetch instruction
        putadr(a); puthex(ir); putchr(' ');               // debug info

        if ((ir & 0x80)==0) { PUSH(ir);               }   // 1-byte literal
        else if (ir==I_LIT) { PUSH(GET16(pc)); pc+=2; }   // 3-byte literal
        else if (ir==I_RET) { pc = PTR(RPOP());       }   // RET
        else {
            U8 op = ir & 0x1f;                            // opcode or top 5-bit of offset
            a = IDX(pc-1) + ((U16)op<<8) + *pc - JMP_SGN; // JMP_SGN ensure 2's complement (for backward jump)
            switch (ir & 0xe0) {
            case PFX_UDJ:                                 // 0x80 unconditional jump
                pc = PTR(a);                              // set jump target
                putchr('\n');                             // debug info
                break;
            case PFX_CDJ:                                 // 0xa0 conditional jump
                pc = POP() ? pc+1 : PTR(a);               // next or target
                break;
            case PFX_CALL:                                // 0xd0 word call
                RPUSH(IDX(pc+1));                         // keep next as return address
                pc = PTR(a);
                break;
            case PFX_PRM:                                 // 0xe0 primitive
                primitive(op);                            // call primitve function with opcode
            }
        }
    }
}
//
//  Execute a Primitive Instruction
//
void primitive(U8 op) {
    switch (op) {
    case 0:  POP();                      break; // DRP
    case 1:  PUSH(TOS);                  break; // DUP
    case 2:  PUSH(POP()); PUSH(POP());   break;	// SWP
    case 3:  RPUSH(POP());               break; // >R
    case 4:  PUSH(RPOP());               break; // R>
    case 5:	 TOS += POP();               break; // +
    case 6:	 TOS -= POP();               break; // -
    case 7:	 TOS *= POP();               break; // *
    case 8:	 TOS /= POP();               break; // /
    case 9:	 TOS %= POP();               break; // MOD
    case 10: TOS &= POP();               break;	// AND
    case 11: TOS |= POP();               break;	// OR
    case 12: TOS ^= POP();               break; // XOR
    case 13: PUSH(POP() == POP());       break; // =
    case 14: PUSH(POP() >= POP());       break; // <
    case 15: PUSH(POP() <= POP());       break; // >
    case 16: PUSH(POP() >  POP());       break; // <=
    case 17: PUSH(POP() <  POP());       break; // >=
    case 18: PUSH(POP() != POP());       break; // <>
    case 19: TOS = (TOS==0);             break;	// NOT
    case 20: { U8 *p = PTR(POP()); PUSH(GET16(p));  } break; // @
    case 21: { U8 *p = PTR(POP()); SET16(p, POP()); } break; // !
    case 22: { U8 *p = PTR(POP()); PUSH((U16)*p);   } break; // C@
    case 23: { U8 *p = PTR(POP()); *p = (U8)POP();  } break; // C!
    case 24: putnum(POP()); putchr(' '); break; // .
    case 25: {	                                // LOOP
        (*(rsp-2))++;               // counter+1
        PUSH(*(rsp-2) >= *(rsp-1)); // range check
        putchr('\n');
    } break;
    case 26: RPOP(); RPOP();             break; // RD2
    case 27: PUSH(*(rsp-2));             break; // I
    case 28: RPUSH(POP()); RPUSH(POP()); break; // P2R2
    }
}

void ok() {
    if (psp > &(stk[STK_SZ])) {     // check stack overflow
        putmsg("OVF\n");
        psp = &(stk[STK_SZ]);
    }
    else {                          // stack dump before OK
        putchr('[');
        for (U16 *p=&stk[STK_SZ]-1; p>=psp; p--) {
            putchr(' '); putnum(*p);
        }
        putmsg(" ] OK ");
    }
}

int main(void) {
    putmsg("Tiny FORTH\n");
    for (;;) {
        U8 *tkn = gettkn();                    // get token from console
        U16 tmp;
        if (find(tkn, LST_RUN, &tmp)) {        // run mode
            switch (tmp) {
            case 0:	compile();     break;      // : (COLON)
            case 1:	variable();    break;      // VAR
            case 2:	forget();      break;      // FGT
            case 3: exit(0);                   // BYE
            }
        }
        else if (lookup(tkn, &tmp)) {          // search word dictionary addr(2), name(3)
        	execute(tmp + 2 + 3);
        }
        else if (find(tkn, LST_PRM, &tmp)) {   // search primitives
        	primitive((U8)tmp);
        }
        else if (literal(tkn, &tmp)) {         // handle numbers
        	PUSH(tmp);
        }
        else {                                                         // error
            putmsg("?\n");
            continue;
        }
        ok();  // stack check and prompt OK
    }
}
