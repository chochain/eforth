#include <stdlib.h>
#include "eforth.h"
//
// Forth VM control registers
//
U8  R=0, S=0;                   // return stack index, data stack index
U32 P, IP, WP;                  // P (program counter), IP (intruction pointer), WP (parameter pointer)
S32 top = 0;                    // stack top value (cache)
//
// Forth VM core storage
//
U32 rack[256]   = { 0 };        // return stack
S32 stack[256]  = { 0 };        // data stack
U8* byte        = 0;            // linear byte array pointer
//
// data and return stack ops
//
#define	_pop()		(top = stack[(U8)S--])
#define	_push(v)	{ stack[(U8)++S] = top; top = (S32)(v); }
#define DATA(ip)    (*(U32*)(byte+(ip)))
//
// tracing instrumentation
//
#if FORTH_TRACE
int tOFF = 0, tTAB = 0;                 // trace indentation counter
void _trc_on()  	{ tOFF++; }
void _trc_off() 	{ tOFF--; }
void TRACE_WORD(int p)  {
	if (tOFF) return;

	U8 *a = &byte[p];		    		// pointer to current code pointer
	if (*a==opEXIT) return;
	for (a-=4; (*a & 0x7f)>31; a-=4);	// retract pointer to word name

	int  len = (int)*a & 0x1f;
	char buf[64];
	memcpy(buf, a+1, len);
	buf[len] = '\0';
	printf("_%s", buf);
}
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
#else // FORTH_TRACE
#define TRACE(s,v)
#define LOG(s)
#define TRACE_COLON()
#define TRACE_EXIT()
void    _trc_on()     {}
void    _trc_off()    {}
void    TRACE_WORD(int p) {}
#endif // FORTH_TRACE
//
// Forth Virtual Machine (primitive functions)
//
void _nop() {}              // ( -- )
void _bye() { exit(0); }    // ( -- ) exit to OS
void _qrx()                 // ( -- c t|f) read a char from terminal input device
{
	_push(getchar());
	if (top) _push(TRUE);
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
	_pop();
}
void _next()                // advance instruction pointer
{
	P  = DATA(IP);	        // fetch next address
	TRACE_WORD(P);
	WP = P + 4;             // parameter pointer (used optionally)
	IP += 4;
}
void _dovar()               // ( -- a) return address of a variable
{
	_push(WP);
}
void _docon()               // ( -- n) push next token onto data stack as constant
{
	_push(DATA(WP));
}
void _dolit()               // ( -- w) push next token as an integer literal
{
	S32 v = DATA(IP);
	TRACE(" %d", v);
	_push(v);
	IP += 4;
    _next();
}
void _dolist()              // ( -- ) push instruction pointer onto return stack and pop
{
	TRACE_COLON();
	rack[(U8)++R] = IP;
	IP = WP;
    _next();
}
void __exit()                // ( -- ) terminate all token lists in colon words
{
	TRACE_EXIT();
	IP = rack[(U8)R--];
    _next();
}
void _execu()               // (a -- ) take execution address from data stack and execute the token
{
	P  = top;
	WP = P + 4;
	_pop();
}
void _donext()              // ( -- ) terminate a FOR-NEXT loop
{
	if (rack[(U8)R]) {
		rack[(U8)R] -= 1;
		IP = DATA(IP);
	}
	else {
		IP += 4;
		R--;
	}
    _next();
}
void _qbran()               // (f -- ) test top as a flag on data stack
{
	if (top) IP += 4;
    else     IP = DATA(IP);
	_pop();
    _next();
}
void _bran()                // ( -- ) branch to address following
{
	IP = DATA(IP);
	_next();
}
void _store()               // (n a -- ) store into memory location from top of stack
{
	DATA(top) = stack[(U8)S--];
	_pop();
}
void _at()                  // (a -- n) fetch from memory address onto top of stack
{
	top = DATA(top);
}
void _cstor()               // (c b -- ) store a byte into memory location
{
	byte[top] = (U8)stack[(U8)S--];
	_pop();
}
void _cat()                 // (b -- n) fetch a byte from memory location
{
	top = (U32)byte[top];
}
void _rfrom()               // (n --) pop from data stack onto return stack
{
	_push(rack[(U8)R--]);
}
void _rat()                 // (-- n) copy a number off the return stack and push onto data stack
{
	_push(rack[(U8)R]);
}
void _tor()                 // (-- n) pop from data stack and push onto return stack
{
	rack[(U8)++R] = top;
	_pop();
}
void _drop()                // (w -- ) drop top of stack item
{
	_pop();
}
void _dup()                 // (w -- w w) duplicate to of stack
{
	stack[(U8)++S] = top;
}
void _swap()                // (w1 w2 -- w2 w1) swap top two items on the data stack
{
	WP  = top;
	top = stack[(U8)S];
	stack[(U8)S] = WP;
}
void _over()                // (w1 w2 -- w1 w2 w1) copy second stack item to top
{
	_push(stack[(U8)S - 1]);
}
void _zless()               // (n -- f) check whether top of stack is negative
{
	top = (top < 0) ? TRUE : FALSE;
}
void _and()                 // (w w -- w) bitwise AND
{
	top &= stack[(U8)S--];
}
void _or()                  // (w w -- w) bitwise OR
{
	top |= stack[(U8)S--];
}
void _xor()                 // (w w -- w) bitwise XOR
{
	top ^= stack[(U8)S--];
}
void _uplus()               // (w w -- w c) add two numbers, return the sum and carry flag
{
	stack[(U8)S] += top;
	top = (U32)stack[(U8)S] < (U32)top;
}
void _qdup()                // (w -- w w | 0) dup top of stack if it is not zero
{
	if (top) stack[(U8) ++S] = top;
}
void _rot()                 // (w1 w2 w3 -- w2 w3 w1) rotate 3rd item to top
{
	WP = stack[(U8)S - 1];
	stack[(U8)S - 1] = stack[(U8)S];
	stack[(U8)S] = top;
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
	top += stack[(U8)S--];
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
	_push(1);
	_uplus();
	_rfrom();
	_plus();
}
void _sub()                 // (n1 n2 -- n1-n2) subtraction
{
	top = stack[(U8)S--] - top;
}
void _abs()                 // (n -- n) absolute value of n
{
	if (top < 0) top = -top;
}
void _great()               // (n1 n2 -- t) true if n1>n2
{
	top = (stack[(U8)S--] > top) ? TRUE : FALSE;
}
void _less()                // (n1 n2 -- t) true if n1<n2
{
	top = (stack[(U8)S--] < top) ? TRUE : FALSE;
}
void _equal()               // (w w -- t) true if top two items are equal
{
	top = (stack[(U8)S--]==top) ? TRUE : FALSE;
}
void _uless()               // (u1 u2 -- t) unsigned compare top two items
{
	top = ((U32)(stack[(U8)S--]) < (U32)top) ? TRUE : FALSE;
}
void _ummod()               // (udl udh u -- ur uq) unsigned divide of a double by single
{
	S64 d = (S64)top;
	S64 m = (S64)((U32)stack[(U8)S]);
	S64 n = (S64)((U32)stack[(U8)S - 1]);
	n += m << 32;
	_pop();
	top = (U32)(n / d);
	stack[(U8)S] = (U32)(n % d);
}
void _msmod()               // (d n -- r q) signed floored divide of double by single
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n += m << 32;
	_pop();
	top = (S32)(n / d);         // mod
	stack[(U8)S] = (U32)(n % d);// quotien
}
void _slmod()               // (n1 n2 -- r q) signed devide, return mod and quotien
{
	if (top) {
		WP  = stack[(U8)S] / top;
		stack[(U8)S] %= top;
		top = WP;
	}
}
void _mod()                 // (n n -- r) signed divide, returns mod
{
	top = (top) ? stack[(U8)S--] % top : stack[(U8)S--];
}
void _slash()               // (n n - q) signed divide, return quotient
{
	top = (top) ? stack[(U8)S--] / top : (stack[(U8)S--], 0);
}
void _umsta()               // (u1 u2 -- ud) unsigned multiply return double product
{
	U64 d = (U64)top;
	U64 m = (U64)stack[(U8)S];
	m *= d;
	top = (U32)(m >> 32);
	stack[(U8)S] = (U32)m;
}
void _star()                // (n n -- n) signed multiply, return single product
{
	top *= stack[(U8)S--];
}
void _mstar()               // (n1 n2 -- d) signed multiply, return double product
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	m *= d;
	top = (S32)(m >> 32);
	stack[(U8)S] = (S32)m;
}
void _ssmod()               // (n1 n2 n3 -- r q) n1*n2/n3, return mod and quotion
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n *= m;
	_pop();
	top = (S32)(n / d);
	stack[(U8)S] = (S32)(n % d);
}
void _stasl()               // (n1 n2 n3 -- q) n1*n2/n3 return quotient
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n *= m;
	_pop();
    _pop();
	top = (S32)(n / d);
}
void _pick()                // (... +n -- ...w) copy nth stack item to top
{
	top = stack[(U8)S - (U8)top];
}
void _pstor()               // (n a -- ) add n to content at address a
{
	DATA(top) += stack[(U8)S--];
    _pop();
}
void _dstor()               // (d a -- ) store the double to address a
{
	DATA(top+4) = stack[(U8)S--];
	DATA(top)   = stack[(U8)S--];
	_pop();
}
void _dat()                 // (a -- d) fetch double from address a
{
	_push(DATA(top));
	top = DATA(top + 4);
}
void _count()               // (b -- b+1 +n) count byte of a string and add 1 to byte address
{
	stack[(U8)++S] = top + 1;
	top = byte[top];
}
void _max()                 // (n1 n2 -- n) return greater of two top stack items
{
	if (top < stack[(U8)S]) _pop();
	else (U8)S--;
}
void _min()                 // (n1 n2 -- n) return smaller of two top stack items
{
	if (top < stack[(U8)S]) (U8)S--;
	else _pop();
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
