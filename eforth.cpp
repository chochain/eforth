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
//Preamble
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
#define PRINTF(s,v)     printf(s,v)
//#define PRINTF(s,v)
#define DEBUG(s)        PRINTF("%s ", s)
#define SHOWOP(op)      printf("\n%04x: %s\t", P, op)
#define DEBUG_COLON() {                       \
	printf("\n");                             \
	for (U32 i=0; i<TAB; i++) printf("  ");   \
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

U32 rack[256]   = { 0 };           // return stack
S32 stack[256]  = { 0 };           // data stack
U32 data[16000] = {};              // 64K forth memory block
U8* cData       = (U8*)data;       // linear byte array pointer
U32 TAB = 0;

void show_word(int j) {
	U8 *p  = &cData[j];			   // pointer to address
	U32 op = data[j>>2];		   // get opocode
	for (p-=4; *p>31; p-=4);	   // retract pointer to word name

	int len = (int)*p;
	U8  buf[64];
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
	PRINTF(" %d", v);
	_push(data[IP >> 2]);
	IP += 4;
    _next();
}
void _dolist(void)              // ( -- ) push instruction pointer onto return stack and pop 
{
	DEBUG_COLON();
	rack[(U8)++R] = IP;
	IP = WP;
    _next();
}
void _exit(void)               // ( -- ) terminate all token lists in colon words
{
	DEBUG(" ;");
	TAB--;
	IP = rack[(U8)R--];
    _next();
}
void _execu(void)              // (a -- ) take execution address from data stack and execute the token
{
	P = top;
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
	cData[top] = (U8)stack[(U8)S--];
	_pop();
}
void _cat(void)                // (b -- n) fetch a byte from memory location
{
	top = (U32)cData[top];
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
	top = cData[top];
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

int IMEDD = 0x80;
int COMPO = 0x40;
int BRAN = 0, QBRAN = 0, DONXT = 0, DOTQP = 0, STRQP = 0, TOR = 0, ABORQP = 0;

void HEADER(int lex, const char *seq) {
	IP = P >> 2;
	int len = lex & 0x1f;                     // max length 31
	data[IP++] = thread;                      // point to previous word

	// dump memory between previous word and this
	PRINTF("%s", "\n    :");
	for (U32 i = thread>>2; thread && i < IP; i++) {
		PRINTF(" %08x", data[i]);
	}
	PRINTF("%c", '\n');

	P = IP << 2;
	thread = P;                               // keep pointer to this word
	cData[P++] = lex;                         // length of word
	for (int i = 0; i < len; i++) {           // memcpy word string
		cData[P++] = seq[i];
	}
	while (P & 3) { cData[P++] = 0xff; }      // padding 4-byte align
	PRINTF("%04x: ", P);
	PRINTF("%s", seq);
}
int CODE(int len, ...) {
	int addr = P;
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {
		U8 j = (U8)va_arg(argList, int);
		cData[P++] = j;
		PRINTF(" %02x", j);
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
		PRINTF(" %04x", j);           \
	}                                 \
	va_end(argList);                  \
}
int COLON(int len, ...) {
	PRINTF("%s", " COLON 0006");
	int addr = P;
	IP = P >> 2;
	data[IP++] = 6; // dolist
    DATACPY(len);
	P = IP << 2;
	return addr;
}
int LABEL(int len, ...) {
	SHOWOP("LABEL");
	int addr = P;
	IP = P >> 2;
    DATACPY(len);
	P = IP << 2;
	return addr;
}
void BEGIN(int len, ...) {
	SHOWOP("BEGIN");
	IP = P >> 2;
	_pushR(IP);
    DATACPY(len);
	P = IP << 2;
}
void AGAIN(int len, ...) {
	SHOWOP("AGAIN");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = _popR() << 2;
    DATACPY(len);
	P = IP << 2;
}
void UNTIL(int len, ...) {
	SHOWOP("UNTIL");
	IP = P >> 2;
	data[IP++] = QBRAN;
	data[IP++] = _popR() << 2;
    DATACPY(len);
	P = IP << 2;
}
void WHILE(int len, ...) {
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
void REPEAT(int len, ...) {
	SHOWOP("REPEAT");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = _popR() << 2;
	data[_popR()] = IP << 2;
    DATACPY(len);
	P = IP << 2;
}
void IF(int len, ...) {
	SHOWOP("IF");
	IP = P >> 2;
	data[IP++] = QBRAN;
	_pushR(IP);
	data[IP++] = 0;
    DATACPY(len);
	P = IP << 2;
}
void ELSE(int len, ...) {
	SHOWOP("ELSE");
	IP = P >> 2;
	data[IP++] = BRAN;
	data[IP++] = 0;
	data[_popR()] = IP << 2;
	_pushR(IP - 1);
    DATACPY(len);
	P = IP << 2;
}
void THEN(int len, ...) {
	SHOWOP("THEN");
	IP = P >> 2;
	data[_popR()] = IP << 2;
    DATACPY(len);
	P = IP << 2;
}
void FOR(int len, ...) {
	SHOWOP("FOR");
	IP = P >> 2;
	data[IP++] = TOR;
	_pushR(IP);
    DATACPY(len);
	P = IP << 2;
}
void NEXT(int len, ...) {
	SHOWOP("NEXT");
	IP = P >> 2;
	data[IP++] = DONXT;
	data[IP++] = _popR() << 2;
    DATACPY(len);
	P = IP << 2;
}
void AFT(int len, ...) {
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
	cData[P++] = len;                  \
	for (int i = 0; i < len; i++) {    \
		cData[P++] = seq[i];           \
	}                                  \
	while (P & 3) { cData[P++] = 0; }  \
	}
void DOTQ(const char *seq) {
	SHOWOP("DOTQ");
	PRINTF("%s", seq);
	STRCPY(DOTQP, seq);
}
void STRQ(const char *seq) {
	SHOWOP("STRQ");
	PRINTF("%s", seq);
	STRCPY(STRQP, seq);
}
void ABORQ(const char *seq) {
	SHOWOP("ABORQP");
	PRINTF("%s", seq);
	STRCPY(ABORQP, seq);
}

void CheckSum() {
    for (int p=0; p<0x2000; p+=0x20) {
        printf("\n%04x: ", p);
        for (int i=0; i<0x20; i++) {
        	U8 c = cData[p+i];
            printf("%02x", c);
            printf("%s", (i%4)==3 ? " " : "");
        }
        for (int i=0; i<0x20; i++) {
            U8 c = cData[p+i];
            printf("%c", c ? ((c>32 && c<127) ? c : '_') : '.');
        }
    }
}

// Byte Code Assembler
int as_nop = 0;
int as_bye = 1;
int as_qrx = 2;
int as_txsto = 3;
int as_docon = 4;
int as_dolit = 5;
int as_dolist = 6;
int as_exit = 7;
int as_execu = 8;
int as_donext = 9;
int as_qbran = 10;
int as_bran = 11;
int as_store = 12;
int as_at = 13;
int as_cstor = 14;
int as_cat = 15;
int as_rpat = 16;
int as_rpsto = 17;
int as_rfrom = 18;
int as_rat = 19;
int as_tor = 20;
int as_spat = 21;
int as_spsto = 22;
int as_drop = 23;
int as_dup = 24;
int as_swap = 25;
int as_over = 26;
int as_zless = 27;
int as_andd = 28;
int as_orr = 29;
int as_xorr = 30;
int as_uplus = 31;
int as_next = 32;
int as_qdup = 33;
int as_rot = 34;
int as_ddrop = 35;
int as_ddup = 36;
int as_plus = 37;
int as_inver = 38;
int as_negat = 39;
int as_dnega = 40;//
int as_subb = 41;
int as_abss = 42;
int as_equal = 43;
int as_uless = 44;
int as_less = 45;
int as_ummod = 46;
int as_msmod = 47;
int as_slmod = 48;
int as_mod = 49;
int as_slash = 50;
int as_umsta = 51;
int as_star = 52;
int as_mstar = 53;
int as_ssmod = 54;
int as_stasl = 55;
int as_pick = 56;
int as_pstor = 57;
int as_dstor = 58;
int as_dat = 59;
int as_count = 60;
int as_dovar = 61;
int as_max = 62;
int as_min = 63;

/*
* Main Program
*/
int main(int ac, char* av[])
{
	cData = (U8*)data;
	P = 0x200;
	R = thread = 0;

	// Kernel

	HEADER(3, "HLD");
	int HLD = CODE(8, as_docon, as_next, 0, 0, 0x80, 0, 0, 0);
	HEADER(4, "SPAN");
	int SPAN = CODE(8, as_docon, as_next, 0, 0, 0x84, 0, 0, 0);
	HEADER(3, ">IN");
	int INN = CODE(8, as_docon, as_next, 0, 0, 0x88, 0, 0, 0);
	HEADER(4, "#TIB");
	int NTIB = CODE(8, as_docon, as_next, 0, 0, 0x8C, 0, 0, 0);
	HEADER(4, "'TIB");
	int TTIB = CODE(8, as_docon, as_next, 0, 0, 0x90, 0, 0, 0);
	HEADER(4, "BASE");
	int BASE = CODE(8, as_docon, as_next, 0, 0, 0x94, 0, 0, 0);
	HEADER(7, "CONTEXT");
	int CNTXT = CODE(8, as_docon, as_next, 0, 0, 0x98, 0, 0, 0);
	HEADER(2, "CP");
	int CP = CODE(8, as_docon, as_next, 0, 0, 0x9C, 0, 0, 0);
	HEADER(4, "LAST");
	int LAST = CODE(8, as_docon, as_next, 0, 0, 0xA0, 0, 0, 0);
	HEADER(5, "'EVAL");
	int TEVAL = CODE(8, as_docon, as_next, 0, 0, 0xA4, 0, 0, 0);
	HEADER(6, "'ABORT");
	int TABRT = CODE(8, as_docon, as_next, 0, 0, 0xA8, 0, 0, 0);
	HEADER(3, "tmp");
	int TEMP = CODE(8, as_docon, as_next, 0, 0, 0xAC, 0, 0, 0);

	HEADER(3, "NOP");
	int NOP = CODE(4, as_next, 0, 0, 0);
	HEADER(3, "BYE");
	int BYE = CODE(4, as_bye, as_next, 0, 0);
	HEADER(3, "?RX");
	int QRX = CODE(4, as_qrx, as_next, 0, 0);
	HEADER(3, "TX!");
	int TXSTO = CODE(4, as_txsto, as_next, 0, 0);
	HEADER(5, "DOCON");
	int DOCON = CODE(4, as_docon, as_next, 0, 0);
	HEADER(5, "DOLIT");
	int DOLIT = CODE(4, as_dolit, as_next, 0, 0);
	HEADER(6, "DOLIST");
	int DOLST = CODE(4, as_dolist, as_next, 0, 0);
	HEADER(4, "EXIT");
	int EXITT = CODE(4, as_exit, as_next, 0, 0);
	HEADER(7, "EXECUTE");
	int EXECU = CODE(4, as_execu, as_next, 0, 0);

	HEADER(6, "DONEXT");
	DONXT = CODE(4, as_donext, as_next, 0, 0);
	HEADER(7, "QBRANCH");
	QBRAN = CODE(4, as_qbran, as_next, 0, 0);
	HEADER(6, "BRANCH");
	BRAN = CODE(4, as_bran, as_next, 0, 0);

	HEADER(1, "!");
	int STORE = CODE(4, as_store, as_next, 0, 0);
	HEADER(1, "@");
	int AT = CODE(4, as_at, as_next, 0, 0);
	HEADER(2, "C!");
	int CSTOR = CODE(4, as_cstor, as_next, 0, 0);
	HEADER(2, "C@");
	int CAT = CODE(4, as_cat, as_next, 0, 0);
	HEADER(2, "R>");
	int RFROM = CODE(4, as_rfrom, as_next, 0, 0);
	HEADER(2, "R@");
	int RAT = CODE(4, as_rat, as_next, 0, 0);
	HEADER(2, ">R");
	TOR = CODE(4, as_tor, as_next, 0, 0);
	HEADER(4, "DROP");
	int DROP = CODE(4, as_drop, as_next, 0, 0);
	HEADER(3, "DUP");
	int DUPP = CODE(4, as_dup, as_next, 0, 0);
	HEADER(4, "SWAP");
	int SWAP = CODE(4, as_swap, as_next, 0, 0);
	HEADER(4, "OVER");
	int OVER = CODE(4, as_over, as_next, 0, 0);
	HEADER(2, "0<");
	int ZLESS = CODE(4, as_zless, as_next, 0, 0);
	HEADER(3, "AND");
	int ANDD = CODE(4, as_andd, as_next, 0, 0);
	HEADER(2, "OR");
	int ORR = CODE(4, as_orr, as_next, 0, 0);
	HEADER(3, "XOR");
	int XORR = CODE(4, as_xorr, as_next, 0, 0);
	HEADER(3, "UM+");
	int UPLUS = CODE(4, as_uplus, as_next, 0, 0);
	HEADER(4, "NEXT");
	int NEXTT = CODE(4, as_next, as_next, 0, 0);
	HEADER(4, "?DUP");
	int QDUP = CODE(4, as_qdup, as_next, 0, 0);
	HEADER(3, "ROT");
	int ROT = CODE(4, as_rot, as_next, 0, 0);
	HEADER(5, "2DROP");
	int DDROP = CODE(4, as_ddrop, as_next, 0, 0);
	HEADER(4, "2DUP");
	int DDUP = CODE(4, as_ddup, as_next, 0, 0);
	HEADER(1, "+");
	int PLUS = CODE(4, as_plus, as_next, 0, 0);
	HEADER(3, "NOT");
	int INVER = CODE(4, as_inver, as_next, 0, 0);
	HEADER(6, "NEGATE");
	int NEGAT = CODE(4, as_negat, as_next, 0, 0);
	HEADER(7, "DNEGATE");
	int DNEGA = CODE(4, as_dnega, as_next, 0, 0);
	HEADER(1, "-");
	int SUBBB = CODE(4, as_subb, as_next, 0, 0);
	HEADER(3, "ABS");
	int ABSS = CODE(4, as_abss, as_next, 0, 0);
	HEADER(1, "=");
	int EQUAL = CODE(4, as_equal, as_next, 0, 0);
	HEADER(2, "U<");
	int ULESS = CODE(4, as_uless, as_next, 0, 0);
	HEADER(1, "<");
	int LESS = CODE(4, as_less, as_next, 0, 0);
	HEADER(6, "UM/MOD");
	int UMMOD = CODE(4, as_ummod, as_next, 0, 0);
	HEADER(5, "M/MOD");
	int MSMOD = CODE(4, as_msmod, as_next, 0, 0);
	HEADER(4, "/MOD");
	int SLMOD = CODE(4, as_slmod, as_next, 0, 0);
	HEADER(3, "MOD");
	int MODD = CODE(4, as_mod, as_next, 0, 0);
	HEADER(1, "/");
	int SLASH = CODE(4, as_slash, as_next, 0, 0);
	HEADER(3, "UM*");
	int UMSTA = CODE(4, as_umsta, as_next, 0, 0);
	HEADER(1, "*");
	int STAR = CODE(4, as_star, as_next, 0, 0);
	HEADER(2, "M*");
	int MSTAR = CODE(4, as_mstar, as_next, 0, 0);
	HEADER(5, "*/MOD");
	int SSMOD = CODE(4, as_ssmod, as_next, 0, 0);
	HEADER(2, "*/");
	int STASL = CODE(4, as_stasl, as_next, 0, 0);
	HEADER(4, "PICK");
	int PICK = CODE(4, as_pick, as_next, 0, 0);
	HEADER(2, "+!");
	int PSTOR = CODE(4, as_pstor, as_next, 0, 0);
	HEADER(2, "2!");
	int DSTOR = CODE(4, as_dstor, as_next, 0, 0);
	HEADER(2, "2@");
	int DAT = CODE(4, as_dat, as_next, 0, 0);
	HEADER(5, "COUNT");
	int COUNT = CODE(4, as_count, as_next, 0, 0);
	HEADER(3, "MAX");
	int MAX = CODE(4, as_max, as_next, 0, 0);
	HEADER(3, "MIN");
	int MIN = CODE(4, as_min, as_next, 0, 0);
	HEADER(2, "BL");
	int BLANK = CODE(8, as_docon, as_next, 0, 0, 32, 0, 0, 0);
	HEADER(4, "CELL");
	int CELL = CODE(8, as_docon, as_next, 0, 0, 4, 0, 0, 0);
	HEADER(5, "CELL+");
	int CELLP = CODE(8, as_docon, as_plus, as_next, 0, 4, 0, 0, 0);
	HEADER(5, "CELL-");
	int CELLM = CODE(8, as_docon, as_subb, as_next, 0, 4, 0, 0, 0);
	HEADER(5, "CELLS");
	int CELLS = CODE(8, as_docon, as_star, as_next, 0, 4, 0, 0, 0);
	HEADER(5, "CELL/");
	int CELLD = CODE(8, as_docon, as_slash, as_next, 0, 4, 0, 0, 0);
	HEADER(2, "1+");
	int ONEP = CODE(8, as_docon, as_plus, as_next, 0, 1, 0, 0, 0);
	HEADER(2, "1-");
	int ONEM = CODE(8, as_docon, as_subb, as_next, 0, 1, 0, 0, 0);
	HEADER(5, "DOVAR");
	int DOVAR = CODE(4, as_dovar, as_next, 0, 0);

	// Common Colon Words

	U8 *c = &cData[P];
	HEADER(4, "?KEY");
	int QKEY = COLON(2, QRX, EXITT);
	HEADER(3, "KEY");
	int KEY = COLON(0);
	BEGIN(1, QKEY);
	UNTIL(1, EXITT);
	HEADER(4, "EMIT");
	int EMIT = COLON(2, TXSTO, EXITT);
	HEADER(6, "WITHIN");
	int WITHI = COLON(7, OVER, SUBBB, TOR, SUBBB, RFROM, ULESS, EXITT);
	HEADER(5, ">CHAR");
	int TCHAR = COLON(8, DOLIT, 0x7F, ANDD, DUPP, DOLIT, 0x7F, BLANK, WITHI);
	IF(3, DROP, DOLIT, 0x5F);
	THEN(1, EXITT);
	HEADER(7, "ALIGNED");
	int ALIGN = COLON(7, DOLIT, 3, PLUS, DOLIT, 0xFFFFFFFC, ANDD, EXITT);
	HEADER(4, "HERE");
	int HERE = COLON(3, CP, AT, EXITT);
	HEADER(3, "PAD");
	int PAD = COLON(5, HERE, DOLIT, 0x50, PLUS, EXITT);
	HEADER(3, "TIB");
	int TIB = COLON(3, TTIB, AT, EXITT);
	HEADER(8, "@EXECUTE");
	int ATEXE = COLON(2, AT, QDUP);
	IF(1, EXECU);
	THEN(1, EXITT);
	HEADER(5, "CMOVE");
	int CMOVEE = COLON(0);
	FOR(0);
	AFT(8, OVER, CAT, OVER, CSTOR, TOR, ONEP, RFROM, ONEP);
	THEN(0);
	NEXT(2, DDROP, EXITT);
	HEADER(4, "MOVE");
	int MOVE = COLON(1, CELLD);
	FOR(0);
	AFT(8, OVER, AT, OVER, STORE, TOR, CELLP, RFROM, CELLP);
	THEN(0);
	NEXT(2, DDROP, EXITT);
	HEADER(4, "FILL");
	int FILL = COLON(1, SWAP);
	FOR(1, SWAP);
	AFT(3, DDUP, CSTOR, ONEP);
	THEN(0);
	NEXT(2, DDROP, EXITT);

	// Number Conversions

	HEADER(5, "DIGIT");
	int DIGIT = COLON(12, DOLIT, 9, OVER, LESS, DOLIT, 7, ANDD, PLUS, DOLIT, 0x30, PLUS, EXITT);
	HEADER(7, "EXTRACT");
	int EXTRC = COLON(7, DOLIT, 0, SWAP, UMMOD, SWAP, DIGIT, EXITT);
	HEADER(2, "<#");
	int BDIGS = COLON(4, PAD, HLD, STORE, EXITT);
	HEADER(4, "HOLD");
	int HOLD = COLON(8, HLD, AT, ONEM, DUPP, HLD, STORE, CSTOR, EXITT);
	HEADER(1, "#");
	int DIG = COLON(5, BASE, AT, EXTRC, HOLD, EXITT);
	HEADER(2, "#S");
	int DIGS = COLON(0);
	BEGIN(2, DIG, DUPP);
	WHILE(0);
	REPEAT(1, EXITT);
	HEADER(4, "SIGN");
	int SIGN = COLON(1, ZLESS);
	IF(3, DOLIT, 0x2D, HOLD);
	THEN(1, EXITT);
	HEADER(2, "#>");
	int EDIGS = COLON(7, DROP, HLD, AT, PAD, OVER, SUBBB, EXITT);
	HEADER(3, "str");
	int STRR = COLON(9, DUPP, TOR, ABSS, BDIGS, DIGS, RFROM, SIGN, EDIGS, EXITT);
	HEADER(3, "HEX");
	int HEXX = COLON(5, DOLIT, 16, BASE, STORE, EXITT);
	HEADER(7, "DECIMAL");
	int DECIM = COLON(5, DOLIT, 10, BASE, STORE, EXITT);
	HEADER(6, "wupper");
	int UPPER = COLON(4, DOLIT, 0x5F5F5F5F, ANDD, EXITT);
	HEADER(6, ">upper");
	int TOUPP = COLON(6, DUPP, DOLIT, 0x61, DOLIT, 0x7B, WITHI);
	IF(3, DOLIT, 0x5F, ANDD);
	THEN(1, EXITT);
	HEADER(6, "DIGIT?");
	int DIGTQ = COLON(9, TOR, TOUPP, DOLIT, 0x30, SUBBB, DOLIT, 9, OVER, LESS);
	IF(8, DOLIT, 7, SUBBB, DUPP, DOLIT, 10, LESS, ORR);
	THEN(4, DUPP, RFROM, ULESS, EXITT);
	HEADER(7, "NUMBER?");
	int NUMBQ = COLON(12, BASE, AT, TOR, DOLIT, 0, OVER, COUNT, OVER, CAT, DOLIT, 0x24, EQUAL);
	IF(5, HEXX, SWAP, ONEP, SWAP, ONEM);
	THEN(13, OVER, CAT, DOLIT, 0x2D, EQUAL, TOR, SWAP, RAT, SUBBB, SWAP, RAT, PLUS, QDUP);
      IF(1, ONEM);
        FOR(6, DUPP, TOR, CAT, BASE, AT, DIGTQ);
        WHILE(7, SWAP, BASE, AT, STAR, PLUS, RFROM, ONEP);
        NEXT(2, DROP, RAT);
        IF(1, NEGAT);
        THEN(1, SWAP);
      ELSE(6, RFROM, RFROM, DDROP, DDROP, DOLIT, 0);
      THEN(1, DUPP);
    THEN(6, RFROM, DDROP, RFROM, BASE, STORE, EXITT);

	// Terminal Output

	HEADER(5, "SPACE");
	int SPACE = COLON(3, BLANK, EMIT, EXITT);
	HEADER(5, "CHARS");
	int CHARS = COLON(4, SWAP, DOLIT, 0, MAX);
	FOR(0);
	AFT(2, DUPP, EMIT);
	THEN(0);
	NEXT(2, DROP, EXITT);
	HEADER(6, "SPACES");
	int SPACS = COLON(3, BLANK, CHARS, EXITT);
	HEADER(4, "TYPE");
	int TYPES = COLON(0);
	FOR(0);
	AFT(3, COUNT, TCHAR, EMIT);
	THEN(0);
	NEXT(2, DROP, EXITT);
	HEADER(2, "CR");
	int CR = COLON(7, DOLIT, 10, DOLIT, 13, EMIT, EMIT, EXITT);
	HEADER(3, "do$");
	int DOSTR = COLON(10, RFROM, RAT, RFROM, COUNT, PLUS, ALIGN, TOR, SWAP, TOR, EXITT);
	HEADER(3, "$\"|");
	int STRQP = COLON(2, DOSTR, EXITT);
	HEADER(3, ".\"|");
	DOTQP = COLON(4, DOSTR, COUNT, TYPES, EXITT);
	HEADER(2, ".R");
	int DOTR = COLON(8, TOR, STRR, RFROM, OVER, SUBBB, SPACS, TYPES, EXITT);
	HEADER(3, "U.R");
	int UDOTR = COLON(10, TOR, BDIGS, DIGS, EDIGS, RFROM, OVER, SUBBB, SPACS, TYPES, EXITT);
	HEADER(2, "U.");
	int UDOT = COLON(6, BDIGS, DIGS, EDIGS, SPACE, TYPES, EXITT);
	HEADER(1, ".");
	int DOT = COLON(5, BASE, AT, DOLIT, 0xA, XORR);
	IF(2, UDOT, EXITT);
	THEN(4, STRR, SPACE, TYPES, EXITT);
	HEADER(1, "?");
	int QUEST = COLON(3, AT, DOT, EXITT);

	// Parser

	HEADER(7, "(parse)");
	int PARS = COLON(5, TEMP, CSTOR, OVER, TOR, DUPP);
	IF(5, ONEM, TEMP, CAT, BLANK, EQUAL);
	  IF(0);
	    FOR(6, BLANK, OVER, CAT, SUBBB, ZLESS, INVER);
	    WHILE(1, ONEP);
	    NEXT(6, RFROM, DROP, DOLIT, 0, DUPP, EXITT);
	  THEN(1, RFROM);
	THEN(2, OVER, SWAP);
	  FOR(9, TEMP, CAT, OVER, CAT, SUBBB, TEMP, CAT, BLANK, EQUAL);
	    IF(1, ZLESS);
	    THEN(0);
	  WHILE(1, ONEP);
	  NEXT(2, DUPP, TOR);
	ELSE(5, RFROM, DROP, DUPP, ONEP, TOR);
	THEN(6, OVER, SUBBB, RFROM, RFROM, SUBBB, EXITT);
	THEN(4, OVER, RFROM, SUBBB, EXITT);
	HEADER(5, "PACK$");
	int PACKS = COLON(18, DUPP, TOR, DDUP, PLUS, DOLIT, 0xFFFFFFFC, ANDD, DOLIT, 0, SWAP, STORE, DDUP, CSTOR, ONEP, SWAP, CMOVEE, RFROM, EXITT);
	HEADER(5, "PARSE");
	int PARSE = COLON(15, TOR, TIB, INN, AT, PLUS, NTIB, AT, INN, AT, SUBBB, RFROM, PARS, INN, PSTOR, EXITT);
	HEADER(5, "TOKEN");
	int TOKEN = COLON(9, BLANK, PARSE, DOLIT, 0x1F, MIN, HERE, CELLP, PACKS, EXITT);
	HEADER(4, "WORD");
	int WORDD = COLON(5, PARSE, HERE, CELLP, PACKS, EXITT);
	HEADER(5, "NAME>");
	int NAMET = COLON(7, COUNT, DOLIT, 0x1F, ANDD, PLUS, ALIGN, EXITT);
	HEADER(5, "SAME?");
	int SAMEQ = COLON(4, DOLIT, 0x1F, ANDD, CELLD);
	FOR(0);
	AFT(14, OVER, RAT, CELLS, PLUS, AT, UPPER, OVER, RAT, CELLS, PLUS, AT, UPPER, SUBBB, QDUP);
      IF(3, RFROM, DROP, EXITT);
	  THEN(0);
	THEN(0);
	NEXT(3, DOLIT, 0, EXITT);
	HEADER(4, "find");
	int FIND = COLON(10, SWAP, DUPP, AT, TEMP, STORE, DUPP, AT, TOR, CELLP, SWAP);
	BEGIN(2, AT, DUPP);
	  IF(9, DUPP, AT, DOLIT, 0xFFFFFF3F, ANDD, UPPER, RAT, UPPER, XORR);
	    IF(3, CELLP, DOLIT, 0xFFFFFFFF);
  	    ELSE(4, CELLP, TEMP, AT, SAMEQ);
	    THEN(0);
	  ELSE(6, RFROM, DROP, SWAP, CELLM, SWAP, EXITT);
	  THEN(0);
	WHILE(2, CELLM, CELLM);
    REPEAT(9, RFROM, DROP, SWAP, DROP, CELLM, DUPP, NAMET, SWAP, EXITT);
	HEADER(5, "NAME?");
	int NAMEQ = COLON(3, CNTXT, FIND, EXITT);

	// Terminal Input

	HEADER(2, "^H");
	int HATH = COLON(6, TOR, OVER, RFROM, SWAP, OVER, XORR);
	IF(9, DOLIT, 8, EMIT, ONEM, BLANK, EMIT, DOLIT, 8, EMIT);
	THEN(1, EXITT);
	HEADER(3, "TAP");
	int TAP = COLON(6, DUPP, EMIT, OVER, CSTOR, ONEP, EXITT);
	HEADER(4, "kTAP");
	int KTAP = COLON(9, DUPP, DOLIT, 0xD, XORR, OVER, DOLIT, 0xA, XORR, ANDD);
	IF(3, DOLIT, 8, XORR);
	  IF(2, BLANK, TAP);
	  ELSE(1, HATH);
	  THEN(1, EXITT);
	THEN(5, DROP, SWAP, DROP, DUPP, EXITT);
	HEADER(6, "ACCEPT");
	int ACCEP = COLON(3, OVER, PLUS, OVER);
	BEGIN(2, DDUP, XORR);
	WHILE(7, KEY, DUPP, BLANK, SUBBB, DOLIT, 0x5F, ULESS);
	  IF(1, TAP);
  	  ELSE(1, KTAP);
	  THEN(0);
	REPEAT(4, DROP, OVER, SUBBB, EXITT);
	HEADER(6, "EXPECT");
	int EXPEC = COLON(5, ACCEP, SPAN, STORE, DROP, EXITT);
	HEADER(5, "QUERY");
	int QUERY = COLON(12, TIB, DOLIT, 0x50, ACCEP, NTIB, STORE, DROP, DOLIT, 0, INN, STORE, EXITT);

	// Text Interpreter

	HEADER(5, "ABORT");
	int ABORT = COLON(2, TABRT, ATEXE);
	HEADER(6, "abort\"");
	ABORQP = COLON(0);
	IF(4, DOSTR, COUNT, TYPES, ABORT);
	THEN(3, DOSTR, DROP, EXITT);
	HEADER(5, "ERROR");
	int ERRORR = COLON(11, SPACE, COUNT, TYPES, DOLIT, 0x3F, EMIT, DOLIT, 0x1B, EMIT, CR, ABORT);
	HEADER(10, "$INTERPRET");
	int INTER = COLON(2, NAMEQ, QDUP);
	IF(4, CAT, DOLIT, COMPO, ANDD);
	ABORQ(" compile only");
	int INTER0 = LABEL(2, EXECU, EXITT);
	THEN(1, NUMBQ);
	IF(1, EXITT);
	ELSE(1, ERRORR);
	THEN(0);
	HEADER(IMEDD + 1, "[");
	int LBRAC = COLON(5, DOLIT, INTER, TEVAL, STORE, EXITT);
	HEADER(3, ".OK");
	int DOTOK = COLON(6, CR, DOLIT, INTER, TEVAL, AT, EQUAL);
	IF(14, TOR, TOR, TOR, DUPP, DOT, RFROM, DUPP, DOT, RFROM, DUPP, DOT, RFROM, DUPP, DOT);
	DOTQ(" ok>");
	THEN(1, EXITT);
	HEADER(4, "EVAL");
	int EVAL = COLON(0);
	BEGIN(3, TOKEN, DUPP, AT);
	WHILE(2, TEVAL, ATEXE);
	REPEAT(3, DROP, DOTOK, EXITT);
	HEADER(4, "QUIT");
	int QUITT = COLON(5, DOLIT, 0x100, TTIB, STORE, LBRAC);
	BEGIN(2, QUERY, EVAL);
	AGAIN(0);

	// Colon Word Compiler

	HEADER(1, ",");
	int COMMA = COLON(7, HERE, DUPP, CELLP, CP, STORE, STORE, EXITT);
	HEADER(IMEDD + 7, "LITERAL");
	int LITER = COLON(5, DOLIT, DOLIT, COMMA, COMMA, EXITT);
	HEADER(5, "ALLOT");
	int ALLOT = COLON(4, ALIGN, CP, PSTOR, EXITT);
	HEADER(3, "$,\"");
	int STRCQ = COLON(9, DOLIT, 0x22, WORDD, COUNT, PLUS, ALIGN, CP, STORE, EXITT);
	HEADER(7, "?UNIQUE");
	int UNIQU = COLON(3, DUPP, NAMEQ, QDUP);
	IF(6, COUNT, DOLIT, 0x1F, ANDD, SPACE, TYPES);
	DOTQ(" reDef");
	THEN(2, DROP, EXITT);
	HEADER(3, "$,n");
	int SNAME = COLON(2, DUPP, AT);
	IF(14, UNIQU, DUPP, NAMET, CP, STORE, DUPP, LAST, STORE, CELLM, CNTXT, AT, SWAP, STORE, EXITT);
	THEN(1, ERRORR);
	HEADER(1, "'");
	int TICK = COLON(2, TOKEN, NAMEQ);
	IF(1, EXITT);
	THEN(1, ERRORR);
	HEADER(IMEDD + 9, "[COMPILE]");
	int BCOMP = COLON(3, TICK, COMMA, EXITT);
	HEADER(7, "COMPILE");
	int COMPI = COLON(7, RFROM, DUPP, AT, COMMA, CELLP, TOR, EXITT);
	HEADER(8, "$COMPILE");
	int SCOMP = COLON(2, NAMEQ, QDUP);
	IF(4, AT, DOLIT, IMEDD, ANDD);
	  IF(1, EXECU);
	  ELSE(1, COMMA);
	  THEN(1, EXITT);
	THEN(1, NUMBQ);
	IF(2, LITER, EXITT);
	THEN(1, ERRORR);
	HEADER(5, "OVERT");
	int OVERT = COLON(5, LAST, AT, CNTXT, STORE, EXITT);
	HEADER(1, "]");
	int RBRAC = COLON(5, DOLIT, SCOMP, TEVAL, STORE, EXITT);
	HEADER(1, ":");
	int COLN = COLON(7, TOKEN, SNAME, RBRAC, DOLIT, 0x6, COMMA, EXITT);
	HEADER(IMEDD + 1, ";");
	int SEMIS = COLON(6, DOLIT, EXITT, COMMA, LBRAC, OVERT, EXITT);

	// Debugging Tools

	HEADER(3, "dm+");
	int DMP = COLON(4, OVER, DOLIT, 6, UDOTR);
	FOR(0);
	AFT(6, DUPP, AT, DOLIT, 9, UDOTR, CELLP);
	THEN(0);
	NEXT(1, EXITT);
	HEADER(4, "DUMP");
	int DUMP = COLON(10, BASE, AT, TOR, HEXX, DOLIT, 0x1F, PLUS, DOLIT, 0x20, SLASH);
	FOR(0);
	AFT(10, CR, DOLIT, 8, DDUP, DMP, TOR, SPACE, CELLS, TYPES, RFROM);
	THEN(0);
	NEXT(5, DROP, RFROM, BASE, STORE, EXITT);
	HEADER(5, ">NAME");
	int TNAME = COLON(1, CNTXT);
	BEGIN(2, AT, DUPP);
	WHILE(3, DDUP, NAMET, XORR);
	  IF(1, ONEM);
	  ELSE(3, SWAP, DROP, EXITT);
	  THEN(0);
	REPEAT(3, SWAP, DROP, EXITT);
	HEADER(3, ".ID");
	int DOTID = COLON(7, COUNT, DOLIT, 0x1F, ANDD, TYPES, SPACE, EXITT);
	HEADER(5, "WORDS");
	int WORDS = COLON(6, CR, CNTXT, DOLIT, 0, TEMP, STORE);
	BEGIN(2, AT, QDUP);
	WHILE(9, DUPP, SPACE, DOTID, CELLM, TEMP, AT, DOLIT, 0xA, LESS);
	  IF(4, DOLIT, 1, TEMP, PSTOR);
	  ELSE(5, CR, DOLIT, 0, TEMP, STORE);
	  THEN(0);
	REPEAT(1, EXITT);
	HEADER(6, "FORGET");
	int FORGT = COLON(3, TOKEN, NAMEQ, QDUP);
	IF(12, CELLM, DUPP, CP, STORE, AT, DUPP, CNTXT, STORE, LAST, STORE, DROP, EXITT);
	THEN(1, ERRORR);
	HEADER(4, "COLD");
	int COLD = COLON(1, CR);
	DOTQ("eForth in C v4.0");
	int DOTQ1 = LABEL(2, CR, QUITT);

	// Structure Compiler

	HEADER(IMEDD + 4, "THEN");
	int THENN = COLON(4, HERE, SWAP, STORE, EXITT);
	HEADER(IMEDD + 3, "FOR");
	int FORR = COLON(4, COMPI, TOR, HERE, EXITT);
	HEADER(IMEDD + 5, "BEGIN");
	int BEGIN = COLON(2, HERE, EXITT);
	HEADER(IMEDD + 4, "NEXT");
	int NEXT = COLON(4, COMPI, DONXT, COMMA, EXITT);
	HEADER(IMEDD + 5, "UNTIL");
	int UNTIL = COLON(4, COMPI, QBRAN, COMMA, EXITT);
	HEADER(IMEDD + 5, "AGAIN");
	int AGAIN = COLON(4, COMPI, BRAN, COMMA, EXITT);
	HEADER(IMEDD + 2, "IF");
	int IFF = COLON(7, COMPI, QBRAN, HERE, DOLIT, 0, COMMA, EXITT);
	HEADER(IMEDD + 5, "AHEAD");
	int AHEAD = COLON(7, COMPI, BRAN, HERE, DOLIT, 0, COMMA, EXITT);
	HEADER(IMEDD + 6, "REPEAT");
	int REPEA = COLON(3, AGAIN, THENN, EXITT);
	HEADER(IMEDD + 3, "AFT");
	int AFT = COLON(5, DROP, AHEAD, HERE, SWAP, EXITT);
	HEADER(IMEDD + 4, "ELSE");
	int ELSEE = COLON(4, AHEAD, SWAP, THENN, EXITT);
	HEADER(IMEDD + 4, "WHEN");
	int WHEN = COLON(3, IFF, OVER, EXITT);
	HEADER(IMEDD + 5, "WHILE");
	int WHILEE = COLON(3, IFF, SWAP, EXITT);
	HEADER(IMEDD + 6, "ABORT\"");
	int ABRTQ = COLON(6, DOLIT, ABORQP, HERE, STORE, STRCQ, EXITT);
	HEADER(IMEDD + 2, "$\"");
	int STRQ = COLON(6, DOLIT, STRQP, HERE, STORE, STRCQ, EXITT);
	HEADER(IMEDD + 2, ".\"");
	int DOTQQ = COLON(6, DOLIT, DOTQP, HERE, STORE, STRCQ, EXITT);
	HEADER(4, "CODE");
	int CODE = COLON(4, TOKEN, SNAME, OVERT, EXITT);
	HEADER(6, "CREATE");
	int CREAT = COLON(5, CODE, DOLIT, 0x203D, COMMA, EXITT);
	HEADER(8, "VARIABLE");
	int VARIA = COLON(5, CREAT, DOLIT, 0, COMMA, EXITT);
	HEADER(8, "CONSTANT");
	int CONST = COLON(6, CODE, DOLIT, 0x2004, COMMA, COMMA, EXITT);
	HEADER(IMEDD + 2, ".(");
	int DOTPR = COLON(5, DOLIT, 0x29, PARSE, TYPES, EXITT);
	HEADER(IMEDD + 1, "\\");
	int BKSLA = COLON(5, DOLIT, 0xA, WORDD, DROP, EXITT);
	HEADER(IMEDD + 1, "(");
	int PAREN = COLON(5, DOLIT, 0x29, PARSE, DDROP, EXITT);
	HEADER(12, "COMPILE-ONLY");
	int ONLY = COLON(6, DOLIT, 0x40, LAST, AT, PSTOR, EXITT);
	HEADER(9, "IMMEDIATE");
	int IMMED = COLON(6, DOLIT, 0x80, LAST, AT, PSTOR, EXITT);
	int ENDD = P;

	// Boot Up

	PRINTF("\n\nIZ=%x ", P);
    PRINTF("R-stack=%x", (_popR() << 2));
	P = 0;
	int RESET = LABEL(2, 6, COLD);
	P = 0x90;
	int USER  = LABEL(8, 0x100, 0x10, IMMED - 12, ENDD, IMMED - 12, INTER, QUITT, 0);
    
	// dump dictionaryHEADER(3, "HLD")
	CheckSum();

	PRINTF("\n%s\n", "ceForth v4.0");
	P   = 0;
	WP  = 4;
	IP  = 0;
	S   = 0;
	R   = 0;
	top = 0;
	for (;;) {
		primitives[cData[P++]]();
	}
}
/* End of ceforth_33.cpp */

