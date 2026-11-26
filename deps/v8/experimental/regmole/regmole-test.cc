// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <optional>

#include "src/baseline/baseline-assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/wasm/baseline/liftoff-register.h"

namespace v8 {
namespace internal {

#define __ masm->

// -----------------------------------------------------------------------------
// 1. Symbolic & Physical Register Use-After-Clobber Tests
// -----------------------------------------------------------------------------

void TestUseAfterScratchClobberBad(MacroAssembler* masm, Register obj,
                                   Register scratch) {
  __ IsObjectType(obj, JS_OBJECT_TYPE, scratch);
  // EXPECT-WARNING: Use of register 'scratch' after it was clobbered
  __ addq(obj, scratch);
}

void TestUseAfterScratchRedefinedGood(MacroAssembler* masm, Register obj,
                                      Register scratch) {
  __ IsObjectType(obj, JS_OBJECT_TYPE, scratch);
  __ movq(scratch, Immediate(42));
  __ addq(obj, scratch);
  __ IsObjectType(obj, JS_OBJECT_TYPE, scratch);
  __ xorq(scratch, scratch);
  __ addq(obj, scratch);
}

void TestUseAfterCallClobberBad(MacroAssembler* masm, Register val) {
  __ CallRuntime(Runtime::kAbort);
  // EXPECT-WARNING: Use of register 'val' after it was clobbered
  __ pushq(val);
}

void TestPushPopPreservesAcrossCallGood(MacroAssembler* masm, Register val) {
  __ pushq(val);
  __ CallRuntime(Runtime::kAbort);
  __ popq(val);
  __ incq(val);
}

void TestPhysicalRegClobberBad(MacroAssembler* masm, Register divisor,
                               Register obj) {
  __ movq(rcx, Immediate(3));
  __ IsObjectType(obj, JS_OBJECT_TYPE, rcx);
  // EXPECT-WARNING: implicitly reads register 'rcx' after it was clobbered
  __ shlq_cl(divisor);
}

// -----------------------------------------------------------------------------
// 2. Operand, ExternalReferenceAsOperand & Generalized GetMemOp Clobber Tests
// -----------------------------------------------------------------------------

void TestOperandBaseClobberedBad(MacroAssembler* masm, Register base,
                                 Register dst) {
  Operand field_op(base, 16);
  __ CallRuntime(Runtime::kAbort);
  // EXPECT-WARNING: Use of Operand 'field_op' whose base/index register 'base'
  __ movq(dst, field_op);
}

void TestExternalReferenceOperandOverwrittenBad(MacroAssembler* masm,
                                                ExternalReference ref1,
                                                ExternalReference ref2,
                                                Register dst) {
  Operand op1 = __ ExternalReferenceAsOperand(ref1);
  Operand op2 = __ ExternalReferenceAsOperand(ref2);
  __ movq(dst, op2);
  // EXPECT-WARNING: Use of Operand 'op1' after its underlying scratch register
  __ addq(dst, op1);
}

void TestExternalReferenceOperandGood(MacroAssembler* masm,
                                      ExternalReference ref1,
                                      ExternalReference ref2, Register dst) {
  Operand op1 = __ ExternalReferenceAsOperand(ref1);
  __ movq(dst, op1);
  Operand op2 = __ ExternalReferenceAsOperand(ref2);
  __ addq(dst, op2);
}

// Regression test modeled on crbug.com/378779897 (Liftoff GetMemOp +
// AtomicBinop):
inline Operand LiftoffGetMemOpUsingScratch1(MacroAssembler* masm, Register addr,
                                            Register offset_reg,
                                            uintptr_t offset_imm) {
  if (is_uint31(offset_imm)) {
    return offset_reg == no_reg
               ? Operand(addr, static_cast<int32_t>(offset_imm))
               : Operand(addr, offset_reg, times_1,
                         static_cast<int32_t>(offset_imm));
  }
  Register scratch = kScratchRegister;
  masm->Move(scratch, offset_imm);
  if (offset_reg != no_reg) masm->addq(scratch, offset_reg);
  return Operand(addr, scratch, times_1, 0);
}

inline Operand LiftoffGetMemOpUsingScratch2(MacroAssembler* masm, Register addr,
                                            Register offset_reg,
                                            uintptr_t offset_imm) {
  if (is_uint31(offset_imm)) {
    return offset_reg == no_reg
               ? Operand(addr, static_cast<int32_t>(offset_imm))
               : Operand(addr, offset_reg, times_1,
                         static_cast<int32_t>(offset_imm));
  }
  Register scratch = r11;
  masm->Move(scratch, offset_imm);
  if (offset_reg != no_reg) masm->addq(scratch, offset_reg);
  return Operand(addr, scratch, times_1, 0);
}

void TestLiftoffGetMemOpClobberBad(MacroAssembler* masm, Register dst_addr,
                                   Register offset_reg, uintptr_t offset_imm,
                                   wasm::LiftoffRegister value,
                                   wasm::LiftoffRegister result) {
  Operand dst_op =
      LiftoffGetMemOpUsingScratch1(masm, dst_addr, offset_reg, offset_imm);
  __ movq(kScratchRegister, value.gp());
  // EXPECT-WARNING: Use of Operand 'dst_op' after its underlying scratch
  __ movq(result.gp(), dst_op);
}

void TestLiftoffGetMemOpSeparateScratchGood(MacroAssembler* masm,
                                            Register dst_addr,
                                            Register offset_reg,
                                            uintptr_t offset_imm,
                                            wasm::LiftoffRegister value,
                                            wasm::LiftoffRegister result) {
  Operand dst_op =
      LiftoffGetMemOpUsingScratch2(masm, dst_addr, offset_reg, offset_imm);
  __ movq(kScratchRegister, value.gp());
  __ movq(result.gp(), dst_op);
}

// -----------------------------------------------------------------------------
// 3. Control-Flow (Forward/Backward Label Branch, Bind, Loop, No-Fallthrough)
// -----------------------------------------------------------------------------

void TestBranchClobberPathMergesIntoBindBad(MacroAssembler* masm, Register val,
                                            Register scratch) {
  Label done;
  __ testq(val, val);
  __ j(zero, &done);
  __ IsObjectType(scratch, JS_OBJECT_TYPE, val);
  __ bind(&done);
  // EXPECT-WARNING: Use of register 'val' after it was clobbered
  __ incq(val);
}

void TestNoFallthroughAbortDoesNotPoisonBindGood(MacroAssembler* masm,
                                                 Register val) {
  Label ok;
  __ testq(val, val);
  __ j(not_zero, &ok);
  __ CallRuntime(Runtime::kAbort);
  __ Abort(AbortReason::kNoReason);
  __ bind(&ok);
  __ incq(val);
}

// Regression test modeled on crbug.com/1439691 (loop-carried register clobber
// across backward branch to bound Label):
void TestBackwardBranchLoopClobberBad(MacroAssembler* masm, Register current,
                                      Register scratch, int count) {
  Label loop;
  __ bind(&loop);
  // On iteration 2+, `current` was clobbered at the bottom of iteration 1!
  // EXPECT-WARNING: Use of register 'current' after it was clobbered
  __ cmpq(current, Immediate(count));
  __ IsObjectType(scratch, JS_OBJECT_TYPE, current);
  __ j(not_equal, &loop);
}

void TestBackwardBranchLoopRedefinedGood(MacroAssembler* masm, Register current,
                                         Register base, Register scratch) {
  Label loop;
  __ bind(&loop);
  __ cmpq(current, base);
  __ IsObjectType(base, JS_OBJECT_TYPE, scratch);
  __ movq(current, Operand(base, 8));
  __ j(not_equal, &loop);
}

void TestCppForLoopClobberBad(MacroAssembler* masm, Register val,
                              Register scratch, int n) {
  for (int i = 0; i < n; ++i) {
    // EXPECT-WARNING: Use of register 'val' after it was clobbered
    __ addq(scratch, val);
    __ IsObjectType(scratch, JS_OBJECT_TYPE, val);
  }
}

void TestCppForLoopGood(MacroAssembler* masm, Register val, Register scratch,
                        int n) {
  for (int i = 0; i < n; ++i) {
    __ IsObjectType(val, JS_OBJECT_TYPE, scratch);
    __ movq(scratch, val);
  }
}

// -----------------------------------------------------------------------------
// 4. Early-Clobber / Input-Output Aliasing Tests
// -----------------------------------------------------------------------------

void HelperEarlyClobber(MacroAssembler* masm, Register V8_REG_OUT dst,
                        Register src) {
  __ movq(dst, Immediate(10));
  __ addq(dst, src);
}

void HelperSafeSameInstruction(MacroAssembler* masm, Register V8_REG_OUT dst,
                               Register src) {
  __ movq(dst, Operand(src, 8));
  __ addq(dst, Immediate(10));
}

void HelperSafeGuarded(MacroAssembler* masm, Register V8_REG_OUT dst,
                       Register src) {
  if (dst != src) {
    __ movq(dst, Immediate(10));
    __ addq(dst, src);
  } else {
    __ addq(dst, Immediate(10));
  }
}

void HelperTwoScratches(MacroAssembler* masm, Register V8_REG_SCRATCH s1,
                        Register V8_REG_SCRATCH s2) {
  __ movq(s1, Immediate(1));
  __ movq(s2, Immediate(2));
}

void TestEarlyClobberCallSites(MacroAssembler* masm, Register r) {
  // EXPECT-WARNING: Register passed as 'dst' to 'HelperEarlyClobber' aliases
  HelperEarlyClobber(masm, r, r);

  HelperSafeSameInstruction(masm, r, r);
  HelperSafeGuarded(masm, r, r);

  // EXPECT-WARNING: Distinct output/scratch parameters 's1' and 's2'
  HelperTwoScratches(masm, r, r);
}

// -----------------------------------------------------------------------------
// 5. BaselineAssembler::ScratchRegisterScope Tests
// -----------------------------------------------------------------------------

void TestBaselineScratchLeakedScopeBad(baseline::BaselineAssembler* basm) {
  Register leaked = no_reg;
  {
    baseline::BaselineAssembler::ScratchRegisterScope scope1(basm);
    leaked = scope1.AcquireScratch();
    basm->Move(leaked, 1);
  }
  {
    baseline::BaselineAssembler::ScratchRegisterScope scope2(basm);
    Register other = scope2.AcquireScratch();
    basm->Move(other, 2);
  }
  // EXPECT-WARNING: Use of register 'leaked' after it was clobbered
  basm->Push(leaked);
}

void TestBaselineNestedScratchScopesGood(baseline::BaselineAssembler* basm) {
  baseline::BaselineAssembler::ScratchRegisterScope scope1(basm);
  Register r1 = scope1.AcquireScratch();
  basm->Move(r1, 1);
  {
    baseline::BaselineAssembler::ScratchRegisterScope scope2(basm);
    Register r2 = scope2.AcquireScratch();
    basm->Move(r2, 2);
    basm->Push(r2);
  }
  basm->Push(r1);
}

// -----------------------------------------------------------------------------
// 6. Annotation Verification Mismatch Tests
// -----------------------------------------------------------------------------

// EXPECT-WARNING: Parameter 'x' of 'v8::internal::BadAnnotationInModified'
void BadAnnotationInModified(MacroAssembler* masm, Register V8_REG_IN x) {
  __ incq(x);
}

// EXPECT-WARNING: Parameter 'out' of 'v8::internal::BadAnnotationOutReadFirst'
void BadAnnotationOutReadFirst(MacroAssembler* masm, Register V8_REG_OUT out) {
  __ addq(out, Immediate(5));
}

// EXPECT-WARNING: modifies/clobbers physical register(s) {rcx}
void BadAnnotationMissingPhysClobber(MacroAssembler* masm,
                                     Register V8_REG_IN x) {
  __ movq(rcx, x);
}

// -----------------------------------------------------------------------------
// 7. Read-Only Provenance: V8_REG_RDONLY & Maglev UseRegister Checking
// -----------------------------------------------------------------------------

// Regression test modeled on crbug.com/516128144 (modifying a read-only
// input/scratch):
void TestRdOnlyParamModifiedBad(MacroAssembler* masm, Register V8_REG_OUT dst,
                                Register V8_REG_RDONLY src) {
  __ movq(dst, src);
  // EXPECT-WARNING: Cannot modify/clobber read-only register 'src'
  __ addq(src, Immediate(1));
}

void TestRdOnlyParamSavedRestoredGood(MacroAssembler* masm,
                                      Register V8_REG_OUT dst,
                                      Register V8_REG_RDONLY src) {
  __ pushq(src);
  __ addq(src, Immediate(1));
  __ movq(dst, src);
  __ popq(src);
}

// Regression test modeled on crbug.com/466701290 (Maglev CheckJSDataViewBounds
// mutating a UseRegister input without UseAndClobberRegister):
struct FakeValueLocation {
  enum ExtendedPolicy { MUST_HAVE_REGISTER, SAME_AS_INPUT };
  enum Lifetime { USED_AT_START, USED_AT_END };
  template <typename... Args>
  void SetUnallocated(Args&&...) {}
};
struct FakeInput {
  FakeValueLocation loc_;
  FakeValueLocation* location() { return &loc_; }
};
struct FakeNodeBase {
  FakeInput input(int) { return {}; }
  FakeValueLocation result() { return {}; }
};
Register ToRegister(FakeInput);
inline void UseRegister(FakeInput input) {
  input.location()->SetUnallocated(FakeValueLocation::MUST_HAVE_REGISTER,
                                   FakeValueLocation::USED_AT_END, -1);
}
inline void UseAndClobberRegister(FakeInput input) {
  input.location()->SetUnallocated(FakeValueLocation::MUST_HAVE_REGISTER,
                                   FakeValueLocation::USED_AT_START, -1);
}
template <typename T>
inline void DefineSameAsFirst(T* node) {
  node->result().SetUnallocated(-1, 0);
}

struct FakeDataViewBoundsBuggy : FakeNodeBase {
  FakeInput IndexInput() { return input(0); }
  FakeInput ByteLengthInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(IndexInput());
    UseRegister(ByteLengthInput());
  }

