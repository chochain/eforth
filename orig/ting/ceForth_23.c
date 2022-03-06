// ceForth_23.cpp : Defines the entry point for the console application.
//
/******************************************************************************/
/* ceForth_23.cpp, Version 2.3 : Forth in C                                   */
/******************************************************************************/
/* Chen-Hanson Ting                                                           */
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

//#include "stdafx.h"	// :mk
#include <stdio.h>		// added :mk
#include <stdlib.h>
//#include <stdarg.h>		// added :mk

#include "rom_23.h"
# define	FALSE	0
# define	TRUE	-1
# define	LOGICAL ? TRUE : FALSE
# define 	LOWER(x,y) ((unsigned long)(x)<(unsigned long)(y))
# define	pop	top = stack[(char) S--]
# define	push	stack[(char) ++S] = top; top =

long rack[256] = { 0 };
long stack[256] = { 0 };
long long int d, n, m;
unsigned char R=0;
unsigned char S=0;
long top = 0;
long  P, IP, WP;
unsigned char* cData;
unsigned char bytecode;

// 64 bit math support without long long
// Change the typedefs to what your compiler expects
//typedef unsigned short     uint16;
typedef unsigned int       uint32;
//typedef unsigned long long uint64;

void add32(uint32 left, uint32 right, uint32 *prh, uint32 *prl)
{
	uint32 rl = left + right;
	uint32 rh = rl < left;

	*prl = rl;
	*prh = rh;
}

// Multiplies two 32bit ints
multiply32(uint32 left, uint32 right, uint32 *prh, uint32 *prl)
{
	uint32 lleft, uleft, lright, uright, a, b, c, d;
	uint32 sr1h, sr1l;
	uint32 sr2h, sr2l;

	// Make 16 bit integers but keep them in 32 bit integer
	// to retain the higher bits

	lleft = left & 0xFFFF;
	lright = right & 0xFFFF;
	uleft = (left >> 16);
	uright = (right >> 16);

	a = lleft * lright;
	b = lleft * uright;
	c = uleft * lright;
	d = uleft * uright;

	add32(a, (b << 16), &sr1h, &sr1l);
	add32(sr1l, (c << 16), &sr2h, &sr2l);

	*prl = sr2l;
	*prh = d + (b >> 16) + (c >> 16);
	if (sr1h)
	{
		++(*prh);
	}

	if (sr2h)
	{
		++(*prh);
	}
}

void bye(void);
void bye (void)
{	exit(0); }
void qrx(void);
void qrx(void)
{	push(long) getchar();
	if (top != 0) push TRUE; }
void txsto(void);
void txsto(void)
{	putchar((char)top);
	pop; }
void next(void);
void next(void)
{	P = data[IP>>2];
	WP = P + 4;
	IP += 4; }
void dovar(void);
void dovar(void)
{	push WP; }
void docon(void);
void docon(void)
{	push data[WP>>2]; }
void dolit(void);
void dolit(void)
{	push data[IP>>2];
	IP += 4;
	next(); }
void dolist(void);
void dolist(void)
{	rack[(char)++R] = IP;
	IP = WP;
	next(); }
void exitt(void);
void exitt(void)
{	IP = (long)rack[(char)R--];
	next(); }
void execu(void);
void execu(void)
{	P = top;
	WP = P + 4;
	pop; }
void donext(void);
void donext(void)
{	if (rack[(char)R]) {
		rack[(char)R] -= 1;
		IP = data[IP>>2]; }
	else {IP += 4;
		R--; }
	next(); }
void qbran(void);
void qbran(void)
{	if (top == 0) IP = data[IP>>2];
	else IP += 4;
	pop;
	next(); }
void bran(void);
void bran(void)
{	IP = data[IP>>2];
	next(); }
void store(void);
void store(void)
{	data[top>>2] = stack[(char) S--];
	pop; }
void at(void);
void at(void)
{	top = data[top>>2]; }
void cstor(void);
void cstor(void)
{	cData[top] = (char)stack[(char) S--];
	pop; }
void cat(void);
void cat(void)
{	top = (long)cData[top]; }
void rfrom(void);
void rfrom(void)
{	push rack[(char)R--]; }
void rat(void);
void rat(void)
{	push rack[(char)R]; }
void tor(void);
void tor(void)
{	rack[(char)++R] = top;
	pop; }
