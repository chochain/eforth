/******************************************************************************/
/* ceForth_33.cpp, Version 3.3 : Forth in C                                   */
/******************************************************************************/
/* Chen-Hanson Ting                                                           */
/* 01jul19cht   version 3.3                                                   */
/* Macro assembler, Visual Studio 2019 Community                              */
/* 13jul17cht   version 2.3                                                   */
/* True byte code machine with bytecode                                       */
/* Change w to WP, pointing to parameter field                                */
/* 08jul17cht  version 2.2                                                    */
/* Stacks are 256 cell circular buffers                                       */
/* Clean up, delete SP@, SP!, RP@, RP!                                        */
/* 13jun17cht  version 2.1                                                    */
/* Compiled as a C++ console project in Visual Studio Community 2017          */
/* Follow the eForth model with 64 primitives                                 */
/* Kernel                                                                     */
/* Use long long int to implement multipy and divide primitives               */
/* Case insensitive interpreter                                               */
/* data[] must be filled with rom_21.h eForth dictionary                      */
/*   from c:/F#/ceforth_21                                                    */
/* C compiler must be reminded that S and R are (char)                        */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#define ASSEM_DUMP   0
#define FORTH_TRACE  0

// tracing/logging macros
#if ASSEM_DUMP
#define DEBUG(s, v)     printf(s, v)
#define SHOWOP(op)      printf("\n%04x: %s\t", P, op)
#else  // ASSEM_DUMP
#define DEBUG(s, v)
#define SHOWOP(op)
#endif // ASSEM_DUMP

#if FORTH_TRACE
int TAB = 0;                       // trace indentation counter
#define LOG(s,v)        printf(s,v)
#define INFO(s)         LOG("%s ", s)
#define TRACE_COLON() {                       \
	printf("\n");                             \
	for (int i=0; i<TAB; i++) printf("  ");   \
    TAB++;                                    \
	printf(":");                              \
}
#define TRACE_EXIT() {                        \
    printf(" ; ");                            \
    TAB--;                                    \
}
#else // FORTH_TRACE
#define LOG(s,v)
#define INFO(s)
#define TRACE_COLON()
#define TRACE_EXIT()
#endif // FORTH_TRACE

// portable types
typedef uint64_t  U64;
typedef uint32_t  U32;
typedef uint16_t  U16;
typedef uint8_t   U8;
typedef int64_t   S64;

typedef int32_t   S32;
typedef int16_t   S16;
typedef int8_t    S8;

// logic and stack op macros
#define	FALSE	    0
#define	TRUE	    -1
#define	_pop()		(top = stack[(U8)S--])
#define	_push(v)	{ stack[(U8)++S] = top; top = (S32)(v); }
#define	_popR()     (rack[(U8)R--])
#define	_pushR(v)   (rack[(U8)++R] = (U32)(v))

U8  R=0, S=0;                      // return stack index, data stack index
U32 P, IP, WP;                     // P (program counter), IP (intruction pointer), WP (parameter pointer)
U32 thread;                        // pointer to previous word
S32 top = 0;                       // stack top value (cache)

U32 rack[256]   = { 0 };           // return stack
S32 stack[256]  = { 0 };           // data stack
U32 data[16000] = {};              // 64K forth memory block
U8* byte        = (U8*)data;       // linear byte array pointer

void show_word(int j) {
#if FORTH_TRACE
	U8 *p  = &byte[j];			   // pointer to address
	U32 op = data[j>>2];		   // get opocode
	for (p-=4; *p>31; p-=4);	   // retract pointer to word name

	int  len = (int)*p;
	char buf[64];
	memcpy(buf, p+1, len);
	buf[len] = '\0';
	printf(" %s", buf);
#endif // FORTH_TRACE
}

