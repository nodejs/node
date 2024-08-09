// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_S390_MAGLEV_ASSEMBLER_S390_INL_H_
#define V8_MAGLEV_S390_MAGLEV_ASSEMBLER_S390_INL_H_

#include "src/base/numbers/double.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/s390/assembler-s390.h"
#include "src/common/globals.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/maglev/maglev-assembler.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-code-gen-state.h"

namespace v8 {
namespace internal {
namespace maglev {

constexpr Condition ConditionForFloat64(Operation operation) {
  return ConditionFor(operation);
}

// constexpr Condition ConditionForNaN() { return vs; }

inline int ShiftFromScale(int n) {
  switch (n) {
    case 1:
      return 0;
    case 2:
      return 1;
    case 4:
      return 2;
    case 8:
      return 3;
    default:
      UNREACHABLE();
  }
}

class MaglevAssembler::ScratchRegisterScope {
 public:
  explicit ScratchRegisterScope(MaglevAssembler* masm)
      : wrapped_scope_(masm),
        masm_(masm),
        prev_scope_(masm->scratch_register_scope_) {
    masm_->scratch_register_scope_ = this;
    if (prev_scope_ == nullptr) {
      // Add extra scratch register if no previous scope.
      // wrapped_scope_.Include(kMaglevExtraScratchRegister);
    }
  }

  ~ScratchRegisterScope() { masm_->scratch_register_scope_ = prev_scope_; }

  void ResetToDefault() {
    SetAvailable(Assembler::DefaultTmpList());
    SetAvailableDouble(Assembler::DefaultFPTmpList());
  }

  Register GetDefaultScratchRegister() { return Acquire(); }
  DoubleRegister GetDefaultScratchDoubleRegister() { return AcquireDouble(); }

  Register Acquire() { return wrapped_scope_.Acquire(); }
  void Include(Register reg) { wrapped_scope_.Include(reg); }
  void Include(const RegList list) { wrapped_scope_.Include(list); }

  DoubleRegister AcquireDouble() { return wrapped_scope_.AcquireDouble(); }
  void IncludeDouble(const DoubleRegList list) { wrapped_scope_.Include(list); }

  RegList Available() { return wrapped_scope_.Available(); }
  void SetAvailable(RegList list) { wrapped_scope_.SetAvailable(list); }

  DoubleRegList AvailableDouble() {
    return wrapped_scope_.AvailableDoubleRegList();
  }
  void SetAvailableDouble(DoubleRegList list) {
    wrapped_scope_.SetAvailableDoubleRegList(list);
  }

