// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_X64_MAGLEV_ASSEMBLER_X64_INL_H_
#define V8_MAGLEV_X64_MAGLEV_ASSEMBLER_X64_INL_H_

#include <tuple>
#include <type_traits>
#include <utility>

#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/common/globals.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "v8-internal.h"

namespace v8 {
namespace internal {
namespace maglev {

constexpr Condition ConditionForFloat64(Operation operation) {
  switch (operation) {
    case Operation::kEqual:
    case Operation::kStrictEqual:
      return equal;
    case Operation::kLessThan:
      return below;
    case Operation::kLessThanOrEqual:
      return below_equal;
    case Operation::kGreaterThan:
      return above;
    case Operation::kGreaterThanOrEqual:
      return above_equal;
    default:
      UNREACHABLE();
  }
}

constexpr Condition ConditionForNaN() { return parity_even; }

inline ScaleFactor ScaleFactorFromInt(int n) {
  switch (n) {
    case 1:
      return times_1;
    case 2:
      return times_2;
    case 4:
      return times_4;
    case 8:
      return times_8;
    default:
      UNREACHABLE();
  }
}

class MaglevAssembler::ScratchRegisterScope {
 public:
  explicit ScratchRegisterScope(MaglevAssembler* masm)
      : masm_(masm),
        prev_scope_(masm->scratch_register_scope_),
        available_(masm->scratch_register_scope_
                       ? masm_->scratch_register_scope_->available_
                       : RegList()),
        available_double_(
            masm->scratch_register_scope_
                ? masm_->scratch_register_scope_->available_double_
                : DoubleRegList()) {
    masm_->scratch_register_scope_ = this;
  }
  ~ScratchRegisterScope() { masm_->scratch_register_scope_ = prev_scope_; }

  void ResetToDefault() {
    available_ = {};
    available_double_ = {};
  }

  Register GetDefaultScratchRegister() { return kScratchRegister; }
  DoubleRegister GetDefaultScratchDoubleRegister() { return kScratchDoubleReg; }

  Register Acquire() { return available_.PopFirst(); }
  void Include(Register reg) { available_.set(reg); }
  void Include(const RegList list) { available_ = available_ | list; }

  DoubleRegister AcquireDouble() { return available_double_.PopFirst(); }
  void IncludeDouble(const DoubleRegList list) {
    available_double_ = available_double_ | list;
  }

  RegList Available() { return available_; }
  void SetAvailable(RegList list) { available_ = list; }

  DoubleRegList AvailableDouble() { return available_double_; }
  void SetAvailableDouble(DoubleRegList list) { available_double_ = list; }

