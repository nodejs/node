// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-gc-lowering.h"

#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/wasm-graph-assembler.h"
#include "src/objects/heap-number.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8 {
namespace internal {
namespace compiler {

WasmGCLowering::WasmGCLowering(Editor* editor, MachineGraph* mcgraph,
                               const wasm::WasmModule* module,
                               bool disable_trap_handler,
                               SourcePositionTable* source_position_table)
    : AdvancedReducer(editor),
      null_check_strategy_(trap_handler::IsTrapHandlerEnabled() &&
                                   V8_STATIC_ROOTS_BOOL && !disable_trap_handler
                               ? kTrapHandler
                               : kExplicitNullChecks),
      gasm_(mcgraph, mcgraph->zone()),
      module_(module),
      dead_(mcgraph->Dead()),
      mcgraph_(mcgraph),
      source_position_table_(source_position_table) {}

Reduction WasmGCLowering::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kWasmTypeCheck:
      return ReduceWasmTypeCheck(node);
    case IrOpcode::kWasmTypeCast:
      return ReduceWasmTypeCast(node);
    case IrOpcode::kAssertNotNull:
      return ReduceAssertNotNull(node);
    case IrOpcode::kNull:
      return ReduceNull(node);
    case IrOpcode::kIsNull:
      return ReduceIsNull(node);
    case IrOpcode::kIsNotNull:
      return ReduceIsNotNull(node);
    case IrOpcode::kRttCanon:
      return ReduceRttCanon(node);
    case IrOpcode::kTypeGuard:
      return ReduceTypeGuard(node);
    case IrOpcode::kWasmExternInternalize:
      return ReduceWasmExternInternalize(node);
    case IrOpcode::kWasmExternExternalize:
      return ReduceWasmExternExternalize(node);
    case IrOpcode::kWasmStructGet:
      return ReduceWasmStructGet(node);
    case IrOpcode::kWasmStructSet:
      return ReduceWasmStructSet(node);
    case IrOpcode::kWasmArrayGet:
      return ReduceWasmArrayGet(node);
    case IrOpcode::kWasmArraySet:
      return ReduceWasmArraySet(node);
    case IrOpcode::kWasmArrayLength:
      return ReduceWasmArrayLength(node);
    case IrOpcode::kWasmArrayInitializeLength:
      return ReduceWasmArrayInitializeLength(node);
    case IrOpcode::kStringAsWtf16:
      return ReduceStringAsWtf16(node);
    case IrOpcode::kStringPrepareForGetCodeunit:
      return ReduceStringPrepareForGetCodeunit(node);
    default:
      return NoChange();
  }
}

Node* WasmGCLowering::Null(wasm::ValueType type) {
  RootIndex index = wasm::IsSubtypeOf(type, wasm::kWasmExternRef, module_)
                        ? RootIndex::kNullValue
                        : RootIndex::kWasmNull;
  return gasm_.LoadImmutable(MachineType::Pointer(), gasm_.LoadRootRegister(),
                             IsolateData::root_slot_offset(index));
}

Node* WasmGCLowering::IsNull(Node* object, wasm::ValueType type) {
  Tagged_t static_null =
      wasm::GetWasmEngine()->compressed_wasm_null_value_or_zero();
  Node* null_value = !wasm::IsSubtypeOf(type, wasm::kWasmExternRef, module_) &&
                             static_null != 0
                         ? gasm_.UintPtrConstant(static_null)
                         : Null(type);
  return gasm_.TaggedEqual(object, null_value);
}

