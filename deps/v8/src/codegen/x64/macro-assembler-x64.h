// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_X64_MACRO_ASSEMBLER_X64_H_
#define V8_CODEGEN_X64_MACRO_ASSEMBLER_X64_H_

#ifndef INCLUDED_FROM_MACRO_ASSEMBLER_H
#error This header must be included via macro-assembler.h
#endif

#include "src/base/flags.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/common/globals.h"
#include "src/execution/frame-constants.h"
#include "src/execution/isolate-data.h"
#include "src/objects/contexts.h"
#include "src/objects/tagged-index.h"
#include "src/sandbox/trusted-pointer-tag.h"

namespace v8 {
namespace internal {

// Convenience for platform-independent signatures.
using MemOperand = Operand;

struct SmiIndex {
  SmiIndex(Register index_register, ScaleFactor scale)
      : reg(index_register), scale(scale) {}
  Register reg;
  ScaleFactor scale;
};

// TODO(victorgomes): Move definition to macro-assembler.h, once all other
// platforms are updated.
enum class StackLimitKind { kInterruptStackLimit, kRealStackLimit };

// Convenient class to access arguments below the stack pointer.
class StackArgumentsAccessor {
 public:
  // argc = the number of arguments not including the receiver.
  explicit StackArgumentsAccessor(Register argc) : argc_(argc) {
    DCHECK_NE(argc_, no_reg);
  }

  // Argument 0 is the receiver (despite argc not including the receiver).
  Operand operator[](int index) const { return GetArgumentOperand(index); }

  Operand GetArgumentOperand(int index) const;
  Operand GetReceiverOperand() const { return GetArgumentOperand(0); }

