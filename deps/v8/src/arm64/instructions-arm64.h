// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_INSTRUCTIONS_ARM64_H_
#define V8_ARM64_INSTRUCTIONS_ARM64_H_

#include "src/arm64/constants-arm64.h"
#include "src/arm64/utils-arm64.h"
#include "src/globals.h"
#include "src/utils.h"

namespace v8 {
namespace internal {


// ISA constants. --------------------------------------------------------------

typedef uint32_t Instr;

// The following macros initialize a float/double variable with a bit pattern
// without using static initializers: If ARM64_DEFINE_FP_STATICS is defined, the
// symbol is defined as uint32_t/uint64_t initialized with the desired bit
// pattern. Otherwise, the same symbol is declared as an external float/double.
#if defined(ARM64_DEFINE_FP_STATICS)
#define DEFINE_FLOAT(name, value) extern const uint32_t name = value
#define DEFINE_DOUBLE(name, value) extern const uint64_t name = value
#else
#define DEFINE_FLOAT(name, value) extern const float name
#define DEFINE_DOUBLE(name, value) extern const double name
#endif  // defined(ARM64_DEFINE_FP_STATICS)

DEFINE_FLOAT(kFP32PositiveInfinity, 0x7f800000);
DEFINE_FLOAT(kFP32NegativeInfinity, 0xff800000);
DEFINE_DOUBLE(kFP64PositiveInfinity, 0x7ff0000000000000UL);
DEFINE_DOUBLE(kFP64NegativeInfinity, 0xfff0000000000000UL);

// This value is a signalling NaN as both a double and as a float (taking the
// least-significant word).
DEFINE_DOUBLE(kFP64SignallingNaN, 0x7ff000007f800001);
DEFINE_FLOAT(kFP32SignallingNaN, 0x7f800001);

// A similar value, but as a quiet NaN.
DEFINE_DOUBLE(kFP64QuietNaN, 0x7ff800007fc00001);
DEFINE_FLOAT(kFP32QuietNaN, 0x7fc00001);

// The default NaN values (for FPCR.DN=1).
DEFINE_DOUBLE(kFP64DefaultNaN, 0x7ff8000000000000UL);
DEFINE_FLOAT(kFP32DefaultNaN, 0x7fc00000);

#undef DEFINE_FLOAT
#undef DEFINE_DOUBLE


enum LSDataSize {
  LSByte        = 0,
  LSHalfword    = 1,
  LSWord        = 2,
  LSDoubleWord  = 3
};

LSDataSize CalcLSPairDataSize(LoadStorePairOp op);

enum ImmBranchType {
  UnknownBranchType = 0,
  CondBranchType    = 1,
  UncondBranchType  = 2,
  CompareBranchType = 3,
  TestBranchType    = 4
};

enum AddrMode {
  Offset,
  PreIndex,
  PostIndex
};

enum FPRounding {
  // The first four values are encodable directly by FPCR<RMode>.
  FPTieEven = 0x0,
  FPPositiveInfinity = 0x1,
  FPNegativeInfinity = 0x2,
  FPZero = 0x3,

  // The final rounding mode is only available when explicitly specified by the
  // instruction (such as with fcvta). It cannot be set in FPCR.
  FPTieAway
};

enum Reg31Mode {
  Reg31IsStackPointer,
  Reg31IsZeroRegister
};

// Instructions. ---------------------------------------------------------------

class Instruction {
 public:
  V8_INLINE Instr InstructionBits() const {
    return *reinterpret_cast<const Instr*>(this);
  }

  V8_INLINE void SetInstructionBits(Instr new_instr) {
    *reinterpret_cast<Instr*>(this) = new_instr;
  }

  int Bit(int pos) const {
    return (InstructionBits() >> pos) & 1;
  }

  uint32_t Bits(int msb, int lsb) const {
    return unsigned_bitextract_32(msb, lsb, InstructionBits());
  }