 private:
  MaglevAssembler* masm_;
  ScratchRegisterScope* prev_scope_;
  RegList available_;
  DoubleRegList available_double_;
};

inline MapCompare::MapCompare(MaglevAssembler* masm, Register object,
                              size_t map_count)
    : masm_(masm), object_(object), map_count_(map_count) {
  if (map_count_ != 1) {
    map_ = masm_->scratch_register_scope()->Acquire();
    masm_->LoadMap(map_, object_);
  }
}

void MapCompare::Generate(Handle<Map> map) {
  if (map_count_ == 1) {
    masm_->Cmp(FieldOperand(object_, HeapObject::kMapOffset), map);
  } else {
    masm_->CompareTagged(map_, map);
  }
}

Register MapCompare::GetMap() {
  if (map_count_ == 1) {
    DCHECK_EQ(map_, Register::no_reg());
    // Load the map; the object is in register_for_map_compare_. This
    // avoids loading the map in the fast path of CheckMapsWithMigration.
    masm_->LoadMap(kScratchRegister, object_);
    return kScratchRegister;
  } else {
    DCHECK_NE(map_, Register::no_reg());
    return map_;
  }
}

int MapCompare::TemporaryCount(size_t map_count) {
  return map_count == 1 ? 0 : 1;
}

namespace detail {

template <typename... Args>
struct PushAllHelper;

template <>
struct PushAllHelper<> {
  static void Push(MaglevAssembler* masm) {}
  static void PushReverse(MaglevAssembler* masm) {}
};

inline void PushInput(MaglevAssembler* masm, const Input& input) {
  if (input.operand().IsConstant()) {
    input.node()->LoadToRegister(masm, kScratchRegister);
    masm->Push(kScratchRegister);
  } else {
    // TODO(leszeks): Consider special casing the value. (Toon: could possibly
    // be done through Input directly?)
    const compiler::AllocatedOperand& operand =
        compiler::AllocatedOperand::cast(input.operand());

    if (operand.IsRegister()) {
      masm->Push(operand.GetRegister());
    } else {
      DCHECK(operand.IsStackSlot());
      masm->Push(masm->GetStackSlot(operand));
    }
  }
}

template <typename T, typename... Args>
inline void PushIterator(MaglevAssembler* masm, base::iterator_range<T> range,
                         Args... args) {
  for (auto iter = range.begin(), end = range.end(); iter != end; ++iter) {
    masm->Push(*iter);
  }
  PushAllHelper<Args...>::Push(masm, args...);
}

template <typename T, typename... Args>
inline void PushIteratorReverse(MaglevAssembler* masm,
                                base::iterator_range<T> range, Args... args) {
  PushAllHelper<Args...>::PushReverse(masm, args...);
  for (auto iter = range.rbegin(), end = range.rend(); iter != end; ++iter) {
    masm->Push(*iter);
  }
}

template <typename... Args>
struct PushAllHelper<Input, Args...> {
  static void Push(MaglevAssembler* masm, const Input& arg, Args... args) {
    PushInput(masm, arg);
    PushAllHelper<Args...>::Push(masm, args...);
  }
  static void PushReverse(MaglevAssembler* masm, const Input& arg,
                          Args... args) {
    PushAllHelper<Args...>::PushReverse(masm, args...);
    PushInput(masm, arg);
  }
};
template <typename Arg, typename... Args>
struct PushAllHelper<Arg, Args...> {
  static void Push(MaglevAssembler* masm, Arg arg, Args... args) {
    if constexpr (is_iterator_range<Arg>::value) {
      PushIterator(masm, arg, args...);
    } else {
      masm->MacroAssembler::Push(arg);
      PushAllHelper<Args...>::Push(masm, args...);
    }
  }
  static void PushReverse(MaglevAssembler* masm, Arg arg, Args... args) {
    if constexpr (is_iterator_range<Arg>::value) {
      PushIteratorReverse(masm, arg, args...);
    } else {
      PushAllHelper<Args...>::PushReverse(masm, args...);
      masm->Push(arg);
    }
  }
};

}  // namespace detail

template <typename... T>
void MaglevAssembler::Push(T... vals) {
  detail::PushAllHelper<T...>::Push(this, vals...);
}

template <typename... T>
void MaglevAssembler::PushReverse(T... vals) {
  detail::PushAllHelper<T...>::PushReverse(this, vals...);
}

inline void MaglevAssembler::BindJumpTarget(Label* label) { bind(label); }

inline void MaglevAssembler::BindBlock(BasicBlock* block) {
  bind(block->label());
}

inline void MaglevAssembler::SmiTagInt32AndSetFlags(Register dst,
                                                    Register src) {
  Move(dst, src);
  if (SmiValuesAre31Bits()) {
    addl(dst, dst);
  } else {
    SmiTag(dst);
  }
}

inline void MaglevAssembler::CheckInt32IsSmi(Register obj, Label* fail,
                                             Register scratch) {
  DCHECK(!SmiValuesAre32Bits());

  if (scratch == Register::no_reg()) {
    scratch = kScratchRegister;
  }
  movl(scratch, obj);
  addl(scratch, scratch);
  JumpIf(kOverflow, fail);
}

inline void MaglevAssembler::MoveHeapNumber(Register dst, double value) {
  movq_heap_number(dst, value);
}

inline Condition MaglevAssembler::IsRootConstant(Input input,
                                                 RootIndex root_index) {
  if (input.operand().IsRegister()) {
    CompareRoot(ToRegister(input), root_index);
  } else {
    DCHECK(input.operand().IsStackSlot());
    CompareRoot(ToMemOperand(input), root_index);
  }
  return equal;
}

inline MemOperand MaglevAssembler::GetStackSlot(
    const compiler::AllocatedOperand& operand) {
  return MemOperand(rbp, GetFramePointerOffsetForStackSlot(operand));
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
      data_pointer, FieldOperand(object, JSTypedArray::kExternalPointerOffset));
  if (JSTypedArray::kMaxSizeInHeap == 0) return;

