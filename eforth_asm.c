#include <stdarg.h>
#include "eforth.h"
#include "eforth_asm.h"
//
// Forth Macro Assembler
//
#define fIMMED  0x80           		// immediate flag
#define fCOMPO  0x40                // compile only flag
//
// variables to keep branching addresses
//
int BRAN, QBRAN, DONXT, ABORTQ;
int DOTQ, STRQ, TOR, NOP;
//
// return stack for branching ops
//
XA aRack[ASSEM_RACK_SZ] = { 0 };    // return stack (independent of Forth return stack for modulization)
U8 *aByte    = 0;                   //
U8 aR        = 0;                   // return stack index
XA aP, aThread;                     // pointer to previous word
//
// stack op macros
//
#define SET(d, v)      (*(XA*)(aByte+d)=(v))
#define DATA(v)        { SET(aP, (v)); aP+=CELLSZ; }
#define	PUSH(v)        (aRack[++aR] = (XA)(v))
#define	POP()          (aRack[aR--])
#define VAR(a, i)      ((a)+CELLSZ*(i))
//
// tracing/logging macros
//
#if    ASSEM_DUMP
#define DEBUG(s, v)     printf(s, v)
#define SHOWOP(op)      printf("\n%04x: %s\t", aP, op)
#else  // ASSEM_DUMP
#define DEBUG(s, v)
#define SHOWOP(op)
#endif // ASSEM_DUMP

void _dump(int b, int u) {
	// dump memory between previous word and this
	DEBUG("%s", "\n    :");
	for (int i=b; b && i<u; i+=CELLSZ) {
		DEBUG(" %08x", *(XA*)(aByte+i));
	}
	DEBUG("%c", '\n');
}
void _header(int lex, const char *seq) {
	DATA(aThread);                            // point to previous word
	_dump(aThread, aP);                       // dump data from previous word to current word
	aThread = aP;                             // keep pointer to this word

	aByte[aP++] = lex;                        // length of word (with optional fIMMED or fCOMPO flags)
	U32 len = lex & 0x1f;                     // Forth allows word max length 31
	for (U32 i = 0; i < len; i++) {           // memcpy word string
		aByte[aP++] = seq[i];
	}
	while (aP&(CELLSZ-1)) { aByte[aP++]=0; }  // padding cell alignment

	DEBUG("%04x: ", aP);
	DEBUG("%s", seq);
}
int _code(const char *seg, int len, ...) {
    _header(strlen(seg), seg);
	int addr = aP;                            // keep address of current word
	va_list argList;
	va_start(argList, len);
	for (; len; len--) {                      // copy bytecodes
		U8 b = (U8)va_arg(argList, int);
		aByte[aP++] = b;
		DEBUG(" %02x", b);
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
		DATA(j);                      \
		DEBUG(" %04x", j);            \
	}                                 \
	va_end(argList);                  \
}
int _colon(const char *seg, int len, ...) {
    _header(strlen(seg), seg);
	DEBUG(" %s", ":0006");
	int addr = aP;
	DATA(opDOLIST);
	DATACPY(len);
	return addr;
}
int _immed(const char *seg, int len, ...) {
    _header(fIMMED | strlen(seg), seg);
	DEBUG(" %s", "i0006");
	int addr = aP;
	DATA(opDOLIST);
    DATACPY(len);
	return addr;
}
int _label(int len, ...) {
	SHOWOP("LABEL");
	int addr = aP;
	// label has no opcode here
    DATACPY(len);
	return addr;
}
void _begin(int len, ...) {
	SHOWOP("BEGIN");
	PUSH(aP);                      // keep current address for looping
    DATACPY(len);
}
void _again(int len, ...) {
	SHOWOP("AGAIN");
	DATA(BRAN);
	DATA(POP());                   // store return address
    DATACPY(len);
}
void _until(int len, ...) {
	SHOWOP("UNTIL");
	DATA(QBRAN);                   // conditional branch
	DATA(POP());                   // loop begin address
    DATACPY(len);
}
void _while(int len, ...) {
	SHOWOP("WHILE");
	DATA(QBRAN);
	DATA(0);                       // branching address
	int k = POP();
	PUSH(aP - CELLSZ);
	PUSH(k);
    DATACPY(len);
}
void _repeat(int len, ...) {
	SHOWOP("REPEAT");
	DATA(BRAN);
	DATA(POP());
	SET(POP(), aP);
    DATACPY(len);
}
void _if(int len, ...) {
	SHOWOP("IF");
	DATA(QBRAN);
	PUSH(aP);                      // keep for ELSE-THEN
	DATA(0);                       // reserved for branching address
    DATACPY(len);
}
void _else(int len, ...) {
	SHOWOP("ELSE");
	DATA(BRAN);
	DATA(0);
	SET(POP(), aP);
	PUSH(aP - CELLSZ);
    DATACPY(len);
}
void _then(int len, ...) {
	SHOWOP("THEN");
	SET(POP(), aP);
    DATACPY(len);
}
void _for(int len, ...) {
	SHOWOP("FOR");
	DATA(TOR);
	PUSH(aP);
    DATACPY(len);
}
void _nxt(int len, ...) {          // _next() is multi-defined in vm
	SHOWOP("NEXT");
	DATA(DONXT);
	DATA(POP());
    DATACPY(len);
}
void _aft(int len, ...) {
	SHOWOP("AFT");
	DATA(BRAN);
	DATA(0);
	POP();
	PUSH(aP);
	PUSH(aP - CELLSZ);
    DATACPY(len);
}
#define STRCPY(op, seq) {                    \
    DATA(op);                                \
	int len = strlen(seq);                   \
	aByte[aP++] = len;                       \
	for (int i = 0; i < len; i++) {          \
		aByte[aP++] = seq[i];                \
	}                                        \
	while (aP&(CELLSZ-1)) { aByte[aP++]=0; } \
	}
