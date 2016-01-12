//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//
// Contains constants and tables used by the encoder.
//
#include "AssemblyStep.h"

// THUMB2 specific decl
typedef unsigned int ENCODE_32;
// THUMB1 specific decl.
typedef unsigned short ENCODE_16;
typedef unsigned char ENCODE_8;

#define MaxCondBranchOffset ((1 << 20) - 2)

#define RETURN_REG          RegR0
#define FIRST_INT_ARG_REG   RegR0
#define LAST_INT_ARG_REG    RegR3
#define NUM_INT_ARG_REGS\
    ((LAST_INT_ARG_REG - FIRST_INT_ARG_REG) + 1)
#define FIRST_CALLEE_SAVED_GP_REG RegR4
#define LAST_CALLEE_SAVED_GP_REG  RegR10
#define SCRATCH_REG         RegR12
#define ALT_LOCALS_PTR      RegR7
#define EH_STACK_SAVE_REG   RegR6
#define SP_ALLOC_SCRATCH_REG RegR4
#define CATCH_OBJ_REG       RegR1

#define RETURN_DBL_REG      RegD0
#define FIRST_CALLEE_SAVED_DBL_REG RegD8
#define LAST_CALLEE_SAVED_DBL_REG  RegD15
#define FIRST_CALLEE_SAVED_DBL_REG_NUM 8
#define LAST_CALLEE_SAVED_DBL_REG_NUM 15
#define CALLEE_SAVED_DOUBLE_REG_COUNT 8

// See comment in LowerEntryInstr: even in a global function, we'll home r0 and r1
#define MIN_HOMED_PARAM_REGS 2
// THUMB Specific
#define MAX_INT_REGISTERS_LOW     8
#define LOW_INT_REGISTERS_MASK    0x7

#define FRAME_REG           RegR11

#define DMOVE              0x0001
#define DLOAD              0x0002
#define DSTORE             0x0003
#define DMASK              0x0007
#define DHALFORSB          0x0020    // halfword or signed byte
#define DCONDEXE           0x0040    // safe for conditional execution
#define DSUPDATE           0x0100
#define DSSUB              0x0200
#define DSPOST             0x0400
#define DSBIT              0x0800
#define DQBR               0x1000 // Qualcomm buggy branch instructions see macro SOFTWARE_FIXFOR_HARDWARE_BUGWIN8_502326

#define D____              (0)
#define D___C              (DCONDEXE)
#define DQ__C              (DQBR | DCONDEXE)
#define D__SC              (DSBIT)
#define DM___              (DMOVE)
#define DM__C              (DMOVE | DCONDEXE)
#define DL___              (DLOAD)
#define DL__C              (DLOAD | DCONDEXE)
#define DLH__              (DLOAD | DHALFORSB)
#define DLH_C              (DLOAD | DHALFORSB | DCONDEXE)
#define DS___              (DSTORE)
#define DS__C              (DSTORE | DCONDEXE)
#define DSH__              (DSTORE | DHALFORSB)
#define DSH_C              (DSTORE | DHALFORSB | DCONDEXE)
#define DLUP_C             (DLOAD | DSUPDATE | DSPOST)
#define DLUPQC             (DLOAD | DSUPDATE | DSPOST | DQBR)
#define DSUS_C             (DSTORE | DSUPDATE | DSSUB)

#define ISMOVE(o)          ((EncoderMD::GetOpdope(o) & DMASK) == DMOVE)
#define ISLOAD(o)          ((EncoderMD::GetOpdope(o) & DMASK) == DLOAD)
#define ISSTORE(o)         ((EncoderMD::GetOpdope(o) & DMASK) == DSTORE)

#define ISSHIFTERUPDATE(o) ((EncoderMD::GetOpdope(o) & DSUPDATE) != 0)
#define ISSHIFTERSUB(o)    ((EncoderMD::GetOpdope(o) & DSSUB) != 0)
#define ISSHIFTERPOST(o)   ((EncoderMD::GetOpdope(o) & DSPOST) != 0)

#define SETS_SBIT(o)       ((EncoderMD::GetOpdope(o) & DSBIT) != 0)
#define ISQBUGGYBR(o)      ((EncoderMD::GetOpdope(o) & DQBR) != 0)

static const uint32 Opdope[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, opbyte, dope, ...) dope,
#include "MdOpcodes.h"
#undef MACRO
};

static const BYTE RegEncode[] =
{
#define REGDAT(Name, Listing, Encoding, ...) Encoding,
#include "RegList.h"
#undef REGDAT
};

#define REGBIT(reg)             (1 << RegEncode[reg])
#define TESTREGBIT(mask, reg)   (((mask) & REGBIT(reg)) != 0)
#define SETREGBIT(mask, reg)    ((mask) |= REGBIT(reg))
#define CLEARREGBIT(mask, reg)  ((mask) &= ~REGBIT(reg))
#define ZEROREGMASK(mask)       ((mask) = 0)
#define REGMASKEMPTY(mask)      ((mask) == 0)

#define CO_UIMMED8(intConst)        ((intConst) & 0xff) // 8 bit unsigned immediate

// Used by GenerateInstrs
#define CASE_OPCODES_ALWAYS_THUMB2      \
    case Js::OpCode::ADDW:              \
    case Js::OpCode::LDRRET:            \
    case Js::OpCode::MLS:               \
    case Js::OpCode::MOVT:              \
    case Js::OpCode::MOVW:              \
    case Js::OpCode::MOV_W:             \
    case Js::OpCode::NOP_W:             \
    case Js::OpCode::SDIV:              \
    case Js::OpCode::SUBW:              \
    case Js::OpCode::TIOFLW:            \
    case Js::OpCode::CLRSIGN:           \
    case Js::OpCode::SBCMPLNT:          \
    case Js::OpCode::PLD:               \
    case Js::OpCode::CLZ:


#define CASE_OPCODES_NEVER_THUMB2           \
    case Js::OpCode::DEBUGBREAK:            \
    case Js::OpCode::NOP:                   \
    case Js::OpCode::BLX:                   \
    case Js::OpCode::BX:                    \
    case Js::OpCode::RET:


#define IS_LOWREG(RegNum) (RegNum < RegR8)

#define IS_CONST_00000007(x) (((x) & ~0x00000007) == 0)
#define IS_CONST_0000001F(x) (((x) & ~0x0000001f) == 0)
#define IS_CONST_0000007F(x) (((x) & ~0x0000007f) == 0)
#define IS_CONST_000000FF(x) (((x) & ~0x000000ff) == 0)
#define IS_CONST_000003FF(x) (((x) & ~0x000003ff) == 0)
#define IS_CONST_00000FFF(x) (((x) & ~0x00000fff) == 0)
#define IS_CONST_0000FFFF(x) (((x) & ~0x0000ffff) == 0)
#define IS_CONST_000FFFFF(x) (((x) & ~0x000fffff) == 0)
#define IS_CONST_007FFFFF(x) (((x) & ~0x007fffff) == 0)

#define IS_CONST_NEG_8(x)    (((x) & ~0x0000007f) == ~0x0000007f)
#define IS_CONST_NEG_12(x)   (((x) & ~0x000007ff) == ~0x000007ff)
#define IS_CONST_NEG_21(x)   (((x) & ~0x000fffff) == ~0x000fffff)
#define IS_CONST_NEG_24(x)   (((x) & ~0x007fffff) == ~0x007fffff)

#define IS_CONST_INT8(x)     (IS_CONST_0000007F(x) || IS_CONST_NEG_8(x))
#define IS_CONST_INT12(x)    (IS_CONST_00000FFF(x) || IS_CONST_NEG_12(x))
#define IS_CONST_INT21(x)    (IS_CONST_000FFFFF(x) || IS_CONST_NEG_21(x))
#define IS_CONST_INT24(x)    (IS_CONST_007FFFFF(x) || IS_CONST_NEG_24(x))