  int32_t SignedBits(int msb, int lsb) const {
    int32_t bits = *(reinterpret_cast<const int32_t*>(this));
    return signed_bitextract_32(msb, lsb, bits);
  }

  Instr Mask(uint32_t mask) const {
    return InstructionBits() & mask;
  }

  V8_INLINE const Instruction* following(int count = 1) const {
    return InstructionAtOffset(count * static_cast<int>(kInstructionSize));
  }

  V8_INLINE Instruction* following(int count = 1) {
    return InstructionAtOffset(count * static_cast<int>(kInstructionSize));
  }

  V8_INLINE const Instruction* preceding(int count = 1) const {
    return following(-count);
  }

  V8_INLINE Instruction* preceding(int count = 1) {
    return following(-count);
  }

#define DEFINE_GETTER(Name, HighBit, LowBit, Func) \
  int32_t Name() const { return Func(HighBit, LowBit); }
  INSTRUCTION_FIELDS_LIST(DEFINE_GETTER)
  #undef DEFINE_GETTER

  // ImmPCRel is a compound field (not present in INSTRUCTION_FIELDS_LIST),
  // formed from ImmPCRelLo and ImmPCRelHi.
  int ImmPCRel() const {
    DCHECK(IsPCRelAddressing());
    int offset = ((ImmPCRelHi() << ImmPCRelLo_width) | ImmPCRelLo());
    int width = ImmPCRelLo_width + ImmPCRelHi_width;
    return signed_bitextract_32(width - 1, 0, offset);
  }

  uint64_t ImmLogical();
  float ImmFP32();
  double ImmFP64();

  LSDataSize SizeLSPair() const {
    return CalcLSPairDataSize(
        static_cast<LoadStorePairOp>(Mask(LoadStorePairMask)));
  }

  // Helpers.
  bool IsCondBranchImm() const {
    return Mask(ConditionalBranchFMask) == ConditionalBranchFixed;
  }

  bool IsUncondBranchImm() const {
    return Mask(UnconditionalBranchFMask) == UnconditionalBranchFixed;
  }

  bool IsCompareBranch() const {
    return Mask(CompareBranchFMask) == CompareBranchFixed;
  }

  bool IsTestBranch() const {
    return Mask(TestBranchFMask) == TestBranchFixed;
  }

  bool IsImmBranch() const {
    return BranchType() != UnknownBranchType;
  }

  bool IsLdrLiteral() const {
    return Mask(LoadLiteralFMask) == LoadLiteralFixed;
  }

  bool IsLdrLiteralX() const {
    return Mask(LoadLiteralMask) == LDR_x_lit;
  }

  bool IsPCRelAddressing() const {
    return Mask(PCRelAddressingFMask) == PCRelAddressingFixed;
  }

  bool IsAdr() const {
    return Mask(PCRelAddressingMask) == ADR;
  }

  bool IsBrk() const { return Mask(ExceptionMask) == BRK; }

  bool IsUnresolvedInternalReference() const {
    // Unresolved internal references are encoded as two consecutive brk
    // instructions.
    return IsBrk() && following()->IsBrk();
  }

  bool IsLogicalImmediate() const {
    return Mask(LogicalImmediateFMask) == LogicalImmediateFixed;
  }

  bool IsAddSubImmediate() const {
    return Mask(AddSubImmediateFMask) == AddSubImmediateFixed;
  }

  bool IsAddSubShifted() const {
    return Mask(AddSubShiftedFMask) == AddSubShiftedFixed;
  }

  bool IsAddSubExtended() const {
    return Mask(AddSubExtendedFMask) == AddSubExtendedFixed;
  }

  // Match any loads or stores, including pairs.
  bool IsLoadOrStore() const {
    return Mask(LoadStoreAnyFMask) == LoadStoreAnyFixed;
  }

  // Match any loads, including pairs.
  bool IsLoad() const;
  // Match any stores, including pairs.
  bool IsStore() const;

