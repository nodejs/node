//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifndef InstructionStart
#define InstructionStart(name, SupportedOperationSize, MaxTemplateSize)
#endif

#ifndef InstructionEnd
#define InstructionEnd(name)
#endif


InstructionStart( ADD       , 1|4   , 11, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<0>)
    FormatAddrImm(EncodeModRM_ByteRM<0>)
InstructionEnd ( ADD )

InstructionStart( ADDSD     , 8     , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( ADDSD )

InstructionStart(ADDSS, 4|8, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(ADDSS)

InstructionStart( AND       , 1|4   , 11, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<4>)
    FormatAddrImm(EncodeModRM_ByteRM<4>)
InstructionEnd ( AND )

InstructionStart( BSR       , 2|4   , 7, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( BSR )

InstructionStart( CALL      , 4     , 7 , NoFlag    )
    FormatUnaryPtr(EncodeFarAddress)
    FormatUnaryReg(EncodeModRM_ByteReg<2>)
    FormatUnaryAddr(EncodeModRM_ByteRM<2>)
InstructionEnd( CALL )

InstructionStart( CDQ      , 4     , 1 , NoFlag    )
    FormatEmpty()
InstructionEnd( CDQ )

InstructionStart( CMP       , 1|4   , 11, NoFlag    )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<7>)
    FormatAddrImm(EncodeModRM_ByteRM<7>)
InstructionEnd ( CMP )

InstructionStart( COMISD    , 8     , 8 , NoFlag    )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( COMISD )

InstructionStart(COMISS, 4|8, 8, NoFlag)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(COMISS)

InstructionStart( CVTDQ2PD  , 8  | 16   , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( CVTDQ2PD )

InstructionStart( CVTPS2PD  , 8  |16   , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( CVTPS2PD )

InstructionStart( CVTSI2SD  , 8     , 8 , AffectOp1 )
    Format2Reg64_32(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( CVTSI2SD )

InstructionStart( CVTTSD2SI , 4     , 8 , AffectOp1 )
    Format2Reg32_64(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( CVTTSD2SI )

InstructionStart( CVTSS2SD , 4|8     , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( CVTSS2SD )

InstructionStart(CVTTSS2SI, 4 | 8, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(CVTTSS2SI)

InstructionStart( CVTSD2SS , 8     , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( CVTSD2SS )

InstructionStart(CVTSI2SS, 4|8, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(CVTSI2SS)

// Unsigned division
InstructionStart( DIV       , 1|4   , 7 , AffectOp1 )
    FormatUnaryReg(EncodeModRM_ByteReg<6>)
    FormatUnaryAddr(EncodeModRM_ByteRM<6>)
InstructionEnd ( DIV )

InstructionStart(IDIV, 1 | 4, 7, AffectOp1)
    FormatUnaryReg(EncodeModRM_ByteReg<7>)
    FormatUnaryAddr(EncodeModRM_ByteRM<7>)
InstructionEnd(IDIV)

InstructionStart( DIVSD     , 8     , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( DIVSD )

InstructionStart(DIVSS, 4|8, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(DIVSS)

InstructionStart( FLD       , 4|8   , 6 , NoFlag    )
    FormatUnaryAddr(EncodeModRM_ByteRM<0>)
InstructionEnd ( FLD )

InstructionStart( FSTP      , 4|8   , 6 , AffectOp1 )
    FormatUnaryAddr(EncodeModRM_ByteRM<3>)
InstructionEnd ( FSTP )

//format imul reg,[addr+offset],imm possible but not implemented
InstructionStart( IMUL      , 4     , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd( IMUL )

InstructionStart( INC       , 1|4   , 7 , AffectOp1 )
    FormatUnaryAddr(EncodeModRM_ByteRM<0>)
    FormatUnaryReg(EncodeOpReg<0x40>)
InstructionEnd ( INC )

InstructionStart( JMP       , 1|4   , 6 , NoFlag    )
    FormatUnaryImm(Encode_Empty)
    FormatUnaryAddr(EncodeModRM_ByteRM<4>)
    FormatUnaryReg(EncodeModRM_ByteReg<4>)
InstructionEnd ( JMP )

InstructionStart( LAHF      , 1|4|8 , 1 , NoFlag    )
    FormatEmpty()
InstructionEnd ( LAHF )

InstructionStart( MOV       , 1|2|4   , 11, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<0>)
    FormatAddrImm(EncodeModRM_ByteRM<0>)
InstructionEnd ( MOV )

InstructionStart( MOVD      , 4|8   , 8 , AffectOp1 )
    Format2RegCustomCheck(EncodeModRM_2Reg,true)
    FormatRegAddrCustomCheck(EncodeModRM_RegRM,true)
    FormatAddrRegCustomCheck(EncodeModRM_RegRM,true)
InstructionEnd ( MOVD )

InstructionStart( MOVSD     , 8     , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd ( MOVSD )

InstructionStart( MOVSS     , 4|8   , 8, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd ( MOVSS )

InstructionStart( MOVSX     , 1|2   , 11, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( MOVSX )

InstructionStart( MOVZX     , 1|2   , 11, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( MOVZX )

// Unsigned multiplication
InstructionStart( MUL       , 1|4   , 7 , AffectOp1 )
    FormatUnaryReg(EncodeModRM_ByteReg<4>)
    FormatUnaryAddr(EncodeModRM_ByteRM<4>)
InstructionEnd ( MUL )

InstructionStart( MULSD     , 8     , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( MULSD )

InstructionStart(MULSS, 4|8, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(MULSS)


InstructionStart( NEG     , 1|4     , 6 , AffectOp1 )
    FormatUnaryReg(EncodeModRM_ByteReg<3>)
    FormatUnaryAddr(EncodeModRM_ByteRM<3>)
InstructionEnd ( NEG )

InstructionStart( NOT     , 1|4     , 6 , AffectOp1 )
    FormatUnaryReg(EncodeModRM_ByteReg<2>)
    FormatUnaryAddr(EncodeModRM_ByteRM<2>)
InstructionEnd ( NOT )

InstructionStart( OR        , 1|4   , 11, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<1>)
    FormatAddrImm(EncodeModRM_ByteRM<1>)
InstructionEnd ( OR )

InstructionStart( POP      , 4   , 7 , AffectOp1    )
    FormatUnaryReg(Encode_Empty)
    FormatUnaryAddr(EncodeModRM_ByteRM<0>)
InstructionEnd( POP )

InstructionStart( PUSH      , 1|4   , 7 , NoFlag    )
    FormatUnaryImm(Encode_Empty)
    FormatUnaryReg(EncodeModRM_ByteReg<6>)
    FormatUnaryAddr(EncodeModRM_ByteRM<6>)
InstructionEnd( PUSH )

InstructionStart( RET      , 1|2|4|8 , 1 , NoFlag    )
    FormatUnaryImm(Encode_Empty)
InstructionEnd( RET )

#define ShiftInstruction(name, val)\
InstructionStart( name       , 1|4     , 7 , AffectOp1 )\
    FormatUnaryReg(EncodeModRM_ByteReg<val>)\
    FormatUnaryAddr(EncodeModRM_ByteRM<val>)\
    Format2Reg(EncodeModRM_ByteReg<val>)\
    FormatAddrReg(EncodeModRM_ByteRM<val>)\
    FormatRegImm(EncodeModRM_ByteReg<val>)\
    FormatAddrImm(EncodeModRM_ByteRM<val>)\
InstructionEnd ( name )

ShiftInstruction(SAL, 4)
ShiftInstruction(SAR, 7)
ShiftInstruction(SHL, 4)
ShiftInstruction(SHR, 5)
#undef ShiftInstruction

InstructionStart( SUB       , 1|4   , 11, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<5>)
    FormatAddrImm(EncodeModRM_ByteRM<5>)
InstructionEnd ( SUB )

InstructionStart(SUBSS, 4 | 8, 11, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<5>)
    FormatAddrImm(EncodeModRM_ByteRM<5>)
InstructionEnd(SUBSS)


InstructionStart( SUBSD     , 8     , 8 , AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( SUBSD )

InstructionStart( TEST      , 1|4   , 7 , NoFlag    )
    Format2Reg(EncodeModRM_2Reg)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<0>)
    FormatAddrImm(EncodeModRM_ByteRM<0>)
InstructionEnd ( TEST )

InstructionStart( UCOMISD   , 8     , 8 , NoFlag    )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd ( UCOMISD )

InstructionStart(UCOMISS, 4|8, 8, NoFlag)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(UCOMISS)

InstructionStart( XOR       , 1|4   , 11, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegImm(EncodeModRM_ByteReg<6>)
    FormatAddrImm(EncodeModRM_ByteRM<6>)
InstructionEnd ( XOR )

InstructionStart( XORPS       , 8 | 16  , 8, AffectOp1 )
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd ( XORPS )

#define Jcc(name) \
InstructionStart( name, 1|4 , 6, NoFlag )\
    FormatUnaryImm(Encode_Empty)\
InstructionEnd ( name )

        Jcc(JA  )
        Jcc(JAE )
        Jcc(JB  )
        Jcc(JBE )
        Jcc(JC  )
        Jcc(JE  )
        Jcc(JG  )
        Jcc(JGE )
        Jcc(JL  )
        Jcc(JLE )
        Jcc(JNA )
        Jcc(JNAE)
        Jcc(JNB )
        Jcc(JNBE)
        Jcc(JNC )
        Jcc(JNE )
        Jcc(JNG )
        Jcc(JNGE)
        Jcc(JNL )
        Jcc(JNLE)
        Jcc(JNO )
        Jcc(JNP )
        Jcc(JNS )
        Jcc(JNZ )
        Jcc(JO  )
        Jcc(JP  )
        Jcc(JPE )
        Jcc(JPO )
        Jcc(JS  )
        Jcc(JZ  )
#undef Jcc

#define SETFLAG(name) \
InstructionStart( name, 1 , 3, AffectOp1 )\
    FormatUnaryReg8Bits(EncodeModRM_ByteReg<0>)\
InstructionEnd ( name )

    SETFLAG(SETA)
    SETFLAG(SETAE)
    SETFLAG(SETB)
    SETFLAG(SETBE)
    SETFLAG(SETC)
    SETFLAG(SETE)
    SETFLAG(SETG)
    SETFLAG(SETGE)
    SETFLAG(SETL)
    SETFLAG(SETLE)
    SETFLAG(SETNA)
    SETFLAG(SETNAE)
    SETFLAG(SETNB)
    SETFLAG(SETNBE)
    SETFLAG(SETNC)
    SETFLAG(SETNE)
    SETFLAG(SETNG)
    SETFLAG(SETNGE)
    SETFLAG(SETNL)
    SETFLAG(SETNLE)
    SETFLAG(SETNO)
    SETFLAG(SETNP)
    SETFLAG(SETNS)
    SETFLAG(SETNZ)
    SETFLAG(SETO)
    SETFLAG(SETP)
    SETFLAG(SETPE)
    SETFLAG(SETPO)
    SETFLAG(SETS)
    SETFLAG(SETZ)
#undef SETFLAG

#define CMOV(name) \
InstructionStart( name, 4 , 7, AffectOp1 )\
    Format2Reg(EncodeModRM_2Reg)\
    FormatRegAddr(EncodeModRM_RegRM)\
InstructionEnd ( name )

    CMOV(CMOVA)
    CMOV(CMOVAE)
    CMOV(CMOVB)
    CMOV(CMOVBE)
    CMOV(CMOVC)
    CMOV(CMOVE)
    CMOV(CMOVG)
    CMOV(CMOVGE)
    CMOV(CMOVL)
    CMOV(CMOVLE)
    CMOV(CMOVNA)
    CMOV(CMOVNAE)
    CMOV(CMOVNB)
    CMOV(CMOVNBE)
    CMOV(CMOVNC)
    CMOV(CMOVNE)
    CMOV(CMOVNG)
    CMOV(CMOVNGE)
    CMOV(CMOVNL)
    CMOV(CMOVNLE)
    CMOV(CMOVNO)
    CMOV(CMOVNP)
    CMOV(CMOVNS)
    CMOV(CMOVNZ)
    CMOV(CMOVO)
    CMOV(CMOVP)
    CMOV(CMOVPE)
    CMOV(CMOVPO)
    CMOV(CMOVS)
    CMOV(CMOVZ)
#undef CMOV

    //SSE2 instructions
InstructionStart(MOVUPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd(MOVUPS)

InstructionStart(MOVAPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd(MOVAPS)

InstructionStart(MOVHPD, 16, 9, AffectOp1)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatAddrReg(EncodeModRM_RegRM)
InstructionEnd(MOVHPD)

InstructionStart(MOVHLPS, 16, 3, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
InstructionEnd(MOVHLPS)

InstructionStart(MOVLHPS, 16, 3, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
InstructionEnd(MOVLHPS)

InstructionStart(SHUFPS, 16, 9, AffectOp1)
    Format2RegImm8(EncodeModRM_2Reg)
    FormatRegAddrImm8(EncodeModRM_RegRM)
InstructionEnd(SHUFPS)

InstructionStart(SHUFPD, 16, 10, AffectOp1)
    Format2RegImm8(EncodeModRM_2Reg)
    FormatRegAddrImm8(EncodeModRM_RegRM)
InstructionEnd(SHUFPD)

InstructionStart(PSHUFD, 16, 10, AffectOp1)
    Format2RegImm8(EncodeModRM_2Reg)
    FormatRegAddrImm8(EncodeModRM_RegRM)
InstructionEnd(PSHUFD)

InstructionStart(CVTPD2PS, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(CVTPD2PS)

InstructionStart(CVTDQ2PS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(CVTDQ2PS)

InstructionStart(CVTTPS2DQ, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(CVTTPS2DQ)

InstructionStart(CVTTPD2DQ, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(CVTTPD2DQ)

InstructionStart(ANDPD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd(ANDPD)

InstructionStart(PXOR, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd(PXOR)

InstructionStart(DIVPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(DIVPS)

InstructionStart(DIVPD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(DIVPD)

InstructionStart(SQRTPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(SQRTPS)

InstructionStart(SQRTPD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(SQRTPD)

InstructionStart(ADDPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(ADDPS)

InstructionStart(ADDPD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(ADDPD)

InstructionStart(PADDD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd(PADDD)

InstructionStart(SUBPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(SUBPS)

InstructionStart(SUBPD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(SUBPD)

InstructionStart(PSUBD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(PSUBD)

InstructionStart(MULPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    InstructionEnd(MULPS)

InstructionStart(MULPD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(MULPD)

InstructionStart(PMULUDQ, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(PMULUDQ)

InstructionStart(PSRLDQ, 16, 9, AffectOp1)
    FormatRegImm8(EncodeModRM_ByteReg<3>)
InstructionEnd(PSRLDQ)

InstructionStart(PUNPCKLDQ, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(PUNPCKLDQ)

InstructionStart(MINPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(MINPS)

InstructionStart(MAXPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(MAXPS)

InstructionStart(MINPD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(MINPD)

InstructionStart(MAXPD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(MAXPD)

enum CMP_IMM8
{
    EQ,
    LT,
    LE,
    UNORD,
    NEQ,
    NLT,
    NLE,
    ORD
};

InstructionStart(CMPPS, 16, 8, AffectOp1)
    Format2RegImm8(EncodeModRM_2Reg)
    FormatRegAddrImm8(EncodeModRM_RegRM)
InstructionEnd(CMPPS)

InstructionStart(CMPPD, 16, 9, AffectOp1)
    Format2RegImm8(EncodeModRM_2Reg)
    FormatRegAddrImm8(EncodeModRM_RegRM)
InstructionEnd(CMPPD)

InstructionStart(PCMPGTD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(PCMPGTD)

InstructionStart(PCMPEQD, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(PCMPEQD)

InstructionStart(ANDPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd(ANDPS)

InstructionStart(ORPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(ORPS)

InstructionStart(PAND, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(PAND)

InstructionStart(ANDNPS, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
    FormatRegPtr(EncodeModRM_RegPtr)
InstructionEnd(ANDNPS)

InstructionStart(ANDNPD, 16, 8, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(ANDNPD)

InstructionStart(POR, 16, 9, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
    FormatRegAddr(EncodeModRM_RegRM)
InstructionEnd(POR)

InstructionStart(MOVMSKPS, 4|16, 3, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
InstructionEnd(MOVMSKPS)

InstructionStart(MOVMSKPD, 4 | 16, 4, AffectOp1)
    Format2Reg(EncodeModRM_2Reg)
InstructionEnd(MOVMSKPD)
