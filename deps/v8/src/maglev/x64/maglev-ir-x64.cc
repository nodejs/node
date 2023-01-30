// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/baseline/baseline-assembler-inl.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/maglev-safepoint-table.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/codegen/x64/register-x64.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/js-heap-broker.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/ic/handler-configuration.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/x64/maglev-assembler-x64-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

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

// ---
// Nodes
// ---

int GeneratorStore::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void GeneratorStore::SetValueLocationConstraints() {
  UseAny(context_input());
  UseRegister(generator_input());
  for (int i = 0; i < num_parameters_and_registers(); i++) {
    UseAny(parameters_and_registers(i));
  }
  RequireSpecificTemporary(WriteBarrierDescriptor::ObjectRegister());
  RequireSpecificTemporary(WriteBarrierDescriptor::SlotAddressRegister());
}
void GeneratorStore::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register generator = ToRegister(generator_input());
  Register array = WriteBarrierDescriptor::ObjectRegister();
  __ LoadTaggedPointerField(
      array, FieldOperand(generator,
                          JSGeneratorObject::kParametersAndRegistersOffset));

  for (int i = 0; i < num_parameters_and_registers(); i++) {
    // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
    // register since it's a known temporary, and the write barrier slow path
    // generates better code when value == scratch. Can't use kScratchRegister
    // because CheckPageFlag uses it.
    Register value =
        __ FromAnyToRegister(parameters_and_registers(i),
                             WriteBarrierDescriptor::SlotAddressRegister());

    ZoneLabelRef done(masm);
    DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
        [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
           Register array, GeneratorStore* node, int32_t offset) {
          ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
          // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
          // register, see comment above.
          __ CheckPageFlag(
              value, WriteBarrierDescriptor::SlotAddressRegister(),
              MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask,
              zero, *done);

          Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();

          __ leaq(slot_reg, FieldOperand(array, offset));

          // TODO(leszeks): Add an interface for flushing all double registers
          // before this Node, to avoid needing to save them here.
          SaveFPRegsMode const save_fp_mode =
              !node->register_snapshot().live_double_registers.is_empty()
                  ? SaveFPRegsMode::kSave
                  : SaveFPRegsMode::kIgnore;

          __ CallRecordWriteStub(array, slot_reg, save_fp_mode);

          __ jmp(*done);
        },
        done, value, array, this, FixedArray::OffsetOfElementAt(i));

    __ StoreTaggedField(FieldOperand(array, FixedArray::OffsetOfElementAt(i)),
                        value);
    __ JumpIfSmi(value, *done, Label::kNear);
    // TODO(leszeks): This will stay either false or true throughout this loop.
    // Consider hoisting the check out of the loop and duplicating the loop into
    // with and without write barrier.
    __ CheckPageFlag(array, kScratchRegister,
                     MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                     &deferred_write_barrier->deferred_code_label);

    __ bind(*done);
  }

  // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
  // register, see comment above.
  Register context = __ FromAnyToRegister(
      context_input(), WriteBarrierDescriptor::SlotAddressRegister());

  ZoneLabelRef done(masm);
  DeferredCodeInfo* deferred_context_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register context,
         Register generator, GeneratorStore* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
        // register, see comment above.
        // TODO(leszeks): The context is almost always going to be in
        // old-space, consider moving this check to the fast path, maybe even
        // as the first bailout.
        __ CheckPageFlag(
            context, WriteBarrierDescriptor::SlotAddressRegister(),
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, zero,
            *done);

        __ Move(WriteBarrierDescriptor::ObjectRegister(), generator);
        generator = WriteBarrierDescriptor::ObjectRegister();
        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();

        __ leaq(slot_reg,
                FieldOperand(generator, JSGeneratorObject::kContextOffset));

        // TODO(leszeks): Add an interface for flushing all double registers
        // before this Node, to avoid needing to save them here.
        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(generator, slot_reg, save_fp_mode);

        __ jmp(*done);
      },
      done, context, generator, this);
  __ StoreTaggedField(
      FieldOperand(generator, JSGeneratorObject::kContextOffset), context);
  __ AssertNotSmi(context);
  __ CheckPageFlag(generator, kScratchRegister,
                   MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                   &deferred_context_write_barrier->deferred_code_label);
  __ bind(*done);

  __ StoreTaggedSignedField(
      FieldOperand(generator, JSGeneratorObject::kContinuationOffset),
      Smi::FromInt(suspend_id()));
  __ StoreTaggedSignedField(
      FieldOperand(generator, JSGeneratorObject::kInputOrDebugPosOffset),
      Smi::FromInt(bytecode_offset()));
}

void GeneratorRestoreRegister::SetValueLocationConstraints() {
  UseRegister(array_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void GeneratorRestoreRegister::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  Register array = ToRegister(array_input());
  Register result_reg = ToRegister(result());
  Register temp = general_temporaries().PopFirst();

  // The input and the output can alias, if that happen we use a temporary
  // register and a move at the end.
  Register value = (array == result_reg ? temp : result_reg);

  // Loads the current value in the generator register file.
  __ DecompressAnyTagged(
      value, FieldOperand(array, FixedArray::OffsetOfElementAt(index())));

  // And trashs it with StaleRegisterConstant.
  __ LoadRoot(kScratchRegister, RootIndex::kStaleRegister);
  __ StoreTaggedField(
      FieldOperand(array, FixedArray::OffsetOfElementAt(index())),
      kScratchRegister);

  if (value != result_reg) {
    __ Move(result_reg, value);
  }
}

int CreateEmptyObjectLiteral::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void CreateEmptyObjectLiteral::SetValueLocationConstraints() {
  DefineAsRegister(this);
}
void CreateEmptyObjectLiteral::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  Register object = ToRegister(result());
  RegisterSnapshot save_registers = register_snapshot();
  __ Allocate(save_registers, object, map().instance_size());
  __ Move(kScratchRegister, map().object());
  __ StoreTaggedField(FieldOperand(object, HeapObject::kMapOffset),
                      kScratchRegister);
  __ LoadRoot(kScratchRegister, RootIndex::kEmptyFixedArray);
  __ StoreTaggedField(FieldOperand(object, JSObject::kPropertiesOrHashOffset),
                      kScratchRegister);
  __ StoreTaggedField(FieldOperand(object, JSObject::kElementsOffset),
                      kScratchRegister);
  __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
  for (int i = 0; i < map().GetInObjectProperties(); i++) {
    int offset = map().GetInObjectPropertyOffset(i);
    __ StoreTaggedField(FieldOperand(object, offset), kScratchRegister);
  }
}

void AssertInt32::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void AssertInt32::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  __ cmpq(ToRegister(left_input()), ToRegister(right_input()));
  __ Check(ToCondition(condition_), reason_);
}

bool AnyMapIsHeapNumber(const ZoneHandleSet<Map>& maps) {
  return std::any_of(maps.begin(), maps.end(),
                     [](Handle<Map> map) { return map->IsHeapNumberMap(); });
}

void CheckMaps::SetValueLocationConstraints() { UseRegister(receiver_input()); }
void CheckMaps::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  Register object = ToRegister(receiver_input());

  // TODO(victorgomes): This can happen, because we do not emit an unconditional
  // deopt when we intersect the map sets.
  if (maps().is_empty()) {
    __ RegisterEagerDeopt(eager_deopt_info(), DeoptimizeReason::kWrongMap);
    __ jmp(eager_deopt_info()->deopt_entry_label());
    return;
  }

  bool maps_include_heap_number = AnyMapIsHeapNumber(maps());

  Label done;
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    if (maps_include_heap_number) {
      // Smis count as matching the HeapNumber map, so we're done.
      __ jmp(&done);
    } else {
      __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
    }
  }

  size_t map_count = maps().size();
  for (size_t i = 0; i < map_count - 1; ++i) {
    Handle<Map> map = maps().at(i);
    __ Cmp(FieldOperand(object, HeapObject::kMapOffset), map);
    __ j(equal, &done);
  }
  Handle<Map> last_map = maps().at(map_count - 1);
  __ Cmp(FieldOperand(object, HeapObject::kMapOffset), last_map);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongMap, this);
  __ bind(&done);
}

void CheckValue::SetValueLocationConstraints() { UseRegister(target_input()); }
void CheckValue::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register target = ToRegister(target_input());

  __ Cmp(target, value().object());
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongValue, this);
}

void CheckDynamicValue::SetValueLocationConstraints() {
  UseRegister(first_input());
  UseRegister(second_input());
}
void CheckDynamicValue::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register first = ToRegister(first_input());
  Register second = ToRegister(second_input());

  __ cmpl(first, second);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongValue, this);
}

void CheckSmi::SetValueLocationConstraints() { UseRegister(receiver_input()); }
void CheckSmi::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Condition is_smi = __ CheckSmi(object);
  __ EmitEagerDeoptIf(NegateCondition(is_smi), DeoptimizeReason::kNotASmi,
                      this);
}

void CheckNumber::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckNumber::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Label done;
  Register value = ToRegister(receiver_input());
  // If {value} is a Smi or a HeapNumber, we're done.
  __ JumpIfSmi(value, &done);
  __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                 RootIndex::kHeapNumberMap);
  if (mode() == Object::Conversion::kToNumeric) {
    // Jump to done if it is a HeapNumber.
    __ j(equal, &done);
    // Check if it is a BigInt.
    __ LoadMap(kScratchRegister, value);
    __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
            Immediate(BIGINT_TYPE));
  }
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotANumber, this);
  __ bind(&done);
}

void CheckHeapObject::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckHeapObject::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Condition is_smi = __ CheckSmi(object);
  __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kSmi, this);
}

void CheckSymbol::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckSymbol::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kNotASymbol, this);
  }
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceType(kScratchRegister, SYMBOL_TYPE);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotASymbol, this);
}

void CheckInstanceType::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckInstanceType::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongInstanceType, this);
  }
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceType(kScratchRegister, instance_type());
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongInstanceType, this);
}

void CheckString::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckString::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kNotAString, this);
  }
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceTypeRange(kScratchRegister, kScratchRegister, FIRST_STRING_TYPE,
                          LAST_STRING_TYPE);
  __ EmitEagerDeoptIf(above, DeoptimizeReason::kNotAString, this);
}

