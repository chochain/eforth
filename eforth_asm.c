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
XA BRAN, QBRAN, DONXT;
XA DOTQ, STRQ, ABORTQ;
XA NOP, TOR;
//
// return stack for branching ops
//
XA aRack[ASSEM_RACK_SZ] = { 0 };    // return stack (independent of Forth return stack for modulization)
U8 *aByte    = 0;                   //
U8 aR        = 0;                   // return stack index
XA aPC, aThread;                    // program counter, pointer to previous word
//
// stack op macros
//
#define SET(d, v)      (*(XA*)(aByte+d)=(XA)(v))
#define STORE(v)       { SET(aPC, (v)); aPC+=CELLSZ; }
#define	RPUSH(v)       (aRack[++aR] = (XA)(v))
#define	RPOP()         (aRack[aR--])
#define VAR(a, i)      ((a)+CELLSZ*(i))

void _dump(int b, int u) {
    // dump memory between previous word and this
    DEBUG("%s", "\n    :");
    for (int i=b; i<u; i+=CELLSZ) {
        DEBUG(" %04x", *(XA*)(aByte+i));
    }
    DEBUG("%c", '\n');
}
void _header(int lex, const char *seq) {
    STORE(aThread);                           // point to previous word
    if (aThread) _dump(aThread-sizeof(XA), aPC-sizeof(XA));  // dump data from previous word to current word
    aThread = aPC;                            // keep pointer to this word

    aByte[aPC++] = lex;                       // length of word (with optional fIMMED or fCOMPO flags)
    U32 len = lex & 0x1f;                     // Forth allows word max length 31
    for (U32 i = 0; i < len; i++) {           // memcpy word string
        aByte[aPC++] = seq[i];
    }
    DEBUG("%04x: ", aPC);
    DEBUG("%s", seq);
}
int _code(const char *seg, int len, ...) {
    _header(strlen(seg), seg);
    int addr = aPC;                           // keep address of current word
    va_list argList;
    va_start(argList, len);
    for (; len; len--) {                      // copy bytecodes
        U8 b = (U8)va_arg(argList, int);
        aByte[aPC++] = b;
        DEBUG(" %02x", b);
    }
    va_end(argList);
    return addr;
}
#define DATACPY(n) {                            \
    va_list argList;                            \
	va_start(argList, n);						\
	for (; n; n--) {							\
		U32 j = va_arg(argList, U32);			\
		if (j==NOP) continue;					\
		STORE(j);								\
		DEBUG(" %04x", j);						\
	}											\
	va_end(argList);							\
}
int _colon(const char *seg, int len, ...) {
    _header(strlen(seg), seg);
    DEBUG(" %s", ":0006");
    int addr = aPC;
    STORE(opENTER);
    DATACPY(len);
    return addr;
}
int _immed(const char *seg, int len, ...) {
    _header(fIMMED | strlen(seg), seg);
    DEBUG(" %s", "i0006");
    int addr = aPC;
    STORE(opENTER);
    DATACPY(len);
    return addr;
}
int _label(int len, ...) {
    SHOWOP("LABEL");
    int addr = aPC;
    // label has no opcode here
    DATACPY(len);
    return addr;
}
void _begin(int len, ...) {
    SHOWOP("BEGIN");
    RPUSH(aPC);                     // keep current address for looping
    DATACPY(len);
}
void _again(int len, ...) {
    SHOWOP("AGAIN");
    STORE(BRAN);
    STORE(RPOP());                  // store return address
    DATACPY(len);
}
void _until(int len, ...) {
    SHOWOP("UNTIL");
    STORE(QBRAN);                   // conditional branch
    STORE(RPOP());                  // loop begin address
    DATACPY(len);
}
void _while(int len, ...) {
    SHOWOP("WHILE");
    STORE(QBRAN);
    STORE(0);                       // branching address
    int k = RPOP();
    RPUSH(aPC - CELLSZ);
    RPUSH(k);
    DATACPY(len);
}
void _repeat(int len, ...) {
    SHOWOP("REPEAT");
    STORE(BRAN);
    STORE(RPOP());
    SET(RPOP(), aPC);
    DATACPY(len);
}
void _if(int len, ...) {
    SHOWOP("IF");
	STORE(QBRAN);
    RPUSH(aPC);                    // keep for ELSE-THEN
    STORE(0);                      // reserved for branching address
    DATACPY(len);
}
void _else(int len, ...) {
    SHOWOP("ELSE");
    STORE(BRAN);
    STORE(0);
    SET(RPOP(), aPC);
    RPUSH(aPC - CELLSZ);
    DATACPY(len);
}
void _then(int len, ...) {
    SHOWOP("THEN");
    SET(RPOP(), aPC);
    DATACPY(len);
}
void _for(int len, ...) {
    SHOWOP("FOR");
    STORE(TOR);
    RPUSH(aPC);
    DATACPY(len);
}
void _nxt(int len, ...) {          // _next() is multi-defined in vm
    SHOWOP("NEXT");
    STORE(DONXT);
    STORE(RPOP());
    DATACPY(len);
}
void _aft(int len, ...) {
    SHOWOP("AFT");
    STORE(BRAN);
    STORE(0);
    RPOP();
    RPUSH(aPC);
    RPUSH(aPC - CELLSZ);
    DATACPY(len);
}
#define STRCPY(op, seq) {                           \
	STORE(op);                                      \
	int len = strlen(seq);							\
	aByte[aPC++] = len;								\
	for (int i = 0; i < len; i++) {					\
		aByte[aPC++] = seq[i];						\
	}												\
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
//
// assembler macros (calculate number of parameters by compiler)
//
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
#define _NEXT(...)           _nxt(_NARG(__VA_ARGS__), __VA_ARGS__)
#define _AFT(...)            _aft(_NARG(__VA_ARGS__), __VA_ARGS__)

