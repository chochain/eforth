#include "eforth.h"

U32 rom[] = {
0x000ef606,0x48030000,0x0004444c,0x04000611,0x4e415053,0x0f110204,0x493e0300,0x1104044e, // 0000 ___..._HLD_.__._SPAN____._>IN___
0x23040019,0x04424954,0x00221106,0x49542704,0x11100442,0x4204002c,0x04455341,0x00361112, // 0020 _._#TIB___"._'TIB___,._BASE___6.
0x04504302,0x00401114,0x4e4f4307,0x54584554,0x48111604,0x414c0400,0x18045453,0x05005511, // 0040 _CP___@._CONTEXT___H._LAST___U._
0x41564527,0x111a044c,0x2706005f,0x524f4241,0x111c0454,0x7403006a,0x1e04706d,0x02007611, // 0060 'EVAL____._'ABORT___j._tmp___v._
0x20044c42,0x04007f00,0x4c4c4543,0x87000204,0x4f440500,0x3d524156,0x4e030091,0x9a00504f, // 0080 BL_ ._._CELL__._._DOVAR=_._NOP._
0x59420300,0x00a10145,0x58523f03,0x0300a802,0x03215854,0x440500af,0x4e4f434f,0x0500b604, // 00a0 ._BYE__._?RX__._TX!__._DOCON__._
0x494c4f44,0x00bf0554,0x544e4505,0xc8065245,0x58450400,0xd1075449,0x58450700,0x54554345, // 00c0 DOLIT__._ENTER__._EXIT__._EXECUT
0x00d90845,0x4e4f4406,0x09545845,0x510700e4,0x4e415242,0xee0a4843,0x52420600,0x48434e41, // 00e0 E__._DONEXT__._QBRANCH__._BRANCH
0x0100f90b,0x01030c21,0x080d4001,0x21430201,0x02010d0e,0x130f4043,0x3e520201,0x02011912, // 0100 __._!____@____C!____C@____R>____
0x1f134052,0x523e0201,0x02012514,0x2b152b31,0x2d310201,0x04013116,0x504f5244,0x03013717, // 0120 R@____>R_%__1+_+__1-_1__DROP_7__
0x18505544,0x5304013f,0x19504157,0x4f040146,0x1a524556,0x3002014e,0x01561b3c,0x444e4103, // 0140 DUP_?__SWAP_F__OVER_N__0<_V__AND
0x02015c1c,0x631d524f,0x4f580301,0x01691e52,0x2b4d5503,0x0401701f,0x5458454e,0x04017720, // 0160 ____OR_c__XOR_i__UM+_p__NEXT w__
0x5055443f,0x03017f21,0x22544f52,0x32050187,0x504f5244,0x04018e23,0x50554432,0x01019724, // 0180 ?DUP!___ROT"___2DROP#___2DUP$___
0x019f252b,0x544f4e03,0x0601a426,0x4147454e,0xab274554,0x4e440701,0x54414745,0x01b52845, // 01a0 +%___NOT&___NEGATE'___DNEGATE(__
0xc0292d01,0x42410301,0x01c52a53,0xcc2b3d01,0x3c550201,0x0101d12c,0x01d72d3c,0x2f4d5506, // 01c0 _-)___ABS*___=+___U<,___<-___UM/
0x2e444f4d,0x4d0501dc,0x444f4d2f,0x0401e62f,0x444f4d2f,0x0301ef30,0x31444f4d,0x2f0101f7, // 01e0 MOD.___M/MOD/___/MOD0___MOD1___/
0x0301fe32,0x332a4d55,0x2a010203,0x02020a34,0x0f352a4d,0x2f2a0502,0x36444f4d,0x2a020215, // 0200 2___UM*3___*4___M*5___*/MOD6___*
0x021e372f,0x43495004,0x0224384b,0x39212b02,0x3202022c,0x02323a21,0x3b403202,0x43050238, // 0220 /7___PICK8$__+!9,__2!:2__2@;8__C
0x544e554f,0x03023e3c,0x3e58414d,0x4d030247,0x4e3f4e49,0x45480402,0x43064552,0xd6010a00, // 0240 OUNT<>__MAX>G__MIN?N__HERE_C.___
0x03025500,0x06444150,0x00c5025a,0x01a10020,0x026300d6,0x4c454305,0x8c062b4c,0xd601a100, // 0260 .U__PAD_Z__. .___.c__CELL+__.___
0x05027400,0x4c4c4543,0x008c062d,0x00d601c2,0x43050283,0x534c4c45,0x0c008c06,0x9200d602, // 0280 .t__CELL-__.___.___CELLS__.___._
0x45430502,0x062f4c4c,0x0200008c,0x02a100d6,0x54495706,0x064e4948,0x01c20153,0x01c20128, // 02a0 __CELL/__..__.___WITHIN_S___(___
0x01d4011c,0x02b000d6,0x4f4d4305,0x28064556,0xe5010001,0x16015302,0x10015301,0x2e012801, // 02c0 _____.___CMOVE_(_.___S___S___(_.
0x2e011c01,0xd500eb01,0xd6019402,0x0402c800,0x45564f4d,0x2802a706,0x0d010001,0x0a015303, // 02e0 ___.__._____.___MOVE___(_.___S__
0x05015301,0x7a012801,0x7a011c02,0xfd00eb02,0xd6019402,0x0402ef00,0x4c4c4946,0x28014b06, // 0300 _S___(_z___z__._____.___FILL_K_(
0x00014b01,0x9c032d01,0x2e011001,0x2700eb01,0xd6019403,0x03031700,0x06584548,0x001000c5, // 0320 _K_._-_____.__.'____.___HEX__._.
0x0105003b,0x033700d6,0x43454407,0x4c414d49,0x0a00c506,0x05003b00,0x4800d601,0x49440503, // 0340 ;.___.7__DECIMAL__._.;.___.H__DI
0x06544947,0x000900c5,0x01d90153,0x000700c5,0x01a10160,0x003000c5,0x00d601a1,0x4507035d, // 0360 GIT__._.S____._.`____.0.___.]__E
0x41525458,0xc5065443,0x4b000000,0x4b01e301,0xd6036301,0x02037e00,0x6706233c,0x05000a02, // 0380 XTRACT__...K___K_c__.~__<#_g__._
0x9700d601,0x4f480403,0x0a06444c,0x34010a00,0x0a014301,0x10010500,0xa500d601,0x06230103, // 03a0 __.___HOLD__.__4_C__._____.___#_
0x010a003b,0x03aa0386,0x03bd00d6,0x06532302,0x014303bf,0x03dc00f6,0x03d00100,0x03cc00d6, // 03c0 ;._______.___#S___C__.__.____.__
0x47495304,0x0159064e,0x03f200f6,0x002d00c5,0x00d603aa,0x230203e0,0x013c063e,0x010a000a, // 03e0 _SIGN_Y__.___.-.___.___#>_<__.__
0x01530267,0x00d601c2,0x730303f6,0x43067274,0xc9012801,0xcf039a01,0xe5011c03,0xd603f903, // 0400 g_S____.___str_C_(______________
0x06040a00,0x70707577,0xc5067265,0x605f5f00,0x2300d601,0x753e0604,0x72657070,0xc5014306, // 0420 .___wupper__.__`__.#__>upper_C__
0xc5006100,0xb7007b00,0x5300f602,0x5f00c504,0xd6016000,0x06043500,0x49474944,0x28063f54, // 0440 .a._.{.___.S__._.`__.5__DIGIT?_(
0xc5043c01,0xc2003000,0x0900c501,0xd9015300,0x8500f601,0x0700c504,0x4301c200,0x0a00c501, // 0460 _<__.0.___._.S____.___._.__C__._
0x6601d900,0x1c014301,0xd601d401,0x07045700,0x424d554e,0x063f5245,0x010a003b,0x00c50128, // 0480 .__f_C______.W__NUMBER?_;.__(__.
0x01530000,0x01530244,0x00c50116,0x01ce0024,0x04be00f6,0x014b033b,0x014b012e,0x01530134, // 04a0 ..S_D_S____.$.___.__;_K_._K_4_S_
0x00c50116,0x01ce002d,0x014b0128,0x01c20122,0x0122014b,0x018401a1,0x052000f6,0x01280134, // 04c0 ___.-.__(_K_"___K_"______. _4_(_
0x01280143,0x003b0116,0x045e010a,0x051200f6,0x003b014b,0x020c010a,0x011c01a1,0x00eb012e, // 04e0 C_(___;.__^__.__K_;.________.__.
0x013c04e0,0x00f60122,0x01b2050c,0x0100014b,0x011c051e,0x0194011c,0x00c50194,0x01430000, // 0500 __<_"__.____K_.____________...C_
0x0194011c,0x003b011c,0x00d60105,0x5403048f,0x31064249,0xd6010a00,0x04052e00,0x59454b3f, // 0520 ______;.___.___TIB_1.___..__?KEY
0xd600ac06,0x03053b00,0x0659454b,0x00f60540,0x00d6054c,0x45040547,0x0654494d,0x00d600b3, // 0540 __._.;__KEY_@__.L__.G__EMIT__._.
0x43020556,0x00c50652,0x00c5000a,0x055b000d,0x00d6055b,0x5e020562,0x01280648,0x011c0153, // 0560 V__CR__._._._.[_[__.b__^H_(_S___
0x0153014b,0x00f6016d,0x00c5059c,0x055b0008,0x00820134,0x00c5055b,0x055b0008,0x057600d6, // 0580 K_S_m__.___._.[_4__.[__._.[__.v_
0x41505305,0x82064543,0xd6055b00,0x0505a000,0x52414843,0x014b0653,0x000000c5,0x0128024b, // 05a0 _SPACE__.[__.___CHARS_K__...K_(_
0x05c80100,0x055b0143,0x05c400eb,0x00d6013c,0x3e0505af,0x52414843,0x7f00c506,0x43016000, // 05c0 .___C_[__.__<__.___>CHAR__._.`_C
0x7f00c501,0xb7008200,0xf300f602,0xc5013c05,0xd6005f00,0x0605d200,0x43415053,0x82065345, // 05e0 __._._.___.__<__._._.___SPACES__
0xd605b500,0x0405f700,0x45505954,0x00012806,0x44061901,0x5b05d802,0x1300eb05,0xd6013c06, // 0600 .___.___TYPE_(_.___D___[__.__<__
0x03060700,0x06246f64,0x0122011c,0x0244011c,0x012801a1,0x0128014b,0x062300d6,0x7c222403, // 0620 .___do$___"___D___(_K_(__.#__$"|
0xd6062706,0x03063c00,0x067c222e,0x02440627,0x00d6060c,0x2e020647,0x01280652,0x011c040e, // 0640 _'__.<__."|_'_D____.G__.R_(_____
0x01c20153,0x060c05fe,0x065600d6,0x522e5503,0x9a012806,0xf903cf03,0x53011c03,0xfe01c201, // 0660 S________.V__U.R_(_________S____
0xd6060c05,0x02066c00,0x9a062e55,0xf903cf03,0x0c05a603,0x8700d606,0x062e0106,0x010a003b, // 0680 ____.l__U.____________.___._;.__
0x000a00c5,0x00f6016d,0x068a06ae,0x040e00d6,0x060c05a6,0x069900d6,0x0a063f01,0xd6069b01, // 06a0 _._.m__._____._______.___?______
0x0706b800,0x72617028,0x06296573,0x0110007a,0x01280153,0x00f60143,0x01340754,0x0116007a, // 06c0 .___(parse)_z.__S_(_C__.T_4_z.__
0x01ce0082,0x070e00f6,0x00820128,0x01160153,0x015901c2,0x00f601a8,0x012e070c,0x06ea00eb, // 06e0 _.___.__(__.S_____Y____.__.__.__
0x013c011c,0x000000c5,0x00d60143,0x0153011c,0x0128014b,0x0116007a,0x01160153,0x007a01c2, // 0700 __<__...C__.__S_K_(_z.__S_____z.
0x00820116,0x00f601ce,0x0159072c,0x073e00f6,0x00eb012e,0x01430714,0x01000128,0x011c0748, // 0720 ___.___.,_Y__.>_.__.__C_(_._H___
0x0143013c,0x0128012e,0x01c20153,0x011c011c,0x00d601c2,0x011c0153,0x00d601c2,0x500506c3, // 0740 <_C_._(_S__________.S______.___P
0x244b4341,0x28014306,0x10019c01,0x4b012e01,0x1c02ce01,0x5e00d601,0x41500507,0x06455352, // 0760 ACK$_C_(_____._K______.^__PARSE_
0x05320128,0x010a001d,0x002701a1,0x001d010a,0x01c2010a,0x06cb011c,0x022f001d,0x077900d6, // 0780 (_2__.____'.___._________./__.y_
0x4b4f5405,0x82064e45,0xc5077f00,0x52001f00,0x7a025a02,0xd6076402,0x0407a000,0x44524f57, // 07a0 _TOKEN__.___._.R_Z_z_d__.___WORD
0x5a077f06,0x64027a02,0xbb00d607,0x414e0507,0x063e454d,0x00c50244,0x0160001f,0x00d601a1, // 07c0 ___Z_z_d__.___NAME>_D__._.`____.
0x530507cd,0x3f454d41,0x9c012806,0x1d010001,0x16014308,0x2e012801,0x43014b01,0x28011601, // 07e0 ___SAME?_(___.___C___(_._K_C___(
0x4b012e01,0x1c011c01,0x8401c201,0x1d00f601,0x3c011c08,0x94012801,0xd6011c01,0xf100eb00, // 0800 _._K__________.____<_(______._._
0xc5019407,0xd6000000,0x0407e200,0x646e6966,0x43014b06,0x7a011601,0x43010500,0x28010a01, // 0820 ____..._.___find_K_C___z.__C___(
0x4b027a01,0x43010a01,0x7d00f601,0x0a014308,0x3f00c501,0x220160ff,0xf6016d01,0x7a086900, // 0840 _z_K___C__.}_C____.?_`_"_m__.i_z
0xff00c502,0x790100ff,0x7a027a08,0x34010a00,0xf6014301,0xe8087900,0x89010007,0x3c011c08, // 0860 __.__._y_z_z.__4_C__.y___._____<
0x89014b01,0xd6014b02,0x9500f600,0x89028908,0x45010002,0x3c011c08,0x3c014b01,0x43028901, // 0880 _K___K__._.______._E___<_K_<___C
0x4b07d301,0x2b00d601,0x414e0508,0x063f454d,0x08300050,0x08a900d6,0x50415403,0x5b014306, // 08a0 ___K__.+__NAME?_P.0__.___TAP_C_[
0x10015305,0xd6012e01,0x0408b800,0x5041546b,0xc5014306,0x6d000d00,0xc5015301,0x6d000a00, // 08c0 _S___.__.___kTAP_C__._.m_S__._.m
0xf6016001,0xc508fd00,0x6d000800,0xf900f601,0xbc008208,0xfb010008,0xd6057908,0x4b013c00, // 08e0 _`__.___._.m__.___.__.___y__.<_K
0x43013c01,0xcb00d601,0x43410608,0x54504543,0xa1015306,0x9c015301,0xf6016d01,0x4b093d00, // 0900 _<_C__.___ACCEPT_S___S___m__.=_K
0x82014305,0xc501c200,0xd4005f00,0x3700f601,0x0008bc09,0xd0093901,0x17010008,0x53013c09, // 0920 _C__.___._.___.7___._9___.___<_S
0xd601c201,0x06090900,0x45505845,0x10065443,0x05001409,0xd6013c01,0x05094700,0x52455551, // 0940 ____.___EXPECT____.__<__.G__QUER
0x05320659,0x004000c5,0x00270910,0x013c0105,0x000000c5,0x0105001d,0x095b00d6,0x58454008, // 0960 Y_2__.@.__'.__<__..._.___.[__@EX
0x54554345,0x010a0645,0x00f60184,0x00e10990,0x097c00d6,0x4f424105,0x71065452,0x94098500, // 0980 ECUTE______.___._.|__ABORT_q.___
0x62610609,0x2274726f,0xb500f606,0x44062709,0x9a060c02,0x3c062709,0xa100d601,0x52450509, // 09a0 __abort"__.__'_D_____'_<__.___ER
0x06524f52,0x024405a6,0x00c5060c,0x055b003f,0x001b00c5,0x0565055b,0x09bd099a,0x4e49240a, // 09c0 ROR___D____.?.[__._.[_e______$IN
0x50524554,0x06544552,0x018408af,0x0a0c00f6,0x00c50116,0x01600040,0x200d09a8,0x706d6f63, // 09e0 TERPRET______._____.@.`____ comp
0x20656c69,0x796c6e6f,0x00d600e1,0x00f60497,0x00d60a18,0x0a1a0100,0x09dc09c3,0xc5065b81, // 0a00 ile only_._.___.___..________[__
0x6509e700,0xd6010500,0x030a1c00,0x064b4f2e,0x00c50565,0x006509e7,0x01ce010a,0x0a6300f6, // 0a20 .__e.___.___.OK_e__.__e._____.c_
0x01280128,0x01430128,0x011c069b,0x069b0143,0x0143011c,0x011c069b,0x069b0143,0x2004064b, // 0a40 (_(_(_C_____C_____C_____C___K__ 
0xd63e6b6f,0x040a2b00,0x4c415645,0x4307a606,0xf6011601,0x650a7f00,0x00098500,0x3c0a6d01, // 0a60 ok>_.+__EVAL___C____.__e.__._m_<
0xd60a2f01,0x040a6700,0x54495551,0xc000c506,0x05003110,0x610a1e01,0x000a6c09,0x870a9701, // 0a80 _/__.g__QUIT__.__1.____a_l_.____
0x062c010a,0x0143025a,0x0043027a,0x01050105,0x0aa100d6,0x54494c87,0x4c415245,0xc500c506, // 0aa0 __,_Z_C_z_C._____.___LITERAL__._
0xa30aa300,0xb400d60a,0x4c41050a,0x06544f4c,0x022f0043,0x0ac900d6,0x222c2403,0x2200c506, // 0ac0 ._____.___ALLOT_C./__.___$,"__."
0x4407c000,0x4301a102,0xd6010500,0x070ad800,0x494e553f,0x06455551,0x08af0143,0x00f60184, // 0ae0 .__D___C.___.___?UNIQUE_C______.
0x02440b17,0x001f00c5,0x05a60160,0x064b060c,0x65722006,0x3c666544,0xef00d601,0x2c24030a, // 0b00 __D__._.`_____K__ reDef<__.___$,
0x0143066e,0x00f6010a,0x0af70b46,0x07d30143,0x01050043,0x005a0143,0x02890105,0x010a0050, // 0b20 n_C____.F___C___C.__C_Z.____P.__
0x0105014b,0x09c300d6,0x27010b1d,0xaf07a606,0x5700f608,0xc300d60b,0x890b4a09,0x4d4f435b, // 0b40 K____._____'______.W__.__J__[COM
0x454c4950,0x0b4c065d,0x00d60aa3,0x43070b5b,0x49504d4f,0x1c06454c,0x0a014301,0x7a0aa301, // 0b60 PILE]_L____.[__COMPILE___C_____z
0xd6012802,0x080b6e00,0x4d4f4324,0x454c4950,0x8408af06,0xaf00f601,0xc5010a0b,0x60008000, // 0b80 _(__.n__$COMPILE______._____._.`
0xab00f601,0x0000e10b,0xa30bad01,0x9700d60a,0xb900f604,0xd60abc0b,0x8709c300,0x564f050b, // 0ba0 __.___..______.___._____._____OV
0x06545245,0x010a005a,0x01050050,0x0bbd00d6,0xc5065d01,0x650b9000,0xd6010500,0x010bd000, // 0bc0 ERT_Z.__P.___.___]__.__e.___.___
0x07a6063a,0x0bd20b21,0x000600c5,0x0143025a,0x0043012e,0x01100105,0x0bdf00d6,0xc5063b81, // 0be0 :___!____._.Z_C_._C._____.___;__
0xa300d600,0xc30a1e0a,0xfc00d60b,0x6d64030b,0x0153062b,0x000600c5,0x01280670,0x0c2c0100, // 0c00 ._._______.___dm+_S__._.p_(_._,_
0x010a0143,0x000500c5,0x027a0670,0x0c2000eb,0x0c0d00d6,0x4d554404,0x003b0650,0x0128010a, // 0c20 C____._.p_z__. __.___DUMP_;.__(_
0x00c5033b,0x01a1001f,0x001000c5,0x01280200,0x0c680100,0x00c50565,0x019c0008,0x01280c11, // 0c40 ;__._.___._.._(_._h_e__._.____(_
0x029805a6,0x011c060c,0x0c5400eb,0x011c013c,0x0105003b,0x0c3400d6,0x414e3e05,0x3406454d, // 0c60 _________.T_<___;.___.4__>NAME_4
0x16014301,0x7f00c501,0xc5016000,0xd9002000,0x7f00f601,0x7800d60c,0x492e030c,0x02440644, // 0c80 _C____._.`__. .___.___.x__.ID_D_
0x001f00c5,0x060c0160,0x00d605a6,0x57050c99,0x5344524f,0x50056506,0x0000c500,0x05007a00, // 0ca0 _._.`______.___WORDS_e_P._...z._
0x84010a01,0xf900f601,0xa601430c,0x890c9d05,0x0a007a02,0x0a00c501,0xf601d900,0xc50ceb00, // 0cc0 ______.__C_______z.___._.___.___
0x7a000100,0x00022f00,0x650cf501,0x0000c505,0x05007a00,0xc1010001,0xae00d60c,0x4f46060c, // 0ce0 ._.z./_.___e__...z.__.____.___FO
0x54454752,0xaf07a606,0xf6018408,0x890d2700,0x43014302,0x0a010500,0x50014301,0x5a010500, // 0d00 RGET________.'___C_C.____C_P.__Z
0x3c010500,0xc300d601,0x840cfd09,0x4e454854,0x4b025a06,0xd6010501,0x830d2b00,0x06524f46, // 0d20 .__<__._____THEN_Z_K____.+__FOR_
0x01280b76,0x00d6025a,0x42850d3b,0x4e494745,0xd6025a06,0x840d4a00,0x5458454e,0xeb0b7606, // 0d40 v_(_Z__.;__BEGIN_Z__.J__NEXT_v__
0xd60aa300,0x850d5700,0x49544e55,0x0b76064c,0x0aa300f6,0x0d6700d6,0x41474185,0x76064e49, // 0d60 .___.W__UNTIL_v__.___.g__AGAIN_v
0xa301000b,0x7800d60a,0x4649820d,0xf60b7606,0xc5025a00,0xa3000000,0x8900d60a,0x4841850d, // 0d80 _.____.x__IF_v__.Z__...___.___AH
0x06444145,0x01000b76,0x00c5025a,0x0aa30000,0x0d9d00d6,0x50455286,0x06544145,0x0d300d7e, // 0da0 EAD_v_._Z__...___.___REPEAT_~_0_
0x0db400d6,0x54464183,0xa3013c06,0x4b025a0d,0xc400d601,0x4c45840d,0xa3064553,0x30014b0d, // 0dc0 _.___AFT_<___Z_K__.___ELSE___K_0
0xd500d60d,0x4857840d,0x8c064e45,0xd601530d,0x850de500,0x4c494857,0x0d8c0645,0x00d6014b, // 0de0 __.___WHEN___S__.___WHILE___K__.
0x41860df3,0x54524f42,0x00c50622,0x025a09a8,0x0adc0105,0x0e0200d6,0x06222482,0x064000c5, // 0e00 ___ABORT"__.__Z______.___$"__.@_
0x0105025a,0x00d60adc,0x2e820e18,0x00c50622,0x025a064b,0x0adc0105,0x0e2a00d6,0x444f4304, // 0e20 Z______.___."__.K_Z______.*__COD
0x07a60645,0x0bc30b21,0x0e3c00d6,0x45524306,0x06455441,0x00c50e41,0x0aa3203d,0x0e4c00d6, // 0e40 E___!____.<__CREATE_A__.= ___.L_
0x52415608,0x4c424149,0x0e530645,0x000000c5,0x00d60aa3,0x43080e60,0x54534e4f,0x06544e41, // 0e60 _VARIABLE_S__...___.`__CONSTANT_
0x00c50e41,0x0aa32004,0x00d60aa3,0x2e820e76,0x00c50628,0x077f0029,0x00d6060c,0x5c810e8e, // 0e80 A__._ _____.v__.(__.)._____.____
0x0a00c506,0x3c07c000,0x9e00d601,0x0628810e,0x002900c5,0x0194077f,0x0ead00d6,0x4d4f430c, // 0ea0 __._.__<__.___(__.)._____.___COM
0x454c4950,0x4c4e4f2d,0x00c50659,0x005a0040,0x022f010a,0x0ebc00d6,0x4d4d4909,0x41494445, // 0ec0 PILE-ONLY__.@.Z.__/__.___IMMEDIA
0xc5064554,0x5a008000,0x2f010a00,0xd800d602,0x4f43040e,0xc506444c,0x500ef100,0xc5010500, // 0ee0 TE__._.Z.__/__.___COLD__.__P.___
0x5a0ef100,0xc5010500,0x6509e700,0xc5010500,0x710a8c00,0x65010500,0x000a8c05,0x00000000, // 0f00 .__Z.___.__e.___.__q.__e___.....
};