int CheckMapsWithMigration::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kTryMigrateInstance)->nargs, 1);
  return 1;
}
void CheckMapsWithMigration::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckMapsWithMigration::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  __ RegisterEagerDeopt(eager_deopt_info(), DeoptimizeReason::kWrongMap);

  // TODO(victorgomes): This can happen, because we do not emit an unconditional
  // deopt when we intersect the map sets.
  if (maps().is_empty()) {
    __ jmp(eager_deopt_info()->deopt_entry_label());
    return;
  }

  Register object = ToRegister(receiver_input());

  bool maps_include_heap_number = AnyMapIsHeapNumber(maps());

  ZoneLabelRef done(masm);
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    if (maps_include_heap_number) {
      // Smis count as matching the HeapNumber map, so we're done.
      __ jmp(*done);
    } else {
      __ j(is_smi, eager_deopt_info()->deopt_entry_label());
    }
  }

  size_t map_count = maps().size();
  for (size_t i = 0; i < map_count; ++i) {
    ZoneLabelRef continue_label(masm);
    Handle<Map> map = maps().at(i);
    __ Cmp(FieldOperand(object, HeapObject::kMapOffset), map);

    bool last_map = (i == map_count - 1);
    if (map->is_migration_target()) {
      __ JumpToDeferredIf(
          not_equal,
          [](MaglevAssembler* masm, ZoneLabelRef continue_label,
             ZoneLabelRef done, Register object, int map_index,
             CheckMapsWithMigration* node) {
            // Reload the map to avoid needing to save it on a temporary in the
            // fast path.
            __ LoadMap(kScratchRegister, object);
            // If the map is not deprecated, we fail the map check, continue to
            // the next one.
            __ movl(kScratchRegister,
                    FieldOperand(kScratchRegister, Map::kBitField3Offset));
            __ testl(kScratchRegister,
                     Immediate(Map::Bits3::IsDeprecatedBit::kMask));
            __ j(zero, *continue_label);

            // Otherwise, try migrating the object. If the migration
            // returns Smi zero, then it failed the migration.
            Register return_val = Register::no_reg();
            {
              RegisterSnapshot register_snapshot = node->register_snapshot();
              // We can eager deopt after the snapshot, so make sure the nodes
              // used by the deopt are included in it.
              // TODO(leszeks): This is a bit of a footgun -- we likely want the
              // snapshot to always include eager deopt input registers.
              AddDeoptRegistersToSnapshot(&register_snapshot,
                                          node->eager_deopt_info());
              SaveRegisterStateForCall save_register_state(masm,
                                                           register_snapshot);

              __ Push(object);
              __ Move(kContextRegister, masm->native_context().object());
              __ CallRuntime(Runtime::kTryMigrateInstance);
              save_register_state.DefineSafepoint();

              // Make sure the return value is preserved across the live
              // register restoring pop all.
              return_val = kReturnRegister0;
              if (register_snapshot.live_registers.has(return_val)) {
                DCHECK(!register_snapshot.live_registers.has(kScratchRegister));
                __ movq(kScratchRegister, return_val);
                return_val = kScratchRegister;
              }
            }

            // On failure, the returned value is zero
            __ cmpl(return_val, Immediate(0));
            __ j(equal, *continue_label);

            // The migrated object is returned on success, retry the map check.
            __ Move(object, return_val);
            // Manually load the map pointer without uncompressing it.
            __ Cmp(FieldOperand(object, HeapObject::kMapOffset),
                   node->maps().at(map_index));
            __ j(equal, *done);
            __ jmp(*continue_label);
          },
          // If this is the last map to check, we should deopt if we fail.
          // This is safe to do, since {eager_deopt_info} is ZoneAllocated.
          (last_map ? ZoneLabelRef::UnsafeFromLabelPointer(
                          eager_deopt_info()->deopt_entry_label())
                    : continue_label),
          done, object, i, this);
    } else if (last_map) {
      // If it is the last map and it is not a migration target, we should deopt
      // if the check fails.
      __ j(not_equal, eager_deopt_info()->deopt_entry_label());
    }

    if (!last_map) {
      // We don't need to bind the label for the last map.
      __ j(equal, *done);
      __ bind(*continue_label);
    }
  }

  __ bind(*done);
}

void CheckJSArrayBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
}
void CheckJSArrayBounds::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  __ AssertNotSmi(object);

  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  __ SmiUntagField(kScratchRegister,
                   FieldOperand(object, JSArray::kLengthOffset));
  __ cmpl(index, kScratchRegister);
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

namespace {
int ElementsKindSize(ElementsKind element_kind) {
  switch (element_kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                           \
    DCHECK_LE(sizeof(ctype), 8);                  \
    return sizeof(ctype);
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
#undef TYPED_ARRAY_CASE
  }
}

int ExternalArrayElementSize(const ExternalArrayType element_type) {
  switch (element_type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                    \
    DCHECK_LE(sizeof(ctype), 8);                  \
    return sizeof(ctype);
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
#undef TYPED_ARRAY_CASE
  }
}
}  // namespace

void CheckJSTypedArrayBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  if (ElementsKindSize(elements_kind_) == 1) {
    UseRegister(index_input());
  } else {
    UseAndClobberRegister(index_input());
  }
}
void CheckJSTypedArrayBounds::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  Register byte_length = kScratchRegister;
  if (v8_flags.debug_code) {
    __ AssertNotSmi(object);
    __ CmpObjectType(object, JS_TYPED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  __ LoadBoundedSizeFromObject(byte_length, object,
                               JSTypedArray::kRawByteLengthOffset);
  int element_size = ElementsKindSize(elements_kind_);
  if (element_size > 1) {
    DCHECK(element_size == 2 || element_size == 4);
    __ shlq(index, Immediate(element_size / 2));
  }
  __ cmpq(index, byte_length);
  // We use {above_equal} which does an unsigned comparison to handle negative
  // indices as well.
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

void CheckJSDataViewBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
}
void CheckJSDataViewBounds::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  Register byte_length = kScratchRegister;
  if (v8_flags.debug_code) {
    __ AssertNotSmi(object);
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  __ LoadBoundedSizeFromObject(byte_length, object,
                               JSDataView::kRawByteLengthOffset);
  int element_size = ExternalArrayElementSize(element_type_);
  if (element_size > 1) {
    __ subq(byte_length, Immediate(element_size - 1));
    __ EmitEagerDeoptIf(negative, DeoptimizeReason::kOutOfBounds, this);
  }
  __ cmpl(index, byte_length);
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

void CheckJSObjectElementsBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
}
void CheckJSObjectElementsBounds::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  __ AssertNotSmi(object);

  if (v8_flags.debug_code) {
    __ CmpObjectType(object, FIRST_JS_OBJECT_TYPE, kScratchRegister);
    __ Assert(greater_equal, AbortReason::kUnexpectedValue);
  }
  __ LoadAnyTaggedField(kScratchRegister,
                        FieldOperand(object, JSObject::kElementsOffset));
  if (v8_flags.debug_code) {
    __ AssertNotSmi(kScratchRegister);
  }
  __ SmiUntagField(kScratchRegister,
                   FieldOperand(kScratchRegister, FixedArray::kLengthOffset));
  __ cmpl(index, kScratchRegister);
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

void CheckedInternalizedString::SetValueLocationConstraints() {
  UseRegister(object_input());
  set_temporaries_needed(1);
  DefineSameAsFirst(this);
}
void CheckedInternalizedString::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register object = ToRegister(object_input());
  RegList temps = general_temporaries();
  Register map_tmp = temps.PopFirst();

  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
  }

  __ LoadMap(map_tmp, object);
  __ RecordComment("Test IsInternalizedString");
  // Go to the slow path if this is a non-string, or a non-internalised string.
  __ testw(FieldOperand(map_tmp, Map::kInstanceTypeOffset),
           Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  static_assert((kStringTag | kInternalizedTag) == 0);
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      not_zero,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object,
         CheckedInternalizedString* node, EagerDeoptInfo* deopt_info,
         Register map_tmp) {
        __ RecordComment("Deferred Test IsThinString");
        __ movw(map_tmp, FieldOperand(map_tmp, Map::kInstanceTypeOffset));
        static_assert(kThinStringTagBit > 0);
        // Deopt if this isn't a string.
        __ testw(map_tmp, Immediate(kIsNotStringMask));
        __ j(not_zero, deopt_info->deopt_entry_label());
        // Deopt if this isn't a thin string.
        __ testb(map_tmp, Immediate(kThinStringTagBit));
        __ j(zero, deopt_info->deopt_entry_label());
        __ LoadTaggedPointerField(
            object, FieldOperand(object, ThinString::kActualOffset));
        if (v8_flags.debug_code) {
          __ RecordComment("DCHECK IsInternalizedString");
          __ LoadMap(map_tmp, object);
          __ testw(FieldOperand(map_tmp, Map::kInstanceTypeOffset),
                   Immediate(kIsNotStringMask | kIsNotInternalizedMask));
          static_assert((kStringTag | kInternalizedTag) == 0);
          __ Check(zero, AbortReason::kUnexpectedValue);
        }
        __ jmp(*done);
      },
      done, object, this, eager_deopt_info(), map_tmp);
  __ bind(*done);
}

int CheckedObjectToIndex::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(1);
}
void CheckedObjectToIndex::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
  set_double_temporaries_needed(1);
}
void CheckedObjectToIndex::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register result_reg = ToRegister(result());

  ZoneLabelRef done(masm);
  Condition is_smi = __ CheckSmi(object);
  __ JumpToDeferredIf(
      NegateCondition(is_smi),
      [](MaglevAssembler* masm, Register object, Register result_reg,
         ZoneLabelRef done, CheckedObjectToIndex* node) {
        Label is_string;
        __ LoadMap(kScratchRegister, object);
        __ CmpInstanceTypeRange(kScratchRegister, kScratchRegister,
                                FIRST_STRING_TYPE, LAST_STRING_TYPE);
        __ j(below_equal, &is_string);

        __ cmpl(kScratchRegister, Immediate(HEAP_NUMBER_TYPE));
        // The IC will go generic if it encounters something other than a
        // Number or String key.
        __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, node);

        // Heap Number.
        {
          DoubleRegister number_value = node->double_temporaries().first();
          DoubleRegister converted_back = kScratchDoubleReg;
          // Convert the input float64 value to int32.
          __ Cvttsd2si(result_reg, number_value);
          // Convert that int32 value back to float64.
          __ Cvtlsi2sd(converted_back, result_reg);
          // Check that the result of the float64->int32->float64 is equal to
          // the input (i.e. that the conversion didn't truncate.
          __ Ucomisd(number_value, converted_back);
          __ j(equal, *done);
          __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
        }

        // String.
        __ bind(&is_string);
        {
          RegisterSnapshot snapshot = node->register_snapshot();
          snapshot.live_registers.clear(result_reg);
          DCHECK(!snapshot.live_tagged_registers.has(result_reg));
          {
            SaveRegisterStateForCall save_register_state(masm, snapshot);
            AllowExternalCallThatCantCauseGC scope(masm);
            __ PrepareCallCFunction(1);
            __ Move(arg_reg_1, object);
            __ CallCFunction(
                ExternalReference::string_to_array_index_function(), 1);
            // No need for safepoint since this is a fast C call.
            __ Move(result_reg, kReturnRegister0);
          }
          __ cmpl(result_reg, Immediate(0));
          __ j(greater_equal, *done);
          __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
        }
      },
      object, result_reg, done, this);

  // If we didn't enter the deferred block, we're a Smi.
  if (result_reg == object) {
    __ SmiUntag(object);
  } else {
    __ SmiUntag(result_reg, object);
  }

  __ bind(*done);
}

