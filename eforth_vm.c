#include <stdlib.h>
#include "eforth.h"
//
// Forth VM control registers
//
XA  PC, IP, WP;                 // PC (program counter), IP (intruction pointer), WP (parameter pointer)
U8  R, S;                       // return stack index, data stack index
XS  top;                        // ALU (i.e. cached stack top value)
//
// Forth VM core storage
//
XA  rack[FORTH_RACK_SZ]   = { 0 };   	// return stack
XS  stack[FORTH_STACK_SZ] = { 0 };   	// data stack
U8* byte = 0;             			 	// linear byte array pointer
//
// data and return stack ops
//              S            R
//              |            |
// ..., S2, S1, S0 <- top -> R0, R1, R2, ...
//
#define STACK(s)    (stack[(s)&(FORTH_STACK_SZ-1)])
#define BOOL(f)     ((f) ? TRUE : FALSE)
#define	POP()		(top=STACK(S--))
#define	PUSH(v)	    (STACK(++S)=top, top=(XS)(v))
#define DATA(ip)    (*(XA*)(byte+(XA)(ip)))

void _break_point(char *name) {
	if (strcmp(name, "find")) return;

	int i=0;
}

char *_name(int pc)  {
	static U8 buf[32];			        // allocated in memory instead of on C stack

	U8 *a = &byte[pc];		    		// pointer to current code pointer
	if (*a==opEXIT) return 0;
	for (a-=CELLSZ; (*a & 0x7f)>0x1f; a-=CELLSZ);  // retract pointer to word name (ASCII range: 0x20~0x7f)

	int  len = (int)*a & 0x1f;          // Forth allows 31 char max
	memcpy(buf, a+1, len);
	buf[len] = '\0';
	return buf;
}
//
// tracing instrumentation
//
#if FORTH_TRACE
int tOFF = 0, tTAB = 0;                 // trace indentation counter
void _trc_on()  	{ tOFF++; }
void _trc_off() 	{ tOFF--; }
#define TRACE(s,v)      { if(!tOFF) printf(s,v); }
#define LOG(s)          TRACE(" %s", s)
#define TRACE_COLON() if (!tOFF) {             \
    printf("\n");                              \
	for (int i=0; i<tTAB; i++) printf("  ");   \
	tTAB++;                                    \
	printf(":");                               \
}
#define TRACE_EXIT()  if (!tOFF) {             \
	printf(" ;");                              \
	tTAB--;                                    \
}
#define TRACE_WORD() if (!tOFF) {              \
	char *n = _name(PC);                       \
	if (n) {                                   \
		printf(" %x_%x_%s", STACK(S), top, n); \
		_break_point(n);                       \
	}                                          \
}
#else // FORTH_TRACE
void    _trc_on()     {}
void    _trc_off()    {}
#define TRACE(s,v)
#define LOG(s)
#define TRACE_COLON()
#define TRACE_EXIT()
#define TRACE_WORD()
#endif // FORTH_TRACE
//
// Forth Virtual Machine (primitive functions)
//
#define NEXT()      {  	 \
	PC = DATA(IP);       \
	WP = PC + CELLSZ; 	 \
	IP += CELLSZ;        \
	TRACE_WORD();        \
	}