int assemble(U8 *rom) {
	aByte = rom;
	aPC   = FORTH_DIC_ADDR;
	aR    = aThread = 0;
	//
	// Kernel variables (in bytecode streams)
	// TIB_ADDR = 0x80
	//
	U8 *dic0 = aByte+aPC;
	XA ta    = FORTH_TVAR_ADDR;
	XA vHLD  = _CODE("HLD",     opDOCON, VAR(ta,0), 0);
	XA vSPAN = _CODE("SPAN",    opDOCON, VAR(ta,1), 0);
	XA vIN   = _CODE(">IN",     opDOCON, VAR(ta,2), 0);
	XA vNTIB = _CODE("#TIB",    opDOCON, VAR(ta,3), 0);
	XA ua    = FORTH_UVAR_ADDR;
	XA vTTIB = _CODE("'TIB",    opDOCON, VAR(ua,0), 0);
	XA vBASE = _CODE("BASE",    opDOCON, VAR(ua,1), 0);
	XA vCNTX = _CODE("CONTEXT", opDOCON, VAR(ua,2), 0);
	XA vCP   = _CODE("CP",      opDOCON, VAR(ua,3), 0);
	XA vLAST = _CODE("LAST",    opDOCON, VAR(ua,4), 0);
	XA vTEVL = _CODE("'EVAL",   opDOCON, VAR(ua,5), 0);
	XA vTABRT= _CODE("'ABORT",  opDOCON, VAR(ua,6), 0);
	XA vTEMP = _CODE("tmp",     opDOCON, VAR(ua,7), 0);
	//
	// Kernel dictionary (primitive proxies)
	//
       NOP   = _CODE("NOP",     opNOP    );
	XA BYE   = _CODE("BYE",     opBYE    );
	XA QRX   = _CODE("?RX",     opQRX    );
	XA TXSTO = _CODE("TX!",     opTXSTO  );
	XA DOCON = _CODE("DOCON",   opDOCON  );
	XA DOLIT = _CODE("DOLIT",   opDOLIT  );
	XA ENTER = _CODE("ENTER",   opENTER  );    // aka DOLIST by Dr. Ting
	XA EXIT  = _CODE("EXIT",    opEXIT   );
	XA EXECU = _CODE("EXECUTE", opEXECU  );
	   DONXT = _CODE("DONEXT",  opDONEXT );
	   QBRAN = _CODE("QBRANCH", opQBRAN  );
	   BRAN  = _CODE("BRANCH",  opBRAN   );
	XA STORE = _CODE("!",       opSTORE  );
	XA AT    = _CODE("@",       opAT     );
	XA CSTOR = _CODE("C!",      opCSTOR  );
	XA CAT   = _CODE("C@",      opCAT    );
	XA RFROM = _CODE("R>",      opRFROM  );
	XA RAT   = _CODE("R@",      opRAT    );
	   TOR   = _CODE(">R",      opTOR    );
	XA DROP  = _CODE("DROP",    opDROP   );
	XA DUP   = _CODE("DUP",     opDUP    );
	XA SWAP  = _CODE("SWAP",    opSWAP   );
	XA OVER  = _CODE("OVER",    opOVER   );
	XA ZLESS = _CODE("0<",      opZLESS  );
	XA AND   = _CODE("AND",     opAND    );
	XA OR    = _CODE("OR",      opOR     );
	XA XOR   = _CODE("XOR",     opXOR    );
	XA UPLUS = _CODE("UM+",     opUPLUS  );
	XA NEXT  = _CODE("NEXT",    opNEXT   );
	XA QDUP  = _CODE("?DUP",    opQDUP   );
	XA ROT   = _CODE("ROT",     opROT    );
	XA DDROP = _CODE("2DROP",   opDDROP  );
	XA DDUP  = _CODE("2DUP",    opDDUP   );
	XA PLUS  = _CODE("+",       opPLUS   );
	XA INVER = _CODE("NOT",     opINVER  );
	XA NEGAT = _CODE("NEGATE",  opNEGAT  );
	XA DNEGA = _CODE("DNEGATE", opDNEGA  );
	XA SUB   = _CODE("-",       opSUB    );
	XA ABS   = _CODE("ABS",     opABS    );
	XA EQUAL = _CODE("=",       opEQUAL  );
	XA ULESS = _CODE("U<",      opULESS  );
	XA LESS  = _CODE("<",       opLESS   );
	XA UMMOD = _CODE("UM/MOD",  opUMMOD  );
	XA MSMOD = _CODE("M/MOD",   opMSMOD  );
	XA SLMOD = _CODE("/MOD",    opSLMOD  );
	XA MOD   = _CODE("MOD",     opMOD    );
	XA SLASH = _CODE("/",       opSLASH  );
	XA UMSTA = _CODE("UM*",     opUMSTA  );
	XA STAR  = _CODE("*",       opSTAR   );
	XA MSTAR = _CODE("M*",      opMSTAR  );
	XA SSMOD = _CODE("*/MOD",   opSSMOD  );
	XA STASL = _CODE("*/",      opSTASL  );
	XA PICK  = _CODE("PICK",    opPICK   );
	XA PSTOR = _CODE("+!",      opPSTOR  );
	XA DSTOR = _CODE("2!",      opDSTOR  );
	XA DAT   = _CODE("2@",      opDAT    );
	XA COUNT = _CODE("COUNT",   opCOUNT  );
	XA MAX   = _CODE("MAX",     opMAX    );
	XA MIN   = _CODE("MIN",     opMIN    );

	XA BLANK = _CODE("BL",      opDOCON, 0x20,   0);
	XA CELL  = _CODE("CELL",    opDOCON, CELLSZ, 0);
	XA CELLP = _CODE("CELL+",   opDOCON, CELLSZ, 0, opPLUS);
	XA CELLM = _CODE("CELL-",   opDOCON, CELLSZ, 0, opSUB );
	XA CELLS = _CODE("CELLS",   opDOCON, CELLSZ, 0, opSTAR);
	XA CELLD = _CODE("CELL/",   opDOCON, CELLSZ, 0, opSLASH);
	XA ONEP  = _CODE("1+",      opDOCON, 1,      0, opPLUS);
	XA ONEM  = _CODE("1-",      opDOCON, 1,      0, opSUB );
	XA DOVAR = _CODE("DOVAR",   opDOVAR);
	//
	// tracing instrumentation (borrow 2 opcodes)
	//
	XA clock   = _CODE("clock",   opSPAT);
	XA trc_on  = _CODE("trc_on",  opRPAT);
	XA trc_off = _CODE("trc_off", opRPSTO);
	//
	// Common Colon Words (in word streams)
	//
	U8 *dic1  = aByte+aPC;
	XA QKEY  = _COLON("?KEY",  QRX, EXIT);
	XA KEY   = _COLON("KEY",   NOP); {
		_BEGIN(QKEY);
		_UNTIL(EXIT);
	}
	XA EMIT  = _COLON("EMIT",    TXSTO, EXIT);
	XA WITHI = _COLON("WITHIN",  OVER, SUB, TOR, SUB, RFROM, ULESS, EXIT);
	XA TCHAR = _COLON(">CHAR",   DOLIT, 0x7f, AND, DUP, DOLIT, 0x7f, BLANK, WITHI); {
		_IF(DROP, DOLIT, 0x5f);
		_THEN(EXIT);
	}
	XA HERE  = _COLON("HERE",    vCP, AT, EXIT);                  // top of dictionary
	XA PAD   = _COLON("PAD",     HERE, DOLIT, 0x50, PLUS, EXIT);  // used 80-byte as output buffer (i.e. pad)
	XA TIB   = _COLON("TIB",     vTTIB, AT, EXIT);                // CC: change PAD,TIB to RAM buffer for R/W
	XA ATEXE = _COLON("@EXECUTE", AT, QDUP); {
		_IF(EXECU);
		_THEN(EXIT);
	}
	XA CMOVE = _COLON("CMOVE", NOP); {
		_FOR(NOP);
		_AFT(OVER, CAT, OVER, CSTOR, TOR, ONEP, RFROM, ONEP);
		_THEN(NOP);
		_NEXT(DDROP, EXIT);
	}
	XA MOVE  = _COLON("MOVE", CELLD); {
		_FOR(NOP);
		_AFT(OVER, AT, OVER, STORE, TOR, CELLP, RFROM, CELLP);
		_THEN(NOP);
		_NEXT(DDROP, EXIT);
	}
	XA FILL = _COLON("FILL", SWAP); {
		_FOR(SWAP);
		_AFT(DDUP, CSTOR, ONEP);
		_THEN(NOP);
		_NEXT(DDROP, EXIT);
	}
	//
	// Number Conversions
	//
	XA DIGIT = _COLON("DIGIT",   DOLIT, 9, OVER, LESS, DOLIT, 7, AND, PLUS, DOLIT, 0x30, PLUS, EXIT);
	XA EXTRC = _COLON("EXTRACT", DOLIT, 0, SWAP, UMMOD, SWAP, DIGIT, EXIT);
	XA BDIGS = _COLON("<#",      PAD, vHLD, STORE, EXIT);
	XA HOLD  = _COLON("HOLD",    vHLD, AT, ONEM, DUP, vHLD, STORE, CSTOR, EXIT);
	XA DIG   = _COLON("#",       vBASE, AT, EXTRC, HOLD, EXIT);
	XA DIGS  = _COLON("#S", NOP); {
		_BEGIN(DIG, DUP);
		_WHILE(NOP);
		_REPEAT(EXIT);
	}
	XA SIGN  = _COLON("SIGN",    ZLESS); {
		_IF(DOLIT, 0x2d, HOLD);
		_THEN(EXIT);
	}
	XA EDIGS = _COLON("#>",      DROP, vHLD, AT, PAD, OVER, SUB, EXIT);
	XA STR   = _COLON("str",     DUP, TOR, ABS, BDIGS, DIGS, RFROM, SIGN, EDIGS, EXIT);
	XA HEX   = _COLON("HEX",     DOLIT, 16, vBASE, STORE, EXIT);
	XA DECIM = _COLON("DECIMAL", DOLIT, 10, vBASE, STORE, EXIT);
	XA UPPER = _COLON("wupper",  DOLIT, 0x5f5f5f5f, AND, EXIT);
	XA TOUPP = _COLON(">upper",  DUP, DOLIT, 0x61, DOLIT, 0x7b, WITHI); {
		_IF(DOLIT, 0x5f, AND);
		_THEN(EXIT);
	}
	XA DIGTQ = _COLON("DIGIT?", TOR, TOUPP, DOLIT, 0x30, SUB, DOLIT, 9, OVER, LESS); {
		_IF(DOLIT, 7, SUB, DUP, DOLIT, 10, LESS, OR);
		_THEN(DUP, RFROM, ULESS, EXIT);
	}
	XA NUMBQ = _COLON("NUMBER?", vBASE, AT, TOR, DOLIT, 0, OVER, COUNT, OVER, CAT, DOLIT, 0x24, EQUAL); {
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
	XA SPACE = _COLON("SPACE", BLANK, EMIT, EXIT);
	XA CHARS = _COLON("CHARS", SWAP, DOLIT, 0, MAX); {
		_FOR(NOP);
		_AFT(DUP, EMIT);
		_THEN(NOP);
		_NEXT(DROP, EXIT);
	}
	XA SPACS = _COLON("SPACES", BLANK, CHARS, EXIT);
	XA TYPE  = _COLON("TYPE", NOP); {
		_FOR(NOP);
		_AFT(COUNT, TCHAR, EMIT);
		_THEN(NOP);
		_NEXT(DROP, EXIT);
	}
	XA CR    = _COLON("CR",   DOLIT, 10, DOLIT, 13, EMIT, EMIT, EXIT);
	XA DOSTR = _COLON("do$",  RFROM, RAT, RFROM, COUNT, PLUS, TOR, SWAP, TOR, EXIT);
	XA STRQ  = _COLON("$\"|", DOSTR, EXIT);
	   DOTQ  = _COLON(".\"|", DOSTR, COUNT, TYPE, EXIT);
	XA DOTR  = _COLON(".R",   TOR, STR, RFROM, OVER, SUB, SPACS, TYPE, EXIT);
	XA UDOTR = _COLON("U.R",  TOR, BDIGS, DIGS, EDIGS, RFROM, OVER, SUB, SPACS, TYPE, EXIT);
	XA UDOT  = _COLON("U.",   BDIGS, DIGS, EDIGS, SPACE, TYPE, EXIT);
	XA DOT   = _COLON(".",    vBASE, AT, DOLIT, 0xa, XOR); {
		_IF(UDOT, EXIT);
		_THEN(STR, SPACE, TYPE, EXIT);
	}
	XA QUEST = _COLON("?", AT, DOT, EXIT);
	//
	// Parser
    //
	XA PARSE0= _COLON("(parse)", vTEMP, CSTOR, OVER, TOR, DUP); {
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
	XA PACKS = _COLON("PACK$",
					   DUP, TOR, DDUP, PLUS, DOLIT, 0xfffc, AND, DOLIT, 0, SWAP, STORE,
					   DDUP, CSTOR, ONEP, SWAP, CMOVE, RFROM,
					   EXIT);
	XA PARSE = _COLON("PARSE",
					   TOR, TIB, vIN, AT, PLUS, vNTIB, AT, vIN, AT, SUB, RFROM,
					   PARSE0, vIN, PSTOR,
					   EXIT);
	XA TOKEN = _COLON("TOKEN", BLANK, PARSE, DOLIT, 0x1f, MIN, HERE, CELLP, PACKS, EXIT);
	XA WORDD = _COLON("WORD",  PARSE, HERE, CELLP, PACKS, EXIT);
	XA NAMET = _COLON("NAME>", COUNT, DOLIT, 0x1f, AND, PLUS, EXIT);
	XA SAMEQ = _COLON("SAME?", DOLIT, 0x1f, AND, CELLD); {
		_FOR(NOP);
		_AFT(OVER, RAT, CELLS, PLUS, AT, UPPER, OVER, RAT, CELLS, PLUS, AT, UPPER, SUB, QDUP); {
			_IF(RFROM, DROP, EXIT);
			_THEN(NOP);
		}
		_THEN(NOP);
		_NEXT(DOLIT, 0, EXIT);
	}
	XA FIND = _COLON("find", SWAP, DUP, AT, vTEMP, STORE, DUP, AT, TOR, CELLP, SWAP); {
		_BEGIN(AT, DUP); {
			_IF(DUP, AT, DOLIT, 0xff3f, AND, UPPER, RAT, UPPER, XOR); {
				_IF(CELLP, DOLIT, 0xffff);
				_ELSE(CELLP, vTEMP, AT, SAMEQ);
				_THEN(NOP);
			}
			_ELSE(RFROM, DROP, SWAP, CELLM, SWAP, EXIT);
			_THEN(NOP);
		}
		_WHILE(CELLM, CELLM);
		_REPEAT(RFROM, DROP, SWAP, DROP, CELLM, DUP, NAMET, SWAP, EXIT);
	}
	XA NAMEQ = _COLON("NAME?", vCNTX, FIND, EXIT);
	//
	// Terminal Input
	//
	XA HATH  = _COLON("^H", TOR, OVER, RFROM, SWAP, OVER, XOR); {
		_IF(DOLIT, 8, EMIT, ONEM, BLANK, EMIT, DOLIT, 8, EMIT);
		_THEN(EXIT);
	}
	XA TAP   = _COLON("TAP", DUP, EMIT, OVER, CSTOR, ONEP, EXIT);
	XA KTAP  = _COLON("kTAP", DUP, DOLIT, 0xd, XOR, OVER, DOLIT, 0xa, XOR, AND); {
		_IF(DOLIT, 8, XOR); {
			_IF(BLANK, TAP);
			_ELSE(HATH);
			_THEN(EXIT);
		}
		_THEN(DROP, SWAP, DROP, DUP, EXIT);
	}
	XA ACCEP = _COLON("ACCEPT", OVER, PLUS, OVER); {
		_BEGIN(DDUP, XOR);
		_WHILE(KEY, DUP, BLANK, SUB, DOLIT, 0x5f, ULESS); {
			_IF(TAP);
			_ELSE(KTAP);
			_THEN(NOP);
		}
		_REPEAT(DROP, OVER, SUB, EXIT);
	}
	XA EXPEC = _COLON("EXPECT", ACCEP, vSPAN, STORE, DROP, EXIT);
	XA QUERY = _COLON("QUERY", TIB, DOLIT, 0x50, ACCEP, vNTIB, STORE, DROP, DOLIT, 0, vIN, STORE, EXIT);
	//
	// Text Interpreter
	//
	/* QUIT Forth main interpreter loop
	   QUERY/ACCEPT - start the interpreter loop <-----------<------.
	   TOKEN/PARSE - get a space delimited word                      \
	   @EXECUTE - attempt to look up that word in the dictionary      \
	   NAME?/find - was the word found?                                ^
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
	       NUMBER? - Can the word be treated as a number?              |
	       |-Yes:                                                      |
    	   | \-Are we in compile mode?                                 |
	       |   |-Yes:                                                  |
    	   |   | \-Compile a literal into the dictionary >------>----->.
	       |   \-No:                                                   |
    	   |     \-Push the number to the variable stack >------>----->.
    	   \-No:                                                      /
	        \-An Error has occurred, prXA out an error message >---->
	*/
	XA ABORT  = _COLON("ABORT", vTABRT, ATEXE);
	   ABORTQ = _COLON("abort\"", NOP); {
		_IF(DOSTR, COUNT, TYPE, ABORT);
		_THEN(DOSTR, DROP, EXIT);
	}
	XA ERROR = _COLON("ERROR", SPACE, COUNT, TYPE, DOLIT, 0x3f, EMIT, DOLIT, 0x1b, EMIT, CR, ABORT);
	XA INTER = _COLON("$INTERPRET", NAMEQ, QDUP); {
		_IF(CAT, DOLIT, fCOMPO, AND);
		_ABORTQ(" compile only");
	}
	XA INTER0= _LABEL(EXECU, EXIT); {
		_THEN(NUMBQ);          // continue from INTER
		_IF(EXIT);
		_ELSE(ERROR);
		_THEN(NOP);
	}
	XA LBRAC = _IMMED("[", DOLIT, INTER, vTEVL, STORE, EXIT);
	XA DOTOK = _COLON(".OK", CR, DOLIT, INTER, vTEVL, AT, EQUAL); {
		_IF(TOR, TOR, TOR, DUP, DOT, RFROM, DUP, DOT, RFROM, DUP, DOT, RFROM, DUP, DOT); {
			_DOTQ(" ok>");
		}
		_THEN(EXIT);
	}
	XA EVAL  = _COLON("EVAL", NOP); {
		_BEGIN(TOKEN, DUP, AT);
		_WHILE(vTEVL, ATEXE);
		_REPEAT(DROP, DOTOK, EXIT);
	}
	XA QUIT  = _COLON("QUIT", DOLIT, FORTH_TIB_ADDR, vTTIB, STORE, LBRAC); {
		_BEGIN(QUERY, EVAL);
		_AGAIN(NOP);
	}
	//
	// Colon Word Compiler
	//
	XA COMMA = _COLON(",",       HERE, DUP, CELLP, vCP, STORE, STORE, EXIT);
	XA LITER = _IMMED("LITERAL", DOLIT, DOLIT, COMMA, COMMA, EXIT);
	XA ALLOT = _COLON("ALLOT",   vCP, PSTOR, EXIT);
	XA STRCQ = _COLON("$,\"",    DOLIT, 0x22, WORDD, COUNT, PLUS, vCP, STORE, EXIT);
	XA UNIQU = _COLON("?UNIQUE", DUP, NAMEQ, QDUP); {
		_IF(COUNT, DOLIT, 0x1f, AND, SPACE, TYPE); {
			_DOTQ(" reDef");
		}
		_THEN(DROP, EXIT);
	}
	XA SNAME = _COLON("$,n", DUP, AT); {
		_IF(UNIQU, DUP, NAMET, vCP, STORE, DUP, vLAST, STORE, CELLM, vCNTX, AT, SWAP, STORE, EXIT);
		_THEN(ERROR);
	}
	XA TICK  = _COLON("'", TOKEN, NAMEQ); {
		_IF(EXIT);
		_THEN(ERROR);
	}
	XA BCOMP = _IMMED("[COMPILE]", TICK, COMMA, EXIT);
	XA COMPI = _COLON("COMPILE",  RFROM, DUP, AT, COMMA, CELLP, TOR, EXIT);
	XA SCOMP = _COLON("$COMPILE", NAMEQ, QDUP); {
		_IF(AT, DOLIT, fIMMED, AND); {
			_IF(EXECU);
			_ELSE(COMMA);
			_THEN(EXIT);
		}
		_THEN(NUMBQ);
		_IF(LITER, EXIT);
		_THEN(ERROR);
	}
	XA OVERT = _COLON("OVERT", vLAST, AT, vCNTX, STORE, EXIT);
	XA RBRAC = _COLON("]", DOLIT, SCOMP, vTEVL, STORE, EXIT);
	XA COLON = _COLON(":", TOKEN, SNAME, RBRAC, DOLIT, 0x6, COMMA, EXIT);
	XA SEMIS = _IMMED(";", DOLIT, EXIT, COMMA, LBRAC, OVERT, EXIT);
	//
	// Debugging Tools
	//
	XA DMP   = _COLON("dm+", OVER, DOLIT, 6, UDOTR); {
		_FOR(NOP);
		_AFT(DUP, AT, DOLIT, 9, UDOTR, CELLP);
		_THEN(NOP);
		_NEXT(EXIT);
	}
	XA DUMP  = _COLON("DUMP", vBASE, AT, TOR, HEX, DOLIT, 0x1f, PLUS, DOLIT, 0x20, SLASH); {              // 
		_FOR(NOP);
		_AFT(CR, DOLIT, 8, DDUP, DMP, TOR, SPACE, CELLS, TYPE, RFROM);
		_THEN(NOP);
		_NEXT(DROP, RFROM, vBASE, STORE, EXIT);      // restore BASE
	}
	/*
	XA TNAME = _COLON(">NAME", vCNTX); {            // scan through the dictionary to find the name
		_BEGIN(AT, QDUP);                            // this is inefficient, see following
		_WHILE(DDUP, NAMET, XOR); {
			_IF(CELLM);                              // fetch previous word, CC: Ting uses ONEM, a bug
			_ELSE(SWAP, DROP, EXIT);
			_THEN(NOP);
		}
		_REPEAT(SWAP, DROP, EXIT);
	}
	*/
	XA TNAME = _COLON(">NAME", NOP); {              // CC: my implementation
		_BEGIN(CELLM, DUP, CAT, DOLIT, 0x7f, AND, DOLIT, 0x20, LESS);
		_UNTIL(EXIT);
	}
	XA DOTID = _COLON(".ID",   COUNT, DOLIT, 0x1f, AND, TYPE, SPACE, EXIT);
	XA WORDS = _COLON("WORDS", CR, vCNTX, DOLIT, 0, vTEMP, STORE); {
		_BEGIN(AT, QDUP);
		_WHILE(DUP, SPACE, DOTID, CELLM, vTEMP, AT, DOLIT, 0xa, LESS); {
			_IF(DOLIT, 1, vTEMP, PSTOR);
			_ELSE(CR, DOLIT, 0, vTEMP, STORE);
			_THEN(NOP);
		}
		_REPEAT(EXIT);
	}
	XA FORGT = _COLON("FORGET", TOKEN, NAMEQ, QDUP); {
		_IF(CELLM, DUP, vCP, STORE, AT, DUP, vCNTX, STORE, vLAST, STORE, DROP, EXIT);
		_THEN(ERROR);
	}
	XA COLD  = _COLON("COLD", CR); { _DOTQ("eForth16 in C v4.0"); }
	XA DOTQ1 = _LABEL(CR, QUIT);
	//
	// Structure Compiler
	//
	XA iTHEN  = _IMMED("THEN",    HERE, SWAP, STORE, EXIT);
	XA iFOR   = _IMMED("FOR",     COMPI, TOR, HERE, EXIT);
	XA iBEGIN = _IMMED("BEGIN",   HERE, EXIT);
	XA iNEXT  = _IMMED("NEXT",    COMPI, DONXT, COMMA, EXIT);
	XA iUNTIL = _IMMED("UNTIL",   COMPI, QBRAN, COMMA, EXIT);
	XA iAGAIN = _IMMED("AGAIN",   COMPI, BRAN,  COMMA, EXIT);
	XA iIF    = _IMMED("IF",      COMPI, QBRAN, HERE, DOLIT, 0, COMMA, EXIT);
	XA iAHEAD = _IMMED("AHEAD",   COMPI, BRAN,  HERE, DOLIT, 0, COMMA, EXIT);
	XA iREPEA = _IMMED("REPEAT",  iAGAIN, iTHEN, EXIT);
	XA iAFT   = _IMMED("AFT",     DROP, iAHEAD, HERE, SWAP, EXIT);
	XA iELSE  = _IMMED("ELSE",    iAHEAD, SWAP, iTHEN, EXIT);
	XA iWHEN  = _IMMED("WHEN",    iIF, OVER, EXIT);
	XA iWHILE = _IMMED("WHILE",   iIF, SWAP, EXIT);
	XA iABRTQ = _IMMED("ABORT\"", DOLIT, ABORTQ, HERE, STORE, STRCQ, EXIT);
	XA iSTRQ  = _IMMED("$\"",     DOLIT, STRQ, HERE, STORE, STRCQ, EXIT);
	XA iDOTQ  = _IMMED(".\"",     DOLIT, DOTQ, HERE, STORE, STRCQ, EXIT);

	XA CODE   = _COLON("CODE",    TOKEN, SNAME, OVERT, EXIT);
	XA CREAT  = _COLON("CREATE",  CODE, DOLIT, ((opNEXT<<8)|opDOVAR), COMMA, EXIT);
	XA VARIA  = _COLON("VARIABLE",CREAT, DOLIT, 0, COMMA, EXIT);
	XA CONST  = _COLON("CONSTANT",CODE, DOLIT, ((opNEXT<<8)|opDOCON), COMMA, COMMA, EXIT);
	XA iDOTPR = _IMMED(".(",      DOLIT, 0x29, PARSE, TYPE, EXIT);
	XA iBKSLA = _IMMED("\\",      DOLIT, 0xa,  WORDD, DROP,  EXIT);
	XA iPAREN = _IMMED("(",       DOLIT, 0x29, PARSE, DDROP, EXIT);
	XA ONLY   = _COLON("COMPILE-ONLY", DOLIT, fCOMPO, vLAST, AT, PSTOR, EXIT);
	XA IMMED  = _COLON("IMMEDIATE",    DOLIT, fIMMED, vLAST, AT, PSTOR, EXIT);

	int XDIC  = aPC;                                   // End of dictionary
	int sz    = strlen("IMMEDIATE");                   // size of last word
	int last  = IMMED - (sz + (-sz & (CELLSZ-1)));     // name field of last word
	//
	// Setup Boot Vector
	//
	aPC = FORTH_BOOT_ADDR;
	XA RESET = _LABEL(opENTER, COLD);
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
	aPC = FORTH_UVAR_ADDR;
	XA USER  = _LABEL(FORTH_TIB_SZ, 0x10, last, XDIC, last, INTER, QUIT, 0);

	return XDIC;
}