// TODO(manoskouk): Use the Callbacks infrastructure from wasm-compiler.h to
// unify all check/cast implementations.
// TODO(manoskouk): Find a way to optimize branches on typechecks.
Reduction WasmGCLowering::ReduceWasmTypeCheck(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmTypeCheck);

  Node* object = node->InputAt(0);
  Node* rtt = node->InputAt(1);
  Node* effect_input = NodeProperties::GetEffectInput(node);
  Node* control_input = NodeProperties::GetControlInput(node);
  auto config = OpParameter<WasmTypeCheckConfig>(node->op());
  int rtt_depth = wasm::GetSubtypingDepth(module_, config.to.ref_index());
  bool object_can_be_null = config.from.is_nullable();
  bool object_can_be_i31 =
      wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), config.from, module_);

  gasm_.InitializeEffectControl(effect_input, control_input);

  auto end_label = gasm_.MakeLabel(MachineRepresentation::kWord32);
  bool is_cast_from_any = config.from.is_reference_to(wasm::HeapType::kAny);

  // Skip the null check if casting from any and if null results in check
  // failure. In that case the instance type check will identify null as not
  // being a wasm object and return 0 (failure).
  if (object_can_be_null && (!is_cast_from_any || config.to.is_nullable())) {
    const int kResult = config.to.is_nullable() ? 1 : 0;
    gasm_.GotoIf(IsNull(object, wasm::kWasmAnyRef), &end_label,
                 BranchHint::kFalse, gasm_.Int32Constant(kResult));
  }

  if (object_can_be_i31) {
    gasm_.GotoIf(gasm_.IsSmi(object), &end_label, gasm_.Int32Constant(0));
  }

  Node* map = gasm_.LoadMap(object);

  if (module_->types[config.to.ref_index()].is_final) {
    gasm_.Goto(&end_label, gasm_.TaggedEqual(map, rtt));
  } else {
    // First, check if types happen to be equal. This has been shown to give
    // large speedups.
    gasm_.GotoIf(gasm_.TaggedEqual(map, rtt), &end_label, BranchHint::kTrue,
                 gasm_.Int32Constant(1));

    // Check if map instance type identifies a wasm object.
    if (is_cast_from_any) {
      Node* is_wasm_obj = gasm_.IsDataRefMap(map);
      gasm_.GotoIfNot(is_wasm_obj, &end_label, BranchHint::kTrue,
                      gasm_.Int32Constant(0));
    }

    Node* type_info = gasm_.LoadWasmTypeInfo(map);
    DCHECK_GE(rtt_depth, 0);
    // If the depth of the rtt is known to be less that the minimum supertype
    // array length, we can access the supertype without bounds-checking the
    // supertype array.
    if (static_cast<uint32_t>(rtt_depth) >= wasm::kMinimumSupertypeArraySize) {
      Node* supertypes_length =
          gasm_.BuildChangeSmiToIntPtr(gasm_.LoadImmutableFromObject(
              MachineType::TaggedSigned(), type_info,
              wasm::ObjectAccess::ToTagged(
                  WasmTypeInfo::kSupertypesLengthOffset)));
      gasm_.GotoIfNot(gasm_.UintLessThan(gasm_.IntPtrConstant(rtt_depth),
                                         supertypes_length),
                      &end_label, BranchHint::kTrue, gasm_.Int32Constant(0));
    }

    Node* maybe_match = gasm_.LoadImmutableFromObject(
        MachineType::TaggedPointer(), type_info,
        wasm::ObjectAccess::ToTagged(WasmTypeInfo::kSupertypesOffset +
                                     kTaggedSize * rtt_depth));

    gasm_.Goto(&end_label, gasm_.TaggedEqual(maybe_match, rtt));
  }

  gasm_.Bind(&end_label);

  ReplaceWithValue(node, end_label.PhiAt(0), gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(end_label.PhiAt(0));  // Meaningless argument.
}

