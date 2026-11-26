// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/baseline/baseline-assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"

namespace v8 {
namespace internal {

#define __ masm->

// -----------------------------------------------------------------------------
// 1. ARM64 GPR Use-After-Clobber & W/X Aliasing Tests
// -----------------------------------------------------------------------------

void TestArm64ScratchClobberBad(MacroAssembler* masm, Register obj, Register s1,
                                Register s2) {
  __ IsObjectType(obj, s1, s2, JS_OBJECT_TYPE);
  // EXPECT-WARNING: Use of register 's1' after it was clobbered
  __ Add(obj, obj, s1);
}

void TestArm64WXPhysicalAliasingBad(MacroAssembler* masm, Register dst) {
  __ Mov(x2, 42);
  __ CallRuntime(Runtime::kAbort);
  // `w2` aliases `x2`, which was clobbered by CallRuntime!
  // EXPECT-WARNING: Use of physical register 'x2' after it was clobbered
  __ Mov(dst.W(), w2);
}

void TestArm64PushPopAcrossCallGood(MacroAssembler* masm, Register a,
                                    Register b) {
  __ Push(a, b);
  __ CallRuntime(Runtime::kAbort);
  __ Pop(b, a);
  __ Add(a, a, b);
}

// -----------------------------------------------------------------------------
// 2. ARM64 Size-Sensitive `!=` Guard Checking (crbug.com/530663440)
// -----------------------------------------------------------------------------

// Regression test modeled on crbug.com/530663440
// (`MacroAssembler::MoveObjectAndSlot`): `CPURegister::operator!=` compares
// register width (`w0 != x0` is true!), so guarding an overwrite of
// `dst_object` before reading `offset` with `!=` fails when `offset` uses a
// 32-bit W register (e.g. `Operand(w0, SXTW)`) and `dst_object` is `x0`.
void TestMoveObjectAndSlotSizeSensitiveGuardBad(MacroAssembler* masm,
                                                Register V8_REG_OUT dst_object,
                                                Register V8_REG_OUT dst_slot,
                                                Register object,
                                                Operand offset) {
  if (!dst_slot.Aliases(object)) {
    __ Add(dst_slot, object, offset);
    __ Mov(dst_object, object);
    return;
  }
  if (offset.IsImmediate() || offset.reg() != dst_object) {
    __ Mov(dst_object, dst_slot);
    // EXPECT-WARNING: overwritten before reading 'offset' under a
    // size-sensitive guard
    __ Add(dst_slot, dst_slot, offset);
    return;
  }
  __ Add(dst_slot, dst_slot, offset);
  __ Sub(dst_object, dst_slot, offset);
}

void TestMoveObjectAndSlotAliasesGuardGood(MacroAssembler* masm,
                                           Register V8_REG_OUT dst_object,
                                           Register V8_REG_OUT dst_slot,
                                           Register object, Operand offset) {
  if (!dst_slot.Aliases(object)) {
    __ Add(dst_slot, object, offset);
    __ Mov(dst_object, object);
    return;
  }
  if (offset.IsImmediate() || !offset.reg().Aliases(dst_object)) {
    __ Mov(dst_object, dst_slot);
    __ Add(dst_slot, dst_slot, offset);
    return;
  }
  __ Add(dst_slot, dst_slot, offset);
  __ Sub(dst_object, dst_slot, offset);
}

// -----------------------------------------------------------------------------
// 3. ARM64 MOPS `Cpy` / `Set` Register Clobbering (crbug.com/502300817)
// -----------------------------------------------------------------------------

void TestArm64MopsCpyClobberBad(MacroAssembler* masm, Register dst,
                                Register src, Register count) {
  __ Cpy(dst, src, count);
  // MOPS `Cpy` modifies/clobbers all three operands (`dst`, `src`, `count`):
  // EXPECT-WARNING: Use of MemOperand whose base/index register 'dst'
  __ Str(xzr, MemOperand(dst, 0));
}

void TestArm64MopsCpySavedGood(MacroAssembler* masm, Register dst, Register src,
                               Register count, Register saved_dst) {
  __ Mov(saved_dst, dst);
  __ Cpy(dst, src, count);
  __ Str(xzr, MemOperand(saved_dst, 0));
}

// -----------------------------------------------------------------------------
// 4. ARM64 UseScratchRegisterScope & Pacib1716 Tests
// -----------------------------------------------------------------------------

void TestArm64ScratchScopeLeakedBad(MacroAssembler* masm, Register out) {
  Register leaked = NoReg;
  {
    UseScratchRegisterScope temps(masm);
    leaked = temps.AcquireX();
    __ Mov(leaked, 99);
  }
  // EXPECT-WARNING: Use of register 'leaked' after it was clobbered
  __ Add(out, out, leaked);
}

void TestArm64Pacib1716OverwritesTempBad(MacroAssembler* masm, Register ptr,
                                         Register mod, Register out) {
  UseScratchRegisterScope temps(masm);
  Register temp = temps.AcquireX();  // Acquires x16 (ip0)
  __ Mov(temp, 123);
  __ Mov(x17, ptr);
  __ Mov(x16, mod);  // Silently overwrites `temp` (x16)!
  __ Pacib1716();
  // EXPECT-WARNING: Use of register 'temp' after it was clobbered
  __ Add(out, out, temp);
}

void TestArm64Pacib1716WithExcludeIncludeGood(MacroAssembler* masm,
                                              Register ptr, Register mod,
                                              Register out) {
  UseScratchRegisterScope temps(masm);
  temps.Exclude(x16, x17);
  temps.Include(x10, x11);
  Register temp = temps.AcquireX();  // Acquires x10 instead of x16!
  __ Mov(temp, 123);
  __ Mov(x17, ptr);
  __ Mov(x16, mod);
  __ Pacib1716();
  __ Add(out, out, temp);
}

void TestArm64NestedScratchScopeGood(MacroAssembler* masm,
                                     Register V8_REG_OUT out, Register in) {
  UseScratchRegisterScope temps(masm);
  Register temp = temps.AcquireX();
  __ Mov(temp, in);
  // Add with large immediate internally uses a nested UseScratchRegisterScope
  // (acquiring ip1), which must NOT clobber `temp` (ip0):
  __ Add(out, in, Operand(0x123456));
  __ Add(out, out, temp);
}

// -----------------------------------------------------------------------------
// 5. ARM64 Backward Branch Loop Clobber Check
// -----------------------------------------------------------------------------

void TestArm64BackwardLoopClobberBad(MacroAssembler* masm, Register cur,
                                     Register s1, Register s2) {
  Label loop;
  __ Bind(&loop);
  // EXPECT-WARNING: Use of register 'cur' after it was clobbered
  __ Cmp(cur, Immediate(0));
  __ IsObjectType(s1, cur, s2, JS_OBJECT_TYPE);
  __ B(ne, &loop);
}

// -----------------------------------------------------------------------------
// 6. ARM64 UseScratchRegisterScope Pool Exhaustion & Physical Bit Clobbering
// -----------------------------------------------------------------------------

void HelperOneArm64Scratch(MacroAssembler* masm) {
  UseScratchRegisterScope temps(masm);
  Register t = temps.AcquireX();
  __ Mov(t, 1);
}

void HelperTwoArm64Scratches(MacroAssembler* masm) {
  UseScratchRegisterScope temps(masm);
  Register t1 = temps.AcquireX();
  Register t2 = temps.AcquireX();
  __ Add(t1, t1, t2);
}

void TestArm64ScratchPoolExhaustedBad(MacroAssembler* masm) {
  UseScratchRegisterScope temps(masm);
  Register held = temps.AcquireX();
  __ Mov(held, 7);
  // Only 1 scratch (`x17`) remains available, so requiring 2 exhausts the pool:
  // EXPECT-WARNING: exhausting UseScratchRegisterScope pool
  HelperTwoArm64Scratches(masm);
}

void TestArm64CalleeScratchClobbersPhysIp0Bad(MacroAssembler* masm,
                                              Register V8_REG_OUT out) {
  __ Mov(x16, 42);
  HelperOneArm64Scratch(masm);
  // `HelperOneArm64Scratch` consumed `x16` from UseScratchRegisterScope!
  // EXPECT-WARNING: Use of physical register 'x16' after it was clobbered
  __ Mov(out, x16);
}

// -----------------------------------------------------------------------------
// 7. ARM64 Maglev Register Allocator Contract Tests
// -----------------------------------------------------------------------------

namespace maglev_test {

struct FakeUnallocatedOperand {
  enum ExtendedPolicy { MUST_HAVE_REGISTER };
  enum Lifetime { USED_AT_START, USED_AT_END };
};

struct ValueLocation {
  template <typename... Args>
  void SetUnallocated(Args&&...) {}
};

struct Input {
  ValueLocation loc_;
  ValueLocation* location() { return &loc_; }
};

struct TemporaryRegisterScope {
  explicit TemporaryRegisterScope(MacroAssembler*) {}
  Register Acquire() { return x0; }
  Register AcquireScratch() { return x16; }
};

Register ToRegister(Input);
Register ToRegister(ValueLocation);
inline void UseRegister(Input input) {
  input.location()->SetUnallocated(FakeUnallocatedOperand::MUST_HAVE_REGISTER,
                                   FakeUnallocatedOperand::USED_AT_END, -1);
}
template <typename T>
inline void DefineAsRegister(T* node) {
  node->result().SetUnallocated(FakeUnallocatedOperand::MUST_HAVE_REGISTER, -1);
}

struct NodeBase {
  Input input(int) { return {}; }
  ValueLocation result() { return {}; }
};

// x27 (cp) is allocatable in Maglev on ARM64, so modifying it without
// requesting it as fixed output/temp must warn.
struct MaglevArm64UnrequestedX27Bad : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(InInput());
    DefineAsRegister(this);
  }

