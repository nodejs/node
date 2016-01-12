//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#define o(form) OPBYTE_ ## form
#ifndef f
# define f(form) FORM_ ## form
#endif

//            opcode
//           /          layout
//          /          /         attrib             byte2
//         /          /         /                   /         form
//        /          /         /                   /         /          opbyte
//       /          /         /                   /         /          /
//      /          /         /                   /         /          /              dope                   leadin
//     /          /         /                   /         /          /              /                       /
MACRO(ADD,      Reg2,       None,           R000,   f(BINOP),   o(ADD),     DOPEQ|DSETCC|DCOMMOP,       OLB_NONE)

MACRO(ADDPD,    Reg2,       None,           RNON,   f(MODRM),   o(ADDPD),   DNO16|DOPEQ|D66|DCOMMOP,    OLB_NONE)
MACRO(ADDPS,    Reg2,       None,           RNON,   f(MODRM),   o(ADDPS),   DNO16|DOPEQ|DZEROF|DCOMMOP, OLB_NONE)

MACRO(ADDSD,    Reg2,       None,           RNON,   f(MODRM),   o(ADDSD),   DNO16|DOPEQ|DCOMMOP|DF2,    OLB_NONE)
MACRO(ADDSS,    Reg2,       None,           RNON,   f(MODRM),   o(ADDSS),   DNO16|DOPEQ|DF3|DCOMMOP,    OLB_NONE)
MACRO(AND,      Reg2,       None,           R100,   f(BINOP),   o(AND),     DOPEQ|DSETCC|DCOMMOP,       OLB_NONE)
MACRO(ANDNPD,   Reg2,       None,           RNON,   f(MODRM),   o(ANDNPD),  DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(ANDNPS,   Reg2,       None,           RNON,   f(MODRM),   o(ANDNPS),  DNO16|DOPEQ|DZEROF,         OLB_NONE)
MACRO(ANDPD,    Reg2,       None,           RNON,   f(MODRM),   o(ANDPD),   DNO16|DOPEQ|D66|DCOMMOP,    OLB_NONE)
MACRO(ANDPS,    Reg2,       None,           RNON,   f(MODRM),   o(ANDPS),   DNO16|DOPEQ|DZEROF|DCOMMOP, OLB_NONE)
MACRO(BSR,      Reg2,       None,           RNON,   f(MODRM),   o(BSR),     DZEROF|DDST,                OLB_NONE)
MACRO(BT,       Reg2,       OpSideEffect,   R100,   f(SPMOD),   o(BT),      DZEROF|DSETCC,              OLB_NONE)
MACRO(BTR,      Reg2,       OpSideEffect,   R110,   f(SPMOD),   o(BTR),     DZEROF|DSETCC,              OLB_NONE)
MACRO(CALL,     CallI,      OpSideEffect,   R010,   f(CALL),    o(CALL),    DSETCC,                     OLB_NONE)
MACRO(CDQ,      Empty,      OpSideEffect,   RNON,   f(NO),      o(CDQ),     DNO16,                      OLB_NONE)
MACRO(CMOVA,    Reg2,       None,           RNON,   f(MODRM),   o(CMOVA),   DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVAE,   Reg2,       None,           RNON,   f(MODRM),   o(CMOVAE),  DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVB,    Reg2,       None,           RNON,   f(MODRM),   o(CMOVB),   DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVBE,   Reg2,       None,           RNON,   f(MODRM),   o(CMOVBE),  DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVE,    Reg2,       None,           RNON,   f(MODRM),   o(CMOVE),   DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVG,    Reg2,       None,           RNON,   f(MODRM),   o(CMOVG),   DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVGE,   Reg2,       None,           RNON,   f(MODRM),   o(CMOVGE),  DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVL,    Reg2,       None,           RNON,   f(MODRM),   o(CMOVL),   DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVLE,   Reg2,       None,           RNON,   f(MODRM),   o(CMOVLE),  DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVNE,   Reg2,       None,           RNON,   f(MODRM),   o(CMOVNE),   DZEROF|DDST|DUSECC,        OLB_NONE)
MACRO(CMOVNO,   Reg2,       None,           RNON,   f(MODRM),   o(CMOVNO),  DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVNP,   Reg2,       None,           RNON,   f(MODRM),   o(CMOVNP),  DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVNS,   Reg2,       None,           RNON,   f(MODRM),   o(CMOVNS),  DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVO,    Reg2,       None,           RNON,   f(MODRM),   o(CMOVO),   DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVP,    Reg2,       None,           RNON,   f(MODRM),   o(CMOVP),   DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMOVS,    Reg2,       None,           RNON,   f(MODRM),   o(CMOVS),   DZEROF|DDST|DUSECC,         OLB_NONE)
MACRO(CMP,      Empty,      OpSideEffect,   R111,   f(BINOP),   o(CMP),     DSETCC,                     OLB_NONE)

MACRO(CMPLTPS,  Empty,      None,           RNON,   f(MODRM),   o(CMPPS),   DZEROF|DSSE,                OLB_NONE)
MACRO(CMPLEPS,  Empty,      None,           RNON,   f(MODRM),   o(CMPPS),   DZEROF|DSSE,                OLB_NONE)
MACRO(CMPEQPS,  Empty,      None,           RNON,   f(MODRM),   o(CMPPS),   DZEROF|DSSE,                OLB_NONE)
MACRO(CMPNEQPS, Empty,      None,           RNON,   f(MODRM),   o(CMPPS),   DZEROF|DSSE,                OLB_NONE)

MACRO(CMPLTPD,  Empty,      None,           RNON,   f(MODRM),   o(CMPPD),   D66|DSSE,                   OLB_NONE)
MACRO(CMPLEPD,  Empty,      None,           RNON,   f(MODRM),   o(CMPPD),   D66|DSSE,                   OLB_NONE)
MACRO(CMPEQPD,  Empty,      None,           RNON,   f(MODRM),   o(CMPPD),   D66|DSSE,                   OLB_NONE)
MACRO(CMPNEQPD, Empty,      None,           RNON,   f(MODRM),   o(CMPPD),   D66|DSSE,                   OLB_NONE)


MACRO(COMISD,   Empty,      OpSideEffect,   RNON,   f(MODRM),   o(COMISD),  DNO16|D66|DSETCC,           OLB_NONE)
MACRO(COMISS,   Empty,      OpSideEffect,   RNON,   f(MODRM),   o(COMISS),  DNO16|DZEROF|DSETCC,        OLB_NONE)
MACRO(CVTDQ2PD, Reg2,       None,           RNON,   f(MODRM),   o(CVTDQ2PD),DDST|DNO16|DF3,             OLB_NONE)
MACRO(CVTDQ2PS, Reg2,       None,           RNON,   f(MODRM),   o(CVTDQ2PS),DDST|DNO16|DZEROF,          OLB_NONE)
MACRO(CVTSD2SI, Reg2,       None,           RNON,   f(MODRM),   o(CVTSD2SI),DDST|DNO16|DF2,             OLB_NONE)
MACRO(CVTSI2SD, Reg2,       None,           RNON,   f(MODRM),   o(CVTSI2SD),DDST|DNO16|DF2,             OLB_NONE)
MACRO(CVTSI2SS, Reg2,       None,           RNON,   f(MODRM),   o(CVTSI2SS),DDST|DNO16|DF3,             OLB_NONE)
MACRO(CVTPD2PS, Reg2,       None,           RNON,   f(MODRM),   o(CVTPD2PS),DDST|DNO16|D66,             OLB_NONE)
MACRO(CVTPS2PD, Reg2,       None,           RNON,   f(MODRM),   o(CVTPS2PD),DDST|DNO16|DZEROF,          OLB_NONE)
MACRO(CVTSD2SS, Reg2,       None,           RNON,   f(MODRM),   o(CVTSD2SS),DDST|DNO16|DF2,             OLB_NONE)
MACRO(CVTSS2SD, Reg2,       None,           RNON,   f(MODRM),   o(CVTSD2SS),DDST|DNO16|DF3,             OLB_NONE)
MACRO(CVTSS2SI, Reg2,       None,           RNON,   f(MODRM),   o(CVTSS2SI),DDST|DNO16|DF3,             OLB_NONE)
MACRO(CVTTSD2SI,Reg2,       None,           RNON,   f(MODRM),   o(CVTTSD2SI),DDST|DNO16|DF2,            OLB_NONE)
MACRO(CVTTSS2SI,Reg2,       None,           RNON,   f(MODRM),   o(CVTTSS2SI),DDST|DNO16|DF3,            OLB_NONE)
MACRO(CVTTPD2DQ,Reg2,       None,           RNON,   f(MODRM),   o(CVTTPD2DQ),DDST|DNO16|D66,            OLB_NONE)
MACRO(CVTTPS2DQ,Reg2,       None,           RNON,   f(MODRM),   o(CVTTPS2DQ),DDST|DNO16|DF3,            OLB_NONE)
MACRO(DEC,      Reg2,       None,           R001,   f(INCDEC),  o(DEC),     DOPEQ|DSETCC,               OLB_NONE)
MACRO(DIV,      Reg3,       None,           R110,   f(MULDIV),  o(DIV),     DSETCC,                     OLB_NONE)

MACRO(DIVPD,    Reg3,       None,           RNON,   f(MODRM),   o(DIVPD),   DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(DIVPS,    Reg3,       None,           RNON,   f(MODRM),   o(DIVPS),   DNO16|DOPEQ|DZEROF,         OLB_NONE)

MACRO(DIVSD,    Reg3,       None,           RNON,   f(MODRM),   o(DIVSD),   DNO16|DOPEQ|DF2,            OLB_NONE)
MACRO(DIVSS,    Reg3,       None,           RNON,   f(MODRM),   o(DIVSS),   DNO16|DOPEQ|DF3,            OLB_NONE)
MACRO(FISTTP,   Reg2,       None,           R001,   f(FILD),    o(FISTTP),  DFLT|NDPdec|DDST,           OLB_NONE)
MACRO(FLD,      Reg2,       None,           R000,   f(FLD),     o(FLD),     DFLT|NDPinc|DDST,           OLB_NONE)
MACRO(FSTP,     Reg1,       None,           R011,   f(FLD),     o(FSTP),    DDST|DFLT,                  OLB_NONE)
MACRO(IDIV,     Reg3,       None,           R111,   f(MULDIV),  o(IDIV),    DSETCC,                     OLB_NONE)
MACRO(INC,      Reg2,       None,           R000,   f(INCDEC),  o(INC),     DOPEQ|DSETCC,               OLB_NONE)
MACRO(IMUL,     Reg3,       None,           R101,   f(MULDIV),  o(IMUL),    DSETCC,                     OLB_NONE)
MACRO(IMUL2,    Reg3,       None,           RNON,   f(SPMOD),   o(IMUL2),   DOPEQ|DSETCC|DZEROF|DCOMMOP,OLB_NONE)
MACRO(INT,      Reg1,       OpSideEffect,   RNON,   f(SPECIAL), o(INT),     DUSECC|DSETCC,              OLB_NONE)
MACRO(JA,       BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JAE,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JB,       BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JBE,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JEQ,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JNE,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JNP,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JLT,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JLE,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JGT,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JGE,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JNO,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JO,       BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JP,       BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JNSB,     BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JSB,      BrReg2,     OpSideEffect,   RNON,   f(SPECIAL), o(JCC),     DUSECC|DNO16,               OLB_NONE)
MACRO(JMP,      Br,         OpSideEffect,   R100,   f(JMP),     o(JMP),     DNO16,                      OLB_NONE)
MACRO(LAHF,     Reg1,       None,           RNON,   f(NO),      o(LAHF),    DUSECC,                     OLB_NONE)
MACRO(LEA,      Reg2,       None,           RNON,   f(MODRM),   o(LEA),     DDST,                       OLB_NONE)

MACRO(MAXPD,    Reg2,       None,           RNON,   f(MODRM),   o(MAXPD),   DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(MAXPS,    Reg2,       None,           RNON,   f(MODRM),   o(MAXPS),   DNO16|DOPEQ|DZEROF,         OLB_NONE)
MACRO(MINPD,    Reg2,       None,           RNON,   f(MODRM),   o(MINPD),   DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(MINPS,    Reg2,       None,           RNON,   f(MODRM),   o(MINPS),   DNO16|DOPEQ|DZEROF,         OLB_NONE)

MACRO(LZCNT,    Reg2,       None,           RNON,   f(MODRM),   o(LZCNT),   DF3|DSETCC|DDST,            OLB_NONE)

MACRO(MOV,      Reg2,       None,           R000,   f(MOV),     o(MOV),     DDST,                       OLB_NONE)
MACRO(MOVAPS,   Reg2,       None,           RNON,   f(SPECIAL), o(MOVAPS),  DDST|DNO16|DZEROF,          OLB_NONE)
MACRO(MOVD,     Reg2,       None,           RNON,   f(SPECIAL), o(MOVD),    DDST|DNO16|D66,             OLB_NONE)

MACRO(MOVHLPS,  Reg2,       None,           RNON,   f(SPECIAL), o(MOVHLPS), DDST|DNO16|DZEROF,          OLB_NONE)
MACRO(MOVHPD,   Reg2,       None,           RNON,   f(SPECIAL), o(MOVHPD),  DDST|DNO16|D66,             OLB_NONE)
MACRO(MOVLHPS,  Reg2,       None,           RNON,   f(SPECIAL), o(MOVLHPS), DDST|DNO16|DZEROF,          OLB_NONE)
MACRO(MOVMSKPD, Reg2,       None,           RNON,   f(SPECIAL), o(MOVMSKPD), DDST|DNO16|D66,            OLB_NONE)
MACRO(MOVMSKPS, Reg2,       None,           RNON,   f(SPECIAL), o(MOVMSKPS), DDST|DNO16|DZEROF,         OLB_NONE)

MACRO(MOVSD,    Reg2,       None,           RNON,   f(MODRM),   o(MOVSD),   DDST|DNO16|DF2,             OLB_NONE)
MACRO(MOVSD_ZERO,Reg2,      None,           RNON,   f(MODRM),   o(MOVSD),   0,                          OLB_NONE)
MACRO(MOVSS,    Reg2,       None,           RNON,   f(SPECIAL), o(MOVSS),   DDST|DNO16|DF3,             OLB_NONE)
MACRO(MOVSX,    Reg2,       None,           RNON,   f(MODRM),   o(MOVSX),   DZEROF|DDST,                OLB_NONE)
MACRO(MOVSXW,   Reg2,       None,           RNON,   f(MODRM),   o(MOVSXW),  DZEROF|DDST,                OLB_NONE)
MACRO(MOVUPS,   Reg2,       None,           RNON,   f(SPECIAL), o(MOVUPS),  DDST|DNO16|DZEROF,          OLB_NONE)

MACRO(MOVZX,    Reg2,       None,           RNON,   f(MODRM),   o(MOVZX),   DZEROF|DDST,                OLB_NONE)
MACRO(MOVZXW,   Reg2,       None,           RNON,   f(MODRM),   o(MOVZXW),  DZEROF|DDST,                OLB_NONE)

MACRO(MULPD,    Reg3,       None,           RNON,   f(MODRM),   o(MULPD),   DNO16|DOPEQ|D66|DCOMMOP,    OLB_NONE)
MACRO(MULPS,    Reg3,       None,           RNON,   f(MODRM),   o(MULPS),   DNO16|DOPEQ|DZEROF|DCOMMOP, OLB_NONE)

MACRO(MULSD,    Reg3,       None,           RNON,   f(MODRM),   o(MULSD),   DNO16|DOPEQ|DF2,            OLB_NONE)
MACRO(MULSS,    Reg3,       None,           RNON,   f(MODRM),   o(MULSS),   DNO16|DOPEQ|DF3|DCOMMOP,    OLB_NONE)
MACRO(NEG,      Reg2,       None,           R011,   f(MODRMW),  o(NEG),     DOPEQ|DSETCC,               OLB_NONE)
MACRO(NOP,      Empty,      None,           RNON,   f(SPECIAL), o(NOP),     DNO16,                      OLB_NONE)
MACRO(NOT,      Reg2,       None,           R010,   f(MODRMW),  o(NOT),     DOPEQ,                      OLB_NONE)
MACRO(OR ,      Reg2,       None,           R001,   f(BINOP),   o(OR),      DOPEQ|DSETCC|DCOMMOP,       OLB_NONE)

MACRO(ORPS,     Reg2,       None,           R001,   f(MODRM),   o(ORPS),    DOPEQ|DOPEQ|DZEROF|DCOMMOP, OLB_NONE)
MACRO(PADDD,    Reg2,       None,           RNON,   f(MODRM),   o(PADDD),   DNO16|DOPEQ|D66|DCOMMOP,    OLB_NONE)
MACRO(PAND,     Reg2,       None,           RNON,   f(MODRM),   o(PAND),    DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(PCMPEQD,  Reg2,       None,           RNON,   f(MODRM),   o(PCMPEQD), DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(PCMPGTD,  Reg2,       None,           RNON,   f(MODRM),   o(PCMPGTD), DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(PMULUDQ,  Reg2,       None,           RNON,   f(MODRM),   o(PMULUDQ), DNO16|DOPEQ|D66|DCOMMOP,    OLB_NONE)

MACRO(POP,      Reg1,       OpSideEffect,   R000,   f(PSHPOP),  o(POP),     DDST,                       OLB_NONE)

MACRO(POR,      Reg2,       None,           RNON,   f(MODRM),   o(POR),     DNO16|DOPEQ|D66|DCOMMOP,    OLB_NONE)
MACRO(PSHUFD,   Reg3,       None,           RNON,   f(MODRM),   o(PSHUFD),  DDST|DNO16|D66|DSSE,        OLB_NONE)
MACRO(PSLLDQ,   Reg2,       None,           R111,   f(SPECIAL), o(PSLLDQ),  DDST|DNO16|DOPEQ|D66|DSSE,  OLB_NONE)
MACRO(PSRLDQ,   Reg2,       None,           R011,   f(SPECIAL), o(PSRLDQ),  DDST|DNO16|DOPEQ|D66|DSSE,  OLB_NONE)
MACRO(PSUBD,    Reg3,       None,           RNON,   f(MODRM),   o(PSUBD),   DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(PUNPCKLDQ, Reg3,      None,           RNON,   f(MODRM),   o(PUNPCKLDQ),DNO16|DOPEQ|D66,           OLB_NONE)

MACRO(PUSH,     Reg1,       OpSideEffect,   R110,   f(PSHPOP),  o(PUSH),    0,                          OLB_NONE)

MACRO(PXOR,     Reg2,       None,           RNON,   f(MODRM),   o(PXOR),    DNO16|DOPEQ|D66|DCOMMOP,    OLB_NONE)

MACRO(RET,      Empty,      OpSideEffect,   RNON,   f(SPECIAL), o(RET),     DSETCC,                     OLB_NONE)
MACRO(ROUNDSD,  Reg3,       None,           RNON,   f(MODRM),   o(ROUNDSD), DDST|DNO16|DSSE|D66,        OLB_0F3A)
MACRO(ROUNDSS,  Reg3,       None,           RNON,   f(MODRM),   o(ROUNDSS), DDST|DNO16|DSSE|D66,        OLB_0F3A)
MACRO(SAR,      Reg2,       None,           R111,   f(SHIFT),   o(SAR),     DOPEQ|DSETCC,               OLB_NONE)
MACRO(SBB,      Reg2,       None,           R011,   f(BINOP),   o(SBB),     DOPEQ|DUSECC|DSETCC,        OLB_NONE)
MACRO(SETE,     Reg1,       None,           RNON,   f(MODRM),   o(SETE),    DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETG,     Reg1,       None,           RNON,   f(MODRM),   o(SETG),    DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETGE,    Reg1,       None,           RNON,   f(MODRM),   o(SETGE),   DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETL,     Reg1,       None,           RNON,   f(MODRM),   o(SETL),    DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETLE,    Reg1,       None,           RNON,   f(MODRM),   o(SETLE),   DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETNE,    Reg1,       None,           RNON,   f(MODRM),   o(SETNE),   DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETO,     Reg1,       None,           RNON,   f(MODRM),   o(SETO),    DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETA,     Reg1,       None,           RNON,   f(MODRM),   o(SETA),    DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETAE,    Reg1,       None,           RNON,   f(MODRM),   o(SETAE),   DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETB,     Reg1,       None,           RNON,   f(MODRM),   o(SETB),    DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SETBE,    Reg1,       None,           RNON,   f(MODRM),   o(SETBE),   DOPEQ|DUSECC|DZEROF|DDST,   OLB_NONE)
MACRO(SHL,      Reg2,       None,           R100,   f(SHIFT),   o(SHL),     DOPEQ|DSETCC,               OLB_NONE)
MACRO(SHR,      Reg2,       None,           R101,   f(SHIFT),   o(SHR),     DOPEQ|DSETCC,               OLB_NONE)

MACRO(SHUFPD,   Reg2,       None,           RNON,   f(MODRM),   o(SHUFPD),  DDST|DNO16|D66|DSSE,        OLB_NONE)
MACRO(SHUFPS,   Reg2,       None,           RNON,   f(MODRM),   o(SHUFPS),  DDST|DNO16|DZEROF|DSSE,     OLB_NONE)
MACRO(SQRTPD,   Reg2,       None,           RNON,   f(MODRM),   o(SQRTPD),  DDST|DNO16|D66,             OLB_NONE)
MACRO(SQRTPS,   Reg2,       None,           RNON,   f(MODRM),   o(SQRTPS),  DDST|DNO16|DZEROF,          OLB_NONE)

MACRO(SQRTSD,   Reg2,       None,           RNON,   f(MODRM),   o(SQRTSD),  DDST|DNO16|DF2,             OLB_NONE)
MACRO(SQRTSS,   Reg2,       None,           RNON,   f(MODRM),   o(SQRTSS),  DDST|DNO16|DF3,             OLB_NONE)
MACRO(SUB,      Reg2,       None,           R101,   f(BINOP),   o(SUB),     DOPEQ|DSETCC,               OLB_NONE)

MACRO(SUBPD,    Reg3,       None,           RNON,   f(MODRM),   o(SUBPD),   DNO16|DOPEQ|D66,            OLB_NONE)
MACRO(SUBPS,    Reg3,       None,           RNON,   f(MODRM),   o(SUBPS),   DNO16|DOPEQ|DZEROF,         OLB_NONE)

MACRO(SUBSD,    Reg3,       None,           RNON,   f(MODRM),   o(SUBSD),   DNO16|DOPEQ|DF2,            OLB_NONE)
MACRO(SUBSS,    Reg3,       None,           RNON,   f(MODRM),   o(SUBSS),   DNO16|DOPEQ|DF3,            OLB_NONE)
MACRO(TEST,     Empty,      None,           R000,   f(TEST),    o(TEST),    DSETCC|DCOMMOP,             OLB_NONE)
MACRO(TEST_AH,  Empty,      None,           R000,   f(TEST_AH), o(TEST_AH), DSETCC|DCOMMOP,             OLB_NONE)
MACRO(UCOMISD,  Empty,      None,           RNON,   f(MODRM),   o(UCOMISD), DNO16|D66|DSETCC,           OLB_NONE)
MACRO(UCOMISS,  Empty,      None,           RNON,   f(MODRM),   o(UCOMISS), DNO16|DZEROF|DSETCC,        OLB_NONE)
MACRO(XCHG,     Reg2,       None,           RNON,   f(XCHG),    o(XCHG),    DOPEQ,                      OLB_NONE)
MACRO(XOR,      Reg2,       None,           R110,   f(BINOP),   o(XOR),     DOPEQ|DSETCC|DCOMMOP,       OLB_NONE)
MACRO(XORPS,    Reg3,       None,           RNON,   f(MODRM),   o(XORPS),   DNO16|DOPEQ|DZEROF|DCOMMOP, OLB_NONE)
#undef o
#undef f