  void GenerateCode(MacroAssembler* masm, int element_size) {
    Register index = ToRegister(IndexInput());
    Register byte_length = ToRegister(ByteLengthInput());
    if (element_size > 1) {
      // EXPECT-WARNING: Cannot modify/clobber Maglev input register
      __ subq(byte_length, Immediate(element_size - 1));
    }
    // In-place 32->64 bit extension on `index` preserves low 32 bits and is OK:
    __ movsxlq(index, index);
    __ cmpq(index, byte_length);
  }
};

struct FakeDataViewBoundsFixed : FakeNodeBase {
  FakeInput IndexInput() { return input(0); }
  FakeInput ByteLengthInput() { return input(1); }
  int element_size_ = 4;

  void SetValueLocationConstraints() {
    UseRegister(IndexInput());
    if (element_size_ > 1) {
      UseAndClobberRegister(ByteLengthInput());
    } else {
      UseRegister(ByteLengthInput());
    }
  }

  void GenerateCode(MacroAssembler* masm) {
    Register index = ToRegister(IndexInput());
    Register byte_length = ToRegister(ByteLengthInput());
    if (element_size_ > 1) {
      __ subq(byte_length, Immediate(element_size_ - 1));
    }
    __ movsxlq(index, index);
    __ cmpq(index, byte_length);
  }
};

struct FakeBinopDefineSameAsFirstGood : FakeNodeBase {
  FakeInput LeftInput() { return input(0); }
  FakeInput RightInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(LeftInput());
    UseRegister(RightInput());
    DefineSameAsFirst(this);
  }

