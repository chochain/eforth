#include <stdlib.h>
#include <time.h>
#include "eforth.h"
//
// Forth VM control registers
//
XA  PC, IP;                     // PC (program counter), IP (instruction pointer)
U8  R, S;                       // return stack index, data stack index
S16 top;                        // ALU (i.e. cached top of stack value)
//
// Forth VM core storage
//
XA  rack[FORTH_RACK_SZ]   = { 0 };   	// return stack (assume FORTH_RACK_SZ is power of 2)
S16 stack[FORTH_STACK_SZ] = { 0 };   	// data stack   (assume FORTH_STACK_SZ is power of 2)
U8* cdata = 0;             			 	// linear byte array pointer
//
// data and return stack ops
//  R                   S
//  |                   |
// [R0, R1,..., S2, S1, S0] <- top
//
// Ting uses 256 and U8 for wrap-around control
//
#define BOOL(f)     ((f) ? TRUE : FALSE)
#define CELL(ip)    (*(XA*)(cdata+(XA)(ip)))
#define RACK(r)     (rack[(r)&(FORTH_RACK_SZ-1)])
#define STACK(s)    (stack[(s)&(FORTH_STACK_SZ-1)])
#define RPUSH(a)    (RACK(++R)=(XA)a)
#define RPOP()      (RACK(R--))
#define	PUSH(v)	    (STACK(++S)=top, top=(S16)(v))
#define	POP()		(top=STACK(S--))
//
// tracing instrumentation
//
int tTAB = 0, tCNT = 0;		// trace indentation and depth counters

