// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_LOONG64_MAGLEV_ASSEMBLER_LOONG64_INL_H_
#define V8_MAGLEV_LOONG64_MAGLEV_ASSEMBLER_LOONG64_INL_H_

#include "src/base/iterator.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/common/globals.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/maglev/maglev-assembler.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/roots/static-roots.h"

namespace v8 {
namespace internal {
namespace maglev {

constexpr Condition ConditionForFloat64(Operation operation) {
  return ConditionFor(operation);
}

constexpr FPUCondition FPUConditionFor(Condition cond, bool* need_swap,
                                       bool check_UN) {
  switch (cond) {
    case eq:
      *need_swap = false;
      return check_UN ? CUEQ : CEQ;
    case ne:
      *need_swap = false;
      return check_UN ? CUNE : CNE;
    case lt:
      *need_swap = false;
      return check_UN ? CULT : CLT;
    case le:
      *need_swap = false;
      return check_UN ? CULE : CLE;
    case gt:
      *need_swap = true;
      return check_UN ? CULT : CLT;
    case ge:
      *need_swap = true;
      return check_UN ? CULE : CLE;
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
    case 8:
      return 3;
    default:
      UNREACHABLE();
  }
}

class MaglevAssembler::TemporaryRegisterScope
    : public TemporaryRegisterScopeBase<TemporaryRegisterScope> {
  using Base = TemporaryRegisterScopeBase<TemporaryRegisterScope>;

 public:
  struct SavedData : public Base::SavedData {
    RegList available_scratch_;
    DoubleRegList available_fp_scratch_;
  };

  explicit TemporaryRegisterScope(MaglevAssembler* masm)
      : Base(masm), scratch_scope_(masm) {}
  explicit TemporaryRegisterScope(MaglevAssembler* masm,
                                  const SavedData& saved_data)
      : Base(masm, saved_data), scratch_scope_(masm) {
    scratch_scope_.SetAvailable(saved_data.available_scratch_);
    scratch_scope_.SetAvailableFP(saved_data.available_fp_scratch_);
  }

  Register AcquireScratch() {
    Register reg = scratch_scope_.Acquire();
    CHECK(!available_.has(reg));
    return reg;
  }
  DoubleRegister AcquireScratchDouble() {
    DoubleRegister reg = scratch_scope_.AcquireFp();
    CHECK(!available_double_.has(reg));
    return reg;
  }
  void IncludeScratch(Register reg) { scratch_scope_.Include(reg); }

  SavedData CopyForDefer() {
    return SavedData{
        CopyForDeferBase(),
        *scratch_scope_.Available(),
        *scratch_scope_.AvailableFP(),
    };
  }

  void ResetToDefaultImpl() {
    scratch_scope_.SetAvailable({t6, t7, t8});
    scratch_scope_.SetAvailableFP({f27, f28});
  }

 private:
  UseScratchRegisterScope scratch_scope_;
};

inline MapCompare::MapCompare(MaglevAssembler* masm, Register object,
                              size_t map_count)
    : masm_(masm), object_(object), map_count_(map_count) {
  map_ = masm_->scratch_register_scope()->AcquireScratch();
  if (PointerCompressionIsEnabled()) {
    masm_->LoadCompressedMap(map_, object_);
  } else {
    masm_->LoadMap(map_, object_);
  }
  USE(map_count_);
}

void MapCompare::Generate(Handle<Map> map, Condition cond, Label* if_true,
                          Label::Distance distance) {
  masm_->MacroAssembler::CompareTaggedAndBranch(if_true, cond, map_,
                                                Operand(map) /*, distance*/);
}

Register MapCompare::GetMap() {
  if (PointerCompressionIsEnabled()) {
    masm_->DecompressTagged(map_, map_);
  }
  return map_;
}

int MapCompare::TemporaryCount(size_t map_count) { return 1; }

namespace detail {

template <typename Arg>
inline Register ToRegister(MaglevAssembler* masm,
                           MaglevAssembler::TemporaryRegisterScope* scratch,
                           Arg arg) {
  Register reg = scratch->AcquireScratch();
  masm->Move(reg, arg);
  return reg;
}
inline Register ToRegister(MaglevAssembler* masm,
                           MaglevAssembler::TemporaryRegisterScope* scratch,
                           Register reg) {
  return reg;
}
inline Register ToRegister(MaglevAssembler* masm,
                           MaglevAssembler::TemporaryRegisterScope* scratch,
                           Input input) {
  if (input.operand().IsConstant()) {
    Register reg = scratch->AcquireScratch();
    input.node()->LoadToRegister(masm, reg);
    return reg;
  }
  const compiler::AllocatedOperand& operand =
      compiler::AllocatedOperand::cast(input.operand());
  if (operand.IsRegister()) {
    return ToRegister(input);
  } else {
    DCHECK(operand.IsStackSlot());
    Register reg = scratch->AcquireScratch();
    masm->Move(reg, masm->ToMemOperand(input.operand()));
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
    if constexpr (is_iterator_range<Arg>::value || std::ranges::range<Arg>) {
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

template <std::ranges::range Range, typename... Args>
inline void PushIterator(MaglevAssembler* masm, Range range, Args... args) {
  for (auto iter = range.begin(), end = range.end(); iter != end; ++iter) {
    masm->Push(*iter);
  }
  PushAllHelper<Args...>::Push(masm, args...);
}

template <typename T, typename... Args>
inline void PushIteratorReverse(MaglevAssembler* masm,
                                base::iterator_range<T> range, Args... args) {
  PushAllHelper<Args...>::PushReverse(masm, args...);
  for (const auto& item : base::Reversed(range)) {
    masm->Push(item);
  }
}

inline void PushInput(MaglevAssembler* masm, ConstInput input) {
  if (input.operand().IsConstant()) {
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    Register scratch = temps.AcquireScratch();
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
      MaglevAssembler::TemporaryRegisterScope temps(masm);
      Register scratch = temps.AcquireScratch();
      masm->Ld_d(scratch, masm->GetStackSlot(operand));
      masm->Push(scratch);
    }
  }
}

template <typename... Args>
struct PushAllHelper<Input, Args...> {
  static void Push(MaglevAssembler* masm, Input arg, Args... args) {
    // PushInput(masm, arg);
    {
      MaglevAssembler::TemporaryRegisterScope temps(masm);
      masm->MacroAssembler::Push(ToRegister(masm, &temps, arg));
    }
    PushAllHelper<Args...>::Push(masm, args...);
  }
  static void PushReverse(MaglevAssembler* masm, Input arg, Args... args) {
    PushAllHelper<Args...>::PushReverse(masm, args...);
    {
      MaglevAssembler::TemporaryRegisterScope temps(masm);
      masm->MacroAssembler::Push(ToRegister(masm, &temps, arg));
    }
    // PushInput(masm, arg);
  }
};
template <typename Arg, typename... Args>
struct PushAllHelper<Arg, Args...> {
  static void Push(MaglevAssembler* masm, Arg arg, Args... args) {
    if constexpr (is_iterator_range<Arg>::value || std::ranges::range<Arg>) {
      PushIterator(masm, arg, args...);
    } else {
      masm->MacroAssembler::Push(arg);
      PushAllHelper<Args...>::Push(masm, args...);
    }
  }
  static void PushReverse(MaglevAssembler* masm, Arg arg, Args... args) {
    if constexpr (is_iterator_range<Arg>::value) {
      PushIteratorReverse(masm, arg, args...);
    } else if constexpr (std::ranges::range<Arg>) {
      PushIteratorReverse(
          masm, base::make_iterator_range(arg.begin(), arg.end()), args...);
    } else {
      PushAllHelper<Args...>::PushReverse(masm, args...);
      masm->Push(arg);
    }
  }
};

}  // namespace detail

template <typename... T>
void MaglevAssembler::Push(T... vals) {
  detail::PushAll(this, vals...);
}

template <typename... T>
void MaglevAssembler::PushReverse(T... vals) {
  detail::PushAllReverse(this, vals...);
}

inline void MaglevAssembler::BindJumpTarget(Label* label) { bind(label); }

inline void MaglevAssembler::BindBlock(BasicBlock* block) {
  if (block->is_start_block_of_switch_case()) {
    BindJumpTarget(block->label());
  } else {
    bind(block->label());
  }
}

inline Condition MaglevAssembler::TrySmiTagInt32(Register dst, Register src) {
  CHECK(!SmiValuesAre32Bits());
  // NB: JumpIf expects the result in dedicated "flag" register
  Register overflow_flag = MaglevAssembler::GetFlagsRegister();
  if (src == dst) {
    Move(overflow_flag, src);
    src = overflow_flag;
  }
  slli_w(dst, src, 1);
  xor_(overflow_flag, src, dst);
  srli_w(overflow_flag, overflow_flag, 31);
  return kNoOverflow;
}

inline void MaglevAssembler::CheckInt32IsSmi(Register maybe_smi, Label* fail,
                                             Register scratch) {
  CHECK(!SmiValuesAre32Bits());
  // Smi is shifted left by 1.
  MaglevAssembler::TemporaryRegisterScope temps(this);
  if (scratch == Register::no_reg()) {
    scratch = temps.AcquireScratch();
  }
  Register bit31 = scratch;
  Register bit30 = temps.AcquireScratch();
  bstrpick_w(bit31, maybe_smi, 31, 31);
  bstrpick_w(bit30, maybe_smi, 30, 30);
  // Overflow happened if bit31 != bit30
  MacroAssembler::Branch(fail, ne, bit30, Operand(bit31));
}

inline void MaglevAssembler::SmiAddConstant(Register dst, Register src,
                                            int value, Label* fail,
                                            Label::Distance distance) {
  AssertSmi(src);
  if (value != 0) {
    MaglevAssembler::TemporaryRegisterScope temps(this);
    Register scratch = temps.AcquireScratch();
    if (SmiValuesAre31Bits()) {
      slli_w(scratch, src, 0);
      src = scratch;
      Add_w(dst, src, Operand(Smi::FromInt(value)));
    } else {
      if (src == dst) {
        Move(scratch, src);
        src = scratch;
      }
      Add_d(dst, src, Operand(Smi::FromInt(value)));
    }
    MacroAssembler::Branch(fail, value > 0 ? lt : gt, dst, Operand(src));
  } else {
    Move(dst, src);
  }
}

inline void MaglevAssembler::SmiSubConstant(Register dst, Register src,
                                            int value, Label* fail,
                                            Label::Distance distance) {
  AssertSmi(src);
  if (value != 0) {
    MaglevAssembler::TemporaryRegisterScope temps(this);
    Register scratch = temps.AcquireScratch();
    if (SmiValuesAre31Bits()) {
      slli_w(scratch, src, 0);
      src = scratch;
      Sub_w(dst, src, Operand(Smi::FromInt(value)));
    } else {
      if (src == dst) {
        Move(scratch, src);
        src = scratch;
      }
      Sub_d(dst, src, Operand(Smi::FromInt(value)));
    }
    MacroAssembler::Branch(fail, value > 0 ? gt : lt, dst, Operand(src));
  } else {
    Move(dst, src);
  }
}

inline void MaglevAssembler::MoveHeapNumber(Register dst, double value) {
  li(dst, Operand::EmbeddedNumber(value));
}

inline Condition MaglevAssembler::CheckSmi(Register src) {
  constexpr Register aflag = MaglevAssembler::GetFlagsRegister();
  static_assert(kSmiTag == 0);
  And(aflag, src, kSmiTagMask);
  return eq;
}

inline void MaglevAssembler::Assert(Condition cond, AbortReason reason) {
  constexpr Register aflag = MaglevAssembler::GetFlagsRegister();
  // whatever cond option came in, we just check the flag to be zero
  switch (cond) {
    case kNoOverflow:
      cond = kEqual;
      break;
    case kOverflow:
      cond = kNotEqual;
      break;
    default:
      break;
  }
  MacroAssembler::Assert(cond, reason, aflag, Operand(zero_reg));
}

// Cmp and Assert are only used in maglev unittests, so to make them happy.
// It's only used with subsequent Assert kEqual,
// so pseudo flag should be 0 if rj equals imm
inline void MaglevAssembler::Cmp(const Register& rj, const Operand& operand) {
  constexpr Register aflag = MaglevAssembler::GetFlagsRegister();
  Sub_d(aflag, rj, operand);
}

inline void MaglevAssembler::Cmp(const Register& rj, const uint32_t imm) {
  constexpr Register aflag = MaglevAssembler::GetFlagsRegister();
  Sub_d(aflag, rj, Operand(static_cast<int32_t>(imm)));
}

inline void MaglevAssembler::CmpTagged(const Register& rj,
                                       const Operand& operand) {
  constexpr Register aflag = MaglevAssembler::GetFlagsRegister();
  if (COMPRESS_POINTERS_BOOL) {
    Sub_w(aflag, rj, operand);
  } else {
    Sub_d(aflag, rj, operand);
  }
}

// Compare the object in a register to a value from the root list.
inline void MaglevAssembler::CompareRoot(const Register& obj, RootIndex index,
                                         ComparisonMode mode) {
  ASM_CODE_COMMENT(this);
  if (mode == ComparisonMode::kFullPointer ||
      !base::IsInRange(index, RootIndex::kFirstStrongOrReadOnlyRoot,
                       RootIndex::kLastStrongOrReadOnlyRoot)) {
    // Some smi roots contain system pointer size values like stack limits.
    TemporaryRegisterScope temps(this);
    Register scratch = temps.AcquireScratch();
    DCHECK(!AreAliased(obj, scratch));
    LoadRoot(scratch, index);
    Cmp(obj, Operand(scratch));
    return;
  }
  CompareTaggedRoot(obj, index);
}

inline void MaglevAssembler::CompareTaggedRoot(const Register& obj,
                                               RootIndex index) {
  ASM_CODE_COMMENT(this);
  AssertSmiOrHeapObjectInMainCompressionCage(obj);
  if (V8_STATIC_ROOTS_BOOL && RootsTable::IsReadOnly(index)) {
    CmpTagged(obj, Operand(ReadOnlyRootPtr(index)));
    return;
  }
  // Some smi roots contain system pointer size values like stack limits.
  DCHECK(base::IsInRange(index, RootIndex::kFirstStrongOrReadOnlyRoot,
                         RootIndex::kLastStrongOrReadOnlyRoot));
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  DCHECK(!AreAliased(obj, scratch));
  LoadRoot(scratch, index);
  CmpTagged(obj, Operand(scratch));
}

inline Condition MaglevAssembler::IsRootConstant(Input input,
                                                 RootIndex root_index) {
  if (input.operand().IsRegister()) {
    CompareRoot(ToRegister(input), root_index);
  } else {
    DCHECK(input.operand().IsStackSlot());
    TemporaryRegisterScope temps(this);
    Register scratch = temps.AcquireScratch();
    Ld_d(scratch, ToMemOperand(input.operand()));
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

inline void MaglevAssembler::BuildTypedArrayDataPointer(Register data_pointer,
                                                        Register object) {
  DCHECK_NE(data_pointer, object);
  LoadExternalPointerField(
      data_pointer,
      FieldMemOperand(object, JSTypedArray::kExternalPointerOffset));
  if (JSTypedArray::kMaxSizeInHeap == 0) return;
  MaglevAssembler::TemporaryRegisterScope scope(this);
  Register base = scope.AcquireScratch();
  if (COMPRESS_POINTERS_BOOL) {
    Ld_wu(base, FieldMemOperand(object, JSTypedArray::kBasePointerOffset));
  } else {
    Ld_d(base, FieldMemOperand(object, JSTypedArray::kBasePointerOffset));
  }
  Add_d(data_pointer, data_pointer, base);
}

inline MemOperand MaglevAssembler::TypedArrayElementOperand(
    Register data_pointer, Register index, int element_size) {
  const int shift = ShiftFromScale(element_size);
  if (shift == 0) {
    Add_d(data_pointer, data_pointer, index);
  } else {
    Alsl_d(data_pointer, index, data_pointer, shift);
  }
  return MemOperand(data_pointer, 0);
}

inline void MaglevAssembler::StoreDataViewElement(Register value,
                                                  Register data_pointer,
                                                  Register index,
                                                  int element_size) {
  MemOperand element_address = MemOperand(data_pointer, index);
  StoreField(element_address, value, element_size);
}

inline void MaglevAssembler::LoadDataViewElement(Register result,
                                                 Register data_pointer,
                                                 Register index,
                                                 int element_size) {
  MemOperand element_address = MemOperand(data_pointer, index);
  LoadSignedField(result, element_address, element_size);
}

inline void MaglevAssembler::LoadTaggedFieldByIndex(Register result,
                                                    Register object,
                                                    Register index, int scale,
                                                    int offset) {
  const int shift = ShiftFromScale(scale);
  if (shift == 0) {
    Add_d(result, object, index);
  } else {
    Alsl_d(result, index, object, shift);
  }
  LoadTaggedField(result, FieldMemOperand(result, offset));
}

inline void MaglevAssembler::LoadBoundedSizeFromObject(Register result,
                                                       Register object,
                                                       int offset) {
  Move(result, FieldMemOperand(object, offset));
#ifdef V8_ENABLE_SANDBOX
  srli_d(result, result, kBoundedSizeShift);
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

void MaglevAssembler::LoadFixedArrayElement(Register result, Register array,
                                            Register index) {
  if (v8_flags.debug_code) {
    AssertObjectType(array, FIXED_ARRAY_TYPE, AbortReason::kUnexpectedValue);
    CompareInt32AndAssert(index, 0, kUnsignedGreaterThanEqual,
                          AbortReason::kUnexpectedNegativeValue);
  }
  LoadTaggedFieldByIndex(result, array, index, kTaggedSize,
                         OFFSET_OF_DATA_START(FixedArray));
}

inline void MaglevAssembler::LoadTaggedFieldWithoutDecompressing(
    Register result, Register object, int offset) {
  MacroAssembler::LoadTaggedFieldWithoutDecompressing(
      result, FieldMemOperand(object, offset));
}

void MaglevAssembler::LoadFixedArrayElementWithoutDecompressing(
    Register result, Register array, Register index) {
  if (v8_flags.debug_code) {
    AssertObjectType(array, FIXED_ARRAY_TYPE, AbortReason::kUnexpectedValue);
    CompareInt32AndAssert(index, 0, kUnsignedGreaterThanEqual,
                          AbortReason::kUnexpectedNegativeValue);
  }
  Alsl_d(result, index, array, kTaggedSizeLog2);
  MacroAssembler::LoadTaggedFieldWithoutDecompressing(
      result, FieldMemOperand(result, OFFSET_OF_DATA_START(FixedArray)));
}

void MaglevAssembler::LoadFixedDoubleArrayElement(DoubleRegister result,
                                                  Register array,
                                                  Register index) {
  if (v8_flags.debug_code) {
    AssertObjectType(array, FIXED_DOUBLE_ARRAY_TYPE,
                     AbortReason::kUnexpectedValue);
    CompareInt32AndAssert(index, 0, kUnsignedGreaterThanEqual,
                          AbortReason::kUnexpectedNegativeValue);
  }
  MaglevAssembler::TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Alsl_d(scratch, index, array, kDoubleSizeLog2);
  Fld_d(result, FieldMemOperand(scratch, OFFSET_OF_DATA_START(FixedArray)));
}

inline void MaglevAssembler::StoreFixedDoubleArrayElement(
    Register array, Register index, DoubleRegister value) {
  MaglevAssembler::TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Alsl_d(scratch, index, array, kDoubleSizeLog2);
  Fst_d(value, FieldMemOperand(scratch, OFFSET_OF_DATA_START(FixedArray)));
}

inline void MaglevAssembler::LoadSignedField(Register result,
                                             MemOperand operand, int size) {
  if (size == 1) {
    Ld_b(result, operand);
  } else if (size == 2) {
    Ld_h(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    Ld_w(result, operand);
  }
}

inline void MaglevAssembler::LoadUnsignedField(Register result,
                                               MemOperand operand, int size) {
  if (size == 1) {
    Ld_bu(result, operand);
  } else if (size == 2) {
    Ld_hu(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    Ld_w(result, operand);
  }
}

inline void MaglevAssembler::SetSlotAddressForTaggedField(Register slot_reg,
                                                          Register object,
                                                          int offset) {
  Add_d(slot_reg, object, offset - kHeapObjectTag);
}

inline void MaglevAssembler::SetSlotAddressForFixedArrayElement(
    Register slot_reg, Register object, Register index) {
  Add_d(slot_reg, object, OFFSET_OF_DATA_START(FixedArray) - kHeapObjectTag);
  Alsl_d(slot_reg, index, slot_reg, kTaggedSizeLog2);
}

inline void MaglevAssembler::StoreTaggedFieldNoWriteBarrier(Register object,
                                                            int offset,
                                                            Register value) {
  MacroAssembler::StoreTaggedField(value, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreFixedArrayElementNoWriteBarrier(
    Register array, Register index, Register value) {
  MaglevAssembler::TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Alsl_d(scratch, index, array, kTaggedSizeLog2);
  MacroAssembler::StoreTaggedField(
      value, FieldMemOperand(scratch, OFFSET_OF_DATA_START(FixedArray)));
}

inline void MaglevAssembler::StoreTaggedSignedField(Register object, int offset,
                                                    Register value) {
  AssertSmi(value);
  MacroAssembler::StoreTaggedField(value, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreTaggedSignedField(Register object, int offset,
                                                    Tagged<Smi> value) {
  MaglevAssembler::TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  li(scratch, Operand(value));
  MacroAssembler::StoreTaggedField(scratch, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreInt32Field(Register object, int offset,
                                             int32_t value) {
  if (value == 0) {
    St_w(zero_reg, FieldMemOperand(object, offset));
    return;
  }
  MaglevAssembler::TemporaryRegisterScope scope(this);
  Register scratch = scope.AcquireScratch();
  Move(scratch, value);
  St_w(scratch, FieldMemOperand(object, offset));
}

inline void MaglevAssembler::StoreField(MemOperand operand, Register value,
                                        int size) {
  DCHECK(size == 1 || size == 2 || size == 4);
  if (size == 1) {
    St_b(value, operand);
  } else if (size == 2) {
    St_h(value, operand);
  } else {
    DCHECK_EQ(size, 4);
    St_w(value, operand);
  }
}

#ifdef V8_ENABLE_SANDBOX

inline void MaglevAssembler::StoreTrustedPointerFieldNoWriteBarrier(
    Register object, int offset, Register value) {
  MacroAssembler::StoreTrustedPointerField(value,
                                           FieldMemOperand(object, offset));
}

#endif  // V8_ENABLE_SANDBOX

inline void MaglevAssembler::ReverseByteOrder(Register value, int size) {
  if (size == 2) {
    revb_2h(value, value);
    ext_w_h(value, value);
  } else if (size == 4) {
    ByteSwap(value, value, 4);
  } else {
    DCHECK_EQ(size, 1);
  }
}

inline void MaglevAssembler::IncrementInt32(Register reg) {
  Add_w(reg, reg, Operand(1));
}

inline void MaglevAssembler::DecrementInt32(Register reg) {
  Sub_w(reg, reg, Operand(1));
}

inline void MaglevAssembler::AddInt32(Register reg, int amount) {
  Add_w(reg, reg, Operand(amount));
}

inline void MaglevAssembler::AndInt32(Register reg, int mask) {
  And(reg, reg, Operand(mask));
}

inline void MaglevAssembler::OrInt32(Register reg, int mask) {
  Or(reg, reg, Operand(mask));
}

inline void MaglevAssembler::AndInt32(Register reg, Register other) {
  and_(reg, reg, other);
}

inline void MaglevAssembler::OrInt32(Register reg, Register other) {
  or_(reg, reg, other);
}

inline void MaglevAssembler::ShiftLeft(Register reg, int amount) {
  slli_w(reg, reg, amount & 0x1f);
}

inline void MaglevAssembler::IncrementAddress(Register reg, int32_t delta) {
  Add_d(reg, reg, Operand(delta));
}

inline void MaglevAssembler::LoadAddress(Register dst, MemOperand location) {
  DCHECK(!location.hasIndexReg());
  Add_d(dst, location.base(), Operand(location.offset()));
}

inline void MaglevAssembler::EmitEnterExitFrame(int extra_slots,
                                                StackFrame::Type frame_type,
                                                Register c_function,
                                                Register scratch) {
  EnterExitFrame(scratch, extra_slots, frame_type);
}

inline void MaglevAssembler::Move(StackSlot dst, Register src) {
  St_d(src, StackSlotOperand(dst));
}
inline void MaglevAssembler::Move(StackSlot dst, DoubleRegister src) {
  Fst_d(src, StackSlotOperand(dst));
}
inline void MaglevAssembler::Move(Register dst, StackSlot src) {
  Ld_d(dst, StackSlotOperand(src));
}
inline void MaglevAssembler::Move(DoubleRegister dst, StackSlot src) {
  Fld_d(dst, StackSlotOperand(src));
}
inline void MaglevAssembler::Move(MemOperand dst, Register src) {
  St_d(src, dst);
}
inline void MaglevAssembler::Move(Register dst, MemOperand src) {
  Ld_d(dst, src);
}
inline void MaglevAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  fmov_d(dst, src);
}
inline void MaglevAssembler::Move(Register dst, Tagged<Smi> src) {
  MacroAssembler::Move(dst, src);
}
inline void MaglevAssembler::Move(Register dst, ExternalReference src) {
  li(dst, src);
}
inline void MaglevAssembler::Move(Register dst, Register src) {
  MacroAssembler::Move(dst, src);
}
inline void MaglevAssembler::Move(Register dst, Tagged<TaggedIndex> i) {
  li(dst, Operand(i.ptr()));
}
inline void MaglevAssembler::Move(Register dst, int32_t i) {
  li(dst, Operand(i));
}
inline void MaglevAssembler::Move(Register dst, uint32_t i) {
  li(dst, Operand(i));
}
inline void MaglevAssembler::Move(Register dst, intptr_t p) {
  li(dst, Operand(p));
}
inline void MaglevAssembler::Move(Register dst, IndirectPointerTag i) {
  li(dst, Operand(i));
}
inline void MaglevAssembler::Move(DoubleRegister dst, double n) {
  MacroAssembler::Move(dst, n);
}
inline void MaglevAssembler::Move(DoubleRegister dst, Float64 n) {
  MacroAssembler::Move(dst, n.get_scalar());
}
inline void MaglevAssembler::Move(Register dst, Handle<HeapObject> obj) {
  li(dst, Operand(obj));
}
void MaglevAssembler::MoveTagged(Register dst, Handle<HeapObject> obj) {
#ifdef V8_COMPRESS_POINTERS
  li(dst, Operand(obj, RelocInfo::COMPRESSED_EMBEDDED_OBJECT));
#else
  li(dst, obj);
#endif
}

inline void MaglevAssembler::LoadInt32(Register dst, MemOperand src) {
  Ld_w(dst, src);
}

inline void MaglevAssembler::StoreInt32(MemOperand dst, Register src) {
  St_w(src, dst);
}

inline void MaglevAssembler::LoadFloat32(DoubleRegister dst, MemOperand src) {
  Fld_s(dst, src);
  fcvt_d_s(dst, dst);
}
inline void MaglevAssembler::StoreFloat32(MemOperand dst, DoubleRegister src) {
  TemporaryRegisterScope temps(this);
  DoubleRegister scratch = temps.AcquireScratchDouble();
  fcvt_s_d(scratch, src);
  Fst_s(scratch, dst);
}
inline void MaglevAssembler::LoadFloat64(DoubleRegister dst, MemOperand src) {
  Fld_d(dst, src);
}
inline void MaglevAssembler::StoreFloat64(MemOperand dst, DoubleRegister src) {
  Fst_d(src, dst);
}

inline void MaglevAssembler::LoadUnalignedFloat64(DoubleRegister dst,
                                                  Register base,
                                                  Register index) {
  LoadFloat64(dst, MemOperand(base, index));
}
inline void MaglevAssembler::LoadUnalignedFloat64AndReverseByteOrder(
    DoubleRegister dst, Register base, Register index) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Ld_d(scratch, MemOperand(base, index));
  ByteSwap(scratch, scratch, 8);
  movgr2fr_d(dst, scratch);
}
inline void MaglevAssembler::StoreUnalignedFloat64(Register base,
                                                   Register index,
                                                   DoubleRegister src) {
  StoreFloat64(MemOperand(base, index), src);
}
inline void MaglevAssembler::ReverseByteOrderAndStoreUnalignedFloat64(
    Register base, Register index, DoubleRegister src) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  movfr2gr_d(scratch, src);
  ByteSwap(scratch, scratch, 8);
  St_d(scratch, MemOperand(base, index));
}

inline void MaglevAssembler::SignExtend32To64Bits(Register dst, Register src) {
  slli_w(dst, src, 0);
}

inline void MaglevAssembler::NegateInt32(Register val) {
  sub_w(val, zero_reg, val);
}

inline void MaglevAssembler::ToUint8Clamped(Register result,
                                            DoubleRegister value, Label* min,
                                            Label* max, Label* done) {
  TemporaryRegisterScope temps(this);
  DoubleRegister scratch = temps.AcquireScratchDouble();
  // Set to 0 if NaN.
  CompareF64(value, kDoubleRegZero, CULE);
  BranchTrueF(min);
  Move(scratch, 255.0);
  CompareF64(scratch, value, CLE);
  BranchTrueF(max);
  // if value in [0, 255], then round up to the nearest.
  Round_d(scratch, value);
  TruncateDoubleToInt32(result, scratch);
  jmp(done);
}

template <typename NodeT>
inline void MaglevAssembler::DeoptIfBufferNotValid(Register array,
                                                   Register scratch,
                                                   TypedArrayAccessMode mode,
                                                   NodeT* node) {
  // A detached buffer leads to megamorphic feedback, so we won't have a deopt
  // loop if we deopt here.
  LoadTaggedField(scratch,
                  FieldMemOperand(array, JSArrayBufferView::kBufferOffset));
  LoadTaggedField(scratch,
                  FieldMemOperand(scratch, JSArrayBuffer::kBitFieldOffset));
  And(scratch, scratch, Operand(JSArrayBuffer::NotValidMask(mode)));
  Label* deopt_label =
      GetDeoptLabel(node, DeoptimizeReason::kArrayBufferWasDetached);
  RecordComment("-- Jump to eager deopt");
  MacroAssembler::Branch(deopt_label, not_equal, scratch, Operand(zero_reg));
}

inline void MaglevAssembler::LoadByte(Register dst, MemOperand src) {
  Ld_bu(dst, src);
}

inline Condition MaglevAssembler::IsCallableAndNotUndetectable(
    Register map, Register scratch) {
  Ld_bu(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  And(scratch, scratch,
      Operand(Map::Bits1::IsUndetectableBit::kMask |
              Map::Bits1::IsCallableBit::kMask));
  // NB: TestTypeOf=>Branch=>JumpIf expects the result of a comparison
  // in dedicated "flag" register
  constexpr Register bit_set_flag = MaglevAssembler::GetFlagsRegister();
  Sub_w(bit_set_flag, scratch, Operand(Map::Bits1::IsCallableBit::kMask));
  return kEqual;
}

inline Condition MaglevAssembler::IsNotCallableNorUndetactable(
    Register map, Register scratch) {
  Ld_bu(scratch, FieldMemOperand(map, Map::kBitFieldOffset));

  // NB: TestTypeOf=>Branch=>JumpIf expects the result of a comparison
  // in dedicated "flag" register
  constexpr Register bits_unset_flag = MaglevAssembler::GetFlagsRegister();
  And(bits_unset_flag, scratch,
      Operand(Map::Bits1::IsUndetectableBit::kMask |
              Map::Bits1::IsCallableBit::kMask));
  return kEqual;
}

inline void MaglevAssembler::LoadInstanceType(Register instance_type,
                                              Register heap_object) {
  LoadMap(instance_type, heap_object);
  Ld_hu(instance_type,
        FieldMemOperand(instance_type, Map::kInstanceTypeOffset));
}

inline void MaglevAssembler::CompareInstanceTypeAndJumpIf(
    Register map, InstanceType type, Condition cond, Label* target,
    Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Ld_hu(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  MacroAssembler::Branch(target, cond, scratch, Operand(type) /*, distance*/);
}

inline Condition MaglevAssembler::CompareInstanceTypeRange(
    Register map, Register instance_type_out, InstanceType lower_limit,
    InstanceType higher_limit) {
  DCHECK_LT(lower_limit, higher_limit);
  Ld_hu(instance_type_out, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Sub_w(instance_type_out, instance_type_out, Operand(lower_limit));
  Register aflag = MaglevAssembler::GetFlagsRegister();
  // NB: JumpIf expects the result in dedicated "flag" register
  Sleu(aflag, instance_type_out, Operand(higher_limit - lower_limit));
  return kNotZero;
}

inline void MaglevAssembler::JumpIfNotObjectType(Register heap_object,
                                                 InstanceType type,
                                                 Label* target,
                                                 Label::Distance distance) {
  ASM_CODE_COMMENT(this);
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  GetObjectType(heap_object, scratch, scratch);
  MacroAssembler::Branch(target, ne, scratch, Operand(type));
}

inline void MaglevAssembler::AssertObjectType(Register heap_object,
                                              InstanceType type,
                                              AbortReason reason) {
  ASM_CODE_COMMENT(this);
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  AssertNotSmi(heap_object);
  GetObjectType(heap_object, scratch, scratch);
  MacroAssembler::Assert(eq, reason, scratch, Operand(type));
}

inline void MaglevAssembler::BranchOnObjectType(
    Register heap_object, InstanceType type, Label* if_true,
    Label::Distance true_distance, bool fallthrough_when_true, Label* if_false,
    Label::Distance false_distance, bool fallthrough_when_false) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  GetObjectType(heap_object, scratch, scratch);
  Cmp(scratch, Operand(type));
  Branch(kEqual, if_true, true_distance, fallthrough_when_true, if_false,
         false_distance, fallthrough_when_false);
}

inline void MaglevAssembler::JumpIfObjectType(Register heap_object,
                                              InstanceType type, Label* target,
                                              Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  GetObjectType(heap_object, scratch, scratch);
  Cmp(scratch, Operand(type));
  JumpIf(kEqual, target, distance);
}

inline void MaglevAssembler::JumpIfObjectTypeInRange(Register heap_object,
                                                     InstanceType lower_limit,
                                                     InstanceType higher_limit,
                                                     Label* target,
                                                     Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  LoadMap(scratch, heap_object);
  Ld_hu(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  Sub_w(scratch, scratch, Operand(lower_limit));
  MacroAssembler::Branch(target, kUnsignedLessThanEqual, scratch,
                         Operand(higher_limit - lower_limit));
}

inline void MaglevAssembler::JumpIfObjectTypeNotInRange(
    Register heap_object, InstanceType lower_limit, InstanceType higher_limit,
    Label* target, Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  LoadMap(scratch, heap_object);
  Ld_hu(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  Sub_w(scratch, scratch, Operand(lower_limit));
  MacroAssembler::Branch(target, kUnsignedGreaterThan, scratch,
                         Operand(higher_limit - lower_limit));
}

inline void MaglevAssembler::AssertObjectTypeInRange(Register heap_object,
                                                     InstanceType lower_limit,
                                                     InstanceType higher_limit,
                                                     AbortReason reason) {
  AssertNotSmi(heap_object);
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  LoadMap(scratch, heap_object);
  Ld_hu(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  Sub_w(scratch, scratch, Operand(lower_limit));
  MacroAssembler::Assert(kUnsignedLessThanEqual, reason, scratch,
                         Operand(higher_limit - lower_limit));
}

inline void MaglevAssembler::BranchOnObjectTypeInRange(
    Register heap_object, InstanceType lower_limit, InstanceType higher_limit,
    Label* if_true, Label::Distance true_distance, bool fallthrough_when_true,
    Label* if_false, Label::Distance false_distance,
    bool fallthrough_when_false) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  LoadMap(scratch, heap_object);
  Ld_hu(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  Sub_w(scratch, scratch, Operand(lower_limit));
  constexpr Register flags_reg = MaglevAssembler::GetFlagsRegister();
  // if scratch <= (higher_limit - lower_limit) then flags_reg = 0 else
  // flags_reg = 1
  Sgtu(flags_reg, scratch, Operand(higher_limit - lower_limit));
  // now compare against 0 witj kEqual
  Branch(Condition::kEqual, if_true, true_distance, fallthrough_when_true,
         if_false, false_distance, fallthrough_when_false);
}

#if V8_STATIC_ROOTS_BOOL
inline void MaglevAssembler::JumpIfObjectInRange(Register heap_object,
                                                 Tagged_t lower_limit,
                                                 Tagged_t higher_limit,
                                                 Label* target,
                                                 Label::Distance distance) {
  // Only allowed for comparisons against RORoots.
  DCHECK_LE(lower_limit, StaticReadOnlyRoot::kLastAllocatedRoot);
  DCHECK_LE(higher_limit, StaticReadOnlyRoot::kLastAllocatedRoot);
  AssertNotSmi(heap_object);
  JumpIfIsInRange(heap_object, lower_limit, higher_limit, target);
}

inline void MaglevAssembler::JumpIfObjectNotInRange(Register heap_object,
                                                    Tagged_t lower_limit,
                                                    Tagged_t higher_limit,
                                                    Label* target,
                                                    Label::Distance distance) {
  // Only allowed for comparisons against RORoots.
  DCHECK_LE(lower_limit, StaticReadOnlyRoot::kLastAllocatedRoot);
  DCHECK_LE(higher_limit, StaticReadOnlyRoot::kLastAllocatedRoot);
  AssertNotSmi(heap_object);
  JumpIfNotInRange(heap_object, lower_limit, higher_limit, target);
}

inline void MaglevAssembler::AssertObjectInRange(Register heap_object,
                                                 Tagged_t lower_limit,
                                                 Tagged_t higher_limit,
                                                 AbortReason reason) {
  // Only allowed for comparisons against RORoots.
  DCHECK_LE(lower_limit, StaticReadOnlyRoot::kLastAllocatedRoot);
  DCHECK_LE(higher_limit, StaticReadOnlyRoot::kLastAllocatedRoot);
  AssertNotSmi(heap_object);
  AssertInRange(heap_object, lower_limit, higher_limit, reason);
}
#endif

inline void MaglevAssembler::JumpIfJSAnyIsNotPrimitive(
    Register heap_object, Label* target, Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  MacroAssembler::JumpIfJSAnyIsNotPrimitive(heap_object, scratch,
                                            target /*, distance*/);
}

template <typename NodeT>
inline void MaglevAssembler::CompareRootAndEmitEagerDeoptIf(
    Register reg, RootIndex index, Condition cond, DeoptimizeReason reason,
    NodeT* node) {
  Label Deopt, Done;

  CompareRootAndBranch(reg, index, cond, &Deopt);
  Jump(&Done, Label::kNear);
  bind(&Deopt);
  EmitEagerDeopt(node, reason);
  bind(&Done);
}

template <typename NodeT>
inline void MaglevAssembler::CompareMapWithRootAndEmitEagerDeoptIf(
    Register reg, RootIndex index, Register scratch, Condition cond,
    DeoptimizeReason reason, NodeT* node) {
  CompareMapWithRoot(reg, index, scratch);
  DCHECK_EQ(cond, kNotEqual);  // so far we only support kNotEqual, flag reg is
                               // 0 for equal, 1 for not equal
  EmitEagerDeoptIf(cond, reason,
                   node);  // Jump to deopt only if flag reg is not equal 0
}

template <typename NodeT>
inline void MaglevAssembler::CompareTaggedRootAndEmitEagerDeoptIf(
    Register reg, RootIndex index, Condition cond, DeoptimizeReason reason,
    NodeT* node) {
  DCHECK_EQ(cond, kNotEqual);
  Label* deopt_label = GetDeoptLabel(node, reason);
  RecordComment("-- Jump to eager deopt");
  MacroAssembler::CompareTaggedRootAndBranch(reg, index, cond, deopt_label);
}

template <typename NodeT>
inline void MaglevAssembler::CompareUInt32AndEmitEagerDeoptIf(
    Register reg, int imm, Condition cond, DeoptimizeReason reason,
    NodeT* node) {
  Label Deopt, Done;

  MacroAssembler::Branch(&Deopt, cond, reg, Operand(imm), Label::kNear);
  Jump(&Done, Label::kNear);
  bind(&Deopt);
  EmitEagerDeopt(node, reason);
  bind(&Done);
}

inline void MaglevAssembler::CompareMapWithRoot(Register object,
                                                RootIndex index,
                                                Register scratch) {
  if (V8_STATIC_ROOTS_BOOL && RootsTable::IsReadOnly(index)) {
    LoadCompressedMap(scratch, object);
    CmpTagged(scratch, Operand(ReadOnlyRootPtr(index)));
    return;
  }
  LoadMap(scratch, object);
  CompareRoot(scratch, index);
}

template <typename NodeT>
inline void MaglevAssembler::CompareInstanceTypeRangeAndEagerDeoptIf(
    Register map, Register instance_type_out, InstanceType lower_limit,
    InstanceType higher_limit, Condition cond, DeoptimizeReason reason,
    NodeT* node) {
  DCHECK_LT(lower_limit, higher_limit);
  Ld_hu(instance_type_out, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Sub_w(instance_type_out, instance_type_out, Operand(lower_limit));
  DCHECK_EQ(cond, kUnsignedGreaterThan);
  Label* deopt_label = GetDeoptLabel(node, reason);
  RecordComment("-- Jump to eager deopt");
  MacroAssembler::Branch(deopt_label, Ugreater, instance_type_out,
                         Operand(higher_limit - lower_limit));
}

inline void MaglevAssembler::CompareFloat64AndJumpIf(
    DoubleRegister src1, DoubleRegister src2, Condition cond, Label* target,
    Label* nan_failed, Label::Distance distance) {
  bool need_swap;
  bool can_combine_check_un = target == nan_failed;
  FPUCondition cc = FPUConditionFor(cond, &need_swap, can_combine_check_un);
  DoubleRegister left = need_swap ? src2 : src1;
  DoubleRegister right = need_swap ? src1 : src2;
  if (!can_combine_check_un) {
    CompareF64(left, right, CUN);
    BranchTrueF(nan_failed);
  }
  CompareF64(left, right, cc);
  BranchTrueF(target);
}

inline void MaglevAssembler::CompareFloat64AndBranch(
    DoubleRegister src1, DoubleRegister src2, Condition cond,
    BasicBlock* if_true, BasicBlock* if_false, BasicBlock* next_block,
    BasicBlock* nan_failed) {
  bool need_swap;
  bool can_combine_check_un = if_true->label() == nan_failed->label();
  FPUCondition cc = FPUConditionFor(cond, &need_swap, can_combine_check_un);
  DoubleRegister left = need_swap ? src2 : src1;
  DoubleRegister right = need_swap ? src1 : src2;
  if (!can_combine_check_un) {
    CompareF64(left, right, CUN);
    BranchTrueF(nan_failed->label());
  }
  CompareF64(left, right, cc);
  BranchTrueF(if_true->label());
  MacroAssembler::Branch(if_false->label());
}

inline void MaglevAssembler::PrepareCallCFunction(int num_reg_arguments,
                                                  int num_double_registers) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  MacroAssembler::PrepareCallCFunction(num_reg_arguments, num_double_registers,
                                       scratch);
}

inline void MaglevAssembler::CallSelf() {
  DCHECK(allow_call());
  DCHECK(code_gen_state()->entry_label()->is_bound());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  MacroAssembler::Call(code_gen_state()->entry_label());
}

inline void MaglevAssembler::Jump(Label* target, Label::Distance distance) {
  DCHECK(!IsDeoptLabel(target));
  MacroAssembler::Branch(target /*, distance*/);
}

inline void MaglevAssembler::JumpToDeopt(Label* target) {
  DCHECK(IsDeoptLabel(target));
  MacroAssembler::Branch(target);
}

inline void MaglevAssembler::EmitEagerDeoptStress(Label* target) {
  // TODO(olivf): On loong64 `--deopt-every-n-times` is currently not supported.
}

inline void MaglevAssembler::JumpIf(Condition cond, Label* target,
                                    Label::Distance distance) {
  // NOTE: for now keep in mind that we always put the result of a comparison
  // into dedicated register ("set flag"), and then compare it with zero_reg.
  constexpr Register aflag = MaglevAssembler::GetFlagsRegister();
  if (cond == Condition::kOverflow) {
    cond = ne;
  } else if (cond == Condition::kNoOverflow) {
    cond = eq;
  }
  MacroAssembler::Branch(target, cond, aflag, Operand(zero_reg));
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
                                 Label* target, Label::Distance distance) {
  MacroAssembler::Branch(target, cc, value, Operand(byte) /*, distance*/);
}

void MaglevAssembler::Float64SilenceNan(DoubleRegister value) {
  FPUCanonicalizeNaN(value, value);
}

#ifdef V8_ENABLE_UNDEFINED_DOUBLE
void MaglevAssembler::JumpIfUndefinedNan(DoubleRegister value, Register scratch,
                                         Label* target,
                                         Label::Distance distance) {
  // TODO(leszeks): Right now this only accepts Zone-allocated target labels.
  // This works because all callsites are jumping to either a deopt, deferred
  // code, or a basic block. If we ever need to jump to an on-stack label, we
  // have to add support for it here change the caller to pass a ZoneLabelRef.
  DCHECK(compilation_info()->zone()->Contains(target));
  ZoneLabelRef is_undefined = ZoneLabelRef::UnsafeFromLabelPointer(target);
  ZoneLabelRef is_not_undefined(this);
  JumpIfNan(
      value,
      MakeDeferredCode(
          [](MaglevAssembler* masm, DoubleRegister value, Register scratch,
             ZoneLabelRef is_undefined, ZoneLabelRef is_not_undefined) {
            masm->movfrh2gr_s(scratch, value);
            masm->MacroAssembler::Branch(
                *is_undefined, kEqual, scratch,
                Operand(static_cast<int32_t>(kUndefinedNanUpper32)));
            masm->Jump(*is_not_undefined);
          },
          value, scratch, is_undefined, is_not_undefined));
  bind(*is_not_undefined);
}

void MaglevAssembler::JumpIfUndefinedNan(MemOperand operand, Label* target,
                                         Label::Distance distance) {
  MaglevAssembler::TemporaryRegisterScope temps(this);
  Register upper_bits = temps.AcquireScratch();
  DCHECK(!operand.hasIndexReg());
  Ld_w(upper_bits,
       MemOperand(operand.base(), operand.offset() + (kDoubleSize / 2)));
  MacroAssembler::Branch(target, eq, upper_bits,
                         Operand(static_cast<int32_t>(kUndefinedNanUpper32)));
}

void MaglevAssembler::JumpIfNotUndefinedNan(DoubleRegister value,
                                            Register scratch, Label* target,
                                            Label::Distance distance) {
  JumpIfNotNan(value, target, distance);
  movfrh2gr_s(scratch, value);
  MacroAssembler::Branch(target, ne, scratch,
                         Operand(static_cast<int32_t>(kUndefinedNanUpper32)));
}
#endif  // V8_ENABLE_UNDEFINED_DOUBLE

void MaglevAssembler::JumpIfHoleNan(DoubleRegister value, Register scratch,
                                    Label* target, Label::Distance distance) {
  // TODO(leszeks): Right now this only accepts Zone-allocated target labels.
  // This works because all callsites are jumping to either a deopt, deferred
  // code, or a basic block. If we ever need to jump to an on-stack label, we
  // have to add support for it here change the caller to pass a ZoneLabelRef.
  DCHECK(compilation_info()->zone()->Contains(target));
  ZoneLabelRef is_hole = ZoneLabelRef::UnsafeFromLabelPointer(target);
  ZoneLabelRef is_not_hole(this);
  CompareF64(value, value, CUN);
  BranchTrueF(MakeDeferredCode(
      [](MaglevAssembler* masm, DoubleRegister value, Register scratch,
         ZoneLabelRef is_hole, ZoneLabelRef is_not_hole) {
        masm->movfrh2gr_s(scratch, value);
        masm->MacroAssembler::Branch(
            *is_hole, eq, scratch,
            Operand(static_cast<int32_t>(kHoleNanUpper32)));
        masm->Jump(*is_not_hole);
      },
      value, scratch, is_hole, is_not_hole));
  bind(*is_not_hole);
}

void MaglevAssembler::JumpIfHoleNan(MemOperand operand, Label* target,
                                    Label::Distance distance) {
  MaglevAssembler::TemporaryRegisterScope temps(this);
  Register upper_bits = temps.AcquireScratch();
  DCHECK(!operand.hasIndexReg());
  Ld_w(upper_bits,
       MemOperand(operand.base(), operand.offset() + (kDoubleSize / 2)));
  MacroAssembler::Branch(target, eq, upper_bits,
                         Operand(static_cast<int32_t>(kHoleNanUpper32)));
}

void MaglevAssembler::JumpIfNotHoleNan(DoubleRegister value, Register scratch,
                                       Label* target,
                                       Label::Distance distance) {
  JumpIfNotNan(value, target /*, distance*/);
  movfrh2gr_s(scratch, value);
  MacroAssembler::Branch(target, ne, scratch,
                         Operand(static_cast<int32_t>(kHoleNanUpper32)));
}

void MaglevAssembler::JumpIfNotHoleNan(MemOperand operand, Label* target,
                                       Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register upper_bits = temps.AcquireScratch();
  DCHECK(!operand.hasIndexReg());
  Ld_w(upper_bits,
       MemOperand(operand.base(), operand.offset() + (kDoubleSize / 2)));
  MacroAssembler::Branch(target, ne, upper_bits,
                         Operand(static_cast<int32_t>(kHoleNanUpper32)));
}

void MaglevAssembler::JumpIfNan(DoubleRegister value, Label* target,
                                Label::Distance distance) {
  CompareF64(value, value, CUN);
  BranchTrueF(target);
}

void MaglevAssembler::JumpIfNotNan(DoubleRegister value, Label* target,
                                   Label::Distance distance) {
  CompareF64(value, value, CUN);
  BranchFalseF(target);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, Register r2,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register lhs = temps.AcquireScratch();
  Register rhs = temps.AcquireScratch();
  // TODO(loong64): Recheck sign-extension here
  slli_w(lhs, r1, 0);
  slli_w(rhs, r2, 0);
  MacroAssembler::Branch(target, cond, lhs, Operand(rhs) /*, distance*/);
}

void MaglevAssembler::CompareIntPtrAndJumpIf(Register r1, Register r2,
                                             Condition cond, Label* target,
                                             Label::Distance distance) {
  MacroAssembler::Branch(target, cond, r1, Operand(r2) /*, distance*/);
}

void MaglevAssembler::CompareIntPtrAndJumpIf(Register r1, int32_t value,
                                             Condition cond, Label* target,
                                             Label::Distance distance) {
  MacroAssembler::Branch(target, cond, r1, Operand(value) /*, distance*/);
}

inline void MaglevAssembler::CompareInt32AndJumpIf(Register r1, int32_t value,
                                                   Condition cond,
                                                   Label* target,
                                                   Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register lhs = temps.AcquireScratch();
  // TODO(loong64): Recheck sign-extension here
  slli_w(lhs, r1, 0);
  MacroAssembler::Branch(target, cond, lhs, Operand(value) /*, distance*/);
}

inline void MaglevAssembler::CompareInt32AndAssert(Register r1, Register r2,
                                                   Condition cond,
                                                   AbortReason reason) {
  // TODO(loong64): Recheck if CompareInt32AndAssert needs sign-extension.
  MacroAssembler::Assert(cond, reason, r1, Operand(r2));
}

inline void MaglevAssembler::CompareInt32AndAssert(Register r1, int32_t value,
                                                   Condition cond,
                                                   AbortReason reason) {
  TemporaryRegisterScope temps(this);
  Register lhs = temps.AcquireScratch();
  // TODO(loong64): Recheck sign-extension here
  slli_w(lhs, r1, 0);
  MacroAssembler::Assert(cond, reason, lhs, Operand(value));
}

inline void MaglevAssembler::Branch(Register r1, const Operand& r2,
                                    Condition cond, Label* if_true,
                                    bool fallthrough_when_true, Label* if_false,
                                    bool fallthrough_when_false) {
  if (fallthrough_when_false) {
    if (fallthrough_when_true) {
      // If both paths are a fallthrough, do nothing.
      DCHECK_EQ(if_true, if_false);
      return;
    }
    // Jump over the false block if true, otherwise fall through into it.
    MacroAssembler::Branch(if_true, cond, r1, r2);
  } else {
    // Jump to the false block if true.
    MacroAssembler::Branch(if_false, NegateCondition(cond), r1, r2);
    // Jump to the true block if it's not the next block.
    if (!fallthrough_when_true) {
      MacroAssembler::Branch(if_true);
    }
  }
}

inline void MaglevAssembler::CompareInt32AndBranch(
    Register r1, int32_t value, Condition cond, Label* if_true,
    Label::Distance true_distance, bool fallthrough_when_true, Label* if_false,
    Label::Distance false_distance, bool fallthrough_when_false) {
  TemporaryRegisterScope temps(this);
  Register lhs = temps.AcquireScratch();
  // TODO(loong64): Recheck sign-extension here
  slli_w(lhs, r1, 0);
  Branch(lhs, Operand(value), cond, if_true, fallthrough_when_true, if_false,
         fallthrough_when_false);
}

inline void MaglevAssembler::CompareInt32AndBranch(
    Register r1, Register value, Condition cond, Label* if_true,
    Label::Distance true_distance, bool fallthrough_when_true, Label* if_false,
    Label::Distance false_distance, bool fallthrough_when_false) {
  TemporaryRegisterScope temps(this);
  Register lhs = temps.AcquireScratch();
  Register rhs = temps.AcquireScratch();
  // TODO(loong64): Recheck sign-extension here
  slli_w(lhs, r1, 0);
  slli_w(rhs, value, 0);
  Branch(lhs, Operand(rhs), cond, if_true, fallthrough_when_true, if_false,
         fallthrough_when_false);
}

inline void MaglevAssembler::CompareIntPtrAndBranch(
    Register r1, int32_t value, Condition cond, Label* if_true,
    Label::Distance true_distance, bool fallthrough_when_true, Label* if_false,
    Label::Distance false_distance, bool fallthrough_when_false) {
  Branch(r1, Operand(value), cond, if_true, fallthrough_when_true, if_false,
         fallthrough_when_false);
}

inline void MaglevAssembler::CompareSmiAndJumpIf(Register r1, Tagged<Smi> value,
                                                 Condition cond, Label* target,
                                                 Label::Distance distance) {
  AssertSmi(r1);
  CompareTaggedAndBranch(target, cond, r1, Operand(value) /*, distance*/);
}

inline void MaglevAssembler::CompareByteAndJumpIf(MemOperand left, int8_t right,
                                                  Condition cond,
                                                  Register scratch,
                                                  Label* target,
                                                  Label::Distance distance) {
  LoadByte(scratch, left);
  MacroAssembler::Branch(target, cond, scratch, Operand(right) /*, distance*/);
}

inline void MaglevAssembler::CompareTaggedAndJumpIf(Register r1,
                                                    Tagged<Smi> value,
                                                    Condition cond,
                                                    Label* target,
                                                    Label::Distance distance) {
  CompareTaggedAndBranch(target, cond, r1, Operand(value));
}

inline void MaglevAssembler::CompareTaggedAndJumpIf(Register r1,
                                                    Handle<HeapObject> obj,
                                                    Condition cond,
                                                    Label* target,
                                                    Label::Distance distance) {
  CompareTaggedAndBranch(target, cond, r1, Operand(obj));
}

inline void MaglevAssembler::CompareTaggedAndJumpIf(Register src1,
                                                    Register src2,
                                                    Condition cond,
                                                    Label* target,
                                                    Label::Distance distance) {
  CompareTaggedAndBranch(target, cond, src1, Operand(src2));
}

inline void MaglevAssembler::CompareDoubleAndJumpIfZeroOrNaN(
    DoubleRegister reg, Label* target, Label::Distance distance) {
  CompareF64(reg, kDoubleRegZero, CUEQ);
  BranchTrueF(target);
}

inline void MaglevAssembler::CompareDoubleAndJumpIfZeroOrNaN(
    MemOperand operand, Label* target, Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  DoubleRegister value_double = temps.AcquireScratchDouble();
  Fld_d(value_double, operand);
  CompareDoubleAndJumpIfZeroOrNaN(value_double, target, distance);
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    Register r1, int32_t mask, Label* target, Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  And(scratch, r1, Operand(static_cast<uint32_t>(mask)));
  MacroAssembler::Branch(target, ne, scratch, Operand(zero_reg) /*, distance*/);
}

inline void MaglevAssembler::TestInt32AndJumpIfAnySet(
    MemOperand operand, int32_t mask, Label* target, Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Ld_wu(scratch, operand);
  And(scratch, scratch, Operand(static_cast<uint32_t>(mask)));
  MacroAssembler::Branch(target, kNotZero, scratch,
                         Operand(zero_reg) /*, distance*/);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    Register r1, int32_t mask, Label* target, Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  And(scratch, r1, Operand(static_cast<uint32_t>(mask)));
  MacroAssembler::Branch(target, eq, scratch, Operand(zero_reg) /*, distance*/);
}

inline void MaglevAssembler::TestInt32AndJumpIfAllClear(
    MemOperand operand, int32_t mask, Label* target, Label::Distance distance) {
  MaglevAssembler::TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Ld_wu(scratch, operand);
  And(scratch, scratch, Operand(static_cast<uint32_t>(mask)));
  MacroAssembler::Branch(target, eq, scratch, Operand(zero_reg) /*, distance*/);
}

inline void MaglevAssembler::TestUint8AndJumpIfAnySet(
    MemOperand operand, uint8_t mask, Label* target, Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Ld_bu(scratch, operand);
  And(scratch, scratch, Operand(mask));
  MacroAssembler::Branch(target, ne, scratch, Operand(zero_reg) /*, distance*/);
}

inline void MaglevAssembler::TestUint8AndJumpIfAllClear(
    MemOperand operand, uint8_t mask, Label* target, Label::Distance distance) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  Ld_bu(scratch, operand);
  And(scratch, scratch, Operand(mask));
  MacroAssembler::Branch(target, eq, scratch, Operand(zero_reg) /*, distance*/);
}

inline void MaglevAssembler::LoadContextCellState(Register state,
                                                  Register cell) {
  Ld_w(state, FieldMemOperand(cell, offsetof(ContextCell, state_)));
}

inline void MaglevAssembler::LoadContextCellInt32Value(Register value,
                                                       Register cell) {
  AssertContextCellState(cell, ContextCell::kInt32);
  Ld_w(value, FieldMemOperand(cell, offsetof(ContextCell, double_value_)));
}

inline void MaglevAssembler::LoadContextCellFloat64Value(DoubleRegister value,
                                                         Register cell) {
  AssertContextCellState(cell, ContextCell::kFloat64);
  Fld_d(value, FieldMemOperand(cell, offsetof(ContextCell, double_value_)));
}

inline void MaglevAssembler::StoreContextCellInt32Value(Register cell,
                                                        Register value) {
  St_w(value, FieldMemOperand(cell, offsetof(ContextCell, double_value_)));
}

inline void MaglevAssembler::StoreContextCellFloat64Value(
    Register cell, DoubleRegister value) {
  Fst_d(value, FieldMemOperand(cell, offsetof(ContextCell, double_value_)));
}

inline void MaglevAssembler::LoadHeapNumberValue(DoubleRegister result,
                                                 Register heap_number) {
  Fld_d(result, FieldMemOperand(heap_number, offsetof(HeapNumber, value_)));
}

inline void MaglevAssembler::Int32ToDouble(DoubleRegister result,
                                           Register src) {
  movgr2fr_w(result, src);
  ffint_d_w(result, result);
}

inline void MaglevAssembler::Uint32ToDouble(DoubleRegister result,
                                            Register src) {
  Ffint_d_uw(result, src);
}

inline void MaglevAssembler::IntPtrToDouble(DoubleRegister result,
                                            Register src) {
  movgr2fr_d(result, src);
  ffint_d_l(result, result);
}

inline void MaglevAssembler::Pop(Register dst) { MacroAssembler::Pop(dst); }

inline void MaglevAssembler::AssertStackSizeCorrect() {
  if (v8_flags.slow_debug_code) {
    TemporaryRegisterScope temps(this);
    Register scratch = temps.AcquireScratch();
    Add_d(scratch, sp,
          Operand(code_gen_state()->stack_slots() * kSystemPointerSize +
                  StandardFrameConstants::kFixedFrameSizeFromFp));
    MacroAssembler::Assert(eq, AbortReason::kStackAccessBelowStackPointer,
                           scratch, Operand(fp));
  }
}

inline Condition MaglevAssembler::FunctionEntryStackCheck(
    int stack_check_offset) {
  TemporaryRegisterScope temps(this);
  Register stack_cmp_reg = sp;
  if (stack_check_offset >= kStackLimitSlackForDeoptimizationInBytes) {
    stack_cmp_reg = temps.AcquireScratch();
    Sub_d(stack_cmp_reg, sp, Operand(stack_check_offset));
  }
  Register interrupt_stack_limit = temps.AcquireScratch();
  LoadStackLimit(interrupt_stack_limit, StackLimitKind::kInterruptStackLimit);
  // Flags register is used in subsequent JumpIfs
  constexpr Register flags_reg = MaglevAssembler::GetFlagsRegister();
  // FLAGS = ( predicted stack pointer < stack limit ) ? 1 : 0
  //     0 - we're Ok
  //     1 - stack will be overflown
  sltu(flags_reg, stack_cmp_reg, interrupt_stack_limit);
  return kZero;
}

inline void MaglevAssembler::FinishCode() {}

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
      return Ld_w(dst, src);
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kWord64:
      return Ld_d(dst, src);
    default:
      UNREACHABLE();
  }
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr,
                                      MemOperand dst, Register src) {
  switch (repr) {
    case MachineRepresentation::kWord32:
      return St_w(src, dst);
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kWord64:
      return St_d(src, dst);
    default:
      UNREACHABLE();
  }
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr,
                                      MemOperand dst, MemOperand src) {
  TemporaryRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  MoveRepr(repr, scratch, src);
  MoveRepr(repr, dst, scratch);
}

inline void MaglevAssembler::MaybeEmitPlaceHolderForDeopt() {
  // Implemented only for x64.
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_LOONG64_MAGLEV_ASSEMBLER_LOONG64_INL_H_