Reduction WasmGCLowering::ReduceWasmTypeCast(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmTypeCast);

  Node* object = node->InputAt(0);
  Node* rtt = node->InputAt(1);
  Node* effect_input = NodeProperties::GetEffectInput(node);
  Node* control_input = NodeProperties::GetControlInput(node);
  auto config = OpParameter<WasmTypeCheckConfig>(node->op());
  int rtt_depth = wasm::GetSubtypingDepth(module_, config.to.ref_index());
  bool object_can_be_null = config.from.is_nullable();
  bool object_can_be_i31 =
      wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), config.from, module_);

  gasm_.InitializeEffectControl(effect_input, control_input);

  auto end_label = gasm_.MakeLabel();
  bool is_cast_from_any = config.from.is_reference_to(wasm::HeapType::kAny);

  // Skip the null check if casting from any and if null results in check
  // failure. In that case the instance type check will identify null as not
  // being a wasm object and trap.
  if (object_can_be_null && (!is_cast_from_any || config.to.is_nullable())) {
    Node* is_null = IsNull(object, wasm::kWasmAnyRef);
    if (config.to.is_nullable()) {
      gasm_.GotoIf(is_null, &end_label, BranchHint::kFalse);
    } else if (!v8_flags.experimental_wasm_skip_null_checks) {
      gasm_.TrapIf(is_null, TrapId::kTrapIllegalCast);
      UpdateSourcePosition(gasm_.effect(), node);
    }
  }

  if (object_can_be_i31) {
    gasm_.TrapIf(gasm_.IsSmi(object), TrapId::kTrapIllegalCast);
    UpdateSourcePosition(gasm_.effect(), node);
  }

  Node* map = gasm_.LoadMap(object);

  if (module_->types[config.to.ref_index()].is_final) {
    gasm_.TrapUnless(gasm_.TaggedEqual(map, rtt), TrapId::kTrapIllegalCast);
    UpdateSourcePosition(gasm_.effect(), node);
    gasm_.Goto(&end_label);
  } else {
    // First, check if types happen to be equal. This has been shown to give
    // large speedups.
    gasm_.GotoIf(gasm_.TaggedEqual(map, rtt), &end_label, BranchHint::kTrue);

    // Check if map instance type identifies a wasm object.
    if (is_cast_from_any) {
      Node* is_wasm_obj = gasm_.IsDataRefMap(map);
      gasm_.TrapUnless(is_wasm_obj, TrapId::kTrapIllegalCast);
      UpdateSourcePosition(gasm_.effect(), node);
    }

    Node* type_info = gasm_.LoadWasmTypeInfo(map);
    DCHECK_GE(rtt_depth, 0);
    // If the depth of the rtt is known to be less that the minimum supertype
    // array length, we can access the supertype without bounds-checking the
    // supertype array.
    if (static_cast<uint32_t>(rtt_depth) >= wasm::kMinimumSupertypeArraySize) {
      Node* supertypes_length =
          gasm_.BuildChangeSmiToIntPtr(gasm_.LoadImmutableFromObject(
              MachineType::TaggedSigned(), type_info,
              wasm::ObjectAccess::ToTagged(
                  WasmTypeInfo::kSupertypesLengthOffset)));
      gasm_.TrapUnless(gasm_.UintLessThan(gasm_.IntPtrConstant(rtt_depth),
                                          supertypes_length),
                       TrapId::kTrapIllegalCast);
      UpdateSourcePosition(gasm_.effect(), node);
    }

    Node* maybe_match = gasm_.LoadImmutableFromObject(
        MachineType::TaggedPointer(), type_info,
        wasm::ObjectAccess::ToTagged(WasmTypeInfo::kSupertypesOffset +
                                     kTaggedSize * rtt_depth));

    gasm_.TrapUnless(gasm_.TaggedEqual(maybe_match, rtt),
                     TrapId::kTrapIllegalCast);
    UpdateSourcePosition(gasm_.effect(), node);
    gasm_.Goto(&end_label);
  }

  gasm_.Bind(&end_label);

  ReplaceWithValue(node, object, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(object);
}

Reduction WasmGCLowering::ReduceAssertNotNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAssertNotNull);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* object = NodeProperties::GetValueInput(node, 0);
  gasm_.InitializeEffectControl(effect, control);
  auto op_parameter = OpParameter<AssertNotNullParameters>(node->op());
  // When able, implement a non-null assertion by loading from the object just
  // after the map word. This will trap for null and be handled by the trap
  // handler.
  if (op_parameter.trap_id == TrapId::kTrapNullDereference) {
    if (!v8_flags.experimental_wasm_skip_null_checks) {
      // For supertypes of i31ref, we would need to check for i31ref anyway
      // before loading from the object, so we might as well just check directly
      // for null.
      // For subtypes of externref, we use JS null, so we have to check
      // explicitly.
      if (null_check_strategy_ == kExplicitNullChecks ||
          wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), op_parameter.type,
                            module_) ||
          wasm::IsSubtypeOf(op_parameter.type, wasm::kWasmExternRef, module_)) {
        gasm_.TrapIf(IsNull(object, op_parameter.type), op_parameter.trap_id);
        UpdateSourcePosition(gasm_.effect(), node);
      } else {
        static_assert(WasmStruct::kHeaderSize > kTaggedSize);
        static_assert(WasmArray::kHeaderSize > kTaggedSize);
        // TODO(manoskouk): JSFunction::kHeaderSize also has to be >kTaggedSize.
        Node* trap_null = gasm_.LoadTrapOnNull(
            MachineType::Int32(), object,
            gasm_.IntPtrConstant(wasm::ObjectAccess::ToTagged(kTaggedSize)));
        UpdateSourcePosition(trap_null, node);
      }
    }
  } else {
    gasm_.TrapIf(IsNull(object, op_parameter.type), op_parameter.trap_id);
    UpdateSourcePosition(gasm_.effect(), node);
  }

  ReplaceWithValue(node, object, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(object);
}

Reduction WasmGCLowering::ReduceNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kNull);
  auto type = OpParameter<wasm::ValueType>(node->op());
  return Replace(Null(type));
}

Reduction WasmGCLowering::ReduceIsNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kIsNull);
  Node* object = NodeProperties::GetValueInput(node, 0);
  auto type = OpParameter<wasm::ValueType>(node->op());
  return Replace(IsNull(object, type));
}