// Virtual Forth Machine
void _nop(void) {}              // ( -- )
void _bye(void)                 // ( -- ) exit to OS
{
	exit(0);
}
void _qrx(void)                 // ( -- c t|f) read a char from terminal input device
{
	_push(getchar());
	if (top) _push(TRUE);
}
void _txsto(void)               // (c -- ) send a char to console
{
#if !FORTH_TRACE
	putchar((U8)top);
#else  // !FORTH_TRACE
	switch (top) {
	case 0xa: printf("<LF>");  break;
	case 0xd: printf("<CR>");  break;
	case 0x8: printf("<TAB>"); break;
	default:  printf("<%c>", (U8)top);
	}
#endif // !FORTH_TRACE
	_pop();
}
void _next(void)                // advance instruction pointer
{
	P  = data[IP >> 2];			// fetch next address
    show_word(P);
	WP = P + 4;                 // parameter pointer (used optionally)
	IP += 4;
}
void _dovar(void)               // ( -- a) return address of a variable
{
	_push(WP);
}
void _docon(void)               // ( -- n) push next token onto data stack as constant
{
	_push(data[WP >> 2]);
}
void _dolit(void)               // ( -- w) push next token as an integer literal
{
	S32 v = data[IP >> 2];
	DEBUG(" %d", v);
	_push(v);
	IP += 4;
    _next();
}
void _dolist(void)              // ( -- ) push instruction pointer onto return stack and pop 
{
	TRACE_COLON();
	rack[(U8)++R] = IP;
	IP = WP;
    _next();
}
void _exit(void)                // ( -- ) terminate all token lists in colon words
{
	TRACE_EXIT();
	IP = rack[(U8)R--];
    _next();
}
void _execu(void)               // (a -- ) take execution address from data stack and execute the token
{
	P  = top;
	WP = P + 4;
	_pop();
}
void _donext(void)              // ( -- ) terminate a FOR-NEXT loop
{
	if (rack[(U8)R]) {
		rack[(U8)R] -= 1;
		IP = data[IP >> 2];
	}
	else {
		IP += 4;
		R--;
	}
    _next();
}
void _qbran(void)               // (f -- ) test top as a flag on data stack
{
	if (top) IP += 4;
    else     IP = data[IP >> 2];
	_pop();
    _next();
}
void _bran(void)                // ( -- ) branch to address following
{
	IP = data[IP >> 2];
	_next();
}
void _store(void)               // (n a -- ) store into memory location from top of stack
{
	data[top >> 2] = stack[(U8)S--];
	_pop();
}
void _at(void)                  // (a -- n) fetch from memory address onto top of stack
{
	top = data[top >> 2];
}
void _cstor(void)               // (c b -- ) store a byte into memory location
{
	byte[top] = (U8)stack[(U8)S--];
	_pop();
}
void _cat(void)                 // (b -- n) fetch a byte from memory location
{
	top = (U32)byte[top];
}
void _rfrom(void)               // (n --) pop from data stack onto return stack
{
	_push(rack[(U8)R--]);
}
void _rat(void)                 // (-- n) copy a number off the return stack and push onto data stack
{
	_push(rack[(U8)R]);
}
void _tor(void)                 // (-- n) pop from data stack and push onto return stack
{
	rack[(U8)++R] = top;
	_pop();
}
void _drop(void)                // (w -- ) drop top of stack item
{
	_pop();
}
void _dup(void)                 // (w -- w w) duplicate to of stack
{
	stack[(U8)++S] = top;
}
void _swap(void)                // (w1 w2 -- w2 w1) swap top two items on the data stack
{
	WP  = top;
	top = stack[(U8)S];
	stack[(U8)S] = WP;
}
void _over(void)                // (w1 w2 -- w1 w2 w1) copy second stack item to top
{
	_push(stack[(U8)S - 1]);
}
void _zless(void)               // (n -- f) check whether top of stack is negative
{
	top = (top < 0) ? TRUE : FALSE;
}
void _and(void)                 // (w w -- w) bitwise AND
{
	top &= stack[(U8)S--];
}
void _or(void)                  // (w w -- w) bitwise OR
{
	top |= stack[(U8)S--];
}
void _xor(void)                 // (w w -- w) bitwise XOR
{
	top ^= stack[(U8)S--];
}
void _uplus(void)               // (w w -- w c) add two numbers, return the sum and carry flag
{
	stack[(U8)S] += top;
	top = (U32)stack[(U8)S] < (U32)top;
}
void _qdup(void)                // (w -- w w | 0) dup top of stack if it is not zero
{
	if (top) stack[(U8) ++S] = top;
}
void _rot(void)                 // (w1 w2 w3 -- w2 w3 w1) rotate 3rd item to top
{
	WP = stack[(U8)S - 1];
	stack[(U8)S - 1] = stack[(U8)S];
	stack[(U8)S] = top;
	top = WP;
}
void _ddrop(void)               // (w w --) drop top two items
{
	_drop();
	_drop();
}
void _ddup(void)                // (w1 w2 -- w1 w2 w1 w2) duplicate top two items
{
	_over();
	_over();
}
void _plus(void)                // (w w -- sum) add top two items
{
	top += stack[(U8)S--];
}
void _inver(void)               // (w -- w) one's complement
{
	top = -top - 1;
}
void _negat(void)               // (n -- -n) two's complement
{
	top = 0 - top;
}
void _dnega(void)               // (d -- -d) two's complement of top double
{
	_inver();
	_tor();
	_inver();
	_push(1);
	_uplus();
	_rfrom();
	_plus();
}
void _sub(void)                 // (n1 n2 -- n1-n2) subtraction
{
	top = stack[(U8)S--] - top;
}
void _abs(void)                 // (n -- n) absolute value of n
{
	if (top < 0) top = -top;
}
void _great(void)               // (n1 n2 -- t) true if n1>n2
{
	top = (stack[(U8)S--] > top) ? TRUE : FALSE;
}
void _less(void)                // (n1 n2 -- t) true if n1<n2
{
	top = (stack[(U8)S--] < top) ? TRUE : FALSE;
}
void _equal(void)               // (w w -- t) true if top two items are equal
{
	top = (stack[(U8)S--]==top) ? TRUE : FALSE;
}
void _uless(void)               // (u1 u2 -- t) unsigned compare top two items
{
	top = ((U32)(stack[(U8)S--]) < (U32)top) ? TRUE : FALSE;
}
void _ummod(void)               // (udl udh u -- ur uq) unsigned divide of a double by single
{
	S64 d = (S64)top;
	S64 m = (S64)((U32)stack[(U8)S]);
	S64 n = (S64)((U32)stack[(U8)S - 1]);
	n += m << 32;
	_pop();
	top = (U32)(n / d);
	stack[(U8)S] = (U32)(n % d);
}
void _msmod(void)               // (d n -- r q) signed floored divide of double by single
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n += m << 32;
	_pop();
	top = (S32)(n / d);         // mod
	stack[(U8)S] = (U32)(n % d);// quotien
}
void _slmod(void)               // (n1 n2 -- r q) signed devide, return mod and quotien
{
	if (top) {
		WP  = stack[(U8)S] / top;
		stack[(U8)S] %= top;
		top = WP;
	}
}
void _mod(void)                 // (n n -- r) signed divide, returns mod
{
	top = (top) ? stack[(U8)S--] % top : stack[(U8)S--];
}
void _slash(void)               // (n n - q) signed divide, return quotient
{
	top = (top) ? stack[(U8)S--] / top : (stack[(U8)S--], 0);
}
void _umsta(void)               // (u1 u2 -- ud) unsigned multiply return double product
{
	U64 d = (U64)top;
	U64 m = (U64)stack[(U8)S];
	m *= d;
	top = (U32)(m >> 32);
	stack[(U8)S] = (U32)m;
}
void _star(void)                // (n n -- n) signed multiply, return single product
{
	top *= stack[(U8)S--];
}
void _mstar(void)               // (n1 n2 -- d) signed multiply, return double product
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	m *= d;
	top = (S32)(m >> 32);
	stack[(U8)S] = (S32)m;
}
void _ssmod(void)               // (n1 n2 n3 -- r q) n1*n2/n3, return mod and quotion
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n *= m;
	_pop();
	top = (S32)(n / d);
	stack[(U8)S] = (S32)(n % d);
}
void _stasl(void)               // (n1 n2 n3 -- q) n1*n2/n3 return quotient
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n *= m;
	_pop();
    _pop();
	top = (S32)(n / d);
}
void _pick(void)                // (... +n -- ...w) copy nth stack item to top
{
	top = stack[(U8)S - (U8)top];
}
void _pstor(void)               // (n a -- ) add n to content at address a
{
	data[top >> 2] += stack[(U8)S--];
    _pop();
}
void _dstor(void)               // (d a -- ) store the double to address a
{
	data[(top >> 2) + 1] = stack[(U8)S--];
	data[top >> 2]       = stack[(U8)S--];
	_pop();
}
void _dat(void)                 // (a -- d) fetch double from address a
{
	_push(data[top >> 2]);
	top = data[(top >> 2) + 1];
}
void _count(void)               // (b -- b+1 +n) count byte of a string and add 1 to byte address
{
	stack[(U8)++S] = top + 1;
	top = byte[top];
}
void _max(void)                 // (n1 n2 -- n) return greater of two top stack items
{
	if (top < stack[(U8)S]) _pop();
	else (U8)S--;
}
void _min(void)                 // (n1 n2 -- n) return smaller of two top stack items
{
	if (top < stack[(U8)S]) (U8)S--;
	else _pop();
}