 private:
  const Register argc_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StackArgumentsAccessor);
};

// When generating a TSAN-aware load, we sometimes pass the register of the base
// object of the load, if the object is shared. If the register holds the
// compressed object value, we must decompress it before passing it to the
// runtime function.
struct SharedBaseTsanArgument {
  Register reg;
  bool must_decompress_reg;
};

class V8_EXPORT_PRIVATE MacroAssembler
    : public SharedMacroAssembler<MacroAssembler> {
 public:
  using SharedMacroAssembler<MacroAssembler>::SharedMacroAssembler;
  using SharedMacroAssembler<MacroAssembler>::Negps;
  using SharedMacroAssembler<MacroAssembler>::Negpd;

  V8_ASM_PUSH void PushReturnAddressFrom(V8_REG_IN Register src) { pushq(src); }
  V8_ASM_POP void PopReturnAddressTo(V8_REG_OUT Register dst) { popq(dst); }

  V8_ASM_NO_FALLTHROUGH void Ret();

  // Call incsspq with {number_of_words} only if the cpu supports it.
  // NOTE: This shouldn't be embedded in optimized code, since the check
  // for CPU support would be redundant (we could check at compiler time).
  void IncsspqIfSupported(V8_REG_IN Register number_of_words,
                          V8_REG_SCRATCH Register scratch);

  // Return and drop arguments from stack, where the number of arguments
  // may be bigger than 2^16 - 1.  Requires a scratch register.
  V8_ASM_NO_FALLTHROUGH void Ret(int bytes_dropped,
                                 V8_REG_SCRATCH Register scratch);

  // Operations on roots in the root-array.
  Operand RootAsOperand(RootIndex index);
  void LoadTaggedRoot(V8_REG_OUT Register destination, RootIndex index);
  V8_REG_CLOBBERS(kScratchRegister)
  void StoreTaggedRoot(Operand destination, RootIndex index);
  void LoadRoot(V8_REG_OUT Register destination, RootIndex index) final;
  V8_REG_CLOBBERS(kScratchRegister)
  void LoadRoot(Operand destination, RootIndex index) {
    LoadRoot(kScratchRegister, index);
    movq(destination, kScratchRegister);
  }

  V8_ASM_PUSH void Push(V8_REG_IN Register src);
  V8_ASM_PUSH void Push(Operand src);
  V8_ASM_PUSH void Push(Immediate value);
  V8_ASM_PUSH V8_REG_CLOBBERS(kScratchRegister) void Push(Tagged<Smi> smi);
  V8_ASM_PUSH void Push(Tagged<TaggedIndex> index) {
    Push(Immediate(static_cast<uint32_t>(index.ptr())));
  }
  V8_ASM_PUSH V8_REG_CLOBBERS(kScratchRegister) void Push(
      Handle<HeapObject> source);

  enum class PushArrayOrder { kNormal, kReverse };
  // `array` points to the first element (the lowest address).
  // `array` and `size` are not modified.
  void PushArray(V8_REG_IN Register array, V8_REG_IN Register size,
                 V8_REG_SCRATCH Register scratch,
                 PushArrayOrder order = PushArrayOrder::kNormal);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, arguments must be stored in rsp[0], rsp[8],
  // etc., not pushed. The argument count assumes all arguments are word sized.
  // The number of slots reserved for arguments depends on platform. On Windows
  // stack slots are reserved for the arguments passed in registers. On other
  // platforms stack slots are only reserved for the arguments actually passed
  // on the stack.
  V8_REG_CLOBBERS(kScratchRegister)
  void PrepareCallCFunction(int num_arguments);

  // Calls a C function and cleans up the space for arguments allocated
  // by PrepareCallCFunction. The called function is not allowed to trigger a
  // garbage collection, since that might move the code and invalidate the
  // return address (unless this is somehow accounted for by the called
  // function).
  V8_REG_CLOBBERS_CALLER_SAVED int CallCFunction(
      ExternalReference function, int num_arguments,
      SetIsolateDataSlots set_isolate_data_slots = SetIsolateDataSlots::kYes,
      V8_ASM_BIND Label* return_location = nullptr);
  V8_REG_CLOBBERS_CALLER_SAVED int CallCFunction(
      V8_REG_IN Register function, int num_arguments,
      SetIsolateDataSlots set_isolate_data_slots = SetIsolateDataSlots::kYes,
      V8_ASM_BIND Label* return_location = nullptr,
      CodeSandboxingMode target_sandboxing_mode =
          CodeSandboxingMode::kUnsandboxed);

  // Calculate the number of stack slots to reserve for arguments when calling a
  // C function.
  static int ArgumentStackSlotsForCFunctionCall(int num_arguments);

  void MemoryChunkHeaderFromObject(V8_REG_IN Register object,
                                   V8_REG_OUT Register header);
  void CheckPageFlag(V8_REG_IN Register object, V8_REG_SCRATCH Register scratch,
                     int mask, Condition cc, V8_ASM_BRANCH Label* condition_met,
                     Label::Distance condition_met_distance = Label::kFar);
  void CheckMarkBit(V8_REG_IN Register object, V8_REG_SCRATCH Register scratch0,
                    V8_REG_SCRATCH Register scratch1, Condition cc,
                    V8_ASM_BRANCH Label* condition_met,
                    Label::Distance condition_met_distance = Label::kFar);
  void JumpIfMarking(V8_ASM_BRANCH Label* is_marking,
                     Label::Distance condition_met_distance = Label::kFar);
  void JumpIfNotMarking(V8_ASM_BRANCH Label* not_marking,
                        Label::Distance condition_met_distance = Label::kFar);

  void PreCheckSkippedWriteBarrier(V8_REG_IN Register object,
                                   V8_REG_IN Register value,
                                   V8_REG_SCRATCH Register scratch,
                                   V8_ASM_BRANCH Label* ok);

  // Define movq here instead of using AVX_OP. movq is defined using templates
  // and there is a function template `void movq(P1)`, while technically
  // impossible, will be selected when deducing the arguments for AvxHelper.
  void Movq(XMMRegister dst, V8_REG_IN Register src);
  void Movq(V8_REG_OUT Register dst, XMMRegister src);

  void Cvtss2sd(XMMRegister dst, XMMRegister src);
  void Cvtss2sd(XMMRegister dst, Operand src);
  void Cvtsd2ss(XMMRegister dst, XMMRegister src);
  void Cvtsd2ss(XMMRegister dst, Operand src);
  void Cvttsd2si(V8_REG_OUT Register dst, XMMRegister src);
  void Cvttsd2si(V8_REG_OUT Register dst, Operand src);
  void Cvttsd2siq(V8_REG_OUT Register dst, XMMRegister src);
  void Cvttsd2siq(V8_REG_OUT Register dst, Operand src);
  void Cvttss2si(V8_REG_OUT Register dst, XMMRegister src);
  void Cvttss2si(V8_REG_OUT Register dst, Operand src);
  void Cvttss2siq(V8_REG_OUT Register dst, XMMRegister src);
  void Cvttss2siq(V8_REG_OUT Register dst, Operand src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtlui2ss(XMMRegister dst, V8_REG_IN Register src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtlui2ss(XMMRegister dst, Operand src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtlui2sd(XMMRegister dst, V8_REG_IN Register src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtlui2sd(XMMRegister dst, Operand src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtqui2ss(XMMRegister dst, V8_REG_IN Register src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtqui2ss(XMMRegister dst, Operand src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtqui2sd(XMMRegister dst, V8_REG_IN Register src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtqui2sd(XMMRegister dst, Operand src);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvttsd2uiq(V8_REG_OUT Register dst, Operand src,
                  V8_ASM_BRANCH Label* fail = nullptr);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvttsd2uiq(V8_REG_OUT Register dst, XMMRegister src,
                  V8_ASM_BRANCH Label* fail = nullptr);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvttsd2ui(V8_REG_OUT Register dst, Operand src,
                 V8_ASM_BRANCH Label* fail = nullptr);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvttsd2ui(V8_REG_OUT Register dst, XMMRegister src,
                 V8_ASM_BRANCH Label* fail = nullptr);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvttss2uiq(V8_REG_OUT Register dst, Operand src,
                  V8_ASM_BRANCH Label* fail = nullptr);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvttss2uiq(V8_REG_OUT Register dst, XMMRegister src,
                  V8_ASM_BRANCH Label* fail = nullptr);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvttss2ui(V8_REG_OUT Register dst, Operand src,
                 V8_ASM_BRANCH Label* fail = nullptr);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvttss2ui(V8_REG_OUT Register dst, XMMRegister src,
                 V8_ASM_BRANCH Label* fail = nullptr);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cvtpd2ph(XMMRegister dst, XMMRegister src, V8_REG_SCRATCH Register tmp);
  void Cvtph2pd(XMMRegister dst, XMMRegister src);

  // cvtsi2sd and cvtsi2ss instructions only write to the low 64/32-bit of dst
  // register, which hinders register renaming and makes dependence chains
  // longer. So we use xorpd to clear the dst register before cvtsi2sd for
  // non-AVX and a scratch XMM register as first src for AVX to solve this
  // issue.
  void Cvtqsi2ss(XMMRegister dst, V8_REG_IN Register src);
  void Cvtqsi2ss(XMMRegister dst, Operand src);
  void Cvtqsi2sd(XMMRegister dst, V8_REG_IN Register src);
  void Cvtqsi2sd(XMMRegister dst, Operand src);
  void Cvtlsi2ss(XMMRegister dst, V8_REG_IN Register src);
  void Cvtlsi2ss(XMMRegister dst, Operand src);
  void Cvtlsi2sd(XMMRegister dst, V8_REG_IN Register src);
  void Cvtlsi2sd(XMMRegister dst, Operand src);

  void Cmpeqss(XMMRegister dst, XMMRegister src);
  void Cmpeqsd(XMMRegister dst, XMMRegister src);

  void PextrdPreSse41(V8_REG_OUT Register dst, XMMRegister src, uint8_t imm8);
  void Pextrq(V8_REG_OUT Register dst, XMMRegister src, int8_t imm8);

  void PinsrdPreSse41(XMMRegister dst, V8_REG_IN Register src2, uint8_t imm8,
                      uint32_t* load_pc_offset = nullptr);
  void PinsrdPreSse41(XMMRegister dst, Operand src2, uint8_t imm8,
                      uint32_t* load_pc_offset = nullptr);

  void Pinsrq(XMMRegister dst, XMMRegister src1, V8_REG_IN Register src2,
              uint8_t imm8, uint32_t* load_pc_offset = nullptr);
  void Pinsrq(XMMRegister dst, XMMRegister src1, Operand src2, uint8_t imm8,
              uint32_t* load_pc_offset = nullptr);

  void Lzcntq(V8_REG_OUT Register dst, V8_REG_IN Register src);
  void Lzcntq(V8_REG_OUT Register dst, Operand src);
  void Lzcntl(V8_REG_OUT Register dst, V8_REG_IN Register src);
  void Lzcntl(V8_REG_OUT Register dst, Operand src);
  void Tzcntq(V8_REG_OUT Register dst, V8_REG_IN Register src);
  void Tzcntq(V8_REG_OUT Register dst, Operand src);
  void Tzcntl(V8_REG_OUT Register dst, V8_REG_IN Register src);
  void Tzcntl(V8_REG_OUT Register dst, Operand src);
  void Popcntl(V8_REG_OUT Register dst, V8_REG_IN Register src);
  void Popcntl(V8_REG_OUT Register dst, Operand src);
  void Popcntq(V8_REG_OUT Register dst, V8_REG_IN Register src);
  void Popcntq(V8_REG_OUT Register dst, Operand src);

  V8_REG_CLOBBERS(kScratchRegister)
  void Cmp(V8_REG_IN Register dst, Tagged<Smi> src);
  V8_REG_CLOBBERS(kScratchRegister) void Cmp(Operand dst, Tagged<Smi> src);
  void Cmp(V8_REG_IN Register dst, int32_t src);
  void Cmpq(V8_REG_IN Register dst, int32_t src);
  void Cmpb(V8_REG_IN Register dst, int32_t src);

  void CmpTagged(V8_REG_IN const Register& src1,
                 V8_REG_IN const Register& src2) {
    cmp_tagged(src1, src2);
  }

  // SIMD128
  void I64x2Abs(XMMRegister dst, XMMRegister src, XMMRegister scratch);
  void I64x2ShrS(XMMRegister dst, XMMRegister src, uint8_t shift,
                 XMMRegister xmm_tmp);
  void I64x2ShrS(XMMRegister dst, XMMRegister src, V8_REG_IN Register shift,
                 XMMRegister xmm_tmp, XMMRegister xmm_shift,
                 V8_REG_SCRATCH Register tmp_shift);
  void I64x2Mul(XMMRegister dst, XMMRegister lhs, XMMRegister rhs,
                XMMRegister tmp1 = XMMRegister::no_reg(),
                XMMRegister tmp2 = XMMRegister::no_reg());
  void I8x16Popcnt(XMMRegister dst, XMMRegister src,
                   V8_REG_SCRATCH Register scratch,
                   XMMRegister tmp1 = XMMRegister::no_reg(),
                   XMMRegister tmp2 = XMMRegister::no_reg());
  void S128Not(XMMRegister dst, XMMRegister src, XMMRegister scratch);
  // AVX10 and SSE paths require dst == mask.
  void S128Select(XMMRegister dst, XMMRegister mask, XMMRegister src1,
                  XMMRegister src2, XMMRegister scratch);

  // SIMD256
  void I64x4Mul(YMMRegister dst, YMMRegister lhs, YMMRegister rhs,
                YMMRegister tmp1 = YMMRegister::no_reg(),
                YMMRegister tmp2 = YMMRegister::no_reg());
  void F64x4Min(YMMRegister dst, YMMRegister lhs, YMMRegister rhs,
                YMMRegister scratch);
  void F64x4Max(YMMRegister dst, YMMRegister lhs, YMMRegister rhs,
                YMMRegister scratch);
  void F32x8Min(YMMRegister dst, YMMRegister lhs, YMMRegister rhs,
                YMMRegister scratch);
  void F32x8Max(YMMRegister dst, YMMRegister lhs, YMMRegister rhs,
                YMMRegister scratch);
  void F16x8Min(YMMRegister dst, XMMRegister lhs, XMMRegister rhs,
                YMMRegister scratch, YMMRegister scratch2);
  void F16x8Max(YMMRegister dst, XMMRegister lhs, XMMRegister rhs,
                YMMRegister scratch, YMMRegister scratch2);
  void I64x4ExtMul(YMMRegister dst, XMMRegister src1, XMMRegister src2,
                   YMMRegister scratch, bool is_signed);
  void I32x8ExtMul(YMMRegister dst, XMMRegister src1, XMMRegister src2,
                   YMMRegister scratch, bool is_signed);
  void I16x16ExtMul(YMMRegister dst, XMMRegister src1, XMMRegister src2,
                    YMMRegister scratch, bool is_signed);
#define MACRO_ASM_X64_IEXTADDPAIRWISE_LIST(V)                      \
  V(I32x8ExtAddPairwiseI16x16S, V8_REG_CLOBBERS(kScratchRegister)) \
  V(I32x8ExtAddPairwiseI16x16U, )                                  \
  V(I16x16ExtAddPairwiseI8x32S, V8_REG_CLOBBERS(kScratchRegister)) \
  V(I16x16ExtAddPairwiseI8x32U, V8_REG_CLOBBERS(kScratchRegister))

#define DECLARE_IEXTADDPAIRWISE(ExtAddPairwiseOp, Attr)        \
  Attr void ExtAddPairwiseOp(YMMRegister dst, YMMRegister src, \
                             YMMRegister scratch);
  MACRO_ASM_X64_IEXTADDPAIRWISE_LIST(DECLARE_IEXTADDPAIRWISE)
#undef DECLARE_IEXTADDPAIRWISE
#undef MACRO_ASM_X64_IEXTADDPAIRWISE_LIST

  void I32x8SConvertF32x8(YMMRegister dst, YMMRegister src, YMMRegister tmp,
                          V8_REG_SCRATCH Register scratch);
  void I16x8SConvertF16x8(YMMRegister dst, XMMRegister src, YMMRegister tmp,
                          V8_REG_SCRATCH Register scratch);
  V8_REG_CLOBBERS(kScratchRegister)
  void I16x8TruncF16x8U(YMMRegister dst, XMMRegister src, YMMRegister tmp);
  void F16x8SConvertI16x8(XMMRegister dst, XMMRegister src, YMMRegister tmp);
  void F16x8UConvertI16x8(XMMRegister dst, XMMRegister src, YMMRegister tmp);
  void F16x8Qfma(YMMRegister dst, XMMRegister src1, XMMRegister src2,
                 XMMRegister src3, YMMRegister tmp, YMMRegister tmp2);
  void F16x8Qfms(YMMRegister dst, XMMRegister src1, XMMRegister src2,
                 XMMRegister src3, YMMRegister tmp, YMMRegister tmp2);

  void S256Not(YMMRegister dst, YMMRegister src, YMMRegister scratch);
  // AVX10 path requires dst == mask.
  void S256Select(YMMRegister dst, YMMRegister mask, YMMRegister src1,
                  YMMRegister src2, YMMRegister scratch);

// Splat
#define MACRO_ASM_X64_ISPLAT_LIST(V) \
  V(I8x32Splat, b, vmovd)            \
  V(I16x16Splat, w, vmovd)           \
  V(I32x8Splat, d, vmovd)            \
  V(I64x4Splat, q, vmovq)

#define DECLARE_ISPLAT(name, suffix, instr_mov)       \
  void name(YMMRegister dst, V8_REG_IN Register src); \
  void name(YMMRegister dst, Operand src);

  MACRO_ASM_X64_ISPLAT_LIST(DECLARE_ISPLAT)

#undef DECLARE_ISPLAT

  void F64x4Splat(YMMRegister dst, XMMRegister src);
  void F32x8Splat(YMMRegister dst, XMMRegister src);
  void F16x8Splat(XMMRegister dst, XMMRegister src);

  void F32x8Qfma(YMMRegister dst, YMMRegister src1, YMMRegister src2,
                 YMMRegister src3, YMMRegister tmp);
  void F32x8Qfms(YMMRegister dst, YMMRegister src1, YMMRegister src2,
                 YMMRegister src3, YMMRegister tmp);
  void F64x4Qfma(YMMRegister dst, YMMRegister src1, YMMRegister src2,
                 YMMRegister src3, YMMRegister tmp);
  void F64x4Qfms(YMMRegister dst, YMMRegister src1, YMMRegister src2,
                 YMMRegister src3, YMMRegister tmp);

  void I32x8DotI8x32I7x32AddS(YMMRegister dst, YMMRegister src1,
                              YMMRegister src2, YMMRegister src3,
                              YMMRegister scratch, YMMRegister splat_reg);

  void I32x8TruncF32x8U(YMMRegister dst, YMMRegister src, YMMRegister scratch1,
                        YMMRegister scratch2);

  void Negpd(YMMRegister dst, YMMRegister src, YMMRegister scratch);
  void Negps(YMMRegister dst, YMMRegister src, YMMRegister scratch);

  // ---------------------------------------------------------------------------
  // Conversions between tagged smi values and non-tagged integer values.

  // Tag an word-size value. The result must be known to be a valid smi value.
  void SmiTag(V8_REG_INOUT Register reg);
  // Requires dst != src
  void SmiTag(V8_REG_OUT Register dst, V8_REG_IN Register src);

  // Simple comparison of smis.  Both sides must be known smis to use these,
  // otherwise use Cmp.
  void SmiCompare(V8_REG_IN Register smi1, V8_REG_IN Register smi2);
  V8_REG_CLOBBERS(kScratchRegister)
  void SmiCompare(V8_REG_IN Register dst, Tagged<Smi> src);
  void SmiCompare(V8_REG_IN Register dst, Operand src);
  void SmiCompare(Operand dst, V8_REG_IN Register src);
  void SmiCompare(Operand dst, Tagged<Smi> src);

  // Functions performing a check on a known or potential smi. Returns
  // a condition that is satisfied if the check is successful.
  Condition CheckSmi(V8_REG_IN Register src);
  Condition CheckSmi(Operand src);

  // This can be used in testing to ensure we never rely on what is in the
  // unused smi bits.
  void ClobberDecompressedSmiBits(V8_REG_INOUT Register smi);

  // Abort execution if argument is a smi, enabled via --debug-code.
  void AssertNotSmi(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if argument is not a smi, enabled via --debug-code.
  void AssertSmi(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;
  void AssertSmi(Operand object) NOOP_UNLESS_DEBUG_CODE;

  // Test-and-jump functions. Typically combines a check function
  // above with a conditional jump.

  // Jump to label if the value is a tagged smi.
  void JumpIfSmi(V8_REG_IN Register src, V8_ASM_BRANCH Label* on_smi,
                 Label::Distance near_jump = Label::kFar);

  // Jump to label if the value is not a tagged smi.
  void JumpIfNotSmi(V8_REG_IN Register src, V8_ASM_BRANCH Label* on_not_smi,
                    Label::Distance near_jump = Label::kFar);

  // Jump to label if the value is not a tagged smi.
  void JumpIfNotSmi(Operand src, V8_ASM_BRANCH Label* on_not_smi,
                    Label::Distance near_jump = Label::kFar);

  // Operations on tagged smi values.

  // Smis represent a subset of integers. The subset is always equivalent to
  // a two's complement interpretation of a fixed number of bits.

  // Add an integer constant to a tagged smi, giving a tagged smi as result.
  // No overflow testing on the result is done.
  V8_REG_CLOBBERS(kScratchRegister)
  void SmiAddConstant(Operand dst, Tagged<Smi> constant);

  // Specialized operations

  // Converts, if necessary, a smi to a combination of number and
  // multiplier to be used as a scaled index.
  // The src register contains a *positive* smi value. The shift is the
  // power of two to multiply the index value by (e.g. to index by
  // smi-value * kSystemPointerSize, pass the smi and kSystemPointerSizeLog2).
  // The returned index register may be either src or dst, depending
  // on what is most efficient. If src and dst are different registers,
  // src is always unchanged.
  SmiIndex SmiToIndex(V8_REG_OUT Register dst, V8_REG_IN Register src,
                      int shift);

  void JumpIf(Condition cond, V8_REG_IN Register a, int32_t b,
              V8_ASM_BRANCH Label* dest) {
    cmpl(a, Immediate(b));
    j(cond, dest);
  }

  void JumpIfEqual(V8_REG_IN Register a, int32_t b, V8_ASM_BRANCH Label* dest) {
    JumpIf(equal, a, b, dest);
  }

  void JumpIfLessThan(V8_REG_IN Register a, int32_t b,
                      V8_ASM_BRANCH Label* dest) {
    JumpIf(less, a, b, dest);
  }

  void JumpIfUnsignedLessThan(V8_REG_IN Register a, int32_t b,
                              V8_ASM_BRANCH Label* dest) {
    JumpIf(below, a, b, dest);
  }

  // Caution: if {reg} is a 32-bit negative int, it should be sign-extended to
  // 64-bit before calling this function.
  void Switch(V8_REG_SCRATCH Register scrach, V8_REG_IN_CLOBBER Register reg,
              int case_base_value, Label** labels, int num_labels);

#ifdef V8_MAP_PACKING
  void UnpackMapWord(V8_REG_INOUT Register r);
#endif

  void LoadMap(V8_REG_OUT Register destination, V8_REG_IN Register object);
  void LoadCompressedMap(V8_REG_OUT Register destination,
                         V8_REG_IN Register object);

  V8_REG_CLOBBERS(rcx)
  void LoadFeedbackVector(V8_REG_OUT Register dst, V8_REG_IN Register closure,
                          V8_ASM_BRANCH Label* fbv_undef,
                          Label::Distance distance);
  void LoadFeedbackCell(V8_REG_OUT Register dst, V8_REG_IN Register closure);
  void LoadFeedbackVectorFromCell(V8_REG_OUT Register dst,
                                  V8_REG_IN Register feedback_cell,
                                  V8_REG_SCRATCH Register scratch,
                                  V8_ASM_BRANCH Label* fbv_undef,
                                  Label::Distance distance);

  void LoadInterpreterDataBytecodeArray(V8_REG_OUT Register destination,
                                        V8_REG_IN Register interpreter_data);
  void LoadInterpreterDataInterpreterTrampoline(
      V8_REG_OUT Register destination, V8_REG_IN Register interpreter_data);

  void Move(V8_REG_OUT Register dst, intptr_t x) {
    if (x == 0) {
      xorl(dst, dst);
      // The following shorter sequence for uint8 causes performance
      // regressions:
      // xorl(dst, dst); movb(dst, Immediate(static_cast<uint32_t>(x)));
    } else if (is_uint32(x)) {
      movl(dst, Immediate(static_cast<uint32_t>(x)));
    } else if (is_int32(x)) {
      // "movq reg64, imm32" is sign extending.
      movq(dst, Immediate(static_cast<int32_t>(x)));
    } else {
      movq(dst, Immediate64(x));
    }
  }
  V8_REG_CLOBBERS(kScratchRegister) void Move(Operand dst, intptr_t x);
  void Move(V8_REG_OUT Register dst, Tagged<Smi> source);

  V8_REG_CLOBBERS(kScratchRegister)
  void Move(Operand dst, Tagged<Smi> source) {
    Register constant = GetSmiConstant(source);
    movq(dst, constant);
  }

  void Move(V8_REG_OUT Register dst, Tagged<TaggedIndex> source) {
    Move(dst, source.ptr());
  }

  V8_REG_CLOBBERS(kScratchRegister)
  void Move(Operand dst, Tagged<TaggedIndex> source) {
    Move(dst, source.ptr());
  }

  void Move(V8_REG_OUT Register dst, ExternalReference ext);

  V8_REG_CLOBBERS(kScratchRegister) void Move(XMMRegister dst, uint32_t src);
  V8_REG_CLOBBERS(kScratchRegister) void Move(XMMRegister dst, uint64_t src);
  V8_REG_CLOBBERS(kScratchRegister) void Move(XMMRegister dst, float src) {
    Move(dst, base::bit_cast<uint32_t>(src));
  }
  V8_REG_CLOBBERS(kScratchRegister) void Move(XMMRegister dst, double src) {
    Move(dst, base::bit_cast<uint64_t>(src));
  }
  V8_REG_CLOBBERS(kScratchRegister)
  void Move(XMMRegister dst, uint64_t high, uint64_t low);

  // Move if the registers are not identical.
  void Move(V8_REG_OUT Register target, V8_REG_IN Register source);
  void Move(XMMRegister target, XMMRegister source);

  void Move(V8_REG_OUT Register target, Operand source);
  void Move(V8_REG_OUT Register target, Immediate source);

  void Move(V8_REG_OUT Register dst, Handle<HeapObject> source,
            RelocInfo::Mode rmode = RelocInfo::FULL_EMBEDDED_OBJECT);
  V8_REG_CLOBBERS(kScratchRegister)
  void Move(Operand dst, Handle<HeapObject> source,
            RelocInfo::Mode rmode = RelocInfo::FULL_EMBEDDED_OBJECT);

  // Loads a pointer into a register with a relocation mode.
  void Move(V8_REG_OUT Register dst, Address ptr, RelocInfo::Mode rmode) {
    // This method must not be used with heap object references. The stored
    // address is not GC safe. Use the handle version instead.
    DCHECK(rmode == RelocInfo::NO_INFO || rmode > RelocInfo::LAST_GCED_ENUM);
    movq(dst, Immediate64(ptr, rmode));
  }

  // Move src0 to dst0 and src1 to dst1, handling possible overlaps.
  void MovePair(V8_REG_OUT Register dst0, V8_REG_IN Register src0,
                V8_REG_OUT Register dst1, V8_REG_IN Register src1);

  // Convert smi to word-size sign-extended value.
  void SmiUntag(V8_REG_INOUT Register reg);
  void SmiUntagUnsigned(V8_REG_INOUT Register reg);
  // Requires dst != src
  void SmiUntag(V8_REG_OUT Register dst, V8_REG_IN Register src);
  void SmiUntag(V8_REG_OUT Register dst, Operand src);
  void SmiUntagUnsigned(V8_REG_OUT Register dst, Operand src);

  // Convert smi to 32-bit value.
  void SmiToInt32(V8_REG_INOUT Register reg);
  void SmiToInt32(V8_REG_OUT Register dst, V8_REG_IN Register src);

  // Loads the address of the external reference into the destination
  // register.
  void LoadAddress(V8_REG_OUT Register destination, ExternalReference source);

  void LoadFromConstantsTable(V8_REG_OUT Register destination,
                              int constant_index) final;
  void LoadRootRegisterOffset(V8_REG_OUT Register destination,
                              intptr_t offset) final;
  void LoadRootRelative(V8_REG_OUT Register destination, int32_t offset) final;
  void StoreRootRelative(int32_t offset, V8_REG_IN Register value) final;

  MemOperand AsMemOperand(IsolateFieldId id) {
    DCHECK(root_array_available());
    return MemOperand(kRootRegister, IsolateData::GetOffset(id));
  }

  // Operand pointing to an external reference.
  // May emit code to set up the scratch register. The operand is
  // only guaranteed to be correct as long as the scratch register
  // isn't changed.
  // If the operand is used more than once, use a scratch register
  // that is guaranteed not to be clobbered.
  Operand ExternalReferenceAsOperand(
      ExternalReference reference,
      V8_REG_OUT Register scratch = kScratchRegister);

  Operand ExternalReferenceAsOperand(IsolateFieldId id) {
    return ExternalReferenceAsOperand(ExternalReference::Create(id), no_reg);
  }

  V8_REG_CLOBBERS_CALLER_SAVED void Call(V8_REG_IN Register reg) { call(reg); }
  V8_REG_CLOBBERS_CALLER_SAVED void Call(Operand op);
  V8_REG_CLOBBERS_CALLER_SAVED void Call(Handle<Code> code_object,
                                         RelocInfo::Mode rmode);
  V8_REG_CLOBBERS_CALLER_SAVED void Call(Address destination,
                                         RelocInfo::Mode rmode);
  V8_REG_CLOBBERS_CALLER_SAVED void Call(ExternalReference ext);
  V8_REG_CLOBBERS_CALLER_SAVED void Call(V8_ASM_BRANCH Label* target) {
    call(target);
  }

  Operand EntryFromBuiltinAsOperand(Builtin builtin_index);
  V8_REG_CLOBBERS(kScratchRegister)
  Operand EntryFromBuiltinIndexAsOperand(V8_REG_IN Register builtin_index);
  V8_REG_CLOBBERS_CALLER_SAVED void CallBuiltinByIndex(
      V8_REG_IN Register builtin_index);
  V8_REG_CLOBBERS_CALLER_SAVED void CallBuiltin(Builtin builtin);
  V8_ASM_NO_FALLTHROUGH void TailCallBuiltin(Builtin builtin);
  V8_REG_CLOBBERS(kScratchRegister)
  void TailCallBuiltin(Builtin builtin, Condition cc);

  // Load the code entry point from the Code object.
  V8_REG_CLOBBERS(kScratchRegister)
  void LoadCodeInstructionStart(V8_REG_OUT Register destination,
                                V8_REG_IN Register code_object,
                                CodeEntrypointTag tag);
  V8_REG_CLOBBERS_CALLER_SAVED void CallCodeObject(
      V8_REG_IN_CLOBBER Register code_object, CodeEntrypointTag tag);
  V8_ASM_NO_FALLTHROUGH V8_REG_CLOBBERS(kScratchRegister) void JumpCodeObject(
      V8_REG_IN_CLOBBER Register code_object, CodeEntrypointTag tag,
      JumpMode jump_mode = JumpMode::kJump);

  // Convenience functions to call/jmp to the code of a JSFunction object.
  // TODO(42204201): These don't work properly with leaptiering as we need to
  // validate the parameter count at runtime. Instead, we should replace them
  // with CallJSDispatchEntry that generates a call to a given (compile-time
  // constant) JSDispatchHandle.
  V8_REG_CLOBBERS_CALLER_SAVED V8_REG_CLOBBERS(r15) void CallJSFunction(
      V8_REG_IN Register function_object, uint16_t expected_parameter_count);
  V8_ASM_NO_FALLTHROUGH void JumpJSFunction(
      V8_REG_IN Register function_object, JumpMode jump_mode = JumpMode::kJump);
  V8_REG_CLOBBERS_CALLER_SAVED V8_REG_CLOBBERS(r15) void CallJSDispatchEntry(
      JSDispatchHandle dispatch_handle, uint16_t argument_count);
#ifdef V8_ENABLE_WEBASSEMBLY
  V8_REG_CLOBBERS_CALLER_SAVED void CallWasmCodePointer(
      V8_REG_IN_CLOBBER Register target, uint64_t signature_hash,
      CallJumpMode call_jump_mode = CallJumpMode::kCall);
  V8_REG_CLOBBERS_CALLER_SAVED void CallWasmCodePointerNoSignatureCheck(
      V8_REG_IN_CLOBBER Register target);
  void LoadWasmCodePointer(V8_REG_OUT Register dst, Operand src);
#endif

  V8_ASM_NO_FALLTHROUGH V8_REG_CLOBBERS(kScratchRegister) void Jump(
      Address destination, RelocInfo::Mode rmode);
  V8_REG_CLOBBERS(kScratchRegister)
  void Jump(Address destination, RelocInfo::Mode rmode, Condition cc);
  V8_ASM_NO_FALLTHROUGH void Jump(const ExternalReference& reference);
  V8_ASM_NO_FALLTHROUGH void Jump(Operand op);
  void Jump(Operand op, Condition cc);
  V8_ASM_NO_FALLTHROUGH V8_REG_CLOBBERS(kScratchRegister) void Jump(
      Handle<Code> code_object, RelocInfo::Mode rmode);
  V8_REG_CLOBBERS(kScratchRegister)
  void Jump(Handle<Code> code_object, RelocInfo::Mode rmode, Condition cc);

  void AssertNotDeoptimized(V8_REG_SCRATCH Register scratch);
  V8_REG_CLOBBERS_CALLER_SAVED void CallForDeoptimization(
      Builtin target, int deopt_id, Label* exit, DeoptimizeKind kind,
      Label* ret, Label* jump_deoptimization_entry_label);

  V8_ASM_NO_FALLTHROUGH void Trap();
  void DebugBreak();

  void CompareRoot(V8_REG_IN Register with, RootIndex index,
                   ComparisonMode mode = ComparisonMode::kDefault);
  void CompareTaggedRoot(V8_REG_IN Register with, RootIndex index);
  V8_REG_CLOBBERS(kScratchRegister)
  void CompareRoot(Operand with, RootIndex index);

#ifdef V8_ENABLE_APX_F
  template <typename Dst, typename Src>
  void Ccmp(V8_REG_IN Dst lhs, V8_REG_IN Src rhs, OszcFlags dfv, Condition cond,
            int size) {
    switch (size) {
      case kInt8Size:
        ccmpb(lhs, rhs, dfv, cond);
        break;
      case kInt16Size:
        ccmpw(lhs, rhs, dfv, cond);
        break;
      case kInt32Size:
        ccmpl(lhs, rhs, dfv, cond);
        break;
      case kInt64Size:
        ccmpq(lhs, rhs, dfv, cond);
        break;
      default:
        UNREACHABLE();
    }
  }

  template <typename Dst, typename Src>
  void Ctest(V8_REG_IN Dst lhs, V8_REG_IN Src rhs, OszcFlags dfv,
             Condition cond, int size) {
    switch (size) {
      case kInt8Size:
        ctestb(lhs, rhs, dfv, cond);
        break;
      case kInt16Size:
        ctestw(lhs, rhs, dfv, cond);
        break;
      case kInt32Size:
        ctestl(lhs, rhs, dfv, cond);
        break;
      case kInt64Size:
        ctestq(lhs, rhs, dfv, cond);
        break;
      default:
        UNREACHABLE();
    }
  }
#endif

  // Generates function and stub prologue code.
  void StubPrologue(StackFrame::Type type);
  void Prologue();

  // Helpers for argument handling.

  // Pushes a single tagged-safe (Smi zero) padding slot. Callers decide
  // whether a padding slot is needed, usually via ShouldPadArguments().
  void PushTaggedStackPadding();
  // Drops a single padding slot. Does not clobber EFLAGS.
  void DropStackPadding();

  // Pushes a single stack argument, preceded by a padding slot when the
  // target requires 16-byte stack alignment. Intended for the common
  // "Push(arg); CallRuntime(fid, 1)" pattern.
  void PushArgument(V8_REG_IN Register arg);
  void PushArgument(Operand arg);

  // Drops {count} arguments and the return address stays on top of the stack.
  // With 16-byte stack alignment, also drops the padding slot that the caller
  // pushed if {count} is odd. {count} is not modified.
  void DropArguments(V8_REG_IN Register count, V8_REG_SCRATCH Register scratch);
  // Drops {argc} (padded, see above) arguments and pushes {receiver} as the
  // single (padded) stack argument for the following call.
  void DropArgumentsAndPushNewReceiver(V8_REG_IN Register argc,
                                       V8_REG_IN Register receiver,
                                       V8_REG_SCRATCH Register scratch);
  void DropArgumentsAndPushNewReceiver(V8_REG_IN Register argc,
                                       Operand receiver,
                                       V8_REG_SCRATCH Register scratch);

  // Calls Abort(msg) if the condition cc is not satisfied.
  // Use --debug_code to enable.
  void Assert(Condition cc, AbortReason reason) NOOP_UNLESS_DEBUG_CODE;

  // Like Assert(), but without condition.
  // Use --debug_code to enable.
  void AssertUnreachable(AbortReason reason) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if a 64 bit register containing a 32 bit payload does not
  // have zeros in the top 32 bits, enabled via --debug-code.
  V8_REG_CLOBBERS(kScratchRegister)
  void AssertZeroExtended(V8_REG_IN Register reg) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if the sign bit of smi register with pointer compression
  // is not zero, enabled via --slow-debug-code.
  void AssertSignBitOfSmiIsZero(V8_REG_IN Register smi) NOOP_UNLESS_DEBUG_CODE;

  // Like Assert(), but always enabled.
  void Check(Condition cc, AbortReason reason);

  // Same as Check() but expresses that the check is needed for the sandbox.
  void SbxCheck(Condition cc, AbortReason reason);

  // Compare instance type for map.
  // Always use unsigned comparisons: above and below, not less and greater.
  void CmpInstanceType(V8_REG_IN Register map, InstanceType type);

  // Abort execution if argument is not a Map, enabled via
  // --debug-code.
  void AssertMap(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if argument is not a Code, enabled via
  // --debug-code.
  void AssertCode(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if argument is not smi nor in the main pointer compression
  // cage, enabled via --debug-code.
  void AssertSmiOrHeapObjectInMainCompressionCage(V8_REG_IN Register object)
      NOOP_UNLESS_DEBUG_CODE;

  // Print a message to stdout and abort execution.
  V8_ASM_NO_FALLTHROUGH V8_REG_CLOBBERS_CALLER_SAVED void Abort(
      AbortReason msg);

  void CheckStackAlignment();

  // Wrapper around CheckStackAlignment. This is used at call sites
  // to enforce rsp is correctly aligned and thus rbp is aligned within
  // the callee. This function can be removed and usages replaced with
  // CheckStackAlignment when V8_X64_16BYTE_STACK_ALIGNMENT_BOOL is enabled
  // by default.
  void AssertSpAlignedForCall() NOOP_UNLESS_DEBUG_CODE;

  void AssertFPCRState() NOOP_UNLESS_DEBUG_CODE;
  void CanonicalizeNaN(const DoubleRegister dst, const DoubleRegister src);
  void CanonicalizeNaN(const DoubleRegister reg) { CanonicalizeNaN(reg, reg); }

  void AlignStackPointer();

  // Activation support.
  void EnterFrame(StackFrame::Type type);
  void EnterFrame(StackFrame::Type type, bool load_constant_pool_pointer_reg) {
    // Out-of-line constant pool not implemented on x64.
    UNREACHABLE();
  }
  void LeaveFrame(StackFrame::Type type);

// Allocate stack space of given size (i.e. decrement {rsp} by the value
// stored in the given register, or by a constant). If you need to perform a
// stack check, do it before calling this function because this function may
// write into the newly allocated space. It may also overwrite the given
// register's value, in the version that takes a register.
#if defined(V8_TARGET_OS_WIN) || defined(V8_TARGET_OS_MACOS)
  void AllocateStackSpace(V8_REG_IN_CLOBBER Register bytes_scratch);
  void AllocateStackSpace(int bytes);
#else
  void AllocateStackSpace(V8_REG_IN Register bytes) { subq(rsp, bytes); }
  void AllocateStackSpace(int bytes) {
    DCHECK_GE(bytes, 0);
    if (bytes == 0) return;
    subq(rsp, Immediate(bytes));
  }
#endif

  void InitializeRootRegister() {
    ExternalReference isolate_root = ExternalReference::isolate_root(isolate());
    Move(kRootRegister, isolate_root);
#ifdef V8_COMPRESS_POINTERS
    LoadRootRelative(kPtrComprCageBaseRegister,
                     IsolateData::cage_base_offset());
#endif
  }

  V8_REG_CLOBBERS(kScratchRegister)
  void CallEphemeronKeyBarrier(V8_REG_IN Register object,
                               V8_REG_IN Register slot_address,
                               SaveFPRegsMode fp_mode);

  V8_REG_CLOBBERS(kScratchRegister)
  void CallTrustedPointerBarrier(V8_REG_IN Register object,
                                 V8_REG_IN Register slot_address,
                                 SaveFPRegsMode fp_mode, TrustedPointerTag tag);

  V8_REG_CLOBBERS(kScratchRegister)
  void CallRecordWriteStubSaveRegisters(
      V8_REG_IN Register object, V8_REG_IN Register slot_address,
      SaveFPRegsMode fp_mode,
      StubCallMode mode = StubCallMode::kCallBuiltinPointer);
  V8_REG_CLOBBERS(kScratchRegister)
  void CallRecordWriteStub(
      V8_REG_IN Register object, V8_REG_IN Register slot_address,
      SaveFPRegsMode fp_mode,
      StubCallMode mode = StubCallMode::kCallBuiltinPointer);

  V8_REG_CLOBBERS(kScratchRegister)
  void CallVerifySkippedWriteBarrierStubSaveRegisters(V8_REG_IN Register object,
                                                      V8_REG_IN Register value,
                                                      SaveFPRegsMode fp_mode);
  V8_REG_CLOBBERS_CALLER_SAVED void CallVerifySkippedWriteBarrierStub(
      V8_REG_IN Register object, V8_REG_IN Register value);

  V8_REG_CLOBBERS(kScratchRegister)
  void CallVerifySkippedTrustedWriteBarrierStubSaveRegisters(
      V8_REG_IN Register object, V8_REG_IN Register value,
      SaveFPRegsMode fp_mode);
  V8_REG_CLOBBERS_CALLER_SAVED void CallVerifySkippedTrustedWriteBarrierStub(
      V8_REG_IN Register object, V8_REG_IN Register value);

#ifdef V8_IS_TSAN
  V8_REG_CLOBBERS(kScratchRegister)
  void CallTSANStoreStub(V8_REG_IN Register address, V8_REG_IN Register value,
                         SaveFPRegsMode fp_mode, int size, StubCallMode mode,
                         std::memory_order order);
  V8_REG_CLOBBERS(kScratchRegister)
  void CallTSANRelaxedLoadStub(
      V8_REG_IN Register address,
      std::optional<SharedBaseTsanArgument> opt_shared_base,
      SaveFPRegsMode fp_mode, int size, StubCallMode mode);
#endif  // V8_IS_TSAN

  void MoveNumber(V8_REG_OUT Register dst, double value);
  void MoveNonSmi(V8_REG_OUT Register dst, double value);

  // Calculate how much stack space (in bytes) are required to store caller
  // registers excluding those specified in the arguments.
  int RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                      Register exclusion = no_reg) const;

  // PushCallerSaved and PopCallerSaved do not arrange the registers in any
  // particular order so they are not useful for calls that can cause a GC.
  // The caller can exclude a register that does not need to be saved and
  // restored.

  // Push caller saved registers on the stack, and return the number of bytes
  // stack pointer is adjusted.
  int PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion = no_reg);
  // Restore caller saved registers from the stack, and return the number of
  // bytes stack pointer is adjusted.
  int PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion = no_reg);

  int PushAll(RegList registers);
  int PopAll(RegList registers);

  int PushAll(DoubleRegList registers,
              int stack_slot_size = kStackSavedSavedFPSize);
  int PopAll(DoubleRegList registers,
             int stack_slot_size = kStackSavedSavedFPSize);

  // Compute the start of the generated instruction stream from the current PC.
  // This is an alternative to embedding the {CodeObject} handle as a reference.
  void ComputeCodeStartAddress(V8_REG_OUT Register dst);

  // Control-flow integrity:

  // Define a function entrypoint which will emit a landing pad instruction if
  // required by the build config.
  void CodeEntry();
  // Define an exception handler.
  void ExceptionHandler();
  // Define an exception handler and bind a label.
  void BindExceptionHandler(V8_ASM_BIND Label* label) { BindJumpTarget(label); }
  // Bind a jump target and mark it as a valid code entry.
  void BindJumpTarget(V8_ASM_BIND Label* label) {
    bind(label);
    CodeEntry();
  }

  // ---------------------------------------------------------------------------
  // Pointer compression support

  // Loads a field containing any tagged value and decompresses it if necessary.
  void LoadTaggedField(V8_REG_OUT Register destination, Operand field_operand);

  // Loads a field containing any tagged value but does not decompress it when
  // pointer compression is enabled.
  void LoadTaggedField(V8_REG_OUT TaggedRegister destination,
                       Operand field_operand);
  void LoadTaggedFieldWithoutDecompressing(V8_REG_OUT Register destination,
                                           Operand field_operand);

  // Loads a field containing a Smi and decompresses it if pointer compression
  // is enabled.
  void LoadTaggedSignedField(V8_REG_OUT Register destination,
                             Operand field_operand);

  // Loads a field containing any tagged value, decompresses it if necessary and
  // pushes the full pointer to the stack. When pointer compression is enabled,
  // uses |scratch| to decompress the value.
  void PushTaggedField(Operand field_operand, V8_REG_SCRATCH Register scratch);

  // Loads a field containing smi value and untags it.
  void SmiUntagField(V8_REG_OUT Register dst, Operand src);
  void SmiUntagFieldUnsigned(V8_REG_OUT Register dst, Operand src);

  // Compresses tagged value if necessary and stores it to given on-heap
  // location.
  void StoreTaggedField(Operand dst_field_operand, Immediate immediate);
  void StoreTaggedField(Operand dst_field_operand, V8_REG_IN Register value);
  V8_REG_CLOBBERS(kScratchRegister)
  void StoreTaggedSignedField(Operand dst_field_operand, Tagged<Smi> value);
  V8_REG_CLOBBERS(kScratchRegister)
  void AtomicStoreTaggedField(Operand dst_field_operand,
                              V8_REG_IN Register value);

  // The following macros work even when pointer compression is not enabled.
  void DecompressTaggedSigned(V8_REG_OUT Register destination,
                              Operand field_operand);
  void DecompressTagged(V8_REG_OUT Register destination, Operand field_operand);
  void DecompressTagged(V8_REG_OUT Register destination,
                        V8_REG_IN Register source);
  void DecompressTagged(V8_REG_OUT Register destination, Tagged_t immediate);
  void DecompressProtected(V8_REG_OUT Register destination,
                           Operand field_operand);

  // ---------------------------------------------------------------------------
  // V8 Sandbox support

  // Enter/exit sandboxed execution mode for the current thread.
  //
  // When in sandboxed mode, and if hardware sandboxing support is active,
  // out-of-sandbox memory cannot be written to (but can be read from). See
  // SandboxHardwareSupport for more details.
  void EnterSandbox();
  void ExitSandbox();
  void AssertInSandboxedExecutionMode();

  // Helper functions for temporarily switching sandboxed execution mode if the
  // current code runs in the a different sandboxing mode than the call target.
  //
  // These are mostly useful inside shared routines that are used for both
  // sandboxed- and unsandboxed code. Examples include CallBuiltin and
  // CallCFunction which may both need to temporarily switch out of sandboxed
  // execution mode.
  // SwitchSandboxingModeBeforeCallIfNeeded will return the old sandboxing mode
  // which must then be passed into SwitchSandboxingModeAfterCallIfNeeded.
  // These function will also change the global sandboxing_mode(). As such,
  // there must not be any control-flow transfers in between the two function
  // calls.
  //
  // TODO(428152530): In the future, we might want to replace this mechanism
  // and instead use dedicated trampolines that perform the mode switching.
  void SwitchSandboxingModeTo(CodeSandboxingMode mode);
  CodeSandboxingMode SwitchSandboxingModeBeforeCallIfNeeded(
      CodeSandboxingMode target_sandboxing_mode);
  void SwitchSandboxingModeAfterCallIfNeeded(
      CodeSandboxingMode previous_sandboxing_mode);

  // Transform a SandboxedPointer from/to its encoded form, which is used when
  // the pointer is stored on the heap and ensures that the pointer will always
  // point into the sandbox.
  void EncodeSandboxedPointer(V8_REG_INOUT Register value);
  void DecodeSandboxedPointer(V8_REG_INOUT Register value);

  // Load and decode a SandboxedPointer from the heap.
  void LoadSandboxedPointerField(V8_REG_OUT Register destination,
                                 Operand field_operand);
  // Encode and store a SandboxedPointer to the heap.
  V8_REG_CLOBBERS(kScratchRegister)
  void StoreSandboxedPointerField(Operand dst_field_operand,
                                  V8_REG_IN Register value);

  enum class IsolateRootLocation { kInScratchRegister, kInRootRegister };
  // Loads a field containing off-heap pointer and does necessary decoding
  // if pointer compression is enabled.
  void LoadExternalPointerField(V8_REG_OUT Register destination,
                                Operand field_operand,
                                ExternalPointerTagRange tag_range,
                                V8_REG_IN_CLOBBER Register scratch,
                                IsolateRootLocation isolateRootLocation =
                                    IsolateRootLocation::kInRootRegister);

  // Load a trusted pointer field.
  // When the sandbox is enabled, these are indirect pointers using the trusted
  // pointer table. Otherwise they are regular tagged fields.
  V8_REG_CLOBBERS(kScratchRegister)
  void LoadTrustedPointerField(V8_REG_OUT Register destination,
                               Operand field_operand,
                               TrustedPointerTagRange tag_range,
                               V8_REG_SCRATCH Register scratch);
  // As above, but for kUnknownTrustedPointerTag. The type of the loaded object
  // is unknown, so this helper will check for a series of expected types and
  // jump to the given labels if the loaded object has a matching type. If the
  // field is null (with enabled sandbox) or a Smi (with disabled sandbox) and
  // the provided is_unavailable label is not a nullptr, then the helper will
  // jump there. If the field is valid and the object has one of the expected
  // types, then the helper will jump to the corresponding label. In all other
  // cases, the destination register will be zeroed and execution continues as
  // fall-through.
  V8_REG_CLOBBERS(kScratchRegister)
  void LoadTrustedUnknownPointerField(
      V8_REG_OUT Register destination, Operand field_operand,
      V8_REG_SCRATCH Register scratch,
      const std::initializer_list<
          std::tuple<InstanceType, Label*, Label::Distance>>& cases,
      V8_ASM_BRANCH Label* is_unavailable = nullptr);
  // Store a trusted pointer field.
  V8_REG_CLOBBERS(kScratchRegister)
  void StoreTrustedPointerField(Operand dst_field_operand,
                                V8_REG_IN Register value);

  // Load a code pointer field.
  // These are special versions of trusted pointers that, when the sandbox is
  // enabled, reference code objects through the trusted pointer table.
  V8_REG_CLOBBERS(kScratchRegister)
  void LoadCodePointerField(V8_REG_OUT Register destination,
                            Operand field_operand,
                            V8_REG_SCRATCH Register scratch) {
    LoadTrustedPointerField(destination, field_operand, kCodeTrustedPointerTag,
                            scratch);
  }
  // Store a code pointer field.
  V8_REG_CLOBBERS(kScratchRegister)
  void StoreCodePointerField(Operand dst_field_operand,
                             V8_REG_IN Register value) {
    StoreTrustedPointerField(dst_field_operand, value);
  }

#ifdef V8_ENABLE_SANDBOX
  // Retrieve the heap object referenced by the given trusted pointer handle.
  V8_REG_CLOBBERS(kScratchRegister)
  void ResolveTrustedPointerHandle(V8_REG_OUT Register destination,
                                   V8_REG_IN_CLOBBER Register handle,
                                   TrustedPointerTagRange tag_range);
#endif  // V8_ENABLE_SANDBOX

  V8_REG_CLOBBERS(kScratchRegister)
  void LoadEntrypointFromJSDispatchTable(V8_REG_OUT Register destination,
                                         V8_REG_IN Register dispatch_handle);
  V8_REG_CLOBBERS(kScratchRegister)
  void LoadParameterCountFromJSDispatchTable(
      V8_REG_OUT Register destination, V8_REG_IN Register dispatch_handle);
  V8_REG_CLOBBERS(kScratchRegister)
  void LoadEntrypointAndParameterCountFromJSDispatchTable(
      V8_REG_OUT Register entrypoint, V8_REG_OUT Register parameter_count,
      V8_REG_IN Register dispatch_handle);
  V8_REG_CLOBBERS(kScratchRegister)
  void PushDispatchHandle(V8_REG_INOUT Register dispatch_handle,
                          V8_REG_SCRATCH Register scratch);
  V8_REG_CLOBBERS(kScratchRegister)
  void PopDispatchHandle(V8_REG_OUT Register dispatch_handle,
                         V8_REG_SCRATCH Register scratch);

  void LoadProtectedPointerField(V8_REG_OUT Register destination,
                                 Operand field_operand);

  // Loads and stores the value of an external reference.
  // Special case code for load and store to take advantage of
  // load_rax/store_rax if possible/necessary.
  // For other operations, just use:
  //   Operand operand = ExternalReferenceAsOperand(extref);
  //   operation(operand, ..);
  V8_REG_CLOBBERS(kScratchRegister)
  void Load(V8_REG_OUT Register destination, ExternalReference source);
  V8_REG_CLOBBERS(kScratchRegister)
  void Store(ExternalReference destination, V8_REG_IN Register source);

  // Pushes the address of the external reference onto the stack.
  void PushAddress(ExternalReference source, V8_REG_SCRATCH Register scratch);

  // Operations on roots in the root-array.
  // Load a root value where the index (or part of it) is variable.
  // The variable_offset register is added to the fixed_offset value
  // to get the index into the root-array.
  void PushRoot(RootIndex index);

  // Compare the object in a register to a value and jump if they are equal.
  void JumpIfRoot(V8_REG_IN Register with, RootIndex index,
                  V8_ASM_BRANCH Label* if_equal,
                  Label::Distance if_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(equal, if_equal, if_equal_distance);
  }
  V8_REG_CLOBBERS(kScratchRegister)
  void JumpIfRoot(Operand with, RootIndex index, V8_ASM_BRANCH Label* if_equal,
                  Label::Distance if_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(equal, if_equal, if_equal_distance);
  }

  // Compare the object in a register to a value and jump if they are not equal.
  void JumpIfNotRoot(V8_REG_IN Register with, RootIndex index,
                     V8_ASM_BRANCH Label* if_not_equal,
                     Label::Distance if_not_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(not_equal, if_not_equal, if_not_equal_distance);
  }
  V8_REG_CLOBBERS(kScratchRegister)
  void JumpIfNotRoot(Operand with, RootIndex index,
                     V8_ASM_BRANCH Label* if_not_equal,
                     Label::Distance if_not_equal_distance = Label::kFar) {
    CompareRoot(with, index);
    j(not_equal, if_not_equal, if_not_equal_distance);
  }

  // ---------------------------------------------------------------------------
  // GC Support

  // Performs a fast check for whether `value` is a read-only object or a small
  // Smi. Only enabled in some configurations.
  void MaybeJumpIfReadOnlyOrSmallSmi(V8_REG_IN Register value,
                                     V8_ASM_BRANCH Label* dest);

  // Notify the garbage collector that we wrote a pointer into an object.
  // |object| is the object being stored into, |value| is the object being
  // stored.  value and scratch registers are clobbered by the operation.
  // The offset is the offset from the start of the object, not the offset from
  // the tagged HeapObject pointer.  For use with FieldOperand(reg, off).
  V8_REG_CLOBBERS(kScratchRegister)
  void RecordWriteField(
      V8_REG_IN Register object, int offset, V8_REG_IN_CLOBBER Register value,
      V8_REG_SCRATCH Register slot_address, SaveFPRegsMode save_fp,
      SmiCheck smi_check = SmiCheck::kInline,
      ReadOnlyCheck ro_check = ReadOnlyCheck::kInline,
      SlotDescriptor slot = SlotDescriptor::ForDirectPointerSlot());

  // For page containing |object| mark region covering |address|
  // dirty. |object| is the object being stored into, |value| is the
  // object being stored. The address and value registers are clobbered by the
  // operation.  RecordWrite filters out smis so it does not update
  // the write barrier if the value is a smi.
  V8_REG_CLOBBERS(kScratchRegister)
  void RecordWrite(
      V8_REG_IN Register object, V8_REG_IN_CLOBBER Register slot_address,
      V8_REG_IN_CLOBBER Register value, SaveFPRegsMode save_fp,
      SmiCheck smi_check = SmiCheck::kInline,
      ReadOnlyCheck ro_check = ReadOnlyCheck::kInline,
      SlotDescriptor slot = SlotDescriptor::ForDirectPointerSlot());

  // Allocates an EXIT/BUILTIN_EXIT/API_CALLBACK_EXIT frame with given number
  // of slots in non-GCed area.
  void EnterExitFrame(int extra_slots, StackFrame::Type frame_type);
  V8_REG_CLOBBERS(rsi, rbp, kScratchRegister) void LeaveExitFrame();

  // ---------------------------------------------------------------------------
  // JavaScript invokes

  // The way we invoke JSFunctions differs depending on whether leaptiering is
  // enabled. As such, these functions exist in two variants. In the future,
  // leaptiering will be used on all platforms. At that point, the
  // non-leaptiering variants will disappear.

  // Invoke the JavaScript function code by either calling or jumping.
  V8_REG_CLOBBERS_CALLER_SAVED V8_REG_CLOBBERS(r15) void InvokeFunctionCode(
      V8_REG_IN Register function, V8_REG_IN Register new_target,
      V8_REG_IN Register actual_parameter_count, InvokeType type,
      ArgumentAdaptionMode argument_adaption_mode =
          ArgumentAdaptionMode::kAdapt);

  // Invoke the JavaScript function in the given register. Changes the
  // current context to the context in the function before invoking.
  V8_REG_CLOBBERS_CALLER_SAVED V8_REG_CLOBBERS(r15) void InvokeFunction(
      V8_REG_IN Register function, V8_REG_IN Register new_target,
      V8_REG_IN Register actual_parameter_count, InvokeType type,
      ArgumentAdaptionMode argument_adaption_mode =
          ArgumentAdaptionMode::kAdapt);

  // On function call, call into the debugger.
  V8_REG_CLOBBERS_CALLER_SAVED void CallDebugOnFunctionCall(
      V8_REG_INOUT Register fun, V8_REG_INOUT Register new_target,
      V8_REG_INOUT Register expected_parameter_count_or_dispatch_handle,
      V8_REG_INOUT Register actual_parameter_count);

  // ---------------------------------------------------------------------------
  // Macro instructions.

  V8_REG_CLOBBERS(kScratchRegister)
  void Cmp(V8_REG_IN Register dst, Handle<Object> source);
  V8_REG_CLOBBERS(kScratchRegister)
  void Cmp(Operand dst, Handle<Object> source);

  // Checks if value is in range [lower_limit, higher_limit] using a single
  // comparison. Flags CF=1 or ZF=1 indicate the value is in the range
  // (condition below_equal).
  V8_REG_CLOBBERS_IF(lower_limit != 0, kScratchRegister)
  void CompareRange(V8_REG_IN Register value, unsigned lower_limit,
                    unsigned higher_limit);
  V8_REG_CLOBBERS_IF(lower_limit != 0, kScratchRegister)
  void JumpIfIsInRange(V8_REG_IN Register value, unsigned lower_limit,
                       unsigned higher_limit, V8_ASM_BRANCH Label* on_in_range,
                       Label::Distance near_jump = Label::kFar);

  // Emit code to discard a non-negative number of pointer-sized elements
  // from the stack, clobbering only the rsp register.
  void Drop(int stack_elements);
  // Emit code to discard a positive number of pointer-sized elements
  // from the stack under the return address which remains on the top,
  // clobbering the rsp register.
  void DropUnderReturnAddress(
      int stack_elements, V8_REG_SCRATCH Register scratch = kScratchRegister);
  V8_ASM_PUSH void PushQuad(Operand src);
  V8_ASM_PUSH void PushImm32(int32_t imm32);
  V8_ASM_POP void Pop(V8_REG_OUT Register dst);
  V8_ASM_POP void Pop(Operand dst);
  V8_ASM_POP void PopQuad(Operand dst);

  // Compare object type for heap object.
  // Always use unsigned comparisons: above and below, not less and greater.
  // Incoming register is heap_object and outgoing register is map.
  // They may be the same register, and may be kScratchRegister.
  void CmpObjectType(V8_REG_IN Register heap_object, InstanceType type,
                     V8_REG_OUT Register map);
  // Variant of the above, which only guarantees to set the correct
  // equal/not_equal flag. Map might not be loaded.
  void IsObjectType(V8_REG_IN Register heap_object, InstanceType type,
                    V8_REG_SCRATCH Register scratch);
  // Variant of the above, which compares against a type range rather than a
  // single type (lower_limit and higher_limit are inclusive).
  //
  // Always use unsigned comparisons: below for a positive result.
  V8_REG_CLOBBERS(kScratchRegister)
  void IsObjectTypeInRange(V8_REG_IN Register heap_object, InstanceType low,
                           InstanceType high, V8_REG_SCRATCH Register scratch);
#if V8_STATIC_ROOTS_BOOL
  // Fast variant which is guaranteed to not actually load the instance type
  // from the map.
  void IsObjectTypeFast(V8_REG_IN Register heap_object, InstanceType type,
                        V8_REG_SCRATCH Register compressed_map_scratch);
  void CompareInstanceTypeWithUniqueCompressedMap(V8_REG_IN Register map,
                                                  InstanceType type);
#endif  // V8_STATIC_ROOTS_BOOL

  // Fast check if the object is a js receiver type. Assumes only primitive
  // objects or js receivers are passed.
  V8_REG_CLOBBERS(kScratchRegister)
  void JumpIfJSAnyIsNotPrimitive(
      V8_REG_IN Register heap_object, V8_REG_SCRATCH Register scratch,
      V8_ASM_BRANCH Label* target, Label::Distance distance = Label::kFar,
      Condition condition = Condition::kUnsignedGreaterThanEqual);
  V8_REG_CLOBBERS(kScratchRegister)
  void JumpIfJSAnyIsPrimitive(V8_REG_IN Register heap_object,
                              V8_REG_SCRATCH Register scratch,
                              V8_ASM_BRANCH Label* target,
                              Label::Distance distance = Label::kFar) {
    return JumpIfJSAnyIsNotPrimitive(heap_object, scratch, target, distance,
                                     Condition::kUnsignedLessThan);
  }

  // Compare instance type ranges for a map (low and high inclusive)
  // Always use unsigned comparisons: below_equal for a positive result.
  V8_REG_CLOBBERS_IF(low != 0, kScratchRegister)
  void CmpInstanceTypeRange(V8_REG_IN Register map,
                            V8_REG_OUT Register instance_type_out,
                            InstanceType low, InstanceType high);

  template <typename Field>
  void DecodeField(V8_REG_INOUT Register reg) {
    static const int shift = Field::kShift;
    static const int mask = Field::kMask >> Field::kShift;
    if (shift != 0) {
      shrq(reg, Immediate(shift));
    }
    andq(reg, Immediate(mask));
  }

  void TestCodeIsMarkedForDeoptimization(V8_REG_IN Register code);
  void TestCodeIsTurbofanned(V8_REG_IN Register code);
  Immediate ClearedValue() const;

  // Tiering support.
  void AssertFeedbackCell(V8_REG_IN Register object,
                          V8_REG_SCRATCH Register scratch)
      NOOP_UNLESS_DEBUG_CODE;
  void AssertFeedbackVector(V8_REG_IN Register object,
                            V8_REG_SCRATCH Register scratch)
      NOOP_UNLESS_DEBUG_CODE;
  // TODO(olivf): Rename to GenerateTailCallToUpdatedFunction.
  V8_ASM_NO_FALLTHROUGH V8_REG_CLOBBERS_CALLER_SAVED void
  GenerateTailCallToReturnedCode(Runtime::FunctionId function_id,
                                 JumpMode jump_mode = JumpMode::kJump);

  // Abort execution if argument is not a Constructor, enabled via --debug-code.
  void AssertConstructor(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if argument is not a JSFunction, enabled via --debug-code.
  V8_REG_CLOBBERS(kScratchRegister)
  void AssertFunction(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if argument is not a callable JSFunction, enabled via
  // --debug-code.
  V8_REG_CLOBBERS(kScratchRegister)
  void AssertCallableFunction(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if argument is not a JSBoundFunction,
  // enabled via --debug-code.
  void AssertBoundFunction(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if argument is not a JSGeneratorObject (or subclass),
  // enabled via --debug-code.
  V8_REG_CLOBBERS(kScratchRegister)
  void AssertGeneratorObject(V8_REG_IN Register object) NOOP_UNLESS_DEBUG_CODE;

  // Abort execution if argument is not undefined or an AllocationSite, enabled
  // via --debug-code.
  V8_REG_CLOBBERS(kScratchRegister)
  void AssertUndefinedOrAllocationSite(V8_REG_IN Register object)
      NOOP_UNLESS_DEBUG_CODE;

  V8_REG_CLOBBERS(kScratchRegister)
  void AssertJSAny(V8_REG_IN Register object, V8_REG_SCRATCH Register map_tmp,
                   AbortReason abort_reason) NOOP_UNLESS_DEBUG_CODE;

  // ---------------------------------------------------------------------------
  // Exception handling

  // Push a new stack handler and link it into stack handler chain.
  void PushStackHandler();

  // Unlink the stack handler on top of the stack from the stack handler chain.
  void PopStackHandler();

  // ---------------------------------------------------------------------------
  // Support functions.

  // Load the global proxy from the current context.
  void LoadGlobalProxy(V8_REG_OUT Register dst) {
    LoadNativeContextSlot(dst, Context::GLOBAL_PROXY_INDEX);
  }

  // Load the native context slot with the current index.
  void LoadNativeContextSlot(V8_REG_OUT Register dst, int index);

  // Falls through and sets scratch_and_result to 0 on failure, jumps to
  // on_result on success.
  V8_REG_CLOBBERS(kScratchRegister)
  void TryLoadOptimizedOsrCode(V8_REG_OUT Register scratch_and_result,
                               CodeKind min_opt_level,
                               V8_REG_IN Register feedback_vector,
                               FeedbackSlot slot,
                               V8_ASM_BRANCH Label* on_result,
                               Label::Distance distance);

  // ---------------------------------------------------------------------------
  // Runtime calls

  // Call a runtime routine.
  V8_REG_CLOBBERS_CALLER_SAVED void CallRuntime(const Runtime::Function* f,
                                                int num_arguments);

  // Convenience function: Same as above, but takes the fid instead.
  V8_REG_CLOBBERS_CALLER_SAVED void CallRuntime(Runtime::FunctionId fid) {
    const Runtime::Function* function = Runtime::FunctionForId(fid);
    CallRuntime(function, function->nargs);
  }

  // Convenience function: Same as above, but takes the fid instead.
  V8_REG_CLOBBERS_CALLER_SAVED void CallRuntime(Runtime::FunctionId fid,
                                                int num_arguments) {
    CallRuntime(Runtime::FunctionForId(fid), num_arguments);
  }

  // Convenience function: tail call a runtime routine (jump)
  V8_ASM_NO_FALLTHROUGH V8_REG_CLOBBERS(
      rax, rbx, kScratchRegister) void TailCallRuntime(Runtime::FunctionId fid);

  // Jump to a runtime routines
  V8_ASM_NO_FALLTHROUGH
  V8_REG_CLOBBERS(rbx, kScratchRegister)
  void JumpToExternalReference(const ExternalReference& ext,
                               bool builtin_exit_frame = false);

  // ---------------------------------------------------------------------------
  // StatsCounter support
  V8_REG_CLOBBERS(kScratchRegister)
  void IncrementCounter(StatsCounter* counter, int value) {
    if (!v8_flags.native_code_counters) return;
    EmitIncrementCounter(counter, value);
  }
  V8_REG_CLOBBERS(kScratchRegister)
  void EmitIncrementCounter(StatsCounter* counter, int value);
  V8_REG_CLOBBERS(kScratchRegister)
  void DecrementCounter(StatsCounter* counter, int value) {
    if (!v8_flags.native_code_counters) return;
    EmitDecrementCounter(counter, value);
  }
  V8_REG_CLOBBERS(kScratchRegister)
  void EmitDecrementCounter(StatsCounter* counter, int value);

  // ---------------------------------------------------------------------------
  // Stack limit utilities
  Operand StackLimitAsOperand(StackLimitKind kind);
  V8_REG_CLOBBERS(kScratchRegister)
  void StackOverflowCheck(
      V8_REG_IN Register num_args, V8_ASM_BRANCH Label* stack_overflow,
      Label::Distance stack_overflow_distance = Label::kFar);

  // ---------------------------------------------------------------------------
  // In-place weak references.
  void LoadWeakValue(V8_REG_INOUT Register in_out,
                     V8_ASM_BRANCH Label* target_if_cleared);

 protected:
  static const int kSmiShift = kSmiTagSize + kSmiShiftSize;

  // Returns a register holding the smi value. The register MUST NOT be
  // modified. It may be the "smi 1 constant" register.
  V8_REG_CLOBBERS(kScratchRegister) Register GetSmiConstant(Tagged<Smi> value);

  // Drops arguments assuming that the return address was already popped.
  // With 16-byte stack alignment, also drops the caller's padding slot if
  // {count} is odd.
  void DropArguments(V8_REG_IN Register count);

 private:
  // Helper functions for generating invokes.
  V8_REG_IMPLICIT_OUT(rax)
  V8_REG_CLOBBERS(r8, r9, r10, r11)
  void InvokePrologue(V8_REG_IN_CLOBBER Register expected_parameter_count,
                      V8_REG_IN Register actual_parameter_count,
                      InvokeType type);

  DISALLOW_IMPLICIT_CONSTRUCTORS(MacroAssembler);
};

// -----------------------------------------------------------------------------
// Static helper functions.

// Generate an Operand for loading a field from an object.
inline Operand FieldOperand(Register object, int offset) {
  return Operand(object, offset - kHeapObjectTag);
}

// For compatibility with platform-independent code.
inline MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}

// Generate an Operand for loading a field from an object. Object pointer is a
// compressed pointer when pointer compression is enabled.
inline Operand FieldOperand(TaggedRegister object, int offset) {
  if (COMPRESS_POINTERS_BOOL) {
    return Operand(kPtrComprCageBaseRegister, object.reg(),
                   ScaleFactor::times_1, offset - kHeapObjectTag);
  } else {
    return Operand(object.reg(), offset - kHeapObjectTag);
  }
}

// Generate an Operand for loading an indexed field from an object.
inline Operand FieldOperand(Register object, Register index, ScaleFactor scale,
                            int offset) {
  return Operand(object, index, scale, offset - kHeapObjectTag);
}

// Provides access to exit frame stack space (not GC-ed).
inline Operand ExitFrameStackSlotOperand(int offset) {
#ifdef V8_TARGET_OS_WIN
  return Operand(rsp, offset + kWindowsHomeStackSlots * kSystemPointerSize);
#else
  return Operand(rsp, offset);
#endif
}

// Provides access to exit frame parameters (GC-ed).
inline Operand ExitFrameCallerStackSlotOperand(int index) {
  return Operand(rbp,
                 (BuiltinExitFrameConstants::kFixedSlotCountAboveFp + index) *
                     kSystemPointerSize);
}

struct MoveCycleState {
  // Whether a move in the cycle needs the scratch or double scratch register.
  bool pending_scratch_register_use = false;
  bool pending_double_scratch_register_use = false;
};

// Calls an API function. Allocates HandleScope, extracts returned value
// from handle and propagates exceptions. Clobbers C argument registers
// and C caller-saved registers. Restores context. On return removes
//   (*argc_operand + slots_to_drop_on_return) * kSystemPointerSize
// (GCed, includes the call JS arguments space and the additional space
// allocated for the fast call).
void CallApiFunctionAndReturn(MacroAssembler* masm, bool with_profiling,
                              Register function_address,
                              ExternalReference thunk_ref, Register thunk_arg,
                              int slots_to_drop_on_return,
                              MemOperand* argc_operand,
                              MemOperand return_value_operand,
                              bool handle_interceptor_result);

#define ACCESS_MASM(masm) masm->

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_X64_MACRO_ASSEMBLER_X64_H_
