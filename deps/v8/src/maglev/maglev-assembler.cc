// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-code-generator.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

void MaglevAssembler::AllocateHeapNumber(RegisterSnapshot register_snapshot,
                                         Register result,
                                         DoubleRegister value) {
  // In the case we need to call the runtime, we should spill the value
  // register. Even if it is not live in the next node, otherwise the
  // allocation call might trash it.
  register_snapshot.live_double_registers.set(value);
  Allocate(register_snapshot, result, HeapNumber::kSize);
  SetMapAsRoot(result, RootIndex::kHeapNumberMap);
  Move(FieldMemOperand(result, HeapNumber::kValueOffset), value);
}

void MaglevAssembler::AllocateTwoByteString(RegisterSnapshot register_snapshot,
                                            Register result, int length) {
  int size = SeqTwoByteString::SizeFor(length);
  Allocate(register_snapshot, result, size);
  StoreInt32Field(result, size - kObjectAlignment, 0);
  SetMapAsRoot(result, RootIndex::kStringMap);
  StoreInt32Field(result, Name::kRawHashFieldOffset, Name::kEmptyHashField);
  StoreInt32Field(result, String::kLengthOffset, length);
}

Register MaglevAssembler::FromAnyToRegister(const Input& input,
                                            Register scratch) {
  if (input.operand().IsConstant()) {
    input.node()->LoadToRegister(this, scratch);
    return scratch;
  }
  const compiler::AllocatedOperand& operand =
      compiler::AllocatedOperand::cast(input.operand());
  if (operand.IsRegister()) {
    return ToRegister(input);
  } else {
    DCHECK(operand.IsStackSlot());
    Move(scratch, ToMemOperand(input));
    return scratch;
  }
}

void MaglevAssembler::LoadSingleCharacterString(Register result,
                                                int char_code) {
  DCHECK_GE(char_code, 0);
  DCHECK_LT(char_code, String::kMaxOneByteCharCode);
  Register table = result;
  LoadRoot(table, RootIndex::kSingleCharacterStringTable);
  DecompressTagged(result, FieldMemOperand(table, FixedArray::kHeaderSize +
                                                      char_code * kTaggedSize));
}

void MaglevAssembler::LoadDataField(const PolymorphicAccessInfo& access_info,
                                    Register result, Register object,
                                    Register scratch) {
  Register load_source = object;
  // Resolve property holder.
  if (access_info.holder().has_value()) {
    load_source = scratch;
    Move(load_source, access_info.holder().value().object());
  }
  FieldIndex field_index = access_info.field_index();
  if (!field_index.is_inobject()) {
    Register load_source_object = load_source;
    if (load_source == object) {
      load_source = scratch;
    }
    // The field is in the property array, first load it from there.
    AssertNotSmi(load_source_object);
    DecompressTagged(load_source,
                     FieldMemOperand(load_source_object,
                                     JSReceiver::kPropertiesOrHashOffset));
  }
  AssertNotSmi(load_source);
  DecompressTagged(result, FieldMemOperand(load_source, field_index.offset()));
}