  void GenerateCode(MacroAssembler* masm) {
    Register left = ToRegister(LeftInput());
    Register right = ToRegister(RightInput());
    __ addl(left, right);
  }
};

// -----------------------------------------------------------------------------
// 8. Partial-Width Writes, Loop-Carried Scratch Operands & Pool Exhaustion
// -----------------------------------------------------------------------------

void TestSetccUpperBitsClobberBad(MacroAssembler* masm, Register dst,
                                  Register obj) {
  __ xorl(dst, dst);
  __ IsObjectType(obj, JS_OBJECT_TYPE, dst);
  // `setcc` only writes the low 8 bits of `dst`; upper bits were clobbered!
  // EXPECT-WARNING: Use of register 'dst' after it was clobbered
  __ setcc(equal, dst);
}

void TestLoopCarriedScratchOperandClobberBad(MacroAssembler* masm,
                                             ExternalReference ref,
                                             Register dst, int n) {
  Operand op = __ ExternalReferenceAsOperand(ref);
  for (int i = 0; i < n; ++i) {
    // On iteration 2+, `op`'s underlying `r10` was overwritten at loop end!
    // EXPECT-WARNING: Use of Operand 'op' after its underlying scratch register
    __ addq(dst, op);
    __ Move(kScratchRegister, 42);
  }
}

void HelperTwoBaselineScratches(baseline::BaselineAssembler* basm) {
  baseline::BaselineAssembler::ScratchRegisterScope scope(basm);
  Register s1 = scope.AcquireScratch();
  Register s2 = scope.AcquireScratch();
  basm->Move(s1, 1);
  basm->Move(s2, 2);
}

void TestBaselineScratchPoolExhaustedBad(baseline::BaselineAssembler* basm) {
  baseline::BaselineAssembler::ScratchRegisterScope scope(basm);
  Register s1 = scope.AcquireScratch();
  Register s2 = scope.AcquireScratch();
  Register s3 = scope.AcquireScratch();
  Register s4 = scope.AcquireScratch();
  basm->Move(s1, 1);
  basm->Move(s2, 2);
  basm->Move(s3, 3);
  basm->Move(s4, 4);
  // Only 1 scratch remains in the 5-register pool, so requiring 2 exhausts it:
  // EXPECT-WARNING: exhausting BaselineAssembler::ScratchRegisterScope pool
  HelperTwoBaselineScratches(basm);
}

// -----------------------------------------------------------------------------
// 9. Maglev Register Allocator Contract Tests (Checks 1-6)
// -----------------------------------------------------------------------------

