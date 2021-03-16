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

typedef uint64_t  U64;
typedef uint32_t  U32;
typedef uint16_t  U16;
typedef uint8_t   U8;
typedef int64_t   S64;

typedef int32_t   S32;
typedef int16_t   S16;
typedef int8_t    S8;

// debugging macros
#define LOG(s,v)        printf(s,v)
#define INFO(s)         LOG("%s ", s)
#define SHOWOP(op)      printf("\n%04x: %s\t", P, op)
#define COLON_INFO() {                        \
	printf("\n");                             \
	for (int i=0; i<TAB; i++) printf("  ");   \
    TAB++;                                    \
	printf(":");                              \
}
// stack and logic macros
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
int TAB = 0;                       // debug indentation counter

U32 rack[256]   = { 0 };           // return stack
S32 stack[256]  = { 0 };           // data stack
U32 data[16000] = {};              // 64K forth memory block
U8* byte        = (U8*)data;       // linear byte array pointer

void show_word(int j) {
	U8 *p  = &byte[j];			   // pointer to address
	U32 op = data[j>>2];		   // get opocode
	for (p-=4; *p>31; p-=4);	   // retract pointer to word name

	int  len = (int)*p;
	char buf[64];
	memcpy(buf, p+1, len);
	buf[len] = '\0';
	printf(" %s", buf);
}

