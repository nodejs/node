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
//          /          /                      byte2
//         /          /                      /         form
//        /          /                      /         /          opbyte
//       /          /                      /         /          /
//      /          /                      /         /          /              dope                      leadin
//     /          /                      /         /          /              /                         /
MACRO(ADD,      Reg2,   OpSideEffect,  R000,   f(BINOP),   o(ADD),     DOPEQ|DSETCC|DCOMMOP,        OLB_NONE)

MACRO(ADDPD,    Reg2,   None,          RNON,   f(MODRM),   o(ADDPD),   DNO16|DOPEQ|D66|DCOMMOP,     OLB_0F)
MACRO(ADDPS,    Reg2,   None,          RNON,   f(MODRM),   o(ADDPS),   DNO16|DOPEQ|DCOMMOP,         OLB_0F)

MACRO(ADDSD,    Reg2,   None,          RNON,   f(MODRM),   o(ADDSD),   DNO16|DOPEQ|DCOMMOP|DF2,     OLB_0F)
MACRO(ADDSS,    Reg2,   None,          RNON,   f(MODRM),   o(ADDSS),   DNO16|DOPEQ|DF3|DCOMMOP,     OLB_0F)
MACRO(AND,      Reg2,   OpSideEffect,  R100,   f(BINOP),   o(AND),     DOPEQ|DSETCC|DCOMMOP,        OLB_NONE)

MACRO(ANDNPD,   Reg2,   None,          RNON,   f(MODRM),   o(ANDNPD),  DNO16|DOPEQ|D66,             OLB_NONE)
MACRO(ANDNPS,   Reg2,   None,          RNON,   f(MODRM),   o(ANDNPS),  DNO16|DOPEQ,                 OLB_0F)

MACRO(ANDPD,    Reg2,   None,          RNON,   f(MODRM),   o(ANDPD),   DNO16|DOPEQ|D66|DCOMMOP,     OLB_0F)
MACRO(ANDPS,    Reg2,   None,          RNON,   f(MODRM),   o(ANDPS),   DNO16|DOPEQ|DCOMMOP,         OLB_0F)
MACRO(BSR,      Reg2,   None,          RNON,   f(MODRM),   o(BSR),     DDST,                        OLB_0F)
MACRO(BT,       Reg2,   OpSideEffect,  R100,   f(SPMOD),   o(BT),      DSETCC,                      OLB_0F)
MACRO(BTR,      Reg2,   OpSideEffect,  R110,   f(SPMOD),   o(BTR),     DOPEQ|DSETCC,                OLB_0F)
MACRO(BTS,      Reg2,   OpSideEffect,  R101,   f(SPMOD),   o(BTS),     DOPEQ|DSETCC,                OLB_0F)
MACRO(CALL,     CallI,  OpSideEffect,  R010,   f(CALL),    o(CALL),    DSETCC,                      OLB_NONE)
MACRO(CDQ,      Empty,  OpSideEffect,  RNON,   f(NO),      o(CDQ),     DNO16,                       OLB_NONE)
MACRO(CQO,      Empty,  None,          RNON,   f(NO),      o(CDQ),     DNO16,                       OLB_NONE)
MACRO(CMOVA,    Reg2,   None,          RNON,   f(MODRM),   o(CMOVA),   DDST|DUSECC,                 OLB_0F)
MACRO(CMOVAE,   Reg2,   None,          RNON,   f(MODRM),   o(CMOVAE),  DDST|DUSECC,                 OLB_0F)
MACRO(CMOVB,    Reg2,   None,          RNON,   f(MODRM),   o(CMOVB),   DDST|DUSECC,                 OLB_0F)
MACRO(CMOVBE,   Reg2,   None,          RNON,   f(MODRM),   o(CMOVBE),  DDST|DUSECC,                 OLB_0F)
MACRO(CMOVE,    Reg2,   None,          RNON,   f(MODRM),   o(CMOVE),   DDST|DUSECC,                 OLB_0F)
MACRO(CMOVG,    Reg2,   None,          RNON,   f(MODRM),   o(CMOVG),   DDST|DUSECC,                 OLB_0F)
MACRO(CMOVGE,   Reg2,   None,          RNON,   f(MODRM),   o(CMOVGE),  DDST|DUSECC,                 OLB_0F)
MACRO(CMOVL,    Reg2,   None,          RNON,   f(MODRM),   o(CMOVL),   DDST|DUSECC,                 OLB_0F)
MACRO(CMOVLE,   Reg2,   None,          RNON,   f(MODRM),   o(CMOVLE),  DDST|DUSECC,                 OLB_0F)
MACRO(CMOVNE,   Reg2,   None,          RNON,   f(MODRM),   o(CMOVNE),  DDST|DUSECC,                 OLB_0F)
MACRO(CMOVNO,   Reg2,   None,          RNON,   f(MODRM),   o(CMOVNO),  DDST|DUSECC,                 OLB_0F)
MACRO(CMOVNP,   Reg2,   None,          RNON,   f(MODRM),   o(CMOVNP),  DDST|DUSECC,                 OLB_0F)
MACRO(CMOVNS,   Reg2,   None,          RNON,   f(MODRM),   o(CMOVNS),  DDST|DUSECC,                 OLB_0F)
MACRO(CMOVO,    Reg2,   None,          RNON,   f(MODRM),   o(CMOVO),   DDST|DUSECC,                 OLB_0F)
MACRO(CMOVP,    Reg2,   None,          RNON,   f(MODRM),   o(CMOVP),   DDST|DUSECC,                 OLB_0F)
MACRO(CMOVS,    Reg2,   None,          RNON,   f(MODRM),   o(CMOVS),   DDST|DUSECC,                 OLB_0F)
MACRO(CMP,      Empty,  OpSideEffect,  R111,   f(BINOP),   o(CMP),     DSETCC,                      OLB_NONE)

