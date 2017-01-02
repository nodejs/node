// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_S390_CONSTANTS_S390_H_
#define V8_S390_CONSTANTS_S390_H_

// Get the standard printf format macros for C99 stdint types.
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <stdint.h>

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Number of registers
const int kNumRegisters = 16;

// FP support.
const int kNumDoubleRegisters = 16;

const int kNoRegister = -1;

// sign-extend the least significant 16-bits of value <imm>
#define SIGN_EXT_IMM16(imm) ((static_cast<int>(imm) << 16) >> 16)

// sign-extend the least significant 26-bits of value <imm>
#define SIGN_EXT_IMM26(imm) ((static_cast<int>(imm) << 6) >> 6)

// -----------------------------------------------------------------------------
// Conditions.

// Defines constants and accessor classes to assemble, disassemble and
// simulate z/Architecture instructions.
//
// Section references in the code refer to the "z/Architecture Principles
// Of Operation" http://publibfi.boulder.ibm.com/epubs/pdf/dz9zr009.pdf
//

// Constants for specific fields are defined in their respective named enums.
// General constants are in an anonymous enum in class Instr.
enum Condition {
  kNoCondition = -1,
  eq = 0x8,  // Equal.
  ne = 0x7,  // Not equal.
  ge = 0xa,  // Greater or equal.
  lt = 0x4,  // Less than.
  gt = 0x2,  // Greater than.
  le = 0xc,  // Less then or equal
  al = 0xf,  // Always.

  CC_NOP = 0x0,           // S390 NOP
  CC_EQ = 0x08,           // S390 condition code 0b1000
  CC_LT = 0x04,           // S390 condition code 0b0100
  CC_LE = CC_EQ | CC_LT,  // S390 condition code 0b1100
  CC_GT = 0x02,           // S390 condition code 0b0010
  CC_GE = CC_EQ | CC_GT,  // S390 condition code 0b1010
  CC_OF = 0x01,           // S390 condition code 0b0001
  CC_NOF = 0x0E,          // S390 condition code 0b1110
  CC_ALWAYS = 0x0F,       // S390 always taken branch
  unordered = CC_OF,      // Floating-point unordered
  ordered = CC_NOF,       // floating-point ordered
  overflow = CC_OF,       // Summary overflow
  nooverflow = CC_NOF,

  mask0x0 = 0,  // no jumps
  mask0x1 = 1,
  mask0x2 = 2,
  mask0x3 = 3,
  mask0x4 = 4,
  mask0x5 = 5,
  mask0x6 = 6,
  mask0x7 = 7,
  mask0x8 = 8,
  mask0x9 = 9,
  mask0xA = 10,
  mask0xB = 11,
  mask0xC = 12,
  mask0xD = 13,
  mask0xE = 14,
  mask0xF = 15,

  // Rounding modes for floating poing facility
  CURRENT_ROUNDING_MODE = 0,
  ROUND_TO_NEAREST_WITH_TIES_AWAY_FROM_0 = 1,
  ROUND_TO_PREPARE_FOR_SHORTER_PRECISION = 3,
  ROUND_TO_NEAREST_WITH_TIES_TO_EVEN = 4,
  ROUND_TOWARD_0 = 5,
  ROUND_TOWARD_PLUS_INFINITE = 6,
  ROUND_TOWARD_MINUS_INFINITE = 7
};

inline Condition NegateCondition(Condition cond) {
  DCHECK(cond != al);
  switch (cond) {
    case eq:
      return ne;
    case ne:
      return eq;
    case ge:
      return lt;
    case gt:
      return le;
    case le:
      return gt;
    case lt:
      return ge;
    case lt | gt:
      return eq;
    case le | ge:
      return CC_OF;
    case CC_OF:
      return CC_NOF;
    default:
      DCHECK(false);
  }
  return al;
}

// Commute a condition such that {a cond b == b cond' a}.
inline Condition CommuteCondition(Condition cond) {
  switch (cond) {
    case lt:
      return gt;
    case gt:
      return lt;
    case ge:
      return le;
    case le:
      return ge;
    case eq:
      return eq;
    case ne:
      return ne;
    default:
      DCHECK(false);
      return cond;
  }
}

// -----------------------------------------------------------------------------
// Instructions encoding.

// Instr is merely used by the Assembler to distinguish 32bit integers
// representing instructions from usual 32 bit values.
// Instruction objects are pointers to 32bit values, and provide methods to
// access the various ISA fields.
typedef int32_t Instr;
typedef uint16_t TwoByteInstr;
typedef uint32_t FourByteInstr;
typedef uint64_t SixByteInstr;