Reduction WasmGCLowering::ReduceIsNotNull(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kIsNotNull);
  Node* object = NodeProperties::GetValueInput(node, 0);
  auto type = OpParameter<wasm::ValueType>(node->op());
  return Replace(
      gasm_.Word32Equal(IsNull(object, type), gasm_.Int32Constant(0)));
}

Reduction WasmGCLowering::ReduceRttCanon(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kRttCanon);
  int type_index = OpParameter<int>(node->op());
  Node* instance_node = node->InputAt(0);
  Node* maps_list = gasm_.LoadImmutable(
      MachineType::TaggedPointer(), instance_node,
      WasmInstanceObject::kManagedObjectMapsOffset - kHeapObjectTag);
  return Replace(gasm_.LoadImmutable(
      MachineType::TaggedPointer(), maps_list,
      wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(type_index)));
}

Reduction WasmGCLowering::ReduceTypeGuard(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kTypeGuard);
  Node* alias = NodeProperties::GetValueInput(node, 0);
  ReplaceWithValue(node, alias);
  node->Kill();
  return Replace(alias);
}

namespace {
constexpr int32_t kInt31MaxValue = 0x3fffffff;
constexpr int32_t kInt31MinValue = -kInt31MaxValue - 1;
}  // namespace

Reduction WasmGCLowering::ReduceWasmExternInternalize(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmExternInternalize);
  Node* input = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  gasm_.InitializeEffectControl(effect, control);

  auto end_label = gasm_.MakeLabel(MachineRepresentation::kTagged);
  auto null_label = gasm_.MakeLabel();
  auto smi_label = gasm_.MakeLabel();
  auto int_to_smi_label = gasm_.MakeLabel();
  auto heap_number_label = gasm_.MakeLabel();

  gasm_.GotoIf(IsNull(input, wasm::kWasmExternRef), &null_label);
  gasm_.GotoIf(gasm_.IsSmi(input), &smi_label);
  Node* is_heap_number = gasm_.HasInstanceType(input, HEAP_NUMBER_TYPE);
  gasm_.GotoIf(is_heap_number, &heap_number_label);
  // For anything else, just pass through the value.
  gasm_.Goto(&end_label, input);

  gasm_.Bind(&null_label);
  gasm_.Goto(&end_label, Null(wasm::kWasmNullRef));

  // Canonicalize SMI.
  gasm_.Bind(&smi_label);
  if constexpr (SmiValuesAre31Bits()) {
    gasm_.Goto(&end_label, input);
  } else {
    auto to_heap_number_label = gasm_.MakeLabel();
    Node* int_value = gasm_.BuildChangeSmiToInt32(input);

    // Convert to heap number if the int32 does not fit into an i31ref.
    gasm_.GotoIf(
        gasm_.Int32LessThan(gasm_.Int32Constant(kInt31MaxValue), int_value),
        &to_heap_number_label);
    gasm_.GotoIf(
        gasm_.Int32LessThan(int_value, gasm_.Int32Constant(kInt31MinValue)),
        &to_heap_number_label);
    gasm_.Goto(&end_label, input);

    gasm_.Bind(&to_heap_number_label);
    Node* heap_number = gasm_.CallBuiltin(Builtin::kWasmInt32ToHeapNumber,
                                          Operator::kPure, int_value);
    gasm_.Goto(&end_label, heap_number);
  }

  // Convert HeapNumber to SMI if possible.
  gasm_.Bind(&heap_number_label);
  Node* float_value = gasm_.LoadFromObject(
      MachineType::Float64(), input,
      wasm::ObjectAccess::ToTagged(HeapNumber::kValueOffset));
  // Check range of float value.
  gasm_.GotoIf(
      gasm_.Float64LessThan(float_value, gasm_.Float64Constant(kInt31MinValue)),
      &end_label, input);
  gasm_.GotoIf(
      gasm_.Float64LessThan(gasm_.Float64Constant(kInt31MaxValue), float_value),
      &end_label, input);
  // Check if value is -0.
  Node* is_minus_zero = nullptr;
  if (mcgraph_->machine()->Is64()) {
    Node* minus_zero = gasm_.Int64Constant(base::bit_cast<int64_t>(-0.0));
    Node* float_bits = gasm_.BitcastFloat64ToInt64(float_value);
    is_minus_zero = gasm_.Word64Equal(float_bits, minus_zero);
  } else {
    constexpr int32_t kMinusZeroLoBits = static_cast<int32_t>(0);
    constexpr int32_t kMinusZeroHiBits = static_cast<int32_t>(1) << 31;
    auto done = gasm_.MakeLabel(MachineRepresentation::kBit);

    Node* value_lo = gasm_.Float64ExtractLowWord32(float_value);
    gasm_.GotoIfNot(
        gasm_.Word32Equal(value_lo, gasm_.Int32Constant(kMinusZeroLoBits)),
        &done, gasm_.Int32Constant(0));
    Node* value_hi = gasm_.Float64ExtractHighWord32(float_value);
    gasm_.Goto(&done, gasm_.Word32Equal(value_hi,
                                        gasm_.Int32Constant(kMinusZeroHiBits)));
    gasm_.Bind(&done);
    is_minus_zero = done.PhiAt(0);
  }
  gasm_.GotoIf(is_minus_zero, &end_label, input);
  // Check if value is integral.
  Node* int_value = gasm_.ChangeFloat64ToInt32(float_value);
  gasm_.GotoIf(
      gasm_.Float64Equal(float_value, gasm_.ChangeInt32ToFloat64(int_value)),
      &int_to_smi_label);
  gasm_.Goto(&end_label, input);

  gasm_.Bind(&int_to_smi_label);
  gasm_.Goto(&end_label, gasm_.BuildChangeInt32ToSmi(int_value));

  gasm_.Bind(&end_label);
  ReplaceWithValue(node, end_label.PhiAt(0), gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(end_label.PhiAt(0));
}