int BuiltinStringFromCharCode::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void BuiltinStringFromCharCode::SetValueLocationConstraints() {
  if (code_input().node()->Is<Int32Constant>()) {
    UseAny(code_input());
  } else {
    UseAndClobberRegister(code_input());
    set_temporaries_needed(1);
  }
  DefineAsRegister(this);
}
void BuiltinStringFromCharCode::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register result_string = ToRegister(result());
  if (Int32Constant* constant = code_input().node()->TryCast<Int32Constant>()) {
    int32_t char_code = constant->value();
    if (0 <= char_code && char_code < String::kMaxOneByteCharCode) {
      __ LoadSingleCharacterString(result_string, char_code);
    } else {
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ movw(FieldOperand(result_string, SeqTwoByteString::kHeaderSize),
              Immediate(char_code & 0xFFFF));
    }
  } else {
    Register char_code = ToRegister(code_input());
    // We only need a scratch here if {char_code} alias with {result}.
    // TODO(victorgomes): Add a constraint in the register allocator for this
    // use case?
    Register scratch = general_temporaries().PopFirst();
    __ StringFromCharCode(register_snapshot(), nullptr, result_string,
                          char_code, scratch);
  }
}

int BuiltinStringPrototypeCharCodeAt::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kStringCharCodeAt)->nargs, 2);
  return 2;
}
void BuiltinStringPrototypeCharCodeAt::SetValueLocationConstraints() {
  UseAndClobberRegister(string_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}

void BuiltinStringPrototypeCharCodeAt::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register string = ToRegister(string_input());
  Register index = ToRegister(index_input());
  Register scratch = general_temporaries().PopFirst();
  ZoneLabelRef done(masm);
  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeAt(save_registers, ToRegister(result()), string, index,
                      scratch, *done);
  __ bind(*done);
}

void LoadDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void LoadDoubleField::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register tmp = general_temporaries().PopFirst();
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  __ DecompressAnyTagged(tmp, FieldOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Movsd(ToDoubleRegister(result()),
           FieldOperand(tmp, HeapNumber::kValueOffset));
}

void LoadTaggedElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadTaggedElement::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_OBJECT_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }
  __ DecompressAnyTagged(kScratchRegister,
                         FieldOperand(object, JSObject::kElementsOffset));
  if (v8_flags.debug_code) {
    __ CmpObjectType(kScratchRegister, FIXED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
    // Reload since CmpObjectType clobbered the scratch register.
    __ DecompressAnyTagged(kScratchRegister,
                           FieldOperand(object, JSObject::kElementsOffset));
  }
  __ DecompressAnyTagged(
      result_reg, FieldOperand(kScratchRegister, index, times_tagged_size,
                               FixedArray::kHeaderSize));
}

void LoadDoubleElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadDoubleElement::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister result_reg = ToDoubleRegister(result());
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_OBJECT_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }
  __ DecompressAnyTagged(kScratchRegister,
                         FieldOperand(object, JSObject::kElementsOffset));
  if (v8_flags.debug_code) {
    __ CmpObjectType(kScratchRegister, FIXED_DOUBLE_ARRAY_TYPE,
                     kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
    // Reload since CmpObjectType clobbered the scratch register.
    __ DecompressAnyTagged(kScratchRegister,
                           FieldOperand(object, JSObject::kElementsOffset));
  }
  __ Movsd(result_reg, FieldOperand(kScratchRegister, index, times_8,
                                    FixedDoubleArray::kHeaderSize));
}

namespace {
bool FromConstantToBool(MaglevAssembler* masm, ValueNode* node) {
  DCHECK(IsConstantNode(node->opcode()));
  LocalIsolate* local_isolate = masm->isolate()->AsLocalIsolate();
  switch (node->opcode()) {
#define CASE(Name)                                       \
  case Opcode::k##Name: {                                \
    return node->Cast<Name>()->ToBoolean(local_isolate); \
  }
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}
}  // namespace

void LoadSignedIntDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (is_little_endian_constant() ||
      type_ == ExternalArrayType::kExternalInt8Array) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void LoadSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());
  Register data_pointer = general_temporaries().PopFirst();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSDataView::kDataPointerOffset));

  int element_size = ExternalArrayElementSize(type_);
  __ LoadSignedField(result_reg, Operand(data_pointer, index, times_1, 0),
                     element_size);

  // We ignore little endian argument if type is a byte size.
  if (type_ != ExternalArrayType::kExternalInt8Array) {
    if (is_little_endian_constant()) {
      if (!FromConstantToBool(masm, is_little_endian_input().node())) {
        __ ReverseByteOrder(result_reg, element_size);
      }
    } else {
      ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
      __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                   is_big_endian, false);
      __ bind(*is_big_endian);
      __ ReverseByteOrder(result_reg, element_size);
      __ bind(*is_little_endian);
      // x64 is little endian.
      static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    }
  }
}

void StoreSignedIntDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (ExternalArrayElementSize(type_) > 1) {
    UseAndClobberRegister(value_input());
  } else {
    UseRegister(value_input());
  }
  if (is_little_endian_constant() ||
      type_ == ExternalArrayType::kExternalInt8Array) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
}
void StoreSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register value = ToRegister(value_input());
  Register data_pointer = general_temporaries().PopFirst();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSDataView::kDataPointerOffset));

  int element_size = ExternalArrayElementSize(type_);

  // We ignore little endian argument if type is a byte size.
  if (element_size > 1) {
    if (is_little_endian_constant()) {
      if (!FromConstantToBool(masm, is_little_endian_input().node())) {
        __ ReverseByteOrder(value, element_size);
      }
    } else {
      ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
      __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                   is_big_endian, false);
      __ bind(*is_big_endian);
      __ ReverseByteOrder(value, element_size);
      __ bind(*is_little_endian);
      // x64 is little endian.
      static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    }
  }

  __ StoreField(Operand(data_pointer, index, times_1, 0), value, element_size);
}

void LoadDoubleDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (is_little_endian_constant()) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void LoadDoubleDataViewElement::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister result_reg = ToDoubleRegister(result());
  Register data_pointer = general_temporaries().PopFirst();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSDataView::kDataPointerOffset));

  if (is_little_endian_constant()) {
    if (FromConstantToBool(masm, is_little_endian_input().node())) {
      __ Movsd(result_reg, Operand(data_pointer, index, times_1, 0));
    } else {
      __ movq(kScratchRegister, Operand(data_pointer, index, times_1, 0));
      __ bswapq(kScratchRegister);
      __ Movq(result_reg, kScratchRegister);
    }
  } else {
    Label done;
    ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
    // TODO(leszeks): We're likely to be calling this on an existing boolean --
    // maybe that's a case we should fast-path here and re-use that boolean
    // value?
    __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                 is_big_endian, true);
    // x64 is little endian.
    static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    __ bind(*is_little_endian);
    __ Movsd(result_reg, Operand(data_pointer, index, times_1, 0));
    __ jmp(&done);
    // We should swap the bytes if big endian.
    __ bind(*is_big_endian);
    __ movq(kScratchRegister, Operand(data_pointer, index, times_1, 0));
    __ bswapq(kScratchRegister);
    __ Movq(result_reg, kScratchRegister);
    __ bind(&done);
  }
}

void StoreDoubleDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  UseRegister(value_input());
  if (is_little_endian_constant()) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
}
void StoreDoubleDataViewElement::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister value = ToDoubleRegister(value_input());
  Register data_pointer = general_temporaries().PopFirst();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSDataView::kDataPointerOffset));

  if (is_little_endian_constant()) {
    if (FromConstantToBool(masm, is_little_endian_input().node())) {
      __ Movsd(Operand(data_pointer, index, times_1, 0), value);
    } else {
      __ Movq(kScratchRegister, value);
      __ bswapq(kScratchRegister);
      __ movq(Operand(data_pointer, index, times_1, 0), kScratchRegister);
    }
  } else {
    Label done;
    ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
    // TODO(leszeks): We're likely to be calling this on an existing boolean --
    // maybe that's a case we should fast-path here and re-use that boolean
    // value?
    __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                 is_big_endian, true);
    // x64 is little endian.
    static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    __ bind(*is_little_endian);
    __ Movsd(Operand(data_pointer, index, times_1, 0), value);
    __ jmp(&done);
    // We should swap the bytes if big endian.
    __ bind(*is_big_endian);
    __ Movq(kScratchRegister, value);
    __ bswapq(kScratchRegister);
    __ movq(Operand(data_pointer, index, times_1, 0), kScratchRegister);
    __ bind(&done);
  }
}

namespace {

ScaleFactor ScaleFactorFromInt(int n) {
  switch (n) {
    case 1:
      return times_1;
    case 2:
      return times_2;
    case 4:
      return times_4;
    default:
      UNREACHABLE();
  }
}

}  // namespace

void LoadSignedIntTypedArrayElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadSignedIntTypedArrayElement::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());
  Register data_pointer = result_reg;
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_TYPED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  int element_size = ElementsKindSize(elements_kind_);
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSTypedArray::kExternalPointerOffset));
  __ LoadSignedField(
      result_reg,
      Operand(data_pointer, index, ScaleFactorFromInt(element_size), 0),
      element_size);
}

void LoadUnsignedIntTypedArrayElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadUnsignedIntTypedArrayElement::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());
  Register data_pointer = result_reg;
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_TYPED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  int element_size = ElementsKindSize(elements_kind_);
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSTypedArray::kExternalPointerOffset));
  __ LoadUnsignedField(
      result_reg,
      Operand(data_pointer, index, ScaleFactorFromInt(element_size), 0),
      element_size);
}

void LoadDoubleTypedArrayElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadDoubleTypedArrayElement::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister result_reg = ToDoubleRegister(result());
  Register data_pointer = kScratchRegister;
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_TYPED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSTypedArray::kExternalPointerOffset));
  switch (elements_kind_) {
    case FLOAT32_ELEMENTS:
      __ Movss(result_reg, Operand(data_pointer, index, times_4, 0));
      __ Cvtss2sd(result_reg, result_reg);
      break;
    case FLOAT64_ELEMENTS:
      __ Movsd(result_reg, Operand(data_pointer, index, times_8, 0));
      break;
    default:
      UNREACHABLE();
  }
}

void StoreDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(value_input());
  set_temporaries_needed(1);
}
void StoreDoubleField::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register tmp = general_temporaries().PopFirst();
  Register object = ToRegister(object_input());
  DoubleRegister value = ToDoubleRegister(value_input());

  __ AssertNotSmi(object);
  __ DecompressAnyTagged(tmp, FieldOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Movsd(FieldOperand(tmp, HeapNumber::kValueOffset), value);
}

void StoreTaggedFieldNoWriteBarrier::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(value_input());
}
void StoreTaggedFieldNoWriteBarrier::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register value = ToRegister(value_input());

  __ AssertNotSmi(object);
  __ StoreTaggedField(FieldOperand(object, offset()), value);
}