// Opcodes as defined in Appendix B-2 table
enum Opcode {
  A = 0x5A,           // Add (32)
  ADB = 0xED1A,       // Add (long BFP)
  ADBR = 0xB31A,      // Add (long BFP)
  ADTR = 0xB3D2,      // Add (long DFP)
  ADTRA = 0xB3D2,     // Add (long DFP)
  AEB = 0xED0A,       // Add (short BFP)
  AEBR = 0xB30A,      // Add (short BFP)
  AFI = 0xC29,        // Add Immediate (32)
  AG = 0xE308,        // Add (64)
  AGF = 0xE318,       // Add (64<-32)
  AGFI = 0xC28,       // Add Immediate (64<-32)
  AGFR = 0xB918,      // Add (64<-32)
  AGHI = 0xA7B,       // Add Halfword Immediate (64)
  AGHIK = 0xECD9,     // Add Immediate (64<-16)
  AGR = 0xB908,       // Add (64)
  AGRK = 0xB9E8,      // Add (64)
  AGSI = 0xEB7A,      // Add Immediate (64<-8)
  AH = 0x4A,          // Add Halfword
  AHHHR = 0xB9C8,     // Add High (32)
  AHHLR = 0xB9D8,     // Add High (32)
  AHI = 0xA7A,        // Add Halfword Immediate (32)
  AHIK = 0xECD8,      // Add Immediate (32<-16)
  AHY = 0xE37A,       // Add Halfword
  AIH = 0xCC8,        // Add Immediate High (32)
  AL = 0x5E,          // Add Logical (32)
  ALC = 0xE398,       // Add Logical With Carry (32)
  ALCG = 0xE388,      // Add Logical With Carry (64)
  ALCGR = 0xB988,     // Add Logical With Carry (64)
  ALCR = 0xB998,      // Add Logical With Carry (32)
  ALFI = 0xC2B,       // Add Logical Immediate (32)
  ALG = 0xE30A,       // Add Logical (64)
  ALGF = 0xE31A,      // Add Logical (64<-32)
  ALGFI = 0xC2A,      // Add Logical Immediate (64<-32)
  ALGFR = 0xB91A,     // Add Logical (64<-32)
  ALGHSIK = 0xECDB,   // Add Logical With Signed Immediate (64<-16)
  ALGR = 0xB90A,      // Add Logical (64)
  ALGRK = 0xB9EA,     // Add Logical (64)
  ALGSI = 0xEB7E,     // Add Logical With Signed Immediate (64<-8)
  ALHHHR = 0xB9CA,    // Add Logical High (32)
  ALHHLR = 0xB9DA,    // Add Logical High (32)
  ALHSIK = 0xECDA,    // Add Logical With Signed Immediate (32<-16)
  ALR = 0x1E,         // Add Logical (32)
  ALRK = 0xB9FA,      // Add Logical (32)
  ALSI = 0xEB6E,      // Add Logical With Signed Immediate (32<-8)
  ALSIH = 0xCCA,      // Add Logical With Signed Immediate High (32)
  ALSIHN = 0xCCB,     // Add Logical With Signed Immediate High (32)
  ALY = 0xE35E,       // Add Logical (32)
  AP = 0xFA,          // Add Decimal
  AR = 0x1A,          // Add (32)
  ARK = 0xB9F8,       // Add (32)
  ASI = 0xEB6A,       // Add Immediate (32<-8)
  AXBR = 0xB34A,      // Add (extended BFP)
  AXTR = 0xB3DA,      // Add (extended DFP)
  AXTRA = 0xB3DA,     // Add (extended DFP)
  AY = 0xE35A,        // Add (32)
  BAL = 0x45,         // Branch And Link
  BALR = 0x05,        // Branch And Link
  BAS = 0x4D,         // Branch And Save
  BASR = 0x0D,        // Branch And Save
  BASSM = 0x0C,       // Branch And Save And Set Mode
  BC = 0x47,          // Branch On Condition
  BCR = 0x07,         // Branch On Condition
  BCT = 0x46,         // Branch On Count (32)
  BCTG = 0xE346,      // Branch On Count (64)
  BCTGR = 0xB946,     // Branch On Count (64)
  BCTR = 0x06,        // Branch On Count (32)
  BPP = 0xC7,         // Branch Prediction Preload
  BPRP = 0xC5,        // Branch Prediction Relative Preload
  BRAS = 0xA75,       // Branch Relative And Save
  BRASL = 0xC05,      // Branch Relative And Save Long
  BRC = 0xA74,        // Branch Relative On Condition
  BRCL = 0xC04,       // Branch Relative On Condition Long
  BRCT = 0xA76,       // Branch Relative On Count (32)
  BRCTG = 0xA77,      // Branch Relative On Count (64)
  BRCTH = 0xCC6,      // Branch Relative On Count High (32)
  BRXH = 0x84,        // Branch Relative On Index High (32)
  BRXHG = 0xEC44,     // Branch Relative On Index High (64)
  BRXLE = 0x85,       // Branch Relative On Index Low Or Eq. (32)
  BRXLG = 0xEC45,     // Branch Relative On Index Low Or Eq. (64)
  BSM = 0x0B,         // Branch And Set Mode
  BXH = 0x86,         // Branch On Index High (32)
  BXHG = 0xEB44,      // Branch On Index High (64)
  BXLE = 0x87,        // Branch On Index Low Or Equal (32)
  BXLEG = 0xEB45,     // Branch On Index Low Or Equal (64)
  C = 0x59,           // Compare (32)
  CDB = 0xED19,       // Compare (long BFP)
  CDBR = 0xB319,      // Compare (long BFP)
  CDFBR = 0xB395,     // Convert From Fixed (32 to long BFP)
  CDFBRA = 0xB395,    // Convert From Fixed (32 to long BFP)
  CDFTR = 0xB951,     // Convert From Fixed (32 to long DFP)
  CDGBR = 0xB3A5,     // Convert From Fixed (64 to long BFP)
  CDGBRA = 0xB3A5,    // Convert From Fixed (64 to long BFP)
  CDGTR = 0xB3F1,     // Convert From Fixed (64 to long DFP)
  CDGTRA = 0xB3F1,    // Convert From Fixed (64 to long DFP)
  CDLFBR = 0xB391,    // Convert From Logical (32 to long BFP)
  CDLFTR = 0xB953,    // Convert From Logical (32 to long DFP)
  CDLGBR = 0xB3A1,    // Convert From Logical (64 to long BFP)
  CDLGTR = 0xB952,    // Convert From Logical (64 to long DFP)
  CDS = 0xBB,         // Compare Double And Swap (32)
  CDSG = 0xEB3E,      // Compare Double And Swap (64)
  CDSTR = 0xB3F3,     // Convert From Signed Packed (64 to long DFP)
  CDSY = 0xEB31,      // Compare Double And Swap (32)
  CDTR = 0xB3E4,      // Compare (long DFP)
  CDUTR = 0xB3F2,     // Convert From Unsigned Packed (64 to long DFP)
  CDZT = 0xEDAA,      // Convert From Zoned (to long DFP)
  CEB = 0xED09,       // Compare (short BFP)
  CEBR = 0xB309,      // Compare (short BFP)
  CEDTR = 0xB3F4,     // Compare Biased Exponent (long DFP)
  CEFBR = 0xB394,     // Convert From Fixed (32 to short BFP)
  CEFBRA = 0xB394,    // Convert From Fixed (32 to short BFP)
  CEGBR = 0xB3A4,     // Convert From Fixed (64 to short BFP)
  CEGBRA = 0xB3A4,    // Convert From Fixed (64 to short BFP)
  CELFBR = 0xB390,    // Convert From Logical (32 to short BFP)
  CELGBR = 0xB3A0,    // Convert From Logical (64 to short BFP)
  CEXTR = 0xB3FC,     // Compare Biased Exponent (extended DFP)
  CFC = 0xB21A,       // Compare And Form Codeword
  CFDBR = 0xB399,     // Convert To Fixed (long BFP to 32)
  CFDBRA = 0xB399,    // Convert To Fixed (long BFP to 32)
  CFDR = 0xB3B9,      // Convert To Fixed (long HFP to 32)
  CFDTR = 0xB941,     // Convert To Fixed (long DFP to 32)
  CFEBR = 0xB398,     // Convert To Fixed (short BFP to 32)
  CFEBRA = 0xB398,    // Convert To Fixed (short BFP to 32)
  CFER = 0xB3B8,      // Convert To Fixed (short HFP to 32)
  CFI = 0xC2D,        // Compare Immediate (32)
  CFXBR = 0xB39A,     // Convert To Fixed (extended BFP to 32)
  CFXBRA = 0xB39A,    // Convert To Fixed (extended BFP to 32)
  CFXR = 0xB3BA,      // Convert To Fixed (extended HFP to 32)
  CFXTR = 0xB949,     // Convert To Fixed (extended DFP to 32)
  CG = 0xE320,        // Compare (64)
  CGDBR = 0xB3A9,     // Convert To Fixed (long BFP to 64)
  CGDBRA = 0xB3A9,    // Convert To Fixed (long BFP to 64)
  CGDR = 0xB3C9,      // Convert To Fixed (long HFP to 64)
  CGDTR = 0xB3E1,     // Convert To Fixed (long DFP to 64)
  CGDTRA = 0xB3E1,    // Convert To Fixed (long DFP to 64)
  CGEBR = 0xB3A8,     // Convert To Fixed (short BFP to 64)
  CGEBRA = 0xB3A8,    // Convert To Fixed (short BFP to 64)
  CGER = 0xB3C8,      // Convert To Fixed (short HFP to 64)
  CGF = 0xE330,       // Compare (64<-32)
  CGFI = 0xC2C,       // Compare Immediate (64<-32)
  CGFR = 0xB930,      // Compare (64<-32)
  CGFRL = 0xC6C,      // Compare Relative Long (64<-32)
  CGH = 0xE334,       // Compare Halfword (64<-16)
  CGHI = 0xA7F,       // Compare Halfword Immediate (64<-16)
  CGHRL = 0xC64,      // Compare Halfword Relative Long (64<-16)
  CGHSI = 0xE558,     // Compare Halfword Immediate (64<-16)
  CGIB = 0xECFC,      // Compare Immediate And Branch (64<-8)
  CGIJ = 0xEC7C,      // Compare Immediate And Branch Relative (64<-8)
  CGIT = 0xEC70,      // Compare Immediate And Trap (64<-16)
  CGR = 0xB920,       // Compare (64)
  CGRB = 0xECE4,      // Compare And Branch (64)
  CGRJ = 0xEC64,      // Compare And Branch Relative (64)
  CGRL = 0xC68,       // Compare Relative Long (64)
  CGRT = 0xB960,      // Compare And Trap (64)
  CGXBR = 0xB3AA,     // Convert To Fixed (extended BFP to 64)
  CGXBRA = 0xB3AA,    // Convert To Fixed (extended BFP to 64)
  CGXR = 0xB3CA,      // Convert To Fixed (extended HFP to 64)
  CGXTR = 0xB3E9,     // Convert To Fixed (extended DFP to 64)
  CGXTRA = 0xB3E9,    // Convert To Fixed (extended DFP to 64)
  CH = 0x49,          // Compare Halfword (32<-16)
  CHF = 0xE3CD,       // Compare High (32)
  CHHR = 0xB9CD,      // Compare High (32)
  CHHSI = 0xE554,     // Compare Halfword Immediate (16)
  CHI = 0xA7E,        // Compare Halfword Immediate (32<-16)
  CHLR = 0xB9DD,      // Compare High (32)
  CHRL = 0xC65,       // Compare Halfword Relative Long (32<-16)
  CHSI = 0xE55C,      // Compare Halfword Immediate (32<-16)
  CHY = 0xE379,       // Compare Halfword (32<-16)
  CIB = 0xECFE,       // Compare Immediate And Branch (32<-8)
  CIH = 0xCCD,        // Compare Immediate High (32)
  CIJ = 0xEC7E,       // Compare Immediate And Branch Relative (32<-8)
  CIT = 0xEC72,       // Compare Immediate And Trap (32<-16)
  CKSM = 0xB241,      // Checksum
  CL = 0x55,          // Compare Logical (32)
  CLC = 0xD5,         // Compare Logical (character)
  CLCL = 0x0F,        // Compare Logical Long
  CLCLE = 0xA9,       // Compare Logical Long Extended
  CLCLU = 0xEB8F,     // Compare Logical Long Unicode
  CLFDBR = 0xB39D,    // Convert To Logical (long BFP to 32)
  CLFDTR = 0xB943,    // Convert To Logical (long DFP to 32)
  CLFEBR = 0xB39C,    // Convert To Logical (short BFP to 32)
  CLFHSI = 0xE55D,    // Compare Logical Immediate (32<-16)
  CLFI = 0xC2F,       // Compare Logical Immediate (32)
  CLFIT = 0xEC73,     // Compare Logical Immediate And Trap (32<-16)
  CLFXBR = 0xB39E,    // Convert To Logical (extended BFP to 32)
  CLFXTR = 0xB94B,    // Convert To Logical (extended DFP to 32)
  CLG = 0xE321,       // Compare Logical (64)
  CLGDBR = 0xB3AD,    // Convert To Logical (long BFP to 64)
  CLGDTR = 0xB942,    // Convert To Logical (long DFP to 64)
  CLGEBR = 0xB3AC,    // Convert To Logical (short BFP to 64)
  CLGF = 0xE331,      // Compare Logical (64<-32)
  CLGFI = 0xC2E,      // Compare Logical Immediate (64<-32)
  CLGR = 0xB921,      // Compare Logical (64)
  CLI = 0x95,         // Compare Logical Immediate (8)
  CLIY = 0xEB55,      // Compare Logical Immediate (8)
  CLR = 0x15,         // Compare Logical (32)
  CLY = 0xE355,       // Compare Logical (32)
  CD = 0x69,          // Compare (LH)
  CDR = 0x29,         // Compare (LH)
  CR = 0x19,          // Compare (32)
  CSST = 0xC82,       // Compare And Swap And Store
  CSXTR = 0xB3EB,     // Convert To Signed Packed (extended DFP to 128)
  CSY = 0xEB14,       // Compare And Swap (32)
  CU12 = 0xB2A7,      // Convert Utf-8 To Utf-16
  CU14 = 0xB9B0,      // Convert Utf-8 To Utf-32
  CU21 = 0xB2A6,      // Convert Utf-16 To Utf-8
  CU24 = 0xB9B1,      // Convert Utf-16 To Utf-32
  CU41 = 0xB9B2,      // Convert Utf-32 To Utf-8
  CU42 = 0xB9B3,      // Convert Utf-32 To Utf-16
  CUDTR = 0xB3E2,     // Convert To Unsigned Packed (long DFP to 64)
  CUSE = 0xB257,      // Compare Until Substring Equal
  CUTFU = 0xB2A7,     // Convert Utf-8 To Unicode
  CUUTF = 0xB2A6,     // Convert Unicode To Utf-8
  CUXTR = 0xB3EA,     // Convert To Unsigned Packed (extended DFP to 128)
  CVB = 0x4F,         // Convert To Binary (32)
  CVBG = 0xE30E,      // Convert To Binary (64)
  CVBY = 0xE306,      // Convert To Binary (32)
  CVD = 0x4E,         // Convert To Decimal (32)
  CVDG = 0xE32E,      // Convert To Decimal (64)
  CVDY = 0xE326,      // Convert To Decimal (32)
  CXBR = 0xB349,      // Compare (extended BFP)
  CXFBR = 0xB396,     // Convert From Fixed (32 to extended BFP)
  CXFBRA = 0xB396,    // Convert From Fixed (32 to extended BFP)
  CXFTR = 0xB959,     // Convert From Fixed (32 to extended DFP)
  CXGBR = 0xB3A6,     // Convert From Fixed (64 to extended BFP)
  CXGBRA = 0xB3A6,    // Convert From Fixed (64 to extended BFP)
  CXGTR = 0xB3F9,     // Convert From Fixed (64 to extended DFP)
  CXGTRA = 0xB3F9,    // Convert From Fixed (64 to extended DFP)
  CXLFBR = 0xB392,    // Convert From Logical (32 to extended BFP)
  CXLFTR = 0xB95B,    // Convert From Logical (32 to extended DFP)
  CXLGBR = 0xB3A2,    // Convert From Logical (64 to extended BFP)
  CXLGTR = 0xB95A,    // Convert From Logical (64 to extended DFP)
  CXSTR = 0xB3FB,     // Convert From Signed Packed (128 to extended DFP)
  CXTR = 0xB3EC,      // Compare (extended DFP)
  CXUTR = 0xB3FA,     // Convert From Unsigned Packed (128 to ext. DFP)
  CXZT = 0xEDAB,      // Convert From Zoned (to extended DFP)
  CY = 0xE359,        // Compare (32)
  CZDT = 0xEDA8,      // Convert To Zoned (from long DFP)
  CZXT = 0xEDA9,      // Convert To Zoned (from extended DFP)
  D = 0x5D,           // Divide (32<-64)
  DDB = 0xED1D,       // Divide (long BFP)
  DDBR = 0xB31D,      // Divide (long BFP)
  DDTR = 0xB3D1,      // Divide (long DFP)
  DDTRA = 0xB3D1,     // Divide (long DFP)
  DEB = 0xED0D,       // Divide (short BFP)
  DEBR = 0xB30D,      // Divide (short BFP)
  DIDBR = 0xB35B,     // Divide To Integer (long BFP)
  DIEBR = 0xB353,     // Divide To Integer (short BFP)
  DL = 0xE397,        // Divide Logical (32<-64)
  DLG = 0xE387,       // Divide Logical (64<-128)
  DLGR = 0xB987,      // Divide Logical (64<-128)
  DLR = 0xB997,       // Divide Logical (32<-64)
  DP = 0xFD,          // Divide Decimal
  DR = 0x1D,          // Divide (32<-64)
  DSG = 0xE30D,       // Divide Single (64)
  DSGF = 0xE31D,      // Divide Single (64<-32)
  DSGFR = 0xB91D,     // Divide Single (64<-32)
  DSGR = 0xB90D,      // Divide Single (64)
  DXBR = 0xB34D,      // Divide (extended BFP)
  DXTR = 0xB3D9,      // Divide (extended DFP)
  DXTRA = 0xB3D9,     // Divide (extended DFP)
  EAR = 0xB24F,       // Extract Access
  ECAG = 0xEB4C,      // Extract Cache Attribute
  ECTG = 0xC81,       // Extract Cpu Time
  ED = 0xDE,          // Edit
  EDMK = 0xDF,        // Edit And Mark
  EEDTR = 0xB3E5,     // Extract Biased Exponent (long DFP to 64)
  EEXTR = 0xB3ED,     // Extract Biased Exponent (extended DFP to 64)
  EFPC = 0xB38C,      // Extract Fpc
  EPSW = 0xB98D,      // Extract Psw
  ESDTR = 0xB3E7,     // Extract Significance (long DFP)
  ESXTR = 0xB3EF,     // Extract Significance (extended DFP)
  ETND = 0xB2EC,      // Extract Transaction Nesting Depth
  EX = 0x44,          // Execute
  EXRL = 0xC60,       // Execute Relative Long
  FIDBR = 0xB35F,     // Load Fp Integer (long BFP)
  FIDBRA = 0xB35F,    // Load Fp Integer (long BFP)
  FIDTR = 0xB3D7,     // Load Fp Integer (long DFP)
  FIEBR = 0xB357,     // Load Fp Integer (short BFP)
  FIEBRA = 0xB357,    // Load Fp Integer (short BFP)
  FIXBR = 0xB347,     // Load Fp Integer (extended BFP)
  FIXBRA = 0xB347,    // Load Fp Integer (extended BFP)
  FIXTR = 0xB3DF,     // Load Fp Integer (extended DFP)
  FLOGR = 0xB983,     // Find Leftmost One
  HSCH = 0xB231,      // Halt Subchannel
  IC_z = 0x43,        // Insert Character
  ICM = 0xBF,         // Insert Characters Under Mask (low)
  ICMH = 0xEB80,      // Insert Characters Under Mask (high)
  ICMY = 0xEB81,      // Insert Characters Under Mask (low)
  ICY = 0xE373,       // Insert Character
  IEDTR = 0xB3F6,     // Insert Biased Exponent (64 to long DFP)
  IEXTR = 0xB3FE,     // Insert Biased Exponent (64 to extended DFP)
  IIHF = 0xC08,       // Insert Immediate (high)
  IIHH = 0xA50,       // Insert Immediate (high high)
  IIHL = 0xA51,       // Insert Immediate (high low)
  IILF = 0xC09,       // Insert Immediate (low)
  IILH = 0xA52,       // Insert Immediate (low high)
  IILL = 0xA53,       // Insert Immediate (low low)
  IPM = 0xB222,       // Insert Program Mask
  KDB = 0xED18,       // Compare And Signal (long BFP)
  KDBR = 0xB318,      // Compare And Signal (long BFP)
  KDTR = 0xB3E0,      // Compare And Signal (long DFP)
  KEB = 0xED08,       // Compare And Signal (short BFP)
  KEBR = 0xB308,      // Compare And Signal (short BFP)
  KIMD = 0xB93E,      // Compute Intermediate Message Digest
  KLMD = 0xB93F,      // Compute Last Message Digest
  KM = 0xB92E,        // Cipher Message
  KMAC = 0xB91E,      // Compute Message Authentication Code
  KMC = 0xB92F,       // Cipher Message With Chaining
  KMCTR = 0xB92D,     // Cipher Message With Counter
  KMF = 0xB92A,       // Cipher Message With Cfb
  KMO = 0xB92B,       // Cipher Message With Ofb
  KXBR = 0xB348,      // Compare And Signal (extended BFP)
  KXTR = 0xB3E8,      // Compare And Signal (extended DFP)
  L = 0x58,           // Load (32)
  LA = 0x41,          // Load Address
  LAA = 0xEBF8,       // Load And Add (32)
  LAAG = 0xEBE8,      // Load And Add (64)
  LAAL = 0xEBFA,      // Load And Add Logical (32)
  LAALG = 0xEBEA,     // Load And Add Logical (64)
  LAE = 0x51,         // Load Address Extended
  LAEY = 0xE375,      // Load Address Extended
  LAN = 0xEBF4,       // Load And And (32)
  LANG = 0xEBE4,      // Load And And (64)
  LAO = 0xEBF6,       // Load And Or (32)
  LAOG = 0xEBE6,      // Load And Or (64)
  LARL = 0xC00,       // Load Address Relative Long
  LAT = 0xE39F,       // Load And Trap (32L<-32)
  LAX = 0xEBF7,       // Load And Exclusive Or (32)
  LAXG = 0xEBE7,      // Load And Exclusive Or (64)
  LAY = 0xE371,       // Load Address
  LB = 0xE376,        // Load Byte (32)
  LBH = 0xE3C0,       // Load Byte High (32<-8)
  LBR = 0xB926,       // Load Byte (32)
  LCDBR = 0xB313,     // Load Complement (long BFP)
  LCDFR = 0xB373,     // Load Complement (long)
  LCEBR = 0xB303,     // Load Complement (short BFP)
  LCGFR = 0xB913,     // Load Complement (64<-32)
  LCGR = 0xB903,      // Load Complement (64)
  LCR = 0x13,         // Load Complement (32)
  LCXBR = 0xB343,     // Load Complement (extended BFP)
  LD = 0x68,          // Load (long)
  LDEB = 0xED04,      // Load Lengthened (short to long BFP)
  LDEBR = 0xB304,     // Load Lengthened (short to long BFP)
  LDETR = 0xB3D4,     // Load Lengthened (short to long DFP)
  LDGR = 0xB3C1,      // Load Fpr From Gr (64 to long)
  LDR = 0x28,         // Load (long)
  LDXBR = 0xB345,     // Load Rounded (extended to long BFP)
  LDXBRA = 0xB345,    // Load Rounded (extended to long BFP)
  LDXTR = 0xB3DD,     // Load Rounded (extended to long DFP)
  LDY = 0xED65,       // Load (long)
  LE = 0x78,          // Load (short)
  LEDBR = 0xB344,     // Load Rounded (long to short BFP)
  LEDBRA = 0xB344,    // Load Rounded (long to short BFP)
  LEDTR = 0xB3D5,     // Load Rounded (long to short DFP)
  LER = 0x38,         // Load (short)
  LEXBR = 0xB346,     // Load Rounded (extended to short BFP)
  LEXBRA = 0xB346,    // Load Rounded (extended to short BFP)
  LEY = 0xED64,       // Load (short)
  LFAS = 0xB2BD,      // Load Fpc And Signal
  LFH = 0xE3CA,       // Load High (32)
  LFHAT = 0xE3C8,     // Load High And Trap (32H<-32)
  LFPC = 0xB29D,      // Load Fpc
  LG = 0xE304,        // Load (64)
  LGAT = 0xE385,      // Load And Trap (64)
  LGB = 0xE377,       // Load Byte (64)
  LGBR = 0xB906,      // Load Byte (64)
  LGDR = 0xB3CD,      // Load Gr From Fpr (long to 64)
  LGF = 0xE314,       // Load (64<-32)
  LGFI = 0xC01,       // Load Immediate (64<-32)
  LGFR = 0xB914,      // Load (64<-32)
  LGFRL = 0xC4C,      // Load Relative Long (64<-32)
  LGH = 0xE315,       // Load Halfword (64)
  LGHI = 0xA79,       // Load Halfword Immediate (64)
  LGHR = 0xB907,      // Load Halfword (64)
  LGHRL = 0xC44,      // Load Halfword Relative Long (64<-16)
  LGR = 0xB904,       // Load (64)
  LGRL = 0xC48,       // Load Relative Long (64)
  LH = 0x48,          // Load Halfword (32)
  LHH = 0xE3C4,       // Load Halfword High (32<-16)
  LHI = 0xA78,        // Load Halfword Immediate (32)
  LHR = 0xB927,       // Load Halfword (32)
  LHRL = 0xC45,       // Load Halfword Relative Long (32<-16)
  LHY = 0xE378,       // Load Halfword (32)
  LLC = 0xE394,       // Load Logical Character (32)
  LLCH = 0xE3C2,      // Load Logical Character High (32<-8)
  LLCR = 0xB994,      // Load Logical Character (32)
  LLGC = 0xE390,      // Load Logical Character (64)
  LLGCR = 0xB984,     // Load Logical Character (64)
  LLGF = 0xE316,      // Load Logical (64<-32)
  LLGFAT = 0xE39D,    // Load Logical And Trap (64<-32)
  LLGFR = 0xB916,     // Load Logical (64<-32)
  LLGFRL = 0xC4E,     // Load Logical Relative Long (64<-32)
  LLGH = 0xE391,      // Load Logical Halfword (64)
  LLGHR = 0xB985,     // Load Logical Halfword (64)
  LLGHRL = 0xC46,     // Load Logical Halfword Relative Long (64<-16)
  LLGT = 0xE317,      // Load Logical Thirty One Bits
  LLGTAT = 0xE39C,    // Load Logical Thirty One Bits And Trap (64<-31)
  LLGTR = 0xB917,     // Load Logical Thirty One Bits
  LLH = 0xE395,       // Load Logical Halfword (32)
  LLHH = 0xE3C6,      // Load Logical Halfword High (32<-16)
  LLHR = 0xB995,      // Load Logical Halfword (32)
  LLHRL = 0xC42,      // Load Logical Halfword Relative Long (32<-16)
  LLIHF = 0xC0E,      // Load Logical Immediate (high)
  LLIHH = 0xA5C,      // Load Logical Immediate (high high)
  LLIHL = 0xA5D,      // Load Logical Immediate (high low)
  LLILF = 0xC0F,      // Load Logical Immediate (low)
  LLILH = 0xA5E,      // Load Logical Immediate (low high)
  LLILL = 0xA5F,      // Load Logical Immediate (low low)
  LM = 0x98,          // Load Multiple (32)
  LMD = 0xEF,         // Load Multiple Disjoint
  LMG = 0xEB04,       // Load Multiple (64)
  LMH = 0xEB96,       // Load Multiple High
  LMY = 0xEB98,       // Load Multiple (32)
  LNDBR = 0xB311,     // Load Negative (long BFP)
  LNDFR = 0xB371,     // Load Negative (long)
  LNEBR = 0xB301,     // Load Negative (short BFP)
  LNGFR = 0xB911,     // Load Negative (64<-32)
  LNGR = 0xB901,      // Load Negative (64)
  LNR = 0x11,         // Load Negative (32)
  LNXBR = 0xB341,     // Load Negative (extended BFP)
  LOC = 0xEBF2,       // Load On Condition (32)
  LOCG = 0xEBE2,      // Load On Condition (64)
  LOCGR = 0xB9E2,     // Load On Condition (64)
  LOCR = 0xB9F2,      // Load On Condition (32)
  LPD = 0xC84,        // Load Pair Disjoint (32)
  LPDBR = 0xB310,     // Load Positive (long BFP)
  LPDFR = 0xB370,     // Load Positive (long)
  LPDG = 0xC85,       // Load Pair Disjoint (64)
  LPEBR = 0xB300,     // Load Positive (short BFP)
  LPGFR = 0xB910,     // Load Positive (64<-32)
  LPGR = 0xB900,      // Load Positive (64)
  LPQ = 0xE38F,       // Load Pair From Quadword
  LPR = 0x10,         // Load Positive (32)
  LPXBR = 0xB340,     // Load Positive (extended BFP)
  LR = 0x18,          // Load (32)
  LRL = 0xC4D,        // Load Relative Long (32)
  LRV = 0xE31E,       // Load Reversed (32)
  LRVG = 0xE30F,      // Load Reversed (64)
  LRVGR = 0xB90F,     // Load Reversed (64)
  LRVH = 0xE31F,      // Load Reversed (16)
  LRVR = 0xB91F,      // Load Reversed (32)
  LT = 0xE312,        // Load And Test (32)
  LTDBR = 0xB312,     // Load And Test (long BFP)
  LTDTR = 0xB3D6,     // Load And Test (long DFP)
  LTEBR = 0xB302,     // Load And Test (short BFP)
  LTG = 0xE302,       // Load And Test (64)
  LTGF = 0xE332,      // Load And Test (64<-32)
  LTGFR = 0xB912,     // Load And Test (64<-32)
  LTGR = 0xB902,      // Load And Test (64)
  LTR = 0x12,         // Load And Test (32)
  LTXBR = 0xB342,     // Load And Test (extended BFP)
  LTXTR = 0xB3DE,     // Load And Test (extended DFP)
  LXDB = 0xED05,      // Load Lengthened (long to extended BFP)
  LXDBR = 0xB305,     // Load Lengthened (long to extended BFP)
  LXDTR = 0xB3DC,     // Load Lengthened (long to extended DFP)
  LXEB = 0xED06,      // Load Lengthened (short to extended BFP)
  LXEBR = 0xB306,     // Load Lengthened (short to extended BFP)
  LXR = 0xB365,       // Load (extended)
  LY = 0xE358,        // Load (32)
  LZDR = 0xB375,      // Load Zero (long)
  LZER = 0xB374,      // Load Zero (short)
  LZXR = 0xB376,      // Load Zero (extended)
  M = 0x5C,           // Multiply (64<-32)
  MADB = 0xED1E,      // Multiply And Add (long BFP)
  MADBR = 0xB31E,     // Multiply And Add (long BFP)
  MAEB = 0xED0E,      // Multiply And Add (short BFP)
  MAEBR = 0xB30E,     // Multiply And Add (short BFP)
  MC = 0xAF,          // Monitor Call
  MDB = 0xED1C,       // Multiply (long BFP)
  MDBR = 0xB31C,      // Multiply (long BFP)
  MDEB = 0xED0C,      // Multiply (short to long BFP)
  MDEBR = 0xB30C,     // Multiply (short to long BFP)
  MDTR = 0xB3D0,      // Multiply (long DFP)
  MDTRA = 0xB3D0,     // Multiply (long DFP)
  MEEB = 0xED17,      // Multiply (short BFP)
  MEEBR = 0xB317,     // Multiply (short BFP)
  MFY = 0xE35C,       // Multiply (64<-32)
  MGHI = 0xA7D,       // Multiply Halfword Immediate (64)
  MH = 0x4C,          // Multiply Halfword (32)
  MHI = 0xA7C,        // Multiply Halfword Immediate (32)
  MHY = 0xE37C,       // Multiply Halfword (32)
  ML = 0xE396,        // Multiply Logical (64<-32)
  MLG = 0xE386,       // Multiply Logical (128<-64)
  MLGR = 0xB986,      // Multiply Logical (128<-64)
  MLR = 0xB996,       // Multiply Logical (64<-32)
  MP = 0xFC,          // Multiply Decimal
  MR = 0x1C,          // Multiply (64<-32)
  MS = 0x71,          // Multiply Single (32)
  MSCH = 0xB232,      // Modify Subchannel
  MSDB = 0xED1F,      // Multiply And Subtract (long BFP)
  MSDBR = 0xB31F,     // Multiply And Subtract (long BFP)
  MSEB = 0xED0F,      // Multiply And Subtract (short BFP)
  MSEBR = 0xB30F,     // Multiply And Subtract (short BFP)
  MSFI = 0xC21,       // Multiply Single Immediate (32)
  MSG = 0xE30C,       // Multiply Single (64)
  MSGF = 0xE31C,      // Multiply Single (64<-32)
  MSGFI = 0xC20,      // Multiply Single Immediate (64<-32)
  MSGFR = 0xB91C,     // Multiply Single (64<-32)
  MSGR = 0xB90C,      // Multiply Single (64)
  MSR = 0xB252,       // Multiply Single (32)
  MSY = 0xE351,       // Multiply Single (32)
  MVC = 0xD2,         // Move (character)
  MVCP = 0xDA,        // Move To Primary
  MVCDK = 0xE50F,     // Move To Primary
  MVCIN = 0xE8,       // Move Inverse
  MVCL = 0x0E,        // Move Long
  MVCLE = 0xA8,       // Move Long Extended
  MVCLU = 0xEB8E,     // Move Long Unicode
  MVGHI = 0xE548,     // Move (64<-16)
  MVHHI = 0xE544,     // Move (16<-16)
  MVHI = 0xE54C,      // Move (32<-16)
  MVI = 0x92,         // Move (immediate)
  MVIY = 0xEB52,      // Move (immediate)
  MVN = 0xD1,         // Move Numerics
  MVO = 0xF1,         // Move With Offset
  MVST = 0xB255,      // Move String
  MVZ = 0xD3,         // Move Zones
  MXBR = 0xB34C,      // Multiply (extended BFP)
  MXDB = 0xED07,      // Multiply (long to extended BFP)
  MXDBR = 0xB307,     // Multiply (long to extended BFP)
  MXTR = 0xB3D8,      // Multiply (extended DFP)
  MXTRA = 0xB3D8,     // Multiply (extended DFP)
  N = 0x54,           // And (32)
  NC = 0xD4,          // And (character)
  NG = 0xE380,        // And (64)
  NGR = 0xB980,       // And (64)
  NGRK = 0xB9E4,      // And (64)
  NI = 0x94,          // And (immediate)
  NIAI = 0xB2FA,      // Next Instruction Access Intent Ie Eh
  NIHF = 0xC0A,       // And Immediate (high)
  NIHH = 0xA54,       // And Immediate (high high)
  NIHL = 0xA55,       // And Immediate (high low)
  NILF = 0xC0B,       // And Immediate (low)
  NILH = 0xA56,       // And Immediate (low high)
  NILL = 0xA57,       // And Immediate (low low)
  NIY = 0xEB54,       // And (immediate)
  NR = 0x14,          // And (32)
  NRK = 0xB9F4,       // And (32)
  NTSTG = 0xE325,     // Nontransactional Store Rxy Tx ¤9 A Sp St B2
  NY = 0xE354,        // And (32)
  O = 0x56,           // Or (32)
  OC = 0xD6,          // Or (character)
  OG = 0xE381,        // Or (64)
  OGR = 0xB981,       // Or (64)
  OGRK = 0xB9E6,      // Or (64)
  OI = 0x96,          // Or (immediate)
  OIHF = 0xC0C,       // Or Immediate (high)
  OIHH = 0xA58,       // Or Immediate (high high)
  OIHL = 0xA59,       // Or Immediate (high low)
  OILF = 0xC0D,       // Or Immediate (low)
  OILH = 0xA5A,       // Or Immediate (low high)
  OILL = 0xA5B,       // Or Immediate (low low)
  OIY = 0xEB56,       // Or (immediate)
  OR = 0x16,          // Or (32)
  ORK = 0xB9F6,       // Or (32)
  OY = 0xE356,        // Or (32)
  PACK = 0xF2,        // Pack
  PCC = 0xB92C,       // Perform Cryptographic Computation
  PFD = 0xE336,       // Prefetch Data
  PFDRL = 0xC62,      // Prefetch Data Relative Long
  PFPO = 0x010A,      // Perform Floating-POINT Operation
  PKA = 0xE9,         // Pack Ascii
  PKU = 0xE1,         // Pack Unicode
  PLO = 0xEE,         // Perform Locked Operation
  POPCNT_Z = 0xB9E1,  // Population Count
  PPA = 0xB2E8,       // Perform Processor Assist
  QADTR = 0xB3F5,     // Quantize (long DFP)
  QAXTR = 0xB3FD,     // Quantize (extended DFP)
  RCHP = 0xB23B,      // Reset Channel Path
  RISBG = 0xEC55,     // Rotate Then Insert Selected Bits
  RISBGN = 0xEC59,    // Rotate Then Insert Selected Bits
  RISBHG = 0xEC5D,    // Rotate Then Insert Selected Bits High
  RISBLG = 0xEC51,    // Rotate Then Insert Selected Bits Low
  RLL = 0xEB1D,       // Rotate Left Single Logical (32)
  RLLG = 0xEB1C,      // Rotate Left Single Logical (64)
  RNSBG = 0xEC54,     // Rotate Then And Selected Bits
  ROSBG = 0xEC56,     // Rotate Then Or Selected Bits
  RRDTR = 0xB3F7,     // Reround (long DFP)
  RRXTR = 0xB3FF,     // Reround (extended DFP)
  RSCH = 0xB238,      // Resume Subchannel
  RXSBG = 0xEC57,     // Rotate Then Exclusive Or Selected Bits
  S = 0x5B,           // Subtract (32)
  SAL = 0xB237,       // Set Address Limit
  SAR = 0xB24E,       // Set Access
  SCHM = 0xB23C,      // Set Channel Monitor
  SDB = 0xED1B,       // Subtract (long BFP)
  SDBR = 0xB31B,      // Subtract (long BFP)
  SDTR = 0xB3D3,      // Subtract (long DFP)
  SDTRA = 0xB3D3,     // Subtract (long DFP)
  SEB = 0xED0B,       // Subtract (short BFP)
  SEBR = 0xB30B,      // Subtract (short BFP)
  SFASR = 0xB385,     // Set Fpc And Signal
  SFPC = 0xB384,      // Set Fpc
  SG = 0xE309,        // Subtract (64)
  SGF = 0xE319,       // Subtract (64<-32)
  SGFR = 0xB919,      // Subtract (64<-32)
  SGR = 0xB909,       // Subtract (64)
  SGRK = 0xB9E9,      // Subtract (64)
  SH = 0x4B,          // Subtract Halfword
  SHHHR = 0xB9C9,     // Subtract High (32)
  SHHLR = 0xB9D9,     // Subtract High (32)
  SHY = 0xE37B,       // Subtract Halfword
  SL = 0x5F,          // Subtract Logical (32)
  SLA = 0x8B,         // Shift Left Single (32)
  SLAG = 0xEB0B,      // Shift Left Single (64)
  SLAK = 0xEBDD,      // Shift Left Single (32)
  SLB = 0xE399,       // Subtract Logical With Borrow (32)
  SLBG = 0xE389,      // Subtract Logical With Borrow (64)
  SLBGR = 0xB989,     // Subtract Logical With Borrow (64)
  SLBR = 0xB999,      // Subtract Logical With Borrow (32)
  SLDA = 0x8F,        // Shift Left Double
  SLDL = 0x8D,        // Shift Left Double Logical
  SLDT = 0xED40,      // Shift Significand Left (long DFP)
  SLFI = 0xC25,       // Subtract Logical Immediate (32)
  SLG = 0xE30B,       // Subtract Logical (64)
  SLGF = 0xE31B,      // Subtract Logical (64<-32)
  SLGFI = 0xC24,      // Subtract Logical Immediate (64<-32)
  SLGFR = 0xB91B,     // Subtract Logical (64<-32)
  SLGR = 0xB90B,      // Subtract Logical (64)
  SLGRK = 0xB9EB,     // Subtract Logical (64)
  SLHHHR = 0xB9CB,    // Subtract Logical High (32)
  SLHHLR = 0xB9DB,    // Subtract Logical High (32)
  SLL = 0x89,         // Shift Left Single Logical (32)
  SLLG = 0xEB0D,      // Shift Left Single Logical (64)
  SLLK = 0xEBDF,      // Shift Left Single Logical (32)
  SLR = 0x1F,         // Subtract Logical (32)
  SLRK = 0xB9FB,      // Subtract Logical (32)
  SLXT = 0xED48,      // Shift Significand Left (extended DFP)
  SLY = 0xE35F,       // Subtract Logical (32)
  SP = 0xFB,          // Subtract Decimal
  SPM = 0x04,         // Set Program Mask
  SQDB = 0xED15,      // Square Root (long BFP)
  SQDBR = 0xB315,     // Square Root (long BFP)
  SQEB = 0xED14,      // Square Root (short BFP)
  SQEBR = 0xB314,     // Square Root (short BFP)
  SQXBR = 0xB316,     // Square Root (extended BFP)
  SR = 0x1B,          // Subtract (32)
  SRA = 0x8A,         // Shift Right Single (32)
  SRAG = 0xEB0A,      // Shift Right Single (64)
  SRAK = 0xEBDC,      // Shift Right Single (32)
  SRDA = 0x8E,        // Shift Right Double
  SRDL = 0x8C,        // Shift Right Double Logical
  SRDT = 0xED41,      // Shift Significand Right (long DFP)
  SRK = 0xB9F9,       // Subtract (32)
  SRL = 0x88,         // Shift Right Single Logical (32)
  SRLG = 0xEB0C,      // Shift Right Single Logical (64)
  SRLK = 0xEBDE,      // Shift Right Single Logical (32)
  SRNM = 0xB299,      // Set BFP Rounding Mode (2 bit)
  SRNMB = 0xB2B8,     // Set BFP Rounding Mode (3 bit)
  SRNMT = 0xB2B9,     // Set DFP Rounding Mode
  SRP = 0xF0,         // Shift And Round Decimal
  SRST = 0xB25E,      // Search String
  SRSTU = 0xB9BE,     // Search String Unicode
  SRXT = 0xED49,      // Shift Significand Right (extended DFP)
  SSCH = 0xB233,      // Start Subchannel
  ST = 0x50,          // Store (32)
  STC = 0x42,         // Store Character
  STCH = 0xE3C3,      // Store Character High (8)
  STCK = 0xB205,      // Store Clock
  STCKE = 0xB278,     // Store Clock Extended
  STCKF = 0xB27C,     // Store Clock Fast
  STCM = 0xBE,        // Store Characters Under Mask (low)
  STCMH = 0xEB2C,     // Store Characters Under Mask (high)
  STCMY = 0xEB2D,     // Store Characters Under Mask (low)
  STCPS = 0xB23A,     // Store Channel Path Status
  STCRW = 0xB239,     // Store Channel Report Word
  STCY = 0xE372,      // Store Character
  STD = 0x60,         // Store (long)
  STDY = 0xED67,      // Store (long)
  STE = 0x70,         // Store (short)
  STEY = 0xED66,      // Store (short)
  STFH = 0xE3CB,      // Store High (32)
  STFLE = 0xB2B0,     // Store Facility List Extended
  STFPC = 0xB29C,     // Store Fpc
  STG = 0xE324,       // Store (64)
  STGRL = 0xC4B,      // Store Relative Long (64)
  STH = 0x40,         // Store Halfword
  STHH = 0xE3C7,      // Store Halfword High (16)
  STHRL = 0xC47,      // Store Halfword Relative Long
  STHY = 0xE370,      // Store Halfword
  STM = 0x90,         // Store Multiple (32)
  STMG = 0xEB24,      // Store Multiple (64)
  STMH = 0xEB26,      // Store Multiple High
  STMY = 0xEB90,      // Store Multiple (32)
  STOC = 0xEBF3,      // Store On Condition (32)
  STOCG = 0xEBE3,     // Store On Condition (64)
  STPQ = 0xE38E,      // Store Pair To Quadword
  STRL = 0xC4F,       // Store Relative Long (32)
  STRV = 0xE33E,      // Store Reversed (32)
  STRVG = 0xE32F,     // Store Reversed (64)
  STRVH = 0xE33F,     // Store Reversed (16)
  STSCH = 0xB234,     // Store Subchannel
  STY = 0xE350,       // Store (32)
  SVC = 0x0A,         // Supervisor Call
  SXBR = 0xB34B,      // Subtract (extended BFP)
  SXTR = 0xB3DB,      // Subtract (extended DFP)
  SXTRA = 0xB3DB,     // Subtract (extended DFP)
  SY = 0xE35B,        // Subtract (32)
  TABORT = 0xB2FC,    // Transaction Abort
  TBDR = 0xB351,      // Convert HFP To BFP (long)
  TBEDR = 0xB350,     // Convert HFP To BFP (long to short)
  TBEGIN = 0xE560,    // Transaction Begin
  TBEGINC = 0xE561,   // Transaction Begin
  TCDB = 0xED11,      // Test Data Class (long BFP)
  TCEB = 0xED10,      // Test Data Class (short BFP)
  TCXB = 0xED12,      // Test Data Class (extended BFP)
  TDCDT = 0xED54,     // Test Data Class (long DFP)
  TDCET = 0xED50,     // Test Data Class (short DFP)
  TDCXT = 0xED58,     // Test Data Class (extended DFP)
  TDGDT = 0xED55,     // Test Data Group (long DFP)
  TDGET = 0xED51,     // Test Data Group (short DFP)
  TDGXT = 0xED59,     // Test Data Group (extended DFP)
  TEND = 0xB2F8,      // Transaction End
  THDER = 0xB358,     // Convert BFP To HFP (short to long)
  THDR = 0xB359,      // Convert BFP To HFP (long)
  TM = 0x91,          // Test Under Mask Si C A B1
  TMH = 0xA70,        // Test Under Mask High
  TMHH = 0xA72,       // Test Under Mask (high high)
  TMHL = 0xA73,       // Test Under Mask (high low)
  TML = 0xA71,        // Test Under Mask Low
  TMLH = 0xA70,       // Test Under Mask (low high)
  TMLL = 0xA71,       // Test Under Mask (low low)
  TMY = 0xEB51,       // Test Under Mask
  TP = 0xEBC0,        // Test Decimal
  TPI = 0xB236,       // Test Pending Interruption
  TR = 0xDC,          // Translate
  TRAP4 = 0xB2FF,     // Trap (4)
  TRE = 0xB2A5,       // Translate Extended
  TROO = 0xB993,      // Translate One To One
  TROT = 0xB992,      // Translate One To Two
  TRT = 0xDD,         // Translate And Test
  TRTE = 0xB9BF,      // Translate And Test Extended
  TRTO = 0xB991,      // Translate Two To One
  TRTR = 0xD0,        // Translate And Test Reverse
  TRTRE = 0xB9BD,     // Translate And Test Reverse Extended
  TRTT = 0xB990,      // Translate Two To Two
  TS = 0x93,          // Test And Set
  TSCH = 0xB235,      // Test Subchannel
  UNPK = 0xF3,        // Unpack
  UNPKA = 0xEA,       // Unpack Ascii
  UNPKU = 0xE2,       // Unpack Unicode
  UPT = 0x0102,       // Update Tree
  X = 0x57,           // Exclusive Or (32)
  XC = 0xD7,          // Exclusive Or (character)
  XG = 0xE382,        // Exclusive Or (64)
  XGR = 0xB982,       // Exclusive Or (64)
  XGRK = 0xB9E7,      // Exclusive Or (64)
  XI = 0x97,          // Exclusive Or (immediate)
  XIHF = 0xC06,       // Exclusive Or Immediate (high)
  XILF = 0xC07,       // Exclusive Or Immediate (low)
  XIY = 0xEB57,       // Exclusive Or (immediate)
  XR = 0x17,          // Exclusive Or (32)
  XRK = 0xB9F7,       // Exclusive Or (32)
  XSCH = 0xB276,      // Cancel Subchannel
  XY = 0xE357,        // Exclusive Or (32)
  ZAP = 0xF8,         // Zero And Add
  BKPT = 0x0001       // GDB Software Breakpoint
};

