#include <stdlib.h>
#include "eforth.h"
//
// Forth VM control registers
//
U32 P, IP, WP;                  // P (program counter), IP (intruction pointer), WP (parameter pointer)
U8  R, S;                       // return stack index, data stack index
S32 top;                        // ALU (i.e. cached stack top value)
//
// Forth VM core storage
//
U32 rack[256]  = { 0 };         // return stack
S32 stack[256] = { 0 };         // data stack
U8* byte       = 0;             // linear byte array pointer
//
// data and return stack ops
//
//                     |
// ..., S2, S1, S0 <- Top -> R0, R1, R2, ...
//
#define	POP()		(top = stack[S--])
#define	PUSH(v)	    { stack[++S] = top; top = (S32)(v); }
#define DATA(ip)    (*(U32*)(byte+(ip)))

void _break_point(char *name) {
	if (strcmp(name, "find")) return;

	int i=0;
}

char *_name(int p)  {
	static U8 *sEXIT = "EXIT";
	static U8 buf[32];			        // allocated in memory instead of on C stack

	U8 *a = &byte[p];		    		// pointer to current code pointer
	if (*a==opEXIT) return sEXIT;
	for (a-=4; (*a & 0x7f)>0x1f; a-=4); // retract pointer to word name (ASCII range: 0x20~0x7f)

	int  len = (int)*a & 0x1f;          // Forth allows 31 char max
	memcpy(buf, a+1, len);
	buf[len] = '\0';

	_break_point(buf);

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
#define TRACE_WORD(p) if (!tOFF) { printf(" %s_%04x", _name(p), p); }
#else // FORTH_TRACE
void    _trc_on()     {}
void    _trc_off()    {}
#define TRACE(s,v)
#define LOG(s)
#define TRACE_COLON()
#define TRACE_EXIT()
#define TRACE_WORD(p)
#endif // FORTH_TRACE
//
// Forth Virtual Machine (primitive functions)
//
#define NEXT()      {  	\
	P  = DATA(IP);      \
	WP = P + 4; 	    \
	IP += 4;       		\
	TRACE_WORD(P); 	    \
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
	S32 v = DATA(IP);		// fetch literal from data
	TRACE(" %d", v);
	PUSH(v);				// push onto data stack
	IP += 4;				// skip to next instruction
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
	P  = top;               // fetch instruction pointer
	WP = top + 4;           // parameter starts here
	POP();
}
void _donext()              // ( -- ) terminate a FOR-NEXT loop
{
	if (rack[R]) {			// check if loop counter > 0
		rack[R] -= 1;		// decrement loop counter
		IP = DATA(IP);		// branch back to FOR
	}
	else {
		IP += 4;			// skip to next instruction
		R--;				// pop off return stack
	}
    NEXT();
}
void _qbran()               // (f -- ) test top as a flag on data stack
{
	if (top) IP += 4;		// next instruction
    else     IP = DATA(IP);	// fetch branching address
	POP();
    NEXT();
}
void _bran()                // ( -- ) branch to address following
{
	IP = DATA(IP);			// fetch branching address
	NEXT();
}
void _store()               // (n a -- ) store into memory location from top of stack
{
	DATA(top) = stack[S--];
	POP();
}
void _at()                  // (a -- n) fetch from memory address onto top of stack
{
	top = DATA(top);
}
void _cstor()               // (c b -- ) store a byte into memory location
{
	byte[top] = (U8)stack[S--];
	POP();
}
void _cat()                 // (b -- n) fetch a byte from memory location
{
	top = (U32)byte[top];
}
void _rfrom()               // (n --) pop from data stack onto return stack
{
	PUSH(rack[R--]);
}
void _rat()                 // (-- n) copy a number off the return stack and push onto data stack
{
	PUSH(rack[R]);
}
void _tor()                 // (-- n) pop from data stack and push onto return stack
{
	rack[++R] = top;
	POP();
}
void _drop()                // (w -- ) drop top of stack item
{
	POP();
}
void _dup()                 // (w -- w w) duplicate to of stack
{
	stack[++S] = top;
}
void _swap()                // (w1 w2 -- w2 w1) swap top two items on the data stack
{
	WP  = top;
	top = stack[S];
	stack[S] = WP;
}
void _over()                // (w1 w2 -- w1 w2 w1) copy second stack item to top
{
	PUSH(stack[S - 1]);
}
void _zless()               // (n -- f) check whether top of stack is negative
{
	top = (top < 0) ? TRUE : FALSE;
}
void _and()                 // (w w -- w) bitwise AND
{
	top &= stack[S--];
}
void _or()                  // (w w -- w) bitwise OR
{
	top |= stack[S--];
}
void _xor()                 // (w w -- w) bitwise XOR
{
	top ^= stack[S--];
}
void _uplus()               // (w w -- w c) add two numbers, return the sum and carry flag
{
	stack[S] += top;
	top = (U32)stack[S] < (U32)top;
}
void _qdup()                // (w -- w w | 0) dup top of stack if it is not zero
{
	if (top) stack[++S] = top;
}
void _rot()                 // (w1 w2 w3 -- w2 w3 w1) rotate 3rd item to top
{
	WP = stack[S - 1];
	stack[S - 1] = stack[S];
	stack[S]     = top;
	top = WP;
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
	top += stack[S--];
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
	top = stack[S--] - top;
}
void _abs()                 // (n -- n) absolute value of n
{
	if (top < 0) top = -top;
}
void _great()               // (n1 n2 -- t) true if n1>n2
{
	top = (stack[S--] > top) ? TRUE : FALSE;
}
void _less()                // (n1 n2 -- t) true if n1<n2
{
	top = (stack[S--] < top) ? TRUE : FALSE;
}
void _equal()               // (w w -- t) true if top two items are equal
{
	top = (stack[S--]==top) ? TRUE : FALSE;
}
void _uless()               // (u1 u2 -- t) unsigned compare top two items
{
	top = ((U32)(stack[S--]) < (U32)top) ? TRUE : FALSE;
}
void _ummod()               // (udl udh u -- ur uq) unsigned divide of a double by single
{
	S64 d = (S64)top;
	S64 m = (S64)stack[S];
	S64 n = (S64)stack[S - 1];
	n += m << 32;
	POP();
	top = (U32)(n / d);
	stack[S] = (U32)(n % d);
}
void _msmod()               // (d n -- r q) signed floored divide of double by single
{
	S64 d = (S64)top;
	S64 m = (S64)stack[S];
	S64 n = (S64)stack[S - 1];
	n += m << 32;
	POP();
	top = (S32)(n / d);     // mod
	stack[S] = (U32)(n % d);// quotien
}
void _slmod()               // (n1 n2 -- r q) signed devide, return mod and quotien
{
	if (top) {
		WP  = stack[S] / top;
		stack[S] %= top;
		top = WP;
	}
}
void _mod()                 // (n n -- r) signed divide, returns mod
{
	top = (top) ? stack[S--] % top : stack[S--];
}
void _slash()               // (n n - q) signed divide, return quotient
{
	top = (top) ? stack[S--] / top : (stack[S--], 0);
}
void _umsta()               // (u1 u2 -- ud) unsigned multiply return double product
{
	U64 d = (U64)top;
	U64 m = (U64)stack[S];
	m *= d;
	top = (U32)(m >> 32);
	stack[S] = (U32)m;
}
void _star()                // (n n -- n) signed multiply, return single product
{
	top *= stack[S--];
}
void _mstar()               // (n1 n2 -- d) signed multiply, return double product
{
	S64 d = (S64)top;
	S64 m = (S64)stack[S];
	m *= d;
	top = (S32)(m >> 32);
	stack[S] = (S32)m;
}
void _ssmod()               // (n1 n2 n3 -- r q) n1*n2/n3, return mod and quotion
{
	S64 d = (S64)top;
	S64 m = (S64)stack[S];
	S64 n = (S64)stack[S - 1];
	n *= m;
	POP();
	top = (S32)(n / d);
	stack[S] = (S32)(n % d);
}
void _stasl()               // (n1 n2 n3 -- q) n1*n2/n3 return quotient
{
	S64 d = (S64)top;
	S64 m = (S64)stack[S];
	S64 n = (S64)stack[S - 1];
	n *= m;
	POP();
    POP();
	top = (S32)(n / d);
}
void _pick()                // (... +n -- ...w) copy nth stack item to top
{
	top = stack[S - (U8)top];
}
void _pstor()               // (n a -- ) add n to content at address a
{
	DATA(top) += stack[S--];
    POP();
}
void _dstor()               // (d a -- ) store the double to address a
{
	DATA(top+4) = stack[S--];
	DATA(top)   = stack[S--];
	POP();
}
void _dat()                 // (a -- d) fetch double from address a
{
	PUSH(DATA(top));
	top = DATA(top + 4);
}
void _count()               // (b -- b+1 +n) count byte of a string and add 1 to byte address
{
	stack[++S] = top + 1;
	top = byte[top];
}
void _max()                 // (n1 n2 -- n) return greater of two top stack items
{
	if (top < stack[S]) POP();
	else (U8)S--;
}
void _min()                 // (n1 n2 -- n) return smaller of two top stack items
{
	if (top < stack[S]) (U8)S--;
	else POP();
}

void(*primitives[64])() = {
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
	R  = S = P = IP = top = 0;
	WP = 4;
}

void vm_run() {
	for (;;) {
		primitives[byte[P++]]();            // walk bytecode stream
	}
}