MACRO(CMPLTPS,    Empty,    None,          RNON,   f(MODRM),   o(CMPPS),   DSSE,                    OLB_0F)
MACRO(CMPLEPS,    Empty,    None,          RNON,   f(MODRM),   o(CMPPS),   DSSE,                    OLB_0F)
MACRO(CMPEQPS,    Empty,    None,          RNON,   f(MODRM),   o(CMPPS),   DSSE,                    OLB_0F)
MACRO(CMPNEQPS,   Empty,    None,          RNON,   f(MODRM),   o(CMPPS),   DSSE,                    OLB_0F)

MACRO(CMPLTPD,    Empty,    None,          RNON,   f(MODRM),   o(CMPPD),   D66|DSSE,                OLB_0F)
MACRO(CMPLEPD,    Empty,    None,          RNON,   f(MODRM),   o(CMPPD),   D66|DSSE,                OLB_0F)
MACRO(CMPEQPD,    Empty,    None,          RNON,   f(MODRM),   o(CMPPD),   D66|DSSE,                OLB_0F)
MACRO(CMPNEQPD,   Empty,    None,          RNON,   f(MODRM),   o(CMPPD),   D66|DSSE,                OLB_0F)

MACRO(COMISD,   Empty,  OpSideEffect,  RNON,   f(MODRM),   o(COMISD),  DNO16|D66|DSETCC,            OLB_0F)
MACRO(COMISS,   Empty,  OpSideEffect,  RNON,   f(MODRM),   o(COMISS),  DNO16|DSETCC,                OLB_0F)
MACRO(CVTDQ2PD, Reg2,   None,          RNON,   f(MODRM),   o(CVTDQ2PD),DDST|DNO16|DF3,              OLB_0F)
MACRO(CVTDQ2PS, Reg2,   None,          RNON,   f(MODRM),   o(CVTDQ2PS),DDST|DNO16,                  OLB_0F)

MACRO(CVTSD2SI, Reg2,   None,          RNON,   f(MODRM),   o(CVTSD2SI),DDST|DNO16|DF2,              OLB_0F)
MACRO(CVTSI2SD, Reg2,   None,          RNON,   f(MODRM),   o(CVTSI2SD),DDST|DNO16|DF2,              OLB_0F)
MACRO(CVTSI2SS, Reg2,   None,          RNON,   f(MODRM),   o(CVTSI2SS),DDST|DNO16|DF3,              OLB_0F)
MACRO(CVTPD2PS, Reg2,   None,          RNON,   f(MODRM),   o(CVTPD2PS),DDST|DNO16|D66,              OLB_0F)


MACRO(CVTPS2PD, Reg2,   None,          RNON,   f(MODRM),   o(CVTPS2PD),DDST|DNO16,                  OLB_0F)
MACRO(CVTSD2SS, Reg2,   None,          RNON,   f(MODRM),   o(CVTSD2SS),DDST|DNO16|DF2,              OLB_0F)
MACRO(CVTSS2SD, Reg2,   None,          RNON,   f(MODRM),   o(CVTSS2SD),DDST|DNO16|DF3,              OLB_0F)
MACRO(CVTSS2SI, Reg2,   None,          RNON,   f(MODRM),   o(CVTSS2SI),DDST|DNO16|DF3,              OLB_0F)