// Instruction encoding bits and masks.
enum {
  // Instruction encoding bit
  B1 = 1 << 1,
  B4 = 1 << 4,
  B5 = 1 << 5,
  B7 = 1 << 7,
  B8 = 1 << 8,
  B9 = 1 << 9,
  B12 = 1 << 12,
  B18 = 1 << 18,
  B19 = 1 << 19,
  B20 = 1 << 20,
  B22 = 1 << 22,
  B23 = 1 << 23,
  B24 = 1 << 24,
  B25 = 1 << 25,
  B26 = 1 << 26,
  B27 = 1 << 27,
  B28 = 1 << 28,

  B6 = 1 << 6,
  B10 = 1 << 10,
  B11 = 1 << 11,
  B16 = 1 << 16,
  B17 = 1 << 17,
  B21 = 1 << 21,

  // Instruction bit masks
  kCondMask = 0x1F << 21,
  kOff12Mask = (1 << 12) - 1,
  kImm24Mask = (1 << 24) - 1,
  kOff16Mask = (1 << 16) - 1,
  kImm16Mask = (1 << 16) - 1,
  kImm26Mask = (1 << 26) - 1,
  kBOfieldMask = 0x1f << 21,
  kOpcodeMask = 0x3f << 26,
  kExt2OpcodeMask = 0x1f << 1,
  kExt5OpcodeMask = 0x3 << 2,
  kBIMask = 0x1F << 16,
  kBDMask = 0x14 << 2,
  kAAMask = 0x01 << 1,
  kLKMask = 0x01,
  kRCMask = 0x01,
  kTOMask = 0x1f << 21
};