Reduction WasmGCLowering::ReduceWasmExternExternalize(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmExternExternalize);
  Node* object = node->InputAt(0);
  gasm_.InitializeEffectControl(NodeProperties::GetEffectInput(node),
                                NodeProperties::GetControlInput(node));
  auto label = gasm_.MakeLabel(MachineRepresentation::kTagged);
  gasm_.GotoIfNot(IsNull(object, wasm::kWasmAnyRef), &label, object);
  gasm_.Goto(&label, Null(wasm::kWasmExternRef));
  gasm_.Bind(&label);
  ReplaceWithValue(node, label.PhiAt(0), gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(label.PhiAt(0));
}

Reduction WasmGCLowering::ReduceWasmStructGet(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmStructGet);
  WasmFieldInfo info = OpParameter<WasmFieldInfo>(node->op());

  Node* object = NodeProperties::GetValueInput(node, 0);

  gasm_.InitializeEffectControl(NodeProperties::GetEffectInput(node),
                                NodeProperties::GetControlInput(node));

  MachineType type = MachineType::TypeForRepresentation(
      info.type->field(info.field_index).machine_representation(),
      info.is_signed);

  Node* offset = gasm_.FieldOffset(info.type, info.field_index);

  if (null_check_strategy_ == kExplicitNullChecks &&
      info.null_check == kWithNullCheck) {
    gasm_.TrapIf(IsNull(object, wasm::kWasmAnyRef),
                 TrapId::kTrapNullDereference);
    UpdateSourcePosition(gasm_.effect(), node);
  }

  bool use_null_trap =
      null_check_strategy_ == kTrapHandler && info.null_check == kWithNullCheck;
  Node* load = use_null_trap ? gasm_.LoadTrapOnNull(type, object, offset)
               : info.type->mutability(info.field_index)
                   ? gasm_.LoadFromObject(type, object, offset)
                   : gasm_.LoadImmutableFromObject(type, object, offset);
  if (use_null_trap) {
    UpdateSourcePosition(load, node);
  }

  ReplaceWithValue(node, load, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(load);
}

Reduction WasmGCLowering::ReduceWasmStructSet(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmStructSet);
  WasmFieldInfo info = OpParameter<WasmFieldInfo>(node->op());

  gasm_.InitializeEffectControl(NodeProperties::GetEffectInput(node),
                                NodeProperties::GetControlInput(node));

  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 1);

  if (null_check_strategy_ == kExplicitNullChecks &&
      info.null_check == kWithNullCheck) {
    gasm_.TrapIf(IsNull(object, wasm::kWasmAnyRef),
                 TrapId::kTrapNullDereference);
    UpdateSourcePosition(gasm_.effect(), node);
  }

  wasm::ValueType field_type = info.type->field(info.field_index);
  Node* offset = gasm_.FieldOffset(info.type, info.field_index);

  Node* store =
      null_check_strategy_ == kTrapHandler && info.null_check == kWithNullCheck
          ? gasm_.StoreTrapOnNull({field_type.machine_representation(),
                                   field_type.is_reference() ? kFullWriteBarrier
                                                             : kNoWriteBarrier},
                                  object, offset, value)
      : info.type->mutability(info.field_index)
          ? gasm_.StoreToObject(ObjectAccessForGCStores(field_type), object,
                                offset, value)
          : gasm_.InitializeImmutableInObject(
                ObjectAccessForGCStores(field_type), object, offset, value);
  ReplaceWithValue(node, store, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(store);
}

