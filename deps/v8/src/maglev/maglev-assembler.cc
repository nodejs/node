// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-code-generator.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

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
          using D = CallInterfaceDescriptorFor<
              Builtin::kCopyFastSmiOrObjectElements>::type;
          snapshot.live_registers.clear(result_reg);
          snapshot.live_tagged_registers.clear(result_reg);
          SaveRegisterStateForCall save_register_state(masm, snapshot);
          __ Move(D::GetRegisterParameter(D::kObject), object);
          __ CallBuiltin(Builtin::kCopyFastSmiOrObjectElements);
          save_register_state.DefineSafepoint();
          __ Move(result_reg, kReturnRegister0);
        }
        __ Jump(*done);
      },
      done, object, elements, register_snapshot);
  bind(*done);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