void MaglevAssembler::JumpIfNotUndetectable(Register object, Register scratch,
                                            CheckType check_type, Label* target,
                                            Label::Distance distance) {
  if (check_type == CheckType::kCheckHeapObject) {
    JumpIfSmi(object, target, distance);
  } else if (v8_flags.debug_code) {
    AssertNotSmi(object);
  }
  // For heap objects, check the map's undetectable bit.
  LoadMap(scratch, object);
  LoadByte(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
  TestInt32AndJumpIfAllClear(scratch, Map::Bits1::IsUndetectableBit::kMask,
                             target, distance);
}

void MaglevAssembler::JumpIfUndetectable(Register object, Register scratch,
                                         CheckType check_type, Label* target,
                                         Label::Distance distance) {
  Label detectable;
  if (check_type == CheckType::kCheckHeapObject) {
    JumpIfSmi(object, &detectable, Label::kNear);
  } else if (v8_flags.debug_code) {
    AssertNotSmi(object);
  }
  // For heap objects, check the map's undetectable bit.
  LoadMap(scratch, object);
  LoadByte(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
  TestInt32AndJumpIfAnySet(scratch, Map::Bits1::IsUndetectableBit::kMask,
                           target, distance);
  bind(&detectable);
}

void MaglevAssembler::EnsureWritableFastElements(
    RegisterSnapshot register_snapshot, Register elements, Register object,
    Register scratch) {
  ZoneLabelRef done(this);
  CompareMapWithRoot(elements, RootIndex::kFixedArrayMap, scratch);
  JumpToDeferredIf(
      kNotEqual,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object,
         Register result_reg, RegisterSnapshot snapshot) {
        {
          snapshot.live_registers.clear(result_reg);
          snapshot.live_tagged_registers.clear(result_reg);
          SaveRegisterStateForCall save_register_state(masm, snapshot);
          __ CallBuiltin<Builtin::kCopyFastSmiOrObjectElements>(object);
          save_register_state.DefineSafepoint();
          __ Move(result_reg, kReturnRegister0);
        }
        __ Jump(*done);
      },
      done, object, elements, register_snapshot);
  bind(*done);
}

void MaglevAssembler::ToBoolean(Register value, CheckType check_type,
                                ZoneLabelRef is_true, ZoneLabelRef is_false,
                                bool fallthrough_when_true) {
  ScratchRegisterScope temps(this);
  Register map = temps.GetDefaultScratchRegister();

  if (check_type == CheckType::kCheckHeapObject) {
    // Check if {{value}} is Smi.
    Condition is_smi = CheckSmi(value);
    JumpToDeferredIf(
        is_smi,
        [](MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
           ZoneLabelRef is_false) {
          // Check if {value} is not zero.
          __ CompareSmiAndJumpIf(value, Smi::FromInt(0), kEqual, *is_false);
          __ Jump(*is_true);
        },
        value, is_true, is_false);
  } else if (v8_flags.debug_code) {
    AssertNotSmi(value);
  }
  // Check if {{value}} is false.
  CompareRoot(value, RootIndex::kFalseValue);
  JumpIf(kEqual, *is_false);

  // Check if {{value}} is empty string.
  CompareRoot(value, RootIndex::kempty_string);
  JumpIf(kEqual, *is_false);

  // Check if {{value}} is undetectable.
  LoadMap(map, value);
  TestInt32AndJumpIfAnySet(FieldMemOperand(map, Map::kBitFieldOffset),
                           Map::Bits1::IsUndetectableBit::kMask, *is_false);

  // Check if {{value}} is a HeapNumber.
  CompareRoot(map, RootIndex::kHeapNumberMap);
  JumpToDeferredIf(
      kEqual,
      [](MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
         ZoneLabelRef is_false) {
        __ CompareDoubleAndJumpIfZeroOrNaN(
            FieldMemOperand(value, HeapNumber::kValueOffset), *is_false);
        __ Jump(*is_true);
      },
      value, is_true, is_false);

  // Check if {{value}} is a BigInt.
  CompareRoot(map, RootIndex::kBigIntMap);
  // {{map}} is not needed from this point on.
  temps.Include(map);
  JumpToDeferredIf(
      kEqual,
      [](MaglevAssembler* masm, Register value, Register map,
         ZoneLabelRef is_true, ZoneLabelRef is_false) {
        __ TestInt32AndJumpIfAllClear(
            FieldMemOperand(value, BigInt::kBitfieldOffset),
            BigInt::LengthBits::kMask, *is_false);
        __ Jump(*is_true);
      },
      value, map, is_true, is_false);

  // Otherwise true.
  if (!fallthrough_when_true) {
    Jump(*is_true);
  }
}

void MaglevAssembler::MaterialiseValueNode(Register dst, ValueNode* value) {
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
  DoubleRegister builtin_input_value = D::GetDoubleRegisterParameter(D::kValue);
  MemOperand src = ToMemOperand(value->allocation());
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kInt32: {
      Label done;
      ScratchRegisterScope temps(this);
      Register scratch = temps.GetDefaultScratchRegister();
      Move(scratch, src);
      SmiTagInt32AndJumpIfSuccess(dst, scratch, &done, Label::kNear);
      // If smi tagging fails, instead of bailing out (deopting), we change
      // representation to a HeapNumber.
      Int32ToDouble(builtin_input_value, scratch);
      CallBuiltin<Builtin::kNewHeapNumber>(builtin_input_value);
      Move(dst, kReturnRegister0);
      bind(&done);
      break;
    }
    case ValueRepresentation::kUint32: {
      Label done;
      ScratchRegisterScope temps(this);
      Register scratch = temps.GetDefaultScratchRegister();
      Move(scratch, src);
      SmiTagUint32AndJumpIfSuccess(dst, scratch, &done, Label::kNear);
      // If smi tagging fails, instead of bailing out (deopting), we change
      // representation to a HeapNumber.
      Uint32ToDouble(builtin_input_value, scratch);
      CallBuiltin<Builtin::kNewHeapNumber>(builtin_input_value);
      Move(dst, kReturnRegister0);
      bind(&done);
      break;
    }
    case ValueRepresentation::kFloat64:
      Move(builtin_input_value, src);
      CallBuiltin<Builtin::kNewHeapNumber>(builtin_input_value);
      Move(dst, kReturnRegister0);
      break;
    case ValueRepresentation::kHoleyFloat64: {
      // TODO(victorgomes): Check if this is a reachable case.
      Label done, box;
      JumpIfNotHoleNan(src, &box, Label::kNear);
      LoadRoot(dst, RootIndex::kUndefinedValue);
      Jump(&done);
      bind(&box);
      Move(builtin_input_value, src);
      CallBuiltin<Builtin::kNewHeapNumber>(builtin_input_value);
      Move(dst, kReturnRegister0);
      bind(&done);
      break;
    }
    case ValueRepresentation::kWord64:
    case ValueRepresentation::kTagged:
      UNREACHABLE();
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