namespace maglev_test {

enum class ValueRepresentation { kTagged = 0, kInt32 = 1, kFloat64 = 3 };
enum class Opcode { kInt32Constant = 0, kLoadTaggedField = 1 };

struct ValueNode {
  Opcode opcode() const { return Opcode::kLoadTaggedField; }
  template <typename T>
  bool Is() const {
    return false;
  }
};
struct Int32Constant : ValueNode {};
struct RootConstant : ValueNode {};

inline std::optional<int32_t> TryGetInt32Const(const ValueNode* node) {
  switch (node->opcode()) {
    case Opcode::kInt32Constant:
      return 1;
    default:
      return std::nullopt;
  }
}

struct FakeUnallocatedOperand {
  enum BasicPolicy { FIXED_SLOT, EXTENDED_POLICY };
  enum ExtendedPolicy {
    NONE,
    REGISTER_OR_SLOT,
    REGISTER_OR_SLOT_OR_CONSTANT,
    FIXED_REGISTER,
    FIXED_FP_REGISTER,
    MUST_HAVE_REGISTER,
    MUST_HAVE_SLOT,
    SAME_AS_INPUT
  };
  enum Lifetime { USED_AT_START, USED_AT_END };
};

struct ValueLocation {
  bool IsRegister() const { return false; }
  bool IsDoubleRegister() const { return false; }
  bool IsConstant() const { return false; }
  template <typename... Args>
  void SetUnallocated(Args&&...) {}
};

struct Input {
  ValueLocation loc_;
  ValueLocation* location() { return &loc_; }
  bool IsRegister() const { return false; }
  bool IsDoubleRegister() const { return false; }
  bool IsConstant() const { return false; }
  ValueNode* node() const { return nullptr; }
};

struct OpProperties {
  uint32_t bits_ = 0;
  constexpr OpProperties() = default;
  explicit constexpr OpProperties(uint32_t b) : bits_(b) {}
  static constexpr OpProperties Pure() { return OpProperties(0); }
  static constexpr OpProperties Call() { return OpProperties(1u << 0); }
  static constexpr OpProperties DeferredCall() {
    return OpProperties(1u << 14);
  }
  constexpr OpProperties operator|(OpProperties o) const {
    return OpProperties(bits_ | o.bits_);
  }
};

struct RegisterSnapshot {
  RegList live_registers;
};

struct FakeMaglevAssembler : MacroAssembler {
  Isolate* isolate() const { return nullptr; }
  inline Condition CompareInstanceTypeRange(Register map,
                                            Register instance_type_out,
                                            InstanceType lower_limit,
                                            InstanceType higher_limit) {
    CmpInstanceTypeRange(map, instance_type_out, lower_limit, higher_limit);
    return kUnsignedLessThanEqual;
  }
  template <typename Fn, typename... Args>
  Label* MakeDeferredCode(Fn&& fn, Args&&... args) {
    return nullptr;
  }
  template <typename Fn, typename... Args>
  void JumpToDeferredIf(Condition cc, Fn&& fn, Args&&... args) {}
};

struct SaveRegisterStateForCall {
  SaveRegisterStateForCall(FakeMaglevAssembler*, RegisterSnapshot) {}
};

struct ZoneLabelRef {
  Label label_;
  explicit ZoneLabelRef(FakeMaglevAssembler*) {}
  Label* operator*() { return &label_; }
};

struct TemporaryRegisterScope {
  explicit TemporaryRegisterScope(FakeMaglevAssembler*) {}
  Register Acquire() { return rax; }
  DoubleRegister AcquireDouble() { return xmm0; }
  Register AcquireScratch() { return kScratchRegister; }
  DoubleRegister AcquireScratchDouble() { return kScratchDoubleReg; }
  void Include(Register) {}
  void Include(RegList) {}
  void IncludeDouble(DoubleRegister) {}
  void ResetToDefault() {}
};

Register ToRegister(Input);
Register ToRegister(ValueLocation);
DoubleRegister ToDoubleRegister(Input);
DoubleRegister ToDoubleRegister(ValueLocation);

static constexpr int kNoVreg = -1;

inline void UseRegister(Input input) {
  input.location()->SetUnallocated(FakeUnallocatedOperand::MUST_HAVE_REGISTER,
                                   FakeUnallocatedOperand::USED_AT_END,
                                   kNoVreg);
}
inline void UseAndClobberRegister(Input input) {
  input.location()->SetUnallocated(FakeUnallocatedOperand::MUST_HAVE_REGISTER,
                                   FakeUnallocatedOperand::USED_AT_START,
                                   kNoVreg);
}
inline void UseAny(Input input) {
  input.location()->SetUnallocated(
      FakeUnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT,
      FakeUnallocatedOperand::USED_AT_END, kNoVreg);
}
inline void UseFixed(Input input, Register reg) {
  input.location()->SetUnallocated(FakeUnallocatedOperand::FIXED_REGISTER,
                                   reg.code(), kNoVreg);
}
inline void UseFixed(Input input, DoubleRegister reg) {
  input.location()->SetUnallocated(FakeUnallocatedOperand::FIXED_FP_REGISTER,
                                   reg.code(), kNoVreg);
}
inline void UseAndClobberFixed(Input input, Register reg) {
  input.location()->SetUnallocated(
      FakeUnallocatedOperand::FIXED_REGISTER, reg.code(),
      FakeUnallocatedOperand::USED_AT_START, kNoVreg);
}
template <typename T>
inline void DefineAsRegister(T* node) {
  node->result().SetUnallocated(FakeUnallocatedOperand::MUST_HAVE_REGISTER,
                                kNoVreg);
}
template <typename T>
inline void DefineSameAsFirst(T* node) {
  node->result().SetUnallocated(kNoVreg, 0);
}
template <typename T>
inline void DefineSameAs(T* node, int input_index) {
  node->result().SetUnallocated(kNoVreg, input_index);
}
template <typename T>
inline void DefineAsFixed(T* node, Register reg) {
  node->result().SetUnallocated(FakeUnallocatedOperand::FIXED_REGISTER,
                                reg.code(), kNoVreg);
}
template <typename T>
inline void DefineAsFixed(T* node, DoubleRegister reg) {
  node->result().SetUnallocated(FakeUnallocatedOperand::FIXED_FP_REGISTER,
                                reg.code(), kNoVreg);
}
template <typename T>
inline void DefineAsConstant(T* node) {
  node->result().SetUnallocated(FakeUnallocatedOperand::NONE, kNoVreg);
}

struct NodeBase {
  Input input(int) { return {}; }
  ValueLocation result() { return {}; }
  RegisterSnapshot register_snapshot() { return {}; }
  void set_temporaries_needed(int) {}
  void set_double_temporaries_needed(int) {}
  void RequireSpecificTemporary(Register) {}
};

// Check 1a: UseAny requires IsRegister() guard; else branch knows not-register.
struct MaglevUseAnyTests : NodeBase {
  Input ValInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseAny(ValInput());
    DefineAsRegister(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    // EXPECT-WARNING: without checking IsRegister()
    Register unguarded = ToRegister(ValInput());
    __ cmpq(unguarded, Immediate(0));

    if (ValInput().IsRegister()) {
      Register ok = ToRegister(ValInput());
      __ cmpq(ok, Immediate(0));
      // EXPECT-WARNING: input is known not to be in a register on this branch
      DoubleRegister wrong_bank = ToDoubleRegister(ValInput());
      (void)wrong_bank;
    } else {
      // EXPECT-WARNING: input is known not to be in a register on this branch
      Register bad_else = ToRegister(ValInput());
      __ cmpq(bad_else, Immediate(0));
    }

    // Non-constant-only UseAny can be a stack slot, so !IsConstant() is not
    // enough to guarantee a register.
    if (!ValInput().IsConstant()) {
      // EXPECT-WARNING: without checking IsRegister()
      Register maybe_slot = ToRegister(ValInput());
      __ cmpq(maybe_slot, Immediate(0));
    }
  }
};

// Check 1b: Constant-only UseAny (`std::optional` predicate) guarded by
// `!IsConstant()` is guaranteed to be a register.
struct MaglevUseAnyConstantOnlyGood : NodeBase {
  Input ValInput() { return input(0); }