// S390 instructions requires bigger shifts,
// make them macros instead of enum because of the typing issue
#define B32 ((uint64_t)1 << 32)
#define B36 ((uint64_t)1 << 36)
#define B40 ((uint64_t)1 << 40)
const FourByteInstr kFourByteBrCondMask = 0xF << 20;
const SixByteInstr kSixByteBrCondMask = static_cast<SixByteInstr>(0xF) << 36;

// -----------------------------------------------------------------------------
// Addressing modes and instruction variants.

// Overflow Exception
enum OEBit {
  SetOE = 1 << 10,   // Set overflow exception
  LeaveOE = 0 << 10  // No overflow exception
};

// Record bit
enum RCBit {   // Bit 0
  SetRC = 1,   // LT,GT,EQ,SO
  LeaveRC = 0  // None
};

// Link bit
enum LKBit {   // Bit 0
  SetLK = 1,   // Load effective address of next instruction
  LeaveLK = 0  // No action
};

enum BOfield {        // Bits 25-21
  DCBNZF = 0 << 21,   // Decrement CTR; branch if CTR != 0 and condition false
  DCBEZF = 2 << 21,   // Decrement CTR; branch if CTR == 0 and condition false
  BF = 4 << 21,       // Branch if condition false
  DCBNZT = 8 << 21,   // Decrement CTR; branch if CTR != 0 and condition true
  DCBEZT = 10 << 21,  // Decrement CTR; branch if CTR == 0 and condition true
  BT = 12 << 21,      // Branch if condition true
  DCBNZ = 16 << 21,   // Decrement CTR; branch if CTR != 0
  DCBEZ = 18 << 21,   // Decrement CTR; branch if CTR == 0
  BA = 20 << 21       // Branch always
};

