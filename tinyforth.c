/*
 * Tiny FORTH
 * 2004-07 T. NAKAGAWA: original
 * 2004-08 T. NAKAGAWA: update
 * 2021-03 CC Lee: rewrite with comments; support Arduino Nano; add buffer handler
 * 2021-04 CC Lee: add multi-tasking
 */
#include "tinyforth.h"
//
// allocate, initialize dictionary pointers
//
U8   _dic[DIC_SZ];
U8   *dptr = _dic;                                 // dictionary pointer
U8   *dmax = PTR(0xffff);                          // end of dictionary
Task *tp;                                          // current task pointer
//
// tracing instrumentation
//
void d_chr(char c)     { putchr(c); }
void d_nib(U8 n)       { d_chr((n) + ((n)>9 ? 'A'-10 : '0')); }
void d_hex(U8 c)       { d_nib(c>>4); d_nib(c&0xf); }
void d_adr(U16 a)      { d_nib((U8)(a>>8)&0xff); d_hex((U8)(a&0xff)); d_chr(':'); }
//
// IO functions ============================================================================
//
void putmsg(char *msg) { while (*msg) putchr(*(msg++)); }
//
//  put a 16-bit integer
//
void putnum(S16 n) {
	if (n<0) { n=-n; putchr('-'); }
	U16 t = n/10;
    if (t) putnum(t);
    putchr('0' + (n - t*10));
}
//
// Process a Literal
//
U8 getnum(U8 *str, U16 *num) {
    U8  neg = 0;
    S16 n   = 0;
    if (*str=='$') {                          // handle Hex
        for (str++; *str != ' '; str++) {
            n *= 16;
            n += *str - (*str<='9' ? '0' : 'A' - 10);
        }
        *num = n;
        return 1;
    }
    if (*str=='-') { str++; neg=1; }          // negative number
    if ('0' <= *str && *str <= '9') {         // handle Decimal
        U16 n = 0;
        for (; *str != ' '; str++) {
            n *= 10;
            n += *str - '0';
        }
        *num = neg ? -n : n;
        return 1;
    }
    return 0;
}
//
// take input string from console (max TIB_SZ bytes)
//
void _console_input(U8 *tib)
{
    U8 *p = tib;
    for (;;) {
        U8 c = getchr();
        if (c=='\r' || c=='\n') {            // split on RETURN
            if (p > tib) {
                *p     = ' ';                // terminate input string
                *(p+1) = '\n';
                break;                       // skip empty token
            }
        }
        else if (c=='\b' && p > tib) {       // backspace
            *(--p) = ' ';
            putchr(' ');
            putchr('\b');
        }
        else if ((p - tib) >= (TIB_SZ-1)) {  // prevent buffer overrun
            putmsg("TIB\n");
            *p = '\n';
            break;
        }
        else *p++ = c;
    }
}    
//
//  Get a Token from input buffer
//
U8 *gettkn(void)
{
    static U8 tib[TIB_SZ], *bptr = tib;

    if (bptr==tib) _console_input(tib);    // buffer empty, read from console

    U8 *p0 = bptr;
    U8 sz  = 0;
    while (*bptr++!=' ') sz++;             // advance to next word
    while (*bptr==' ')   bptr++;           // skip blanks

    if (*bptr=='\r' || *bptr=='\n') bptr = tib;

#if ASM_TRACE
    // debug info
    d_chr('\n');
    for (U8 i=0; i<4; i++) {
    	d_chr(i<sz ? (*(p0+i)<0x20 ? '_' : *(p0+i)) : ' ');
    }
#endif // ASM_TRACE
    return p0;
}
//
// memory dumper with delimiter option
// 
void dump(U8 *p0, U8 *p1, U8 d)
{
	U16 n = IDX(p0);
	d_adr(n);
	for (; p0<p1; n++, p0++) {
		if (d && (n&0x3)==0)  d_chr(d);
		d_hex(*p0);
	}
}
//
// show  a section of dictionary memory
//
void showdic(U16 idx, U16 sz)            // idx: dictionary offset, sz: bytes
{
    U8 *p = PTR(idx & 0xfff0);           // 16-byte aligned
    sz &= 0xfff0;                        // 16-byte aligned
    putchr('\n');
    for (U8 i=0; i<sz; i+=0x20) {
        dump(p, p+0x20, ' ');
        putchr(' ');
        for (U8 j=0; j<0x20; j++, p++) {    // print and advance to next byte
            U8 c = *p & 0x7f;
            putchr((c==0x7f||c<0x20) ? '_' : c);
        }
        putchr('\n');
    }
}
//
// Lookup the Keyword from the Dictionary
//
U8 lookup(U8 *key, U16 *adr)
{
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
U8 find(U8 *key, const char *lst, U16 *id)
{
    for (U8 n=0, m=*(lst++); n < m; n++, lst += 3) {
        if (lst[0]==key[0] && lst[1]==key[1] && (key[1]==' ' || lst[2]==key[2])) {
            *id = (U16)n;
            return 1;
        }
    }
    return 0;
}
//
// token handler
//
U8 parse_token(U8 *tkn, U16 *rst, U8 run)
{
    if (find(tkn, run ? LST_CMD : LST_JMP, rst)) return TKN_EXE;  // run, compile mode
    if (lookup(tkn, rst))        return TKN_DIC; // search word dictionary addr(2), name(3)
    if (find(tkn, LST_EXT, rst)) return TKN_EXT; // search extended words
    if (find(tkn, LST_PRM, rst)) return TKN_PRM; // search primitives
    if (getnum(tkn, rst))        return TKN_NUM; // parse as number literal
    
    return TKN_ERR;
}
//  Forth Compiler =====================================================================
//
void _do_branch(U8 op)
{
    switch (op) {
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
// create word on dictionary
//
void compile(void)
{
    U8  *tkn = gettkn();
    U8  *p0  = dptr;
    U16 tmp  = IDX(dmax);
    
    dmax = dptr;
    SET16(dptr, tmp);         // link to previous word
    SETNM(dptr, tkn);         // 3-byte name

    for (; tkn;) {
        dump(p0, dptr, 0);

        tkn = gettkn();
        p0  = dptr;
        switch(parse_token(tkn, &tmp, 0)) {
        case TKN_EXE:
            if (tmp==0) {	/* ; */
                SET8(dptr, I_RET);
                tkn = NULL;
            }
            else _do_branch(tmp);
            break;
        case TKN_DIC:
        	JMPBCK(tmp+2+3, PFX_CALL);          // add found word addr + adr(2) + name(3)
            break;
        case TKN_EXT:
            SET8(dptr, I_EXT);                  // extended opcodes
            SET8(dptr, (U8)tmp);                // supports extra 256 opcodes
            break;
        case TKN_PRM:
        	SET8(dptr, PFX_PRM | (U8)tmp);      // add found primitive opcode
            break;
        case TKN_NUM:
            if (tmp < 128) {
                SET8(dptr, (U8)tmp);            // 1-byte literal
            }
			else {
                SET8(dptr, I_LIT);              // 3-byte literal
                SET16(dptr, tmp);
            }
            break;
        default: putmsg("!\n");                 // error
        }
    }
    // debug memory dump
    dump(dmax, dptr, ' ');
}
//= Forth VM core ======================================================================
//
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
//  Execute a Primitive Instruction
//
void primitive(U8 op) {
    switch (op) {
    case 0:  POP();                      break; // DRP
    case 1:  PUSH(TOS);                  break; // DUP
    case 2:  {                                  // SWP
        U16 x = TOS1;
        TOS1  = TOS;
        TOS   = x;
    } break;
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
    case 13: TOS = POP()==TOS;           break; // =
    case 14: TOS = POP()> TOS;           break; // <
    case 15: TOS = POP()< TOS;           break; // >
    case 16: TOS = POP()>=TOS;           break; // <=
    case 17: TOS = POP()<=TOS;           break; // >=
    case 18: TOS = POP()!=TOS;           break; // <>POP()
    case 19: TOS = (TOS==0);             break;	// NOT
    case 20: { U8 *p = PTR(POP()); PUSH(GET16(p));  } break; // @
    case 21: { U8 *p = PTR(POP()); SET16(p, POP()); } break; // !
    case 22: { U8 *p = PTR(POP()); PUSH((U16)*p);   } break; // C@
    case 23: { U8 *p = PTR(POP()); *p = (U8)POP();  } break; // C!
    case 24: putnum(POP()); putchr(' '); break; // .
    case 25: {	                                // LOOP
        (*(tp->rp-2))++;                  // counter+1
        PUSH(*(tp->rp-2) >= *(tp->rp-1)); // range check
#if EXE_TRACE
        d_chr('\n');                      // trace info
#endif // EXE_TRACE
    } break;
    case 26: RPOP(); RPOP();             break; // RD2
    case 27: PUSH(*(tp->rp-2));          break; // I
    case 28: RPUSH(POP()); RPUSH(POP()); break; // P2R2
    // the following 3 opcodes changes pc, so are done at one level up
    case 29: /* used by I_RET */         break;
    case 30: /* used by I_LIT */         break;
    case 31: /* used by I_EXT */         break;
    }
}
//
// show stack elements and OK prompt
//
void ok() {
    S16 *s0 = (S16*)&tp->s[STK_SZ];            // stack[0]
    if (tp->sp > s0) {                         // check stack overflow
        putmsg("OVF\n");
        tp->sp = s0;
    }
    else {                                      // dump stack then prompt OK
        putchr('[');
        for (S16 *p=s0-1; p >= tp->sp; p--) {
            putchr(' '); putnum(*p);
        }
        putmsg(" ] OK ");
    }
}
//
//  Virtual Code Execution (atomic)
//
void execute(U16 adr) {
    RPUSH(0xffff);
    for (tp->pc=PTR(adr); tp->pc!=PTR(0xffff); ) {
        U16 a  = IDX(tp->pc);                             // current execution address
        U8  ir = *(tp->pc++);                             // fetch instruction
        
#if EXE_TRACE
        d_adr(a); d_hex(ir); d_chr(' ');                  // trace addr:opcode
#endif //EXE_TRACE
        
        if ((ir & 0x80)==0) { PUSH(ir);               }   // 1-byte literal
        else if (ir==I_LIT) { PUSH(GET16(tp->pc)); tp->pc+=2; }   // 3-byte literal
        else if (ir==I_RET) { tp->pc = PTR(RPOP());   }   // RET
        else if (ir==I_EXT) { extended(*tp->pc++);    }   // EXT extended opcodes
        else {
            U8 op = ir & 0x1f;                            // opcode or top 5-bit of offset
            a = IDX(tp->pc-1) +
                ((U16)op<<8) + *tp->pc - JMP_BIT;         // JMP_BIT ensure 2's complement (for backward jump)
            switch (ir & 0xe0) {
            case PFX_UDJ:                                 // 0x80 unconditional jump
                tp->pc = PTR(a);                          // set jump target
#if EXE_TRACE
                d_chr('\n');                              // debug info
#endif // EXE_TRACE
                break;
            case PFX_CDJ:                                 // 0xa0 conditional jump
                tp->pc = POP() ? tp->pc+1 : PTR(a);       // next or target
                break;
            case PFX_CALL:                                // 0xd0 word call
                RPUSH(IDX(tp->pc+1));                     // keep next as return address
                tp->pc = PTR(a);
                break;
            case PFX_PRM:                                 // 0xe0 primitive
                primitive(op);                            // call primitve function with opcode
                break;
            }
        }
    }
}
//
// extended primitive words
//
void words()
{
    U8 n = 0;
    for (U8 *p=dmax; p!=PTR(0xffff); p=PTR(GET16(p)), n++) {
        if (n%8==0) d_chr('\n');
#if EXE_TRACE
        d_adr(IDX(p));                                        // optionally show address
#endif // EXE_TRACE
        d_chr(p[2]); d_chr(p[3]); d_chr(p[4]); d_chr(' ');    // 3-char name + space
    }
}
//
// block device (EEPROM) management
//
void save() {}
void load() {}
//
// extended dictionary words
//
void extended(U8 op)
{
    switch (op) {
    case 0:  PUSH(IDX(dptr));              break; // HRE
    case 1:  PUSH(IDX(dmax));              break; // CP
    case 2:  PUSH(TOS1);                   break; // OVR
    case 3:  TOS = -TOS;                   break; // INV
    case 4:  PUSH(POP()*sizeof(U16));      break; // CEL
    case 5:  dptr += POP();                break; // ALO
    case 6:  words();                      break; // WRD
    case 7:  save();                       break; // SAV
    case 8:  load();                       break; // LD
    }
}

void setup()
{
    tp     = (Task*)&_dic[DIC_SZ - sizeof(Task)];  // allocate user space
    tp->rp = &tp->s[0];                            // rstack[0]
    tp->sp = (S16*)&tp->s[STK_SZ];                 // stack[0]
    
	setvbuf(stdout, NULL, _IONBF, 0);		       // autoflush (turn STDOUT buffering off)
    putmsg("Tiny FORTH\n");
}

int main(int argc, char **argv) {
    setup();
    for (;;) {
        U8 *tkn = gettkn();                        // get token from console
        U16 tmp;
        switch (parse_token(tkn, &tmp, 1)) {
        case TKN_EXE:
            switch (tmp) {
            case 0:	compile();              break; // : (COLON)
            case 1:	variable();             break; // VAR
            case 2:	forget();               break; // FGT
			case 3: showdic(POP(), POP());  break; // DMP
            case 4: exit(0);                       // BYE
            }
            break;
        case TKN_DIC: execute(tmp + 2 + 3); break;
        case TKN_EXT: extended((U8)tmp);    break;
        case TKN_PRM: primitive((U8)tmp);   break;
        case TKN_NUM: PUSH(tmp);            break;
        default:
            putmsg("?\n");
            continue;
        }
        ok();  // stack check and prompt OK
    }
    return 0;
}