void(*primitives[64])(void) = {
	/* case 0 */ _nop,
	/* case 1 */ _bye,
	/* case 2 */ _qrx,
	/* case 3 */ _txsto,
	/* case 4 */ _docon,
	/* case 5 */ _dolit,
	/* case 6 */ _dolist,
	/* case 7 */ _exit,
	/* case 8 */ _execu,
	/* case 9 */ _donext,
	/* case 10 */ _qbran,
	/* case 11 */ _bran,
	/* case 12 */ _store,
	/* case 13 */ _at,
	/* case 14 */ _cstor,
	/* case 15 */ _cat,
	/* case 16  rpat, */  _nop,
	/* case 17  rpsto, */ _nop,
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

// Opcodes (for Bytecode Assembler)
enum {
    as_nop = 0,    // 0
    as_bye,        // 1
    as_qrx,        // 2
    as_txsto,      // 3
    as_docon,      // 4
    as_dolit,      // 5
    as_dolist,     // 6
    as_exit,       // 7
    as_execu,      // 8
    as_donext,     // 9
    as_qbran,      // 10
    as_bran,       // 11
    as_store,      // 12
    as_at,         // 13
    as_cstor,      // 14
    as_cat,        // 15
    as_rpat,       // 16
    as_rpsto,      // 17
    as_rfrom,      // 18
    as_rat,        // 19
    as_tor,        // 20
    as_spat,       // 21
    as_spsto,      // 22
    as_drop,       // 23
    as_dup,        // 24
    as_swap,       // 25
    as_over,       // 26
    as_zless,      // 27
    as_and,        // 28
    as_or,         // 29
    as_xor,        // 30
    as_uplus,      // 31
    as_next,       // 32
    as_qdup,       // 33
    as_rot,        // 34
    as_ddrop,      // 35
    as_ddup,       // 36
    as_plus,       // 37
    as_inver,      // 38
    as_negat,      // 39
    as_dnega,      // 40
    as_sub,        // 41
    as_abs,        // 42
    as_equal,      // 43
    as_uless,      // 44
    as_less,       // 45
    as_ummod,      // 46
    as_msmod,      // 47
    as_slmod,      // 48
    as_mod,        // 49
    as_slash,      // 50
    as_umsta,      // 51
    as_star,       // 52
    as_mstar,      // 53
    as_ssmod,      // 54
    as_stasl,      // 55
    as_pick,       // 56
    as_pstor,      // 57
    as_dstor,      // 58
    as_dat,        // 59
    as_count,      // 60
    as_dovar,      // 61
    as_max,        // 62
    as_min         // 63
};

// Macro Assembler
#define FLAG_IMEDD  0x80              // immediate flag
#define FLAG_COMPO  0x40              // composit flag
int BRAN = 0, QBRAN = 0, DONXT = 0, DOTQP = 0, STRQP = 0, TOR = 0, ABORQP = 0, NOP = 0;

void _header(int lex, const char *seq) {
	IP = P >> 2;
	U32 len = lex & 0x1f;                     // max length 31
	data[IP++] = thread;                      // point to previous word

	// dump memory between previous word and this
	DEBUG("%s", "\n    :");
	for (U32 i = thread>>2; thread && i < IP; i++) {
		DEBUG(" %08x", data[i]);
	}
	DEBUG("%c", '\n');

	P = IP << 2;
	thread = P;                               // keep pointer to this word
	byte[P++] = lex;                          // length of word
	for (U32 i = 0; i < len; i++) {           // memcpy word string
		byte[P++] = seq[i];
	}
	while (P & 3) { byte[P++] = 0; }          // padding 4-byte align
	DEBUG("%04x: ", P);
	DEBUG("%s", seq);
}
int _code(const char *seg, int len, ...) {
    _header(strlen(seg), seg);
	int addr = P;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		U8 j = (U8)va_arg(argList, int);
		byte[P++] = j;
		DEBUG(" %02x", j);
	}
	va_end(argList);
	return addr;
}
#define DATACPY(n) {                  \
	va_list argList;                  \
	va_start(argList, n);             \
	for (; n; n--) {                  \
		U32 j = va_arg(argList, U32); \
		if (j==NOP) continue;         \
		data[IP++] = j;               \
		DEBUG(" %04x", j);            \
	}                                 \
	va_end(argList);                  \
}
int _colon(const char *seg, int len, ...) {
    _header(strlen(seg), seg);
	DEBUG("%s", " COLON 0006");
	int addr = P;
	IP = P >> 2;
	data[IP++] = as_dolist;
	DATACPY(len);
	P = IP << 2;
	return addr;
}
int _immed(const char *seg, int len, ...) {
    _header(FLAG_IMEDD | strlen(seg), seg);
	DEBUG("%s", " IMMED 0006");
	int addr = P;
	IP = P >> 2;
	data[IP++] = as_dolist;
    DATACPY(len);
	P = IP << 2;
	return addr;
}
int _label(int len, ...) {
	SHOWOP("LABEL");
	int addr = P;
	IP = P >> 2;
	// label has no opcode here
    DATACPY(len);
	P = IP << 2;
	return addr;
}
void _begin(int len, ...) {
	SHOWOP("BEGIN");
	IP = P >> 2;
	_pushR(IP);                   // keep current address for looping
    DATACPY(len);
	P = IP << 2;
}
void _again(int len, ...) {
	SHOWOP("AGAIN");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = _popR() << 2;    // loop begin address
    DATACPY(len);
	P = IP << 2;
}
void _until(int len, ...) {
	SHOWOP("UNTIL");
	IP = P >> 2;
	data[IP++] = QBRAN;           // conditional branch
	data[IP++] = _popR() << 2;    // loop begin address
    DATACPY(len);
	P = IP << 2;
}
void _while(int len, ...) {
	SHOWOP("WHILE");
	IP = P >> 2;
	data[IP++] = QBRAN;           // conditional branch
	data[IP++] = 0;
	int k = _popR();
	_pushR(IP - 1);
	_pushR(k);
    DATACPY(len);
	P = IP << 2;
}
void _repeat(int len, ...) {
	SHOWOP("REPEAT");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = _popR() << 2;    // loop begin address
	data[_popR()] = IP << 2;
    DATACPY(len);
	P = IP << 2;
}
void _if(int len, ...) {
	SHOWOP("IF");
	IP = P >> 2;
	data[IP++] = QBRAN;
	_pushR(IP);
	data[IP++] = 0;
    DATACPY(len);
	P = IP << 2;
}
void _else(int len, ...) {
	SHOWOP("ELSE");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = 0;
	data[_popR()] = IP << 2;
	_pushR(IP - 1);
    DATACPY(len);
	P = IP << 2;
}
void _then(int len, ...) {
	SHOWOP("THEN");
	IP = P >> 2;
	data[_popR()] = IP << 2;
    DATACPY(len);
	P = IP << 2;
}
void _for(int len, ...) {
	SHOWOP("FOR");
	IP = P >> 2;
	data[IP++] = TOR;
	_pushR(IP);
    DATACPY(len);
	P = IP << 2;
}
void _next(int len, ...) {
	SHOWOP("NEXT");
	IP = P >> 2;
	data[IP++] = DONXT;
	data[IP++] = _popR() << 2;
    DATACPY(len);
	P = IP << 2;
}
void _aft(int len, ...) {
	SHOWOP("AFT");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = 0;
	_popR();
	_pushR(IP);
	_pushR(IP - 1);
    DATACPY(len);
	P = IP << 2;
}
#define STRCPY(op, seq) {              \
	IP = P >> 2;                       \
    data[IP++] = op;                   \
    P  = IP << 2;                      \
	int len = strlen(seq);             \
	byte[P++] = len;                   \
	for (int i = 0; i < len; i++) {    \
		byte[P++] = seq[i];            \
	}                                  \
	while (P & 3) { byte[P++] = 0; }   \
	}