  Register base = kScratchRegister;
  if (COMPRESS_POINTERS_BOOL) {
    movl(base, FieldOperand(object, JSTypedArray::kBasePointerOffset));
  } else {
    movq(base, FieldOperand(object, JSTypedArray::kBasePointerOffset));
  }
  addq(data_pointer, base);
}

inline MemOperand MaglevAssembler::TypedArrayElementOperand(
    Register data_pointer, Register index, int element_size) {
  return Operand(data_pointer, index, ScaleFactorFromInt(element_size), 0);
}

inline MemOperand MaglevAssembler::DataViewElementOperand(Register data_pointer,
                                                          Register index) {
  return Operand(data_pointer, index, times_1, 0);
}

inline void MaglevAssembler::LoadTaggedFieldByIndex(Register result,
                                                    Register object,
                                                    Register index, int scale,
                                                    int offset) {
  LoadTaggedField(
      result, FieldOperand(object, index, ScaleFactorFromInt(scale), offset));
}

inline void MaglevAssembler::LoadBoundedSizeFromObject(Register result,
                                                       Register object,
                                                       int offset) {
  movq(result, FieldOperand(object, offset));
#ifdef V8_ENABLE_SANDBOX
  shrq(result, Immediate(kBoundedSizeShift));
#endif  // V8_ENABLE_SANDBOX
}

inline void MaglevAssembler::LoadExternalPointerField(Register result,
                                                      Operand operand) {
#ifdef V8_ENABLE_SANDBOX
  LoadSandboxedPointerField(result, operand);
#else
  movq(result, operand);
#endif
}

void MaglevAssembler::LoadFixedArrayElement(Register result, Register array,
                                            Register index) {
  if (v8_flags.debug_code) {
    AssertNotSmi(array);
    IsObjectType(array, FIXED_ARRAY_TYPE);
    Assert(kEqual, AbortReason::kUnexpectedValue);
    CompareInt32(index, 0);
    Assert(kUnsignedGreaterThanEqual, AbortReason::kUnexpectedNegativeValue);
  }
  LoadTaggedFieldByIndex(result, array, index, kTaggedSize,
                         FixedArray::kHeaderSize);
}

void MaglevAssembler::LoadFixedArrayElementWithoutDecompressing(
    Register result, Register array, Register index) {
  if (v8_flags.debug_code) {
    AssertNotSmi(array);
    IsObjectType(array, FIXED_ARRAY_TYPE);
    Assert(kEqual, AbortReason::kUnexpectedValue);
    CompareInt32(index, 0);
    Assert(kUnsignedGreaterThanEqual, AbortReason::kUnexpectedNegativeValue);
  }
  MacroAssembler::LoadTaggedFieldWithoutDecompressing(
      result,
      FieldOperand(array, index, times_tagged_size, FixedArray::kHeaderSize));
}

void MaglevAssembler::LoadFixedDoubleArrayElement(DoubleRegister result,
                                                  Register array,
                                                  Register index) {
  if (v8_flags.debug_code) {
    AssertNotSmi(array);
    IsObjectType(array, FIXED_DOUBLE_ARRAY_TYPE);
    Assert(kEqual, AbortReason::kUnexpectedValue);
    CompareInt32(index, 0);
    Assert(kUnsignedGreaterThanEqual, AbortReason::kUnexpectedNegativeValue);
  }
  Movsd(result,
        FieldOperand(array, index, times_8, FixedDoubleArray::kHeaderSize));
}

inline void MaglevAssembler::StoreFixedDoubleArrayElement(
    Register array, Register index, DoubleRegister value) {
  Movsd(FieldOperand(array, index, times_8, FixedDoubleArray::kHeaderSize),
        value);
}

inline void MaglevAssembler::LoadSignedField(Register result, Operand operand,
                                             int size) {
  if (size == 1) {
    movsxbl(result, operand);
  } else if (size == 2) {
    movsxwl(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    movl(result, operand);
  }
}

inline void MaglevAssembler::LoadUnsignedField(Register result, Operand operand,
                                               int size) {
  if (size == 1) {
    movzxbl(result, operand);
  } else if (size == 2) {
    movzxwl(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    movl(result, operand);
  }
}

inline void MaglevAssembler::SetSlotAddressForTaggedField(Register slot_reg,
                                                          Register object,
                                                          int offset) {
  leaq(slot_reg, FieldOperand(object, offset));
}
inline void MaglevAssembler::SetSlotAddressForFixedArrayElement(
    Register slot_reg, Register object, Register index) {
  leaq(slot_reg,
       FieldOperand(object, index, times_tagged_size, FixedArray::kHeaderSize));
}

inline void MaglevAssembler::StoreTaggedFieldNoWriteBarrier(Register object,
                                                            int offset,
                                                            Register value) {
  MacroAssembler::StoreTaggedField(FieldOperand(object, offset), value);
}

inline void MaglevAssembler::StoreFixedArrayElementNoWriteBarrier(
    Register array, Register index, Register value) {
  MacroAssembler::StoreTaggedField(
      FieldOperand(array, index, times_tagged_size, FixedArray::kHeaderSize),
      value);
}

inline void MaglevAssembler::StoreTaggedSignedField(Register object, int offset,
                                                    Register value) {
  AssertSmi(value);
  MacroAssembler::StoreTaggedField(FieldOperand(object, offset), value);
}

inline void MaglevAssembler::StoreTaggedSignedField(Register object, int offset,
                                                    Tagged<Smi> value) {
  MacroAssembler::StoreTaggedSignedField(FieldOperand(object, offset), value);
}

inline void MaglevAssembler::StoreInt32Field(Register object, int offset,
                                             int32_t value) {
  movl(FieldOperand(object, offset), Immediate(value));
}

inline void MaglevAssembler::StoreField(Operand operand, Register value,
                                        int size) {
  DCHECK(size == 1 || size == 2 || size == 4);
  if (size == 1) {
    movb(operand, value);
  } else if (size == 2) {
    movw(operand, value);
  } else {
    DCHECK_EQ(size, 4);
    movl(operand, value);
  }
}

inline void MaglevAssembler::ReverseByteOrder(Register value, int size) {
  if (size == 2) {
    bswapl(value);
    sarl(value, Immediate(16));
  } else if (size == 4) {
    bswapl(value);
  } else {
    DCHECK_EQ(size, 1);
  }
}

inline MemOperand MaglevAssembler::StackSlotOperand(StackSlot stack_slot) {
  return MemOperand(rbp, stack_slot.index);
}

inline void MaglevAssembler::IncrementInt32(Register reg) { incl(reg); }

inline void MaglevAssembler::Move(StackSlot dst, Register src) {
  movq(StackSlotOperand(dst), src);
}

inline void MaglevAssembler::Move(StackSlot dst, DoubleRegister src) {
  Movsd(StackSlotOperand(dst), src);
}

inline void MaglevAssembler::Move(Register dst, StackSlot src) {
  movq(dst, StackSlotOperand(src));
}

inline void MaglevAssembler::Move(DoubleRegister dst, StackSlot src) {
  Movsd(dst, StackSlotOperand(src));
}

inline void MaglevAssembler::Move(MemOperand dst, Register src) {
  movq(dst, src);
}

inline void MaglevAssembler::Move(Register dst, Tagged<TaggedIndex> i) {
  MacroAssembler::Move(dst, i);
}

inline void MaglevAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, Tagged<Smi> src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, ExternalReference src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, MemOperand src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, Register src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, int32_t i) {
  // Move as a uint32 to avoid sign extension.
  MacroAssembler::Move(dst, static_cast<uint32_t>(i));
}

inline void MaglevAssembler::Move(DoubleRegister dst, double n) {
  MacroAssembler::Move(dst, n);
}

inline void MaglevAssembler::Move(DoubleRegister dst, Float64 n) {
  MacroAssembler::Move(dst, n.get_bits());
}

inline void MaglevAssembler::Move(Register dst, Handle<HeapObject> obj) {
  MacroAssembler::Move(dst, obj);
}

inline void MaglevAssembler::LoadFloat32(DoubleRegister dst, MemOperand src) {
  Movss(dst, src);
  Cvtss2sd(dst, dst);
}
inline void MaglevAssembler::StoreFloat32(MemOperand dst, DoubleRegister src) {
  Cvtsd2ss(kScratchDoubleReg, src);
  Movss(dst, kScratchDoubleReg);
}
inline void MaglevAssembler::LoadFloat64(DoubleRegister dst, MemOperand src) {
  Movsd(dst, src);
}
inline void MaglevAssembler::StoreFloat64(MemOperand dst, DoubleRegister src) {
  Movsd(dst, src);
}

inline void MaglevAssembler::LoadUnalignedFloat64(DoubleRegister dst,
                                                  Register base,
                                                  Register index) {
  LoadFloat64(dst, Operand(base, index, times_1, 0));
}
inline void MaglevAssembler::LoadUnalignedFloat64AndReverseByteOrder(
    DoubleRegister dst, Register base, Register index) {
  movq(kScratchRegister, Operand(base, index, times_1, 0));
  bswapq(kScratchRegister);
  Movq(dst, kScratchRegister);
}
inline void MaglevAssembler::StoreUnalignedFloat64(Register base,
                                                   Register index,
                                                   DoubleRegister src) {
  StoreFloat64(Operand(base, index, times_1, 0), src);
}
inline void MaglevAssembler::ReverseByteOrderAndStoreUnalignedFloat64(
    Register base, Register index, DoubleRegister src) {
  Movq(kScratchRegister, src);
  bswapq(kScratchRegister);
  movq(Operand(base, index, times_1, 0), kScratchRegister);
}

inline void MaglevAssembler::SignExtend32To64Bits(Register dst, Register src) {
  movsxlq(dst, src);
}
inline void MaglevAssembler::NegateInt32(Register val) { negl(val); }

inline void MaglevAssembler::ToUint8Clamped(Register result,
                                            DoubleRegister value, Label* min,
                                            Label* max, Label* done) {
  DCHECK(CpuFeatures::IsSupported(SSE4_1));
  Move(kScratchDoubleReg, 0.0);
  Ucomisd(kScratchDoubleReg, value);
  // Set to 0 if NaN.
  j(parity_even, min);
  j(above_equal, min);
  Move(kScratchDoubleReg, 255.0);
  Ucomisd(value, kScratchDoubleReg);
  j(above_equal, max);
  // if value in [0, 255], then round up to the nearest.
  Roundsd(kScratchDoubleReg, value, kRoundToNearest);
  TruncateDoubleToInt32(result, kScratchDoubleReg);
  jmp(done);
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
                    FieldOperand(array, JSArrayBufferView::kBufferOffset));
    LoadTaggedField(scratch,
                    FieldOperand(scratch, JSArrayBuffer::kBitFieldOffset));
    testl(scratch, Immediate(JSArrayBuffer::WasDetachedBit::kMask));
    EmitEagerDeoptIf(not_zero, DeoptimizeReason::kArrayBufferWasDetached, node);
  }
}

inline void MaglevAssembler::LoadByte(Register dst, MemOperand src) {
  movzxbl(dst, src);
}

inline Condition MaglevAssembler::IsCallableAndNotUndetectable(
    Register map, Register scratch) {
  movl(scratch, FieldOperand(map, Map::kBitFieldOffset));
  andl(scratch, Immediate(Map::Bits1::IsUndetectableBit::kMask |
                          Map::Bits1::IsCallableBit::kMask));
  cmpl(scratch, Immediate(Map::Bits1::IsCallableBit::kMask));
  return kEqual;
}

inline Condition MaglevAssembler::IsNotCallableNorUndetactable(
    Register map, Register scratch) {
  testl(FieldOperand(map, Map::kBitFieldOffset),
        Immediate(Map::Bits1::IsUndetectableBit::kMask |
                  Map::Bits1::IsCallableBit::kMask));
  return kEqual;
}

inline void MaglevAssembler::LoadInstanceType(Register instance_type,
                                              Register heap_object) {
  LoadMap(instance_type, heap_object);
  movzxwl(instance_type, FieldOperand(instance_type, Map::kInstanceTypeOffset));
}

inline void MaglevAssembler::IsObjectType(Register heap_object,
                                          InstanceType type) {
  MacroAssembler::IsObjectType(heap_object, type, kScratchRegister);
}

inline void MaglevAssembler::CompareObjectType(Register heap_object,
                                               InstanceType type) {
  LoadMap(kScratchRegister, heap_object);
  CmpInstanceType(kScratchRegister, type);
}

inline void MaglevAssembler::JumpIfJSAnyIsNotPrimitive(
    Register heap_object, Label* target, Label::Distance distance) {
  MacroAssembler::JumpIfJSAnyIsNotPrimitive(heap_object, kScratchRegister,
                                            target, distance);
}

inline void MaglevAssembler::CompareObjectType(Register heap_object,
                                               InstanceType type,
                                               Register scratch) {
  CompareObjectType(heap_object, type);
}

inline void MaglevAssembler::CompareObjectTypeRange(Register heap_object,
                                                    InstanceType lower_limit,
                                                    InstanceType higher_limit) {
  CompareObjectTypeRange(heap_object, kScratchRegister, lower_limit,
                         higher_limit);
}

inline void MaglevAssembler::CompareObjectTypeRange(Register heap_object,
                                                    Register scratch,
                                                    InstanceType lower_limit,
                                                    InstanceType higher_limit) {
  LoadMap(scratch, heap_object);
  CmpInstanceTypeRange(scratch, scratch, lower_limit, higher_limit);
}

inline void MaglevAssembler::CompareMapWithRoot(Register object,
                                                RootIndex index,
                                                Register scratch) {
  if (CanBeImmediate(index)) {
    cmp_tagged(FieldOperand(object, HeapObject::kMapOffset),
               Immediate(static_cast<uint32_t>(ReadOnlyRootPtr(index))));
    return;
  }
  LoadMap(scratch, object);
  CompareRoot(scratch, index);
}

inline void MaglevAssembler::CompareInstanceType(Register map,
                                                 InstanceType instance_type) {
  CmpInstanceType(map, instance_type);
}

inline void MaglevAssembler::CompareInstanceTypeRange(
    Register map, Register instance_type_out, InstanceType lower_limit,
    InstanceType higher_limit) {
  CmpInstanceTypeRange(map, instance_type_out, lower_limit, higher_limit);
}

inline void MaglevAssembler::CompareTagged(Register reg, Tagged<Smi> obj) {
  Cmp(reg, obj);
}

inline void MaglevAssembler::CompareTagged(Register reg,
                                           Handle<HeapObject> obj) {
  Cmp(reg, obj);
}

inline void MaglevAssembler::CompareTagged(Register src1, Register src2) {
  cmp_tagged(src1, src2);
}

inline void MaglevAssembler::CompareInt32(Register reg, int32_t imm) {
  Cmp(reg, imm);
}

inline void MaglevAssembler::CompareInt32(Register src1, Register src2) {
  cmpl(src1, src2);
}

inline void MaglevAssembler::CompareFloat64(DoubleRegister src1,
                                            DoubleRegister src2) {
  Ucomisd(src1, src2);
}

inline void MaglevAssembler::PrepareCallCFunction(int num_reg_arguments,
                                                  int num_double_registers) {
  MacroAssembler::PrepareCallCFunction(num_reg_arguments +
                                       num_double_registers);
}

inline void MaglevAssembler::CallSelf() {
  DCHECK(allow_call());
  DCHECK(code_gen_state()->entry_label()->is_bound());
  Call(code_gen_state()->entry_label());
}

inline void MaglevAssembler::Jump(Label* target, Label::Distance distance) {
  // Any eager deopts should go through JumpIf to enable us to support the
  // `--deopt-every-n-times` stress mode. See EmitEagerDeoptStress.
  DCHECK(!IsDeoptLabel(target));
  jmp(target, distance);
}

inline void MaglevAssembler::JumpToDeopt(Label* target) {
  DCHECK(IsDeoptLabel(target));
  jmp(target);
}

inline void MaglevAssembler::EmitEagerDeoptStress(Label* target) {
  if (V8_LIKELY(v8_flags.deopt_every_n_times <= 0)) {
    return;
  }

  ExternalReference counter = ExternalReference::stress_deopt_count(isolate());

  Label fallthrough;
  pushfq();
  pushq(rax);
  load_rax(counter);
  decl(rax);
  JumpIf(not_zero, &fallthrough, Label::kNear);

  RecordComment("-- deopt_every_n_times hit, jump to eager deopt");
  Move(rax, v8_flags.deopt_every_n_times);
  store_rax(counter);
  popq(rax);
  popfq();
  JumpToDeopt(target);

  bind(&fallthrough);
  store_rax(counter);
  popq(rax);
  popfq();
}

inline void MaglevAssembler::JumpIf(Condition cond, Label* target,
                                    Label::Distance distance) {
  // The least common denominator of all eager deopts is that they eventually
  // (should) bottom out in `JumpIf`. We use the opportunity here to trigger
  // extra eager deoptimizations with the `--deopt-every-n-times` stress mode.
  // Since `IsDeoptLabel` is slow we duplicate the test for the flag here.
  if (V8_UNLIKELY(v8_flags.deopt_every_n_times > 0)) {
    if (IsDeoptLabel(target)) {
      EmitEagerDeoptStress(target);
    }
  }
  j(cond, target, distance);
}

inline void MaglevAssembler::JumpIfRoot(Register with, RootIndex index,
                                        Label* if_equal,
                                        Label::Distance distance) {
  MacroAssembler::JumpIfRoot(with, index, if_equal, distance);
}

inline void MaglevAssembler::JumpIfNotRoot(Register with, RootIndex index,
                                           Label* if_not_equal,
                                           Label::Distance distance) {
  MacroAssembler::JumpIfNotRoot(with, index, if_not_equal, distance);
}

inline void MaglevAssembler::JumpIfSmi(Register src, Label* on_smi,
                                       Label::Distance distance) {
  MacroAssembler::JumpIfSmi(src, on_smi, distance);
}

inline void MaglevAssembler::JumpIfNotSmi(Register src, Label* on_not_smi,
                                          Label::Distance distance) {
  MacroAssembler::JumpIfNotSmi(src, on_not_smi, distance);
}

void MaglevAssembler::JumpIfByte(Condition cc, Register value, int32_t byte,
                                 Label* target, Label::Distance distance) {
  cmpb(value, Immediate(byte));
  j(cc, target, distance);
}

void MaglevAssembler::JumpIfHoleNan(DoubleRegister value, Register scratch,
                                    Label* target, Label::Distance distance) {
  Movq(scratch, value);
  movq(kScratchRegister, kHoleNanInt64);
  cmpq(scratch, kScratchRegister);
  JumpIf(kEqual, target, distance);
}

void MaglevAssembler::JumpIfNotHoleNan(DoubleRegister value, Register scratch,
                                       Label* target,
                                       Label::Distance distance) {
  Movq(scratch, value);
  movq(kScratchRegister, kHoleNanInt64);
  cmpq(scratch, kScratchRegister);
  JumpIf(kNotEqual, target, distance);
}

void MaglevAssembler::JumpIfNotHoleNan(MemOperand operand, Label* target,
                                       Label::Distance distance) {
  movq(kScratchRegister, kHoleNanInt64);
  cmpq(operand, kScratchRegister);
  JumpIf(kNotEqual, target, distance);
}

void MaglevAssembler::CompareInt32AndJumpIf(Register r1, Register r2,
                                            Condition cond, Label* target,
                                            Label::Distance distance) {
  CompareInt32(r1, r2);
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, int32_t value,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  CompareInt32(r1, value);
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareSmiAndJumpIf(Register r1, Tagged<Smi> value,
                                                 Condition cond, Label* target,
                                                 Label::Distance distance) {
  AssertSmi(r1);
  Cmp(r1, value);
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareByteAndJumpIf(MemOperand left, int8_t right,
                                                  Condition cond,
                                                  Register scratch,
                                                  Label* target,
                                                  Label::Distance distance) {
  cmpb(left, Immediate(right));
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareTaggedAndJumpIf(Register r1,
                                                    Tagged<Smi> value,
                                                    Condition cond,
                                                    Label* target,
                                                    Label::Distance distance) {
  Cmp(r1, value);
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareDoubleAndJumpIfZeroOrNaN(
    DoubleRegister reg, Label* target, Label::Distance distance) {
  // Sets scratch register to 0.0.
  Xorpd(kScratchDoubleReg, kScratchDoubleReg);
  // Sets ZF if equal to 0.0, -0.0 or NaN.
  Ucomisd(kScratchDoubleReg, reg);
  JumpIf(kZero, target, distance);
}

inline void MaglevAssembler::CompareDoubleAndJumpIfZeroOrNaN(
    MemOperand operand, Label* target, Label::Distance distance) {
  // Sets scratch register to 0.0.
  Xorpd(kScratchDoubleReg, kScratchDoubleReg);
  // Sets ZF if equal to 0.0, -0.0 or NaN.
  Ucomisd(kScratchDoubleReg, operand);
  JumpIf(kZero, target, distance);
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    Register r1, int32_t mask, Label* target, Label::Distance distance) {
  testl(r1, Immediate(mask));
  JumpIf(kNotZero, target, distance);
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    MemOperand operand, int32_t mask, Label* target, Label::Distance distance) {
  testl(operand, Immediate(mask));
  JumpIf(kNotZero, target, distance);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    Register r1, int32_t mask, Label* target, Label::Distance distance) {
  testl(r1, Immediate(mask));
  JumpIf(kZero, target, distance);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    MemOperand operand, int32_t mask, Label* target, Label::Distance distance) {
  testl(operand, Immediate(mask));
  JumpIf(kZero, target, distance);
}

inline void MaglevAssembler::LoadHeapNumberValue(DoubleRegister result,
                                                 Register heap_number) {
  Movsd(result, FieldOperand(heap_number, HeapNumber::kValueOffset));
}

inline void MaglevAssembler::Int32ToDouble(DoubleRegister result,
                                           Register src) {
  Cvtlsi2sd(result, src);
}

inline void MaglevAssembler::Uint32ToDouble(DoubleRegister result,
                                            Register src) {
  // TODO(leszeks): Cvtlui2sd does a manual movl to clear the top bits of the
  // input register. We could eliminate this movl by ensuring that word32
  // registers are always written with 32-bit ops and not 64-bit ones.
  Cvtlui2sd(result, src);
}

inline void MaglevAssembler::Pop(Register dst) { MacroAssembler::Pop(dst); }

template <typename NodeT>
inline void MaglevAssembler::EmitEagerDeoptIfNotEqual(DeoptimizeReason reason,
                                                      NodeT* node) {
  EmitEagerDeoptIf(not_equal, reason, node);
}

inline void MaglevAssembler::AssertStackSizeCorrect() {
  if (v8_flags.debug_code) {
    movq(kScratchRegister, rbp);
    subq(kScratchRegister, rsp);
    cmpq(kScratchRegister,
         Immediate(code_gen_state()->stack_slots() * kSystemPointerSize +
                   StandardFrameConstants::kFixedFrameSizeFromFp));
    Assert(equal, AbortReason::kStackAccessBelowStackPointer);
  }
}

inline Condition MaglevAssembler::FunctionEntryStackCheck(
    int stack_check_offset) {
  Register stack_cmp_reg = rsp;
  if (stack_check_offset > kStackLimitSlackForDeoptimizationInBytes) {
    stack_cmp_reg = kScratchRegister;
    leaq(stack_cmp_reg, Operand(rsp, -stack_check_offset));
  }
  cmpq(stack_cmp_reg,
       StackLimitAsOperand(StackLimitKind::kInterruptStackLimit));
  return kUnsignedGreaterThanEqual;
}

inline void MaglevAssembler::FinishCode() {}

template <typename Dest, typename Source>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr, Dest dst,
                                      Source src) {
  switch (repr) {
    case MachineRepresentation::kWord32:
      return movl(dst, src);
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
      return movq(dst, src);
    default:
      UNREACHABLE();
  }
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr,
                                      MemOperand dst, MemOperand src) {
  MoveRepr(repr, kScratchRegister, src);
  MoveRepr(repr, dst, kScratchRegister);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_X64_MAGLEV_ASSEMBLER_X64_INL_H_