  void SetValueLocationConstraints() {
    if (TryGetInt32Const(ValInput().node())) {
      UseAny(ValInput());
    } else {
      UseRegister(ValInput());
    }
    DefineAsRegister(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    if (!ValInput().IsConstant()) {
      Register v = ToRegister(ValInput());
      __ movq(ToRegister(result()), v);
    }
  }
};

// Check 1c: Unconstrained input and invalid result() Register uses.
struct MaglevUnconstrainedAndConstResultBad : NodeBase {
  Input AInput() { return input(0); }
  Input BInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(AInput());
    DefineAsConstant(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register a = ToRegister(AInput());
    // EXPECT-WARNING: has no location constraint in SetValueLocationConstraints
    Register b = ToRegister(BInput());
    // EXPECT-WARNING: defines result as constant (DefineAsConstant)
    Register res = ToRegister(result());
    __ movq(res, a);
    __ addq(res, b);
  }
};

struct MaglevMissingResultDefBad : NodeBase {
  Input AInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(AInput()); }

  void GenerateCode(FakeMaglevAssembler* masm) {
    // EXPECT-WARNING: did not define a register result
    Register res = ToRegister(result());
    __ movq(res, ToRegister(AInput()));
  }
};

// Check 2: UseFixed vs UseAndClobberFixed, SaveRegisterStateForCall,
// snapshot.live_registers.clear, and TemporaryRegisterScope::Include.
struct MaglevReadonlyFixedAndIncludeBad : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::DeferredCall();
  Input FixedInInput() { return input(0); }
  Input SavedInInput() { return input(1); }
  Input ClearedInInput() { return input(2); }

  void SetValueLocationConstraints() {
    UseFixed(FixedInInput(), rcx);
    UseRegister(SavedInInput());
    UseRegister(ClearedInInput());
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register fixed_in = ToRegister(FixedInInput());
    Register saved_in = ToRegister(SavedInInput());
    Register cleared_in = ToRegister(ClearedInInput());

    // EXPECT-WARNING: Cannot modify/clobber Maglev input register
    // 'FixedInInput'
    __ incq(fixed_in);

    {
      TemporaryRegisterScope temps(masm);
      // EXPECT-WARNING: Cannot modify/clobber Maglev input register
      // 'SavedInInput'
      temps.Include(saved_in);
    }

    {
      RegisterSnapshot snapshot = register_snapshot();
      snapshot.live_registers.clear(cleared_in);
      SaveRegisterStateForCall save_state(masm, snapshot);
      // `saved_in` is preserved by SaveRegisterStateForCall -> OK.
      __ incq(saved_in);
      // `cleared_in` was removed from live_registers -> NOT restored!
      // EXPECT-WARNING: Cannot modify/clobber Maglev input register
      // 'ClearedInInput'
      __ incq(cleared_in);
    }
  }
};

struct MaglevUseAndClobberFixedGood : NodeBase {
  Input FixedInInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseAndClobberFixed(FixedInInput(), rcx);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register fixed_in = ToRegister(FixedInInput());
    __ incq(fixed_in);
  }
};

// Check 3a: Early-clobber on DefineAsRegister + partial vs full alias guard
// (modeled on GeneratorRestoreRegister).
struct MaglevEarlyClobberPartialGuardBad : NodeBase {
  Input ArrayInput() { return input(0); }
  Input StaleInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(ArrayInput());
    UseRegister(StaleInput());
    DefineAsRegister(this);
    set_temporaries_needed(1);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    TemporaryRegisterScope temps(masm);
    Register temp = temps.Acquire();
    Register array = ToRegister(ArrayInput());
    Register stale = ToRegister(StaleInput());
    Register res = ToRegister(result());

    Register value = (array == res ? temp : res);
    __ movq(value, Operand(array, 8));
    // `array` was guarded against aliasing `res` -> OK; `stale` was not!
    // EXPECT-WARNING: Maglev input 'StaleInput' is read
    __ movq(Operand(array, 8), stale);
    if (value != res) {
      __ movq(res, value);
    }
  }
};