Reduction WasmGCLowering::ReduceWasmArrayGet(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmArrayGet);
  WasmElementInfo info = OpParameter<WasmElementInfo>(node->op());

  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* index = NodeProperties::GetValueInput(node, 1);

  gasm_.InitializeEffectControl(NodeProperties::GetEffectInput(node),
                                NodeProperties::GetControlInput(node));

  Node* offset = gasm_.WasmArrayElementOffset(index, info.type->element_type());

  MachineType type = MachineType::TypeForRepresentation(
      info.type->element_type().machine_representation(), info.is_signed);

  Node* value = info.type->mutability()
                    ? gasm_.LoadFromObject(type, object, offset)
                    : gasm_.LoadImmutableFromObject(type, object, offset);

  return Replace(value);
}

Reduction WasmGCLowering::ReduceWasmArraySet(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmArraySet);
  const wasm::ArrayType* type = OpParameter<const wasm::ArrayType*>(node->op());

  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* index = NodeProperties::GetValueInput(node, 1);
  Node* value = NodeProperties::GetValueInput(node, 2);

  gasm_.InitializeEffectControl(NodeProperties::GetEffectInput(node),
                                NodeProperties::GetControlInput(node));

  Node* offset = gasm_.WasmArrayElementOffset(index, type->element_type());

  ObjectAccess access = ObjectAccessForGCStores(type->element_type());

  Node* store =
      type->mutability()
          ? gasm_.StoreToObject(access, object, offset, value)
          : gasm_.InitializeImmutableInObject(access, object, offset, value);

  return Replace(store);
}

Reduction WasmGCLowering::ReduceWasmArrayLength(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmArrayLength);
  Node* object = NodeProperties::GetValueInput(node, 0);

  gasm_.InitializeEffectControl(NodeProperties::GetEffectInput(node),
                                NodeProperties::GetControlInput(node));

  bool null_check = OpParameter<bool>(node->op());

  if (null_check_strategy_ == kExplicitNullChecks &&
      null_check == kWithNullCheck) {
    gasm_.TrapIf(IsNull(object, wasm::kWasmAnyRef),
                 TrapId::kTrapNullDereference);
    UpdateSourcePosition(gasm_.effect(), node);
  }

  bool use_null_trap =
      null_check_strategy_ == kTrapHandler && null_check == kWithNullCheck;
  Node* length =
      use_null_trap
          ? gasm_.LoadTrapOnNull(
                MachineType::Uint32(), object,
                gasm_.IntPtrConstant(
                    wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset)))
          : gasm_.LoadImmutableFromObject(
                MachineType::Uint32(), object,
                wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset));
  if (use_null_trap) {
    UpdateSourcePosition(length, node);
  }

  ReplaceWithValue(node, length, gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(length);
}

Reduction WasmGCLowering::ReduceWasmArrayInitializeLength(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmArrayInitializeLength);
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* length = NodeProperties::GetValueInput(node, 1);

  gasm_.InitializeEffectControl(NodeProperties::GetEffectInput(node),
                                NodeProperties::GetControlInput(node));

  Node* set_length = gasm_.InitializeImmutableInObject(
      ObjectAccess{MachineType::Uint32(), kNoWriteBarrier}, object,
      wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset), length);

  return Replace(set_length);
}

Reduction WasmGCLowering::ReduceStringAsWtf16(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStringAsWtf16);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* str = NodeProperties::GetValueInput(node, 0);

  gasm_.InitializeEffectControl(effect, control);

  auto done = gasm_.MakeLabel(MachineRepresentation::kTaggedPointer);
  Node* instance_type = gasm_.LoadInstanceType(gasm_.LoadMap(str));
  Node* string_representation = gasm_.Word32And(
      instance_type, gasm_.Int32Constant(kStringRepresentationMask));
  gasm_.GotoIf(gasm_.Word32Equal(string_representation,
                                 gasm_.Int32Constant(kSeqStringTag)),
               &done, str);
  gasm_.Goto(&done, gasm_.CallBuiltin(Builtin::kWasmStringAsWtf16,
                                      Operator::kPure, str));
  gasm_.Bind(&done);
  ReplaceWithValue(node, done.PhiAt(0), gasm_.effect(), gasm_.control());
  node->Kill();
  return Replace(done.PhiAt(0));
}