void drop(void);
void drop(void)
{	pop; }
void dup(void);
void dup(void)
{	stack[(char) ++S] = top; }
void swap(void);
void swap(void)
{	WP = top;
	top = stack[(char) S];
	stack[(char) S] = WP; }
void over(void);
void over(void)
{	push stack[(char) S - 1]; }
void zless(void);
void zless(void)
{	top = (top < 0) LOGICAL; }
void andd(void);
void andd(void)
{	top &= stack[(char) S--]; }
void orr(void);
void orr(void)
{	top |= stack[(char) S--]; }
void xorr(void);
void xorr(void)
{	top ^= stack[(char) S--]; }
void uplus(void);
void uplus(void)
{	stack[(char) S] += top;
	top = LOWER(stack[(char) S], top); }
void nop(void);
void nop(void)
{	next(); }
void qdup(void);
void qdup(void)
{	if (top) stack[(char) ++S] = top; }
void rot(void);
void rot(void)
{	WP = stack[(char) S - 1];
	stack[(char) S - 1] = stack[(char) S];
	stack[(char) S] = top;
	top = WP; }
void ddrop(void);
void ddrop(void)
{	drop(); drop(); }
void ddup(void);
void ddup(void)
{	over(); over(); }
void plus(void);
void plus(void)
{	top += stack[(char) S--]; }
void inver(void);
void inver(void)
{	top = -top - 1; }
void negat(void);
void negat(void)
{	top = 0 - top; }
void dnega(void);
void dnega(void)
{	inver();
	tor();
	inver();
	push 1;
	uplus();
	rfrom();
	plus(); }
void subb(void);
void subb(void)
{	top = stack[(char) S--] - top; }
void abss(void);
void abss(void)
{	if (top < 0)
		top = -top; }
void great(void);
void great(void)
{	top = (stack[(char) S--] > top) LOGICAL; }
void less(void);
void less(void)
{	top = (stack[(char) S--] < top) LOGICAL; }
void equal(void);
void equal(void)
{	top = (stack[(char) S--] == top) LOGICAL; }
void uless(void);
void uless(void)
{	top = LOWER(stack[(char) S], top) LOGICAL; (char) S--; }
void ummod(void);
void ummod(void)  // mk: Without long long
{	d = (long long int)((unsigned long)top);
	m = (long long int)((unsigned long)stack[(char) S]);      // Upper 32-bit
	n = (long long int)((unsigned long)stack[(char) S - 1]);  // Lower 32-bit
	n += m << 32;
	pop;
	top = (unsigned long)(n / d);
	stack[(char) S] = (unsigned long)(n%d); }
void msmod(void);
void msmod(void)  // mk: Without long long
{	d = (signed long long int)((signed long)top);
	m = (signed long long int)((signed long)stack[(char) S]);
	n = (signed long long int)((signed long)stack[(char) S - 1]);
	n += m << 32;
	pop;
	top = (signed long)(n / d);
	stack[(char) S] = (signed long)(n%d); }
void slmod(void);
void slmod(void)
{	if (top != 0) {
		WP = stack[(char) S] / top;
		stack[(char) S] %= top;
		top = WP;
	} }
void mod(void);
void mod(void)
{	top = (top) ? stack[(char) S--] % top : stack[(char) S--]; }
void slash(void);
void slash(void)
{	top = (top) ? stack[(char) S--] / top : (stack[(char) S--], 0); }
void umsta(void);
void umsta(void)  // mk: Without long long
{
	int resulth, resultl;
	multiply32(top, stack[(char)S], &resulth, &resultl);
	//d = (unsigned long long int)top;
	//m = (unsigned long long int)stack[(char) S];
	//m *= d;
	top = resulth;
	stack[(char) S] = resultl; }
void star(void);
void star(void)
{	top *= stack[(char) S--]; }
void mstar(void);
void mstar(void)  // mk: Without long long
{	d = (signed long long int)top;
	m = (signed long long int)stack[(char) S];
	m *= d;
	top = (signed long)(m >> 32);
	stack[(char) S] = (signed long)m; }