MACRO(CVTTPD2DQ,Reg2,   None,          RNON,   f(MODRM),   o(CVTTPD2DQ),DDST|DNO16|D66,             OLB_0F)
MACRO(CVTTPS2DQ,Reg2,   None,          RNON,   f(MODRM),   o(CVTTPS2DQ),DDST|DNO16|DF3,             OLB_0F)


MACRO(CVTTSD2SI,Reg2,   None,          RNON,   f(MODRM),   o(CVTTSD2SI),DDST|DNO16|DF2,             OLB_0F)
MACRO(CVTTSS2SI,Reg2,   None,          RNON,   f(MODRM),   o(CVTTSS2SI),DDST|DNO16|DF3,             OLB_0F)
MACRO(DEC,      Reg2,   OpSideEffect,  R001,   f(INCDEC),  o(DEC),     DOPEQ|DSETCC,                OLB_NONE)
MACRO(DIV,      Reg3,   None,          R110,   f(MULDIV),  o(DIV),     DSETCC,                      OLB_NONE)


MACRO(DIVPD,    Reg3,   None,          RNON,   f(MODRM),   o(DIVPD),   DNO16|DOPEQ|D66,            OLB_0F)
MACRO(DIVPS,    Reg3,   None,          RNON,   f(MODRM),   o(DIVPS),   DNO16|DOPEQ,                OLB_0F)


MACRO(DIVSD,    Reg3,   None,          RNON,   f(MODRM),   o(DIVSD),   DNO16|DOPEQ|DF2,             OLB_0F)
MACRO(DIVSS,    Reg3,   None,          RNON,   f(MODRM),   o(DIVSS),   DNO16|DOPEQ|DF3,             OLB_0F)
MACRO(IDIV,     Reg3,   None,          R111,   f(MULDIV),  o(IDIV),    DSETCC,                      OLB_NONE)
MACRO(INC,      Reg2,   OpSideEffect,  R000,   f(INCDEC),  o(INC),     DOPEQ|DSETCC,                OLB_NONE)
MACRO(IMUL,     Reg3,   OpSideEffect,  R101,   f(MULDIV),  o(IMUL),    DSETCC,                      OLB_NONE)
MACRO(IMUL2,    Reg3,   OpSideEffect,  RNON,   f(SPMOD),   o(IMUL2),   DOPEQ|DSETCC|DCOMMOP,        OLB_0F)
MACRO(INT,      Reg1,   OpSideEffect,  RNON,   f(SPECIAL), o(INT),     DUSECC|DSETCC,               OLB_NONE)
MACRO(JA,       BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JA),      DUSECC|DNO16,                OLB_0F)
MACRO(JAE,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JAE),     DUSECC|DNO16,                OLB_0F)
MACRO(JB,       BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JB),      DUSECC|DNO16,                OLB_0F)
MACRO(JBE,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JBE),     DUSECC|DNO16,                OLB_0F)
MACRO(JEQ,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JEQ),     DUSECC|DNO16,                OLB_0F)
MACRO(JNE,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JNE),     DUSECC|DNO16,                OLB_0F)
MACRO(JLT,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JLT),     DUSECC|DNO16,                OLB_0F)
MACRO(JLE,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JLE),     DUSECC|DNO16,                OLB_0F)
MACRO(JGT,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JGT),     DUSECC|DNO16,                OLB_0F)
MACRO(JGE,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JGE),     DUSECC|DNO16,                OLB_0F)
MACRO(JNO,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JNO),     DUSECC|DNO16,                OLB_0F)
MACRO(JO,       BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JO),      DUSECC|DNO16,                OLB_0F)
MACRO(JP,       BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JP),      DUSECC|DNO16,                OLB_0F)
MACRO(JNP,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JNP),     DUSECC|DNO16,                OLB_0F)
MACRO(JNSB,     BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JNSB),    DUSECC|DNO16,                OLB_0F)
MACRO(JSB,      BrReg2, OpSideEffect,  RNON,   f(JCC),     o(JSB),     DUSECC|DNO16,                OLB_0F)
MACRO(JMP,      Br,     OpSideEffect,  R100,   f(JMP),     o(JMP),     DNO16,                       OLB_NONE)
MACRO(LEA,      Reg2,   None,          RNON,   f(MODRM),   o(LEA),     DDST,                        OLB_NONE)