int StoreMap::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void StoreMap::SetValueLocationConstraints() {
  UseFixed(object_input(), WriteBarrierDescriptor::ObjectRegister());
}
void StoreMap::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));

  __ AssertNotSmi(object);
  Register value = kScratchRegister;
  __ Move(value, map_.object());
  __ StoreTaggedField(FieldOperand(object, HeapObject::kMapOffset),
                      kScratchRegister);

  ZoneLabelRef done(masm);
  DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
         Register object, StoreMap* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        __ CheckPageFlag(
            value, kScratchRegister,
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, zero,
            *done);

        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        RegList saved;
        if (node->register_snapshot().live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);
        __ leaq(slot_reg, FieldOperand(object, HeapObject::kMapOffset));

        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ jmp(*done);
      },
      done, value, object, this);

  __ JumpIfSmi(value, *done);
  __ CheckPageFlag(object, kScratchRegister,
                   MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                   &deferred_write_barrier->deferred_code_label);
  __ bind(*done);
}

int StoreTaggedFieldWithWriteBarrier::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void StoreTaggedFieldWithWriteBarrier::SetValueLocationConstraints() {
  UseFixed(object_input(), WriteBarrierDescriptor::ObjectRegister());
  UseRegister(value_input());
}
void StoreTaggedFieldWithWriteBarrier::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));

  Register value = ToRegister(value_input());

  __ AssertNotSmi(object);
  __ StoreTaggedField(FieldOperand(object, offset()), value);

  ZoneLabelRef done(masm);
  DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
         Register object, StoreTaggedFieldWithWriteBarrier* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        __ CheckPageFlag(
            value, kScratchRegister,
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, zero,
            *done);

        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        RegList saved;
        if (node->register_snapshot().live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);
        __ leaq(slot_reg, FieldOperand(object, node->offset()));

        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ jmp(*done);
      },
      done, value, object, this);

  __ JumpIfSmi(value, *done);
  __ CheckPageFlag(object, kScratchRegister,
                   MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                   &deferred_write_barrier->deferred_code_label);
  __ bind(*done);
}

void StringLength::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void StringLength::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register object = ToRegister(object_input());
  if (v8_flags.debug_code) {
    // Use return register as temporary. Push it in case it aliases the object
    // register.
    Register tmp = ToRegister(result());
    __ Push(tmp);
    // Check if {object} is a string.
    __ AssertNotSmi(object);
    __ LoadMap(tmp, object);
    __ CmpInstanceTypeRange(tmp, tmp, FIRST_STRING_TYPE, LAST_STRING_TYPE);
    __ Check(below_equal, AbortReason::kUnexpectedValue);
    __ Pop(tmp);
  }
  __ movl(ToRegister(result()), FieldOperand(object, String::kLengthOffset));
}

int StringAt::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kStringCharCodeAt)->nargs, 2);
  return std::max(2, AllocateDescriptor::GetStackParameterCount());
}
void StringAt::SetValueLocationConstraints() {
  UseAndClobberRegister(string_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void StringAt::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  Register result_string = ToRegister(result());
  Register string = ToRegister(string_input());
  Register index = ToRegister(index_input());
  Register scratch = general_temporaries().PopFirst();
  Register char_code = string;

  ZoneLabelRef done(masm);
  Label cached_one_byte_string;

  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeAt(save_registers, char_code, string, index, scratch,
                      &cached_one_byte_string);
  __ StringFromCharCode(save_registers, &cached_one_byte_string, result_string,
                        char_code, scratch);
}

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ addl(left, right);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{left} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ subl(left, right);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{left} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
  set_temporaries_needed(1);
}

void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register result = ToRegister(this->result());
  Register right = ToRegister(right_input());
  DCHECK_EQ(result, ToRegister(left_input()));

  Register saved_left = general_temporaries().first();
  __ movl(saved_left, result);
  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ imull(result, right);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{saved_left, result} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ cmpl(result, Immediate(0));
  __ j(not_zero, &end);
  {
    __ orl(saved_left, right);
    __ cmpl(saved_left, Immediate(0));
    // If one of them is negative, we must have a -0 result, which is non-int32,
    // so deopt.
    // TODO(leszeks): Consider splitting these deopts to have distinct deopt
    // reasons. Otherwise, the reason has to match the above.
    __ EmitEagerDeoptIf(less, DeoptimizeReason::kOverflow, this);
  }
  __ bind(&end);
}

void Int32ModulusWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsFixed(this, rdx);
  // rax,rdx are clobbered by div.
  RequireSpecificTemporary(rax);
  RequireSpecificTemporary(rdx);
}

void Int32ModulusWithOverflow::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  // Using same algorithm as in EffectControlLinearizer:
  //   if rhs <= 0 then
  //     rhs = -rhs
  //     deopt if rhs == 0
  //   if lhs < 0 then
  //     let lhs_abs = -lsh in
  //     let res = lhs_abs % rhs in
  //     deopt if res == 0
  //     -res
  //   else
  //     let msk = rhs - 1 in
  //     if rhs & msk == 0 then
  //       lhs & msk
  //     else
  //       lhs % rhs

  DCHECK(general_temporaries().has(rax));
  DCHECK(general_temporaries().has(rdx));
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);

  __ cmpl(right, Immediate(0));
  __ JumpToDeferredIf(
      less_equal,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register right,
         Int32ModulusWithOverflow* node) {
        __ negl(right);
        __ EmitEagerDeoptIf(zero, DeoptimizeReason::kDivisionByZero, node);
        __ jmp(*rhs_checked);
      },
      rhs_checked, right, this);
  __ bind(*rhs_checked);

  __ cmpl(left, Immediate(0));
  __ JumpToDeferredIf(
      less,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register left,
         Register right, Int32ModulusWithOverflow* node) {
        __ negl(left);
        __ movl(rax, left);
        __ xorl(rdx, rdx);
        __ divl(right);
        __ testl(rdx, rdx);
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
        // allows one deopt reason per IR.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kDivisionByZero, node);
        __ negl(rdx);
        __ jmp(*done);
      },
      done, left, right, this);

  Label right_not_power_of_2;
  Register mask = rax;
  __ leal(mask, Operand(right, -1));
  __ testl(right, mask);
  __ j(not_zero, &right_not_power_of_2, Label::kNear);

  // {right} is power of 2.
  __ andl(mask, left);
  __ movl(ToRegister(result()), mask);
  __ jmp(*done, Label::kNear);

  __ bind(&right_not_power_of_2);
  __ movl(rax, left);
  __ xorl(rdx, rdx);
  __ divl(right);
  // Result is implicitly written to rdx.
  DCHECK_EQ(ToRegister(result()), rdx);

  __ bind(*done);
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsFixed(this, rax);
  // rax,rdx are clobbered by idiv.
  RequireSpecificTemporary(rax);
  RequireSpecificTemporary(rdx);
}

void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  DCHECK(general_temporaries().has(rax));
  DCHECK(general_temporaries().has(rdx));
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ movl(rax, left);

  // TODO(leszeks): peephole optimise division by a constant.

  // Sign extend eax into edx.
  __ cdq();

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ cmpl(right, Immediate(0));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      less_equal,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register right,
         Int32DivideWithOverflow* node) {
        // {right} is negative or zero.

        // Check if {right} is zero.
        // We've already done the compare and flags won't be cleared yet.
        // TODO(leszeks): Using kNotInt32 here, but kDivisionByZero would be
        // better. Right now all eager deopts in a node have to be the same --
        // we should allow a node to emit multiple eager deopts with different
        // reasons.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is zero, as that would produce minus zero. Left is in
        // rax already.
        __ cmpl(rax, Immediate(0));
        // TODO(leszeks): Better DeoptimizeReason = kMinusZero.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ cmpl(rax, Immediate(kMinInt));
        __ j(not_equal, *done);
        __ cmpl(right, Immediate(-1));
        __ j(not_equal, *done);
        // TODO(leszeks): Better DeoptimizeReason = kOverflow, but
        // eager_deopt_info is already configured as kNotInt32.
        __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
      },
      done, right, this);
  __ bind(*done);

  // Perform the actual integer division.
  __ idivl(right);

  // Check that the remainder is zero.
  __ cmpl(rdx, Immediate(0));
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{rax, rdx} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, this);
  DCHECK_EQ(ToRegister(result()), rax);
}

void Int32BitwiseAnd::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32BitwiseAnd::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ andl(left, right);
}

void Int32BitwiseOr::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32BitwiseOr::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ orl(left, right);
}

void Int32BitwiseXor::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32BitwiseXor::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ xorl(left, right);
}

void Int32ShiftLeft::SetValueLocationConstraints() {
  UseRegister(left_input());
  // Use the "shift by cl" variant of shl.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(this);
}

void Int32ShiftLeft::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ shll_cl(left);
}

void Int32ShiftRight::SetValueLocationConstraints() {
  UseRegister(left_input());
  // Use the "shift by cl" variant of sar.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(this);
}

void Int32ShiftRight::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ sarl_cl(left);
}

void Int32ShiftRightLogical::SetValueLocationConstraints() {
  UseRegister(left_input());
  // Use the "shift by cl" variant of shr.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(this);
}

void Int32ShiftRightLogical::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ shrl_cl(left);
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineSameAsFirst(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input());
  __ incl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineSameAsFirst(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input());
  __ decl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineSameAsFirst(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(value_input());
  __ negl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineSameAsFirst(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(value_input());
  __ notl(value);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  Register result = ToRegister(this->result());
  Label is_true, end;
  __ cmpl(left, right);
  // TODO(leszeks): Investigate using cmov here.
  __ j(ConditionFor(kOperation), &is_true);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kFalseValue);
  __ jmp(&end);
  {
    __ bind(&is_true);
    __ LoadRoot(result, RootIndex::kTrueValue);
  }
  __ bind(&end);
}

#define DEF_OPERATION(Name)                               \
  void Name::SetValueLocationConstraints() {              \
    Base::SetValueLocationConstraints();                  \
  }                                                       \
  void Name::GenerateCode(MaglevAssembler* masm,          \
                          const ProcessingState& state) { \
    Base::GenerateCode(masm, state);                      \
  }
DEF_OPERATION(Int32Equal)
DEF_OPERATION(Int32StrictEqual)
DEF_OPERATION(Int32LessThan)
DEF_OPERATION(Int32LessThanOrEqual)
DEF_OPERATION(Int32GreaterThan)
DEF_OPERATION(Int32GreaterThanOrEqual)
#undef DEF_OPERATION

void Float64Add::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Float64Add::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Addsd(left, right);
}

void Float64Subtract::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Float64Subtract::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Subsd(left, right);
}

void Float64Multiply::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Float64Multiply::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Mulsd(left, right);
}

void Float64Divide::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Float64Divide::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Divsd(left, right);
}

void Float64Modulus::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  RequireSpecificTemporary(rax);
  DefineAsRegister(this);
}

