// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_ARM_MAGLEV_ASSEMBLER_ARM_INL_H_
#define V8_MAGLEV_ARM_MAGLEV_ASSEMBLER_ARM_INL_H_

#include "src/base/numbers/double.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
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

constexpr Condition ConditionForNaN() { return vs; }

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
      wrapped_scope_.Include(kMaglevExtraScratchRegister);
    }
  }

  ~ScratchRegisterScope() { masm_->scratch_register_scope_ = prev_scope_; }

  void ResetToDefault() {
    wrapped_scope_.SetAvailable(Assembler::DefaultTmpList() |
                                kMaglevExtraScratchRegister);
    wrapped_scope_.SetAvailableVfp(Assembler::DefaultFPTmpList());
  }

  Register GetDefaultScratchRegister() { return Acquire(); }
  DoubleRegister GetDefaultScratchDoubleRegister() { return AcquireDouble(); }

  Register Acquire() { return wrapped_scope_.Acquire(); }
  void Include(Register reg) { wrapped_scope_.Include(reg); }
  void Include(const RegList list) { wrapped_scope_.Include(list); }

  DoubleRegister AcquireDouble() { return wrapped_scope_.AcquireD(); }
  void IncludeDouble(const DoubleRegList list) {
    wrapped_scope_.Include(DoubleToVpfRegList(list));
  }

  RegList Available() { return wrapped_scope_.Available(); }
  void SetAvailable(RegList list) { wrapped_scope_.SetAvailable(list); }

  DoubleRegList AvailableDouble() {
    return VpfToDoubleRegList(wrapped_scope_.AvailableVfp());
  }
  void SetAvailableDouble(DoubleRegList list) {
    wrapped_scope_.SetAvailableVfp(DoubleToVpfRegList(list));
  }

  // TODO(victorgomes): These are very inefficient, but it suffices for now to
  // bootstrap arm. We should be able to define a special DoubleRegList for arm
  // that takes VpfRegList into account directly.
  DoubleRegList VpfToDoubleRegList(VfpRegList list) {
    DoubleRegList double_list;
    for (int index = 0; index < DoubleRegister::kNumRegisters; index++) {
      DoubleRegister reg = DoubleRegister::from_code(index);
      uint64_t mask = reg.ToVfpRegList();
      if ((list & mask) == mask) {
        double_list.set(reg);
      }
    }
    return double_list;
  }
  VfpRegList DoubleToVpfRegList(DoubleRegList list) {
    VfpRegList vfp_list = 0;
    for (DoubleRegister reg : list) {
      vfp_list |= reg.ToVfpRegList();
    }
    return vfp_list;
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

void MapCompare::Generate(Handle<Map> map) {
  MaglevAssembler::ScratchRegisterScope temps(masm_);
  Register temp = temps.Acquire();
  masm_->Move(temp, map);
  masm_->cmp(map_, temp);
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
      MaglevAssembler::ScratchRegisterScope temps(masm);
      Register scratch = temps.Acquire();
      masm->ldr(scratch, masm->GetStackSlot(operand));
      masm->Push(scratch);
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
  add(dst, src, src, SetCC);
}

inline void MaglevAssembler::CheckInt32IsSmi(Register obj, Label* fail,
                                             Register scratch) {
  static_assert(!SmiValuesAre32Bits());

  ScratchRegisterScope temps(this);
  if (scratch == Register::no_reg()) {
    scratch = temps.Acquire();
  }
  add(scratch, obj, obj, SetCC);
  JumpIf(kOverflow, fail);
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
    ldr(scratch, ToMemOperand(input));
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
  ldr(data_pointer,
      FieldMemOperand(object, JSTypedArray::kExternalPointerOffset));
  if (JSTypedArray::kMaxSizeInHeap == 0) return;
  ScratchRegisterScope temps(this);
  Register base = temps.Acquire();
  ldr(base, FieldMemOperand(object, JSTypedArray::kBasePointerOffset));
  add(data_pointer, data_pointer, base);
}

inline MemOperand MaglevAssembler::TypedArrayElementOperand(
    Register data_pointer, Register index, int element_size) {
  add(data_pointer, data_pointer,
      Operand(index, LSL, ShiftFromScale(element_size)));
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
  add(result, object, Operand(index, LSL, ShiftFromScale(scale)));
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
  // No compression mode on arm.
  LoadFixedArrayElement(result, array, index);
}

void MaglevAssembler::LoadFixedDoubleArrayElement(DoubleRegister result,
                                                  Register array,
                                                  Register index) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (v8_flags.debug_code) {
    AssertNotSmi(array);
    IsObjectType(array, FIXED_DOUBLE_ARRAY_TYPE);
    Assert(kEqual, AbortReason::kUnexpectedValue);
    CompareInt32(index, 0);
    Assert(kUnsignedGreaterThanEqual, AbortReason::kUnexpectedNegativeValue);
  }
  add(scratch, array, Operand(index, LSL, kDoubleSizeLog2));
  vldr(result, FieldMemOperand(scratch, FixedArray::kHeaderSize));
}

