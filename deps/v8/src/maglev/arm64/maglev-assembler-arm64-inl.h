// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_ARM64_MAGLEV_ASSEMBLER_ARM64_INL_H_
#define V8_MAGLEV_ARM64_MAGLEV_ASSEMBLER_ARM64_INL_H_

#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/maglev/maglev-assembler.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-code-gen-state.h"

namespace v8 {
namespace internal {
namespace maglev {

constexpr Condition ConditionFor(Operation operation) {
  switch (operation) {
    case Operation::kEqual:
    case Operation::kStrictEqual:
      return eq;
    case Operation::kLessThan:
      return lt;
    case Operation::kLessThanOrEqual:
      return le;
    case Operation::kGreaterThan:
      return gt;
    case Operation::kGreaterThanOrEqual:
      return ge;
    default:
      UNREACHABLE();
  }
}

inline int ShiftFromScale(int n) {
  switch (n) {
    case 1:
      return 0;
    case 2:
      return 1;
    case 4:
      return 2;
    default:
      UNREACHABLE();
  }
}

class MaglevAssembler::ScratchRegisterScope {
 public:
  explicit ScratchRegisterScope(MaglevAssembler* masm) : wrapped_scope_(masm) {
    // This field is never used in arm64.
    DCHECK_NULL(masm->scratch_register_scope_);
  }

  Register Acquire() { return wrapped_scope_.AcquireX(); }
  void Include(Register reg) { wrapped_scope_.Include(reg); }
  void Include(const RegList list) {
    wrapped_scope_.Include(CPURegList(kXRegSizeInBits, list));
  }

  DoubleRegister AcquireDouble() { return wrapped_scope_.AcquireD(); }
  void IncludeDouble(const DoubleRegList list) {
    wrapped_scope_.IncludeFP(CPURegList(kDRegSizeInBits, list));
  }

  RegList Available() {
    return RegList::FromBits(wrapped_scope_.Available()->bits());
  }
  void SetAvailable(RegList list) {
    wrapped_scope_.SetAvailable(CPURegList(kXRegSizeInBits, list));
  }

  DoubleRegList AvailableDouble() {
    uint64_t bits = wrapped_scope_.AvailableFP()->bits();
    // AvailableFP fits in a 32 bits word.
    DCHECK_LE(bits, std::numeric_limits<uint32_t>::max());
    return DoubleRegList::FromBits(static_cast<uint32_t>(bits));
  }
  void SetAvailableDouble(DoubleRegList list) {
    wrapped_scope_.SetAvailableFP(CPURegList(kDRegSizeInBits, list));
  }