void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  // Approach copied from code-generator-x64.cc
  // Allocate space to use fld to move the value to the FPU stack.
  __ AllocateStackSpace(kDoubleSize);
  Operand scratch_stack_space = Operand(rsp, 0);
  __ Movsd(scratch_stack_space, ToDoubleRegister(right_input()));
  __ fld_d(scratch_stack_space);
  __ Movsd(scratch_stack_space, ToDoubleRegister(left_input()));
  __ fld_d(scratch_stack_space);
  // Loop while fprem isn't done.
  Label mod_loop;
  __ bind(&mod_loop);
  // This instructions traps on all kinds inputs, but we are assuming the
  // floating point control word is set to ignore them all.
  __ fprem();
  // The following 2 instruction implicitly use rax.
  __ fnstsw_ax();
  if (CpuFeatures::IsSupported(SAHF)) {
    CpuFeatureScope sahf_scope(masm, SAHF);
    __ sahf();
  } else {
    __ shrl(rax, Immediate(8));
    __ andl(rax, Immediate(0xFF));
    __ pushq(rax);
    __ popfq();
  }
  __ j(parity_even, &mod_loop);
  // Move output to stack and clean up.
  __ fstp(1);
  __ fstp_d(scratch_stack_space);
  __ Movsd(ToDoubleRegister(result()), scratch_stack_space);
  __ addq(rsp, Immediate(kDoubleSize));
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}

void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  __ Negpd(value, value, kScratchRegister);
}

int Float64Exponentiate::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(2);
}
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(left_input(), xmm0);
  UseFixed(right_input(), xmm1);
  DefineSameAsFirst(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(2);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 2);
}

int Float64Ieee754Unary::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(1);
}
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseFixed(input(), xmm0);
  DefineSameAsFirst(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(1);
  __ CallCFunction(ieee_function_, 1);
}

template <class Derived, Operation kOperation>
void Float64CompareNode<Derived, kOperation>::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

template <class Derived, Operation kOperation>
void Float64CompareNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  Register result = ToRegister(this->result());
  Label is_false, end;
  __ Ucomisd(left, right);
  // Ucomisd sets these flags accordingly:
  //   UNORDERED(one of the operands is a NaN): ZF,PF,CF := 111;
  //   GREATER_THAN: ZF,PF,CF := 000;
  //   LESS_THAN: ZF,PF,CF := 001;
  //   EQUAL: ZF,PF,CF := 100;
  // Since ZF can be set by NaN or EQUAL, we check for NaN first.
  __ j(parity_even, &is_false);
  __ j(NegateCondition(ConditionForFloat64(kOperation)), &is_false);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kTrueValue);
  __ jmp(&end);
  {
    __ bind(&is_false);
    __ LoadRoot(result, RootIndex::kFalseValue);
  }
  __ bind(&end);
}

#define DEF_OPERATION(Name)                               \
  void Name::SetValueLocationConstraints() {              \
    Base::SetValueLocationConstraints();                  \
  }                                                       \
  void Name::GenerateCode(MaglevAssembler* masm,          \
                          const ProcessingState& state) { \
    Base::GenerateCode(masm, state);                      \
  }
DEF_OPERATION(Float64Equal)
DEF_OPERATION(Float64StrictEqual)
DEF_OPERATION(Float64LessThan)
DEF_OPERATION(Float64LessThanOrEqual)
DEF_OPERATION(Float64GreaterThan)
DEF_OPERATION(Float64GreaterThanOrEqual)
#undef DEF_OPERATION

void CheckInt32IsSmi::SetValueLocationConstraints() { UseRegister(input()); }
void CheckInt32IsSmi::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  // TODO(leszeks): This basically does a SmiTag and throws the result away.
  // Don't throw the result away if we want to actually use it.
  Register reg = ToRegister(input());
  __ movl(kScratchRegister, reg);
  __ addl(kScratchRegister, kScratchRegister);
  DCHECK_REGLIST_EMPTY(RegList{reg} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kNotASmi, this);
}

void CheckUint32IsSmi::SetValueLocationConstraints() { UseRegister(input()); }
void CheckUint32IsSmi::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register reg = ToRegister(input());
  // Perform an unsigned comparison against Smi::kMaxValue.
  __ cmpl(reg, Immediate(Smi::kMaxValue));
  __ EmitEagerDeoptIf(above, DeoptimizeReason::kNotASmi, this);
}

void CheckedSmiTagInt32::SetValueLocationConstraints() {
  UseAndClobberRegister(input());
  DefineSameAsFirst(this);
}
void CheckedSmiTagInt32::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register reg = ToRegister(input());
  __ addl(reg, reg);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{reg} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void CheckedSmiTagUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedSmiTagUint32::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register reg = ToRegister(input());
  // Perform an unsigned comparison against Smi::kMaxValue.
  __ cmpl(reg, Immediate(Smi::kMaxValue));
  __ EmitEagerDeoptIf(above, DeoptimizeReason::kOverflow, this);
  __ addl(reg, reg);
  __ Assert(no_overflow, AbortReason::kInputDoesNotFitSmi);
}

void UnsafeSmiTag::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void UnsafeSmiTag::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register reg = ToRegister(input());
  if (v8_flags.debug_code) {
    if (input().node()->properties().value_representation() ==
        ValueRepresentation::kUint32) {
      __ cmpl(reg, Immediate(Smi::kMaxValue));
      __ Check(below_equal, AbortReason::kInputDoesNotFitSmi);
    }
  }
  __ addl(reg, reg);
  if (v8_flags.debug_code) {
    __ Check(no_overflow, AbortReason::kInputDoesNotFitSmi);
  }
}

void Int32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Int32ToNumber::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register value = ToRegister(input());
  Register object = ToRegister(result());
  __ movl(kScratchRegister, value);
  __ addl(kScratchRegister, kScratchRegister);
  __ JumpToDeferredIf(
      overflow,
      [](MaglevAssembler* masm, Register object, Register value,
         ZoneLabelRef done, Int32ToNumber* node) {
        DoubleRegister double_value = kScratchDoubleReg;
        __ Cvtlsi2sd(double_value, value);
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ jmp(*done);
      },
      object, value, done, this);
  __ Move(object, kScratchRegister);
  __ bind(*done);
}

void Uint32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void Uint32ToNumber::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register value = ToRegister(input());
  Register object = ToRegister(result());
  __ cmpl(value, Immediate(Smi::kMaxValue));
  __ JumpToDeferredIf(
      above,
      [](MaglevAssembler* masm, Register object, Register value,
         ZoneLabelRef done, Uint32ToNumber* node) {
        DoubleRegister double_value = kScratchDoubleReg;
        __ Cvtlui2sd(double_value, value);
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ jmp(*done);
      },
      object, value, done, this);
  __ addl(value, value);
  DCHECK_EQ(object, value);
  __ bind(*done);
}

void Float64Box::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Box::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  Register object = ToRegister(result());
  __ AllocateHeapNumber(register_snapshot(), object, value);
}

void HoleyFloat64Box::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void HoleyFloat64Box::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  ZoneLabelRef done(masm);
  DoubleRegister value = ToDoubleRegister(input());
  Register object = ToRegister(result());
  __ movq(object, value);
  __ movq(kScratchRegister, kHoleNanInt64);
  __ cmpq(object, kScratchRegister);
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, Register object, ZoneLabelRef done) {
        __ LoadRoot(object, RootIndex::kUndefinedValue);
        __ jmp(*done);
      },
      object, done);
  __ AllocateHeapNumber(register_snapshot(), object, value);
  __ bind(*done);
}

void CheckedFloat64Unbox::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedFloat64Unbox::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register value = ToRegister(input());
  Label is_not_smi, done;
  // Check if Smi.
  __ JumpIfNotSmi(value, &is_not_smi, Label::kNear);
  // If Smi, convert to Float64.
  __ SmiToInt32(value);
  __ Cvtlsi2sd(ToDoubleRegister(result()), value);
  // TODO(v8:7700): Add a constraint to the register allocator to indicate that
  // the value in the input register is "trashed" by this node. Currently we
  // have the invariant that the input register should not be mutated when it is
  // not the same as the output register or the function does not call a
  // builtin. So, we recover the Smi value here.
  __ SmiTag(value);
  __ jmp(&done, Label::kNear);
  __ bind(&is_not_smi);
  // Check if HeapNumber, deopt otherwise.
  __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                 RootIndex::kHeapNumberMap);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotANumber, this);
  __ Movsd(ToDoubleRegister(result()),
           FieldOperand(value, HeapNumber::kValueOffset));
  __ bind(&done);
}

void CheckedTruncateNumberToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedTruncateNumberToInt32::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  Register value = ToRegister(input());
  Register result_reg = ToRegister(result());
  DCHECK_EQ(value, result_reg);
  Label is_not_smi, done;
  // Check if Smi.
  __ JumpIfNotSmi(value, &is_not_smi, Label::kNear);
  // If Smi, convert to Int32.
  __ SmiToInt32(value);
  __ jmp(&done, Label::kNear);
  __ bind(&is_not_smi);
  // Check if HeapNumber, deopt otherwise.
  __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                 RootIndex::kHeapNumberMap);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotANumber, this);
  auto double_value = kScratchDoubleReg;
  __ Movsd(double_value, FieldOperand(value, HeapNumber::kValueOffset));
  __ TruncateDoubleToInt32(result_reg, double_value);
  __ bind(&done);
}

void LogicalNot::SetValueLocationConstraints() {
  UseRegister(value());
  DefineAsRegister(this);
}
void LogicalNot::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());

  if (v8_flags.debug_code) {
    // LogicalNot expects either TrueValue or FalseValue.
    Label next;
    __ CompareRoot(object, RootIndex::kFalseValue);
    __ j(equal, &next);
    __ CompareRoot(object, RootIndex::kTrueValue);
    __ Check(equal, AbortReason::kUnexpectedValue);
    __ bind(&next);
  }

  Label return_false, done;
  __ CompareRoot(object, RootIndex::kTrueValue);
  __ j(equal, &return_false, Label::kNear);
  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ jmp(&done, Label::kNear);

  __ bind(&return_false);
  __ LoadRoot(return_value, RootIndex::kFalseValue);

  __ bind(&done);
}

void SetPendingMessage::SetValueLocationConstraints() {
  UseRegister(value());
  set_temporaries_needed(1);
  DefineAsRegister(this);
}

void SetPendingMessage::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register new_message = ToRegister(value());
  Register return_value = ToRegister(result());

  MemOperand pending_message_operand = __ ExternalReferenceAsOperand(
      ExternalReference::address_of_pending_message(masm->isolate()),
      kScratchRegister);

  if (new_message != return_value) {
    __ Move(return_value, pending_message_operand);
    __ movq(pending_message_operand, new_message);
  } else {
    Register scratch = general_temporaries().PopFirst();
    __ Move(scratch, pending_message_operand);
    __ movq(pending_message_operand, new_message);
    __ Move(return_value, scratch);
  }
}