MACRO(MAXPD,    Reg2,       None,           RNON,   f(MODRM),   o(MAXPD),   DNO16|DOPEQ|D66,        OLB_0F)
MACRO(MAXPS,    Reg2,       None,           RNON,   f(MODRM),   o(MAXPS),   DNO16|DOPEQ,            OLB_0F)
MACRO(MINPD,    Reg2,       None,           RNON,   f(MODRM),   o(MINPD),   DNO16|DOPEQ|D66,        OLB_0F)
MACRO(MINPS,    Reg2,       None,           RNON,   f(MODRM),   o(MINPS),   DNO16|DOPEQ,            OLB_0F)

MACRO(LZCNT,    Reg2,   None,          RNON,   f(MODRM),   o(LZCNT),   DF3|DSETCC|DDST,             OLB_0F)

MACRO(MOV,      Reg2,   None,          R000,   f(MOV),     o(MOV),     DDST,                        OLB_NONE)
MACRO(MOV_TRUNC,Reg2,   None,          R000,   f(MOV),     o(MOV),     DDST,                        OLB_NONE)// Like a MOV, but MOV ECX, ECX can't be removed (truncation)
MACRO(MOVD,     Reg2,   None,          RNON,   f(SPECIAL), o(MOVD),    DDST|DNO16|D66,              OLB_0F)


MACRO(MOVHLPS,  Reg2,       None,           RNON,   f(SPECIAL), o(MOVHLPS), DDST|DNO16,              OLB_0F)
MACRO(MOVHPD,   Reg2,       None,           RNON,   f(SPECIAL), o(MOVHPD),  DDST|DNO16|D66,          OLB_0F)
MACRO(MOVLHPS,  Reg2,       None,           RNON,   f(SPECIAL), o(MOVLHPS), DDST|DNO16,              OLB_0F)
MACRO(MOVMSKPD, Reg2,       None,           RNON,   f(SPECIAL), o(MOVMSKPD), DDST|DNO16|D66,         OLB_0F)
MACRO(MOVMSKPS, Reg2,       None,           RNON,   f(SPECIAL), o(MOVMSKPS), DDST|DNO16,             OLB_0F)


MACRO(MOVSD,    Reg2,   None,          RNON,   f(SPECIAL), o(MOVSD),   DDST|DNO16|DF2,              OLB_0F)
MACRO(MOVSD_ZERO,Reg2,      None,      RNON,   f(MODRM),   o(MOVSD),   0,                           OLB_NONE)
MACRO(MOVSS,    Reg2,   None,          RNON,   f(SPECIAL), o(MOVSS),   DDST|DNO16|DF3,              OLB_0F)
MACRO(MOVAPS,   Reg2,   None,          RNON,   f(SPECIAL), o(MOVAPS),  DDST|DNO16,                  OLB_0F)
MACRO(MOVAPD,   Reg2,   None,          RNON,   f(SPECIAL), o(MOVAPD),  DDST|DNO16|D66,              OLB_0F)
MACRO(MOVSX,    Reg2,   None,          RNON,   f(MODRM),   o(MOVSX),   DDST,                        OLB_0F)
MACRO(MOVSXW,   Reg2,   None,          RNON,   f(MODRM),   o(MOVSXW),  DDST,                        OLB_0F)

MACRO(MOVUPS,   Reg2,       None,           RNON,   f(SPECIAL), o(MOVUPS),  DDST|DNO16,             OLB_0F)

MACRO(MOVZX,    Reg2,   None,          RNON,   f(MODRM),   o(MOVZX),   DDST,                        OLB_0F)
MACRO(MOVZXW,   Reg2,   None,          RNON,   f(MODRM),   o(MOVZXW),  DDST,                        OLB_0F)
MACRO(MOVSXD,   Reg2,   None,          RNON,   f(MODRM),   o(MOVSXD),  DDST,                        OLB_NONE)

MACRO(MULPD,    Reg3,       None,           RNON,   f(MODRM),   o(MULPD),   DNO16|DOPEQ|D66|DCOMMOP,    OLB_0F)
MACRO(MULPS,    Reg3,       None,           RNON,   f(MODRM),   o(MULPS),   DNO16|DOPEQ|DCOMMOP, OLB_0F)