struct MaglevEarlyClobberFullGuardGood : NodeBase {
  Input ArrayInput() { return input(0); }
  Input StaleInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(ArrayInput());
    UseRegister(StaleInput());
    DefineAsRegister(this);
    set_temporaries_needed(1);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    TemporaryRegisterScope temps(masm);
    Register temp = temps.Acquire();
    Register array = ToRegister(ArrayInput());
    Register stale = ToRegister(StaleInput());
    Register res = ToRegister(result());

    Register value = (array == res || stale == res) ? temp : res;
    __ movq(value, Operand(array, 8));
    __ movq(Operand(array, 8), stale);
    if (value != res) {
      __ movq(res, value);
    }
  }
};

// Check 3b: De Morgan conditional distinctness (`if (flag && out == other)`).
struct MaglevDeMorganGuardGood : NodeBase {
  bool flag_ = false;
  Input OtherInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(OtherInput());
    DefineAsRegister(this);
    set_temporaries_needed(1);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    TemporaryRegisterScope temps(masm);
    Register out = ToRegister(result());
    if (flag_ && out == ToRegister(OtherInput())) {
      out = temps.Acquire();
    }
    __ movq(out, Immediate(1));
    if (flag_) {
      __ addq(out, ToRegister(OtherInput()));
    }
  }
};

// Check 3c: DefineAsFixed + RequireSpecificTemporary blocks non-fixed inputs.
struct MaglevDefineAsFixedSpecificTempGood : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(InInput());
    DefineAsFixed(this, rax);
    RequireSpecificTemporary(rax);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register res = ToRegister(result());
    Register in = ToRegister(InInput());
    __ movq(res, Immediate(0));
    __ addq(res, in);
  }
};

// Check 3d: DefineSameAsFirst with differing vs matching kInputRepresentations.
struct MaglevDefineSameAsFirstDiffReprGood : NodeBase {
  static constexpr int kFirstIndex = 0;
  static constexpr int kSecondIndex = 1;
  static constexpr std::array<ValueRepresentation, 2> kInputRepresentations = {
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  Input FirstInput() { return input(kFirstIndex); }
  Input SecondInput() { return input(kSecondIndex); }

  void SetValueLocationConstraints() {
    UseRegister(FirstInput());
    UseRegister(SecondInput());
    DefineSameAsFirst(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register res = ToRegister(result());
    Register second = ToRegister(SecondInput());
    __ movq(res, Immediate(0));
    __ addq(res, second);
  }
};

struct MaglevDefineSameAsFirstSameReprBad : NodeBase {
  static constexpr int kFirstIndex = 0;
  static constexpr int kSecondIndex = 1;
  static constexpr std::array<ValueRepresentation, 2> kInputRepresentations = {
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  Input FirstInput() { return input(kFirstIndex); }
  Input SecondInput() { return input(kSecondIndex); }

  void SetValueLocationConstraints() {
    UseRegister(FirstInput());
    UseRegister(SecondInput());
    DefineSameAsFirst(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register res = ToRegister(result());
    Register second = ToRegister(SecondInput());
    __ movq(res, Immediate(0));
    // EXPECT-WARNING: Maglev input 'SecondInput' is read
    __ addq(res, second);
  }
};

// Check 4: TemporaryRegisterScope general/double temp, scratch, Include,
// nested scope, ResetToDefault, and interprocedural helper budgets.
void HelperUsesTwoMaglevTemps(FakeMaglevAssembler* masm) {
  TemporaryRegisterScope temps(masm);
  Register t1 = temps.Acquire();
  Register t2 = temps.Acquire();
  __ movq(t1, t2);
}

struct MaglevTempBudgetsBad : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(InInput());
    set_temporaries_needed(1);
    set_double_temporaries_needed(0);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    {
      TemporaryRegisterScope temps(masm);
      Register t1 = temps.Acquire();
      // EXPECT-WARNING: requires 2 general temporary register(s), but
      // SetValueLocationConstraints only reserved 1
      Register t2 = temps.Acquire();
      __ movq(t1, t2);
    }
    {
      TemporaryRegisterScope temps(masm);
      // EXPECT-WARNING: requires 1 double temporary register(s), but
      // SetValueLocationConstraints only reserved 0
      DoubleRegister d1 = temps.AcquireDouble();
      (void)d1;
    }
    {
      TemporaryRegisterScope temps(masm);
      Register s1 = temps.AcquireScratch();
      // EXPECT-WARNING: requires 2 simultaneous general scratch register(s),
      // exceeding the architecture scratch budget (1)
      Register s2 = temps.AcquireScratch();
      __ movq(s1, s2);
    }
    {
      TemporaryRegisterScope temps(masm);
      temps.ResetToDefault();
      // EXPECT-WARNING: acquires 1 general temporary register(s) after
      // ResetToDefault() with only 0 explicitly included
      Register t1 = temps.Acquire();
      __ movq(t1, Immediate(0));
    }
    // EXPECT-WARNING: requires 2 general temporary register(s), but
    // SetValueLocationConstraints only reserved 1
    HelperUsesTwoMaglevTemps(masm);
  }
};

struct MaglevTempBudgetsIncludeAndNestedGood : NodeBase {
  Input ClobberedInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseAndClobberRegister(ClobberedInput());
    set_temporaries_needed(1);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    {
      TemporaryRegisterScope temps(masm);
      Register t1 = temps.Acquire();
      __ movq(t1, Immediate(1));
    }
    {
      TemporaryRegisterScope temps(masm);
      temps.Include(ToRegister(ClobberedInput()));
      Register t1 = temps.Acquire();
      Register t2 = temps.Acquire();
      __ movq(t1, t2);
    }
  }
};

// Check 5: Unrequested vs requested physical allocatable register writes.
struct MaglevPhysWriteBad : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(InInput());
    DefineAsRegister(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    // EXPECT-WARNING: Physical register 'rcx' is modified/clobbered
    __ movq(rcx, ToRegister(InInput()));
    __ movq(ToRegister(result()), rcx);
  }
};