void TestUndetectable::SetValueLocationConstraints() {
  UseRegister(value());
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void TestUndetectable::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());
  Register scratch = general_temporaries().PopFirst();

  Label return_false, done;
  __ JumpIfSmi(object, &return_false, Label::kNear);
  // For heap objects, check the map's undetectable bit.
  __ LoadMap(scratch, object);
  __ testl(FieldOperand(scratch, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsUndetectableBit::kMask));
  __ j(zero, &return_false, Label::kNear);

  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ jmp(&done, Label::kNear);

  __ bind(&return_false);
  __ LoadRoot(return_value, RootIndex::kFalseValue);

  __ bind(&done);
}

void TestTypeOf::SetValueLocationConstraints() {
  UseRegister(value());
  DefineAsRegister(this);
}
void TestTypeOf::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  using LiteralFlag = interpreter::TestTypeOfFlags::LiteralFlag;
  Register object = ToRegister(value());
  // Use return register as temporary if needed.
  Register tmp = ToRegister(result());
  Label is_true, is_false, done;
  switch (literal_) {
    case LiteralFlag::kNumber:
      __ JumpIfSmi(object, &is_true, Label::kNear);
      __ CompareRoot(FieldOperand(object, HeapObject::kMapOffset),
                     RootIndex::kHeapNumberMap);
      __ j(not_equal, &is_false, Label::kNear);
      break;
    case LiteralFlag::kString:
      __ JumpIfSmi(object, &is_false, Label::kNear);
      __ LoadMap(tmp, object);
      __ cmpw(FieldOperand(tmp, Map::kInstanceTypeOffset),
              Immediate(FIRST_NONSTRING_TYPE));
      __ j(greater_equal, &is_false, Label::kNear);
      break;
    case LiteralFlag::kSymbol:
      __ JumpIfSmi(object, &is_false, Label::kNear);
      __ LoadMap(tmp, object);
      __ cmpw(FieldOperand(tmp, Map::kInstanceTypeOffset),
              Immediate(SYMBOL_TYPE));
      __ j(not_equal, &is_false, Label::kNear);
      break;
    case LiteralFlag::kBoolean:
      __ CompareRoot(object, RootIndex::kTrueValue);
      __ j(equal, &is_true, Label::kNear);
      __ CompareRoot(object, RootIndex::kFalseValue);
      __ j(not_equal, &is_false, Label::kNear);
      break;
    case LiteralFlag::kBigInt:
      __ JumpIfSmi(object, &is_false, Label::kNear);
      __ LoadMap(tmp, object);
      __ cmpw(FieldOperand(tmp, Map::kInstanceTypeOffset),
              Immediate(BIGINT_TYPE));
      __ j(not_equal, &is_false, Label::kNear);
      break;
    case LiteralFlag::kUndefined:
      __ JumpIfSmi(object, &is_false, Label::kNear);
      // Check it has the undetectable bit set and it is not null.
      __ LoadMap(tmp, object);
      __ testl(FieldOperand(tmp, Map::kBitFieldOffset),
               Immediate(Map::Bits1::IsUndetectableBit::kMask));
      __ j(zero, &is_false, Label::kNear);
      __ CompareRoot(object, RootIndex::kNullValue);
      __ j(equal, &is_false, Label::kNear);
      break;
    case LiteralFlag::kFunction:
      __ JumpIfSmi(object, &is_false, Label::kNear);
      // Check if callable bit is set and not undetectable.
      __ LoadMap(tmp, object);
      __ movl(tmp, FieldOperand(tmp, Map::kBitFieldOffset));
      __ andl(tmp, Immediate(Map::Bits1::IsUndetectableBit::kMask |
                             Map::Bits1::IsCallableBit::kMask));
      __ cmpl(tmp, Immediate(Map::Bits1::IsCallableBit::kMask));
      __ j(not_equal, &is_false, Label::kNear);
      break;
    case LiteralFlag::kObject:
      __ JumpIfSmi(object, &is_false, Label::kNear);
      // If the object is null then return true.
      __ CompareRoot(object, RootIndex::kNullValue);
      __ j(equal, &is_true, Label::kNear);
      // Check if the object is a receiver type,
      __ LoadMap(tmp, object);
      __ cmpw(FieldOperand(tmp, Map::kInstanceTypeOffset),
              Immediate(FIRST_JS_RECEIVER_TYPE));
      __ j(less, &is_false, Label::kNear);
      // ... and is not undefined (undetectable) nor callable.
      __ testl(FieldOperand(tmp, Map::kBitFieldOffset),
               Immediate(Map::Bits1::IsUndetectableBit::kMask |
                         Map::Bits1::IsCallableBit::kMask));
      __ j(not_zero, &is_false, Label::kNear);
      break;
    case LiteralFlag::kOther:
      UNREACHABLE();
  }
  __ bind(&is_true);
  __ LoadRoot(ToRegister(result()), RootIndex::kTrueValue);
  __ jmp(&done, Label::kNear);
  __ bind(&is_false);
  __ LoadRoot(ToRegister(result()), RootIndex::kFalseValue);
  __ bind(&done);
}

int ToObject::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  return D::GetStackParameterCount();
}
void ToObject::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kInput));
  DefineAsFixed(this, kReturnRegister0);
}
void ToObject::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kInput));
#endif  // DEBUG
  Register value = ToRegister(value_input());
  Label call_builtin, done;
  // Avoid the builtin call if {value} is a JSReceiver.
  __ JumpIfSmi(value, &call_builtin);
  __ LoadMap(kScratchRegister, value);
  __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
          Immediate(FIRST_JS_RECEIVER_TYPE));
  __ j(greater_equal, &done);
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToObject);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

int ToString::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  return D::GetStackParameterCount();
}
void ToString::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kO));
  DefineAsFixed(this, kReturnRegister0);
}
void ToString::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kO));
#endif  // DEBUG
  Register value = ToRegister(value_input());
  Label call_builtin, done;
  // Avoid the builtin call if {value} is a string.
  __ JumpIfSmi(value, &call_builtin);
  __ LoadMap(kScratchRegister, value);
  __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
          Immediate(FIRST_NONSTRING_TYPE));
  __ j(less, &done);
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToString);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

void CheckedInt32ToUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedInt32ToUint32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(input());
  __ testl(input_reg, input_reg);
  __ EmitEagerDeoptIf(negative, DeoptimizeReason::kNotUint32, this);
}

void CheckedUint32ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedUint32ToInt32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(input());
  // Check if the top bit is set -- if it is, then this is not a valid int32,
  // otherwise it is.
  __ testl(input_reg, Immediate(1 << 31));
  __ EmitEagerDeoptIf(not_zero, DeoptimizeReason::kNotInt32, this);
}

void ChangeInt32ToFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void ChangeInt32ToFloat64::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  __ Cvtlsi2sd(ToDoubleRegister(result()), ToRegister(input()));
}

void ChangeUint32ToFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void ChangeUint32ToFloat64::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  // TODO(leszeks): Cvtlui2sd does a manual movl to clear the top bits of the
  // input register. We could eliminate this movl by ensuring that word32
  // registers are always written with 32-bit ops and not 64-bit ones.
  __ Cvtlui2sd(ToDoubleRegister(result()), ToRegister(input()));
}

void CheckedTruncateFloat64ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateFloat64ToInt32::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  DoubleRegister input_reg = ToDoubleRegister(input());
  Register result_reg = ToRegister(result());
  DoubleRegister converted_back = kScratchDoubleReg;

  // Convert the input float64 value to int32.
  __ Cvttsd2si(result_reg, input_reg);
  // Convert that int32 value back to float64.
  __ Cvtlsi2sd(converted_back, result_reg);
  // Check that the result of the float64->int32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  __ Ucomisd(input_reg, converted_back);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, this);

  // Check if {input} is -0.
  Label check_done;
  __ cmpl(result_reg, Immediate(0));
  __ j(not_equal, &check_done);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Register high_word32_of_input = kScratchRegister;
  __ Pextrd(high_word32_of_input, input_reg, 1);
  __ cmpl(high_word32_of_input, Immediate(0));
  __ EmitEagerDeoptIf(less, DeoptimizeReason::kNotInt32, this);

  __ bind(&check_done);
}

void CheckedTruncateFloat64ToUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateFloat64ToUint32::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  DoubleRegister input_reg = ToDoubleRegister(input());
  Register result_reg = ToRegister(result());
  DoubleRegister converted_back = kScratchDoubleReg;

  Label fail;
  // Convert the input float64 value to uint32.
  __ Cvttsd2ui(result_reg, input_reg, &fail);
  // Convert that uint32 value back to float64.
  __ Cvtlui2sd(converted_back, result_reg);
  // Check that the result of the float64->uint32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  __ Ucomisd(input_reg, converted_back);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotUint32, this);

  // Check if {input} is -0.
  Label check_done;
  __ cmpl(result_reg, Immediate(0));
  __ j(not_equal, &check_done);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Register high_word32_of_input = kScratchRegister;
  __ Pextrd(high_word32_of_input, input_reg, 1);
  __ cmpl(high_word32_of_input, Immediate(0));
  __ EmitEagerDeoptIf(less, DeoptimizeReason::kNotUint32, this);

  __ bind(&fail);
  __ EmitEagerDeopt(this, DeoptimizeReason::kNotUint32);

  __ bind(&check_done);
}

void TruncateUint32ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void TruncateUint32ToInt32::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  // No code emitted -- as far as the machine is concerned, int32 is uint32.
  DCHECK_EQ(ToRegister(input()), ToRegister(result()));
}

void TruncateFloat64ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void TruncateFloat64ToInt32::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  __ TruncateDoubleToInt32(ToRegister(result()), ToDoubleRegister(input()));
}

int Construct::MaxCallStackArgs() const {
  using D = Construct_WithFeedbackDescriptor;
  return num_args() + D::GetStackParameterCount();
}
void Construct::SetValueLocationConstraints() {
  using D = Construct_WithFeedbackDescriptor;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseFixed(new_target(), D::GetRegisterParameter(D::kNewTarget));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(this, kReturnRegister0);
}
void Construct::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  using D = Construct_WithFeedbackDescriptor;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(new_target()), D::GetRegisterParameter(D::kNewTarget));
  DCHECK_EQ(ToRegister(context()), kContextRegister);

  for (int i = num_args() - 1; i >= 0; --i) {
    __ Push(arg(i));
  }
  static_assert(D::GetStackParameterIndex(D::kFeedbackVector) == 0);
  static_assert(D::GetStackParameterCount() == 1);
  __ Push(feedback().vector);

  uint32_t arg_count = num_args();
  __ Move(D::GetRegisterParameter(D::kActualArgumentsCount), arg_count);
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());

  __ CallBuiltin(Builtin::kConstruct_WithFeedback);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallBuiltin::MaxCallStackArgs() const {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  if (!descriptor.AllowVarArgs()) {
    return descriptor.GetStackParameterCount();
  } else {
    int all_input_count = InputCountWithoutContext() + (has_feedback() ? 2 : 0);
    DCHECK_GE(all_input_count, descriptor.GetRegisterParameterCount());
    return all_input_count - descriptor.GetRegisterParameterCount();
  }
}
void CallBuiltin::SetValueLocationConstraints() {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  bool has_context = descriptor.HasContextParameter();
  int i = 0;
  for (; i < InputsInRegisterCount(); i++) {
    UseFixed(input(i), descriptor.GetRegisterParameter(i));
  }
  for (; i < InputCountWithoutContext(); i++) {
    UseAny(input(i));
  }
  if (has_context) {
    UseFixed(input(i), kContextRegister);
  }
  DefineAsFixed(this, kReturnRegister0);
}