void _DOTQ(const char *seq) {
	SHOWOP("DOTQ");
	DEBUG("%s", seq);
	STRCPY(DOTQP, seq);
}
void _STRQ(const char *seq) {
	SHOWOP("STRQ");
	DEBUG("%s", seq);
	STRCPY(STRQP, seq);
}
void _ABORQ(const char *seq) {
	SHOWOP("ABORQP");
	DEBUG("%s", seq);
	STRCPY(ABORQP, seq);
}
/* Note dummy first argument _ and ##__VA_ARGS__ instead of __VA_ARGS__ */
//#define my_func(...)     func(_NARG(__VA_ARGS__), __VA_ARGS__)

#define _ARG_N(                                            \
          _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, _10, \
         _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
         _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
         _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
         _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
         _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
         _61, _62, _63, N, ...) N
#define _NUM_N()                                           \
         62, 61, 60,                                       \
         59, 58, 57, 56, 55, 54, 53, 52, 51, 50,           \
         49, 48, 47, 46, 45, 44, 43, 42, 41, 40,           \
         39, 38, 37, 36, 35, 34, 33, 32, 31, 30,           \
         29, 28, 27, 26, 25, 24, 23, 22, 21, 20,           \
         19, 18, 17, 16, 15, 14, 13, 12, 11, 10,           \
          9,  8,  7,  6,  5,  4,  3,  2,  1,  0
#define _NARG0(...)          _ARG_N(__VA_ARGS__)
#define _NARG(...)           _NARG0(_, ##__VA_ARGS__, _NUM_N())
#define _CODE(seg, ...)      _code(seg, _NARG(__VA_ARGS__), __VA_ARGS__)
#define _COLON(seg, ...)     _colon(seg, _NARG(__VA_ARGS__), __VA_ARGS__)
#define _IMMED(seg, ...)     _immed(seg, _NARG(__VA_ARGS__), __VA_ARGS__)
#define _LABEL(...)          _label(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _BEGIN(...)          _begin(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _AGAIN(...)          _again(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _UNTIL(...)          _until(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _WHILE(...)          _while(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _REPEAT(...)         _repeat(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _IF(...)             _if(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _ELSE(...)           _else(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _THEN(...)           _then(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _FOR(...)            _for(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _NEXT(...)           _next(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _AFT(...)            _aft(_NARG(__VA_ARGS__), __VA_ARGS__)