struct MaglevPhysWriteRequestedGood : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(InInput());
    DefineAsRegister(this);
    RequireSpecificTemporary(rcx);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    __ movq(rcx, ToRegister(InInput()));
    __ movq(ToRegister(result()), rcx);
  }
};

// Check 6: Mainline calls, rejoining deferred calls, and diverging deferred
// callbacks vs OpProperties & SaveRegisterStateForCall.
struct MaglevMainlineCallMissingPropBad : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::Pure();
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(InInput()); }

  void GenerateCode(FakeMaglevAssembler* masm) {
    // EXPECT-WARNING: kProperties does not include OpProperties::Call()
    __ CallRuntime(Runtime::kAbort);
  }
};

struct MaglevMainlineCallWithPropGood : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::Call();
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(InInput()); }

  void GenerateCode(FakeMaglevAssembler* masm) {
    __ CallRuntime(Runtime::kAbort);
  }
};

struct MaglevDeferredCallChecksBad : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::Pure();
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(InInput()); }

  void GenerateCode(FakeMaglevAssembler* masm) {
    ZoneLabelRef done(masm);
    __ j(zero, __ MakeDeferredCode(
                   [](FakeMaglevAssembler* masm, ZoneLabelRef done,
                      MaglevDeferredCallChecksBad* node) {
                     {
                       // EXPECT-WARNING: SaveRegisterStateForCall used
                       SaveRegisterStateForCall save(masm,
                                                     node->register_snapshot());
                       // EXPECT-WARNING: requires OpProperties::DeferredCall()
                       __ CallRuntime(Runtime::kAbort);
                     }
                     __ jmp(*done);
                   },
                   done, this));
    __ bind(*done);
  }
};

struct MaglevDeferredCallMissingSaveStateBad : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::DeferredCall();
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(InInput()); }

  void GenerateCode(FakeMaglevAssembler* masm) {
    ZoneLabelRef done(masm);
    __ j(zero, __ MakeDeferredCode(
                   [](FakeMaglevAssembler* masm, ZoneLabelRef done) {
                     // EXPECT-WARNING: clobbers caller-saved registers without
                     // SaveRegisterStateForCall
                     __ CallRuntime(Runtime::kAbort);
                     __ jmp(*done);
                   },
                   done));
    __ bind(*done);
  }
};

struct MaglevDeferredCallProperGood : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::DeferredCall();
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(InInput()); }

  void GenerateCode(FakeMaglevAssembler* masm) {
    ZoneLabelRef done(masm);
    __ j(zero, __ MakeDeferredCode(
                   [](FakeMaglevAssembler* masm, ZoneLabelRef done,
                      MaglevDeferredCallProperGood* node) {
                     {
                       SaveRegisterStateForCall save(masm,
                                                     node->register_snapshot());
                       __ CallRuntime(Runtime::kAbort);
                     }
                     __ jmp(*done);
                   },
                   done, this));
    __ bind(*done);
  }
};

struct MaglevDivergingDeferredGood : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::DeferredCall();
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(InInput()); }

  void GenerateCode(FakeMaglevAssembler* masm) {
    __ JumpToDeferredIf(zero, [](FakeMaglevAssembler* masm) {
      __ CallRuntime(Runtime::kAbort);
      __ Abort(AbortReason::kNoReason);
    });
  }
};

struct MaglevDivergingDeferredMissingPropBad : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::Pure();
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(InInput()); }

  void GenerateCode(FakeMaglevAssembler* masm) {
    __ JumpToDeferredIf(zero, [](FakeMaglevAssembler* masm) {
      // EXPECT-WARNING: requires OpProperties::DeferredCall()
      __ CallRuntime(Runtime::kAbort);
      __ Abort(AbortReason::kNoReason);
    });
  }
};

// V8_REG_CLOBBERS_IF: CompareRange / CompareInstanceTypeRange only clobber
// kScratchRegister when lower_limit != 0.
void TestConditionalClobberZeroLowGood(FakeMaglevAssembler* masm,
                                       Register string) {
  TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  __ LoadMap(scratch, string);
  __ CompareInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE,
                              LAST_STRING_TYPE);
  __ andl(scratch, Immediate(0xff));
}

void TestConditionalClobberNonZeroLowBad(FakeMaglevAssembler* masm,
                                         Register obj) {
  TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  __ LoadMap(scratch, obj);
  __ CompareInstanceTypeRange(scratch, scratch, FIRST_JS_RECEIVER_TYPE,
                              LAST_JS_RECEIVER_TYPE);
  // EXPECT-WARNING: Use of register 'scratch' after it was clobbered
  __ andl(scratch, Immediate(0xff));
}

void TestCompareRangeDirectCondClobber(MacroAssembler* masm, Register val) {
  __ movq(kScratchRegister, Immediate(7));
  __ CompareRange(val, 0, 10);
  __ addq(val, kScratchRegister);

  __ movq(kScratchRegister, Immediate(7));
  __ CompareRange(val, 1, 10);
  // EXPECT-WARNING: Use of physical register 'r10' after it was clobbered
  __ addq(val, kScratchRegister);
}

// Root-relative ExternalReferenceAsOperand:
// Outside Maglev, IsolateFieldId is always root-relative, whereas raw
// ExternalReference::address_of_pending_message(isolate) may use scratch when
// serializer_enabled() disables root-relative access.
void TestNonMaglevIsolateFieldIdGood(MacroAssembler* masm, Register val) {
  Operand op = __ ExternalReferenceAsOperand(
      ExternalReference::Create(IsolateFieldId::kPendingMessage),
      kScratchRegister);
  __ movq(kScratchRegister, op);
  __ movq(op, val);
}

void TestNonMaglevRawPendingMessageBad(MacroAssembler* masm, Register val) {
  Operand op = __ ExternalReferenceAsOperand(
      ExternalReference::address_of_pending_message(masm->isolate()),
      kScratchRegister);
  __ movq(kScratchRegister, op);
  // EXPECT-WARNING: Use of Operand 'op' after its underlying scratch register
  __ movq(op, val);
}