  void GenerateCode(MacroAssembler* masm) {
    // EXPECT-WARNING: Physical register 'x27' is modified/clobbered
    __ Mov(x27, ToRegister(InInput()));
    __ Mov(ToRegister(result()), x27);
  }
};

// ToRegister() always produces X-sized registers on ARM64, so `!=` between
// two X-width registers is width-safe.
struct MaglevArm64WidthSafeXGuardGood : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(InInput());
    DefineAsRegister(this);
  }

  void GenerateCode(MacroAssembler* masm) {
    Register out = ToRegister(result());
    Register in = ToRegister(InInput());
    if (out != in) {
      __ Mov(out, 1);
      __ Add(out, out, in);
    }
  }
};

// Comparing two `.W()` registers stored in a local `bool` is also width-safe.
struct MaglevArm64WidthSafeWBoolGuardGood : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(InInput());
    DefineAsRegister(this);
  }

  void GenerateCode(MacroAssembler* masm) {
    Register out = ToRegister(result()).W();
    Register in = ToRegister(InInput()).W();
    bool out_alias_input = out == in;
    if (!out_alias_input) {
      __ Mov(out, 1);
      __ Add(out, out, in);
    }
  }
};

// Comparing a `.W()` register against an X-width register via `!=` is
// size-sensitive (`w0 != x0` is true despite aliasing) and must warn.
struct MaglevArm64MismatchedWidthGuardBad : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() {
    UseRegister(InInput());
    DefineAsRegister(this);
  }

  void GenerateCode(MacroAssembler* masm) {
    Register out = ToRegister(result());
    Register in = ToRegister(InInput());
    if (out.W() != in) {
      __ Mov(out, 1);
      // EXPECT-WARNING: under a size-sensitive '!=' guard on ARM64
      __ Add(out, out, in);
    }
  }
};

// ARM64 provides 2 general scratch registers (`x16`/`x17`); acquiring 2 is OK
// and acquiring 3 simultaneously exceeds the budget.
struct MaglevArm64ScratchBudgetBad : NodeBase {
  Input InInput() { return input(0); }

  void SetValueLocationConstraints() { UseRegister(InInput()); }

  void GenerateCode(MacroAssembler* masm) {
    TemporaryRegisterScope temps(masm);
    Register s1 = temps.AcquireScratch();
    Register s2 = temps.AcquireScratch();
    // EXPECT-WARNING: requires 3 simultaneous general scratch register(s),
    // exceeding the architecture scratch budget (2)
    Register s3 = temps.AcquireScratch();
    __ Add(s1, s2, s3);
  }
};

}  // namespace maglev_test

#undef __

}  // namespace internal
}  // namespace v8