void _trc_on()  	{ tCNT++; }
void _trc_off() 	{ tCNT -= tCNT ? 1 : 0; }
void _break_point(U32 pc, char *name)
{
	if (name && strcmp("EVAL", name)) return;

	int i=pc;
}
#define TRACE(s,v)  if(tCNT) PRINTF(s,v)
#define LOG(s)      TRACE(" %s", s)
#define TRACE_COLON() if (tCNT) {              \
    PRINTF("\n");                              \
	for (int i=0; i<tTAB; i++) PRINTF("  ");   \
	tTAB++;                                    \
	PRINTF(":");                               \
}
#define TRACE_EXIT()  if (tCNT) {              \
	PRINTF(" ;");                              \
	tTAB--;                                    \
}
void TRACE_WORD()
{
	if (!tCNT) return;

	static char buf[32];			    // allocated in memory instead of on C stack

	//PRINTF(" (pc=%x, ip=%x, R=%x)", PC, IP, RACK(R));
	U8 *a = &cdata[PC];		            // pointer to current code pointer
	if (!PC || *a==opEXIT) return;
	for (--a; (*a & 0x7f)>0x1f; a--);   // retract pointer to word name (ASCII range: 0x20~0x7f)

	int  len = (int)*a & 0x1f;          // Forth allows 31 char max
	memcpy(buf, a+1, len);
	buf[len] = '\0';

	PRINTF(" %x_%x_%x_%s", STACK(S-1), STACK(S), top, buf);

	_break_point(PC, buf);
}
//
// Forth Virtual Machine (primitive functions)
//
void _next()                // advance instruction pointer
{
	PC = CELL(IP);          // fetch instruction address
	IP += sizeof(XA);       // advance, except control flow instructions
}
void _nop() { _next(); }    // ( -- )
void _bye() { exit(0); }    // ( -- ) exit to OS
void _qrx()                 // ( -- c t|f) read a char from terminal input device
{
	PUSH(GETCHAR());
	if (top) PUSH(TRUE);
    _next();
}
void _txsto()               // (c -- ) send a char to console
{
#if !EXE_TRACE
	PRINTF("%c", (U8)top);
#else  // !EXE_TRACE
	switch (top) {
	case 0xa: PRINTF("<LF>");  break;
	case 0xd: PRINTF("<CR>");  break;
	case 0x8: PRINTF("<TAB>"); break;
	default:
		if (tCNT) PRINTF("<%c>", (U8)top);
		else      PRINTF("%c", (U8)top);
	}
#endif // !EXE_TRACE
	POP();
    _next();
}
void _dovar()               // ( -- a) return address of a variable
{
	TRACE(" %x", IP);       // fetch literal from data
	PUSH(IP);
	_next();
}
void _docon()               // ( -- n) push next token onto data stack as constant
{
	PUSH(CELL(++PC));       // fetch next byte (skip opDOCON opcode)
    _next();
}
void _dolit()               // ( -- w) push next token as an integer literal
{
	TRACE(" %d", CELL(IP)); // fetch literal from data
	PUSH(CELL(IP));	        // push onto data stack
	IP += CELLSZ;			// skip to next instruction
    _next();
}
void _enter()               // ( -- ) push instruction pointer onto return stack and pop, aka DOLIST by Dr. Ting
{
	TRACE_COLON();
	RPUSH(IP);              // keep return address
	IP = PC+sizeof(XA);     // advance to next instruction
    _next();
}
void __exit()               // ( -- ) terminate all token lists in colon words
{
	TRACE_EXIT();
	IP = RPOP();            // pop return address
    _next();
}
void _execu()               // (a -- ) take execution address from data stack and execute the token
{
	PC = (XA)top;           // fetch program counter
	POP();
}
void _donext()              // ( -- ) terminate a FOR-NEXT loop
{
	if (RACK(R)) {			// check if loop counter > 0
		RACK(R) -= 1;		// decrement loop counter
		IP = CELL(IP);		// branch back to FOR
	}
	else {
		IP += CELLSZ;		// skip to next instruction
		RPOP();				// pop off return stack
	}
    _next();
}
void _qbran()               // (f -- ) test top as a flag on data stack
{
	if (top) IP += CELLSZ;	// next instruction
    else     IP = CELL(IP);	// fetch branching target address
	POP();
    _next();
}
void _bran()                // ( -- ) branch to address following
{
	IP = CELL(IP);			// fetch branching target address
    _next();
}
void _store()               // (n a -- ) store into memory location from top of stack
{
	CELL(top) = STACK(S--);
	POP();
    _next();
}
void _at()                  // (a -- n) fetch from memory address onto top of stack
{
	top = (S16)CELL(top);
    _next();
}
void _cstor()               // (c b -- ) store a byte into memory location
{
	cdata[top] = (U8)STACK(S--);
	POP();
    _next();
}
void _cat()                 // (b -- n) fetch a byte from memory location
{
	top = (S16)cdata[top];
    _next();
}
void _rfrom()               // (n --) pop from return stack onto data stack (Ting comments different ???)
{
	PUSH(RPOP());
    _next();
}
void _rat()                 // (-- n) copy a number off the return stack and push onto data stack
{
	PUSH(RACK(R));
    _next();
}
void _tor()                 // (-- n) pop from data stack and push onto return stack
{
	RPUSH(top);
	POP();
    _next();
}
void _drop()                // (w -- ) drop top of stack item
{
	POP();
    _next();
}
void _dup()                 // (w -- w w) duplicate to of stack
{
	STACK(++S) = top;
    _next();
}
void _swap()                // (w1 w2 -- w2 w1) swap top two items on the data stack
{
	S16 tmp  = top;
	top = STACK(S);
	STACK(S) = tmp;
    _next();
}
void _over()                // (w1 w2 -- w1 w2 w1) copy second stack item to top
{
	PUSH(STACK(S - 1));
    _next();
}
void _zless()               // (n -- f) check whether top of stack is negative
{
	top = BOOL(top < 0);
    _next();
}
void _and()                 // (w w -- w) bitwise AND
{
	top &= STACK(S--);
    _next();
}
void _or()                  // (w w -- w) bitwise OR
{
	top |= STACK(S--);
    _next();
}
void _xor()                 // (w w -- w) bitwise XOR
{
	top ^= STACK(S--);
	_next();
}
void _uplus()               // (w w -- w c) add two numbers, return the sum and carry flag
{
	STACK(S) += top;
	top = (U16)STACK(S) < (U16)top;
    _next();
}
void _qdup()                // (w -- w w | 0) dup top of stack if it is not zero
{
	if (top) STACK(++S) = top;
    _next();
}
void _rot()                 // (w1 w2 w3 -- w2 w3 w1) rotate 3rd item to top
{
	S16 tmp = STACK(S - 1);
	STACK(S - 1) = STACK(S);
	STACK(S)     = top;
	top = tmp;
    _next();
}
void _ddrop()               // (w w --) drop top two items
{
	POP();
	POP();
    _next();
}
void _ddup()                // (w1 w2 -- w1 w2 w1 w2) duplicate top two items
{
	PUSH(STACK(S - 1));
	PUSH(STACK(S - 1));
    _next();
}
void _plus()                // (w w -- sum) add top two items
{
	top += STACK(S--);
    _next();
}
void _inver()               // (w -- w) one's complement
{
	top = -top - 1;
    _next();
}
void _negat()               // (n -- -n) two's complement
{
	top = 0 - top;
    _next();
}
void _dnega()               // (d -- -d) two's complement of top double
{
	top = -top - 1;         // _inver()
	RPUSH(top);             // _tor()
	POP();
	top = -top - 1;         // _inver()
	PUSH(1);
	STACK(S) += top;        // _uplus()
	top = (U16)STACK(S) < (U16)top;
	PUSH(RPOP());           // _rfrom()
	top += STACK(S--);      // _plus()
    _next();
}
void _sub()                 // (n1 n2 -- n1-n2) subtraction
{
	top = STACK(S--) - top;
    _next();
}
void _abs()                 // (n -- n) absolute value of n
{
	if (top < 0) top = -top;
    _next();
}
void _great()               // (n1 n2 -- t) true if n1>n2
{
	top = BOOL(STACK(S--) > top);
    _next();
}
void _less()                // (n1 n2 -- t) true if n1<n2
{
	top = BOOL(STACK(S--) < top);
    _next();
}
void _equal()               // (w w -- t) true if top two items are equal
{
	top = BOOL(STACK(S--)==top);
    _next();
}
void _uless()               // (u1 u2 -- t) unsigned compare top two items
{
	top = BOOL((U32)(STACK(S--)) < (U32)top);
    _next();
}
void _ummod()               // (udl udh u -- ur uq) unsigned divide of a double by single
{
	S32 d = (S32)top;       // CC: auto variable uses C stack 
	S32 m = (S32)STACK(S);
	S32 n = (S32)STACK(S - 1);
	n += m << (CELLSZ<<3);
	POP();
	top      = (U16)(n / d); // quotient
	STACK(S) = (U16)(n % d); // remainder
    _next();
}
void _msmod()               // (d n -- r q) signed floored divide of double by single
{
	S32 d = (S32)top;
	S32 m = (S32)STACK(S);
	S32 n = (S32)STACK(S - 1);
	n += m << 16;
	POP();
	top      = (S16)(n / d); // quotient
	STACK(S) = (S16)(n % d); // remainder
    _next();
}
void _slmod()               // (n1 n2 -- r q) signed devide, return mod and quotien
{
	if (top) {
		S16 tmp = STACK(S) / top;
		STACK(S) %= top;
		top = tmp;
	}
    _next();
}
void _mod()                 // (n n -- r) signed divide, returns mod
{
	top = (top) ? STACK(S--) % top : STACK(S--);
    _next();
}
void _slash()               // (n n - q) signed divide, return quotient
{
	top = (top) ? STACK(S--) / top : (STACK(S--), 0);
    _next();
}
void _umsta()               // (u1 u2 -- ud) unsigned multiply return double product
{
	U64 d = (U64)top;
	U64 m = (U64)STACK(S);
	m *= d;
	top      = (U32)(m >> 32);
	STACK(S) = (U32)m;
    _next();
}
void _star()                // (n n -- n) signed multiply, return single product
{
	top *= STACK(S--);
    _next();
}
void _mstar()               // (n1 n2 -- d) signed multiply, return double product
{
	S32 d = (S32)top;
	S32 m = (S32)STACK(S);
	m *= d;
	top      = (S16)(m >> 16);
	STACK(S) = (S16)m;
    _next();
}
void _ssmod()               // (n1 n2 n3 -- r q) n1*n2/n3, return mod and quotion
{
	S32 d = (S32)top;
	S32 m = (S32)STACK(S);
	S32 n = (S32)STACK(S - 1);
	n *= m;
	POP();
	top      = (S16)(n / d);
	STACK(S) = (S16)(n % d);
    _next();
}
void _stasl()               // (n1 n2 n3 -- q) n1*n2/n3 return quotient
{
	S32 d = (S32)top;
	S32 m = (S32)STACK(S);
	S32 n = (S32)STACK(S - 1);
	n *= m;
	POP();
    POP();
	top = (S16)(n / d);
    _next();
}
void _pick()                // (... +n -- ...w) copy nth stack item to top
{
	top = STACK(S - (U8)top);
    _next();
}
void _pstor()               // (n a -- ) add n to content at address a
{
	CELL(top) += (XA)STACK(S--);
    POP();
    _next();
}
void _dstor()               // (d a -- ) store the double to address a
{
	CELL(top+CELLSZ) = STACK(S--);
	CELL(top)        = STACK(S--);
	POP();
    _next();
}
void _dat()                 // (a -- d) fetch double from address a
{
	PUSH(CELL(top));
	top = CELL(top + CELLSZ);
    _next();
}
void _count()               // (b -- b+1 +n) count byte of a string and add 1 to byte address
{
	STACK(++S) = top + 1;
	top = cdata[top];
    _next();
}
void _max()                 // (n1 n2 -- n) return greater of two top stack items
{
	if (top < STACK(S)) POP();
	else (U8)S--;
    _next();
}
void _min()                 // (n1 n2 -- n) return smaller of two top stack items
{
	if (top < STACK(S)) (U8)S--;
	else POP();
    _next();
}
void _clock()
{
    PUSH((U32)clock());
    _next();
}