#define IS_CONST_UINT3(x)    IS_CONST_00000007(x)
#define IS_CONST_UINT5(x)    IS_CONST_0000001F(x)
#define IS_CONST_UINT7(x)    IS_CONST_0000007F(x)
#define IS_CONST_UINT8(x)    IS_CONST_000000FF(x)
#define IS_CONST_UINT10(x)   IS_CONST_000003FF(x)
#define IS_CONST_UINT12(x)   IS_CONST_00000FFF(x)
#define IS_CONST_UINT16(x)   IS_CONST_0000FFFF(x)


typedef struct _FormTable
{
    int form;
    ENCODE_32 inst;
    const AssemblyStep * steps;
} FormTable;

#define FDST(_form)         (FORM_ ## _form)
#define FSET(_form, _bits)  (FORM_ ## _form << _bits)
#define FSRC(_form,_opnum)  (FORM_ ## _form << ((_opnum) * FORM_BITS_PER_OPN))
#define FDST2(_form,_opnum) (FORM_ ## _form << ((_opnum) * FORM_BITS_PER_OPN))
#define FTHUMB2             (FORM_ ## THUMB2 << 28)
#define FTHUMB              (FORM_ ## THUMB << 28)

#define THUMB2_THUMB1_FORM(FTP,IFORM) \
        ( (FTP) == (((IFORM) & 0x0FFFFFFF) | FTHUMB))