Reduction WasmGCLowering::ReduceStringPrepareForGetCodeunit(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStringPrepareForGetCodeunit);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* original_string = NodeProperties::GetValueInput(node, 0);

  gasm_.InitializeEffectControl(effect, control);

  auto dispatch =
      gasm_.MakeLoopLabel(MachineRepresentation::kTaggedPointer,  // String.
                          MachineRepresentation::kWord32,   // Instance type.
                          MachineRepresentation::kWord32);  // Offset.
  auto next = gasm_.MakeLabel(MachineRepresentation::kTaggedPointer,  // String.
                              MachineRepresentation::kWord32,  // Instance type.
                              MachineRepresentation::kWord32);  // Offset.
  auto direct_string =
      gasm_.MakeLabel(MachineRepresentation::kTaggedPointer,  // String.
                      MachineRepresentation::kWord32,         // Instance type.
                      MachineRepresentation::kWord32);        // Offset.

  // These values will be used to replace the original node's projections.
  // The first, "string", is either a SeqString or Smi(0) (in case of external
  // string). Notably this makes it GC-safe: if that string moves, this pointer
  // will be updated accordingly.
  // The second, "offset", has full register width so that it can be used to
  // store external pointers: for external strings, we add up the character
  // backing store's base address and any slice offset.
  // The third, "character width", is a shift width, i.e. it is 0 for one-byte
  // strings, 1 for two-byte strings, kCharWidthBailoutSentinel for uncached
  // external strings (for which "string"/"offset" are invalid and unusable).
  auto done =
      gasm_.MakeLabel(MachineRepresentation::kTagged,        // String.
                      MachineType::PointerRepresentation(),  // Offset.
                      MachineRepresentation::kWord32);       // Character width.

  Node* original_type = gasm_.LoadInstanceType(gasm_.LoadMap(original_string));
  gasm_.Goto(&dispatch, original_string, original_type, gasm_.Int32Constant(0));

  gasm_.Bind(&dispatch);
  {
    auto thin_string = gasm_.MakeLabel(MachineRepresentation::kTaggedPointer);
    auto cons_string = gasm_.MakeLabel(MachineRepresentation::kTaggedPointer);

    Node* string = dispatch.PhiAt(0);
    Node* instance_type = dispatch.PhiAt(1);
    Node* offset = dispatch.PhiAt(2);
    static_assert(kIsIndirectStringTag == 1);
    static constexpr int kIsDirectStringTag = 0;
    gasm_.GotoIf(gasm_.Word32Equal(
                     gasm_.Word32And(instance_type, gasm_.Int32Constant(
                                                        kIsIndirectStringMask)),
                     gasm_.Int32Constant(kIsDirectStringTag)),
                 &direct_string, string, instance_type, offset);

    // Handle indirect strings.
    Node* string_representation = gasm_.Word32And(
        instance_type, gasm_.Int32Constant(kStringRepresentationMask));
    gasm_.GotoIf(gasm_.Word32Equal(string_representation,
                                   gasm_.Int32Constant(kThinStringTag)),
                 &thin_string, string);
    gasm_.GotoIf(gasm_.Word32Equal(string_representation,
                                   gasm_.Int32Constant(kConsStringTag)),
                 &cons_string, string);

    // Sliced string.
    Node* new_offset = gasm_.Int32Add(
        offset,
        gasm_.BuildChangeSmiToInt32(gasm_.LoadImmutableFromObject(
            MachineType::TaggedSigned(), string,
            wasm::ObjectAccess::ToTagged(SlicedString::kOffsetOffset))));
    Node* parent = gasm_.LoadImmutableFromObject(
        MachineType::TaggedPointer(), string,
        wasm::ObjectAccess::ToTagged(SlicedString::kParentOffset));
    Node* parent_type = gasm_.LoadInstanceType(gasm_.LoadMap(parent));
    gasm_.Goto(&next, parent, parent_type, new_offset);

    // Thin string.
    gasm_.Bind(&thin_string);
    Node* actual = gasm_.LoadImmutableFromObject(
        MachineType::TaggedPointer(), string,
        wasm::ObjectAccess::ToTagged(ThinString::kActualOffset));
    Node* actual_type = gasm_.LoadInstanceType(gasm_.LoadMap(actual));
    // ThinStrings always reference (internalized) direct strings.
    gasm_.Goto(&direct_string, actual, actual_type, offset);

    // Flat cons string. (Non-flat cons strings are ruled out by
    // string.as_wtf16.)
    gasm_.Bind(&cons_string);
    Node* first = gasm_.LoadImmutableFromObject(
        MachineType::TaggedPointer(), string,
        wasm::ObjectAccess::ToTagged(ConsString::kFirstOffset));
    Node* first_type = gasm_.LoadInstanceType(gasm_.LoadMap(first));
    gasm_.Goto(&next, first, first_type, offset);

    gasm_.Bind(&next);
    gasm_.Goto(&dispatch, next.PhiAt(0), next.PhiAt(1), next.PhiAt(2));
  }

  gasm_.Bind(&direct_string);
  {
    Node* string = direct_string.PhiAt(0);
    Node* instance_type = direct_string.PhiAt(1);
    Node* offset = direct_string.PhiAt(2);

    Node* is_onebyte = gasm_.Word32And(
        instance_type, gasm_.Int32Constant(kStringEncodingMask));
    // Char width shift is 1 - (is_onebyte).
    static_assert(kStringEncodingMask == 1 << 3);
    Node* charwidth_shift =
        gasm_.Int32Sub(gasm_.Int32Constant(1),
                       gasm_.Word32Shr(is_onebyte, gasm_.Int32Constant(3)));

    auto external = gasm_.MakeLabel();
    Node* string_representation = gasm_.Word32And(
        instance_type, gasm_.Int32Constant(kStringRepresentationMask));
    gasm_.GotoIf(gasm_.Word32Equal(string_representation,
                                   gasm_.Int32Constant(kExternalStringTag)),
                 &external);

    // Sequential string.
    static_assert(SeqOneByteString::kCharsOffset ==
                  SeqTwoByteString::kCharsOffset);
    Node* final_offset = gasm_.Int32Add(
        gasm_.Int32Constant(
            wasm::ObjectAccess::ToTagged(SeqOneByteString::kCharsOffset)),
        gasm_.Word32Shl(offset, charwidth_shift));
    gasm_.Goto(&done, string, gasm_.BuildChangeInt32ToIntPtr(final_offset),
               charwidth_shift);

    // External string.
    gasm_.Bind(&external);
    gasm_.GotoIf(
        gasm_.Word32And(instance_type,
                        gasm_.Int32Constant(kUncachedExternalStringMask)),
        &done, string, gasm_.IntPtrConstant(0),
        gasm_.Int32Constant(kCharWidthBailoutSentinel));
    Node* resource = gasm_.BuildLoadExternalPointerFromObject(
        string, ExternalString::kResourceDataOffset,
        kExternalStringResourceDataTag, gasm_.LoadRootRegister());
    Node* shifted_offset = gasm_.Word32Shl(offset, charwidth_shift);
    final_offset = gasm_.IntPtrAdd(
        resource, gasm_.BuildChangeInt32ToIntPtr(shifted_offset));
    gasm_.Goto(&done, gasm_.SmiConstant(0), final_offset, charwidth_shift);
  }

  gasm_.Bind(&done);
  Node* base = done.PhiAt(0);
  Node* final_offset = done.PhiAt(1);
  Node* charwidth_shift = done.PhiAt(2);

  Node* base_proj = NodeProperties::FindProjection(node, 0);
  Node* offset_proj = NodeProperties::FindProjection(node, 1);
  Node* charwidth_proj = NodeProperties::FindProjection(node, 2);
  if (base_proj) {
    ReplaceWithValue(base_proj, base, gasm_.effect(), gasm_.control());
    base_proj->Kill();
  }
  if (offset_proj) {
    ReplaceWithValue(offset_proj, final_offset, gasm_.effect(),
                     gasm_.control());
    offset_proj->Kill();
  }
  if (charwidth_proj) {
    ReplaceWithValue(charwidth_proj, charwidth_shift, gasm_.effect(),
                     gasm_.control());
    charwidth_proj->Kill();
  }

  // Wire up the dangling end of the new effect chain.
  ReplaceWithValue(node, node, gasm_.effect(), gasm_.control());

  node->Kill();
  return Replace(base);
}

void WasmGCLowering::UpdateSourcePosition(Node* new_node, Node* old_node) {
  if (source_position_table_) {
    SourcePosition position =
        source_position_table_->GetSourcePosition(old_node);
    if (position.ScriptOffset() != kNoSourcePosition) {
      source_position_table_->SetSourcePosition(new_node, position);
    } else {
      // TODO(mliedtke): Source positions are not yet supported for inlining
      // wasm into JS. Add support for it and replace the if with a DCHECK.
      DCHECK_EQ(kExplicitNullChecks, null_check_strategy_);
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