// Inside Maglev, Isolate-embedded ExternalReferences always resolve via
// [kRootRegister + offset] without using scratch, whereas non-Isolate globals
// and off-Isolate heap-allocated sub-objects still require scratch.
struct MaglevIsolateExternalRefGood : NodeBase {
  Input ValInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(ValInput());
    DefineAsRegister(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register in_reg = ToRegister(ValInput());
    Register out_reg = ToRegister(result());
    TemporaryRegisterScope temps(masm);
    Register scratch = temps.AcquireScratch();
    Operand op = __ ExternalReferenceAsOperand(
        ExternalReference::address_of_pending_message(masm->isolate()),
        scratch);
    __ movq(scratch, op);
    __ movq(op, in_reg);
    __ movq(out_reg, scratch);
  }
};

struct MaglevGlobalExternalRefBad : NodeBase {
  Input ValInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(ValInput());
    DefineAsRegister(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register in_reg = ToRegister(ValInput());
    Register out_reg = ToRegister(result());
    TemporaryRegisterScope temps(masm);
    Register scratch = temps.AcquireScratch();
    Operand op = __ ExternalReferenceAsOperand(
        ExternalReference::address_of_min_int(), scratch);
    __ movq(scratch, op);
    // EXPECT-WARNING: Use of Operand 'op' after its underlying scratch register
    __ movq(op, in_reg);
    __ movq(out_reg, scratch);
  }
};

struct MaglevOffIsolateExternalRefBad : NodeBase {
  Input ValInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(ValInput());
    DefineAsRegister(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register in_reg = ToRegister(ValInput());
    Register out_reg = ToRegister(result());
    TemporaryRegisterScope temps(masm);
    Register scratch = temps.AcquireScratch();
    Operand op = __ ExternalReferenceAsOperand(
        ExternalReference::debug_is_active_address(masm->isolate()), scratch);
    __ movq(scratch, op);
    // EXPECT-WARNING: Use of Operand 'op' after its underlying scratch register
    __ movq(op, in_reg);
    __ movq(out_reg, scratch);
  }
};

// DefineSameAsFirst always binds result() to input(0), even when
// SetValueLocationConstraints constrains input(1) before input(0).
struct MaglevDefineSameAsFirstOutOfOrderGood : NodeBase {
  static constexpr std::array<ValueRepresentation, 2> kInputRepresentations = {
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  Input FirstInput() { return input(0); }
  Input SecondInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(SecondInput());
    UseAndClobberRegister(FirstInput());
    DefineSameAsFirst(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register res = ToRegister(result());
    Register second = ToRegister(SecondInput());
    __ movq(res, Immediate(0));
    __ addq(res, second);
  }
};

struct MaglevDefineSameAsFirstOutOfOrderBad : NodeBase {
  static constexpr std::array<ValueRepresentation, 2> kInputRepresentations = {
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  Input FirstInput() { return input(0); }
  Input SecondInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(SecondInput());
    UseRegister(FirstInput());
    DefineSameAsFirst(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register res = ToRegister(result());
    Register second = ToRegister(SecondInput());
    __ movq(res, Immediate(0));
    // EXPECT-WARNING: Maglev input 'SecondInput' is read
    __ addq(res, second);
  }
};

// DefineSameAsFirst with UseAndClobberFixed on input(0) prevents aliasing
// with arbitrary UseRegister inputs.
struct MaglevDefineSameAsFirstClobberFixedGood : NodeBase {
  static constexpr std::array<ValueRepresentation, 2> kInputRepresentations = {
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  Input FirstInput() { return input(0); }
  Input SecondInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseAndClobberFixed(FirstInput(), rax);
    UseRegister(SecondInput());
    DefineSameAsFirst(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register res = ToRegister(result());
    Register second = ToRegister(SecondInput());
    __ movq(res, Immediate(0));
    __ addq(res, second);
  }
};

// Pre-deferred CHECK_NE(first, second) is preserved across MakeDeferredCode.
struct MaglevDeferredDcheckNeGood : NodeBase {
  static constexpr OpProperties kProperties = OpProperties::DeferredCall();
  static constexpr std::array<ValueRepresentation, 2> kInputRepresentations = {
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input FirstInput() { return input(0); }
  Input SecondInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(FirstInput());
    UseRegister(SecondInput());
    DefineSameAsFirst(this);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register first = ToRegister(FirstInput());
    Register second = ToRegister(SecondInput());
    DCHECK_EQ(first, ToRegister(result()));
    CHECK_NE(first, second);
    __ MakeDeferredCode(
        [](FakeMaglevAssembler* masm, Register obj, Register res_reg) {
          __ movq(res_reg, Immediate(0));
          __ cmpq(obj, res_reg);
        },
        second, first);
  }
};

// Generic SAME_AS_INPUT with non-zero input index (`SetUnallocated(kNoVreg,
// 1)`) binds result() to SecondInput() (`input(1)`) rather than FirstInput().
struct MaglevDefineSameAsSecondInputTests : NodeBase {
  static constexpr std::array<ValueRepresentation, 2> kInputRepresentations = {
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  Input FirstInput() { return input(0); }
  Input SecondInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(FirstInput());
    UseRegister(SecondInput());
    DefineSameAs(this, 1);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register first = ToRegister(FirstInput());
    Register second = ToRegister(SecondInput());
    __ addq(second, Immediate(1));
    // EXPECT-WARNING: Cannot modify/clobber Maglev input register 'FirstInput'
    __ movq(first, Immediate(1));
  }
};

struct MaglevDefineSameAsSecondClobberGood : NodeBase {
  static constexpr std::array<ValueRepresentation, 2> kInputRepresentations = {
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  Input FirstInput() { return input(0); }
  Input SecondInput() { return input(1); }

  void SetValueLocationConstraints() {
    UseRegister(FirstInput());
    UseAndClobberRegister(SecondInput());
    DefineSameAs(this, 1);
  }

  void GenerateCode(FakeMaglevAssembler* masm) {
    Register res = ToRegister(result());
    Register first = ToRegister(FirstInput());
    __ movq(res, Immediate(0));
    __ addq(res, first);
  }
};

}  // namespace maglev_test

#undef __

}  // namespace internal
}  // namespace v8