void _nop() {}              // ( -- )
void _bye() { exit(0); }    // ( -- ) exit to OS
void _qrx()                 // ( -- c t|f) read a char from terminal input device
{
	PUSH(getchar());
	if (top) PUSH(TRUE);
}
void _txsto()               // (c -- ) send a char to console
{
#if !FORTH_TRACE
	putchar((U8)top);
#else  // !FORTH_TRACE
	switch (top) {
	case 0xa: printf("<LF>");  break;
	case 0xd: printf("<CR>");  break;
	case 0x8: printf("<TAB>"); break;
	default:
		if (tOFF) putchar((U8)top);
		else      printf("<%c>", (U8)top);
	}
#endif // !FORTH_TRACE
	POP();
}
void _next()                // advance instruction pointer
{
	NEXT();                 // step into next opcode (in-line)
}
void _dovar()               // ( -- a) return address of a variable
{
	PUSH(WP);
}
void _docon()               // ( -- n) push next token onto data stack as constant
{
	PUSH(DATA(WP));
}
void _dolit()               // ( -- w) push next token as an integer literal
{
	XS v = (XS)DATA(IP);	// fetch literal from data
	TRACE(" %d", v);
	PUSH(v);				// push onto data stack
	IP += CELLSZ;			// skip to next instruction
    NEXT();
}
void _dolist()              // ( -- ) push instruction pointer onto return stack and pop
{
	TRACE_COLON();
	rack[++R] = IP;
	IP = WP;
    NEXT();
}
void __exit()                // ( -- ) terminate all token lists in colon words
{
	TRACE_EXIT();
	IP = rack[R--];
    NEXT();
}
void _execu()               // (a -- ) take execution address from data stack and execute the token
{
	PC = (XA)top;           // fetch instruction pointer
	WP = (XA)top + CELLSZ;  // parameter starts here
	POP();
}
void _donext()              // ( -- ) terminate a FOR-NEXT loop
{
	if (rack[R]) {			// check if loop counter > 0
		rack[R] -= 1;		// decrement loop counter
		IP = DATA(IP);		// branch back to FOR
	}
	else {
		IP += CELLSZ;		// skip to next instruction
		R--;				// pop off return stack
	}
    NEXT();
}
void _qbran()               // (f -- ) test top as a flag on data stack
{
	if (top) IP += CELLSZ;	// next instruction
    else     IP = DATA(IP);	// fetch branching target address
	POP();
    NEXT();
}
void _bran()                // ( -- ) branch to address following
{
	IP = DATA(IP);			// fetch branching target address
	NEXT();
}
void _store()               // (n a -- ) store into memory location from top of stack
{
	DATA(top) = STACK(S--);
	POP();
}
void _at()                  // (a -- n) fetch from memory address onto top of stack
{
	top = (XS)DATA(top);
}
void _cstor()               // (c b -- ) store a byte into memory location
{
	byte[top] = (U8)STACK(S--);
	POP();
}
void _cat()                 // (b -- n) fetch a byte from memory location
{
	top = (XS)byte[top];
}
void _rfrom()               // (n --) pop from return stack onto data stack (Ting comments different ???)
{
	PUSH(rack[R--]);
}
void _rat()                 // (-- n) copy a number off the return stack and push onto data stack
{
	PUSH(rack[R]);
}
void _tor()                 // (-- n) pop from data stack and push onto return stack
{
	rack[++R] = (XA)top;
	POP();
}
void _drop()                // (w -- ) drop top of stack item
{
	POP();
}
void _dup()                 // (w -- w w) duplicate to of stack
{
	STACK(++S) = top;
}
void _swap()                // (w1 w2 -- w2 w1) swap top two items on the data stack
{
	WP  = (XA)top;          // use WP as temp
	top = STACK(S);
	STACK(S) = (XS)WP;
}
void _over()                // (w1 w2 -- w1 w2 w1) copy second stack item to top
{
	PUSH(STACK(S - 1));
}
void _zless()               // (n -- f) check whether top of stack is negative
{
	top = BOOL(top < 0);
}
void _and()                 // (w w -- w) bitwise AND
{
	top &= STACK(S--);
}
void _or()                  // (w w -- w) bitwise OR
{
	top |= STACK(S--);
}
void _xor()                 // (w w -- w) bitwise XOR
{
	top ^= STACK(S--);
}
void _uplus()               // (w w -- w c) add two numbers, return the sum and carry flag
{
	STACK(S) += top;
	top = (XU)STACK(S) < (XU)top;
}
void _qdup()                // (w -- w w | 0) dup top of stack if it is not zero
{
	if (top) STACK(++S) = top;
}
void _rot()                 // (w1 w2 w3 -- w2 w3 w1) rotate 3rd item to top
{
	WP = (XA)STACK(S - 1);
	STACK(S - 1) = STACK(S);
	STACK(S)     = top;
	top = (XS)WP;
}
void _ddrop()               // (w w --) drop top two items
{
	_drop();
	_drop();
}
void _ddup()                // (w1 w2 -- w1 w2 w1 w2) duplicate top two items
{
	_over();
	_over();
}
void _plus()                // (w w -- sum) add top two items
{
	top += STACK(S--);
}
void _inver()               // (w -- w) one's complement
{
	top = -top - 1;
}
void _negat()               // (n -- -n) two's complement
{
	top = 0 - top;
}
void _dnega()               // (d -- -d) two's complement of top double
{
	_inver();
	_tor();
	_inver();
	PUSH(1);
	_uplus();
	_rfrom();
	_plus();
}
void _sub()                 // (n1 n2 -- n1-n2) subtraction
{
	top = STACK(S--) - top;
}
void _abs()                 // (n -- n) absolute value of n
{
	if (top < 0) top = -top;
}
void _great()               // (n1 n2 -- t) true if n1>n2
{
	top = BOOL(STACK(S--) > top);
}
void _less()                // (n1 n2 -- t) true if n1<n2
{
	top = BOOL(STACK(S--) < top);
}
void _equal()               // (w w -- t) true if top two items are equal
{
	top = BOOL(STACK(S--)==top);
}
void _uless()               // (u1 u2 -- t) unsigned compare top two items
{
	top = BOOL((XU)(STACK(S--)) < (XU)top);
}
void _ummod()               // (udl udh u -- ur uq) unsigned divide of a double by single
{
	S64 d = (S64)top;
	S64 m = (S64)STACK(S);
	S64 n = (S64)STACK(S - 1);
	n += m << (CELLSZ<<3);
	POP();
	top = (XU)(n / d);
	STACK(S) = (XU)(n%d);
}
void _msmod()               // (d n -- r q) signed floored divide of double by single
{
	S64 d = (S64)top;
	S64 m = (S64)STACK(S);
	S64 n = (S64)STACK(S - 1);
	n += m << 32;
	POP();
	top = (XS)(n / d);     // mod
	STACK(S) = (XS)(n%d);  // quotion
}
void _slmod()               // (n1 n2 -- r q) signed devide, return mod and quotien
{
	if (top) {
		WP  = STACK(S) / top;
		STACK(S) %= top;
		top = WP;
	}
}
void _mod()                 // (n n -- r) signed divide, returns mod
{
	top = (top) ? STACK(S--) % top : STACK(S--);
}
void _slash()               // (n n - q) signed divide, return quotient
{
	top = (top) ? STACK(S--) / top : (STACK(S--), 0);
}
void _umsta()               // (u1 u2 -- ud) unsigned multiply return double product
{
	U64 d = (U64)top;
	U64 m = (U64)STACK(S);
	m *= d;
	top = (XU)(m >> 32);
	STACK(S) = (XU)m;
}
void _star()                // (n n -- n) signed multiply, return single product
{
	top *= STACK(S--);
}
void _mstar()               // (n1 n2 -- d) signed multiply, return double product
{
	S64 d = (S64)top;
	S64 m = (S64)STACK(S);
	m *= d;
	top = (XS)(m >> 32);
	STACK(S) = (XS)m;
}
void _ssmod()               // (n1 n2 n3 -- r q) n1*n2/n3, return mod and quotion
{
	S64 d = (S64)top;
	S64 m = (S64)STACK(S);
	S64 n = (S64)STACK(S - 1);
	n *= m;
	POP();
	top = (XS)(n / d);
	STACK(S) = (XS)(n % d);
}
void _stasl()               // (n1 n2 n3 -- q) n1*n2/n3 return quotient
{
	S64 d = (S64)top;
	S64 m = (S64)STACK(S);
	S64 n = (S64)STACK(S - 1);
	n *= m;
	POP();
    POP();
	top = (XS)(n / d);
}
void _pick()                // (... +n -- ...w) copy nth stack item to top
{
	top = STACK(S - (U8)top);
}
void _pstor()               // (n a -- ) add n to content at address a
{
	DATA(top) += (XA)STACK(S--);
    POP();
}
void _dstor()               // (d a -- ) store the double to address a
{
	DATA(top+4) = STACK(S--);
	DATA(top)   = STACK(S--);
	POP();
}
void _dat()                 // (a -- d) fetch double from address a
{
	PUSH(DATA(top));
	top = DATA(top + CELLSZ);
}
void _count()               // (b -- b+1 +n) count byte of a string and add 1 to byte address
{
	STACK(++S) = top + 1;
	top = byte[top];
}
void _max()                 // (n1 n2 -- n) return greater of two top stack items
{
	if (top < STACK(S)) POP();
	else (U8)S--;
}
void _min()                 // (n1 n2 -- n) return smaller of two top stack items
{
	if (top < STACK(S)) (U8)S--;
	else POP();
}

void(*prim[FORTH_PRIMITIVES])() = {
	/* case 0 */ _nop,
	/* case 1 */ _bye,
	/* case 2 */ _qrx,
	/* case 3 */ _txsto,
	/* case 4 */ _docon,
	/* case 5 */ _dolit,
	/* case 6 */ _dolist,
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
	/* case 21 spat, */  _nop,
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
	byte = rom;
	R  = S = PC = IP = top = 0;
	WP = CELLSZ;
}

void vm_run() {
	for (;;) {
		prim[byte[PC++]]();            // walk bytecode stream
	}
}