// Virtual Forth Machine
void _bye(void)                   // ( -- ) exit to OS
{
	exit(0);
}
void _qrx(void)                   // ( -- c t|f) read a char from terminal input device
{
	_push(getchar());
	if (top) _push(TRUE);
}
void _txsto(void)                 // (c -- ) send a char to console
{
	//putchar((U8)top);
	switch (top) {
	case 0xa: printf("<LF>");  break;
	case 0xd: printf("<CR>");  break;
	case 0x8: printf("<TAB>"); break;
	default:  printf("<%c>", (U8)top);
	}
	_pop();
}
void _next(void)                // advance instruction pointer
{
	P  = data[IP >> 2];			// fetch next address
    show_word(P);
	WP = P + 4;                 // parameter pointer (used optionally)
	IP += 4;
}
void _nop(void)                 // ( -- ) 
{
	_next();
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
	LOG(" %d", v);
	_push(v);
	IP += 4;
    _next();
}
void _dolist(void)              // ( -- ) push instruction pointer onto return stack and pop 
{
	COLON_INFO();
	rack[(U8)++R] = IP;
	IP = WP;
    _next();
}
void _exit(void)               // ( -- ) terminate all token lists in colon words
{
	INFO(" ;");
	TAB--;
	IP = rack[(U8)R--];
    _next();
}
void _execu(void)              // (a -- ) take execution address from data stack and execute the token
{
	P  = top;
	WP = P + 4;
	_pop();
}
void _donext(void)             // ( -- ) terminate a FOR-NEXT loop
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
void _qbran(void)              // (f -- ) test top as a flag on data stack
{
	if (top) IP += 4;
    else     IP = data[IP >> 2];
	_pop();
    _next();
}
void _bran(void)               // ( -- ) branch to address following
{
	IP = data[IP >> 2];
	_next();
}
void _store(void)              // (n a -- ) store into memory location from top of stack
{
	data[top >> 2] = stack[(U8)S--];
	_pop();
}
void _at(void)                 // (a -- n) fetch from memory address onto top of stack
{
	top = data[top >> 2];
}
void _cstor(void)              // (c b -- ) store a byte into memory location
{
	byte[top] = (U8)stack[(U8)S--];
	_pop();
}
void _cat(void)                // (b -- n) fetch a byte from memory location
{
	top = (U32)byte[top];
}
void _rfrom(void)              // (n --) pop from data stack onto return stack
{
	_push(rack[(U8)R--]);
}
void _rat(void)                // (-- n) copy a number off the return stack and push onto data stack
{
	_push(rack[(U8)R]);
}
void _tor(void)                // (-- n) pop from data stack and push onto return stack
{
	rack[(U8)++R] = top;
	_pop();
}
void _drop(void)               // (w -- ) drop top of stack item
{
	_pop();
}
void _dup(void)                // (w -- w w) duplicate to of stack
{
	stack[(U8)++S] = top;
}
void _swap(void)               // (w1 w2 -- w2 w1) swap top two items on the data stack
{
	WP  = top;
	top = stack[(U8)S];
	stack[(U8)S] = WP;
}
void _over(void)               // (w1 w2 -- w1 w2 w1) copy second stack item to top
{
	_push(stack[(U8)S - 1]);
}
void _zless(void)              // (n -- f) check whether top of stack is negative 
{
	top = (top < 0) ? TRUE : FALSE;
}
void _and(void)                // (w w -- w) bitwise AND
{
	top &= stack[(U8)S--];
}
void _or(void)                 // (w w -- w) bitwise OR
{
	top |= stack[(U8)S--];
}
void _xor(void)                // (w w -- w) bitwise XOR
{
	top ^= stack[(U8)S--];
}
void _uplus(void)              // (w w -- w c) add two numbers, return the sum and carry flag
{
	stack[(U8)S] += top;
	top = (U32)stack[(U8)S] < (U32)top;
}
void _qdup(void)               // (w -- w w | 0) dup top of stack if it is not zero
{
	if (top) stack[(U8) ++S] = top;
}
void _rot(void)                // (w1 w2 w3 -- w2 w3 w1) rotate 3rd item to top
{
	WP = stack[(U8)S - 1];
	stack[(U8)S - 1] = stack[(U8)S];
	stack[(U8)S] = top;
	top = WP;
}
void _ddrop(void)              // (w w --) drop top two items
{
	_drop();
	_drop();
}
void _ddup(void)               // (w1 w2 -- w1 w2 w1 w2) duplicate top two items
{
	_over();
	_over();
}
void _plus(void)               // (w w -- sum) add top two items
{
	top += stack[(U8)S--];
}
void _inver(void)              // (w -- w) one's complement
{
	top = -top - 1;
}
void _negat(void)              // (n -- -n) two's complement
{
	top = 0 - top;
}
void _dnega(void)              // (d -- -d) two's complement of top double
{
	_inver();
	_tor();
	_inver();
	_push(1);
	_uplus();
	_rfrom();
	_plus();
}
void _sub(void)                // (n1 n2 -- n1-n2) subtraction
{
	top = stack[(U8)S--] - top;
}
void _abs(void)                // (n -- n) absolute value of n
{
	if (top < 0) top = -top;
}
void _great(void)              // (n1 n2 -- t) true if n1>n2
{
	top = (stack[(U8)S--] > top) ? TRUE : FALSE;
}
void _less(void)               // (n1 n2 -- t) true if n1<n2
{
	top = (stack[(U8)S--] < top) ? TRUE : FALSE;
}
void _equal(void)              // (w w -- t) true if top two items are equal
{
	top = (stack[(U8)S--]==top) ? TRUE : FALSE;
}
void _uless(void)              // (u1 u2 -- t) unsigned compare top two items
{
	top = ((U32)(stack[(U8)S--]) < (U32)top) ? TRUE : FALSE;
}
void _ummod(void)              // (udl udh u -- ur uq) unsigned divide of a double by single
{
	S64 d = (S64)top;
	S64 m = (S64)((U32)stack[(U8)S]);
	S64 n = (S64)((U32)stack[(U8)S - 1]);
	n += m << 32;
	_pop();
	top = (U32)(n / d);
	stack[(U8)S] = (U32)(n % d);
}
void _msmod(void)              // (d n -- r q) signed floored divide of double by single 
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n += m << 32;
	_pop();
	top = (S32)(n / d);           // mod
	stack[(U8)S] = (U32)(n % d);  // quotien
}
void _slmod(void)              // (n1 n2 -- r q) signed devide, return mod and quotien
{
	if (top) {
		WP  = stack[(U8)S] / top;
		stack[(U8)S] %= top;
		top = WP;
	}
}
void _mod(void)                // (n n -- r) signed divide, returns mod
{
	top = (top) ? stack[(U8)S--] % top : stack[(U8)S--];
}
void _slash(void)              // (n n - q) signed divide, return quotient
{
	top = (top) ? stack[(U8)S--] / top : (stack[(U8)S--], 0);
}
void _umsta(void)              // (u1 u2 -- ud) unsigned multiply return double product
{
	U64 d = (U64)top;
	U64 m = (U64)stack[(U8)S];
	m *= d;
	top = (U32)(m >> 32);
	stack[(U8)S] = (U32)m;
}
void _star(void)               // (n n -- n) signed multiply, return single product
{
	top *= stack[(U8)S--];
}
void _mstar(void)              // (n1 n2 -- d) signed multiply, return double product
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	m *= d;
	top = (S32)(m >> 32);
	stack[(U8)S] = (S32)m;
}
void _ssmod(void)              // (n1 n2 n3 -- r q) n1*n2/n3, return mod and quotion
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n *= m;
	_pop();
	top = (S32)(n / d);
	stack[(U8)S] = (S32)(n % d);
}
void _stasl(void)              // (n1 n2 n3 -- q) n1*n2/n3 return quotient
{
	S64 d = (S64)top;
	S64 m = (S64)stack[(U8)S];
	S64 n = (S64)stack[(U8)S - 1];
	n *= m;
	_pop();
    _pop();
	top = (S32)(n / d);
}
void _pick(void)               // (... +n -- ...w) copy nth stack item to top
{
	top = stack[(U8)S - (U8)top];
}
void _pstor(void)              // (n a -- ) add n to content at address a
{
	data[top >> 2] += stack[(U8)S--];
    _pop();
}
void _dstor(void)              // (d a -- ) store the double to address a
{
	data[(top >> 2) + 1] = stack[(U8)S--];
	data[top >> 2]       = stack[(U8)S--];
	_pop();
}
void _dat(void)                // (a -- d) fetch double from address a
{
	_push(data[top >> 2]);
	top = data[(top >> 2) + 1];
}
void _count(void)              // (b -- b+1 +n) count byte of a string and add 1 to byte address
{
	stack[(U8)++S] = top + 1;
	top = byte[top];
}
void _max(void)                // (n1 n2 -- n) return greater of two top stack items
{
	if (top < stack[(U8)S]) _pop();
	else (U8)S--;
}
void _min(void)                // (n1 n2 -- n) return smaller of two top stack items
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

// Macro Assembler
#define FLAG_IMEDD  0x80              // immediate flag
#define FLAG_COMPO  0x40              // composit flag
int BRAN = 0, QBRAN = 0, DONXT = 0, DOTQP = 0, STRQP = 0, TOR = 0, ABORQP = 0;