void ssmod(void);
void ssmod(void)  // mk: Without long long
{	d = (signed long long int)top;
	m = (signed long long int)stack[(char) S];
	n = (signed long long int)stack[(char) S - 1];
	n += m << 32;
	pop;
	top = (signed long)(n / d);
	stack[(char) S] = (signed long)(n%d); }
void stasl(void);
void stasl(void)  // mk: Without long long
{	d = (signed long long int)top;
	m = (signed long long int)stack[(char) S];
	n = (signed long long int)stack[(char) S - 1];
	n += m << 32;
	pop; pop;
	top = (signed long)(n / d); }
void pick(void);
void pick(void)
{	top = stack[(char) S - (char)top]; }
void pstor(void);
void pstor(void)
{	data[top>>2] += stack[(char) S--], pop; }
void dstor(void);
void dstor(void)
{	data[(top>>2) + 1] = stack[(char) S--];
	data[top>>2] = stack[(char) S--];
	pop; }
void dat(void);
void dat(void)
{	push data[top>>2];
	top = data[(top>>2) + 1]; }
void count(void);
void count(void)
{	stack[(char) ++S] = top + 1;
	top = cData[top]; }
void max_(void); // mk: max is reserved.
void max_(void)
{	if (top < stack[(char) S]) pop;
	else (char) S--; }
void min_(void); // mk: min is reserved.
void min_(void)
{	if (top < stack[(char) S]) (char) S--;
	else pop; }
void(*primitives[64])(void);
void(*primitives[64])(void) = {
	/* case 0 */ nop,
	/* case 1 */ bye,
	/* case 2 */ qrx,
	/* case 3 */ txsto,
	/* case 4 */ docon,
	/* case 5 */ dolit,
	/* case 6 */ dolist,
	/* case 7 */ exitt,
	/* case 8 */ execu,
	/* case 9 */ donext,
	/* case 10 */ qbran,
	/* case 11 */ bran,
	/* case 12 */ store,
	/* case 13 */ at,
	/* case 14 */ cstor,
	/* case 15 */ cat,
	/* case 16  rpat, */ nop,
	/* case 17  rpsto, */ nop,
	/* case 18 */ rfrom,
	/* case 19 */ rat,
	/* case 20 */ tor,
	/* case 21 spat, */ nop,
	/* case 22 spsto, */ nop,
	/* case 23 */ drop,
	/* case 24 */ dup,
	/* case 25 */ swap,
	/* case 26 */ over,
	/* case 27 */ zless,
	/* case 28 */ andd,
	/* case 29 */ orr,
	/* case 30 */ xorr,
	/* case 31 */ uplus,
	/* case 32 */ next,
	/* case 33 */ qdup,
	/* case 34 */ rot,
	/* case 35 */ ddrop,
	/* case 36 */ ddup,
	/* case 37 */ plus,
	/* case 38 */ inver,
	/* case 39 */ negat,
	/* case 40 */ dnega,
	/* case 41 */ subb,
	/* case 42 */ abss,
	/* case 43 */ equal,
	/* case 44 */ uless,
	/* case 45 */ less,
	/* case 46 */ ummod,
	/* case 47 */ msmod,
	/* case 48 */ slmod,
	/* case 49 */ mod,
	/* case 50 */ slash,
	/* case 51 */ umsta,
	/* case 52 */ star,
	/* case 53 */ mstar,
	/* case 54 */ ssmod,
	/* case 55 */ stasl,
	/* case 56 */ pick,
	/* case 57 */ pstor,
	/* case 58 */ dstor,
	/* case 59 */ dat,
	/* case 60 */ count,
	/* case 61 */ dovar,
	/* case 62 */ max_,
	/* case 63 */ min_,
};

void execute(unsigned char code);
void execute(unsigned char code)
{	if (code < 64) { primitives[bytecode](); }
	else { printf("\n Illegal code= %x P= %x", code, P); } }
/*
* Main Program
*/
int main(int ac, char* av[])
{	P = 0;
	WP = 4;
	IP = 0;
	S = 0;
	R = 0;
	top = 0;
	cData = (unsigned char *)data;
	printf("\nceForth v2.3, 13jul17cht\n");
	while (TRUE) {
		bytecode = (unsigned char)cData[P++];
		execute(bytecode); 
	} }
/* End of ceforth_23.cpp */