 private:
  UseScratchRegisterScope wrapped_scope_;
};

namespace detail {

template <typename Arg>
inline Register ToRegister(MaglevAssembler* masm,
                           MaglevAssembler::ScratchRegisterScope* scratch,
                           Arg arg) {
  Register reg = scratch->Acquire();
  masm->Move(reg, arg);
  return reg;
}
inline Register ToRegister(MaglevAssembler* masm,
                           MaglevAssembler::ScratchRegisterScope* scratch,
                           Register reg) {
  return reg;
}
inline Register ToRegister(MaglevAssembler* masm,
                           MaglevAssembler::ScratchRegisterScope* scratch,
                           const Input& input) {
  if (input.operand().IsConstant()) {
    Register reg = scratch->Acquire();
    input.node()->LoadToRegister(masm, reg);
    return reg;
  }
  const compiler::AllocatedOperand& operand =
      compiler::AllocatedOperand::cast(input.operand());
  if (operand.IsRegister()) {
    return ToRegister(input);
  } else {
    DCHECK(operand.IsStackSlot());
    Register reg = scratch->Acquire();
    masm->Move(reg, masm->ToMemOperand(input));
    return reg;
  }
}

template <typename... Args>
struct CountPushHelper;

template <>
struct CountPushHelper<> {
  static int Count() { return 0; }
};

template <typename Arg, typename... Args>
struct CountPushHelper<Arg, Args...> {
  static int Count(Arg arg, Args... args) {
    int arg_count = 1;
    if constexpr (is_iterator_range<Arg>::value) {
      arg_count = static_cast<int>(std::distance(arg.begin(), arg.end()));
    }
    return arg_count + CountPushHelper<Args...>::Count(args...);
  }
};

template <typename... Args>
struct PushAllHelper;

template <typename... Args>
inline void PushAll(MaglevAssembler* masm, Args... args) {
  PushAllHelper<Args...>::Push(masm, args...);
}

template <typename... Args>
inline void PushAllReverse(MaglevAssembler* masm, Args... args) {
  PushAllHelper<Args...>::PushReverse(masm, args...);
}

template <>
struct PushAllHelper<> {
  static void Push(MaglevAssembler* masm) {}
  static void PushReverse(MaglevAssembler* masm) {}
};

template <typename T, typename... Args>
inline void PushIterator(MaglevAssembler* masm, base::iterator_range<T> range,
                         Args... args) {
  using value_type = typename base::iterator_range<T>::value_type;
  for (auto iter = range.begin(), end = range.end(); iter != end; ++iter) {
    value_type val1 = *iter;
    ++iter;
    if (iter == end) {
      PushAll(masm, val1, args...);
      return;
    }
    value_type val2 = *iter;
    masm->Push(val1, val2);
  }
  PushAll(masm, args...);
}

template <typename T, typename... Args>
inline void PushIteratorReverse(MaglevAssembler* masm,
                                base::iterator_range<T> range, Args... args) {
  using value_type = typename base::iterator_range<T>::value_type;
  using difference_type = typename base::iterator_range<T>::difference_type;
  difference_type count = std::distance(range.begin(), range.end());
  DCHECK_GE(count, 0);
  auto iter = range.rbegin();
  auto end = range.rend();
  if (count % 2 != 0) {
    PushAllReverse(masm, *iter, args...);
    ++iter;
  } else {
    PushAllReverse(masm, args...);
  }
  while (iter != end) {
    value_type val1 = *iter;
    ++iter;
    value_type val2 = *iter;
    ++iter;
    masm->Push(val1, val2);
  }
}

template <typename Arg1, typename Arg2>
inline void PushAligned(MaglevAssembler* masm, Arg1 arg1, Arg2 arg2) {
  {
    // Push the first argument together with padding to ensure alignment.
    // The second argument is not pushed together with the first so we can
    // re-use any scratch registers used to materialise the first argument for
    // the second one.
    MaglevAssembler::ScratchRegisterScope temps(masm);
    masm->MacroAssembler::Push(ToRegister(masm, &temps, arg1), padreg);
  }
  {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    masm->MacroAssembler::str(ToRegister(masm, &temps, arg2), MemOperand(sp));
  }
}

template <typename Arg>
struct PushAllHelper<Arg> {
  static void Push(MaglevAssembler* masm, Arg arg) {
    if constexpr (is_iterator_range<Arg>::value) {
      PushIterator(masm, arg);
    } else {
      FATAL("Unaligned push");
    }
  }
  static void PushReverse(MaglevAssembler* masm, Arg arg) {
    if constexpr (is_iterator_range<Arg>::value) {
      PushIteratorReverse(masm, arg);
    } else {
      PushAllReverse(masm, arg, padreg);
    }
  }
};

template <typename Arg1, typename Arg2, typename... Args>
struct PushAllHelper<Arg1, Arg2, Args...> {
  static void Push(MaglevAssembler* masm, Arg1 arg1, Arg2 arg2, Args... args) {
    if constexpr (is_iterator_range<Arg1>::value) {
      PushIterator(masm, arg1, arg2, args...);
    } else if constexpr (is_iterator_range<Arg2>::value) {
      if (arg2.begin() != arg2.end()) {
        auto val = *arg2.begin();
        PushAligned(masm, arg1, val);
        PushAll(masm,
                base::make_iterator_range(std::next(arg2.begin()), arg2.end()),
                args...);
      } else {
        PushAll(masm, arg1, args...);
      }
    } else {
      PushAligned(masm, arg1, arg2);
      PushAll(masm, args...);
    }
  }
  static void PushReverse(MaglevAssembler* masm, Arg1 arg1, Arg2 arg2,
                          Args... args) {
    if constexpr (is_iterator_range<Arg1>::value) {
      PushIteratorReverse(masm, arg1, arg2, args...);
    } else if constexpr (is_iterator_range<Arg2>::value) {
      if (arg2.begin() != arg2.end()) {
        auto val = *arg2.begin();
        PushAllReverse(
            masm,
            base::make_iterator_range(std::next(arg2.begin()), arg2.end()),
            args...);
        PushAligned(masm, val, arg1);
      } else {
        PushAllReverse(masm, arg1, args...);
      }
    } else {
      PushAllReverse(masm, args...);
      PushAligned(masm, arg2, arg1);
    }
  }
};

}  // namespace detail

template <typename... T>
void MaglevAssembler::Push(T... vals) {
  const int push_count = detail::CountPushHelper<T...>::Count(vals...);
  if (push_count % 2 == 0) {
    detail::PushAll(this, vals...);
  } else {
    detail::PushAll(this, padreg, vals...);
  }
}

template <typename... T>
void MaglevAssembler::PushReverse(T... vals) {
  detail::PushAllReverse(this, vals...);
}

inline void MaglevAssembler::BindJumpTarget(Label* label) {
  MacroAssembler::BindJumpTarget(label);
}

inline void MaglevAssembler::BindBlock(BasicBlock* block) {
  if (block->is_start_block_of_switch_case()) {
    BindJumpTarget(block->label());
  } else {
    Bind(block->label());
  }
}

inline void MaglevAssembler::DoubleToInt64Repr(Register dst,
                                               DoubleRegister src) {
  Mov(dst, src, 0);
}

inline void MaglevAssembler::SmiTagInt32(Register obj, Label* fail) {
  Adds(obj.W(), obj.W(), obj.W());
  JumpIf(vs, fail);
}

inline Condition MaglevAssembler::IsInt64Constant(Register reg,
                                                  int64_t constant) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Mov(scratch, kHoleNanInt64);
  Cmp(reg, scratch);
  return eq;
}

inline Condition MaglevAssembler::IsRootConstant(Input input,
                                                 RootIndex root_index) {
  if (input.operand().IsRegister()) {
    CompareRoot(ToRegister(input), root_index);
  } else {
    DCHECK(input.operand().IsStackSlot());
    ScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Ldr(scratch, ToMemOperand(input));
    CompareRoot(scratch, root_index);
  }
  return eq;
}

inline MemOperand MaglevAssembler::StackSlotOperand(StackSlot slot) {
  return MemOperand(fp, slot.index);
}

// TODO(Victorgomes): Unify this to use StackSlot struct.
inline MemOperand MaglevAssembler::GetStackSlot(
    const compiler::AllocatedOperand& operand) {
  return MemOperand(fp, GetFramePointerOffsetForStackSlot(operand));
}

inline MemOperand MaglevAssembler::ToMemOperand(
    const compiler::InstructionOperand& operand) {
  return GetStackSlot(compiler::AllocatedOperand::cast(operand));
}

inline MemOperand MaglevAssembler::ToMemOperand(const ValueLocation& location) {
  return ToMemOperand(location.operand());
}

inline void MaglevAssembler::BuildTypedArrayDataPointer(Register data_pointer,
                                                        Register object) {
  DCHECK_NE(data_pointer, object);
  LoadExternalPointerField(
      data_pointer,
      FieldMemOperand(object, JSTypedArray::kExternalPointerOffset));
  if (JSTypedArray::kMaxSizeInHeap == 0) return;
  ScratchRegisterScope scope(this);
  Register base = scope.Acquire();
  Ldr(base.W(), FieldMemOperand(object, JSTypedArray::kBasePointerOffset));
  Add(data_pointer, data_pointer, base);
}

inline void MaglevAssembler::LoadTaggedFieldByIndex(Register result,
                                                    Register object,
                                                    Register index, int scale,
                                                    int offset) {
  if (scale == 1) {
    Add(result, object, index);
  } else {
    Add(result, object, Operand(index, LSL, ShiftFromScale(scale / 2)));
  }
  MacroAssembler::LoadTaggedField(result, FieldMemOperand(result, offset));
}

inline void MaglevAssembler::LoadBoundedSizeFromObject(Register result,
                                                       Register object,
                                                       int offset) {
  Move(result, FieldMemOperand(object, offset));
#ifdef V8_ENABLE_SANDBOX
  Lsr(result, result, kBoundedSizeShift);
#endif  // V8_ENABLE_SANDBOX
}

inline void MaglevAssembler::LoadExternalPointerField(Register result,
                                                      MemOperand operand) {
#ifdef V8_ENABLE_SANDBOX
  LoadSandboxedPointerField(result, operand);
#else
  Move(result, operand);
#endif
}

inline void MaglevAssembler::LoadSignedField(Register result,
                                             MemOperand operand, int size) {
  if (size == 1) {
    Ldrsb(result, operand);
  } else if (size == 2) {
    Ldrsh(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    DCHECK(result.IsW());
    Ldr(result, operand);
  }
}

inline void MaglevAssembler::LoadUnsignedField(Register result,
                                               MemOperand operand, int size) {
  if (size == 1) {
    Ldrb(result, operand);
  } else if (size == 2) {
    Ldrh(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    DCHECK(result.IsW());
    Ldr(result, operand);
  }
}

inline void MaglevAssembler::StoreTaggedSignedField(Register object, int offset,
                                                    Register value) {
  AssertSmi(value);
  StoreTaggedField(value, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreTaggedSignedField(Register object, int offset,
                                                    Smi value) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Mov(scratch, value);
  StoreTaggedField(scratch, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreField(MemOperand operand, Register value,
                                        int size) {
  DCHECK(size == 1 || size == 2 || size == 4);
  if (size == 1) {
    Strb(value, operand);
  } else if (size == 2) {
    Strh(value, operand);
  } else {
    DCHECK_EQ(size, 4);
    DCHECK(value.IsW());
    Str(value, operand);
  }
}

inline void MaglevAssembler::ReverseByteOrder(Register value, int size) {
  if (size == 2) {
    Rev16(value, value);
  } else if (size == 4) {
    Rev32(value, value);
  } else {
    DCHECK_EQ(size, 1);
  }
}

inline void MaglevAssembler::Move(StackSlot dst, Register src) {
  Str(src, StackSlotOperand(dst));
}
inline void MaglevAssembler::Move(StackSlot dst, DoubleRegister src) {
  Str(src, StackSlotOperand(dst));
}
inline void MaglevAssembler::Move(Register dst, StackSlot src) {
  Ldr(dst, StackSlotOperand(src));
}
inline void MaglevAssembler::Move(DoubleRegister dst, StackSlot src) {
  Ldr(dst, StackSlotOperand(src));
}
inline void MaglevAssembler::Move(MemOperand dst, Register src) {
  Str(src, dst);
}
inline void MaglevAssembler::Move(MemOperand dst, DoubleRegister src) {
  Str(src, dst);
}
inline void MaglevAssembler::Move(Register dst, MemOperand src) {
  Ldr(dst, src);
}
inline void MaglevAssembler::Move(DoubleRegister dst, MemOperand src) {
  Ldr(dst, src);
}
inline void MaglevAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  Fmov(dst, src);
}
inline void MaglevAssembler::Move(Register dst, Smi src) {
  MacroAssembler::Move(dst, src);
}
inline void MaglevAssembler::Move(Register dst, ExternalReference src) {
  Mov(dst, src);
}
inline void MaglevAssembler::Move(Register dst, Register src) {
  MacroAssembler::Move(dst, src);
}
inline void MaglevAssembler::Move(Register dst, TaggedIndex i) {
  Mov(dst, i.ptr());
}
inline void MaglevAssembler::Move(Register dst, int32_t i) {
  Mov(dst.W(), Immediate(i));
}
inline void MaglevAssembler::Move(DoubleRegister dst, double n) {
  Fmov(dst, n);
}
inline void MaglevAssembler::Move(DoubleRegister dst, Float64 n) {
  Fmov(dst, n.get_scalar());
}
inline void MaglevAssembler::Move(Register dst, Handle<HeapObject> obj) {
  Mov(dst, Operand(obj));
}

inline void MaglevAssembler::SignExtend32To64Bits(Register dst, Register src) {
  Mov(dst, Operand(src.W(), SXTW));
}
inline void MaglevAssembler::NegateInt32(Register val) {
  Neg(val.W(), val.W());
}

inline void MaglevAssembler::ToUint8Clamped(Register result,
                                            DoubleRegister value, Label* min,
                                            Label* max, Label* done) {
  ScratchRegisterScope temps(this);
  DoubleRegister scratch = temps.AcquireDouble();
  Move(scratch, 0.0);
  Fcmp(scratch, value);
  // Set to 0 if NaN.
  B(vs, min);
  B(ge, min);
  Move(scratch, 255.0);
  Fcmp(value, scratch);
  B(ge, max);
  // if value in [0, 255], then round up to the nearest.
  Frintn(scratch, value);
  TruncateDoubleToInt32(result, scratch);
  B(done);
}

template <typename NodeT>
inline void MaglevAssembler::DeoptIfBufferDetached(Register array,
                                                   Register scratch,
                                                   NodeT* node) {
  if (!code_gen_state()
           ->broker()
           ->dependencies()
           ->DependOnArrayBufferDetachingProtector()) {
    // A detached buffer leads to megamorphic feedback, so we won't have a deopt
    // loop if we deopt here.
    LoadTaggedField(scratch,
                    FieldMemOperand(array, JSArrayBufferView::kBufferOffset));
    LoadTaggedField(scratch,
                    FieldMemOperand(scratch, JSArrayBuffer::kBitFieldOffset));
    Tst(scratch.W(), Immediate(JSArrayBuffer::WasDetachedBit::kMask));
    EmitEagerDeoptIf(ne, DeoptimizeReason::kArrayBufferWasDetached, node);
  }
}

inline void MaglevAssembler::LoadByte(Register dst, MemOperand src) {
  Ldrb(dst, src);
}

inline void MaglevAssembler::IsObjectType(Register heap_object,
                                          InstanceType type) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MacroAssembler::IsObjectType(heap_object, scratch, scratch, type);
}

inline void MaglevAssembler::CompareObjectType(Register heap_object,
                                               InstanceType type) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  CompareObjectType(heap_object, type, scratch);
}

inline void MaglevAssembler::JumpIfJSAnyIsNotPrimitive(
    Register heap_object, Label* target, Label::Distance distance) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MacroAssembler::JumpIfJSAnyIsNotPrimitive(heap_object, scratch, target,
                                            distance);
}

inline void MaglevAssembler::CompareObjectType(Register heap_object,
                                               InstanceType type,
                                               Register scratch) {
  LoadMap(scratch, heap_object);
  CompareInstanceType(scratch, scratch, type);
}

inline void MaglevAssembler::CompareObjectTypeRange(Register heap_object,
                                                    InstanceType lower_limit,
                                                    InstanceType higher_limit) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  LoadMap(scratch, heap_object);
  CompareInstanceTypeRange(scratch, scratch, lower_limit, higher_limit);
}

inline void MaglevAssembler::CompareMapWithRoot(Register object,
                                                RootIndex index,
                                                Register scratch) {
  if (V8_STATIC_ROOTS_BOOL && RootsTable::IsReadOnly(index)) {
    Ldr(scratch.W(), FieldMemOperand(object, HeapObject::kMapOffset));
    CmpTagged(scratch, Immediate(ReadOnlyRootPtr(index)));
    return;
  }
  LoadMap(scratch, object);
  CompareRoot(scratch, index);
}

inline void MaglevAssembler::CompareInstanceTypeRange(
    Register map, InstanceType lower_limit, InstanceType higher_limit) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  CompareInstanceTypeRange(map, scratch, lower_limit, higher_limit);
}

inline void MaglevAssembler::CompareInstanceTypeRange(
    Register map, Register instance_type_out, InstanceType lower_limit,
    InstanceType higher_limit) {
  MacroAssembler::CompareInstanceTypeRange(map, instance_type_out, lower_limit,
                                           higher_limit);
}

inline void MaglevAssembler::CompareTagged(Register reg,
                                           Handle<HeapObject> obj) {
  CmpTagged(reg, Operand(obj, RelocInfo::COMPRESSED_EMBEDDED_OBJECT));
}

inline void MaglevAssembler::CompareTagged(Register src1, Register src2) {
  CmpTagged(src1, src2);
}

inline void MaglevAssembler::CompareInt32(Register reg, int32_t imm) {
  Cmp(reg.W(), Immediate(imm));
}

inline void MaglevAssembler::CompareInt32(Register src1, Register src2) {
  Cmp(src1.W(), src2.W());
}

inline void MaglevAssembler::CallSelf() {
  DCHECK(code_gen_state()->entry_label()->is_bound());
  Bl(code_gen_state()->entry_label());
}

inline void MaglevAssembler::Jump(Label* target, Label::Distance) { B(target); }

inline void MaglevAssembler::JumpIf(Condition cond, Label* target,
                                    Label::Distance) {
  B(target, cond);
}

inline void MaglevAssembler::JumpIfRoot(Register with, RootIndex index,
                                        Label* if_equal,
                                        Label::Distance distance) {
  MacroAssembler::JumpIfRoot(with, index, if_equal);
}

inline void MaglevAssembler::JumpIfNotRoot(Register with, RootIndex index,
                                           Label* if_not_equal,
                                           Label::Distance distance) {
  MacroAssembler::JumpIfNotRoot(with, index, if_not_equal);
}

inline void MaglevAssembler::JumpIfSmi(Register src, Label* on_smi,
                                       Label::Distance distance) {
  MacroAssembler::JumpIfSmi(src, on_smi);
}

void MaglevAssembler::JumpIfByte(Condition cc, Register value, int32_t byte,
                                 Label* target, Label::Distance) {
  CompareAndBranch(value, Immediate(byte), cc, target);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, Register r2,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  CompareAndBranch(r1.W(), r2.W(), cond, target);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, int32_t value,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  CompareAndBranch(r1.W(), Immediate(value), cond, target);
}

inline void MaglevAssembler::CompareSmiAndJumpIf(Register r1, Smi value,
                                                 Condition cond, Label* target,
                                                 Label::Distance distance) {
  AssertSmi(r1);
  CompareAndBranch(r1, Immediate(value), cond, target);
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    Register r1, int32_t mask, Label* target, Label::Distance distance) {
  TestAndBranchIfAnySet(r1.W(), mask, target);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    Register r1, int32_t mask, Label* target, Label::Distance distance) {
  TestAndBranchIfAllClear(r1.W(), mask, target);
}

inline void MaglevAssembler::LoadHeapNumberValue(DoubleRegister result,
                                                 Register heap_number) {
  Ldr(result, FieldMemOperand(heap_number, HeapNumber::kValueOffset));
}

inline void MaglevAssembler::Int32ToDouble(DoubleRegister result, Register n) {
  Scvtf(result, n.W());
}

inline void MaglevAssembler::Pop(Register dst) { Pop(dst, padreg); }

inline void MaglevAssembler::AssertStackSizeCorrect() {
  if (v8_flags.debug_code) {
    ScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Add(scratch, sp,
        RoundUp<2 * kSystemPointerSize>(
            code_gen_state()->stack_slots() * kSystemPointerSize +
            StandardFrameConstants::kFixedFrameSizeFromFp));
    Cmp(scratch, fp);
    Assert(eq, AbortReason::kStackAccessBelowStackPointer);
  }
}

inline void MaglevAssembler::FinishCode() {
  ForceConstantPoolEmissionWithoutJump();
}

template <typename NodeT>
inline void MaglevAssembler::EmitEagerDeoptIfNotEqual(DeoptimizeReason reason,
                                                      NodeT* node) {
  EmitEagerDeoptIf(ne, reason, node);
}

inline void MaglevAssembler::MaterialiseValueNode(Register dst,
                                                  ValueNode* value) {
  switch (value->opcode()) {
    case Opcode::kInt32Constant: {
      int32_t int_value = value->Cast<Int32Constant>()->value();
      if (Smi::IsValid(int_value)) {
        Move(dst, Smi::FromInt(int_value));
      } else {
        MoveHeapNumber(dst, int_value);
      }
      return;
    }
    case Opcode::kFloat64Constant: {
      double double_value =
          value->Cast<Float64Constant>()->value().get_scalar();
      MoveHeapNumber(dst, double_value);
      return;
    }
    default:
      break;
  }

  DCHECK(!value->allocation().IsConstant());
  DCHECK(value->allocation().IsAnyStackSlot());
  using D = NewHeapNumberDescriptor;
  MemOperand src = ToMemOperand(value->allocation());
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kInt32: {
      Label done;
      ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      Ldr(scratch.W(), src);
      Adds(dst.W(), scratch.W(), scratch.W());
      B(&done, vc);
      // If we overflow, instead of bailing out (deopting), we change
      // representation to a HeapNumber.
      Scvtf(D::GetDoubleRegisterParameter(D::kValue), scratch);
      CallBuiltin(Builtin::kNewHeapNumber);
      Move(dst, kReturnRegister0);
      bind(&done);
      break;
    }
    case ValueRepresentation::kUint32: {
      Label done, tag_smi;
      Ldr(dst.W(), src);
      // Unsigned comparison against Smi::kMaxValue.
      Cmp(dst.W(), Immediate(Smi::kMaxValue));
      B(&tag_smi, ls);
      // If we don't fit in a Smi, instead of bailing out (deopting), we
      // change representation to a HeapNumber.
      Ucvtf(D::GetDoubleRegisterParameter(D::kValue), dst.W());
      CallBuiltin(Builtin::kNewHeapNumber);
      Move(dst, kReturnRegister0);
      B(&done);
      bind(&tag_smi);
      SmiTag(dst);
      bind(&done);
      break;
    }
    case ValueRepresentation::kFloat64:
      Ldr(D::GetDoubleRegisterParameter(D::kValue), src);
      CallBuiltin(Builtin::kNewHeapNumber);
      Move(dst, kReturnRegister0);
      break;
    case ValueRepresentation::kWord64:
    case ValueRepresentation::kTagged:
      UNREACHABLE();
  }
}

template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr, Register dst,
                                      Register src) {
  Mov(dst, src);
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr, Register dst,
                                      MemOperand src) {
  switch (repr) {
    case MachineRepresentation::kWord32:
      return Ldr(dst.W(), src);
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
      return Ldr(dst, src);
    default:
      UNREACHABLE();
  }
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr,
                                      MemOperand dst, Register src) {
  switch (repr) {
    case MachineRepresentation::kWord32:
      return Str(src.W(), dst);
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
      return Str(src, dst);
    default:
      UNREACHABLE();
  }
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr,
                                      MemOperand dst, MemOperand src) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MoveRepr(repr, scratch, src);
  MoveRepr(repr, dst, scratch);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_ARM64_MAGLEV_ASSEMBLER_ARM64_INL_H_