void _header(int lex, const char *seq) {
	IP = P >> 2;
	U32 len = lex & 0x1f;                     // max length 31
	data[IP++] = thread;                      // point to previous word

	// dump memory between previous word and this
	LOG("%s", "\n    :");
	for (U32 i = thread>>2; thread && i < IP; i++) {
		LOG(" %08x", data[i]);
	}
	LOG("%c", '\n');

	P = IP << 2;
	thread = P;                               // keep pointer to this word
	byte[P++] = lex;                         // length of word
	for (U32 i = 0; i < len; i++) {           // memcpy word string
		byte[P++] = seq[i];
	}
	while (P & 3) { byte[P++] = 0xff; }      // padding 4-byte align
	LOG("%04x: ", P);
	LOG("%s", seq);
}
int _CODE(const char *seg, int len, ...) {
    _header(strlen(seg), seg);
	int addr = P;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		U8 j = (U8)va_arg(argList, int);
		byte[P++] = j;
		LOG(" %02x", j);
	}
	va_end(argList);
	return addr;
}
#define DATACPY(n) {                  \
	va_list argList;                  \
	va_start(argList, n);             \
	for (; n; n--) {                  \
		U32 j = va_arg(argList, U32); \
		data[IP++] = j;               \
		LOG(" %04x", j);           \
	}                                 \
	va_end(argList);                  \
}
int _COLON(const char *seg, int len, ...) {
    _header(strlen(seg), seg);
	LOG("%s", " COLON 0006");
	int addr = P;
	IP = P >> 2;
	data[IP++] = 6; // dolist
    DATACPY(len);
	P = IP << 2;
	return addr;
}
int _IMMEDIATE(const char *seg, int len, ...) {
    _header(FLAG_IMEDD | strlen(seg), seg);
	LOG("%s", " COLON 0006");
	int addr = P;
	IP = P >> 2;
	data[IP++] = 6; // dolist
    DATACPY(len);
	P = IP << 2;
	return addr;
}
int _LABEL(int len, ...) {
	SHOWOP("LABEL");
	int addr = P;
	IP = P >> 2;
    DATACPY(len);
	P = IP << 2;
	return addr;
}
void _BEGIN(int len, ...) {
	SHOWOP("BEGIN");
	IP = P >> 2;
	_pushR(IP);
    DATACPY(len);
	P = IP << 2;
}
void _AGAIN(int len, ...) {
	SHOWOP("AGAIN");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = _popR() << 2;
    DATACPY(len);
	P = IP << 2;
}
void _UNTIL(int len, ...) {
	SHOWOP("UNTIL");
	IP = P >> 2;
	data[IP++] = QBRAN;
	data[IP++] = _popR() << 2;
    DATACPY(len);
	P = IP << 2;
}
void _WHILE(int len, ...) {
	SHOWOP("WHILE");
	IP = P >> 2;
	data[IP++] = QBRAN;
	data[IP++] = 0;
	int k = _popR();
	_pushR(IP - 1);
	_pushR(k);
    DATACPY(len);
	P = IP << 2;
}
void _REPEAT(int len, ...) {
	SHOWOP("REPEAT");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = _popR() << 2;
	data[_popR()] = IP << 2;
    DATACPY(len);
	P = IP << 2;
}
void _IF(int len, ...) {
	SHOWOP("IF");
	IP = P >> 2;
	data[IP++] = QBRAN;
	_pushR(IP);
	data[IP++] = 0;
    DATACPY(len);
	P = IP << 2;
}
void _ELSE(int len, ...) {
	SHOWOP("ELSE");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = 0;
	data[_popR()] = IP << 2;
	_pushR(IP - 1);
    DATACPY(len);
	P = IP << 2;
}
void _THEN(int len, ...) {
	SHOWOP("THEN");
	IP = P >> 2;
	data[_popR()] = IP << 2;
    DATACPY(len);
	P = IP << 2;
}
void _FOR(int len, ...) {
	SHOWOP("FOR");
	IP = P >> 2;
	data[IP++] = TOR;
	_pushR(IP);
    DATACPY(len);
	P = IP << 2;
}
void _NEXT(int len, ...) {
	SHOWOP("NEXT");
	IP = P >> 2;
	data[IP++] = DONXT;
	data[IP++] = _popR() << 2;
    DATACPY(len);
	P = IP << 2;
}
void _AFT(int len, ...) {
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
	LOG("%s", seq);
	STRCPY(DOTQP, seq);
}
void _STRQ(const char *seq) {
	SHOWOP("STRQ");
	LOG("%s", seq);
	STRCPY(STRQP, seq);
}
void _ABORQ(const char *seq) {
	SHOWOP("ABORQP");
	LOG("%s", seq);
	STRCPY(ABORQP, seq);
}

void dump_data(int len) {
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
}

// Byte Code Assembler (opcode)
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