  // Indicate whether Rd can be the stack pointer or the zero register. This
  // does not check that the instruction actually has an Rd field.
  Reg31Mode RdMode() const {
    // The following instructions use csp or wsp as Rd:
    //  Add/sub (immediate) when not setting the flags.
    //  Add/sub (extended) when not setting the flags.
    //  Logical (immediate) when not setting the flags.
    // Otherwise, r31 is the zero register.
    if (IsAddSubImmediate() || IsAddSubExtended()) {
      if (Mask(AddSubSetFlagsBit)) {
        return Reg31IsZeroRegister;
      } else {
        return Reg31IsStackPointer;
      }
    }
    if (IsLogicalImmediate()) {
      // Of the logical (immediate) instructions, only ANDS (and its aliases)
      // can set the flags. The others can all write into csp.
      // Note that some logical operations are not available to
      // immediate-operand instructions, so we have to combine two masks here.
      if (Mask(LogicalImmediateMask & LogicalOpMask) == ANDS) {
        return Reg31IsZeroRegister;
      } else {
        return Reg31IsStackPointer;
      }
    }
    return Reg31IsZeroRegister;
  }

  // Indicate whether Rn can be the stack pointer or the zero register. This
  // does not check that the instruction actually has an Rn field.
  Reg31Mode RnMode() const {
    // The following instructions use csp or wsp as Rn:
    //  All loads and stores.
    //  Add/sub (immediate).
    //  Add/sub (extended).
    // Otherwise, r31 is the zero register.
    if (IsLoadOrStore() || IsAddSubImmediate() || IsAddSubExtended()) {
      return Reg31IsStackPointer;
    }
    return Reg31IsZeroRegister;
  }

  ImmBranchType BranchType() const {
    if (IsCondBranchImm()) {
      return CondBranchType;
    } else if (IsUncondBranchImm()) {
      return UncondBranchType;
    } else if (IsCompareBranch()) {
      return CompareBranchType;
    } else if (IsTestBranch()) {
      return TestBranchType;
    } else {
      return UnknownBranchType;
    }
  }

  static int ImmBranchRangeBitwidth(ImmBranchType branch_type) {
    switch (branch_type) {
      case UncondBranchType:
        return ImmUncondBranch_width;
      case CondBranchType:
        return ImmCondBranch_width;
      case CompareBranchType:
        return ImmCmpBranch_width;
      case TestBranchType:
        return ImmTestBranch_width;
      default:
        UNREACHABLE();
        return 0;
    }
  }

  // The range of the branch instruction, expressed as 'instr +- range'.
  static int32_t ImmBranchRange(ImmBranchType branch_type) {
    return
      (1 << (ImmBranchRangeBitwidth(branch_type) + kInstructionSizeLog2)) / 2 -
      kInstructionSize;
  }

  int ImmBranch() const {
    switch (BranchType()) {
      case CondBranchType: return ImmCondBranch();
      case UncondBranchType: return ImmUncondBranch();
      case CompareBranchType: return ImmCmpBranch();
      case TestBranchType: return ImmTestBranch();
      default: UNREACHABLE();
    }
    return 0;
  }

  int ImmUnresolvedInternalReference() const {
    DCHECK(IsUnresolvedInternalReference());
    // Unresolved references are encoded as two consecutive brk instructions.
    // The associated immediate is made of the two 16-bit payloads.
    int32_t high16 = ImmException();
    int32_t low16 = following()->ImmException();
    return (high16 << 16) | low16;
  }

  bool IsBranchAndLinkToRegister() const {
    return Mask(UnconditionalBranchToRegisterMask) == BLR;
  }

  bool IsMovz() const {
    return (Mask(MoveWideImmediateMask) == MOVZ_x) ||
           (Mask(MoveWideImmediateMask) == MOVZ_w);
  }

  bool IsMovk() const {
    return (Mask(MoveWideImmediateMask) == MOVK_x) ||
           (Mask(MoveWideImmediateMask) == MOVK_w);
  }

  bool IsMovn() const {
    return (Mask(MoveWideImmediateMask) == MOVN_x) ||
           (Mask(MoveWideImmediateMask) == MOVN_w);
  }