void CallBuiltin::PassFeedbackSlotOnStack(MaglevAssembler* masm) {
  DCHECK(has_feedback());
  switch (slot_type()) {
    case kTaggedIndex:
      __ Push(TaggedIndex::FromIntptr(feedback().index()));
      break;
    case kSmi:
      __ Push(Smi::FromInt(feedback().index()));
      break;
  }
}

void CallBuiltin::PassFeedbackSlotInRegister(MaglevAssembler* masm) {
  DCHECK(has_feedback());
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  int slot_index = InputCountWithoutContext();
  switch (slot_type()) {
    case kTaggedIndex:
      __ Move(descriptor.GetRegisterParameter(slot_index),
              TaggedIndex::FromIntptr(feedback().index()));
      break;
    case kSmi:
      __ Move(descriptor.GetRegisterParameter(slot_index),
              Smi::FromInt(feedback().index()));
      break;
  }
}

void CallBuiltin::PushFeedback(MaglevAssembler* masm) {
  DCHECK(has_feedback());

  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());
  int slot_index = InputCountWithoutContext();
  int vector_index = slot_index + 1;

  // There are three possibilities:
  // 1. Feedback slot and vector are in register.
  // 2. Feedback slot is in register and vector is on stack.
  // 3. Feedback slot and vector are on stack.
  if (vector_index < descriptor.GetRegisterParameterCount()) {
    PassFeedbackSlotInRegister(masm);
    __ Move(descriptor.GetRegisterParameter(vector_index), feedback().vector);
  } else if (vector_index == descriptor.GetRegisterParameterCount()) {
    PassFeedbackSlotInRegister(masm);
    // We do not allow var args if has_feedback(), so here we have only one
    // parameter on stack and do not need to check stack arguments order.
    __ Push(feedback().vector);
  } else {
    // Same as above. We does not allow var args if has_feedback(), so feedback
    // slot and vector must be last two inputs.
    if (descriptor.GetStackArgumentOrder() == StackArgumentOrder::kDefault) {
      PassFeedbackSlotOnStack(masm);
      __ Push(feedback().vector);
    } else {
      DCHECK_EQ(descriptor.GetStackArgumentOrder(), StackArgumentOrder::kJS);
      __ Push(feedback().vector);
      PassFeedbackSlotOnStack(masm);
    }
  }
}

void CallBuiltin::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin());

  if (descriptor.GetStackArgumentOrder() == StackArgumentOrder::kDefault) {
    for (int i = InputsInRegisterCount(); i < InputCountWithoutContext(); ++i) {
      __ Push(input(i));
    }
    if (has_feedback()) {
      PushFeedback(masm);
    }
  } else {
    DCHECK_EQ(descriptor.GetStackArgumentOrder(), StackArgumentOrder::kJS);
    if (has_feedback()) {
      PushFeedback(masm);
    }
    for (int i = InputCountWithoutContext() - 1; i >= InputsInRegisterCount();
         --i) {
      __ Push(input(i));
    }
  }
  __ CallBuiltin(builtin());
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallRuntime::MaxCallStackArgs() const { return num_args(); }
void CallRuntime::SetValueLocationConstraints() {
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(this, kReturnRegister0);
}
void CallRuntime::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  for (int i = 0; i < num_args(); i++) {
    __ Push(arg(i));
  }
  __ CallRuntime(function_id(), num_args());
  // TODO(victorgomes): Not sure if this is needed for all runtime calls.
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallWithSpread::MaxCallStackArgs() const {
  int argc_no_spread = num_args() - 1;
  if (feedback_.IsValid()) {
    using D =
        CallInterfaceDescriptorFor<Builtin::kCallWithSpread_WithFeedback>::type;
    return argc_no_spread + D::GetStackParameterCount();
  } else {
    using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
    return argc_no_spread + D::GetStackParameterCount();
  }
}
void CallWithSpread::SetValueLocationConstraints() {
  if (feedback_.IsValid()) {
    using D =
        CallInterfaceDescriptorFor<Builtin::kCallWithSpread_WithFeedback>::type;
    UseFixed(function(), D::GetRegisterParameter(D::kTarget));
    UseFixed(spread(), D::GetRegisterParameter(D::kSpread));
  } else {
    using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
    UseFixed(function(), D::GetRegisterParameter(D::kTarget));
    UseFixed(spread(), D::GetRegisterParameter(D::kSpread));
  }
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args() - 1; i++) {
    UseAny(arg(i));
  }
  DefineAsFixed(this, kReturnRegister0);
}
void CallWithSpread::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
#ifdef DEBUG
  if (feedback_.IsValid()) {
    using D =
        CallInterfaceDescriptorFor<Builtin::kCallWithSpread_WithFeedback>::type;
    DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
    DCHECK_EQ(ToRegister(spread()), D::GetRegisterParameter(D::kSpread));
  } else {
    using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
    DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
    DCHECK_EQ(ToRegister(spread()), D::GetRegisterParameter(D::kSpread));
  }
  DCHECK_EQ(ToRegister(context()), kContextRegister);
#endif
  // Push other arguments (other than the spread) to the stack.
  int argc_no_spread = num_args() - 1;
  for (int i = argc_no_spread - 1; i >= 0; --i) {
    __ Push(arg(i));
  }
  if (feedback_.IsValid()) {
    using D =
        CallInterfaceDescriptorFor<Builtin::kCallWithSpread_WithFeedback>::type;
    static_assert(D::GetStackParameterIndex(D::kReceiver) == 0);
    static_assert(D::GetStackParameterCount() == 1);
    __ Push(arg(0));
    __ Move(D::GetRegisterParameter(D::kArgumentsCount), argc_no_spread);
    __ Move(D::GetRegisterParameter(D::kFeedbackVector), feedback().vector);
    __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());
    __ CallBuiltin(Builtin::kCallWithSpread_WithFeedback);
  } else {
    using D = CallInterfaceDescriptorFor<Builtin::kCallWithSpread>::type;
    __ Move(D::GetRegisterParameter(D::kArgumentsCount), argc_no_spread);
    __ CallBuiltin(Builtin::kCallWithSpread);
  }

  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int CallWithArrayLike::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kCallWithArrayLike>::type;
  return D::GetStackParameterCount();
}
void CallWithArrayLike::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kCallWithArrayLike>::type;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseAny(receiver());
  UseFixed(arguments_list(), D::GetRegisterParameter(D::kArgumentsList));
  UseFixed(context(), kContextRegister);
  DefineAsFixed(this, kReturnRegister0);
}
void CallWithArrayLike::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kCallWithArrayLike>::type;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(arguments_list()),
            D::GetRegisterParameter(D::kArgumentsList));
  DCHECK_EQ(ToRegister(context()), kContextRegister);
#endif  // DEBUG
  __ Push(receiver());
  __ CallBuiltin(Builtin::kCallWithArrayLike);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int ConstructWithSpread::MaxCallStackArgs() const {
  int argc_no_spread = num_args() - 1;
  using D = CallInterfaceDescriptorFor<
      Builtin::kConstructWithSpread_WithFeedback>::type;
  return argc_no_spread + D::GetStackParameterCount();
}
void ConstructWithSpread::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<
      Builtin::kConstructWithSpread_WithFeedback>::type;
  UseFixed(function(), D::GetRegisterParameter(D::kTarget));
  UseFixed(new_target(), D::GetRegisterParameter(D::kNewTarget));
  UseFixed(context(), kContextRegister);
  for (int i = 0; i < num_args() - 1; i++) {
    UseAny(arg(i));
  }
  UseFixed(spread(), D::GetRegisterParameter(D::kSpread));
  DefineAsFixed(this, kReturnRegister0);
}
void ConstructWithSpread::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  using D = CallInterfaceDescriptorFor<
      Builtin::kConstructWithSpread_WithFeedback>::type;
  DCHECK_EQ(ToRegister(function()), D::GetRegisterParameter(D::kTarget));
  DCHECK_EQ(ToRegister(new_target()), D::GetRegisterParameter(D::kNewTarget));
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  // Push other arguments (other than the spread) to the stack.
  int argc_no_spread = num_args() - 1;
  for (int i = argc_no_spread - 1; i >= 0; --i) {
    __ Push(arg(i));
  }
  static_assert(D::GetStackParameterIndex(D::kFeedbackVector) == 0);
  static_assert(D::GetStackParameterCount() == 1);
  __ Push(feedback().vector);

  __ Move(D::GetRegisterParameter(D::kActualArgumentsCount), argc_no_spread);
  __ Move(D::GetRegisterParameter(D::kSlot), feedback().index());
  __ CallBuiltin(Builtin::kConstructWithSpread_WithFeedback);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
}

int ConvertReceiver::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  return D::GetStackParameterCount();
}
void ConvertReceiver::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  UseFixed(receiver_input(), D::GetRegisterParameter(D::kInput));
  set_temporaries_needed(1);
  DefineAsFixed(this, kReturnRegister0);
}
void ConvertReceiver::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Label convert_to_object, done;
  Register receiver = ToRegister(receiver_input());
  Register scratch = general_temporaries().first();
  __ JumpIfSmi(receiver, &convert_to_object, Label::kNear);
  static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CmpObjectType(receiver, FIRST_JS_RECEIVER_TYPE, scratch);
  __ j(above_equal, &done);

  if (mode_ != ConvertReceiverMode::kNotNullOrUndefined) {
    Label convert_global_proxy;
    __ JumpIfRoot(receiver, RootIndex::kUndefinedValue, &convert_global_proxy,
                  Label::kNear);
    __ JumpIfNotRoot(receiver, RootIndex::kNullValue, &convert_to_object,
                     Label::kNear);
    __ bind(&convert_global_proxy);
    {
      // Patch receiver to global proxy.
      __ Move(ToRegister(result()),
              target_.native_context().global_proxy_object().object());
    }
    __ jmp(&done);
  }

  __ bind(&convert_to_object);
  // ToObject needs to be ran with the target context installed.
  __ Move(kContextRegister, target_.context().object());
  __ CallBuiltin(Builtin::kToObject);
  __ bind(&done);
}

void IncreaseInterruptBudget::SetValueLocationConstraints() {
  set_temporaries_needed(1);
}
void IncreaseInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register scratch = general_temporaries().first();
  __ movq(scratch, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      scratch, FieldOperand(scratch, JSFunction::kFeedbackCellOffset));
  __ addl(FieldOperand(scratch, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount()));
}