void assemble() {
	P = 0x200;
	R = thread = 0;

	// Kernel (user variables for input)
	int vHLD  = _CODE("HLD",     as_docon, as_next, 0, 0, 0x80, 0, 0, 0);
	int vSPAN = _CODE("SPAN",    as_docon, as_next, 0, 0, 0x84, 0, 0, 0);
	int vIN   = _CODE(">IN",     as_docon, as_next, 0, 0, 0x88, 0, 0, 0);
	int vNTIB = _CODE("#TIB",    as_docon, as_next, 0, 0, 0x8c, 0, 0, 0);
	int vTTIB = _CODE("'TIB",    as_docon, as_next, 0, 0, 0x90, 0, 0, 0);
	int vBASE = _CODE("BASE",    as_docon, as_next, 0, 0, 0x94, 0, 0, 0);
	int vCNTX = _CODE("CONTEXT", as_docon, as_next, 0, 0, 0x98, 0, 0, 0);
	int vCP   = _CODE("CP",      as_docon, as_next, 0, 0, 0x9c, 0, 0, 0);
	int vLAST = _CODE("LAST",    as_docon, as_next, 0, 0, 0xa0, 0, 0, 0);
	int vTEVL = _CODE("'EVAL",   as_docon, as_next, 0, 0, 0xa4, 0, 0, 0);
	int vTABRT= _CODE("'ABORT",  as_docon, as_next, 0, 0, 0xa8, 0, 0, 0);
	int vTEMP = _CODE("tmp",     as_docon, as_next, 0, 0, 0xac, 0, 0, 0);

	// Kernel dictionary (primitive proxies)
	NOP       = _CODE("NOP",     as_nop,   as_next, 0, 0);
	int BYE   = _CODE("BYE",     as_bye,   as_next, 0, 0);
	int QRX   = _CODE("?RX",     as_qrx,   as_next, 0, 0);
	int TXSTO = _CODE("TX!",     as_txsto, as_next, 0, 0);
	int DOCON = _CODE("DOCON",   as_docon, as_next, 0, 0);
	int DOLIT = _CODE("DOLIT",   as_dolit, as_next, 0, 0);
	int DOLST = _CODE("DOLIST",  as_dolist,as_next, 0, 0);
	int EXIT  = _CODE("EXIT",    as_exit,  as_next, 0, 0);
	int EXECU = _CODE("EXECUTE", as_execu, as_next, 0, 0);

	DONXT     = _CODE("DONEXT",  as_donext,as_next, 0, 0);
	QBRAN     = _CODE("QBRANCH", as_qbran, as_next, 0, 0);
	BRAN      = _CODE("BRANCH",  as_bran,  as_next, 0, 0);

	int STORE = _CODE("!",       as_store, as_next, 0, 0);
	int AT    = _CODE("@",       as_at,    as_next, 0, 0);
	int CSTOR = _CODE("C!",      as_cstor, as_next, 0, 0);
	int CAT   = _CODE("C@",      as_cat,   as_next, 0, 0);
	int RFROM = _CODE("R>",      as_rfrom, as_next, 0, 0);
	int RAT   = _CODE("R@",      as_rat,   as_next, 0, 0);
	TOR       = _CODE(">R",      as_tor,   as_next, 0, 0);
	int DROP  = _CODE("DROP",    as_drop,  as_next, 0, 0);
	int DUP   = _CODE("DUP",     as_dup,   as_next, 0, 0);
	int SWAP  = _CODE("SWAP",    as_swap,  as_next, 0, 0);
	int OVER  = _CODE("OVER",    as_over,  as_next, 0, 0);
	int ZLESS = _CODE("0<",      as_zless, as_next, 0, 0);
	int AND   = _CODE("AND",     as_and,   as_next, 0, 0);
	int OR    = _CODE("OR",      as_or,    as_next, 0, 0);
	int XOR   = _CODE("XOR",     as_xor,   as_next, 0, 0);
	int UPLUS = _CODE("UM+",     as_uplus, as_next, 0, 0);
	int NEXT  = _CODE("NEXT",    as_next,  as_next, 0, 0);
	int QDUP  = _CODE("?DUP",    as_qdup,  as_next, 0, 0);
	int ROT   = _CODE("ROT",     as_rot,   as_next, 0, 0);
	int DDROP = _CODE("2DROP",   as_ddrop, as_next, 0, 0);
	int DDUP  = _CODE("2DUP",    as_ddup,  as_next, 0, 0);
	int PLUS  = _CODE("+",       as_plus,  as_next, 0, 0);
	int INVER = _CODE("NOT",     as_inver, as_next, 0, 0);
	int NEGAT = _CODE("NEGATE",  as_negat, as_next, 0, 0);
	int DNEGA = _CODE("DNEGATE", as_dnega, as_next, 0, 0);
	int SUB   = _CODE("-",       as_sub,   as_next, 0, 0);
	int ABS   = _CODE("ABS",     as_abs,   as_next, 0, 0);
	int EQUAL = _CODE("=",       as_equal, as_next, 0, 0);
	int ULESS = _CODE("U<",      as_uless, as_next, 0, 0);
	int LESS  = _CODE("<",       as_less,  as_next, 0, 0);
	int UMMOD = _CODE("UM/MOD",  as_ummod, as_next, 0, 0);
	int MSMOD = _CODE("M/MOD",   as_msmod, as_next, 0, 0);
	int SLMOD = _CODE("/MOD",    as_slmod, as_next, 0, 0);
	int MOD   = _CODE("MOD",     as_mod,   as_next, 0, 0);
	int SLASH = _CODE("/",       as_slash, as_next, 0, 0);
	int UMSTA = _CODE("UM*",     as_umsta, as_next, 0, 0);
	int STAR  = _CODE("*",       as_star,  as_next, 0, 0);
	int MSTAR = _CODE("M*",      as_mstar, as_next, 0, 0);
	int SSMOD = _CODE("*/MOD",   as_ssmod, as_next, 0, 0);
	int STASL = _CODE("*/",      as_stasl, as_next, 0, 0);
	int PICK  = _CODE("PICK",    as_pick,  as_next, 0, 0);
	int PSTOR = _CODE("+!",      as_pstor, as_next, 0, 0);
	int DSTOR = _CODE("2!",      as_dstor, as_next, 0, 0);
	int DAT   = _CODE("2@",      as_dat,   as_next, 0, 0);
	int COUNT = _CODE("COUNT",   as_count, as_next, 0, 0);
	int MAX   = _CODE("MAX",     as_max,   as_next, 0, 0);
	int MIN   = _CODE("MIN",     as_min,   as_next, 0, 0);

	int BLANK = _CODE("BL",      as_docon, as_next, 0,       0, 32, 0, 0, 0);
	int CELL  = _CODE("CELL",    as_docon, as_next, 0,       0,  4, 0, 0, 0);
	int CELLP = _CODE("CELL+",   as_docon, as_plus, as_next, 0,  4, 0, 0, 0);
	int CELLM = _CODE("CELL-",   as_docon, as_sub,  as_next, 0,  4, 0, 0, 0);
	int CELLS = _CODE("CELLS",   as_docon, as_star, as_next, 0,  4, 0, 0, 0);
	int CELLD = _CODE("CELL/",   as_docon, as_slash,as_next, 0,  4, 0, 0, 0);
	int ONEP  = _CODE("1+",      as_docon, as_plus, as_next, 0,  1, 0, 0, 0);
	int ONEM  = _CODE("1-",      as_docon, as_sub,  as_next, 0,  1, 0, 0, 0);
	int DOVAR = _CODE("DOVAR",   as_dovar, as_next, 0, 0);

	// Common Colon Words

	int QKEY  = _COLON("?KEY",  QRX, EXIT);
	int KEY   = _COLON("KEY",   NOP); {
        _BEGIN(QKEY);
        _UNTIL(EXIT);
    }
	int EMIT  = _COLON("EMIT",    TXSTO, EXIT);
	int WITHI = _COLON("WITHIN",  OVER, SUB, TOR, SUB, RFROM, ULESS, EXIT);
	int TCHAR = _COLON(">CHAR",   DOLIT, 0x7f, AND, DUP, DOLIT, 0x7f, BLANK, WITHI); {
        _IF(DROP, DOLIT, 0x5f);
        _THEN(EXIT);
    }
	int ALIGN = _COLON("ALIGNED", DOLIT, 3, PLUS, DOLIT, 0xfffffffc, AND, EXIT);
	int HERE  = _COLON("HERE",    vCP, AT, EXIT);
	int PAD   = _COLON("PAD",     HERE, DOLIT, 0x50, PLUS, EXIT);
	int TIB   = _COLON("TIB",     vTTIB, AT, EXIT);
	int ATEXE = _COLON("@EXECUTE",AT, QDUP); {
        _IF(EXECU);
        _THEN(EXIT);
    }
    int CMOVE = _COLON("CMOVE", NOP); {
        _FOR(NOP);
        _AFT(OVER, CAT, OVER, CSTOR, TOR, ONEP, RFROM, ONEP);
        _THEN(NOP);
        _NEXT(DDROP, EXIT);
    }
	int MOVE  = _COLON("MOVE", CELLD); {
        _FOR(NOP);
        _AFT(OVER, AT, OVER, STORE, TOR, CELLP, RFROM, CELLP);
        _THEN(NOP);
        _NEXT(DDROP, EXIT);
    }
	int FILL = _COLON("FILL", SWAP); {
        _FOR(SWAP);
        _AFT(DDUP, CSTOR, ONEP);
        _THEN(NOP);
        _NEXT(DDROP, EXIT);
    }

	// Number Conversions

	int DIGIT = _COLON("DIGIT",   DOLIT, 9, OVER, LESS, DOLIT, 7, AND, PLUS, DOLIT, 0x30, PLUS, EXIT);
	int EXTRC = _COLON("EXTRACT", DOLIT, 0, SWAP, UMMOD, SWAP, DIGIT, EXIT);
	int BDIGS = _COLON("<#",      PAD, vHLD, STORE, EXIT);
	int HOLD  = _COLON("HOLD",    vHLD, AT, ONEM, DUP, vHLD, STORE, CSTOR, EXIT);
	int DIG   = _COLON("#",       vBASE, AT, EXTRC, HOLD, EXIT);
	int DIGS  = _COLON("#S", NOP); {
        _BEGIN(DIG, DUP);
        _WHILE(NOP);
        _REPEAT(EXIT);
    }
	int SIGN  = _COLON("SIGN",    ZLESS); {
        _IF(DOLIT, 0x2d, HOLD);
        _THEN(EXIT);
    }
	int EDIGS = _COLON("#>",      DROP, vHLD, AT, PAD, OVER, SUB, EXIT);
	int STR   = _COLON("str",     DUP, TOR, ABS, BDIGS, DIGS, RFROM, SIGN, EDIGS, EXIT);
	int HEX   = _COLON("HEX",     DOLIT, 16, vBASE, STORE, EXIT);
	int DECIM = _COLON("DECIMAL", DOLIT, 10, vBASE, STORE, EXIT);
	int UPPER = _COLON("wupper",  DOLIT, 0x5f5f5f5f, AND, EXIT);
	int TOUPP = _COLON(">upper",  DUP, DOLIT, 0x61, DOLIT, 0x7b, WITHI); {
        _IF(DOLIT, 0x5f, AND);
        _THEN(EXIT);
    }
	int DIGTQ = _COLON("DIGIT?",  TOR, TOUPP, DOLIT, 0x30, SUB, DOLIT, 9, OVER, LESS); {
        _IF(DOLIT, 7, SUB, DUP, DOLIT, 10, LESS, OR);
        _THEN(DUP, RFROM, ULESS, EXIT);
    }
	int NUMBQ = _COLON("NUMBER?", vBASE, AT, TOR, DOLIT, 0, OVER, COUNT, OVER, CAT, DOLIT, 0x24, EQUAL); {
        _IF(HEX, SWAP, ONEP, SWAP, ONEM);
        _THEN(OVER, CAT, DOLIT, 0x2d, EQUAL, TOR, SWAP, RAT, SUB, SWAP, RAT, PLUS, QDUP); {
            _IF(ONEM); {
                _FOR(DUP, TOR, CAT, vBASE, AT, DIGTQ);
                _WHILE(SWAP, vBASE, AT, STAR, PLUS, RFROM, ONEP);
                _NEXT(DROP, RAT);
                _IF(NEGAT);
                _THEN(SWAP);
            }
            _ELSE(RFROM, RFROM, DDROP, DDROP, DOLIT, 0);
            _THEN(DUP);
        }
        _THEN(RFROM, DDROP, RFROM, vBASE, STORE, EXIT);
    }

	// Terminal Output

	int SPACE = _COLON("SPACE", BLANK, EMIT, EXIT);
	int CHARS = _COLON("CHARS", SWAP, DOLIT, 0, MAX); {
        _FOR(NOP);
        _AFT(DUP, EMIT);
        _THEN(NOP);
        _NEXT(DROP, EXIT);
    }
	int SPACS = _COLON("SPACES", BLANK, CHARS, EXIT);
	int TYPE  = _COLON("TYPE",   NOP); {
        _FOR(NOP);
        _AFT(COUNT, TCHAR, EMIT);
        _THEN(NOP);
        _NEXT(DROP, EXIT);
    }
	int CR    = _COLON("CR",    DOLIT, 10, DOLIT, 13, EMIT, EMIT, EXIT);
	int DOSTR = _COLON("do$",   RFROM, RAT, RFROM, COUNT, PLUS, ALIGN, TOR, SWAP, TOR, EXIT);
	int STRQP = _COLON("$\"|",  DOSTR, EXIT);
	DOTQP     = _COLON(".\"|",  DOSTR, COUNT, TYPE, EXIT);
	int DOTR  = _COLON(".R",    TOR, STR, RFROM, OVER, SUB, SPACS, TYPE, EXIT);
	int UDOTR = _COLON("U.R",   TOR, BDIGS, DIGS, EDIGS, RFROM, OVER, SUB, SPACS, TYPE, EXIT);
	int UDOT  = _COLON("U.",    BDIGS, DIGS, EDIGS, SPACE, TYPE, EXIT);
	int DOT   = _COLON(".",     vBASE, AT, DOLIT, 0xa, XOR); {
        _IF(UDOT, EXIT);
        _THEN(STR, SPACE, TYPE, EXIT);
    }
	int QUEST = _COLON("?",     AT, DOT, EXIT);

	// Parser

	int PARS  = _COLON("(parse)", vTEMP, CSTOR, OVER, TOR, DUP); {
        _IF(ONEM, vTEMP, CAT, BLANK, EQUAL); {
            _IF(NOP); {
                _FOR(BLANK, OVER, CAT, SUB, ZLESS, INVER);
                _WHILE(ONEP);
                _NEXT(RFROM, DROP, DOLIT, 0, DUP, EXIT);
                _THEN(RFROM);
            }
            _THEN(OVER, SWAP); {
                _FOR(vTEMP, CAT, OVER, CAT, SUB, vTEMP, CAT, BLANK, EQUAL); {
                    _IF(ZLESS);
                    _THEN(NOP);
                }
                _WHILE(ONEP);
                _NEXT(DUP, TOR);
            }
        }
        _ELSE(RFROM, DROP, DUP, ONEP, TOR);
        _THEN(OVER, SUB, RFROM, RFROM, SUB, EXIT);
        _THEN(OVER, RFROM, SUB, EXIT);                   // CC: this line is questionable
    }
	int PACKS = _COLON("PACK$", DUP, TOR, DDUP, PLUS, DOLIT, 0xfffffffc, AND, DOLIT, 0, SWAP, STORE, DDUP, CSTOR, ONEP, SWAP, CMOVE, RFROM, EXIT);
	int PARSE = _COLON("PARSE", TOR, TIB, vIN, AT, PLUS, vNTIB, AT, vIN, AT, SUB, RFROM, PARS, vIN, PSTOR, EXIT);
	int TOKEN = _COLON("TOKEN", BLANK, PARSE, DOLIT, 0x1f, MIN, HERE, CELLP, PACKS, EXIT);
	int WORDD = _COLON("WORD",  PARSE, HERE, CELLP, PACKS, EXIT);
	int NAMET = _COLON("NAME>", COUNT, DOLIT, 0x1f, AND, PLUS, ALIGN, EXIT);
	int SAMEQ = _COLON("SAME?", DOLIT, 0x1f, AND, CELLD); {
        _FOR(NOP);
        _AFT(OVER, RAT, CELLS, PLUS, AT, UPPER, OVER, RAT, CELLS, PLUS, AT, UPPER, SUB, QDUP); {
            _IF(RFROM, DROP, EXIT);
            _THEN(NOP);
        }
        _THEN(NOP);
        _NEXT(DOLIT, 0, EXIT);
    }
	int FIND = _COLON("find", SWAP, DUP, AT, vTEMP, STORE, DUP, AT, TOR, CELLP, SWAP); {
        _BEGIN(AT, DUP); {
            _IF(DUP, AT, DOLIT, 0xffffff3f, AND, UPPER, RAT, UPPER, XOR); {
                _IF(CELLP, DOLIT, 0xffffffff);
                _ELSE(CELLP, vTEMP, AT, SAMEQ);
                _THEN(NOP);
            }
            _ELSE(RFROM, DROP, SWAP, CELLM, SWAP, EXIT);
            _THEN(NOP);
        }
        _WHILE(CELLM, CELLM);
        _REPEAT(RFROM, DROP, SWAP, DROP, CELLM, DUP, NAMET, SWAP, EXIT);
    }
	int NAMEQ = _COLON("NAME?", vCNTX, FIND, EXIT);

	// Terminal Input

	int HATH  = _COLON("^H", TOR, OVER, RFROM, SWAP, OVER, XOR); {
        _IF(DOLIT, 8, EMIT, ONEM, BLANK, EMIT, DOLIT, 8, EMIT);
        _THEN(EXIT);
    }
	int TAP   = _COLON("TAP",  DUP, EMIT, OVER, CSTOR, ONEP, EXIT);
	int KTAP  = _COLON("kTAP", DUP, DOLIT, 0xd, XOR, OVER, DOLIT, 0xa, XOR, AND); {
        _IF(DOLIT, 8, XOR); {
            _IF(BLANK, TAP);
            _ELSE(HATH);
            _THEN(EXIT);
        }
        _THEN(DROP, SWAP, DROP, DUP, EXIT);
    }
	int ACCEP = _COLON("ACCEPT", OVER, PLUS, OVER); {
        _BEGIN(DDUP, XOR);
        _WHILE(KEY, DUP, BLANK, SUB, DOLIT, 0x5f, ULESS); {
            _IF(TAP);
            _ELSE(KTAP);
            _THEN(NOP);
        }
        _REPEAT(DROP, OVER, SUB, EXIT);
    }
	int EXPEC = _COLON("EXPECT", ACCEP, vSPAN, STORE, DROP, EXIT);
	int QUERY = _COLON("QUERY",  TIB, DOLIT, 0x50, ACCEP, vNTIB, STORE, DROP, DOLIT, 0, vIN, STORE, EXIT);

	// Text Interpreter

	int ABORT = _COLON("ABORT", vTABRT, ATEXE);
	ABORQP = _COLON("abort\"",  NOP); {
        _IF(DOSTR, COUNT, TYPE, ABORT);
        _THEN(DOSTR, DROP, EXIT);
    }
	int ERROR = _COLON("ERROR",      SPACE, COUNT, TYPE, DOLIT, 0x3f, EMIT, DOLIT, 0x1b, EMIT, CR, ABORT);
	int INTER = _COLON("$INTERPRET", NAMEQ, QDUP); {
        _IF(CAT, DOLIT, FLAG_COMPO, AND);
        _ABORQ(" compile only");
    }
	int INTER0= _LABEL(EXECU, EXIT); {
        _THEN(NUMBQ);
        _IF(EXIT);
        _ELSE(ERROR);
        _THEN(NOP);
    }
	int LBRAC = _IMMED("[",   DOLIT, INTER, vTEVL, STORE, EXIT);
	int DOTOK = _COLON(".OK", CR, DOLIT, INTER, vTEVL, AT, EQUAL); {
        _IF(TOR, TOR, TOR, DUP, DOT, RFROM, DUP, DOT, RFROM, DUP, DOT, RFROM, DUP, DOT); {
            _DOTQ(" ok>");
        }
        _THEN(EXIT);
    }
	int EVAL  = _COLON("EVAL", NOP); {
        _BEGIN(TOKEN, DUP, AT);
        _WHILE(vTEVL, ATEXE);
        _REPEAT(DROP, DOTOK, EXIT);
    }
	int QUIT  = _COLON("QUIT", DOLIT, 0x100, vTTIB, STORE, LBRAC); {
        _BEGIN(QUERY, EVAL);
        _AGAIN(NOP);
    }

	// Colon Word Compiler

	int COMMA = _COLON(",",       HERE, DUP, CELLP, vCP, STORE, STORE, EXIT);
	int LITER = _IMMED("LITERAL", DOLIT, DOLIT, COMMA, COMMA, EXIT);
	int ALLOT = _COLON("ALLOT",   ALIGN, vCP, PSTOR, EXIT);
	int STRCQ = _COLON("$,\"",    DOLIT, 0x22, WORDD, COUNT, PLUS, ALIGN, vCP, STORE, EXIT);
	int UNIQU = _COLON("?UNIQUE", DUP, NAMEQ, QDUP); {
        _IF(COUNT, DOLIT, 0x1f, AND, SPACE, TYPE); {
            _DOTQ(" reDef");
        }
        _THEN(DROP, EXIT);
    }
	int SNAME = _COLON("$,n", DUP, AT); {
        _IF(UNIQU, DUP, NAMET, vCP, STORE, DUP, vLAST, STORE, CELLM, vCNTX, AT, SWAP, STORE, EXIT);
        _THEN(ERROR);
    }
	int TICK  = _COLON("'", TOKEN, NAMEQ); {
        _IF(EXIT);
        _THEN(ERROR);
    }
	int BCOMP = _IMMED("[COMPILE]", TICK, COMMA, EXIT);
	int COMPI = _COLON("COMPILE",  RFROM, DUP, AT, COMMA, CELLP, TOR, EXIT);
	int SCOMP = _COLON("$COMPILE", NAMEQ, QDUP); {
        _IF(AT, DOLIT, FLAG_IMEDD, AND); {
            _IF(EXECU);
            _ELSE(COMMA);
            _THEN(EXIT);
        }
        _THEN(NUMBQ);
        _IF(LITER, EXIT);
        _THEN(ERROR);
    }
	int OVERT = _COLON("OVERT", vLAST, AT, vCNTX, STORE, EXIT);
	int RBRAC = _COLON("]", DOLIT, SCOMP, vTEVL, STORE, EXIT);
	int COLON = _COLON(":", TOKEN, SNAME, RBRAC, DOLIT, 0x6, COMMA, EXIT);
	int SEMIS = _IMMED(";", DOLIT, EXIT, COMMA, LBRAC, OVERT, EXIT);

	// Debugging Tools

	int DMP   = _COLON("dm+", OVER, DOLIT, 6, UDOTR); {
        _FOR(NOP);
        _AFT(DUP, AT, DOLIT, 9, UDOTR, CELLP);
        _THEN(NOP);
        _NEXT(EXIT);
    }
	int DUMP  = _COLON("DUMP", vBASE, AT, TOR, HEX, DOLIT, 0x1f, PLUS, DOLIT, 0x20, SLASH); {
        _FOR(NOP);
        _AFT(CR, DOLIT, 8, DDUP, DMP, TOR, SPACE, CELLS, TYPE, RFROM);
        _THEN(NOP);
        _NEXT(DROP, RFROM, vBASE, STORE, EXIT);
    }
	int TNAME = _COLON(">NAME", vCNTX); {
        _BEGIN(AT, DUP);
        _WHILE(DDUP, NAMET, XOR); {
            _IF(ONEM);
            _ELSE(SWAP, DROP, EXIT);
            _THEN(NOP);
        }
        _REPEAT(SWAP, DROP, EXIT);
    }
	int DOTID = _COLON(".ID",   COUNT, DOLIT, 0x1f, AND, TYPE, SPACE, EXIT);
	int WORDS = _COLON("WORDS", CR, vCNTX, DOLIT, 0, vTEMP, STORE); {
        _BEGIN(AT, QDUP);
        _WHILE(DUP, SPACE, DOTID, CELLM, vTEMP, AT, DOLIT, 0xa, LESS); {
            _IF(DOLIT, 1, vTEMP, PSTOR);
            _ELSE(CR, DOLIT, 0, vTEMP, STORE);
            _THEN(NOP);
        }
        _REPEAT(EXIT);
    }
	int FORGT = _COLON("FORGET", TOKEN, NAMEQ, QDUP); {
        _IF(CELLM, DUP, vCP, STORE, AT, DUP, vCNTX, STORE, vLAST, STORE, DROP, EXIT);
        _THEN(ERROR);
    }
	int COLD  = _COLON("COLD", CR); {  _DOTQ("eForth in C v4.0"); }
	int DOTQ1 = _LABEL(CR, QUIT);

	// Structure Compiler

	int iTHEN  = _IMMED("THEN",    HERE, SWAP, STORE, EXIT);
    int iFOR   = _IMMED("FOR",     COMPI, TOR, HERE, EXIT);
	int iBEGIN = _IMMED("BEGIN",   HERE, EXIT);
	int iNEXT  = _IMMED("NEXT",    COMPI, DONXT, COMMA, EXIT);
	int iUNTIL = _IMMED("UNTIL",   COMPI, QBRAN, COMMA, EXIT);
	int iAGAIN = _IMMED("AGAIN",   COMPI, BRAN,  COMMA, EXIT);
	int iIF    = _IMMED("IF",      COMPI, QBRAN, HERE, DOLIT, 0, COMMA, EXIT);
	int iAHEAD = _IMMED("AHEAD",   COMPI, BRAN,  HERE, DOLIT, 0, COMMA, EXIT);
	int iREPEA = _IMMED("REPEAT",  iAGAIN, iTHEN, EXIT);
	int iAFT   = _IMMED("AFT",     DROP, iAHEAD, HERE, SWAP, EXIT);
	int iELSE  = _IMMED("ELSE",    iAHEAD, SWAP, iTHEN, EXIT);
	int iWHEN  = _IMMED("WHEN",    iIF, OVER, EXIT);
	int iWHILE = _IMMED("WHILE",   iIF, SWAP, EXIT);
	int iABRTQ = _IMMED("ABORT\"", DOLIT, ABORQP, HERE, STORE, STRCQ, EXIT);
	int iSTRQ  = _IMMED("$\"",     DOLIT, STRQP, HERE, STORE, STRCQ, EXIT);
	int iDOTQQ = _IMMED(".\"",     DOLIT, DOTQP, HERE, STORE, STRCQ, EXIT);

	int CODE   = _COLON("CODE",    TOKEN, SNAME, OVERT, EXIT);
	int CREAT  = _COLON("CREATE",  CODE, DOLIT, 0x203d, COMMA, EXIT);         // CC: hardcoded
	int VARIA  = _COLON("VARIABLE",CREAT, DOLIT, 0, COMMA, EXIT);
	int CONST  = _COLON("CONSTANT",CODE, DOLIT, 0x2004, COMMA, COMMA, EXIT);  // CC: hardcoded
	int iDOTPR = _IMMED(".(",      DOLIT, 0x29, PARSE, TYPE, EXIT);
	int iBKSLA = _IMMED("\\",      DOLIT, 0xa,  WORDD, DROP,  EXIT);
	int iPAREN = _IMMED("(",       DOLIT, 0x29, PARSE, DDROP, EXIT);
	int ONLY   = _COLON("COMPILE-ONLY", DOLIT, 0x40, vLAST, AT, PSTOR, EXIT);
	int IMMED  = _COLON("IMMEDIATE",    DOLIT, 0x80, vLAST, AT, PSTOR, EXIT);
	int ENDD   = P;

	DEBUG("IZ=%04x", P);
    DEBUG(" R=%02x", (_popR() << 2));

	// Boot Up
	P = 0;
	int RESET = _LABEL(as_dolist, COLD);
	P = 0x90;
	int USER  = _LABEL(0x100, 0x10, IMMED - 12, ENDD, IMMED - 12, INTER, QUIT, 0);
}

void dump_data(int len) {
#if ASSEM_DUMP
    for (int p=0; p<len; p+=0x20) {
        printf("\n%04x: ", p);
        for (int i=0; i<0x20; i++) {
        	U8 c = byte[p+i];
            printf("%02x", c);
            printf("%s", (i%4)==3 ? " " : "");
        }
        for (int i=0; i<0x20; i++) {
            U8 c = byte[p+i];
            printf("%c", c ? ((c>32 && c<127) ? c : '_') : '.');
        }
    }
#endif // ASSEM_DUMP
}

int main(int ac, char* av[])
{
    assemble();
	dump_data(0x2000);

	LOG("\n%s\n", "ceForth v4.0");
	R  = S = P = IP = top = 0;
	WP = 4;
	for (;;) {
		primitives[byte[P++]]();
	}
}
/* End of ceforth_33.cpp */