void _DOTQ(const char *seq) {
	SHOWOP("DOTQ");
	DEBUG("%s", seq);
	STRCPY(DOTQ, seq);
}
void _STRQ(const char *seq) {
	SHOWOP("STRQ");
	DEBUG("%s", seq);
	STRCPY(STRQ, seq);
}
void _ABORTQ(const char *seq) {
	SHOWOP("ABORTQ");
	DEBUG("%s", seq);
	STRCPY(ABORTQ, seq);
}

int assemble(U8 *rom) {
	aByte = rom;
	aP    = FORTH_DIC_ADDR;
	aR    = aThread = 0;
    //
	// Kernel variables (in bytecode streams)
	// FORTH_TIB_ADDR = 0x80
	//
	int ta    = FORTH_TVAR_ADDR;
	int vHLD  = _CODE("HLD",     opDOCON, opNEXT, 0, 0, VAR(ta,0), 0, 0, 0);
	int vSPAN = _CODE("SPAN",    opDOCON, opNEXT, 0, 0, VAR(ta,1), 0, 0, 0);
	int vIN   = _CODE(">IN",     opDOCON, opNEXT, 0, 0, VAR(ta,2), 0, 0, 0);
	int vNTIB = _CODE("#TIB",    opDOCON, opNEXT, 0, 0, VAR(ta,3), 0, 0, 0);
	int ua    = FORTH_UVAR_ADDR;
	int vTTIB = _CODE("'TIB",    opDOCON, opNEXT, 0, 0, VAR(ua,0), 0, 0, 0);
	int vBASE = _CODE("BASE",    opDOCON, opNEXT, 0, 0, VAR(ua,1), 0, 0, 0);
	int vCNTX = _CODE("CONTEXT", opDOCON, opNEXT, 0, 0, VAR(ua,2), 0, 0, 0);
	int vCP   = _CODE("CP",      opDOCON, opNEXT, 0, 0, VAR(ua,3), 0, 0, 0);
	int vLAST = _CODE("LAST",    opDOCON, opNEXT, 0, 0, VAR(ua,4), 0, 0, 0);
	int vTEVL = _CODE("'EVAL",   opDOCON, opNEXT, 0, 0, VAR(ua,5), 0, 0, 0);
	int vTABRT= _CODE("'ABORT",  opDOCON, opNEXT, 0, 0, VAR(ua,6), 0, 0, 0);
	int vTEMP = _CODE("tmp",     opDOCON, opNEXT, 0, 0, VAR(ua,7), 0, 0, 0);
	//
	// Kernel dictionary (primitive proxies)
	//
	    NOP   = _CODE("NOP",     opNOP,   opNEXT, 0, 0);
	int BYE   = _CODE("BYE",     opBYE,   opNEXT, 0, 0);
	int QRX   = _CODE("?RX",     opQRX,   opNEXT, 0, 0);
	int TXSTO = _CODE("TX!",     opTXSTO, opNEXT, 0, 0);
	int DOCON = _CODE("DOCON",   opDOCON, opNEXT, 0, 0);
	int DOLIT = _CODE("DOLIT",   opDOLIT, opNEXT, 0, 0);
	int DOLST = _CODE("DOLIST",  opDOLIST,opNEXT, 0, 0);
	int EXIT  = _CODE("EXIT",    opEXIT,  opNEXT, 0, 0);
	int EXECU = _CODE("EXECUTE", opEXECU, opNEXT, 0, 0);

	    DONXT = _CODE("DONEXT",  opDONEXT,opNEXT, 0, 0);
	    QBRAN = _CODE("QBRANCH", opQBRAN, opNEXT, 0, 0);
	    BRAN  = _CODE("BRANCH",  opBRAN,  opNEXT, 0, 0);

	int STORE = _CODE("!",       opSTORE, opNEXT, 0, 0);
	int AT    = _CODE("@",       opAT,    opNEXT, 0, 0);
	int CSTOR = _CODE("C!",      opCSTOR, opNEXT, 0, 0);
	int CAT   = _CODE("C@",      opCAT,   opNEXT, 0, 0);
	int RFROM = _CODE("R>",      opRFROM, opNEXT, 0, 0);
	int RAT   = _CODE("R@",      opRAT,   opNEXT, 0, 0);
	    TOR   = _CODE(">R",      opTOR,   opNEXT, 0, 0);
	int DROP  = _CODE("DROP",    opDROP,  opNEXT, 0, 0);
	int DUP   = _CODE("DUP",     opDUP,   opNEXT, 0, 0);
	int SWAP  = _CODE("SWAP",    opSWAP,  opNEXT, 0, 0);
	int OVER  = _CODE("OVER",    opOVER,  opNEXT, 0, 0);
	int ZLESS = _CODE("0<",      opZLESS, opNEXT, 0, 0);
	int AND   = _CODE("AND",     opAND,   opNEXT, 0, 0);
	int OR    = _CODE("OR",      opOR,    opNEXT, 0, 0);
	int XOR   = _CODE("XOR",     opXOR,   opNEXT, 0, 0);
	int UPLUS = _CODE("UM+",     opUPLUS, opNEXT, 0, 0);
	int NEXT  = _CODE("NEXT",    opNEXT,  opNEXT, 0, 0);
	int QDUP  = _CODE("?DUP",    opQDUP,  opNEXT, 0, 0);
	int ROT   = _CODE("ROT",     opROT,   opNEXT, 0, 0);
	int DDROP = _CODE("2DROP",   opDDROP, opNEXT, 0, 0);
	int DDUP  = _CODE("2DUP",    opDDUP,  opNEXT, 0, 0);
	int PLUS  = _CODE("+",       opPLUS,  opNEXT, 0, 0);
	int INVER = _CODE("NOT",     opINVER, opNEXT, 0, 0);
	int NEGAT = _CODE("NEGATE",  opNEGAT, opNEXT, 0, 0);
	int DNEGA = _CODE("DNEGATE", opDNEGA, opNEXT, 0, 0);
	int SUB   = _CODE("-",       opSUB,   opNEXT, 0, 0);
	int ABS   = _CODE("ABS",     opABS,   opNEXT, 0, 0);
	int EQUAL = _CODE("=",       opEQUAL, opNEXT, 0, 0);
	int ULESS = _CODE("U<",      opULESS, opNEXT, 0, 0);
	int LESS  = _CODE("<",       opLESS,  opNEXT, 0, 0);
	int UMMOD = _CODE("UM/MOD",  opUMMOD, opNEXT, 0, 0);
	int MSMOD = _CODE("M/MOD",   opMSMOD, opNEXT, 0, 0);
	int SLMOD = _CODE("/MOD",    opSLMOD, opNEXT, 0, 0);
	int MOD   = _CODE("MOD",     opMOD,   opNEXT, 0, 0);
	int SLASH = _CODE("/",       opSLASH, opNEXT, 0, 0);
	int UMSTA = _CODE("UM*",     opUMSTA, opNEXT, 0, 0);
	int STAR  = _CODE("*",       opSTAR,  opNEXT, 0, 0);
	int MSTAR = _CODE("M*",      opMSTAR, opNEXT, 0, 0);
	int SSMOD = _CODE("*/MOD",   opSSMOD, opNEXT, 0, 0);
	int STASL = _CODE("*/",      opSTASL, opNEXT, 0, 0);
	int PICK  = _CODE("PICK",    opPICK,  opNEXT, 0, 0);
	int PSTOR = _CODE("+!",      opPSTOR, opNEXT, 0, 0);
	int DSTOR = _CODE("2!",      opDSTOR, opNEXT, 0, 0);
	int DAT   = _CODE("2@",      opDAT,   opNEXT, 0, 0);
	int COUNT = _CODE("COUNT",   opCOUNT, opNEXT, 0, 0);
	int MAX   = _CODE("MAX",     opMAX,   opNEXT, 0, 0);
	int MIN   = _CODE("MIN",     opMIN,   opNEXT, 0, 0);

	int BLANK = _CODE("BL",      opDOCON, opNEXT, 0,      0, 0x20,   0, 0, 0);
	int CELL  = _CODE("CELL",    opDOCON, opNEXT, 0,      0, CELLSZ, 0, 0, 0);
	int CELLP = _CODE("CELL+",   opDOCON, opPLUS, opNEXT, 0, CELLSZ, 0, 0, 0);
	int CELLM = _CODE("CELL-",   opDOCON, opSUB,  opNEXT, 0, CELLSZ, 0, 0, 0);
	int CELLS = _CODE("CELLS",   opDOCON, opSTAR, opNEXT, 0, CELLSZ, 0, 0, 0);
	int CELLD = _CODE("CELL/",   opDOCON, opSLASH,opNEXT, 0, CELLSZ, 0, 0, 0);
	int ONEP  = _CODE("1+",      opDOCON, opPLUS, opNEXT, 0, 1,      0, 0, 0);
	int ONEM  = _CODE("1-",      opDOCON, opSUB,  opNEXT, 0, 1,      0, 0, 0);
	int DOVAR = _CODE("DOVAR",   opDOVAR, opNEXT, 0,      0);
	//
	// tracing instrumentation (borrow 2 opcodes)
	//
#if FORTH_TRACE
    int trc_on  = _CODE("trc_on",  opRPAT,  opNEXT, 0, 0);
    int trc_off = _CODE("trc_off", opRPSTO, opNEXT, 0, 0);
#else
    int trc_on  = NOP;
    int trc_off = NOP;
#endif // FORTH_TRACE
	//
	// Common Colon Words (in word streams)
	//
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
	//
	// Number Conversions
	//
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
	//
	// Terminal Output
	//
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
	int STRQ  = _COLON("$\"|",  DOSTR, EXIT);
	    DOTQ  = _COLON(".\"|",  trc_off, DOSTR, COUNT, TYPE, trc_on, EXIT);
	int DOTR  = _COLON(".R",    TOR, STR, RFROM, OVER, SUB, SPACS, TYPE, EXIT);
	int UDOTR = _COLON("U.R",   TOR, BDIGS, DIGS, EDIGS, RFROM, OVER, SUB, SPACS, TYPE, EXIT);
	int UDOT  = _COLON("U.",    BDIGS, DIGS, EDIGS, SPACE, TYPE, EXIT);
	int DOT   = _COLON(".",     vBASE, AT, DOLIT, 0xa, XOR); {
        _IF(UDOT, EXIT);
        _THEN(STR, SPACE, TYPE, EXIT);
    }
	int QUEST = _COLON("?",     AT, DOT, EXIT);

	// Parser

	int PARSE0= _COLON("(parse)", vTEMP, CSTOR, OVER, TOR, DUP); {
        _IF(ONEM, vTEMP, CAT, BLANK, EQUAL); {
            _IF(NOP); {
                _FOR(BLANK, OVER, CAT, SUB, ZLESS, INVER);
                _WHILE(ONEP);
                _NEXT(RFROM, DROP, DOLIT, 0, DUP, EXIT);
                _THEN(RFROM);
            }
            _THEN(OVER, SWAP);
            _FOR(vTEMP, CAT, OVER, CAT, SUB, vTEMP, CAT, BLANK, EQUAL); {
                _IF(ZLESS);
                _THEN(NOP);
            }
            _WHILE(ONEP);
            _NEXT(DUP, TOR);
        }
        _ELSE(RFROM, DROP, DUP, ONEP, TOR);
        _THEN(OVER, SUB, RFROM, RFROM, SUB, EXIT);
        _THEN(OVER, RFROM, SUB, EXIT);                   // CC: this line is questionable
    }
	int PACKS = _COLON("PACK$", DUP, TOR, DDUP, PLUS, DOLIT, 0xfffffffc, AND, DOLIT, 0, SWAP, STORE, DDUP, CSTOR, ONEP, SWAP, CMOVE, RFROM, EXIT);
	int PARSE = _COLON("PARSE", TOR, TIB, vIN, AT, PLUS, vNTIB, AT, vIN, AT, SUB, RFROM, PARSE0, vIN, PSTOR, EXIT);
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
	//
	// Terminal Input
	//
	int HATH  = _COLON("^H", TOR, OVER, RFROM, SWAP, OVER, XOR); {
        _IF(DOLIT, 8, EMIT, ONEM, BLANK, EMIT, DOLIT, 8, EMIT);
        _THEN(EXIT);
    }
	int TAP   = _COLON("TAP", DUP, EMIT, OVER, CSTOR, ONEP, EXIT);
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
	int QUERY = _COLON("QUERY", TIB, DOLIT, 0x50, ACCEP, vNTIB, STORE, DROP, DOLIT, 0, vIN, STORE, EXIT);
	//
	// Text Interpreter
	//
    /* QUIT Forth main interpreter loop
       QUERY/ACCEPT - start the interpreter loop <-----------<------.
	   TOKEN - get a space delimited word                            \
	   find  - attempt to look up that word in the dictionary         \
	   EVAL - was the word found?                                      ^
	   |-Yes:                                                          |
	   |   $INTERPRET - are we in compile mode?                        |
	   |   |-Yes:                                                      ^
	   |   | \-Is the Word an Immediate word?                          |
	   |   |   |-Yes:                                                  |
	   |   |   | \- EXECUTE Execute the word ------------------->----->.
	   |   |   \-No:                                                   |
	   |   |     \-Compile the word into the dictionary -------->----->.
	   |   \-No:                                                       |
	   |     \- EXECUTE Execute the word ----->----------------->----->.
	   \-No:                                                           ^
	       Can the word be treated as a number?                        |
 	       |-Yes:                                                      |
	       | \-Are we in compile mode?                                 |
	       |   |-Yes:                                                  |
	       |   | \-Compile a literal into the dictionary >------>----->.
	       |   \-No:                                                   |
	       |     \-Push the number to the variable stack >------>----->.
	       \-No:                                                      /
	        \-An Error has occurred, print out an error message >---->
    */
	int ABORT  = _COLON("ABORT", vTABRT, ATEXE);
	    ABORTQ = _COLON("abort\"", NOP); {
        _IF(DOSTR, COUNT, TYPE, ABORT);
        _THEN(DOSTR, DROP, EXIT);
    }
	int ERROR = _COLON("ERROR", SPACE, COUNT, TYPE, DOLIT, 0x3f, EMIT, DOLIT, 0x1b, EMIT, CR, ABORT);
	int INTER = _COLON("$INTERPRET", NAMEQ, QDUP); {
        _IF(CAT, DOLIT, fCOMPO, AND);
        _ABORTQ(" compile only");
    }
	int INTER0= _LABEL(EXECU, EXIT); {
        _THEN(NUMBQ);
        _IF(EXIT);
        _ELSE(ERROR);
        _THEN(NOP);
    }
	int LBRAC = _IMMED("[", DOLIT, INTER, vTEVL, STORE, EXIT);
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
	int QUIT  = _COLON("QUIT", DOLIT, FORTH_TIB_ADDR, vTTIB, STORE, LBRAC); {
        _BEGIN(QUERY, EVAL);
        _AGAIN(NOP);
    }
	//
	// Colon Word Compiler
	//
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
        _IF(AT, DOLIT, fIMMED, AND); {
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
	//
	// Debugging Tools
	//
	int DMP   = _COLON("dm+", OVER, DOLIT, 6, UDOTR); {
        _FOR(NOP);
        _AFT(DUP, AT, DOLIT, 9, UDOTR, CELLP);
        _THEN(NOP);
        _NEXT(EXIT);
    }
	int DUMP  = _COLON("DUMP", vBASE, AT, TOR, HEX, DOLIT, 0x1f, PLUS, DOLIT, 0x20, SLASH); {              // 
        _FOR(NOP);
        _AFT(CR, DOLIT, 8, DDUP, DMP, TOR, SPACE, CELLS, TYPE, RFROM);
        _THEN(NOP);
        _NEXT(DROP, RFROM, vBASE, STORE, EXIT);    // restore BASE
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
	//
	// Structure Compiler
	//
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
	int iABRTQ = _IMMED("ABORT\"", DOLIT, ABORTQ, HERE, STORE, STRCQ, EXIT);
	int iSTRQ  = _IMMED("$\"",     DOLIT, STRQ, HERE, STORE, STRCQ, EXIT);
	int iDOTQ  = _IMMED(".\"",     DOLIT, DOTQ, HERE, STORE, STRCQ, EXIT);

	int CODE   = _COLON("CODE",    TOKEN, SNAME, OVERT, EXIT);
	int CREAT  = _COLON("CREATE",  CODE, DOLIT, ((opNEXT<<8)|opDOVAR), COMMA, EXIT);
	int VARIA  = _COLON("VARIABLE",CREAT, DOLIT, 0, COMMA, EXIT);
	int CONST  = _COLON("CONSTANT",CODE, DOLIT, ((opNEXT<<8)|opDOCON), COMMA, COMMA, EXIT);
	int iDOTPR = _IMMED(".(",      DOLIT, 0x29, PARSE, TYPE, EXIT);
	int iBKSLA = _IMMED("\\",      DOLIT, 0xa,  WORDD, DROP,  EXIT);
	int iPAREN = _IMMED("(",       DOLIT, 0x29, PARSE, DDROP, EXIT);
	int ONLY   = _COLON("COMPILE-ONLY", DOLIT, fCOMPO, vLAST, AT, PSTOR, EXIT);
	int IMMED  = _COLON("IMMEDIATE",    DOLIT, fIMMED, vLAST, AT, PSTOR, EXIT);

	int XDIC   = aP;                                    // End of dictionary
	int sz     = strlen("IMMEDIATE");                   // size of last word
	int last   = IMMED - (sz + (-sz & (CELLSZ-1)));     // name field of last word
	//
	// Setup Boot Vector
	//
	aP = FORTH_BOOT_ADDR;
	int RESET = _LABEL(opDOLIST, COLD);
	//
	// Forth internal (user) variables
	//
    //   'TIB    = FORTH_TIB_SIZE (pointer to top of input buffer)
	//   BASE    = 0x10           (numerical base 0xa for decimal, 0x10 for hex)
	//   CONTEXT = IMMED - 12     (pointer to name field of the most recently defined word in dictionary)
	//   CP      = XDIC           (pointer to top of dictionary, first memory location to add new word)
	//   LAST    = IMMED - 12     (pointer to name field of last word in dictionary)
	//   'EVAL   = INTER          ($COMPILE for compiler or $INTERPRET for interpreter)
	//   ABORT   = QUIT           (pointer to error handler, QUIT is the main loop)
	//   tmp     = 0              (scratch pad)
	//
	aP = FORTH_UVAR_ADDR;
	int USER  = _LABEL(FORTH_TIB_SZ, 0x10, last, XDIC, last, INTER, QUIT, 0);

	return XDIC;
}