MACRO(MULSD,    Reg3,   None,          RNON,   f(MODRM),   o(MULSD),   DNO16|DOPEQ|DF2,             OLB_0F)
MACRO(MULSS,    Reg3,   None,          RNON,   f(MODRM),   o(MULSS),   DNO16|DOPEQ|DF3|DCOMMOP,     OLB_0F)
MACRO(NEG,      Reg2,   OpSideEffect,  R011,   f(MODRMW),  o(NEG),     DOPEQ|DSETCC,                OLB_NONE)
MACRO(NOP,      Empty,  None,          RNON,   f(SPECIAL), o(NOP),     DNO16,                       OLB_NONE)
MACRO(NOT,      Reg2,   OpSideEffect,  R010,   f(MODRMW),  o(NOT),     DOPEQ,                       OLB_NONE)
MACRO(POP,      Reg1,   OpSideEffect,  R000,   f(PSHPOP),  o(POP),     DDST,                        OLB_NONE)
MACRO(PUSH,     Reg1,   OpSideEffect,  R110,   f(PSHPOP),  o(PUSH),    0,                           OLB_NONE)
MACRO(OR ,      Reg2,   OpSideEffect,  R001,   f(BINOP),   o(OR),      DOPEQ|DSETCC|DCOMMOP,        OLB_NONE)

MACRO(ORPS,     Reg2,   None,           R001,   f(MODRM),   o(ORPS),    DOPEQ|DOPEQ|DCOMMOP,        OLB_0F)
MACRO(PADDD,    Reg2,   None,           RNON,   f(MODRM),   o(PADDD),   DNO16|DOPEQ|D66|DCOMMOP,    OLB_0F)
MACRO(PAND,     Reg2,   None,           RNON,   f(MODRM),   o(PAND),    DNO16|DOPEQ|D66,            OLB_0F)
MACRO(PCMPEQD,  Reg2,   None,           RNON,   f(MODRM),   o(PCMPEQD), DNO16|DOPEQ|D66,            OLB_0F)
MACRO(PCMPGTD,  Reg2,   None,           RNON,   f(MODRM),   o(PCMPGTD), DNO16|DOPEQ|D66,            OLB_0F)
MACRO(PMULUDQ,  Reg2,   None,           RNON,   f(MODRM),   o(PMULUDQ), DNO16|DOPEQ|D66|DCOMMOP,    OLB_0F)
MACRO(POR,      Reg2,   None,           RNON,   f(MODRM),   o(POR),     DNO16|DOPEQ|D66|DCOMMOP,    OLB_0F)
MACRO(PSHUFD,   Reg3,   None,           RNON,   f(MODRM),   o(PSHUFD),  DDST|DNO16|D66|DSSE,        OLB_0F)
MACRO(PSLLDQ,   Reg2,   None,           R111,   f(SPECIAL), o(PSLLDQ),  DDST|DNO16|DOPEQ|D66|DSSE,  OLB_0F)
MACRO(PSRLDQ,   Reg2,   None,           R011,   f(SPECIAL), o(PSRLDQ),  DDST|DNO16|DOPEQ|D66|DSSE,  OLB_0F)
MACRO(PSUBD,    Reg3,   None,           RNON,   f(MODRM),   o(PSUBD),   DNO16|DOPEQ|D66,            OLB_0F)
MACRO(PUNPCKLDQ, Reg3,  None,           RNON,   f(MODRM),   o(PUNPCKLDQ), DNO16|DOPEQ|D66,          OLB_0F)
MACRO(PXOR,     Reg2,   None,           RNON,   f(MODRM),   o(PXOR),    DNO16|DOPEQ|D66|DCOMMOP,    OLB_0F)