#ifdef _AIX
#undef CR_LT
#undef CR_GT
#undef CR_EQ
#undef CR_SO
#endif

enum CRBit { CR_LT = 0, CR_GT = 1, CR_EQ = 2, CR_SO = 3, CR_FU = 3 };

#define CRWIDTH 4

// -----------------------------------------------------------------------------
// Supervisor Call (svc) specific support.

// Special Software Interrupt codes when used in the presence of the S390
// simulator.
// SVC provides a 24bit immediate value. Use bits 22:0 for standard
// SoftwareInterrupCode. Bit 23 is reserved for the stop feature.
enum SoftwareInterruptCodes {
  // Transition to C code
  kCallRtRedirected = 0x0010,
  // Breakpoint
  kBreakpoint = 0x0000,
  // Stop
  kStopCode = 1 << 23
};
const uint32_t kStopCodeMask = kStopCode - 1;
const uint32_t kMaxStopCode = kStopCode - 1;
const int32_t kDefaultStopCode = -1;

// FP rounding modes.
enum FPRoundingMode {
  RN = 0,  // Round to Nearest.
  RZ = 1,  // Round towards zero.
  RP = 2,  // Round towards Plus Infinity.
  RM = 3,  // Round towards Minus Infinity.

  // Aliases.
  kRoundToNearest = RN,
  kRoundToZero = RZ,
  kRoundToPlusInf = RP,
  kRoundToMinusInf = RM
};