// NOTE if we end up with a form with more than six operands the IFORM
// encoding needs to be reworked.  (runs out of bits)
typedef enum tagIFORM
{
    IFORM_A       = 1,  // This is used in mdtupdat.h, and must be IFORM type

    // CONSIDER: we seem to have some collisions here
    //  (is there a way to cleanup this? we'd need more than 16 values)

    FORM_REG      = 1,
    FORM_SREG     = 2,  // VFP single precision register
    FORM_DREG     = 3,  // VFP double precision register
    FORM_XREG     = 7,
    FORM_WREG     = 8,  // maps to "wR"
    FORM_WCREG    = 9,  // maps to "wC"
    FORM_CONST    = 4,
    FORM_INDIR    = 5,

    FORM_LABEL    = 6,
    FORM_CODE     = 7,

    FORM_PC       = 6,
    FORM_SP       = 7,
    FORM_CC       = 10,

    FORM_HREG     = 2,
    FORM_ACC      = 3,
    FORM_SR       = 5,

    FORM_NREG     = 11, // NEON 64bit register
    FORM_QREG     = 12, // NEON 128bit register

    FORM_THUMB    = 2,
    FORM_THUMB2   = 8,
    FORM_ARM      = 4,

    FORM_BITS_PER_OPN  = 4,

    FORM__________  = 0,
    FORM_2________  = FTHUMB2,
    FORM_T________  = FTHUMB,

    FORM_Trrc_____  = FTHUMB | FDST(REG) | FSRC(REG,1) | FSRC(CONST,2),
    FORM_Trc______  = FTHUMB | FDST(REG) | FSRC(CONST, 1),
    FORM_TrcC_____  = FTHUMB | FDST(REG) | FSRC(CONST, 1) | FSRC(CC,2),
    FORM_Trrr_____  = FTHUMB | FDST(REG) | FSRC(REG,1) | FSRC(REG,2),
    FORM_Trr______  = FTHUMB | FDST(REG) | FSRC(REG,1),
    FORM_TrC_____   = FTHUMB | FDST(REG) | FSRC(CC,1),
    FORM_TrrC_____  = FTHUMB | FDST(REG) | FSRC(REG,1) | FSRC(CC,2),
    FORM_Tr_______  = FTHUMB | FDST(REG),
    FORM_T_r______  = FTHUMB | FSRC(REG,1),
    FORM_T_rc_____  = FTHUMB | FSRC(REG,1) | FSRC(CONST, 2),
    FORM_T_rr_____  = FTHUMB | FSRC(REG,1) | FSRC(REG,2),
    FORM_Thr______  = FTHUMB | FSET(REG,28) | FSRC(REG,1),
    FORM_Th_r_____  = FTHUMB | FSET(REG,28) | FDST(REG),
    FORM_Th_rr____  = FTHUMB | FSET(REG,28) | FSRC(REG,1) | FSRC(REG,2),
    FORM_T_spr____  = FTHUMB | FSRC(SP,1) | FSRC(REG,2),
    FORM_Th_spr___  = FTHUMB | FSET(REG,28) | FSRC(SP,1) | FSRC(REG,2),

    // Thumb2
    FORM_2rrrcc___  = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(REG,2) | FSRC(CONST,3) | FSRC(CONST,4),
    FORM_2rrrccC__  = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(REG,2) | FSRC(CONST,3) | FSRC(CONST,4) | FSRC(CC,5),
    FORM_2rrc_____  = FTHUMB2 | FDST(REG) | FSRC(REG, 1) | FSRC(CONST, 2),
    FORM_2rrr_____  = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(REG,2),
    FORM_2rrrc____  = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(REG,2) | FSRC(CONST, 3),
    FORM_2rc______  = FTHUMB2 | FDST(REG) | FSRC(CONST, 1),
    FORM_2rcc_____  = FTHUMB2 | FDST(REG) | FSRC(CONST, 1) | FSRC(CONST, 2),
    FORM_2rrcc____  = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(CONST, 2) | FSRC(CONST, 3),
    FORM_2rrcC____  = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(CONST, 2) | FSRC(CC, 3),
    FORM_2rr______  = FTHUMB2 | FDST(REG) | FSRC(REG,1),
    FORM_2ipcc____  = FTHUMB2 | FDST(INDIR) | FSRC(PC,1) | FSRC(CONST,2),
    FORM_2ispc____  = FTHUMB2 | FDST(INDIR) | FSRC(SP,1) | FSRC(CONST,2),
    FORM_2risp____  = FTHUMB2 | FDST(REG) | FSRC(INDIR,1) | FSRC(SP,2),
    FORM_2rrC_____  = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(CC,2),
    FORM_ThrC_____  = FTHUMB | FSET(REG,28) | FDST(REG) | FSRC(CC,1),
    FORM_ThrrC____  = FTHUMB | FSET(REG,28) | FDST(REG) | FSRC(REG,1) | FSRC(CC,2),
    FORM_TrhrC____  = FTHUMB | FDST(REG) | FSET(REG,28) |FSRC(REG,1) | FSRC(CC,2),
    FORM_ThrhrC___  = FTHUMB | FDST(REG) | FSET(REG,28) |FSRC(REG,1) | FSRC(CC,2),
    FORM_Tspsp____  = FTHUMB | FDST(SP) | FSRC(SP,1),

    FORM_2irr_____  = FTHUMB2 | FDST(INDIR) | FSRC(REG,1) | FSRC(REG,2),
    FORM_2ircrr___  = FTHUMB2 | FDST(INDIR) | FSRC(REG,1) | FSRC(CONST,2) | FSRC(REG,3) | FSRC(REG,4),
    FORM_2ispcrr__  = FTHUMB2 | FDST(INDIR) | FSRC(SP,1) | FSRC(CONST,2) | FSRC(REG,3) | FSRC(REG,4),

    // MOVW with relocation
    FORM_2rlsp____  = FTHUMB2 | FDST(REG) | FSRC(LABEL,1) | FSRC(SP,2),
    FORM_2re______  = FTHUMB2 | FDST(REG) | FSRC(CODE,1),
    FORM_2rl______  = FTHUMB2 | FDST(REG) | FSRC(LABEL,1),
    // MOVW with PAIR relocation
    FORM_2rlspc___  = FTHUMB2 | FDST(REG) | FSRC(LABEL,1) | FSRC(SP,2) | FSRC(CONST, 3),

    // MOVT with relocation
    FORM_2rlspr___  = FTHUMB2 | FDST(REG) | FSRC(LABEL,1) | FSRC(SP,2) | FSRC(REG,3),
    FORM_2rer_____  = FTHUMB2 | FDST(REG) | FSRC(CODE,1) | FSRC(REG,2),
    FORM_2rlr_____  = FTHUMB2 | FDST(REG) | FSRC(LABEL,1) | FSRC(REG,2),
    FORM_2rlsprc__  = FTHUMB2 | FDST(REG) | FSRC(LABEL,1) | FSRC(SP,2) | FSRC(REG,3) | FSRC(CONST, 4),

    //ADD
    FORM_Trpcc____  = FTHUMB | FDST(REG) | FSRC(PC,1) | FSRC(CONST,2),
    FORM_Trspc____  = FTHUMB | FDST(REG) | FSRC(SP,1) | FSRC(CONST,2),
    FORM_2rspc____  = FTHUMB2 | FDST(REG) | FSRC(SP,1) | FSRC(CONST,2),
    FORM_2sprc____  = FTHUMB2 | FDST(SP) | FSRC(REG,1) | FSRC(CONST,2),
    FORM_Tsphr____  = FTHUMB | FDST(SP) | FSET(REG, 28) | FSRC(REG,1),
    FORM_Trpc_____  = FTHUMB | FDST(REG) | FSRC(PC,1),
    //ADD, MOVH
    FORM_Trsp_____ = FTHUMB | FDST(REG) | FSRC(SP,1),
    FORM_Thrsp____ = FTHUMB | FSET(REG,28) |FDST(REG) | FSRC(SP,1),

    // ADD, SUB sp,sp,#imm
    FORM_2spspc___  = FTHUMB2 | FDST(SP) | FSRC(SP,1) | FSRC(CONST,2),
    FORM_2rrrC____  = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(REG,2) | FSRC(CC,3),

    FORM_Tspspc___  = FTHUMB | FDST(SP) | FSRC(SP,1) | FSRC(CONST,2),
    FORM_Tspspr___  = FTHUMB | FDST(SP) | FSRC(SP,1) | FSRC(REG,2),
    FORM_Tspsphr__  = FTHUMB | FDST(SP) | FSET(REG, 28) | FSRC(SP,1) | FSRC(REG,2),
    FORM_Tpcpcr___  = FTHUMB | FDST(PC) | FSRC(PC,1) | FSRC(REG,2),

    //CMP
    FORM_2_r______ = FTHUMB2 | FSRC(REG,1),
    FORM_2_rr_____ = FTHUMB2 | FSRC(REG,1) |FSRC(REG,2),
    FORM_2_rc_____ = FTHUMB2 | FSRC(REG,1) |FSRC(CONST,2),

    //TBB, TBH
    FORM_2_irr____ = FTHUMB2 | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_2_irrc___ = FTHUMB2 | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(REG,3) | FSRC(CONST,4),
    FORM_2_ipcr___ = FTHUMB2 | FSRC(INDIR,1) | FSRC(PC,2) | FSRC(REG,3),
    FORM_2_ipcrc__ = FTHUMB2 | FSRC(INDIR,1) | FSRC(PC,2) | FSRC(REG,3) | FSRC(CONST,4),

    //PLD
    FORM_2_irc____ = FTHUMB2 | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(CONST,3),

    //B,BL,BCALL,BBL
    FORM_2l_______ = FTHUMB2 | FSRC(LABEL,1),
    FORM_2lC______ = FTHUMB2 | FSRC(LABEL,1) | FSRC(CC,2),
    FORM_2e_______ = FTHUMB2 | FSRC(CODE,1),
    FORM_Tl_______ = FTHUMB | FSRC(LABEL,1),
    FORM_TlC______ = FTHUMB | FSRC(LABEL,1) | FSRC(CC,2),
    FORM_Te_______ = FTHUMB | FSRC(CODE,1),

    //MULL, UMULL
    FORM_2rrrr____ = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(REG,2) | FSRC(REG,3),

    //SMLAL, UMLAL
    FORM_2rrrrr___ = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(REG,2) | FSRC(REG,3) | FSRC(REG,4),

    //LDRRET
    FORM_2pcispc__ = FTHUMB2 | FDST(PC) | FSRC(INDIR,1) | FSRC(SP,2) | FSRC(CONST,3),

    //LDR
    FORM_2rirc____ = FTHUMB2 | FDST(REG) | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(CONST,3),
    FORM_2rispc___ = FTHUMB2 | FDST(REG) | FSRC(INDIR,1) | FSRC(SP,2) | FSRC(CONST,3),
    FORM_2ripcc___ = FTHUMB2 | FDST(REG) | FSRC(INDIR,1) | FSRC(PC,2) | FSRC(CONST,3),
    FORM_2rirr____ = FTHUMB2 | FDST(REG) | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_2rispr___ = FTHUMB2 | FDST(REG) | FSRC(INDIR,1) | FSRC(SP,2) | FSRC(REG,3),
    FORM_2rirrc___ = FTHUMB2 | FDST(REG) | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(REG,3) | FSRC(CONST,4),

    FORM_Trirc____ = FTHUMB | FDST(REG) | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(CONST,3),
    FORM_Trirr____ = FTHUMB | FDST(REG) | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_Tripcc___ = FTHUMB | FDST(REG) | FSRC(INDIR,1) | FSRC(PC,2) | FSRC(CONST,3),
    FORM_Trispc___ = FTHUMB | FDST(REG) | FSRC(INDIR,1) | FSRC(SP,2) | FSRC(CONST,3),
    //LDR SYM
    FORM_2ri______ = FTHUMB2 | FDST(REG) | FSRC(INDIR,1),
    FORM_2ripc____ = FTHUMB2 | FDST(REG) | FSRC(INDIR,1) | FSRC(PC,2),
    FORM_Tri______ = FTHUMB | FDST(REG) | FSRC(INDIR,1),
    FORM_Trisp____ = FTHUMB | FDST(REG) | FSRC(INDIR,1) | FSRC(SP,2),
    FORM_Tripc____ = FTHUMB | FDST(REG) | FSRC(INDIR,1) | FSRC(PC,2),

    //STR
    FORM_2ircr____ = FTHUMB2 | FDST(INDIR) | FSRC(REG,1) | FSRC(CONST,2) | FSRC(REG,3),
    FORM_2ispcr___ = FTHUMB2 | FDST(INDIR) | FSRC(SP,1) | FSRC(CONST,2) | FSRC(REG,3),
    FORM_2irrr____ = FTHUMB2 | FDST(INDIR) | FSRC(REG,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_2irrrc___ = FTHUMB2 | FDST(INDIR) | FSRC(REG,1) | FSRC(REG,2) | FSRC(REG,3) | FSRC(CONST,4),
    FORM_2isprr___ = FTHUMB2 | FDST(INDIR) | FSRC(SP,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_2ispr____ = FTHUMB2 | FDST(INDIR) | FSRC(SP,1) | FSRC(REG,2),
    FORM_2ir______ = FTHUMB2 | FDST(INDIR) | FSRC(REG,1),
    FORM_2irsp____ = FTHUMB2 | FDST(INDIR) | FSRC(REG,1) | FSRC(SP,2),

    FORM_Tircr____ = FTHUMB | FSRC(INDIR,0) | FSRC(REG,1) | FSRC(CONST,2) | FSRC(REG,3),
    FORM_Tirrr____ = FTHUMB | FDST(INDIR) | FSRC(REG,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_Tispcr___ = FTHUMB | FDST(INDIR) | FSRC(SP,1) | FSRC(CONST,2) | FSRC(REG,3),
    FORM_Tir______ = FTHUMB | FDST(INDIR) | FSRC(REG,1),
    FORM_Tispr____ = FTHUMB | FDST(INDIR) | FSRC(SP,1) | FSRC(REG,2),
    FORM_Tipcr____ = FTHUMB | FDST(INDIR) | FSRC(PC,1) | FSRC(REG,2),
    FORM_Tirsp____ = FTHUMB | FDST(INDIR) | FSRC(REG,1) | FSRC(SP,2),


    //STM/LDM
    FORM_Tispc____ = FTHUMB | FDST(INDIR) | FSRC(SP,1) | FSRC(CONST,2),
    FORM_Tirc_____ = FTHUMB | FDST(INDIR) | FSRC(REG,1) | FSRC(CONST,2),
    FORM_2pirc____ = FTHUMB2 | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(CONST,3),
    FORM_2pirr____ = FTHUMB2 | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_2pirrc___ = FTHUMB2 | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(REG,3) | FSRC(CONST,4),
    FORM_2irc_____ = FTHUMB2 | FDST(INDIR) | FSRC(REG,1) | FSRC(CONST,2),

    //LDMRET
    FORM_Tspispc__ = FTHUMB | FDST(SP) | FSRC(INDIR,1) | FSRC(SP,2) | FSRC(CONST,3),

    //MOVH
    FORM_Tspr_____ = FTHUMB | FDST(SP) | FSRC(REG,1),
    FORM_Thrr_____ = FTHUMB | FSET(REG,28) | FDST(REG) | FSRC(REG,1),
    FORM_2sphr____ = FTHUMB2 | FSET(REG,28) |FDST(SP) | FSRC(REG,1),
    FORM_2hrpc____ = FTHUMB2 | FSET(REG,28) | FDST(REG) | FSRC(PC,1),
    FORM_2rpc_____ = FTHUMB2 | FDST(REG) | FSRC(PC,1),

    //LDM
    FORM_Tric_____ = FTHUMB | FDST(REG) | FSRC(INDIR,1) | FSRC(CONST,1),
    FORM_Ticr_____ = FTHUMB | FDST(INDIR) | FSRC(CONST,1) | FSRC(REG,2),

    // CBZ
    FORM_2lr______ = FTHUMB2 | FSRC(LABEL,1) | FSRC(REG,2),
    FORM_2er______ = FTHUMB2 | FSRC(CODE,1) | FSRC(REG,2),

    //LEA
    FORM_Trl______ = FTHUMB | FDST(REG) | FSRC(LABEL,1),
    FORM_Trlsp____ = FTHUMB | FDST(REG) | FSRC(LABEL,1) | FSRC(SP,2),
    FORM_Tspl_____ = FTHUMB | FDST(SP) | FSRC(LABEL,1),

    //RET pc, r
    FORM_Tpchr____ = FTHUMB | FDST(PC) | FSET(REG,28) | FSRC(REG,1),
    FORM_Tpcr_____ = FTHUMB | FDST(PC) | FSRC(REG,1),
    FORM_Tpcriw___ = FTHUMB | FDST(PC) | FSRC(REG,1) | FSRC(REG,2),
    FORM_Tpchriw__ = FTHUMB | FDST(PC) | FSET(REG,28) | FSRC(REG,1) | FSRC(REG,2),

    //DCD
    FORM_Tlsp_____ = FTHUMB | FSRC(LABEL,1) | FSRC(SP,2),
    FORM_Tc_______ = FTHUMB | FSRC(CONST,1),
    FORM_2r_______ = FTHUMB2 | FDST(REG),
    FORM_2c_______ = FTHUMB2 | FSRC(CONST,1),
    FORM_Tcll_____ = FTHUMB | FSRC(CONST,1) | FSRC(LABEL,2) | FSRC(LABEL,3),
    FORM_Tcee_____ = FTHUMB | FSRC(CONST,1) | FSRC(CODE,2) | FSRC(CODE,3),


    FORM_2rrcrc___ = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(CONST,2) | FSRC(REG,3) | FSRC(CONST,4),
    FORM_2rrrrcrc_ = FTHUMB2 | FDST(REG) | FSRC(REG,1) | FSRC(REG,2) | FSRC(REG,3) | FSRC(CONST,4) | FSRC(REG,5) | FSRC(CONST,6),

    // MRS, MSR
    FORM_2rsrc____ = FTHUMB2 | FDST(REG) | FSRC(SR,1) | FSRC(CONST, 2),
    FORM_2srrcc___ = FTHUMB2 | FDST(SR) | FSRC(REG,1) | FSRC(CONST, 2) | FSRC(CONST, 3),

    // MCRR, MRRC
    FORM_2cprr____ = FTHUMB2 | FSRC(CONST,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_2rcp_____ = FTHUMB2 | FDST(REG) | FSRC(CONST,1),

    // MAR, MRA
    FORM_2acprr___ = FTHUMB2 | FDST(ACC) | FSRC(CONST,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_2rcpa____ = FTHUMB2 | FDST(REG) | FSRC(CONST,1) | FSRC(ACC,2),

    // MCR, MRC
    FORM_2crcccc__ = FTHUMB2 | FSRC(CONST,1) | FSRC(REG,2) | FSRC(CONST, 3) | FSRC(CONST,4) | FSRC(CONST,5) | FSRC(CONST,6),
    FORM_2rccccc__ = FTHUMB2 | FDST(REG) | FSRC(CONST,1) | FSRC(CONST, 2) | FSRC(CONST,3) | FSRC(CONST,4) | FSRC(CONST,5),

    // WLDR
    FORM_2rcr_____ = FTHUMB2 | FDST(REG) | FSRC(CONST,1) | FSRC(REG,2),
    FORM_2rcrr____ = FTHUMB2 | FDST(REG) | FSRC(CONST,1) | FSRC(REG,2) | FSRC(REG,3),
    FORM_2rcrrr___ = FTHUMB2 | FDST(REG) | FSRC(CONST,1) | FSRC(REG,2) | FSRC(REG,3) | FSRC(REG,4),
    FORM_2rcrrrr__ = FTHUMB2 | FDST(REG) | FSRC(CONST,1) | FSRC(REG,2) | FSRC(REG,3) | FSRC(REG,4) | FSRC(REG,5),

    //FLOAT Forms
    FORM_2dd______ = FTHUMB2 | FDST(DREG) | FSRC(DREG,1),
    FORM_2ddd_____ = FTHUMB2 | FDST(DREG) | FSRC(DREG,1) | FSRC(DREG,2),
    FORM_2_dd_____ = FTHUMB2 | FSRC(DREG,1) | FSRC(DREG,2),
    FORM_2dr______ = FTHUMB2 | FDST(DREG) | FSRC(REG,1),
    FORM_2rd______ = FTHUMB2 | FDST(REG)  | FSRC(DREG,1),
    FORM_2dirc____ = FTHUMB2 | FDST(DREG) | FSRC(INDIR,1) | FSRC(REG,2) | FSRC(CONST,3),
    FORM_2disp____ = FTHUMB2 | FDST(DREG) | FSRC(INDIR,1) | FSRC(SP,2),
    FORM_2dispc___ = FTHUMB2 | FDST(DREG) | FSRC(INDIR,1) | FSRC(SP,2) | FSRC(CONST,3),
    FORM_2ircd____ = FTHUMB2 | FDST(INDIR) | FSRC(REG,1) | FSRC(CONST,2) | FSRC(DREG,3),
    FORM_2ispd____ = FTHUMB2 | FDST(INDIR) | FSRC(SP,1) | FSRC(DREG,2),

    FORM_NOMORE     = -1
}IFORM;

#define FT(_form, _inst, _steps)  { FORM_ ## _form, _inst, _steps }

static const FormTable Forms_ADD [] =
{
    FT (2rrc_____, 0x0000F100, Steps_T2_ALU_dn_modc12),
    FT (2rrrcc___, 0x0000EB00, Steps_T2_ALU_dnm_Shift_c),
    FT (2rrrccC__, 0x0000EB40, Steps_T2_ALU_dnm_Shift_c),
    FT (2rrr_____, 0x0000EB00, Steps_T2_ALU_dnm),
    FT (2rrcC____, 0x0000F140, Steps_T2_ALU_dn_modc12),
    FT (2rrrC____, 0x0000EB40, Steps_T2_ALU_dnm),
    FT (2rrC_____, 0x0000EB40, Steps_T2_ALU_ddm),

    // Thumb2 and Thumb1 ADD(4)
    FT (ThrC_____, 0x4140, Steps_T_ALU_dm),         // ADC rd, rd, rm
    FT (TrhrC____, 0x4140, Steps_T_ALU_dm),         // ADC rd, rd, rm
    FT (ThrrC____, 0x4140, Steps_T_ALU_dm),         // ADC rd, rd, rm
    FT (ThrhrC___, 0x4140, Steps_T_ALU_dm),         // ADC rd, rd, rm

    FT (Trrc_____, 0x1c00, Steps_T_Add_Sub_dnc),    // ADD rd, rn #<3_bit_immd>

    FT (Trc______, 0x3000, Steps_T_Add_Sub_ddc),    // ADD rd, #<8_bit_immd>
    FT (Tr_______, 0x1800, Steps_T_Add_Sub_dnm),    // ADD rd, rd, rm
    FT (Trr______, 0x1800, Steps_T_Add_Sub_dnm),    // ADD rd, rd, rm
    FT (Trrr_____, 0x1800, Steps_T_Add_Sub_dnm),    // ADD rd, rn, rm
    FT (Tpcpcr___, 0x4400, Steps_T_Add_High_dm),    // ADD pc, rm, h1,h2
    FT (Tspspr___, 0x4400, Steps_T_Add_High_dm),    // ADD sp, rm, h1,h2
    FT (Tspsphr__, 0x4400, Steps_T_Add_High_dm),    // ADD sp, rm, h1,h2
    FT (Thrr_____, 0x4400, Steps_T_Add_High_dm),    // ADD rd, rm, h1,h2
    FT (Thrsp____, 0x4400, Steps_T_Add_High_dm),    // ADD rd, sp, h1
    FT (Trpc_____, 0x4400, Steps_T_Add_High_dm),    // ADD rd, rm, h1, h2
    FT (Trsp_____, 0x4400, Steps_T_Add_High_dm),    // ADD rd, rm, h1, h2
    FT (Tpchr____, 0x4400, Steps_T_Add_High_dm),    // ADD rd, rm, h1, h2
    FT (Trpcc____, 0xa000, Steps_T_Add_SP_or_PC),   // ADD rd, PC, #<8_bit_immd>
    FT (Trspc____, 0xa800, Steps_T_Add_SP_or_PC),   // ADD rd, SP, #<8_bit_immd>
    FT (Tspspc___, 0xb000, Steps_T_Add_Sub_SP),     // ADD SP, SP, #<7_bit_immd>
    FT (TrC_____,  0x4140, Steps_T_ALU_dm),         // ADC rd, rd, rm
    FT (TrrC_____, 0x4140, Steps_T_ALU_dm),         // ADC rd, rd, rm

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_ADDW [] =
{
    FT (2rrc_____, 0x0000F200, Steps_T2_ALU_dn_imm12),
    FT (2rspc____, 0x0000F200, Steps_T2_ALU_dn_imm12),
    FT (2spspc___, 0x0000F200, Steps_T2_ALU_dn_imm12),
    FT (2sprc____, 0x0000F200, Steps_T2_ALU_dn_imm12),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_ASR [] =
{
    FT (2rrc_____, 0x0020ea4f, Steps_T2_Shift_dmc),
    FT (2rrr_____, 0xF000FA40, Steps_T2_ALU_dnm),

    FT (Trc______, 0x1000, Steps_T_Shift_dmc),
    FT (Trrc_____, 0x1000, Steps_T_Shift_dmc),
    FT (Trr______, 0x4100, Steps_T_Shift_ds),
    FT (Tr_______, 0x4100, Steps_T_Shift_ds),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BX [] =
{
    FT (Tr_______, 0x4700, Steps_T_BX),
    FT (T_r______, 0x4700, Steps_T_BX),
    FT (Thr______, 0x4700, Steps_T_BX),
    FT (NOMORE,   0x0,   0),
};


static const FormTable Forms_B [] =
{
    FT (2l_______, 0x9000F000, Steps_T2_BranchUncond),
//    FT (2l_______, 0x9000F000, Steps_T2_Branch),
//    FT (2lC______, 0x8000F000, Steps_T2_Branch),
//    FT (Tl_______, 0xe000, Steps_T_Branch),
//    FT (TlC______, 0xd000, Steps_T_Branch),
    FT (NOMORE,   0x0,   0),
};

// B<C> explicit ///////////////////////////////////
static const FormTable Forms_BEQ [] =
{
    //FT (2l_______, 0x9000F000, Steps_T2_Branch),
    FT (2l_______, 0x8000F000, Steps_T2_BranchCond),
    //FT (Tl_______, 0xe000, Steps_T_Branch),
    //FT (TlC______, 0xd000, Steps_T_Branch),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BNE [] =
{
    FT (2l_______, 0x8000F040, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BLT [] =
{
    FT (2l_______, 0x8000F2C0, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BGT [] =
{
    FT (2l_______, 0x8000F300, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BLE [] =
{
    FT (2l_______, 0x8000F340, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BGE [] =
{
    FT (2l_______, 0x8000F280, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BCS [] =
{
    FT (2l_______, 0x8000F080, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BCC [] =
{
    FT (2l_______, 0x8000F0C0, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BHI [] =
{
    FT (2l_______, 0x8000F200, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BLS [] =
{
    FT (2l_______, 0x8000F240, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BMI [] =
{
    FT (2l_______, 0x8000F100, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BPL [] =
{
    FT (2l_______, 0x8000F140, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BVS [] =
{
    FT (2l_______, 0x8000F180, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BVC [] =
{
    FT (2l_______, 0x8000F1C0, Steps_T2_BranchCond),
    FT (NOMORE,   0x0,   0),
};
// End B<C> explicit ///////////////////////////////////


static const FormTable Forms_BL [] =
{
    FT (2e_______, 0xF800F000, Steps_T2_BL),

    FT (Tl_______, 0xf800f000, Steps_T_BL),
    FT (Te_______, 0xf800f000, Steps_T_BL),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_BLX [] =
{
    FT (2e_______, 0xE800F000, Steps_T2_BLX),

    FT (Te_______, 0xf400e400, Steps_T_BLX),
    FT (T_r______, 0x4780, Steps_T_BX),
    FT (Tr_______, 0x4780, Steps_T_BX), //same src, dst
    FT (Thr______, 0x4780, Steps_T_BX),
    FT (Th_r_____, 0x4780, Steps_T_BX),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_CMP [] =
{
    FT (2_rc_____, 0x0F00F1B0, Steps_T2_n_modc12),
    FT (2_rr_____, 0x0F00EBB0, Steps_T2_ALU_nm),
    //FT (2rrcc____, 0x0F00EBB0, Steps_T2_ALU_nm_Shift_c),

    FT (T_rc_____, 0x2800, Steps_T_RR_dc),
    FT (T_rr_____, 0x4280, Steps_T_RR_ds),
    FT (T_r______, 0x4280, Steps_T_RR_ds),
    FT (Th_rr____, 0x4500, Steps_T_HRR_ds),
    FT (T_spr____, 0x4500, Steps_T_HRR_ds),
    FT (Th_spr___, 0x4500, Steps_T_HRR_ds),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_CMP_ASR31 [] =
{
    // CMP.W Rn, Rm, ASR #31
    // T3 .im3 1111 i2ty Rm__ 1110 1011 1011 Rn__, ty = 10, im3:i2 = 31
    // -> .111 1111 1110 Rm__ 1110 1011 1011 Rn__
    FT (2_rr_____, 0x7FE0EBB0, Steps_T2_ALU_nm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_CLZ [] =
{
    FT(2rr______, 0xF080FAB0, Steps_T2_ALU_dnn),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_CMN [] =
{
    FT (2_rc_____, 0x0F00F110, Steps_T2_n_modc12),
    FT (2_rr_____, 0x0F00EB10, Steps_T2_ALU_nm),
    //FT (2rrcc____, 0x0F00EB10, Steps_T2_ALU_nm_Shift_c),

    FT (T_rr_____, 0x42c0, Steps_T_RR_ds),
    FT (T_r______, 0x42c0, Steps_T_RR_ds),

    FT (NOMORE,   0x0,   0),
};


static const FormTable Forms_EOR [] =
{
    FT (2rrc_____, 0x0000F080, Steps_T2_ALU_dn_modc12),
    FT (2rrrcc___, 0x0000EA80, Steps_T2_ALU_dnm_Shift_c),
    FT (2rrr_____, 0x0000EA80, Steps_T2_ALU_dnm),
    FT (2rr______, 0x0000EA80, Steps_T2_ALU_dnm),

    FT (Trr______, 0x4040, Steps_T_ALU_dm),
    FT (Tr_______, 0x4040, Steps_T_ALU_dm),

    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_EOR_ASR31 [] =
{
    // EOR.W Rd, Rn, Rm, ASR #31
    // T2 0im3 Rd__ i2ty Rm__ 1110 1010 100S Rn__, ty = 10, im3:i2=31=11111
    // -> 0111 0000 1110 Rm__ 1110 1010 1000 Rn__
    FT (2rrr_____, 0x70E0EA80, Steps_T2_ALU_dnm),
    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_CLRSIGN [] =
{
    FT (2rr______, 0x70E0EA80, Steps_T2_ALU_dnn),
    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_LDM [] =
{
    FT (2ipcc____, 0x0000E810, Steps_T2_LDM),
    FT (2irc_____, 0x0000E810, Steps_T2_LDM),
    FT (2ispc____, 0x0000E810, Steps_T2_LDM),
    FT (2rispc___, 0x0B04F85D, Steps_T2_LDM_ONE),

    FT (Tispc____, 0xbc00, Steps_T_LDM_rspx),
    FT (Tirc_____, 0xc800, Steps_T_LDM_rrx),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_LDIMM [] =
{
    FT (Trc______, 0x2000, Steps_T_ALU_rc),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_LEA [] =
{
    // Treat this as a Thumb2 add: r = ADDW sp,offset
    FT (2risp____, 0x0000F200, Steps_T2_LEA_rrd),

//    FT (Trl______, 0xa800, Steps_T_LEA_rrd),
//    FT (Trlsp____, 0xa800, Steps_T_LEA_rspd),
//    FT (Tspl_____, 0xa800, Steps_T_LEA_rspd),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_STM [] =
{
    FT (2ispc____, 0x0000E800, Steps_T2_STM_rrx),
    FT (2irc_____, 0x0000E800, Steps_T2_STM_rrx),
    FT (2ispcr___, 0x0D04F84D, Steps_T2_STM_rsp_ONE),

    FT (Tispc____, 0xb400, Steps_T_STM_rspx),
    FT (Tirc_____, 0xc000, Steps_T_STM_rrx),
    FT (Tric_____, 0xc000, Steps_T_STM_rrx),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_LDRN [] =
{
    FT (Trirc____, 0x0000, Steps_T_LDRN_rcr),                   //LDR rd, [rn, #5_bit_off]
    FT (Trirr____, 0x0000, Steps_T_LDRN_rrr),                   //LDR rd, [rn, rm]
    FT (Tripcc___, 0x4800, Steps_T_LDRN_PC_or_SP),              //LDR rd, [PC, #8_bit_off]
    FT (Trispc___, 0x9800, Steps_T_LDRN_PC_or_SP),              //LDR rd, [SP, #8_bit_off]
    FT (Trisp____, 0x9800, Steps_T_LDRN_PC_or_SP),              //LDR rd, [rn, #5_bit_off]
    FT (Tripc____, 0x4800, Steps_T_LDRN_PC_or_SP),              //LDR rd, [PC, #8_bit_off]
    FT (Tri______, 0x6838, Steps_T_LDRN_ri),                    //LDR rd, [rn, #5_bit_off]

    FT (NOMORE, 0x0, 0),
};

// Thumb2        LDR.W
static const FormTable Forms_LDR_W [] =
{
    FT (2rirc____, 0x0000F810, Steps_T2_LDR_OFF),
    FT (2risp____, 0x0000F810, Steps_T2_LDR_Stack),
    FT (2ripc____, 0x0100F810, Steps_T2_LDR_Stack),
    FT (2ri______, 0x0000F810, Steps_T2_LDR_Stack),
    FT (2rispc___, 0x0000F810, Steps_T2_LDR_OFF),
    FT (2ripcc___, 0x0000F810, Steps_T2_LDR_OFF),
    FT (2rispr___, 0x0000F810, Steps_T2_LDR_RegIndir),
    FT (2rirr____, 0x0000F810, Steps_T2_LDR_RegIndir),
    FT (2rirrc___, 0x0000F810, Steps_T2_LDR_RegIndir),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_LDRRET [] =
{
    FT (2pcispc__, 0xf000f85d, Steps_T2_LDRRET),                // LDR PC, [SP], #imm
    FT (NOMORE, 0x0, 0),
};

static const FormTable Forms_MOV [] =
{
    FT (Trc______, 0x2000, Steps_T_ALU_rc),
    FT (Trr______, 0x4600, Steps_T_MovHigh_rr),
    FT (Tr_______, 0x4600, Steps_T_MovHigh_rr),

    FT (Thrsp____, 0x4600, Steps_T_MovHigh_rr),
    FT (Tsphr____, 0x4600, Steps_T_MovHigh_rr),
    FT (Th_r_____, 0x4600, Steps_T_MovHigh_rr),

    FT (Trsp_____, 0x4600, Steps_T_MovHigh_rr),
    FT (Tpchr____, 0x4600, Steps_T_MovHigh_rr),
    FT (Thrr_____, 0x4600, Steps_T_MovHigh_rr),
    FT (Tspr_____, 0x4600, Steps_T_MovHigh_rr),
    FT (Tspsp____, 0x4600, Steps_T_MovHigh_rr),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_MOV_W [] =
{
    FT (2rc______, 0x0000F04F, Steps_T2_ALU_r_modc12),
    FT (2rr______, 0x0000EA4F, Steps_T2_ALU_dm),
    FT (NOMORE,   0x0,   0),
};


static const FormTable Forms_MOVW [] =
{
    FT (2rc______, 0x0000F240, Steps_T2_ALU_r_imm16),
    FT (2rl______, 0x0000F240, Steps_T2_MOVW_reloc),

    FT (NOMORE,   0x0,   0),
};

// Thumb2 & ARMv7
static const FormTable Forms_MOVT [] =
{
    //UTC - FT (2rrc_____, 0x0000F2C0, Steps_T2_ALU_r_imm16),
    FT (2rc______, 0x0000F2C0, Steps_T2_ALU_r_imm16),
    FT (2rl______, 0x0000F2C0, Steps_T2_MOVT_reloc),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_RET [] =
{
    FT (Tpcr_____, 0x4600, Steps_T_Ret_rr),
    FT (Tpchr____, 0x4600, Steps_T_Ret_rr),
    FT (Tpcriw___, 0x4700, Steps_T_IWRET_dm),
    FT (Tpchriw__, 0x4700, Steps_T_IWRET_dm),

    FT (NOMORE,   0x0,   0),
};

// initializing it similar to SDIV. won't be used as we split REM into SDIV and MLS
static const FormTable Forms_REM [] =
{
    FT (2rrr_____, 0xF0F0FB90, Steps_T2_ALU_dnm_no_sbit),

    FT (NOMORE,    0x0,   0),
};

// Thumb2        SUBW rd, rn #<12_bit_immediate>
static const FormTable Forms_SUBW [] =
{
    FT (2rrc_____, 0x0000F2A0, Steps_T2_ALU_dn_imm12),
    FT (2rspc____, 0x0000F2A0, Steps_T2_ALU_dn_imm12),
    FT (2spspc___, 0x0000F2A0, Steps_T2_ALU_dn_imm12),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_SUB [] =
{
    FT (2rrc_____, 0x0000F1A0, Steps_T2_ALU_dn_modc12),
    //FT (2rrcc____, 0x0000EBA0, Steps_T2_ALU_ddm_Shift_c), //3rd operand Shifter operand not supported yet
    //FT (2rrrcc___, 0x0000EBA0, Steps_T2_ALU_dnm_Shift_c),
    //FT (2rrrccC__, 0x0000EB60, Steps_T2_ALU_dnm_Shift_c),
    FT (2rrr_____, 0x0000EBA0, Steps_T2_ALU_dnm),
    FT (2rrrC____, 0x0000EB60, Steps_T2_ALU_dnm),       // SBC
    FT (2rrcC____, 0x0000F160, Steps_T2_ALU_dn_modc12), // SBC

    FT (Trrc_____, 0x1e00, Steps_T_Add_Sub_dnc),        // SUB rd, rn #<3_bit_immd>
    FT (Trc______, 0x3800, Steps_T_Add_Sub_ddc),        // SUB rd, #<8_bit_immd>
    FT (Tr_______, 0x1a00, Steps_T_Add_Sub_dnm),        // ADD rd, rd, rm
    FT (Trr______, 0x1a00, Steps_T_Add_Sub_dnm),        // SUB rd, rd, rm
    FT (Trrr_____, 0x1a00, Steps_T_Add_Sub_dnm),        // SUB rd, rn, rm
    FT (Tspspc___, 0xb080, Steps_T_Add_Sub_SP),         // SUB SP, SP, #<7_bit_immd>
    FT (TrrC_____, 0x4180, Steps_T_ALU_dm),             // SBC

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_SBCMPLNT [] =
{
    FT (2rrr_____, 0x70E0EBA0, Steps_T2_ALU_dnm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_STRN[] =
{
    FT (Tircr____, 0x0000, Steps_T_STRN_rcr),           // STR rd, [rn, #off]
    FT (Tirrr____, 0x0000, Steps_T_STRN_rrr),           // STR rd, [rn, rm]
    FT (Tir______, 0x6038, Steps_T_STRN_ri),            // STR rd, [rn, #8_bit_off]
    FT (Tispr____, 0x9000, Steps_T_STRN_spcr),          // STR rd, [SP, #8_bit_off]
    FT (Tispcr___, 0x9000, Steps_T_STRN_spcr),          // STR rd, [SP, #8_bit_off]
    FT (Tipcr____, 0x6000, Steps_T_STRN_spcr),          // STR rd, [PC, #8_bit_off]
    FT (Tirsp____, 0x9000, Steps_T_STRN_spcr),          // STR rd, [SP, #8_bit_off]

    FT (NOMORE, 0x0, 0),
};

static const FormTable Forms_STR_W [] =
{
    FT (2ircr____, 0x0000F800, Steps_T2_STR_OFF),
    FT (2ispcr___, 0x0000F800, Steps_T2_STR_OFF),
    FT (2ispr____, 0x0000F800, Steps_T2_STR_Stack),
    FT (2irrr____, 0x0000F800, Steps_T2_STR_RegIndir),
    FT (2isprr___, 0x0000F800, Steps_T2_STR_RegIndir),
    FT (2irrrc___, 0x0000F800, Steps_T2_STR_RegIndir),
    FT (2ir______, 0x0000F800, Steps_T2_STR_Stack),
    FT (2irsp____, 0x0000F800, Steps_T2_STR_Stack),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_AND [] =
{
    FT (2rrc_____, 0x0000F000, Steps_T2_ALU_dn_modc12),
    FT (2rrr_____, 0x0000EA00, Steps_T2_ALU_dnm),
/*
    FT (2rrrcc___, 0x0000EA00, Steps_T2_ALU_dnm_Shift_c),
    FT (2rr______, 0x0000EA00, Steps_T2_ALU_dnm),
*/
    FT (Trr______, 0x4000, Steps_T_ALU_dm),
    FT (Tr_______, 0x4000, Steps_T_ALU_dm),

    FT (NOMORE,    0x0,   0),
};


static const FormTable Forms_LSL [] =
{
    FT (2rrc_____, 0x0000ea4f, Steps_T2_Shift_dmc),
    FT (2rrr_____, 0xF000FA00, Steps_T2_ALU_dnm),

    FT (Trc______, 0x0000, Steps_T_Shift_dmc),
    FT (Trrc_____, 0x0000, Steps_T_Shift_dmc),
    FT (Trr______, 0x4080, Steps_T_Shift_ds),
    FT (Tr_______, 0x4080, Steps_T_Shift_ds),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_LSR [] =
{
    FT (2rrc_____, 0x0010ea4f, Steps_T2_Shift_dmc),
    FT (2rrr_____, 0xF000FA20, Steps_T2_ALU_dnm),

    FT (Trc______, 0x0800, Steps_T_Shift_dmc),
    FT (Trrc_____, 0x0800, Steps_T_Shift_dmc),
    FT (Trr______, 0x40c0, Steps_T_Shift_ds),
    FT (Tr_______, 0x40c0, Steps_T_Shift_ds),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_MUL [] =
{
    FT (2rrrr____, 0x0000FB00, Steps_T2_MULA_dnsm),
    FT (2rrr_____, 0xF000FB00, Steps_T2_ALU_dnm_no_sbit),
    FT (2rr______, 0xF000FB00, Steps_T2_ALU_dnm_no_sbit),

    FT (Trr______, 0x4340, Steps_T_ALU_dm),

    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_SMLAL [] =
{
    // SMLAL RdLo, RdHi, Rn, Rm. We always use r12 as RdHi, as we don't currently support instructions with 4 operands.
    // RdLo RdHi 0000 Rm__ | 1111 1011 1100 Rn__
    FT (2rrr_____, 0x0000FBC0, Steps_T2_ALU_mull_no_sbit),
    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_SMULL [] =
{
    // SMULL<c> RdLo, RdHi, Rn, Rm. We always use r12 as RdHi, as we don't currently support instructions with 4 operands.
    // RdLo RdHi 0000 Rm__ | 1111 1011 1000 Rn__
    FT (2rrr_____, 0x0000FB80, Steps_T2_ALU_mull_no_sbit),
    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_MLS [] =
{
    FT (2rrr_____, 0x0010FB00, Steps_T2_mls_dnma),

    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_MVN [] =
{
    FT (2rc______, 0x0000F06F, Steps_T2_ALU_r_modc12),
    FT (2rrcc____, 0x0000EA6F, Steps_T2_ALU_dm_Shift_c),
    FT (2rr______, 0x0000EA6F, Steps_T2_ALU_dm),

    FT (Tr_______, 0x43c0, Steps_T_Neg_Mvn_dm),
    FT (Trr______, 0x43c0, Steps_T_Neg_Mvn_dm),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_ORR [] =
{
    FT (2rrc_____, 0x0000F040, Steps_T2_ALU_dn_modc12),
    FT (2rrrcc___, 0x0000EA40, Steps_T2_ALU_dnm_Shift_c),
    FT (2rrr_____, 0x0000EA40, Steps_T2_ALU_dnm),
    FT (2rr______, 0x0000EA40, Steps_T2_ALU_dnm),

    FT (Trr______, 0x4300, Steps_T_ALU_dm),
    FT (Tr_______, 0x4300, Steps_T_ALU_dm),

    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_RSB [] =
{
    FT (2rrc_____, 0x0000F1C0, Steps_T2_ALU_dn_modc12),
    FT (2rrrcc___, 0x0000EBC0, Steps_T2_ALU_dnm_Shift_c),
    FT (2rrr_____, 0x0000EBC0, Steps_T2_ALU_dnm),

    FT (Tr_______, 0x4240, Steps_T_Neg_Mvn_dm),
    FT (Trr______, 0x4240, Steps_T_Neg_Mvn_dm),

    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_SDIV [] =
{
    FT (2rrr_____, 0xF0F0FB90, Steps_T2_ALU_dnm_no_sbit),

    FT (NOMORE,    0x0,   0),
};

static const FormTable Forms_TST [] =
{
    FT (2_rr_____, 0x0F00EA10, Steps_T2_ALU_nm),
    FT (2_rc_____, 0x0F00F010, Steps_T2_n_modc12),
    FT (2rrcc____, 0x0F00EA10, Steps_T2_ALU_nm_Shift_c),

    FT (T_rr_____, 0x4200, Steps_T_RR_ds),
    FT (T_r______, 0x4200, Steps_T_RR_ds),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_TIOFLW [] =
{
    FT (2_r______, 0x0F40EA90, Steps_T2_ALU_nn),

    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_DBGBRK [] =
{
    FT (T________, 0xdefe, Steps_T_Dbg),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_NOP [] =
{
    FT (T________, 0x046c0, Steps_NOP),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_NOP_W [] =
{
    FT (2________, 0x8000F3AF, Steps_NOP),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VABS [] =
{
    FT (2dd______, 0x0bc0eeb0, Steps_DBL_Unary_dm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VSQRT [] =
{
    // VSQRT.F64 <Dd>, <Dm>
    //   s = sz = double precision
    //   D = register selector for dest = sz ? D:Vd : Vd:D
    //   M = register selector for src  = sz ? M:Vm : Vm:M
    // T1/A1 Vd__ 101s 11M0 Vm__ | 1110 1110 1D11 0001 (flipped low and hi 2 bytes from as it's in the manual)
    //    -> 0000 1011 1100 0000 | 1110 1110 1011 0001
    // Note: using 2dd form as for Thumb2, although the manual says there is only T1/A1 form --
    //       the instr is essentially same as in Thumb2 (4 bytes).
    FT (2dd______, 0x0bc0eeb1, Steps_DBL_Unary_dm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VADDF64 [] =
{
    FT (2ddd_____, 0x0b00ee30, Steps_DBL_ALU_dnm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VSUBF64 [] =
{
    FT (2ddd_____, 0x0b40ee30, Steps_DBL_ALU_dnm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VMULF64 [] =
{
    FT (2ddd_____, 0x0b00ee20, Steps_DBL_ALU_dnm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VDIVF64 [] =
{
    FT (2ddd_____, 0x0b00ee80, Steps_DBL_ALU_dnm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VCMPF64 [] =
{
    FT (2_dd_____, 0x0b40eeb4, Steps_DBL_Cmp_dm),
    FT (NOMORE,   0x0,   0),
};

// This always moves FPSCR to ARM Status register.
// APSR_nzcv is encoded as Rt = ’1111’, and the instruction transfers the FPSCR N, Z, C, and
// V flags to the APSR N, Z, C, and V flags.
static const FormTable Forms_VMRS [] =
{
    FT (2________, 0xfa10eef1, Steps_FLT_FMRS_flags),
    FT (NOMORE, 0x0, 0),
};

static const FormTable Forms_VMRSR [] =
{
    FT (2r_______, 0x0a10eef1, Steps_FLT_FMRSR_flags),
    FT (NOMORE, 0x0, 0),
};

static const FormTable Forms_VMSR [] =
{
    FT (2_r______, 0x0a10eee1, Steps_FLT_FMRSR_flags),
    FT (NOMORE, 0x0, 0),
};

static const FormTable Forms_VNEGF64 [] =
{
    FT (2dd______, 0x0b40eeb1, Steps_DBL_Unary_dm),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VLDR [] =
{
    FT (2dirc____, 0x0b00ec10, Steps_A_LDRN_DBL_Am_rcr),        //VLDR rd,[rn,#off]
    FT (2disp____, 0x0b00ec10, Steps_A_LDRN_DBL_Am_rcr),        //VLDR rd,[SP,#off]
    FT (NOMORE, 0x0, 0),
};

static const FormTable Forms_VLDR32 [] =
{
    FT (2dirc____, 0x0a00ec10, Steps_A_LDRN_FLT_Am_rcr),        //FLDS rd,[rn,#off]
    FT (2disp____, 0x0a00ec10, Steps_A_LDRN_FLT_Am_rcr),        //FLDS rd,[SP,#off]
    FT (NOMORE, 0x0, 0),
};

// used by floating point STRs
static const FormTable Forms_VSTR[] =
{
    FT (2ircd____, 0x0b00ec00, Steps_A_DBL_STRN_Am_rcr),        //VSTR dd, [rn, #off]
    FT (2ispd____, 0x0b00ec00, Steps_A_DBL_STRN_Am_rcr),        //VSTR dd, [SP, #off]
    FT (NOMORE, 0x0, 0),
};

// used by floating point STRs
static const FormTable Forms_VSTR32[] =
{
    FT (2ircd____, 0x0a00ec00, Steps_A_FLT_STRN_Am_rcr),        //FSTS sd, [rn, #off]
    FT (2ispd____, 0x0a00ec00, Steps_A_FLT_STRN_Am_rcr),        //FSTS sd, [SP, #off]
    FT (NOMORE, 0x0, 0),
};

static const FormTable Forms_VMOV[] =
{
    FT (2dd______, 0x0b40eeb0, Steps_DBL_Unary_dm),
    FT (NOMORE,   0x0,   0),
};

// This moves data between ARM core registers & VFP registers.
static const FormTable Forms_VMOVARMVFP[] =
{
    FT (2dr______, 0x0a10ee00, Steps_FLT_FMSR_sr),
    FT (2rd______, 0x0a10ee10, Steps_FLT_FMRS_rs),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VCVTF64F32 [] =
{
    FT (2dd______, 0x0ac0eeb7, Steps_FCVTDS_ds),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VCVTF32F64 [] =
{
    FT (2dd______, 0x0bc0eeb7, Steps_FCVTSD_sd),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VCVTF64S32 [] =
{
    FT (2dd______, 0x0bc0eeb8, Steps_FITOD_ds),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VCVTF64U32 [] =
{
    FT (2dd______, 0x0b40eeb8, Steps_FITOD_ds),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VCVTS32F64 [] =
{
    FT (2dd______, 0x0bc0eebd, Steps_FDTOI_sd),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_VCVTRS32F64 [] =
{
    FT (2dd______, 0x0b40eebd, Steps_FDTOI_sd),
    FT (NOMORE,   0x0,   0),
};

static const FormTable Forms_PLD [] =
{
    FT (2_irc____, 0xF000F810, Steps_T2_PLD_offset),
    FT (2_irr____, 0xF000F810, Steps_T2_PLD_RegIndir),
    FT (NOMORE,   0x0,   0),
} ;

// used by floating point LDMs
static const FormTable Forms_VPOP[] =
{
    FT (2irc_____, 0x0b00ec10, Steps_A_DBL_LDM),                //FLDMD rn, {dreg list}
    FT (2ispc____, 0x0b00ec10, Steps_A_DBL_LDM),                //FLDMD SP, {dreg list}
    FT (2rirc____, 0x0b00ec10, Steps_A_DBL_LDM),                //FLDMD rn, {single register}
    FT (2rispc___, 0x0b00ec10, Steps_A_DBL_LDM),                //FLDMD SP, {single register}

    FT (NOMORE, 0x0, 0),
};

// used by floating point STMs
static const FormTable Forms_VPUSH[] =
{
    FT (2irc_____, 0x0b00ec00, Steps_A_DBL_STM),                //FSTMD rn, {dreg list}
    FT (2ispc____, 0x0b00ec00, Steps_A_DBL_STM),                //FSTMD SP, {dreg list}
    FT (2ircr____, 0x0b00ec00, Steps_A_DBL_STM),                //FSTMD rn, {single register}
    FT (2ispcr___, 0x0b00ec00, Steps_A_DBL_STM),                //FSTMD SP, {single register}

    FT (NOMORE, 0x0, 0),
};

static const FormTable Forms_BIC [] =
{
    FT (2rrc_____, 0x0000F020, Steps_T2_ALU_dn_modc12),
    FT (2rrrcc___, 0x0000EA20, Steps_T2_ALU_dnm_Shift_c),
    FT (2rrr_____, 0x0000EA20, Steps_T2_ALU_dnm),
    FT (2rr______, 0x0000EA20, Steps_T2_ALU_dnm),

    FT (Trr______, 0x4380, Steps_T_ALU_dm),
    FT (Tr_______, 0x4380, Steps_T_ALU_dm),

    FT (NOMORE,    0x0,   0),
};