 private:
  UseScratchRegisterScope wrapped_scope_;
  MaglevAssembler* masm_;
  ScratchRegisterScope* prev_scope_;
};

inline MapCompare::MapCompare(MaglevAssembler* masm, Register object,
                              size_t map_count)
    : masm_(masm), object_(object), map_count_(map_count) {
  map_ = masm_->scratch_register_scope()->Acquire();
  masm_->LoadMap(map_, object_);
  USE(map_count_);
}

void MapCompare::Generate(Handle<Map> map, Condition cond, Label* if_true,
                          Label::Distance distance) {
  MaglevAssembler::ScratchRegisterScope temps(masm_);
  Register temp = temps.Acquire();
  masm_->Move(temp, map);
  masm_->CmpS64(map_, temp);
  CHECK(is_signed(cond));
  masm_->JumpIf(cond, if_true, distance);
}

Register MapCompare::GetMap() { return map_; }

int MapCompare::TemporaryCount(size_t map_count) { return 1; }

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
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    input.node()->LoadToRegister(masm, scratch);
    masm->Push(scratch);
  } else {
    // TODO(leszeks): Consider special casing the value. (Toon: could possibly
    // be done through Input directly?)
    const compiler::AllocatedOperand& operand =
        compiler::AllocatedOperand::cast(input.operand());
    if (operand.IsRegister()) {
      masm->Push(operand.GetRegister());
    } else {
      DCHECK(operand.IsStackSlot());
      masm->LoadU64(r0, masm->GetStackSlot(operand));
      masm->Push(r0);
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
  if (SmiValuesAre31Bits()) {
    AddS32(dst, src, src);
  } else {
    SmiTag(dst, src);
  }
}

inline void MaglevAssembler::CheckInt32IsSmi(Register obj, Label* fail,
                                             Register scratch) {
  DCHECK(!SmiValuesAre32Bits());
  if (scratch == Register::no_reg()) {
    scratch = r0;
  }
  mov(scratch, obj);
  AddS32(scratch, scratch);
  JumpIf(kOverflow, fail);
}

inline void MaglevAssembler::SmiAddConstant(Register dst, Register src,
                                            int value, Label* fail,
                                            Label::Distance distance) {
  AssertSmi(src);
  Move(dst, src);
  if (value != 0) {
    Register scratch = r0;
    Move(scratch, Smi::FromInt(value));
    if (SmiValuesAre31Bits()) {
      AddS32(dst, scratch);
    } else {
      AddS64(dst, scratch);
    }
    JumpIf(kOverflow, fail, distance);
  }
}

inline void MaglevAssembler::SmiSubConstant(Register dst, Register src,
                                            int value, Label* fail,
                                            Label::Distance distance) {
  AssertSmi(src);
  Move(dst, src);
  if (value != 0) {
    Register scratch = r0;
    Move(scratch, Smi::FromInt(value));
    if (SmiValuesAre31Bits()) {
      SubS32(dst, scratch);
    } else {
      SubS64(dst, scratch);
    }
    JumpIf(kOverflow, fail, distance);
  }
}

inline void MaglevAssembler::MoveHeapNumber(Register dst, double value) {
  mov(dst, Operand::EmbeddedNumber(value));
}

inline Condition MaglevAssembler::IsRootConstant(Input input,
                                                 RootIndex root_index) {
  if (input.operand().IsRegister()) {
    CompareRoot(ToRegister(input), root_index);
  } else {
    DCHECK(input.operand().IsStackSlot());
    ScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    LoadU64(scratch, ToMemOperand(input), scratch);
    CompareRoot(scratch, root_index);
  }
  return eq;
}

inline MemOperand MaglevAssembler::StackSlotOperand(StackSlot slot) {
  return MemOperand(fp, slot.index);
}

inline Register MaglevAssembler::GetFramePointer() { return fp; }

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
  // ScratchRegisterScope temps(this);
  Register base = r0;
  if (COMPRESS_POINTERS_BOOL) {
    LoadU32(base, FieldMemOperand(object, JSTypedArray::kBasePointerOffset));
  } else {
    LoadU64(base, FieldMemOperand(object, JSTypedArray::kBasePointerOffset));
  }
  AddU64(data_pointer, data_pointer, base);
}

inline MemOperand MaglevAssembler::TypedArrayElementOperand(
    Register data_pointer, Register index, int element_size) {
  // ScratchRegisterScope temps(this);
  Register temp = r0;
  ShiftLeftU64(temp, index, Operand(ShiftFromScale(element_size)));
  AddU64(data_pointer, data_pointer, temp);
  return MemOperand(data_pointer);
}

inline MemOperand MaglevAssembler::DataViewElementOperand(Register data_pointer,
                                                          Register index) {
  return MemOperand(data_pointer, index);
}

inline void MaglevAssembler::LoadTaggedFieldByIndex(Register result,
                                                    Register object,
                                                    Register index, int scale,
                                                    int offset) {
  ShiftLeftU64(result, index, Operand(ShiftFromScale(scale)));
  AddU64(result, result, object);
  MacroAssembler::LoadTaggedField(result, FieldMemOperand(result, offset));
}

inline void MaglevAssembler::LoadBoundedSizeFromObject(Register result,
                                                       Register object,
                                                       int offset) {
  Move(result, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::LoadExternalPointerField(Register result,
                                                      MemOperand operand) {
  Move(result, operand);
}

void MaglevAssembler::LoadFixedArrayElement(Register result, Register array,
                                            Register index) {
  if (v8_flags.debug_code) {
    CompareObjectTypeAndAssert(array, FIXED_ARRAY_TYPE, kEqual,
                               AbortReason::kUnexpectedValue);
    CompareInt32AndAssert(index, 0, kUnsignedGreaterThanEqual,
                          AbortReason::kUnexpectedNegativeValue);
  }
  LoadTaggedFieldByIndex(result, array, index, kTaggedSize,
                         FixedArray::kHeaderSize);
}

void MaglevAssembler::LoadFixedArrayElementWithoutDecompressing(
    Register result, Register array, Register index) {
  if (v8_flags.debug_code) {
    CompareObjectTypeAndAssert(array, FIXED_ARRAY_TYPE, kEqual,
                               AbortReason::kUnexpectedValue);
    CompareInt32AndAssert(index, 0, kUnsignedGreaterThanEqual,
                          AbortReason::kUnexpectedNegativeValue);
  }
  int times_tagged_size = (kTaggedSize == 8) ? 3 : 2;
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  ShiftLeftU64(scratch, index, Operand(times_tagged_size));
  MacroAssembler::LoadTaggedFieldWithoutDecompressing(
      result, FieldMemOperand(array, scratch, FixedArray::kHeaderSize));
}

void MaglevAssembler::LoadFixedDoubleArrayElement(DoubleRegister result,
                                                  Register array,
                                                  Register index) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (v8_flags.debug_code) {
    CompareObjectTypeAndAssert(array, FIXED_DOUBLE_ARRAY_TYPE, eq,
                               AbortReason::kUnexpectedValue);
    CompareInt32AndAssert(index, 0, kUnsignedGreaterThanEqual,
                          AbortReason::kUnexpectedNegativeValue);
  }
  ShiftLeftU64(scratch, index, Operand(kDoubleSizeLog2));
  LoadF64(result,
          FieldMemOperand(array, scratch, FixedDoubleArray::kHeaderSize));
}

inline void MaglevAssembler::StoreFixedDoubleArrayElement(
    Register array, Register index, DoubleRegister value) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  ShiftLeftU64(scratch, index, Operand(kDoubleSizeLog2));
  StoreF64(value,
           FieldMemOperand(array, scratch, FixedDoubleArray::kHeaderSize));
}

inline void MaglevAssembler::LoadSignedField(Register result,
                                             MemOperand operand, int size) {
  if (size == 1) {
    LoadS8(result, operand);
  } else if (size == 2) {
    LoadS16(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    LoadS32(result, operand);
  }
}

inline void MaglevAssembler::LoadUnsignedField(Register result,
                                               MemOperand operand, int size) {
  if (size == 1) {
    LoadU8(result, operand);
  } else if (size == 2) {
    LoadU16(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    LoadU32(result, operand);
  }
}

inline void MaglevAssembler::SetSlotAddressForTaggedField(Register slot_reg,
                                                          Register object,
                                                          int offset) {
  mov(slot_reg, object);
  AddS64(slot_reg, Operand(offset - kHeapObjectTag));
}

inline void MaglevAssembler::SetSlotAddressForFixedArrayElement(
    Register slot_reg, Register object, Register index) {
  // ScratchRegisterScope temps(this);
  Register scratch = r0;
  mov(slot_reg, object);
  AddU64(slot_reg, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  ShiftLeftU64(scratch, index, Operand(kTaggedSizeLog2));
  AddU64(slot_reg, slot_reg, scratch);
}

inline void MaglevAssembler::StoreTaggedFieldNoWriteBarrier(Register object,
                                                            int offset,
                                                            Register value) {
  MacroAssembler::StoreTaggedField(value, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreFixedArrayElementNoWriteBarrier(
    Register array, Register index, Register value) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  ShiftLeftU64(scratch, index, Operand(kTaggedSizeLog2));
  AddU64(scratch, scratch, array);
  MacroAssembler::StoreTaggedField(
      value, FieldMemOperand(scratch, FixedArray::kHeaderSize));
}

inline void MaglevAssembler::StoreTaggedSignedField(Register object, int offset,
                                                    Register value) {
  AssertSmi(value);
  MacroAssembler::StoreTaggedField(value, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreTaggedSignedField(Register object, int offset,
                                                    Tagged<Smi> value) {
  ScratchRegisterScope scope(this);
  Register scratch = r0;
  Move(scratch, value);
  MacroAssembler::StoreTaggedField(scratch, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreInt32Field(Register object, int offset,
                                             int32_t value) {
  ScratchRegisterScope scope(this);
  Register scratch = r0;
  Move(scratch, value);
  StoreU32(scratch, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreField(MemOperand operand, Register value,
                                        int size) {
  DCHECK(size == 1 || size == 2 || size == 4);
  if (size == 1) {
    StoreU8(value, operand);
  } else if (size == 2) {
    StoreU16(value, operand);
  } else {
    DCHECK_EQ(size, 4);
    StoreU32(value, operand);
  }
}

inline void MaglevAssembler::ReverseByteOrder(Register value, int size) {
  if (size == 2) {
    lay(sp, MemOperand(sp, -kSystemPointerSize));
    StoreU16(value, MemOperand(sp));
    lrvh(value, MemOperand(sp));
    LoadS16(value, value);
    lay(sp, MemOperand(sp, kSystemPointerSize));
  } else if (size == 4) {
    lrvr(value, value);
    LoadS32(value, value);
  } else {
    DCHECK_EQ(size, 1);
  }
}

inline void MaglevAssembler::IncrementInt32(Register reg) {
  AddS32(reg, Operand(1));
}

inline void MaglevAssembler::DecrementInt32(Register reg) {
  SubS32(reg, Operand(1));
}

inline void MaglevAssembler::AddInt32(Register reg, int amount) {
  AddS32(reg, Operand(amount));
}

inline void MaglevAssembler::AndInt32(Register reg, int mask) {
  And(reg, Operand(mask));
  LoadU32(reg, reg);
}

inline void MaglevAssembler::OrInt32(Register reg, int mask) {
  Or(reg, Operand(mask));
  LoadU32(reg, reg);
}

inline void MaglevAssembler::ShiftLeft(Register reg, int amount) {
  ShiftLeftU32(reg, reg, Operand(amount));
}

inline void MaglevAssembler::IncrementAddress(Register reg, int32_t delta) {
  CHECK(is_int20(delta));
  lay(reg, MemOperand(reg, delta));
}

inline void MaglevAssembler::LoadAddress(Register dst, MemOperand location) {
  lay(dst, location);
}

inline void MaglevAssembler::Call(Label* target) {
  MacroAssembler::Call(target);
}

inline void MaglevAssembler::EmitEnterExitFrame(int extra_slots,
                                                StackFrame::Type frame_type,
                                                Register c_function,
                                                Register scratch) {
  EnterExitFrame(scratch, extra_slots, frame_type);
}

inline void MaglevAssembler::Move(StackSlot dst, Register src) {
  StoreU64(src, StackSlotOperand(dst));
}
inline void MaglevAssembler::Move(StackSlot dst, DoubleRegister src) {
  StoreF64(src, StackSlotOperand(dst));
}
inline void MaglevAssembler::Move(Register dst, StackSlot src) {
  LoadU64(dst, StackSlotOperand(src));
}
inline void MaglevAssembler::Move(DoubleRegister dst, StackSlot src) {
  LoadF64(dst, StackSlotOperand(src));
}
inline void MaglevAssembler::Move(MemOperand dst, Register src) {
  StoreU64(src, dst);
}
inline void MaglevAssembler::Move(Register dst, MemOperand src) {
  LoadU64(dst, src);
}
inline void MaglevAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  if (dst != src) {
    MacroAssembler::Move(dst, src);
  }
}
inline void MaglevAssembler::Move(Register dst, Tagged<Smi> src) {
  MacroAssembler::Move(dst, src);
}
inline void MaglevAssembler::Move(Register dst, ExternalReference src) {
  MacroAssembler::Move(dst, src);
}
inline void MaglevAssembler::Move(Register dst, Register src) {
  if (dst != src) {
    mov(dst, src);
  }
}
inline void MaglevAssembler::Move(Register dst, Tagged<TaggedIndex> i) {
  mov(dst, Operand(i.ptr()));
}
inline void MaglevAssembler::Move(Register dst, int32_t i) {
  mov(dst, Operand(i));
}
inline void MaglevAssembler::Move(DoubleRegister dst, double n) {
  ScratchRegisterScope scope(this);
  Register scratch = scope.Acquire();
  MacroAssembler::LoadF64(dst, n, scratch);
}
inline void MaglevAssembler::Move(DoubleRegister dst, Float64 n) {
  ScratchRegisterScope scope(this);
  Register scratch = scope.Acquire();
  MacroAssembler::LoadF64(dst, n, scratch);
}
inline void MaglevAssembler::Move(Register dst, Handle<HeapObject> obj) {
  MacroAssembler::Move(dst, obj);
}

inline void MaglevAssembler::Move(Register dst, uint32_t i) {
  // Move as a uint32 to avoid sign extension.
  mov(dst, Operand(i));
  LoadU32(dst, dst);
}

void MaglevAssembler::MoveTagged(Register dst, Handle<HeapObject> obj) {
#ifdef V8_COMPRESS_POINTERS
  MacroAssembler::Move(dst, obj, RelocInfo::COMPRESSED_EMBEDDED_OBJECT);
#else
  MacroAssembler::Move(dst, obj);
#endif
}

inline void MaglevAssembler::LoadFloat32(DoubleRegister dst, MemOperand src) {
  MacroAssembler::LoadF32AsF64(dst, src);
}
inline void MaglevAssembler::StoreFloat32(MemOperand dst, DoubleRegister src) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  DoubleRegister double_scratch = temps.AcquireDouble();
  ledbr(double_scratch, src);
  MacroAssembler::StoreF32(double_scratch, dst);
}
inline void MaglevAssembler::LoadFloat64(DoubleRegister dst, MemOperand src) {
  MacroAssembler::LoadF64(dst, src);
}
inline void MaglevAssembler::StoreFloat64(MemOperand dst, DoubleRegister src) {
  MacroAssembler::StoreF64(src, dst);
}

inline void MaglevAssembler::LoadUnalignedFloat64(DoubleRegister dst,
                                                  Register base,
                                                  Register index) {
  LoadF64(dst, MemOperand(base, index));
}
inline void MaglevAssembler::LoadUnalignedFloat64AndReverseByteOrder(
    DoubleRegister dst, Register base, Register index) {
  ScratchRegisterScope scope(this);
  Register scratch = r0;
  LoadU64(scratch, MemOperand(base, index));
  lrvgr(scratch, scratch);
  ldgr(dst, scratch);
}
inline void MaglevAssembler::StoreUnalignedFloat64(Register base,
                                                   Register index,
                                                   DoubleRegister src) {
  StoreF64(src, MemOperand(base, index));
}
inline void MaglevAssembler::ReverseByteOrderAndStoreUnalignedFloat64(
    Register base, Register index, DoubleRegister src) {
  ScratchRegisterScope scope(this);
  Register scratch = r0;
  lgdr(scratch, src);
  lrvgr(scratch, scratch);
  StoreU64(scratch, MemOperand(base, index));
}

inline void MaglevAssembler::SignExtend32To64Bits(Register dst, Register src) {
  // No 64-bit registers.
  LoadS32(dst, src);
}
inline void MaglevAssembler::NegateInt32(Register val) {
  LoadU32(val, val);
  lcgr(val, val);
}

inline void MaglevAssembler::ToUint8Clamped(Register result,
                                            DoubleRegister value, Label* min,
                                            Label* max, Label* done) {
  ScratchRegisterScope temps(this);
  DoubleRegister scratch = temps.AcquireDouble();
  lzdr(kDoubleRegZero);
  CmpF64(kDoubleRegZero, value);
  // Set to 0 if NaN.
  JumpIf(Condition(CC_OF | ge), min);
  LoadF64(scratch, 255.0, r0);
  CmpF64(value, scratch);
  JumpIf(ge, max);
  // if value in [0, 255], then round up to the nearest.
  ConvertDoubleToInt32(result, value, kRoundToNearest);
  Jump(done);
}

template <typename NodeT>
inline void MaglevAssembler::DeoptIfBufferDetached(Register array,
                                                   Register scratch,
                                                   NodeT* node) {
  // A detached buffer leads to megamorphic feedback, so we won't have a deopt
  // loop if we deopt here.
  LoadTaggedField(scratch,
                  FieldMemOperand(array, JSArrayBufferView::kBufferOffset));
  LoadTaggedField(scratch,
                  FieldMemOperand(scratch, JSArrayBuffer::kBitFieldOffset));
  tmll(scratch, Operand(JSArrayBuffer::WasDetachedBit::kMask));
  EmitEagerDeoptIf(ne, DeoptimizeReason::kArrayBufferWasDetached, node);
}

inline void MaglevAssembler::LoadByte(Register dst, MemOperand src) {
  LoadU8(dst, src);
}

inline Condition MaglevAssembler::IsCallableAndNotUndetectable(
    Register map, Register scratch) {
  LoadU32(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  AndP(scratch, Operand(Map::Bits1::IsUndetectableBit::kMask |
                        Map::Bits1::IsCallableBit::kMask));
  CmpS32(scratch, Operand(Map::Bits1::IsCallableBit::kMask));
  return eq;
}

inline Condition MaglevAssembler::IsNotCallableNorUndetactable(
    Register map, Register scratch) {
  LoadU8(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  tmll(scratch, Operand(Map::Bits1::IsUndetectableBit::kMask |
                        Map::Bits1::IsCallableBit::kMask));
  return eq;
}

inline void MaglevAssembler::LoadInstanceType(Register instance_type,
                                              Register heap_object) {
  LoadMap(instance_type, heap_object);
  LoadU16(instance_type,
          FieldMemOperand(instance_type, Map::kInstanceTypeOffset));
}

inline void MaglevAssembler::CompareObjectTypeAndJumpIf(
    Register heap_object, InstanceType type, Condition cond, Label* target,
    Label::Distance distance) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (cond == kEqual || cond == kNotEqual) {
    IsObjectType(heap_object, scratch, scratch, type);
  } else if (is_signed(cond)) {
    CompareObjectType(heap_object, scratch, scratch, type);
  } else {
    CompareObjectType<true>(heap_object, scratch, scratch, type);
  }
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareObjectTypeAndAssert(Register heap_object,
                                                        InstanceType type,
                                                        Condition cond,
                                                        AbortReason reason) {
  AssertNotSmi(heap_object);
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (cond == kEqual || cond == kNotEqual) {
    IsObjectType(heap_object, scratch, scratch, type);
  } else if (is_signed(cond)) {
    CompareObjectType(heap_object, scratch, scratch, type);
  } else {
    CompareObjectType<true>(heap_object, scratch, scratch, type);
  }
  Assert(cond, reason);
}

inline void MaglevAssembler::CompareObjectTypeAndBranch(
    Register heap_object, InstanceType type, Condition condition,
    Label* if_true, Label::Distance true_distance, bool fallthrough_when_true,
    Label* if_false, Label::Distance false_distance,
    bool fallthrough_when_false) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (condition == kEqual || condition == kNotEqual) {
    IsObjectType(heap_object, scratch, scratch, type);
  } else if (is_signed(condition)) {
    CompareObjectType(heap_object, scratch, scratch, type);
  } else {
    CompareObjectType<true>(heap_object, scratch, scratch, type);
  }
  Branch(condition, if_true, true_distance, fallthrough_when_true, if_false,
         false_distance, fallthrough_when_false);
}

inline void MaglevAssembler::JumpIfJSAnyIsNotPrimitive(
    Register heap_object, Label* target, Label::Distance distance) {
  // If the type of the result (stored in its map) is less than
  // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
  static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MacroAssembler::CompareObjectType<true>(heap_object, scratch, scratch,
                                          FIRST_JS_RECEIVER_TYPE);
  JumpIf(ge, target, distance);
}

inline void MaglevAssembler::CompareObjectTypeRange(Register heap_object,
                                                    InstanceType lower_limit,
                                                    InstanceType higher_limit) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  CompareObjectTypeRange(heap_object, scratch, lower_limit, higher_limit);
}

inline void MaglevAssembler::CompareObjectTypeRange(Register heap_object,
                                                    Register scratch,
                                                    InstanceType lower_limit,
                                                    InstanceType higher_limit) {
  LoadMap(scratch, heap_object);
  CompareInstanceTypeRange(scratch, scratch, lower_limit, higher_limit);
}

inline void MaglevAssembler::CompareMapWithRoot(Register object,
                                                RootIndex index,
                                                Register scratch) {
  LoadMap(scratch, object);
  CompareRoot(scratch, index);
}

inline void MaglevAssembler::CompareInstanceType(Register map,
                                                 InstanceType instance_type) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MacroAssembler::CompareInstanceType(map, scratch, instance_type);
}

inline void MaglevAssembler::CompareInstanceTypeRange(
    Register map, Register instance_type_out, InstanceType lower_limit,
    InstanceType higher_limit) {
  MacroAssembler::CompareInstanceTypeRange(map, instance_type_out, lower_limit,
                                           higher_limit);
}

inline void MaglevAssembler::CompareFloat64AndJumpIf(
    DoubleRegister src1, DoubleRegister src2, Condition cond, Label* target,
    Label* nan_failed, Label::Distance distance) {
  CmpF64(src1, src2);
  JumpIf(CC_OF, nan_failed);
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareFloat64AndBranch(
    DoubleRegister src1, DoubleRegister src2, Condition cond,
    BasicBlock* if_true, BasicBlock* if_false, BasicBlock* next_block,
    BasicBlock* nan_failed) {
  CmpF64(src1, src2);
  JumpIf(CC_OF, nan_failed->label());
  Branch(cond, if_true, if_false, next_block);
}

inline void MaglevAssembler::PrepareCallCFunction(int num_reg_arguments,
                                                  int num_double_registers) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MacroAssembler::PrepareCallCFunction(num_reg_arguments, num_double_registers,
                                       scratch);
}

inline void MaglevAssembler::CallSelf() {
  DCHECK(code_gen_state()->entry_label()->is_bound());
  Call(code_gen_state()->entry_label());
}

inline void MaglevAssembler::Jump(Label* target, Label::Distance) {
  // Any eager deopts should go through JumpIf to enable us to support the
  // `--deopt-every-n-times` stress mode. See EmitEagerDeoptStress.
  DCHECK(!IsDeoptLabel(target));
  b(target);
}

inline void MaglevAssembler::JumpToDeopt(Label* target) {
  DCHECK(IsDeoptLabel(target));
  b(target);
}

inline void MaglevAssembler::EmitEagerDeoptStress(Label* target) {
  // TODO(olivf): On arm `--deopt-every-n-times` is currently not supported.
  // Supporting it would require to implement this method, additionally handle
  // deopt branches in Cbz, and handle all cases where we fall through to the
  // deopt branch (like Int32Divide).
}

inline void MaglevAssembler::JumpIf(Condition cond, Label* target,
                                    Label::Distance) {
  b(to_condition(cond), target);
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

inline void MaglevAssembler::JumpIfNotSmi(Register src, Label* on_smi,
                                          Label::Distance distance) {
  MacroAssembler::JumpIfNotSmi(src, on_smi);
}

void MaglevAssembler::JumpIfByte(Condition cc, Register value, int32_t byte,
                                 Label* target, Label::Distance) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  Register scratch = r0;
  mov(scratch, Operand(byte));
  LoadS8(scratch, scratch);
  if (is_signed(cc)) {
    CmpS32(value, scratch);
  } else {
    CmpU32(value, scratch);
  }
  b(to_condition(cc), target);
}

void MaglevAssembler::JumpIfHoleNan(DoubleRegister value, Register scratch,
                                    Label* target, Label::Distance distance) {
  // TODO(leszeks): Right now this only accepts Zone-allocated target labels.
  // This works because all callsites are jumping to either a deopt, deferred
  // code, or a basic block. If we ever need to jump to an on-stack label, we
  // have to add support for it here change the caller to pass a ZoneLabelRef.
  DCHECK(compilation_info()->zone()->Contains(target));
  ZoneLabelRef is_hole = ZoneLabelRef::UnsafeFromLabelPointer(target);
  ZoneLabelRef is_not_hole(this);
  CmpF64(value, value);
  JumpIf(unordered,
         MakeDeferredCode(
             [](MaglevAssembler* masm, DoubleRegister value, Register scratch,
                ZoneLabelRef is_hole, ZoneLabelRef is_not_hole) {
               masm->lgdr(scratch, value);
               masm->ShiftRightU64(scratch, scratch, Operand(32));
               masm->CompareInt32AndJumpIf(scratch, kHoleNanUpper32, kEqual,
                                           *is_hole);
               masm->Jump(*is_not_hole);
             },
             value, scratch, is_hole, is_not_hole));
  bind(*is_not_hole);
}

void MaglevAssembler::JumpIfNotHoleNan(DoubleRegister value, Register scratch,
                                       Label* target,
                                       Label::Distance distance) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  CmpF64(value, value);
  JumpIf(ordered, target, distance);

  lgdr(scratch, value);
  ShiftRightU64(scratch, scratch, Operand(32));
  CompareInt32AndJumpIf(scratch, kHoleNanUpper32, kNotEqual, target, distance);
}

void MaglevAssembler::JumpIfNotHoleNan(MemOperand operand, Label* target,
                                       Label::Distance distance) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  Register scratch = r0;
  mov(scratch, Operand(kHoleNanInt64));
  CmpU32(scratch, operand);
  JumpIf(ne, target, distance);

  LoadU64(scratch, operand);
  ShiftRightU64(scratch, scratch, Operand(32));
  CompareInt32AndJumpIf(scratch, kHoleNanUpper32, kNotEqual, target, distance);
}

void MaglevAssembler::JumpIfNan(DoubleRegister value, Label* target,
                                Label::Distance distance) {
  CmpF64(value, value);
  JumpIf(unordered, target, distance);
}

void MaglevAssembler::JumpIfNotNan(DoubleRegister value, Label* target,
                                   Label::Distance distance) {
  CmpF64(value, value);
  JumpIf(ordered, target, distance);
}

void MaglevAssembler::CompareIntPtrAndJumpIf(Register r1, Register r2,
                                             Condition cond, Label* target,
                                             Label::Distance distance) {
  if (is_signed(cond)) {
    CmpS64(r1, r2);
  } else {
    CmpU64(r1, r2);
  }
  b(to_condition(cond), target);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, Register r2,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  if (is_signed(cond)) {
    CmpS32(r1, r2);
  } else {
    CmpU32(r1, r2);
  }
  b(to_condition(cond), target);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, int32_t value,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  if (is_signed(cond)) {
    CmpS32(r1, Operand(value));
  } else {
    CmpU32(r1, Operand(value));
  }
  JumpIf(cond, target);
}

inline void MaglevAssembler::CompareInt32AndAssert(Register r1, Register r2,
                                                   Condition cond,
                                                   AbortReason reason) {
  if (is_signed(cond)) {
    CmpS32(r1, r2);
  } else {
    CmpU32(r1, r2);
  }
  Assert(to_condition(cond), reason);
}

inline void MaglevAssembler::CompareInt32AndAssert(Register r1, int32_t value,
                                                   Condition cond,
                                                   AbortReason reason) {
  if (is_signed(cond)) {
    CmpS32(r1, Operand(value));
  } else {
    CmpU32(r1, Operand(value));
  }
  Assert(to_condition(cond), reason);
}

inline void MaglevAssembler::CompareInt32AndBranch(
    Register r1, int32_t value, Condition cond, Label* if_true,
    Label::Distance true_distance, bool fallthrough_when_true, Label* if_false,
    Label::Distance false_distance, bool fallthrough_when_false) {
  if (is_signed(cond)) {
    CmpS32(r1, Operand(value));
  } else {
    CmpU32(r1, Operand(value));
  }
  Branch(to_condition(cond), if_true, true_distance, fallthrough_when_true,
         if_false, false_distance, fallthrough_when_false);
}

inline void MaglevAssembler::CompareInt32AndBranch(
    Register r1, Register r2, Condition cond, Label* if_true,
    Label::Distance true_distance, bool fallthrough_when_true, Label* if_false,
    Label::Distance false_distance, bool fallthrough_when_false) {
  if (is_signed(cond)) {
    CmpS32(r1, r2);
  } else {
    CmpU32(r1, r2);
  }
  Branch(to_condition(cond), if_true, true_distance, fallthrough_when_true,
         if_false, false_distance, fallthrough_when_false);
}

inline void MaglevAssembler::CompareSmiAndJumpIf(Register r1, Tagged<Smi> value,
                                                 Condition cond, Label* target,
                                                 Label::Distance distance) {
  CmpSmiLiteral(r1, value, r0);
  JumpIf(cond, target);
}

inline void MaglevAssembler::CompareByteAndJumpIf(MemOperand left, int8_t right,
                                                  Condition cond,
                                                  Register scratch,
                                                  Label* target,
                                                  Label::Distance distance) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  Register scratch2 = r0;
  LoadS8(scratch, left);
  mov(scratch2, Operand(right));
  LoadS8(scratch2, scratch2);
  CmpS32(scratch, scratch2);
  CHECK(is_signed(cond));
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareTaggedAndJumpIf(Register reg,
                                                    Tagged<Smi> value,
                                                    Condition cond,
                                                    Label* target,
                                                    Label::Distance distance) {
  CmpSmiLiteral(reg, value, r0);
  JumpIf(cond, target);
}

inline void MaglevAssembler::CompareTaggedAndJumpIf(Register reg,
                                                    Handle<HeapObject> obj,
                                                    Condition cond,
                                                    Label* target,
                                                    Label::Distance distance) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  Register scratch = r0;
  MacroAssembler::Move(scratch, obj,
                       COMPRESS_POINTERS_BOOL
                           ? RelocInfo::COMPRESSED_EMBEDDED_OBJECT
                           : RelocInfo::FULL_EMBEDDED_OBJECT);
  CmpTagged(reg, scratch);
  b(to_condition(cond), target);
}

inline void MaglevAssembler::CompareTaggedAndJumpIf(Register src1,
                                                    Register src2,
                                                    Condition cond,
                                                    Label* target,
                                                    Label::Distance distance) {
  CmpTagged(src1, src2);
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareDoubleAndJumpIfZeroOrNaN(
    DoubleRegister reg, Label* target, Label::Distance distance) {
  lzdr(kDoubleRegZero);
  CmpF64(kDoubleRegZero, reg);
  JumpIf(eq, target);
  JumpIf(CC_OF, target);  // NaN check
}

inline void MaglevAssembler::CompareDoubleAndJumpIfZeroOrNaN(
    MemOperand operand, Label* target, Label::Distance distance) {
  lzdr(kDoubleRegZero);
  CmpF64(kDoubleRegZero, operand);
  JumpIf(eq, target);
  JumpIf(CC_OF, target);  // NaN check
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    Register value, int32_t mask, Label* target, Label::Distance distance) {
  And(r0, value, Operand(mask));
  bne(target);
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    MemOperand operand, int32_t mask, Label* target, Label::Distance distance) {
  LoadU32(r0, operand);
  And(r0, Operand(mask));
  bne(target);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    Register value, int32_t mask, Label* target, Label::Distance distance) {
  And(r0, value, Operand(mask));
  beq(target);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    MemOperand operand, int32_t mask, Label* target, Label::Distance distance) {
  LoadU32(r0, operand);
  And(r0, Operand(mask));
  beq(target);
}

inline void MaglevAssembler::LoadHeapNumberValue(DoubleRegister result,
                                                 Register heap_number) {
  LoadF64(result, FieldMemOperand(heap_number, offsetof(HeapNumber, value_)));
}

inline void MaglevAssembler::Int32ToDouble(DoubleRegister result,
                                           Register src) {
  ConvertIntToDouble(result, src);
}

inline void MaglevAssembler::Uint32ToDouble(DoubleRegister result,
                                            Register src) {
  ConvertUnsignedIntToDouble(result, src);
}

inline void MaglevAssembler::Pop(Register dst) { pop(dst); }

inline void MaglevAssembler::AssertStackSizeCorrect() {
  if (v8_flags.debug_code) {
    mov(r0, sp);
    AddU64(r0, Operand(code_gen_state()->stack_slots() * kSystemPointerSize +
                       StandardFrameConstants::kFixedFrameSizeFromFp));
    CmpU64(r0, fp);
    Assert(eq, AbortReason::kStackAccessBelowStackPointer);
  }
}

inline Condition MaglevAssembler::FunctionEntryStackCheck(
    int stack_check_offset) {
  ScratchRegisterScope temps(this);
  Register interrupt_stack_limit = temps.Acquire();
  LoadStackLimit(interrupt_stack_limit, StackLimitKind::kInterruptStackLimit);

  Register stack_cmp_reg = sp;
  if (stack_check_offset >= kStackLimitSlackForDeoptimizationInBytes) {
    stack_cmp_reg = r0;
    mov(stack_cmp_reg, sp);
    lay(stack_cmp_reg, MemOperand(stack_cmp_reg, -stack_check_offset));
  }
  CmpU64(stack_cmp_reg, interrupt_stack_limit);
  return ge;
}

inline void MaglevAssembler::FinishCode() {}

template <typename NodeT>
inline void MaglevAssembler::EmitEagerDeoptIfNotEqual(DeoptimizeReason reason,
                                                      NodeT* node) {
  EmitEagerDeoptIf(ne, reason, node);
}

template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr, Register dst,
                                      Register src) {
  Move(dst, src);
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr, Register dst,
                                      MemOperand src) {
  switch (repr) {
    case MachineRepresentation::kWord32:
      return LoadU32(dst, src);
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kWord64:
      return LoadU64(dst, src);
    default:
      UNREACHABLE();
  }
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr,
                                      MemOperand dst, Register src) {
  switch (repr) {
    case MachineRepresentation::kWord32:
      return StoreU32(src, dst);
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
      return StoreU64(src, dst);
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

#endif  // V8_MAGLEV_S390_MAGLEV_ASSEMBLER_S390_INL_H_