const uint32_t kFPRoundingModeMask = 3;

enum CheckForInexactConversion {
  kCheckForInexactConversion,
  kDontCheckForInexactConversion
};

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

// use TRAP4 to indicate redirection call for simulation mode
const Instr rtCallRedirInstr = TRAP4;

// -----------------------------------------------------------------------------
// Instruction abstraction.

// The class Instruction enables access to individual fields defined in the
// z/Architecture instruction set encoding.
class Instruction {
 public:
  // S390 Opcode Format Types
  //   Based on the first byte of the opcode, we can determine how to extract
  //   the entire opcode of the instruction.  The various favours include:
  enum OpcodeFormatType {
    ONE_BYTE_OPCODE,           // One Byte - Bits 0 to 7
    TWO_BYTE_OPCODE,           // Two Bytes - Bits 0 to 15
    TWO_BYTE_DISJOINT_OPCODE,  // Two Bytes - Bits 0 to 7, 40 to 47
    THREE_NIBBLE_OPCODE        // Three Nibbles - Bits 0 to 7, 12 to 15
  };

  static OpcodeFormatType OpcodeFormatTable[256];
// Helper macro to define static accessors.
// We use the cast to char* trick to bypass the strict anti-aliasing rules.
#define DECLARE_STATIC_TYPED_ACCESSOR(return_type, Name) \
  static inline return_type Name(Instr instr) {          \
    char* temp = reinterpret_cast<char*>(&instr);        \
    return reinterpret_cast<Instruction*>(temp)->Name(); \
  }

#define DECLARE_STATIC_ACCESSOR(Name) DECLARE_STATIC_TYPED_ACCESSOR(int, Name)