inline void MaglevAssembler::StoreFixedDoubleArrayElement(
    Register array, Register index, DoubleRegister value) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  add(scratch, array, Operand(index, LSL, kDoubleSizeLog2));
  vstr(value, FieldMemOperand(scratch, FixedArray::kHeaderSize));
}

inline void MaglevAssembler::LoadSignedField(Register result,
                                             MemOperand operand, int size) {
  if (size == 1) {
    ldrsb(result, operand);
  } else if (size == 2) {
    ldrsh(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    ldr(result, operand);
  }
}

inline void MaglevAssembler::LoadUnsignedField(Register result,
                                               MemOperand operand, int size) {
  if (size == 1) {
    ldrb(result, operand);
  } else if (size == 2) {
    ldrh(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    ldr(result, operand);
  }
}

inline void MaglevAssembler::SetSlotAddressForTaggedField(Register slot_reg,
                                                          Register object,
                                                          int offset) {
  add(slot_reg, object, Operand(offset - kHeapObjectTag));
}
inline void MaglevAssembler::SetSlotAddressForFixedArrayElement(
    Register slot_reg, Register object, Register index) {
  add(slot_reg, object, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  add(slot_reg, slot_reg, Operand(index, LSL, kTaggedSizeLog2));
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
  add(scratch, array, Operand(index, LSL, kTaggedSizeLog2));
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
  Register scratch = scope.Acquire();
  Move(scratch, value);
  MacroAssembler::StoreTaggedField(scratch, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreInt32Field(Register object, int offset,
                                             int32_t value) {
  ScratchRegisterScope scope(this);
  Register scratch = scope.Acquire();
  Move(scratch, value);
  str(scratch, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreField(MemOperand operand, Register value,
                                        int size) {
  DCHECK(size == 1 || size == 2 || size == 4);
  if (size == 1) {
    strb(value, operand);
  } else if (size == 2) {
    strh(value, operand);
  } else {
    DCHECK_EQ(size, 4);
    str(value, operand);
  }
}

inline void MaglevAssembler::ReverseByteOrder(Register value, int size) {
  if (size == 2) {
    rev(value, value);
    asr(value, value, Operand(16));
  } else if (size == 4) {
    rev(value, value);
  } else {
    DCHECK_EQ(size, 1);
  }
}

inline void MaglevAssembler::IncrementInt32(Register reg) {
  add(reg, reg, Operand(1));
}

inline void MaglevAssembler::Move(StackSlot dst, Register src) {
  str(src, StackSlotOperand(dst));
}
inline void MaglevAssembler::Move(StackSlot dst, DoubleRegister src) {
  vstr(src, StackSlotOperand(dst));
}
inline void MaglevAssembler::Move(Register dst, StackSlot src) {
  ldr(dst, StackSlotOperand(src));
}
inline void MaglevAssembler::Move(DoubleRegister dst, StackSlot src) {
  vldr(dst, StackSlotOperand(src));
}
inline void MaglevAssembler::Move(MemOperand dst, Register src) {
  str(src, dst);
}
inline void MaglevAssembler::Move(Register dst, MemOperand src) {
  ldr(dst, src);
}
inline void MaglevAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  if (dst != src) {
    vmov(dst, src);
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
  vmov(dst, base::Double(n));
}
inline void MaglevAssembler::Move(DoubleRegister dst, Float64 n) {
  vmov(dst, base::Double(n.get_bits()));
}
inline void MaglevAssembler::Move(Register dst, Handle<HeapObject> obj) {
  MacroAssembler::Move(dst, obj);
}

inline void MaglevAssembler::LoadFloat32(DoubleRegister dst, MemOperand src) {
  UseScratchRegisterScope temps(this);
  SwVfpRegister temp_vfps = SwVfpRegister::no_reg();
  if (dst.code() < 16) {
    temp_vfps = LowDwVfpRegister::from_code(dst.code()).low();
  } else {
    temp_vfps = temps.AcquireS();
  }
  vldr(temp_vfps, src);
  vcvt_f64_f32(dst, temp_vfps);
}
inline void MaglevAssembler::StoreFloat32(MemOperand dst, DoubleRegister src) {
  UseScratchRegisterScope temps(this);
  SwVfpRegister temp_vfps = temps.AcquireS();
  vcvt_f32_f64(temp_vfps, src);
  vstr(temp_vfps, dst);
}
inline void MaglevAssembler::LoadFloat64(DoubleRegister dst, MemOperand src) {
  vldr(dst, src);
}
inline void MaglevAssembler::StoreFloat64(MemOperand dst, DoubleRegister src) {
  vstr(src, dst);
}

inline void MaglevAssembler::LoadUnalignedFloat64(DoubleRegister dst,
                                                  Register base,
                                                  Register index) {
  // vldr only works on 4 bytes aligned access.
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  ldr(scratch, MemOperand(base, index));
  VmovLow(dst, scratch);
  add(scratch, index, Operand(4));
  ldr(scratch, MemOperand(base, scratch));
  VmovHigh(dst, scratch);
}
inline void MaglevAssembler::LoadUnalignedFloat64AndReverseByteOrder(
    DoubleRegister dst, Register base, Register index) {
  // vldr only works on 4 bytes aligned access.
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  ldr(scratch, MemOperand(base, index));
  rev(scratch, scratch);
  VmovHigh(dst, scratch);
  add(scratch, index, Operand(4));
  ldr(scratch, MemOperand(base, scratch));
  rev(scratch, scratch);
  VmovLow(dst, scratch);
}
inline void MaglevAssembler::StoreUnalignedFloat64(Register base,
                                                   Register index,
                                                   DoubleRegister src) {
  // vstr only works on 4 bytes aligned access.
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Register index_scratch = temps.Acquire();
  VmovLow(scratch, src);
  str(scratch, MemOperand(base, index));
  add(index_scratch, index, Operand(4));
  VmovHigh(scratch, src);
  str(scratch, MemOperand(base, index_scratch));
}
inline void MaglevAssembler::ReverseByteOrderAndStoreUnalignedFloat64(
    Register base, Register index, DoubleRegister src) {
  // vstr only works on 4 bytes aligned access.
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  Register index_scratch = temps.Acquire();
  VmovHigh(scratch, src);
  rev(scratch, scratch);
  str(scratch, MemOperand(base, index));
  add(index_scratch, index, Operand(4));
  VmovLow(scratch, src);
  rev(scratch, scratch);
  str(scratch, MemOperand(base, index_scratch));
}

inline void MaglevAssembler::SignExtend32To64Bits(Register dst, Register src) {
  // No 64-bit registers.
}
inline void MaglevAssembler::NegateInt32(Register val) {
  rsb(val, val, Operand(0));
}

inline void MaglevAssembler::ToUint8Clamped(Register result,
                                            DoubleRegister value, Label* min,
                                            Label* max, Label* done) {
  CpuFeatureScope scope(this, ARMv8);
  ScratchRegisterScope temps(this);
  DoubleRegister scratch = temps.AcquireDouble();
  Move(scratch, 0.0);
  VFPCompareAndSetFlags(scratch, value);
  // Set to 0 if NaN.
  JumpIf(kOverflow, min);
  JumpIf(kGreaterThanEqual, min);
  Move(scratch, 255.0);
  VFPCompareAndSetFlags(value, scratch);
  JumpIf(kGreaterThanEqual, max);
  // if value in [0, 255], then round up to the nearest.
  vrintn(scratch, value);
  TruncateDoubleToInt32(result, scratch);
  Jump(done);
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
    tst(scratch, Operand(JSArrayBuffer::WasDetachedBit::kMask));
    EmitEagerDeoptIf(ne, DeoptimizeReason::kArrayBufferWasDetached, node);
  }
}

inline void MaglevAssembler::LoadByte(Register dst, MemOperand src) {
  ldrb(dst, src);
}

inline Condition MaglevAssembler::IsCallableAndNotUndetectable(
    Register map, Register scratch) {
  ldr(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  and_(scratch, scratch,
       Operand(Map::Bits1::IsUndetectableBit::kMask |
               Map::Bits1::IsCallableBit::kMask));
  cmp(scratch, Operand(Map::Bits1::IsCallableBit::kMask));
  return kEqual;
}

inline Condition MaglevAssembler::IsNotCallableNorUndetactable(
    Register map, Register scratch) {
  ldr(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  tst(scratch, Operand(Map::Bits1::IsUndetectableBit::kMask |
                       Map::Bits1::IsCallableBit::kMask));
  return kEqual;
}

inline void MaglevAssembler::LoadInstanceType(Register instance_type,
                                              Register heap_object) {
  LoadMap(instance_type, heap_object);
  ldrh(instance_type, FieldMemOperand(instance_type, Map::kInstanceTypeOffset));
}

inline void MaglevAssembler::IsObjectType(Register heap_object,
                                          InstanceType type) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MacroAssembler::CompareObjectType(heap_object, scratch, scratch, type);
}

inline void MaglevAssembler::CompareObjectType(Register heap_object,
                                               InstanceType type) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  CompareObjectType(heap_object, type, scratch);
}

inline void MaglevAssembler::JumpIfJSAnyIsNotPrimitive(
    Register heap_object, Label* target, Label::Distance distance) {
  // If the type of the result (stored in its map) is less than
  // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
  static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MacroAssembler::CompareObjectType(heap_object, scratch, scratch,
                                    FIRST_JS_RECEIVER_TYPE);
  JumpIf(kUnsignedGreaterThanEqual, target, distance);
}

inline void MaglevAssembler::CompareObjectType(Register heap_object,
                                               InstanceType type,
                                               Register scratch) {
  LoadMap(scratch, heap_object);
  MacroAssembler::CompareInstanceType(scratch, scratch, type);
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

inline void MaglevAssembler::CompareTagged(Register reg, Tagged<Smi> smi) {
  cmp(reg, Operand(smi));
}

inline void MaglevAssembler::CompareTagged(Register reg,
                                           Handle<HeapObject> obj) {
  cmp(reg, Operand(obj));
}

inline void MaglevAssembler::CompareTagged(Register src1, Register src2) {
  cmp(src1, src2);
}

inline void MaglevAssembler::CompareInt32(Register reg, int32_t imm) {
  cmp(reg, Operand(imm));
}

inline void MaglevAssembler::CompareInt32(Register src1, Register src2) {
  cmp(src1, src2);
}

inline void MaglevAssembler::CompareFloat64(DoubleRegister src1,
                                            DoubleRegister src2) {
  VFPCompareAndSetFlags(src1, src2);
}

inline void MaglevAssembler::PrepareCallCFunction(int num_reg_arguments,
                                                  int num_double_registers) {
  MacroAssembler::PrepareCallCFunction(num_reg_arguments, num_double_registers);
}

inline void MaglevAssembler::CallSelf() {
  DCHECK(code_gen_state()->entry_label()->is_bound());
  bl(code_gen_state()->entry_label());
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
  b(target, cond);
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
  cmp(value, Operand(byte));
  b(cc, target);
}

void MaglevAssembler::JumpIfHoleNan(DoubleRegister value, Register scratch,
                                    Label* target, Label::Distance distance) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  Register repr = temps.Acquire();
  Label not_hole_nan;
  static_assert(kHoleNanLower32 == kHoleNanUpper32);
  Move(scratch, kHoleNanLower32);
  VmovLow(repr, value);
  cmp(repr, scratch);
  JumpIf(kNotEqual, &not_hole_nan, Label::kNear);
  VmovHigh(repr, value);
  cmp(repr, scratch);
  JumpIf(kEqual, target, distance);
  bind(&not_hole_nan);
}

void MaglevAssembler::JumpIfNotHoleNan(DoubleRegister value, Register scratch,
                                       Label* target,
                                       Label::Distance distance) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  Register repr = temps.Acquire();
  static_assert(kHoleNanLower32 == kHoleNanUpper32);
  Move(scratch, kHoleNanLower32);
  VmovLow(repr, value);
  cmp(repr, scratch);
  JumpIf(kNotEqual, target, distance);
  VmovHigh(repr, value);
  cmp(repr, scratch);
  JumpIf(kNotEqual, target, distance);
}

void MaglevAssembler::JumpIfNotHoleNan(MemOperand operand, Label* target,
                                       Label::Distance distance) {
  MaglevAssembler::ScratchRegisterScope temps(this);
  Register repr = temps.Acquire();
  ldr(repr, operand);
  // Acquire {scratch} after ldr, since this might need a scratch register.
  Register scratch = temps.Acquire();
  static_assert(kHoleNanLower32 == kHoleNanUpper32);
  Move(scratch, kHoleNanUpper32);
  cmp(repr, scratch);
  JumpIf(kNotEqual, target, distance);
  operand.set_offset(operand.offset() + 4);
  ldr(repr, operand);
  cmp(repr, scratch);
  JumpIf(kNotEqual, target, distance);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, Register r2,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  cmp(r1, r2);
  b(cond, target);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, int32_t value,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  cmp(r1, Operand(value));
  b(cond, target);
}

inline void MaglevAssembler::CompareSmiAndJumpIf(Register r1, Tagged<Smi> value,
                                                 Condition cond, Label* target,
                                                 Label::Distance distance) {
  cmp(r1, Operand(value));
  b(cond, target);
}

inline void MaglevAssembler::CompareByteAndJumpIf(MemOperand left, int8_t right,
                                                  Condition cond,
                                                  Register scratch,
                                                  Label* target,
                                                  Label::Distance distance) {
  LoadByte(scratch, left);
  Cmp(scratch, right);
  JumpIf(cond, target, distance);
}

inline void MaglevAssembler::CompareTaggedAndJumpIf(Register r1,
                                                    Tagged<Smi> value,
                                                    Condition cond,
                                                    Label* target,
                                                    Label::Distance distance) {
  cmp(r1, Operand(value));
  b(cond, target);
}

inline void MaglevAssembler::CompareDoubleAndJumpIfZeroOrNaN(
    DoubleRegister reg, Label* target, Label::Distance distance) {
  VFPCompareAndSetFlags(reg, 0.0);
  JumpIf(eq, target);
  JumpIf(vs, target);  // NaN check
}

inline void MaglevAssembler::CompareDoubleAndJumpIfZeroOrNaN(
    MemOperand operand, Label* target, Label::Distance distance) {
  ScratchRegisterScope temps(this);
  DoubleRegister value_double = temps.AcquireDouble();
  vldr(value_double, operand);
  CompareDoubleAndJumpIfZeroOrNaN(value_double, target, distance);
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    Register r1, int32_t mask, Label* target, Label::Distance distance) {
  tst(r1, Operand(mask));
  b(ne, target);
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    MemOperand operand, int32_t mask, Label* target, Label::Distance distance) {
  ScratchRegisterScope temps(this);
  Register value = temps.Acquire();
  ldr(value, operand);
  TestInt32AndJumpIfAnySet(value, mask, target);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    Register r1, int32_t mask, Label* target, Label::Distance distance) {
  tst(r1, Operand(mask));
  b(eq, target);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    MemOperand operand, int32_t mask, Label* target, Label::Distance distance) {
  ScratchRegisterScope temps(this);
  Register value = temps.Acquire();
  ldr(value, operand);
  TestInt32AndJumpIfAllClear(value, mask, target);
}

inline void MaglevAssembler::LoadHeapNumberValue(DoubleRegister result,
                                                 Register heap_number) {
  vldr(result, FieldMemOperand(heap_number, HeapNumber::kValueOffset));
}

inline void MaglevAssembler::Int32ToDouble(DoubleRegister result,
                                           Register src) {
  UseScratchRegisterScope temps(this);
  SwVfpRegister temp_vfps = SwVfpRegister::no_reg();
  if (result.code() < 16) {
    temp_vfps = LowDwVfpRegister::from_code(result.code()).low();
  } else {
    temp_vfps = temps.AcquireS();
  }
  vmov(temp_vfps, src);
  vcvt_f64_s32(result, temp_vfps);
}

inline void MaglevAssembler::Uint32ToDouble(DoubleRegister result,
                                            Register src) {
  UseScratchRegisterScope temps(this);
  SwVfpRegister temp_vfps = SwVfpRegister::no_reg();
  if (result.code() < 16) {
    temp_vfps = LowDwVfpRegister::from_code(result.code()).low();
  } else {
    temp_vfps = temps.AcquireS();
  }
  vmov(temp_vfps, src);
  vcvt_f64_u32(result, temp_vfps);
}

inline void MaglevAssembler::Pop(Register dst) { pop(dst); }

inline void MaglevAssembler::AssertStackSizeCorrect() {
  if (v8_flags.debug_code) {
    ScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    add(scratch, sp,
        Operand(code_gen_state()->stack_slots() * kSystemPointerSize +
                StandardFrameConstants::kFixedFrameSizeFromFp));
    cmp(scratch, fp);
    Assert(eq, AbortReason::kStackAccessBelowStackPointer);
  }
}

inline Condition MaglevAssembler::FunctionEntryStackCheck(
    int stack_check_offset) {
  ScratchRegisterScope temps(this);
  Register stack_cmp_reg = sp;
  if (stack_check_offset > kStackLimitSlackForDeoptimizationInBytes) {
    stack_cmp_reg = temps.Acquire();
    sub(stack_cmp_reg, sp, Operand(stack_check_offset));
  }
  Register interrupt_stack_limit = temps.Acquire();
  LoadStackLimit(interrupt_stack_limit, StackLimitKind::kInterruptStackLimit);
  cmp(stack_cmp_reg, interrupt_stack_limit);
  return kUnsignedGreaterThanEqual;
}

inline void MaglevAssembler::FinishCode() { CheckConstPool(true, false); }

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
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
      return ldr(dst, src);
    default:
      UNREACHABLE();
  }
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr,
                                      MemOperand dst, Register src) {
  switch (repr) {
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
      return str(src, dst);
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

#endif  // V8_MAGLEV_ARM_MAGLEV_ASSEMBLER_ARM_INL_H_