MACRO(RET,      Empty,  OpSideEffect,  RNON,   f(SPECIAL), o(RET),     DSETCC,                      OLB_NONE)
MACRO(ROUNDSD,  Reg3,   None,          RNON,   f(MODRM),   o(ROUNDSD), DDST|DNO16|DSSE|D66,         OLB_0F3A)
MACRO(ROUNDSS,  Reg3,   None,          RNON,   f(MODRM),   o(ROUNDSS), DDST|DNO16|DSSE|D66,         OLB_0F3A)
MACRO(SAR,      Reg2,   OpSideEffect,  R111,   f(SHIFT),   o(SAR),     DOPEQ|DSETCC,                OLB_NONE)
MACRO(SBB,      Reg2,   None,          R011,   f(BINOP),   o(SBB),     DOPEQ|DUSECC|DSETCC,         OLB_NONE)
MACRO(SETE,     Reg1,   None,          RNON,   f(MODRM),   o(SETE),    DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETG,     Reg1,   None,          RNON,   f(MODRM),   o(SETG),    DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETGE,    Reg1,   None,          RNON,   f(MODRM),   o(SETGE),   DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETL,     Reg1,   None,          RNON,   f(MODRM),   o(SETL),    DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETLE,    Reg1,   None,          RNON,   f(MODRM),   o(SETLE),   DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETNE,    Reg1,   None,          RNON,   f(MODRM),   o(SETNE),   DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETO,     Reg1,   None,          RNON,   f(MODRM),   o(SETO),    DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETA,     Reg1,   None,          RNON,   f(MODRM),   o(SETA),    DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETAE,    Reg1,   None,          RNON,   f(MODRM),   o(SETAE),   DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETB,     Reg1,   None,          RNON,   f(MODRM),   o(SETB),    DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SETBE,    Reg1,   None,          RNON,   f(MODRM),   o(SETBE),   DOPEQ|DUSECC|DDST,           OLB_0F)
MACRO(SHL,      Reg2,   OpSideEffect,  R100,   f(SHIFT),   o(SHL),     DOPEQ|DSETCC,                OLB_NONE)
MACRO(SHR,      Reg2,   OpSideEffect,  R101,   f(SHIFT),   o(SHR),     DOPEQ|DSETCC,                OLB_NONE)
MACRO(SHLD,     Reg3,   OpSideEffect,  R100,   f(SPECIAL), o(SHLD),    DDST|DOPEQ,                  OLB_0F)

MACRO(SHUFPD,   Reg2,   None,           RNON,   f(MODRM),   o(SHUFPD),  DDST|DNO16|D66|DSSE,        OLB_0F)
MACRO(SHUFPS,   Reg2,   None,           RNON,   f(MODRM),   o(SHUFPS),  DDST|DNO16|DSSE,            OLB_0F)
MACRO(SQRTPD,   Reg2,   None,           RNON,   f(MODRM),   o(SQRTPD),  DDST|DNO16|D66,             OLB_0F)
MACRO(SQRTPS,   Reg2,   None,           RNON,   f(MODRM),   o(SQRTPS),  DDST|DNO16,                 OLB_0F)

MACRO(SQRTSD,   Reg2,   None,          RNON,   f(MODRM),   o(SQRTSD),  DDST|DNO16|DF2,              OLB_0F)
MACRO(SQRTSS,   Reg2,   None,          RNON,   f(MODRM),   o(SQRTSS),  DDST|DNO16|DF3,              OLB_0F)
MACRO(SUB,      Reg2,   OpSideEffect,  R101,   f(BINOP),   o(SUB),     DOPEQ|DSETCC,                OLB_NONE)

MACRO(SUBPD,    Reg3,   None,           RNON,   f(MODRM),   o(SUBPD),   DNO16|DOPEQ|D66,            OLB_0F)
MACRO(SUBPS,    Reg3,   None,           RNON,   f(MODRM),   o(SUBPS),   DNO16|DOPEQ,                OLB_0F)

MACRO(SUBSD,    Reg3,   None,          RNON,   f(MODRM),   o(SUBSD),   DNO16|DOPEQ|DF2,             OLB_0F)
MACRO(SUBSS,    Reg3,   None,          RNON,   f(MODRM),   o(SUBSS),   DNO16|DOPEQ|DF3,             OLB_0F)
MACRO(TEST,     Empty,  OpSideEffect,  R000,   f(TEST),    o(TEST),    DSETCC|DCOMMOP,              OLB_NONE)
MACRO(UCOMISD,  Empty,  None,          RNON,   f(MODRM),   o(UCOMISD), DNO16|D66|DSETCC,            OLB_0F)
MACRO(UCOMISS,  Empty,  None,          RNON,   f(MODRM),   o(UCOMISS), DNO16|DSETCC,                OLB_0F)
MACRO(XCHG,     Reg2,   None,          R000,   f(XCHG),    o(XCHG),    DOPEQ,                       OLB_NONE)
MACRO(XOR,      Reg2,   OpSideEffect,  R110,   f(BINOP),   o(XOR),     DOPEQ|DSETCC|DCOMMOP,        OLB_NONE)
MACRO(XORPS,    Reg3,   None,          RNON,   f(MODRM),   o(XORPS),   DNO16|DOPEQ|DCOMMOP,         OLB_0F)

#undef o
#undef f