  // Get the raw instruction bits.
  template <typename T>
  inline T InstructionBits() const {
    return Instruction::InstructionBits<T>(reinterpret_cast<const byte*>(this));
  }
  inline Instr InstructionBits() const {
    return *reinterpret_cast<const Instr*>(this);
  }

  // Set the raw instruction bits to value.
  template <typename T>
  inline void SetInstructionBits(T value) const {
    Instruction::SetInstructionBits<T>(reinterpret_cast<const byte*>(this),
                                       value);
  }
  inline void SetInstructionBits(Instr value) {
    *reinterpret_cast<Instr*>(this) = value;
  }

  // Read one particular bit out of the instruction bits.
  inline int Bit(int nr) const { return (InstructionBits() >> nr) & 1; }

  // Read a bit field's value out of the instruction bits.
  inline int Bits(int hi, int lo) const {
    return (InstructionBits() >> lo) & ((2 << (hi - lo)) - 1);
  }

  // Read bits according to instruction type
  template <typename T, typename U>
  inline U Bits(int hi, int lo) const {
    return (InstructionBits<T>() >> lo) & ((2 << (hi - lo)) - 1);
  }

  // Read a bit field out of the instruction bits.
  inline int BitField(int hi, int lo) const {
    return InstructionBits() & (((2 << (hi - lo)) - 1) << lo);
  }

  // Determine the instruction length
  inline int InstructionLength() {
    return Instruction::InstructionLength(reinterpret_cast<const byte*>(this));
  }
  // Extract the Instruction Opcode
  inline Opcode S390OpcodeValue() {
    return Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(this));
  }

  // Static support.

  // Read one particular bit out of the instruction bits.
  static inline int Bit(Instr instr, int nr) { return (instr >> nr) & 1; }

  // Read the value of a bit field out of the instruction bits.
  static inline int Bits(Instr instr, int hi, int lo) {
    return (instr >> lo) & ((2 << (hi - lo)) - 1);
  }

  // Read a bit field out of the instruction bits.
  static inline int BitField(Instr instr, int hi, int lo) {
    return instr & (((2 << (hi - lo)) - 1) << lo);
  }

  // Determine the instruction length of the given instruction
  static inline int InstructionLength(const byte* instr) {
    // Length can be determined by the first nibble.
    // 0x0 to 0x3 => 2-bytes
    // 0x4 to 0xB => 4-bytes
    // 0xC to 0xF => 6-bytes
    byte topNibble = (*instr >> 4) & 0xF;
    if (topNibble <= 3)
      return 2;
    else if (topNibble <= 0xB)
      return 4;
    return 6;
  }

  // Returns the instruction bits of the given instruction
  static inline uint64_t InstructionBits(const byte* instr) {
    int length = InstructionLength(instr);
    if (2 == length)
      return static_cast<uint64_t>(InstructionBits<TwoByteInstr>(instr));
    else if (4 == length)
      return static_cast<uint64_t>(InstructionBits<FourByteInstr>(instr));
    else
      return InstructionBits<SixByteInstr>(instr);
  }

  // Extract the raw instruction bits
  template <typename T>
  static inline T InstructionBits(const byte* instr) {
#if !V8_TARGET_LITTLE_ENDIAN
    if (sizeof(T) <= 4) {
      return *reinterpret_cast<const T*>(instr);
    } else {
      // We cannot read 8-byte instructon address directly, because for a
      // six-byte instruction, the extra 2-byte address might not be
      // allocated.
      uint64_t fourBytes = *reinterpret_cast<const uint32_t*>(instr);
      uint16_t twoBytes = *reinterpret_cast<const uint16_t*>(instr + 4);
      return (fourBytes << 16 | twoBytes);
    }
#else
    // Even on little endian hosts (simulation), the instructions
    // are stored as big-endian in order to decode the opcode and
    // instruction length.
    T instr_bits = 0;

    // 6-byte instrs are represented by uint64_t
    uint32_t size = (sizeof(T) == 8) ? 6 : sizeof(T);

    for (T i = 0; i < size; i++) {
      instr_bits <<= 8;
      instr_bits |= *(instr + i);
    }
    return instr_bits;
#endif
  }

  // Set the Instruction Bits to value
  template <typename T>
  static inline void SetInstructionBits(byte* instr, T value) {
#if V8_TARGET_LITTLE_ENDIAN
    // The instruction bits are stored in big endian format even on little
    // endian hosts, in order to decode instruction length and opcode.
    // The following code will reverse the bytes so that the stores later
    // (which are in native endianess) will effectively save the instruction
    // in big endian.
    if (sizeof(T) == 2) {
      // Two Byte Instruction
      value = ((value & 0x00FF) << 8) | ((value & 0xFF00) >> 8);
    } else if (sizeof(T) == 4) {
      // Four Byte Instruction
      value = ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) |
              ((value & 0x00FF0000) >> 8) | ((value & 0xFF000000) >> 24);
    } else if (sizeof(T) == 8) {
      // Six Byte Instruction
      uint64_t orig_value = static_cast<uint64_t>(value);
      value = (static_cast<uint64_t>(orig_value & 0xFF) << 40) |
              (static_cast<uint64_t>((orig_value >> 8) & 0xFF) << 32) |
              (static_cast<uint64_t>((orig_value >> 16) & 0xFF) << 24) |
              (static_cast<uint64_t>((orig_value >> 24) & 0xFF) << 16) |
              (static_cast<uint64_t>((orig_value >> 32) & 0xFF) << 8) |
              (static_cast<uint64_t>((orig_value >> 40) & 0xFF));
    }