  bool IsNop(int n) {
    // A marking nop is an instruction
    //   mov r<n>,  r<n>
    // which is encoded as
    //   orr r<n>, xzr, r<n>
    return (Mask(LogicalShiftedMask) == ORR_x) &&
           (Rd() == Rm()) &&
           (Rd() == n);
  }

  // Find the PC offset encoded in this instruction. 'this' may be a branch or
  // a PC-relative addressing instruction.
  // The offset returned is unscaled.
  int64_t ImmPCOffset();

  // Find the target of this instruction. 'this' may be a branch or a
  // PC-relative addressing instruction.
  Instruction* ImmPCOffsetTarget();

  static bool IsValidImmPCOffset(ImmBranchType branch_type, ptrdiff_t offset);
  bool IsTargetInImmPCOffsetRange(Instruction* target);
  // Patch a PC-relative offset to refer to 'target'. 'this' may be a branch or
  // a PC-relative addressing instruction.
  void SetImmPCOffsetTarget(Isolate* isolate, Instruction* target);
  void SetUnresolvedInternalReferenceImmTarget(Isolate* isolate,
                                               Instruction* target);
  // Patch a literal load instruction to load from 'source'.
  void SetImmLLiteral(Instruction* source);

  uintptr_t LiteralAddress() {
    int offset = ImmLLiteral() << kLoadLiteralScaleLog2;
    return reinterpret_cast<uintptr_t>(this) + offset;
  }

  enum CheckAlignment { NO_CHECK, CHECK_ALIGNMENT };

  V8_INLINE const Instruction* InstructionAtOffset(
      int64_t offset, CheckAlignment check = CHECK_ALIGNMENT) const {
    // The FUZZ_disasm test relies on no check being done.
    DCHECK(check == NO_CHECK || IsAligned(offset, kInstructionSize));
    return this + offset;
  }

  V8_INLINE Instruction* InstructionAtOffset(
      int64_t offset, CheckAlignment check = CHECK_ALIGNMENT) {
    // The FUZZ_disasm test relies on no check being done.
    DCHECK(check == NO_CHECK || IsAligned(offset, kInstructionSize));
    return this + offset;
  }

  template<typename T> V8_INLINE static Instruction* Cast(T src) {
    return reinterpret_cast<Instruction*>(src);
  }

  V8_INLINE ptrdiff_t DistanceTo(Instruction* target) {
    return reinterpret_cast<Address>(target) - reinterpret_cast<Address>(this);
  }


  static const int ImmPCRelRangeBitwidth = 21;
  static bool IsValidPCRelOffset(ptrdiff_t offset) { return is_int21(offset); }
  void SetPCRelImmTarget(Isolate* isolate, Instruction* target);
  void SetBranchImmTarget(Instruction* target);
};


// Where Instruction looks at instructions generated by the Assembler,
// InstructionSequence looks at instructions sequences generated by the
// MacroAssembler.
class InstructionSequence : public Instruction {
 public:
  static InstructionSequence* At(Address address) {
    return reinterpret_cast<InstructionSequence*>(address);
  }