int ReduceInterruptBudget::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudget::SetValueLocationConstraints() {
  set_temporaries_needed(1);
}
void ReduceInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register scratch = general_temporaries().first();
  __ movq(scratch, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      scratch, FieldOperand(scratch, JSFunction::kFeedbackCellOffset));
  __ subl(FieldOperand(scratch, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount()));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      less,
      [](MaglevAssembler* masm, ZoneLabelRef done,
         ReduceInterruptBudget* node) {
        {
          SaveRegisterStateForCall save_register_state(
              masm, node->register_snapshot());
          __ Move(kContextRegister, masm->native_context().object());
          __ Push(MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
          __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev,
                         1);
          save_register_state.DefineSafepointWithLazyDeopt(
              node->lazy_deopt_info());
        }
        __ jmp(*done);
      },
      done, this);
  __ bind(*done);
}

int ThrowReferenceErrorIfHole::MaxCallStackArgs() const { return 1; }
void ThrowReferenceErrorIfHole::SetValueLocationConstraints() {
  UseAny(value());
}
void ThrowReferenceErrorIfHole::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  if (value().operand().IsRegister()) {
    __ CompareRoot(ToRegister(value()), RootIndex::kTheHoleValue);
  } else {
    DCHECK(value().operand().IsStackSlot());
    __ CompareRoot(masm->ToMemOperand(value()), RootIndex::kTheHoleValue);
  }
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, ThrowReferenceErrorIfHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ Push(node->name().object());
        __ CallRuntime(Runtime::kThrowAccessedUninitializedVariable, 1);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

int ThrowSuperNotCalledIfHole::MaxCallStackArgs() const { return 0; }
void ThrowSuperNotCalledIfHole::SetValueLocationConstraints() {
  UseAny(value());
}
void ThrowSuperNotCalledIfHole::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  if (value().operand().IsRegister()) {
    __ CompareRoot(ToRegister(value()), RootIndex::kTheHoleValue);
  } else {
    DCHECK(value().operand().IsStackSlot());
    __ CompareRoot(masm->ToMemOperand(value()), RootIndex::kTheHoleValue);
  }
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, ThrowSuperNotCalledIfHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowSuperNotCalled, 0);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

int ThrowSuperAlreadyCalledIfNotHole::MaxCallStackArgs() const { return 0; }
void ThrowSuperAlreadyCalledIfNotHole::SetValueLocationConstraints() {
  UseAny(value());
}
void ThrowSuperAlreadyCalledIfNotHole::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  if (value().operand().IsRegister()) {
    __ CompareRoot(ToRegister(value()), RootIndex::kTheHoleValue);
  } else {
    DCHECK(value().operand().IsStackSlot());
    __ CompareRoot(masm->ToMemOperand(value()), RootIndex::kTheHoleValue);
  }
  __ JumpToDeferredIf(
      not_equal,
      [](MaglevAssembler* masm, ThrowSuperAlreadyCalledIfNotHole* node) {
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowSuperAlreadyCalledError, 0);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

int ThrowIfNotSuperConstructor::MaxCallStackArgs() const { return 2; }
void ThrowIfNotSuperConstructor::SetValueLocationConstraints() {
  UseRegister(constructor());
  UseRegister(function());
}
void ThrowIfNotSuperConstructor::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  __ LoadMap(kScratchRegister, ToRegister(constructor()));
  __ testl(FieldOperand(kScratchRegister, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsConstructorBit::kMask));
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, ThrowIfNotSuperConstructor* node) {
        __ Push(ToRegister(node->constructor()));
        __ Push(ToRegister(node->function()));
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowNotSuperConstructor, 2);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

// ---
// Control nodes
// ---
void Return::SetValueLocationConstraints() {
  UseFixed(value_input(), kReturnRegister0);
}
void Return::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  DCHECK_EQ(ToRegister(value_input()), kReturnRegister0);

  // Read the formal number of parameters from the top level compilation unit
  // (i.e. the outermost, non inlined function).
  int formal_params_size =
      masm->compilation_info()->toplevel_compilation_unit()->parameter_count();

  // We're not going to continue execution, so we can use an arbitrary register
  // here instead of relying on temporaries from the register allocator.
  Register actual_params_size = r8;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to re-use the
  // incoming argc's register (if it's still valid).
  __ movq(actual_params_size,
          MemOperand(rbp, StandardFrameConstants::kArgCOffset));

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label drop_dynamic_arg_size;
  __ cmpq(actual_params_size, Immediate(formal_params_size));
  __ j(greater, &drop_dynamic_arg_size);

  // Drop receiver + arguments according to static formal arguments size.
  __ Ret(formal_params_size * kSystemPointerSize, kScratchRegister);

  __ bind(&drop_dynamic_arg_size);
  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(actual_params_size, r9, TurboAssembler::kCountIsInteger,
                   TurboAssembler::kCountIncludesReceiver);
  __ Ret();
}

void Switch::SetValueLocationConstraints() { UseRegister(value()); }
void Switch::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  std::unique_ptr<Label*[]> labels = std::make_unique<Label*[]>(size());
  for (int i = 0; i < size(); i++) {
    labels[i] = (targets())[i].block_ptr()->label();
  }
  __ Switch(kScratchRegister, ToRegister(value()), value_base(), labels.get(),
            size());
  if (has_fallthrough()) {
    DCHECK_EQ(fallthrough(), state.next_block());
  } else {
    __ Trap();
  }
}

namespace {

void AttemptOnStackReplacement(MaglevAssembler* masm,
                               ZoneLabelRef no_code_for_osr,
                               JumpLoopPrologue* node, Register scratch0,
                               Register scratch1, int32_t loop_depth,
                               FeedbackSlot feedback_slot,
                               BytecodeOffset osr_offset) {
  // Two cases may cause us to attempt OSR, in the following order:
  //
  // 1) Presence of cached OSR Turbofan code.
  // 2) The OSR urgency exceeds the current loop depth - in that case, trigger
  //    a Turbofan OSR compilation if in concurrent mode.
  //    Otherwise trigger an eager deopt and delegate OSR to Ignition.
  //
  // See also: InterpreterAssembler::OnStackReplacement.

  baseline::BaselineAssembler basm(masm);
  __ AssertFeedbackVector(scratch0);

  // Case 1).
  Label deopt;
  Register maybe_target_code = scratch1;
  {
    basm.TryLoadOptimizedOsrCode(maybe_target_code, scratch0, feedback_slot,
                                 &deopt, Label::kFar);
  }

  // Case 2).
  {
    __ movb(scratch0, FieldOperand(scratch0, FeedbackVector::kOsrStateOffset));
    __ DecodeField<FeedbackVector::OsrUrgencyBits>(scratch0);
    basm.JumpIfByte(baseline::Condition::kUnsignedLessThanEqual, scratch0,
                    loop_depth, *no_code_for_osr, Label::kNear);

    // The osr_urgency exceeds the current loop_depth, signaling an OSR
    // request. Call into runtime to compile.
    {
      // At this point we need a custom register snapshot since additional
      // registers may be live at the eager deopt below (the normal
      // register_snapshot only contains live registers *after this
      // node*).
      // TODO(v8:7700): Consider making the snapshot location
      // configurable.
      RegisterSnapshot snapshot = node->register_snapshot();
      AddDeoptRegistersToSnapshot(&snapshot, node->eager_deopt_info());
      DCHECK(!snapshot.live_registers.has(maybe_target_code));
      SaveRegisterStateForCall save_register_state(masm, snapshot);
      __ Move(kContextRegister, masm->native_context().object());
      __ Push(Smi::FromInt(osr_offset.ToInt()));
      __ CallRuntime(Runtime::kCompileOptimizedOSRFromMaglev, 1);
      save_register_state.DefineSafepoint();
      __ Move(maybe_target_code, kReturnRegister0);
    }

    // A `0` return value means there is no OSR code available yet. Fall
    // through for now, OSR code will be picked up once it exists and is
    // cached on the feedback vector.
    __ Cmp(maybe_target_code, 0);
    __ j(equal, *no_code_for_osr, Label::kNear);
  }

  __ bind(&deopt);
  if (V8_LIKELY(v8_flags.turbofan)) {
    // None of the mutated input registers should be a register input into the
    // eager deopt info.
    DCHECK_REGLIST_EMPTY(
        RegList{scratch0, scratch1} &
        GetGeneralRegistersUsedAsInputs(node->eager_deopt_info()));
    __ EmitEagerDeopt(node, DeoptimizeReason::kPrepareForOnStackReplacement);
  } else {
    // Fall through. With TF disabled we cannot OSR and thus it doesn't make
    // sense to start the process. We do still perform all remaining
    // bookkeeping above though, to keep Maglev code behavior roughly the same
    // in both configurations.
  }
}

}  // namespace

int JumpLoopPrologue::MaxCallStackArgs() const {
  // For the kCompileOptimizedOSRFromMaglev call.
  return 1;
}
void JumpLoopPrologue::SetValueLocationConstraints() {
  if (!v8_flags.use_osr) return;
  set_temporaries_needed(2);
}
void JumpLoopPrologue::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  if (!v8_flags.use_osr) return;
  Register scratch0 = general_temporaries().PopFirst();
  Register scratch1 = general_temporaries().PopFirst();

  const Register osr_state = scratch1;
  __ Move(scratch0, unit_->feedback().object());
  __ AssertFeedbackVector(scratch0);
  __ movb(osr_state, FieldOperand(scratch0, FeedbackVector::kOsrStateOffset));

  // The quick initial OSR check. If it passes, we proceed on to more
  // expensive OSR logic.
  static_assert(FeedbackVector::MaybeHasOptimizedOsrCodeBit::encode(true) >
                FeedbackVector::kMaxOsrUrgency);
  __ cmpl(osr_state, Immediate(loop_depth_));
  ZoneLabelRef no_code_for_osr(masm);
  __ JumpToDeferredIf(above, AttemptOnStackReplacement, no_code_for_osr, this,
                      scratch0, scratch1, loop_depth_, feedback_slot_,
                      osr_offset_);
  __ bind(*no_code_for_osr);
}

void BranchIfUndefinedOrNull::SetValueLocationConstraints() {
  UseRegister(condition_input());
}
void BranchIfUndefinedOrNull::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(condition_input());
  __ JumpIfRoot(value, RootIndex::kUndefinedValue, if_true()->label());
  __ JumpIfRoot(value, RootIndex::kNullValue, if_true()->label());
  auto* next_block = state.next_block();
  if (if_false() != next_block) {
    __ jmp(if_false()->label());
  }
}

void BranchIfJSReceiver::SetValueLocationConstraints() {
  UseRegister(condition_input());
}
void BranchIfJSReceiver::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register value = ToRegister(condition_input());
  __ JumpIfSmi(value, if_false()->label());
  __ LoadMap(kScratchRegister, value);
  __ CmpInstanceType(kScratchRegister, FIRST_JS_RECEIVER_TYPE);
  __ Branch(above_equal, if_true(), if_false(), state.next_block());
}

void BranchIfFloat64Compare::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfFloat64Compare::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Ucomisd(left, right);
  __ j(parity_even, if_false()->label());
  __ Branch(ConditionForFloat64(operation_), if_true(), if_false(),
            state.next_block());
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