#endif
    if (sizeof(T) <= 4) {
      *reinterpret_cast<T*>(instr) = value;
    } else {
#if V8_TARGET_LITTLE_ENDIAN
      uint64_t orig_value = static_cast<uint64_t>(value);
      *reinterpret_cast<uint32_t*>(instr) = static_cast<uint32_t>(value);
      *reinterpret_cast<uint16_t*>(instr + 4) =
          static_cast<uint16_t>((orig_value >> 32) & 0xFFFF);
#else
      *reinterpret_cast<uint32_t*>(instr) = static_cast<uint32_t>(value >> 16);
      *reinterpret_cast<uint16_t*>(instr + 4) =
          static_cast<uint16_t>(value & 0xFFFF);
#endif
    }
  }

  // Get Instruction Format Type
  static OpcodeFormatType getOpcodeFormatType(const byte* instr) {
    const byte firstByte = *instr;
    return OpcodeFormatTable[firstByte];
  }

  // Extract the full opcode from the instruction.
  static inline Opcode S390OpcodeValue(const byte* instr) {
    OpcodeFormatType opcodeType = getOpcodeFormatType(instr);

    // The native instructions are encoded in big-endian format
    // even if running on little-endian host.  Hence, we need
    // to ensure we use byte* based bit-wise logic.
    switch (opcodeType) {
      case ONE_BYTE_OPCODE:
        // One Byte - Bits 0 to 7
        return static_cast<Opcode>(*instr);
      case TWO_BYTE_OPCODE:
        // Two Bytes - Bits 0 to 15
        return static_cast<Opcode>((*instr << 8) | (*(instr + 1)));
      case TWO_BYTE_DISJOINT_OPCODE:
        // Two Bytes - Bits 0 to 7, 40 to 47
        return static_cast<Opcode>((*instr << 8) | (*(instr + 5) & 0xFF));
      default:
        // case THREE_NIBBLE_OPCODE:
        // Three Nibbles - Bits 0 to 7, 12 to 15
        return static_cast<Opcode>((*instr << 4) | (*(instr + 1) & 0xF));
    }

    UNREACHABLE();
    return static_cast<Opcode>(-1);
  }

  // Fields used in Software interrupt instructions
  inline SoftwareInterruptCodes SvcValue() const {
    return static_cast<SoftwareInterruptCodes>(Bits<FourByteInstr, int>(15, 0));
  }

  // Instructions are read of out a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instruction.
  // Use the At(pc) function to create references to Instruction.
  static Instruction* At(byte* pc) {
    return reinterpret_cast<Instruction*>(pc);
  }

 private:
  // We need to prevent the creation of instances of class Instruction.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instruction);
};

// I Instruction -- suspect this will not be used,
// but implement for completeness
class IInstruction : Instruction {
 public:
  inline int IValue() const { return Bits<TwoByteInstr, int>(7, 0); }

  inline int size() const { return 2; }
};

// RR Instruction
class RRInstruction : Instruction {
 public:
  inline int R1Value() const {
    // the high and low parameters of Bits is the number of bits from
    // rightmost place
    return Bits<TwoByteInstr, int>(7, 4);
  }
  inline int R2Value() const { return Bits<TwoByteInstr, int>(3, 0); }
  inline Condition M1Value() const {
    return static_cast<Condition>(Bits<TwoByteInstr, int>(7, 4));
  }

  inline int size() const { return 2; }
};

// RRE Instruction
class RREInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<FourByteInstr, int>(7, 4); }
  inline int R2Value() const { return Bits<FourByteInstr, int>(3, 0); }
  inline int M3Value() const { return Bits<FourByteInstr, int>(15, 12); }
  inline int M4Value() const { return Bits<FourByteInstr, int>(19, 16); }
  inline int size() const { return 4; }
};

// RRF Instruction
class RRFInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<FourByteInstr, int>(7, 4); }
  inline int R2Value() const { return Bits<FourByteInstr, int>(3, 0); }
  inline int R3Value() const { return Bits<FourByteInstr, int>(15, 12); }
  inline int M3Value() const { return Bits<FourByteInstr, int>(15, 12); }
  inline int M4Value() const { return Bits<FourByteInstr, int>(11, 8); }
  inline int size() const { return 4; }
};

// RRD Isntruction
class RRDInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<FourByteInstr, int>(15, 12); }
  inline int R2Value() const { return Bits<FourByteInstr, int>(3, 0); }
  inline int R3Value() const { return Bits<FourByteInstr, int>(7, 4); }
  inline int size() const { return 4; }
};

// RI Instruction
class RIInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<FourByteInstr, int>(23, 20); }
  inline int16_t I2Value() const { return Bits<FourByteInstr, int16_t>(15, 0); }
  inline uint16_t I2UnsignedValue() const {
    return Bits<FourByteInstr, uint16_t>(15, 0);
  }
  inline Condition M1Value() const {
    return static_cast<Condition>(Bits<FourByteInstr, int>(23, 20));
  }
  inline int size() const { return 4; }
};

// RS Instruction
class RSInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<FourByteInstr, int>(23, 20); }
  inline int R3Value() const { return Bits<FourByteInstr, int>(19, 16); }
  inline int B2Value() const { return Bits<FourByteInstr, int>(15, 12); }
  inline unsigned int D2Value() const {
    return Bits<FourByteInstr, unsigned int>(11, 0);
  }
  inline int size() const { return 4; }
};

// RSY Instruction
class RSYInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<SixByteInstr, int>(39, 36); }
  inline int R3Value() const { return Bits<SixByteInstr, int>(35, 32); }
  inline int B2Value() const { return Bits<SixByteInstr, int>(31, 28); }
  inline int32_t D2Value() const {
    int32_t value = Bits<SixByteInstr, int32_t>(27, 16);
    value += Bits<SixByteInstr, int8_t>(15, 8) << 12;
    return value;
  }
  inline int size() const { return 6; }
};

// RX Instruction
class RXInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<FourByteInstr, int>(23, 20); }
  inline int X2Value() const { return Bits<FourByteInstr, int>(19, 16); }
  inline int B2Value() const { return Bits<FourByteInstr, int>(15, 12); }
  inline uint32_t D2Value() const {
    return Bits<FourByteInstr, uint32_t>(11, 0);
  }
  inline int size() const { return 4; }
};

// RXY Instruction
class RXYInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<SixByteInstr, int>(39, 36); }
  inline int X2Value() const { return Bits<SixByteInstr, int>(35, 32); }
  inline int B2Value() const { return Bits<SixByteInstr, int>(31, 28); }
  inline int32_t D2Value() const {
    int32_t value = Bits<SixByteInstr, uint32_t>(27, 16);
    value += Bits<SixByteInstr, int8_t>(15, 8) << 12;
    return value;
  }
  inline int size() const { return 6; }
};

// RIL Instruction
class RILInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<SixByteInstr, int>(39, 36); }
  inline int32_t I2Value() const { return Bits<SixByteInstr, int32_t>(31, 0); }
  inline uint32_t I2UnsignedValue() const {
    return Bits<SixByteInstr, uint32_t>(31, 0);
  }
  inline int size() const { return 6; }
};

// SI Instruction
class SIInstruction : Instruction {
 public:
  inline int B1Value() const { return Bits<FourByteInstr, int>(15, 12); }
  inline uint32_t D1Value() const {
    return Bits<FourByteInstr, uint32_t>(11, 0);
  }
  inline uint8_t I2Value() const {
    return Bits<FourByteInstr, uint8_t>(23, 16);
  }
  inline int size() const { return 4; }
};

// SIY Instruction
class SIYInstruction : Instruction {
 public:
  inline int B1Value() const { return Bits<SixByteInstr, int>(31, 28); }
  inline int32_t D1Value() const {
    int32_t value = Bits<SixByteInstr, uint32_t>(27, 16);
    value += Bits<SixByteInstr, int8_t>(15, 8) << 12;
    return value;
  }
  inline uint8_t I2Value() const { return Bits<SixByteInstr, uint8_t>(39, 32); }
  inline int size() const { return 6; }
};

// SIL Instruction
class SILInstruction : Instruction {
 public:
  inline int B1Value() const { return Bits<SixByteInstr, int>(31, 28); }
  inline int D1Value() const { return Bits<SixByteInstr, int>(27, 16); }
  inline int I2Value() const { return Bits<SixByteInstr, int>(15, 0); }
  inline int size() const { return 6; }
};

// SS Instruction
class SSInstruction : Instruction {
 public:
  inline int B1Value() const { return Bits<SixByteInstr, int>(31, 28); }
  inline int B2Value() const { return Bits<SixByteInstr, int>(15, 12); }
  inline int D1Value() const { return Bits<SixByteInstr, int>(27, 16); }
  inline int D2Value() const { return Bits<SixByteInstr, int>(11, 0); }
  inline int Length() const { return Bits<SixByteInstr, int>(39, 32); }
  inline int size() const { return 6; }
};

// RXE Instruction
class RXEInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<SixByteInstr, int>(39, 36); }
  inline int X2Value() const { return Bits<SixByteInstr, int>(35, 32); }
  inline int B2Value() const { return Bits<SixByteInstr, int>(31, 28); }
  inline int D2Value() const { return Bits<SixByteInstr, int>(27, 16); }
  inline int size() const { return 6; }
};

// RIE Instruction
class RIEInstruction : Instruction {
 public:
  inline int R1Value() const { return Bits<SixByteInstr, int>(39, 36); }
  inline int R2Value() const { return Bits<SixByteInstr, int>(35, 32); }
  inline int I3Value() const { return Bits<SixByteInstr, uint32_t>(31, 24); }
  inline int I4Value() const { return Bits<SixByteInstr, uint32_t>(23, 16); }
  inline int I5Value() const { return Bits<SixByteInstr, uint32_t>(15, 8); }
  inline int I6Value() const {
    return static_cast<int32_t>(Bits<SixByteInstr, int16_t>(31, 16));
  }
  inline int size() const { return 6; }
};

// Helper functions for converting between register numbers and names.
class Registers {
 public:
  // Lookup the register number for the name provided.
  static int Number(const char* name);

 private:
  static const char* names_[kNumRegisters];
};

// Helper functions for converting between FP register numbers and names.
class DoubleRegisters {
 public:
  // Lookup the register number for the name provided.
  static int Number(const char* name);

 private:
  static const char* names_[kNumDoubleRegisters];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_S390_CONSTANTS_S390_H_