  // Sequences generated by MacroAssembler::InlineData().
  bool IsInlineData() const;
  uint64_t InlineData() const;
};


// Simulator/Debugger debug instructions ---------------------------------------
// Each debug marker is represented by a HLT instruction. The immediate comment
// field in the instruction is used to identify the type of debug marker. Each
// marker encodes arguments in a different way, as described below.

// Indicate to the Debugger that the instruction is a redirected call.
const Instr kImmExceptionIsRedirectedCall = 0xca11;

// Represent unreachable code. This is used as a guard in parts of the code that
// should not be reachable, such as in data encoded inline in the instructions.
const Instr kImmExceptionIsUnreachable = 0xdebf;

// A pseudo 'printf' instruction. The arguments will be passed to the platform
// printf method.
const Instr kImmExceptionIsPrintf = 0xdeb1;
// Most parameters are stored in ARM64 registers as if the printf
// pseudo-instruction was a call to the real printf method:
//      x0: The format string.
//   x1-x7: Optional arguments.
//   d0-d7: Optional arguments.
//
// Also, the argument layout is described inline in the instructions:
//  - arg_count: The number of arguments.
//  - arg_pattern: A set of PrintfArgPattern values, packed into two-bit fields.
//
// Floating-point and integer arguments are passed in separate sets of registers
// in AAPCS64 (even for varargs functions), so it is not possible to determine
// the type of each argument without some information about the values that were
// passed in. This information could be retrieved from the printf format string,
// but the format string is not trivial to parse so we encode the relevant
// information with the HLT instruction.
const unsigned kPrintfArgCountOffset = 1 * kInstructionSize;
const unsigned kPrintfArgPatternListOffset = 2 * kInstructionSize;
const unsigned kPrintfLength = 3 * kInstructionSize;

const unsigned kPrintfMaxArgCount = 4;

// The argument pattern is a set of two-bit-fields, each with one of the
// following values:
enum PrintfArgPattern {
  kPrintfArgW = 1,
  kPrintfArgX = 2,
  // There is no kPrintfArgS because floats are always converted to doubles in C
  // varargs calls.
  kPrintfArgD = 3
};
static const unsigned kPrintfArgPatternBits = 2;

// A pseudo 'debug' instruction.
const Instr kImmExceptionIsDebug = 0xdeb0;
// Parameters are inlined in the code after a debug pseudo-instruction:
// - Debug code.
// - Debug parameters.
// - Debug message string. This is a NULL-terminated ASCII string, padded to
//   kInstructionSize so that subsequent instructions are correctly aligned.
// - A kImmExceptionIsUnreachable marker, to catch accidental execution of the
//   string data.
const unsigned kDebugCodeOffset = 1 * kInstructionSize;
const unsigned kDebugParamsOffset = 2 * kInstructionSize;
const unsigned kDebugMessageOffset = 3 * kInstructionSize;

// Debug parameters.
// Used without a TRACE_ option, the Debugger will print the arguments only
// once. Otherwise TRACE_ENABLE and TRACE_DISABLE will enable or disable tracing
// before every instruction for the specified LOG_ parameters.
//
// TRACE_OVERRIDE enables the specified LOG_ parameters, and disabled any
// others that were not specified.
//
// For example:
//
// __ debug("print registers and fp registers", 0, LOG_REGS | LOG_FP_REGS);
// will print the registers and fp registers only once.
//
// __ debug("trace disasm", 1, TRACE_ENABLE | LOG_DISASM);
// starts disassembling the code.
//
// __ debug("trace rets", 2, TRACE_ENABLE | LOG_REGS);
// adds the general purpose registers to the trace.
//
// __ debug("stop regs", 3, TRACE_DISABLE | LOG_REGS);
// stops tracing the registers.
const unsigned kDebuggerTracingDirectivesMask = 3 << 6;
enum DebugParameters {
  NO_PARAM       = 0,
  BREAK          = 1 << 0,
  LOG_DISASM     = 1 << 1,  // Use only with TRACE. Disassemble the code.
  LOG_REGS       = 1 << 2,  // Log general purpose registers.
  LOG_FP_REGS    = 1 << 3,  // Log floating-point registers.
  LOG_SYS_REGS   = 1 << 4,  // Log the status flags.
  LOG_WRITE      = 1 << 5,  // Log any memory write.

  LOG_STATE      = LOG_REGS | LOG_FP_REGS | LOG_SYS_REGS,
  LOG_ALL        = LOG_DISASM | LOG_STATE | LOG_WRITE,

  // Trace control.
  TRACE_ENABLE   = 1 << 6,
  TRACE_DISABLE  = 2 << 6,
  TRACE_OVERRIDE = 3 << 6
};


}  // namespace internal
}  // namespace v8


#endif  // V8_ARM64_INSTRUCTIONS_ARM64_H_