int main(int ac, char* av[])
{
	P = 0x200;
	R = thread = 0;

	// Kernel
	int HLD   = _CODE("HLD",     8, as_docon, as_next, 0, 0, 0x80, 0, 0, 0);
	int SPAN  = _CODE("SPAN",    8, as_docon, as_next, 0, 0, 0x84, 0, 0, 0);
	int INN   = _CODE(">IN",     8, as_docon, as_next, 0, 0, 0x88, 0, 0, 0);
	int NTIB  = _CODE("#TIB",    8, as_docon, as_next, 0, 0, 0x8c, 0, 0, 0);
	int TTIB  = _CODE("'TIB",    8, as_docon, as_next, 0, 0, 0x90, 0, 0, 0);
	int BASE  = _CODE("BASE",    8, as_docon, as_next, 0, 0, 0x94, 0, 0, 0);
	int CNTXT = _CODE("CONTEXT", 8, as_docon, as_next, 0, 0, 0x98, 0, 0, 0);
	int CP    = _CODE("CP",      8, as_docon, as_next, 0, 0, 0x9c, 0, 0, 0);
	int LAST  = _CODE("LAST",    8, as_docon, as_next, 0, 0, 0xa0, 0, 0, 0);
	int TEVAL = _CODE("'EVAL",   8, as_docon, as_next, 0, 0, 0xa4, 0, 0, 0);
	int TABRT = _CODE("'ABORT",  8, as_docon, as_next, 0, 0, 0xa8, 0, 0, 0);
	int TEMP  = _CODE("tmp",     8, as_docon, as_next, 0, 0, 0xac, 0, 0, 0);

	int NOP   = _CODE("NOP",     4, as_next,  0,       0, 0);
	int BYE   = _CODE("BYE",     4, as_bye,   as_next, 0, 0);
	int QRX   = _CODE("?RX",     4, as_qrx,   as_next, 0, 0);
	int TXSTO = _CODE("TX!",     4, as_txsto, as_next, 0, 0);
	int DOCON = _CODE("DOCON",   4, as_docon, as_next, 0, 0);
	int DOLIT = _CODE("DOLIT",   4, as_dolit, as_next, 0, 0);
	int DOLST = _CODE("DOLIST",  4, as_dolist,as_next, 0, 0);
	int EXIT  = _CODE("EXIT",    4, as_exit,  as_next, 0, 0);
	int EXECU = _CODE("EXECUTE", 4, as_execu, as_next, 0, 0);

	DONXT     = _CODE("DONEXT",  4, as_donext,as_next, 0, 0);
	QBRAN     = _CODE("QBRANCH", 4, as_qbran, as_next, 0, 0);
	BRAN      = _CODE("BRANCH",  4, as_bran,  as_next, 0, 0);

	int STORE = _CODE("!",       4, as_store, as_next, 0, 0);
	int AT    = _CODE("@",       4, as_at,    as_next, 0, 0);
	int CSTOR = _CODE("C!",      4, as_cstor, as_next, 0, 0);
	int CAT   = _CODE("C@",      4, as_cat,   as_next, 0, 0);
	int RFROM = _CODE("R>",      4, as_rfrom, as_next, 0, 0);
	int RAT   = _CODE("R@",      4, as_rat,   as_next, 0, 0);
	TOR       = _CODE(">R",      4, as_tor,   as_next, 0, 0);
	int DROP  = _CODE("DROP",    4, as_drop,  as_next, 0, 0);
	int DUP   = _CODE("DUP",     4, as_dup,   as_next, 0, 0);
	int SWAP  = _CODE("SWAP",    4, as_swap,  as_next, 0, 0);
	int OVER  = _CODE("OVER",    4, as_over,  as_next, 0, 0);
	int ZLESS = _CODE("0<",      4, as_zless, as_next, 0, 0);
	int AND   = _CODE("AND",     4, as_and,   as_next, 0, 0);
	int OR    = _CODE("OR",      4, as_or,    as_next, 0, 0);
	int XOR   = _CODE("XOR",     4, as_xor,   as_next, 0, 0);
	int UPLUS = _CODE("UM+",     4, as_uplus, as_next, 0, 0);
	int NEXT  = _CODE("NEXT",    4, as_next,  as_next, 0, 0);
	int QDUP  = _CODE("?DUP",    4, as_qdup,  as_next, 0, 0);
	int ROT   = _CODE("ROT",     4, as_rot,   as_next, 0, 0);
	int DDROP = _CODE("2DROP",   4, as_ddrop, as_next, 0, 0);
	int DDUP  = _CODE("2DUP",    4, as_ddup,  as_next, 0, 0);
	int PLUS  = _CODE("+",       4, as_plus,  as_next, 0, 0);
	int INVER = _CODE("NOT",     4, as_inver, as_next, 0, 0);
	int NEGAT = _CODE("NEGATE",  4, as_negat, as_next, 0, 0);
	int DNEGA = _CODE("DNEGATE", 4, as_dnega, as_next, 0, 0);
	int SUB   = _CODE("-",       4, as_sub,   as_next, 0, 0);
	int ABS   = _CODE("ABS",     4, as_abs,   as_next, 0, 0);
	int EQUAL = _CODE("=",       4, as_equal, as_next, 0, 0);
	int ULESS = _CODE("U<",      4, as_uless, as_next, 0, 0);
	int LESS  = _CODE("<",       4, as_less,  as_next, 0, 0);
	int UMMOD = _CODE("UM/MOD",  4, as_ummod, as_next, 0, 0);
	int MSMOD = _CODE("M/MOD",   4, as_msmod, as_next, 0, 0);
	int SLMOD = _CODE("/MOD",    4, as_slmod, as_next, 0, 0);
	int MOD   = _CODE("MOD",     4, as_mod,   as_next, 0, 0);
	int SLASH = _CODE("/",       4, as_slash, as_next, 0, 0);
	int UMSTA = _CODE("UM*",     4, as_umsta, as_next, 0, 0);
	int STAR  = _CODE("*",       4, as_star,  as_next, 0, 0);
	int MSTAR = _CODE("M*",      4, as_mstar, as_next, 0, 0);
	int SSMOD = _CODE("*/MOD",   4, as_ssmod, as_next, 0, 0);
	int STASL = _CODE("*/",      4, as_stasl, as_next, 0, 0);
	int PICK  = _CODE("PICK",    4, as_pick,  as_next, 0, 0);
	int PSTOR = _CODE("+!",      4, as_pstor, as_next, 0, 0);
	int DSTOR = _CODE("2!",      4, as_dstor, as_next, 0, 0);
	int DAT   = _CODE("2@",      4, as_dat,   as_next, 0, 0);
	int COUNT = _CODE("COUNT",   4, as_count, as_next, 0, 0);
	int MAX   = _CODE("MAX",     4, as_max,   as_next, 0, 0);
	int MIN   = _CODE("MIN",     4, as_min,   as_next, 0, 0);
    
	int BLANK = _CODE("BL",      8, as_docon, as_next, 0,       0, 32, 0, 0, 0);
	int CELL  = _CODE("CELL",    8, as_docon, as_next, 0,       0,  4, 0, 0, 0);
	int CELLP = _CODE("CELL+",   8, as_docon, as_plus, as_next, 0,  4, 0, 0, 0);
	int CELLM = _CODE("CELL-",   8, as_docon, as_sub,  as_next, 0,  4, 0, 0, 0);
	int CELLS = _CODE("CELLS",   8, as_docon, as_star, as_next, 0,  4, 0, 0, 0);
	int CELLD = _CODE("CELL/",   8, as_docon, as_slash,as_next, 0,  4, 0, 0, 0);
	int ONEP  = _CODE("1+",      8, as_docon, as_plus, as_next, 0,  1, 0, 0, 0);
	int ONEM  = _CODE("1-",      8, as_docon, as_sub,  as_next, 0,  1, 0, 0, 0);
	int DOVAR = _CODE("DOVAR",   4, as_dovar, as_next, 0, 0);

	// Common Colon Words

	int QKEY  = _COLON("?KEY",    2, QRX, EXIT);
	int KEY   = _COLON("KEY",     0); {
        _BEGIN(1, QKEY);
        _UNTIL(1, EXIT);
    }
	int EMIT  = _COLON("EMIT",    2, TXSTO, EXIT);
	int WITHI = _COLON("WITHIN",  7, OVER, SUB, TOR, SUB, RFROM, ULESS, EXIT);
	int TCHAR = _COLON(">CHAR",   8, DOLIT, 0x7f, AND, DUP, DOLIT, 0x7f, BLANK, WITHI); {
        _IF(3, DROP, DOLIT, 0x5f);
        _THEN(1, EXIT);
    }
	int ALIGN = _COLON("ALIGNED", 7, DOLIT, 3, PLUS, DOLIT, 0xfffffffc, AND, EXIT);
	int HERE  = _COLON("HERE",    3, CP, AT, EXIT);
	int PAD   = _COLON("PAD",     5, HERE, DOLIT, 0x50, PLUS, EXIT);
	int TIB   = _COLON("TIB",     3, TTIB, AT, EXIT);
	int ATEXE = _COLON("@EXECUTE",2, AT, QDUP); {
        _IF(1, EXECU);
        _THEN(1, EXIT);
    }
    int CMOVE = _COLON("CMOVE", 0); {
        _FOR(0);
        _AFT(8, OVER, CAT, OVER, CSTOR, TOR, ONEP, RFROM, ONEP);
        _THEN(0);
        _NEXT(2, DDROP, EXIT);
    }
	int MOVE  = _COLON("MOVE", 1, CELLD); {
        _FOR(0);
        _AFT(8, OVER, AT, OVER, STORE, TOR, CELLP, RFROM, CELLP);
        _THEN(0);
        _NEXT(2, DDROP, EXIT);
    }
	int FILL = _COLON("FILL", 1, SWAP); {
        _FOR(1, SWAP);
        _AFT(3, DDUP, CSTOR, ONEP);
        _THEN(0);
        _NEXT(2, DDROP, EXIT);
    }

	// Number Conversions

	int DIGIT = _COLON("DIGIT",   12, DOLIT, 9, OVER, LESS, DOLIT, 7, AND, PLUS, DOLIT, 0x30, PLUS, EXIT);
	int EXTRC = _COLON("EXTRACT",  7, DOLIT, 0, SWAP, UMMOD, SWAP, DIGIT, EXIT);
	int BDIGS = _COLON("<#",       4, PAD, HLD, STORE, EXIT);
	int HOLD  = _COLON("HOLD",     8, HLD, AT, ONEM, DUP, HLD, STORE, CSTOR, EXIT);
	int DIG   = _COLON("#",        5, BASE, AT, EXTRC, HOLD, EXIT);
	int DIGS  = _COLON("#S", 0); {
        _BEGIN(2, DIG, DUP);
        _WHILE(0);
        _REPEAT(1, EXIT);
    }
	int SIGN  = _COLON("SIGN",     1, ZLESS); {
        _IF(3, DOLIT, 0x2d, HOLD);
        _THEN(1, EXIT);
    }
	int EDIGS = _COLON("#>",      7, DROP, HLD, AT, PAD, OVER, SUB, EXIT);
	int STRR  = _COLON("str",     9, DUP, TOR, ABS, BDIGS, DIGS, RFROM, SIGN, EDIGS, EXIT);
	int HEXX  = _COLON("HEX",     5, DOLIT, 16, BASE, STORE, EXIT);
	int DECIM = _COLON("DECIMAL", 5, DOLIT, 10, BASE, STORE, EXIT);
	int UPPER = _COLON("wupper",  4, DOLIT, 0x5f5f5f5f, AND, EXIT);
	int TOUPP = _COLON(">upper",  6, DUP, DOLIT, 0x61, DOLIT, 0x7b, WITHI); {
        _IF(3, DOLIT, 0x5f, AND);
        _THEN(1, EXIT);
    }
	int DIGTQ = _COLON("DIGIT?",  9, TOR, TOUPP, DOLIT, 0x30, SUB, DOLIT, 9, OVER, LESS); {
        _IF(8, DOLIT, 7, SUB, DUP, DOLIT, 10, LESS, OR);
        _THEN(4, DUP, RFROM, ULESS, EXIT);
    }
	int NUMBQ = _COLON("NUMBER?", 12, BASE, AT, TOR, DOLIT, 0, OVER, COUNT, OVER, CAT, DOLIT, 0x24, EQUAL); {
        _IF(5, HEXX, SWAP, ONEP, SWAP, ONEM);
        _THEN(13, OVER, CAT, DOLIT, 0x2d, EQUAL, TOR, SWAP, RAT, SUB, SWAP, RAT, PLUS, QDUP); {
            _IF(1, ONEM); {
                _FOR(6, DUP, TOR, CAT, BASE, AT, DIGTQ);
                _WHILE(7, SWAP, BASE, AT, STAR, PLUS, RFROM, ONEP);
                _NEXT(2, DROP, RAT);
                _IF(1, NEGAT);
                _THEN(1, SWAP);
            }
            _ELSE(6, RFROM, RFROM, DDROP, DDROP, DOLIT, 0);
            _THEN(1, DUP);
        }
        _THEN(6, RFROM, DDROP, RFROM, BASE, STORE, EXIT);
    }

	// Terminal Output

	int SPACE = _COLON("SPACE", 3, BLANK, EMIT, EXIT);
	int CHARS = _COLON("CHARS", 4, SWAP, DOLIT, 0, MAX); {
        _FOR(0);
        _AFT(2, DUP, EMIT);
        _THEN(0);
        _NEXT(2, DROP, EXIT);
    }
	int SPACS = _COLON("SPACES", 3, BLANK, CHARS, EXIT);
	int TYPES = _COLON("TYPE",   0); {
        _FOR(0);
        _AFT(3, COUNT, TCHAR, EMIT);
        _THEN(0);
        _NEXT(2, DROP, EXIT);
    }
	int CR = _COLON("CR", 7, DOLIT, 10, DOLIT, 13, EMIT, EMIT, EXIT);
	int DOSTR = _COLON("do$",   10, RFROM, RAT, RFROM, COUNT, PLUS, ALIGN, TOR, SWAP, TOR, EXIT);
	int STRQP = _COLON("$\"|",   2, DOSTR, EXIT);
	DOTQP     = _COLON(".\"|",   4, DOSTR, COUNT, TYPES, EXIT);
	int DOTR  = _COLON(".R",     8, TOR, STRR, RFROM, OVER, SUB, SPACS, TYPES, EXIT);
	int UDOTR = _COLON("U.R",   10, TOR, BDIGS, DIGS, EDIGS, RFROM, OVER, SUB, SPACS, TYPES, EXIT);
	int UDOT  = _COLON("U.",     6, BDIGS, DIGS, EDIGS, SPACE, TYPES, EXIT);
	int DOT   = _COLON(".",      5, BASE, AT, DOLIT, 0xa, XOR); {
        _IF(2, UDOT, EXIT);
        _THEN(4, STRR, SPACE, TYPES, EXIT);
    }
	int QUEST = _COLON("?",      3, AT, DOT, EXIT);

	// Parser

	int PARS  = _COLON("(parse)",5, TEMP, CSTOR, OVER, TOR, DUP); {
        _IF(5, ONEM, TEMP, CAT, BLANK, EQUAL); {
            _IF(0); {
                _FOR(6, BLANK, OVER, CAT, SUB, ZLESS, INVER);
                _WHILE(1, ONEP);
                _NEXT(6, RFROM, DROP, DOLIT, 0, DUP, EXIT);
                _THEN(1, RFROM);
            }
            _THEN(2, OVER, SWAP); {
                _FOR(9, TEMP, CAT, OVER, CAT, SUB, TEMP, CAT, BLANK, EQUAL); {
                    _IF(1, ZLESS);
                    _THEN(0);
                }
                _WHILE(1, ONEP);
                _NEXT(2, DUP, TOR);
            }
        }
        _ELSE(5, RFROM, DROP, DUP, ONEP, TOR);
        _THEN(6, OVER, SUB, RFROM, RFROM, SUB, EXIT);
        _THEN(4, OVER, RFROM, SUB, EXIT);                   // CC: this line is questionable
    }
	int PACKS = _COLON("PACK$", 18, DUP, TOR, DDUP, PLUS, DOLIT, 0xfffffffc, AND, DOLIT, 0, SWAP, STORE, DDUP, CSTOR, ONEP, SWAP, CMOVE, RFROM, EXIT);
	int PARSE = _COLON("PARSE", 15, TOR, TIB, INN, AT, PLUS, NTIB, AT, INN, AT, SUB, RFROM, PARS, INN, PSTOR, EXIT);
	int TOKEN = _COLON("TOKEN",  9, BLANK, PARSE, DOLIT, 0x1f, MIN, HERE, CELLP, PACKS, EXIT);
	int WORDD = _COLON("WORD",   5, PARSE, HERE, CELLP, PACKS, EXIT);
	int NAMET = _COLON("NAME>",  7, COUNT, DOLIT, 0x1f, AND, PLUS, ALIGN, EXIT);
	int SAMEQ = _COLON("SAME?",  4, DOLIT, 0x1f, AND, CELLD); {
        _FOR(0);
        _AFT(14, OVER, RAT, CELLS, PLUS, AT, UPPER, OVER, RAT, CELLS, PLUS, AT, UPPER, SUB, QDUP); {
            _IF(3, RFROM, DROP, EXIT);
            _THEN(0);
        }
        _THEN(0);
        _NEXT(3, DOLIT, 0, EXIT);
    }
	int FIND  = _COLON("find",  10, SWAP, DUP, AT, TEMP, STORE, DUP, AT, TOR, CELLP, SWAP); {
        _BEGIN(2, AT, DUP); {
            _IF(9, DUP, AT, DOLIT, 0xffffff3f, AND, UPPER, RAT, UPPER, XOR); {
                _IF(3, CELLP, DOLIT, 0xffffffff);
                _ELSE(4, CELLP, TEMP, AT, SAMEQ);
                _THEN(0);
            }
            _ELSE(6, RFROM, DROP, SWAP, CELLM, SWAP, EXIT);
            _THEN(0);
        }
        _WHILE(2, CELLM, CELLM);
        _REPEAT(9, RFROM, DROP, SWAP, DROP, CELLM, DUP, NAMET, SWAP, EXIT);
    }
	int NAMEQ = _COLON("NAME?", 3, CNTXT, FIND, EXIT);

	// Terminal Input

	int HATH  = _COLON("^H",    6, TOR, OVER, RFROM, SWAP, OVER, XOR); {
        _IF(9, DOLIT, 8, EMIT, ONEM, BLANK, EMIT, DOLIT, 8, EMIT);
        _THEN(1, EXIT);
    }
	int TAP   = _COLON("TAP",   6, DUP, EMIT, OVER, CSTOR, ONEP, EXIT);
	int KTAP  = _COLON("kTAP",  9, DUP, DOLIT, 0xd, XOR, OVER, DOLIT, 0xa, XOR, AND); {
        _IF(3, DOLIT, 8, XOR); {
            _IF(2, BLANK, TAP);
            _ELSE(1, HATH);
            _THEN(1, EXIT);
        }
        _THEN(5, DROP, SWAP, DROP, DUP, EXIT);
    }
	int ACCEP = _COLON("ACCEPT", 3, OVER, PLUS, OVER); {
        _BEGIN(2, DDUP, XOR);
        _WHILE(7, KEY, DUP, BLANK, SUB, DOLIT, 0x5f, ULESS); {
            _IF(1, TAP);
            _ELSE(1, KTAP);
            _THEN(0);
        }
        _REPEAT(4, DROP, OVER, SUB, EXIT);
    }
	int EXPEC = _COLON("EXPECT",  5, ACCEP, SPAN, STORE, DROP, EXIT);
	int QUERY = _COLON("QUERY",  12, TIB, DOLIT, 0x50, ACCEP, NTIB, STORE, DROP, DOLIT, 0, INN, STORE, EXIT);

	// Text Interpreter

	int ABORT = _COLON("ABORT", 2, TABRT, ATEXE);
	ABORQP = _COLON("abort\"",  0); {
        _IF(4, DOSTR, COUNT, TYPES, ABORT);
        _THEN(3, DOSTR, DROP, EXIT);
    }
	int ERRORR= _COLON("ERROR", 11, SPACE, COUNT, TYPES, DOLIT, 0x3f, EMIT, DOLIT, 0x1b, EMIT, CR, ABORT);
	int INTER = _COLON("$INTERPRET", 2, NAMEQ, QDUP); {
        _IF(4, CAT, DOLIT, FLAG_COMPO, AND);
        _ABORQ(" compile only");
    }
	int INTER0= _LABEL(2, EXECU, EXIT); {
        _THEN(1, NUMBQ);
        _IF(1, EXIT);
        _ELSE(1, ERRORR);
        _THEN(0);
    }
	int LBRAC = _IMMEDIATE("[", 5, DOLIT, INTER, TEVAL, STORE, EXIT);
	int DOTOK = _COLON(".OK",   6, CR, DOLIT, INTER, TEVAL, AT, EQUAL); {
        _IF(14, TOR, TOR, TOR, DUP, DOT, RFROM, DUP, DOT, RFROM, DUP, DOT, RFROM, DUP, DOT); {
            _DOTQ(" ok>");
        }
        _THEN(1, EXIT);
    }
	int EVAL  = _COLON("EVAL", 0); {
        _BEGIN(3, TOKEN, DUP, AT);
        _WHILE(2, TEVAL, ATEXE);
        _REPEAT(3, DROP, DOTOK, EXIT);
    }
	int QUITT = _COLON("QUIT", 5, DOLIT, 0x100, TTIB, STORE, LBRAC); {
        _BEGIN(2, QUERY, EVAL);
        _AGAIN(0);
    }

	// Colon Word Compiler

	int COMMA = _COLON(",", 7, HERE, DUP, CELLP, CP, STORE, STORE, EXIT);
	int LITER = _IMMEDIATE("LITERAL", 5, DOLIT, DOLIT, COMMA, COMMA, EXIT);
	int ALLOT = _COLON("ALLOT", 4, ALIGN, CP, PSTOR, EXIT);
	int STRCQ = _COLON("$,\"",  9, DOLIT, 0x22, WORDD, COUNT, PLUS, ALIGN, CP, STORE, EXIT);
	int UNIQU = _COLON("?UNIQUE", 3, DUP, NAMEQ, QDUP); {
        _IF(6, COUNT, DOLIT, 0x1f, AND, SPACE, TYPES); {
            _DOTQ(" reDef");
        }
        _THEN(2, DROP, EXIT);
    }
	int SNAME = _COLON("$,n", 2, DUP, AT); {
        _IF(14, UNIQU, DUP, NAMET, CP, STORE, DUP, LAST, STORE, CELLM, CNTXT, AT, SWAP, STORE, EXIT);
        _THEN(1, ERRORR);
    }
	int TICK  = _COLON("'", 2, TOKEN, NAMEQ); {
        _IF(1, EXIT);
        _THEN(1, ERRORR);
    }
	int BCOMP = _IMMEDIATE("[COMPILE]", 3, TICK, COMMA, EXIT);
	int COMPI = _COLON("COMPILE",  7, RFROM, DUP, AT, COMMA, CELLP, TOR, EXIT);
	int SCOMP = _COLON("$COMPILE", 2, NAMEQ, QDUP); {
        _IF(4, AT, DOLIT, FLAG_IMEDD, AND); {
            _IF(1, EXECU);
            _ELSE(1, COMMA);
            _THEN(1, EXIT);
        }
        _THEN(1, NUMBQ);
        _IF(2, LITER, EXIT);
        _THEN(1, ERRORR);
    }
	int OVERT = _COLON("OVERT", 5, LAST, AT, CNTXT, STORE, EXIT);
	int RBRAC = _COLON("]",     5, DOLIT, SCOMP, TEVAL, STORE, EXIT);
	int COLN  = _COLON(":",     7, TOKEN, SNAME, RBRAC, DOLIT, 0x6, COMMA, EXIT);
	int SEMIS = _IMMEDIATE(";", 6, DOLIT, EXIT, COMMA, LBRAC, OVERT, EXIT);

	// Debugging Tools

	int DMP   = _COLON("dm+",   4, OVER, DOLIT, 6, UDOTR); {
        _FOR(0);
        _AFT(6, DUP, AT, DOLIT, 9, UDOTR, CELLP);
        _THEN(0);
        _NEXT(1, EXIT);
    }
	int DUMP  = _COLON("DUMP", 10, BASE, AT, TOR, HEXX, DOLIT, 0x1f, PLUS, DOLIT, 0x20, SLASH); {
        _FOR(0);
        _AFT(10, CR, DOLIT, 8, DDUP, DMP, TOR, SPACE, CELLS, TYPES, RFROM);
        _THEN(0);
        _NEXT(5, DROP, RFROM, BASE, STORE, EXIT);
    }
	int TNAME = _COLON(">NAME", 1, CNTXT); {
        _BEGIN(2, AT, DUP);
        _WHILE(3, DDUP, NAMET, XOR); {
            _IF(1, ONEM);
            _ELSE(3, SWAP, DROP, EXIT);
            _THEN(0);
        }
        _REPEAT(3, SWAP, DROP, EXIT);
    }
	int DOTID = _COLON(".ID",   7, COUNT, DOLIT, 0x1f, AND, TYPES, SPACE, EXIT);
	int WORDS = _COLON("WORDS", 6, CR, CNTXT, DOLIT, 0, TEMP, STORE); {
        _BEGIN(2, AT, QDUP);
        _WHILE(9, DUP, SPACE, DOTID, CELLM, TEMP, AT, DOLIT, 0xa, LESS); {
            _IF(4, DOLIT, 1, TEMP, PSTOR);
            _ELSE(5, CR, DOLIT, 0, TEMP, STORE);
            _THEN(0);
        }
        _REPEAT(1, EXIT);
    }
	int FORGT = _COLON("FORGET", 3, TOKEN, NAMEQ, QDUP); {
        _IF(12, CELLM, DUP, CP, STORE, AT, DUP, CNTXT, STORE, LAST, STORE, DROP, EXIT);
        _THEN(1, ERRORR);
    }
	int COLD  = _COLON("COLD", 1, CR); {
        _DOTQ("eForth in C v4.0");
    }
	int DOTQ1 = _LABEL(2, CR, QUITT);

	// Structure Compiler

	int iTHEN  = _IMMEDIATE("THEN",    4, HERE, SWAP, STORE, EXIT);
    int iFOR   = _IMMEDIATE("FOR",     4, COMPI, TOR, HERE, EXIT);
	int iBEGIN = _IMMEDIATE("BEGIN",   2, HERE, EXIT);
	int iNEXT  = _IMMEDIATE("NEXT",    4, COMPI, DONXT, COMMA, EXIT);
	int iUNTIL = _IMMEDIATE("UNTIL",   4, COMPI, QBRAN, COMMA, EXIT);
	int iAGAIN = _IMMEDIATE("AGAIN",   4, COMPI, BRAN, COMMA, EXIT);
	int iIF    = _IMMEDIATE("IF",      7, COMPI, QBRAN, HERE, DOLIT, 0, COMMA, EXIT);
	int iAHEAD = _IMMEDIATE("AHEAD",   7, COMPI, BRAN, HERE, DOLIT, 0, COMMA, EXIT);
	int iREPEA = _IMMEDIATE("REPEAT",  3, iAGAIN, iTHEN, EXIT);
	int iAFT   = _IMMEDIATE("AFT",     5, DROP, iAHEAD, HERE, SWAP, EXIT);
	int iELSE  = _IMMEDIATE("ELSE",    4, iAHEAD, SWAP, iTHEN, EXIT);
	int iWHEN  = _IMMEDIATE("WHEN",    3, iIF, OVER, EXIT);
	int iWHILE = _IMMEDIATE("WHILE",   3, iIF, SWAP, EXIT);
	int iABRTQ = _IMMEDIATE("ABORT\"", 6, DOLIT, ABORQP, HERE, STORE, STRCQ, EXIT);
	int iSTRQ  = _IMMEDIATE("$\"",     6, DOLIT, STRQP, HERE, STORE, STRCQ, EXIT);
	int iDOTQQ = _IMMEDIATE(".\"",     6, DOLIT, DOTQP, HERE, STORE, STRCQ, EXIT);

	int CODE   = _COLON("CODE",        4, TOKEN, SNAME, OVERT, EXIT);
	int CREAT  = _COLON("CREATE",      5, CODE, DOLIT, 0x203d, COMMA, EXIT);
	int VARIA  = _COLON("VARIABLE",    5, CREAT, DOLIT, 0, COMMA, EXIT);
	int CONST  = _COLON("CONSTANT",    6, CODE, DOLIT, 0x2004, COMMA, COMMA, EXIT);
	int iDOTPR = _IMMEDIATE(".(",      5, DOLIT, 0x29, PARSE, TYPES, EXIT);
	int iBKSLA = _IMMEDIATE("\\",      5, DOLIT, 0xa, WORDD, DROP, EXIT);
	int iPAREN = _IMMEDIATE("(",       5, DOLIT, 0x29, PARSE, DDROP, EXIT);
	int ONLY   = _COLON("COMPILE-ONLY",6, DOLIT, 0x40, LAST, AT, PSTOR, EXIT);
	int IMMED  = _COLON("IMMEDIATE",   6, DOLIT, 0x80, LAST, AT, PSTOR, EXIT);
	int ENDD   = P;

	// Boot Up

	LOG("\n\nIZ=%x ", P);
    LOG("R-stack=%x", (_popR() << 2));
	P = 0;
	int RESET = _LABEL(2, 6, COLD);
	P = 0x90;
	int USER  = _LABEL(8, 0x100, 0x10, IMMED - 12, ENDD, IMMED - 12, INTER, QUITT, 0);
    
	// dump dictionary
	dump_data(0x2000);

	LOG("\n%s\n", "ceForth v4.0");
	R  = S = P = IP = top = 0;
	WP = 4;
	for (;;) {
		primitives[byte[P++]]();
	}
}
/* End of ceforth_33.cpp */