void(*prim[FORTH_PRIMITIVES])() = {
	/* case 0 */ _nop,
	/* case 1 */ _bye,
	/* case 2 */ _qrx,
	/* case 3 */ _txsto,
	/* case 4 */ _docon,
	/* case 5 */ _dolit,
	/* case 6 */ _enter,
	/* case 7 */ __exit,
	/* case 8 */ _execu,
	/* case 9 */ _donext,
	/* case 10 */ _qbran,
	/* case 11 */ _bran,
	/* case 12 */ _store,
	/* case 13 */ _at,
	/* case 14 */ _cstor,
	/* case 15 */ _cat,
	/* case 16  rpat, */  _trc_on,
	/* case 17  rpsto, */ _trc_off,
	/* case 18 */ _rfrom,
	/* case 19 */ _rat,
	/* case 20 */ _tor,
	/* case 21 spat, */  _clock,
	/* case 22 spsto, */ _nop,
	/* case 23 */ _drop,
	/* case 24 */ _dup,
	/* case 25 */ _swap,
	/* case 26 */ _over,
	/* case 27 */ _zless,
	/* case 28 */ _and,
	/* case 29 */ _or,
	/* case 30 */ _xor,
	/* case 31 */ _uplus,
	/* case 32 */ _next,
	/* case 33 */ _qdup,
	/* case 34 */ _rot,
	/* case 35 */ _ddrop,
	/* case 36 */ _ddup,
	/* case 37 */ _plus,
	/* case 38 */ _inver,
	/* case 39 */ _negat,
	/* case 40 */ _dnega,
	/* case 41 */ _sub,
	/* case 42 */ _abs,
	/* case 43 */ _equal,
	/* case 44 */ _uless,
	/* case 45 */ _less,
	/* case 46 */ _ummod,
	/* case 47 */ _msmod,
	/* case 48 */ _slmod,
	/* case 49 */ _mod,
	/* case 50 */ _slash,
	/* case 51 */ _umsta,
	/* case 52 */ _star,
	/* case 53 */ _mstar,
	/* case 54 */ _ssmod,
	/* case 55 */ _stasl,
	/* case 56 */ _pick,
	/* case 57 */ _pstor,
	/* case 58 */ _dstor,
	/* case 59 */ _dat,
	/* case 60 */ _count,
	/* case 61 */ _dovar,
	/* case 62 */ _max,
	/* case 63 */ _min,
};

void vm_init(U8 *rom) {
	cdata = rom;
	R  = S = PC = IP = top = 0;
}

void vm_run() {
	for (;;) {
	    TRACE_WORD();               // tracing stack and word name
		prim[cdata[PC]]();          // walk bytecode stream
	}
}
