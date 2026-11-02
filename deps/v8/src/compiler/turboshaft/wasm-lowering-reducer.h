// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_LOWERING_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_WASM_LOWERING_REDUCER_H_

#include "src/codegen/atomic-memory-order.h"
#include "src/compiler/turboshaft/builtin-call-descriptors.h"
#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/wasm-assembler-helpers.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class WasmLoweringReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(WasmLowering)

  V<Any> REDUCE(GlobalGet)(V<WasmTrustedInstanceData> instance,
                           const wasm::WasmGlobal* global) {
    return LowerGlobalSetOrGet(instance, OpIndex::Invalid(), global,
                               GlobalMode::kLoad);
  }

  OpIndex REDUCE(GlobalSet)(V<WasmTrustedInstanceData> instance, V<Any> value,
                            const wasm::WasmGlobal* global) {
    return LowerGlobalSetOrGet(instance, value, global, GlobalMode::kStore);
  }

  OpIndex REDUCE(RootConstant)(RootIndex index) {
    DCHECK(RootsTable::IsImmortalImmovable(index));
    return __ Load(__ LoadRootRegister(),
                   LoadOp::Kind::RawAligned().Immutable(),
                   MemoryRepresentation::AnyUncompressedTagged(),
                   IsolateData::root_slot_offset(index));
  }

  V<Word32> REDUCE(IsRootConstant)(OpIndex object, RootIndex index) {
#if V8_STATIC_ROOTS_BOOL
    if (RootsTable::IsReadOnly(index)) {
      V<Object> root = __ BitcastWordPtrToTagged(__ UintPtrConstant(
          StaticReadOnlyRootsPointerTable[static_cast<size_t>(index)]));
      return __ TaggedEqual(object, root);
    }
#endif
    return __ TaggedEqual(object, __ RootConstant(index));
  }

  OpIndex REDUCE(Null)(wasm::ValueType type) {
    RootIndex index =
        type.use_wasm_null() ? RootIndex::kWasmNull : RootIndex::kNullValue;
    return ReduceRootConstant(index);
  }

  V<Word32> REDUCE(IsNull)(OpIndex object, wasm::ValueType type) {
    RootIndex index =
        type.use_wasm_null() ? RootIndex::kWasmNull : RootIndex::kNullValue;
    return ReduceIsRootConstant(object, index);
  }

  V<Object> REDUCE(AssertNotNull)(V<Object> object,
                                  OptionalV<EagerFrameState> frame_state,
                                  wasm::ValueType type, TrapId trap_id) {
    if (trap_id == TrapId::kTrapNullDereference) {
      if (v8_flags.wasm_skip_null_checks) return object;

      // Use an explicit null check if
      // (1) we cannot use trap handler or
      // (2) the object might be a Smi (anyref, externref, exnref) or
      // (3) null checks for the object's type must check for JS null.
      bool use_explicit_check =
          null_check_strategy_ == NullCheckStrategy::kExplicit ||
          wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), type.AsNonShared(),
                            module_) ||
          wasm::IsSubtypeOf(wasm::kWasmExnRef.AsNonNull(), type.AsNonShared(),
                            module_) ||
          !type.use_wasm_null();
      if (!use_explicit_check) {
        LoadOp::Kind load_kind = LoadOp::Kind::TrapOnNull();
        if (type.is_shared()) load_kind = load_kind.SharedBase();
        if (!v8_flags.wasm_stringref ||
            !wasm::IsSubtypeOf(wasm::kWasmStringRef.AsNonNull(),
                               type.AsNonShared(), module_)) {
          load_kind = load_kind.Immutable();
        }
        __ Load(object, load_kind, MemoryRepresentation::TaggedPointer(),
                offsetof(HeapObject, map_));
        return object;
      }
    }
    __ TrapIf(__ IsNull(object, type), frame_state, trap_id);
    return object;
  }

  V<Map> REDUCE(RttCanon)(V<FixedArray> rtts,
                          wasm::ModuleTypeIndex type_index) {
    int map_offset =
        OFFSET_OF_DATA_START(FixedArray) + type_index.index * kTaggedSize;
    return __ Load(rtts, LoadOp::Kind::TaggedBase().Immutable(),
                   MemoryRepresentation::AnyTagged(), map_offset);
  }

  V<Word32> REDUCE(WasmTypeCheck)(V<Object> object, OptionalV<Map> rtt,
                                  WasmTypeCheckConfig config) {
    if (rtt.has_value()) {
      return ReduceWasmTypeCheckRtt(object, rtt, config);
    } else {
      return ReduceWasmTypeCheckAbstract(object, config);
    }
  }

  // A type check whose only use is the branch ending its block (br_on_cast,
  // br_on_cast_fail, or an `if` on a ref.test) is lowered directly into that
  // branch, rather than into a Phi of the results of the different checks
  // that the branch then tests: each comparison then ends up in the same block
  // as the branch that uses it, which the InstructionSelector can combine.
  V<Word32> REDUCE_INPUT_GRAPH(WasmTypeCheck)(V<Word32> ig_index,
                                              const WasmTypeCheckOp& check) {
    if (ShouldLowerIntoBranch(ig_index, check)) return V<Word32>::Invalid();
    return Next::ReduceInputGraphWasmTypeCheck(ig_index, check);
  }

  V<None> REDUCE_INPUT_GRAPH(Branch)(V<None> ig_index, const BranchOp& branch) {
    const WasmTypeCheckOp* check = __ input_graph()
                                       .Get(branch.condition())
                                       .template TryCast<WasmTypeCheckOp>();
    if (check == nullptr ||
        !ShouldLowerIntoBranch(branch.condition(), *check)) {
      return Next::ReduceInputGraphBranch(ig_index, branch);
    }
    // The type check was skipped when visiting it.
    DCHECK(!__ template MapToNewGraph<true>(branch.condition()).valid());
    BranchOnWasmTypeCheckRtt(__ MapToNewGraph(check->object()),
                             __ MapToNewGraph(check->rtt().value()),
                             check->config, __ MapToNewGraph(branch.if_true),
                             __ MapToNewGraph(branch.if_false), branch.hint);
    return V<None>::Invalid();
  }

  V<Object> REDUCE(WasmTypeCast)(V<Object> object, OptionalV<Map> rtt,
                                 OptionalV<EagerFrameState> frame_state,
                                 WasmTypeCheckConfig config) {
    if (rtt.has_value()) {
      return ReduceWasmTypeCastRtt(object, rtt, frame_state, config);
    } else {
      return ReduceWasmTypeCastAbstract(object, frame_state, config);
    }
  }

  V<Object> REDUCE(AnyConvertExtern)(V<Object> object, SharedFlag is_shared,
                                     bool is_nullable) {
    Label<Object> end_label(&Asm());
    Label<> null_label(&Asm());
    Label<> smi_label(&Asm());
    Label<> int_to_smi_label(&Asm());
    Label<> heap_number_label(&Asm());
    Label<> custom_map_wrapper_label(&Asm());

    if (is_nullable) {
      GOTO_IF(__ IsNull(object, wasm::kWasmExternRef), null_label);
    }
    GOTO_IF(__ IsSmi(object), smi_label);
    // We don't need to mark the object as potentially-shared because we put
    // TSan annotations on the any->extern conversion.
    GOTO_IF(__ HasInstanceType(object, HEAP_NUMBER_TYPE, SharedFlag{false}),
            heap_number_label);
    GOTO_IF(__ HasInstanceType(object, WASM_CUSTOM_MAP_WRAPPER_TYPE,
                               SharedFlag{false}),
            custom_map_wrapper_label);
    // For anything else, just pass through the value.
    GOTO(end_label, object);

    if (is_nullable) {
      BIND(null_label);
      GOTO(end_label, __ Null(wasm::kWasmAnyRef));
    }

    // Canonicalize SMI.
    BIND(smi_label);
    if constexpr (SmiValuesAre31Bits()) {
      GOTO(end_label, object);
    } else {
      Label<> convert_to_heap_number_label(&Asm());
      V<Word32> int_value = __ UntagSmi(V<Smi>::Cast(object));

      // Convert to heap number if the int32 does not fit into an i31ref.
      GOTO_IF(__ Int32LessThan(wasm::kInt31MaxValue, int_value),
              convert_to_heap_number_label);
      GOTO_IF(__ Int32LessThan(int_value, wasm::kInt31MinValue),
              convert_to_heap_number_label);
      GOTO(end_label, object);

      BIND(convert_to_heap_number_label);
      V<Object> heap_number =
          is_shared
              ? __ template CallWasmBuiltin<
                    builtin::WasmInt32ToSharedHeapNumber>({.value = int_value})
              : __ template CallWasmBuiltin<builtin::WasmInt32ToHeapNumber>(
                    {.value = int_value});
      GOTO(end_label, heap_number);
    }

    // Convert HeapNumber to SMI if possible.
    BIND(heap_number_label);
    V<Float64> float_value =
        __ LoadHeapNumberValue(V<HeapNumber>::Cast(object));
    // Check range of float value.
    GOTO_IF(__ Float64LessThan(float_value,
                               __ Float64Constant(wasm::kInt31MinValue)),
            end_label, object);
    GOTO_IF(__ Float64LessThan(__ Float64Constant(wasm::kInt31MaxValue),
                               float_value),
            end_label, object);
    // Check if value is -0.
    V<Word32> is_minus_zero;
    if constexpr (Is64()) {
      V<Word64> float_bits = __ BitcastFloat64ToWord64(float_value);
      is_minus_zero = __ Word64Equal(float_bits, kMinusZeroBits);
    } else {
      Label<Word32> done(&Asm());

      V<Word32> value_lo = __ Float64ExtractLowWord32(float_value);
      GOTO_IF_NOT(__ Word32Equal(value_lo, kMinusZeroLoBits), done, 0);
      V<Word32> value_hi = __ Float64ExtractHighWord32(float_value);
      GOTO(done, __ Word32Equal(value_hi, kMinusZeroHiBits));
      BIND(done, phi_is_minus_zero);
      is_minus_zero = phi_is_minus_zero;
    }
    GOTO_IF(is_minus_zero, end_label, object);
    // Check if value is integral.
    V<Word32> int_value =
        __ TruncateFloat64ToInt32OverflowUndefined(float_value);
    GOTO_IF(__ Float64Equal(float_value, __ ChangeInt32ToFloat64(int_value)),
            int_to_smi_label);
    GOTO(end_label, object);

    BIND(int_to_smi_label);
    GOTO(end_label, __ TagSmi(int_value));

    BIND(custom_map_wrapper_label);
    V<WasmCustomMap> custom_map =
        __ Load(object, LoadOp::Kind::TaggedBase().Immutable(),
                MemoryRepresentation::TaggedPointer(),
                offsetof(WasmCustomMapWrapper, wrapped_));
#if DEBUG
    if (v8_flags.debug_code) {
      IF_NOT (LIKELY(__ HasInstanceType(custom_map, WASM_CUSTOM_MAP_TYPE,
                                        SharedFlag{false}))) {
        if (__ data()->pipeline_kind() == TurboshaftPipelineKind::kWasm) {
          OpIndex message_id =
              __ SmiConstant(Smi::FromEnum(AbortReason::kOperandIsNotAMap));
          __ WasmCallRuntime(__ phase_zone(), Runtime::kAbort, {message_id},
                             __ NoContextConstant());
        } else {
          __ template CallRuntime<runtime::Abort>(
              __ NoContextConstant(),
              {.messageOrMessageId = __ SmiConstant(
                   Smi::FromEnum(AbortReason::kOperandIsNotAMap))});
        }
        __ Unreachable();
      }
    }
#endif  // DEBUG
    GOTO(end_label, custom_map);

    BIND(end_label, result);
    return result;
  }

  V<Object> REDUCE(ExternConvertAny)(V<Object> object, bool is_nullable) {
    Label<Object> end(&Asm());
    if (is_nullable) {
      IF (__ IsNull(object, wasm::kWasmAnyRef)) {
        GOTO(end, __ Null(wasm::kWasmExternRef));
      }
    }
    // TODO(jkummerow): Would be nice if we had type information to decide
    // which checks are necessary.
    // Perhaps move this lowering to the WasmGCTypedOptimizationReducer?
    GOTO_IF(__ IsSmi(object), end, object);
    SharedFlag maybe_shared{v8_flags.wasm_shared};
    GOTO_IF_NOT(__ HasInstanceType(object, WASM_CUSTOM_MAP_TYPE, maybe_shared),
                end, object);

    // For WasmCustomMaps, fetch or allocate their JS wrapper.
    {
      V<Object> maybe_wrapper = __ Load(object, LoadOp::Kind::TaggedBase(),
                                        MemoryRepresentation::TaggedPointer(),
                                        offsetof(WasmCustomMap, js_wrapper_));
      GOTO_IF_NOT(UNLIKELY(__ IsNull(maybe_wrapper, wasm::kWasmExternRef)), end,
                  maybe_wrapper);
      V<JSObject> wrapper;
      if (__ data()->pipeline_kind() == TurboshaftPipelineKind::kWasm) {
        wrapper =
            __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmExternConvertAny,
                               {object}, __ NoContextConstant());
      } else {
        wrapper = __ template CallRuntime<runtime::WasmExternConvertAny>(
            __ NoContextConstant(), {.object = object});
      }
      GOTO(end, wrapper);
    }
    BIND(end, result);
    return result;
  }

  V<Object> REDUCE(WasmTypeAnnotation)(V<Object> value, wasm::ValueType type) {
    // Remove type annotation operations as they are not needed any more.
    return value;
  }

  V<Any> REDUCE(StructGet)(V<WasmStructNullable> object,
                           OptionalV<EagerFrameState> frame_state,
                           const wasm::StructType* type,
                           wasm::ModuleTypeIndex type_index, int field_index,
                           bool is_signed, CheckForNull null_check,
                           std::optional<AtomicMemoryOrder> memory_order) {
    auto [explicit_null_check, implicit_null_check] =
        null_checks_for_struct_op(null_check, field_index);

    if (explicit_null_check) {
      __ TrapIf(__ IsNull(object, wasm::kWasmAnyRef), frame_state,
                TrapId::kTrapNullDereference);
    }

    LoadOp::Kind load_kind = implicit_null_check ? LoadOp::Kind::TrapOnNull()
                                                 : LoadOp::Kind::TaggedBase();
    if (type->is_shared()) load_kind = load_kind.SharedBase();

    if (field_index == StructGetOp::kDescFieldIndex) {
      // Can't use {LoadMapField} because that doesn't support specifying
      // {LoadOp::Kind::TrapOnNull}.
      V<Map> map = __ Load(object, load_kind.Immutable(),
                           MemoryRepresentation::TaggedPointer(),
                           offsetof(HeapObject, map_));
#if V8_MAP_PACKING
      UNIMPLEMENTED();
#endif
      if (type->is_shared()) UNIMPLEMENTED();
      if (v8_flags.wasm_merged_descriptors) return map;
      return __ Load(map, LoadOp::Kind::TaggedBase().Immutable(),
                     MemoryRepresentation::TaggedPointer(),
                     offsetof(Map, instance_descriptors_));
    }

    if (!type->mutability(field_index)) {
      load_kind = load_kind.Immutable();
    }
    if (memory_order.has_value()) {
      // TODO(mliedtke): Support acquire release semantics if specified as the
      // memory order.
      load_kind = load_kind.Atomic();
    }
    MemoryRepresentation repr =
        RepresentationFor(type->field(field_index), is_signed);

    return __ Load(object, load_kind, repr,
                   WasmStruct::FieldOffset(type, field_index));
  }

  V<None> REDUCE(StructSet)(V<WasmStructNullable> object, V<Any> value,
                            OptionalV<EagerFrameState> frame_state,
                            const wasm::StructType* type,
                            wasm::ModuleTypeIndex type_index, int field_index,
                            CheckForNull null_check,
                            std::optional<AtomicMemoryOrder> memory_order,
                            WriteBarrierKind write_barrier,
                            StructSetOp::Kind kind) {
    // TODO(rezvan): We do not support AcqRel memory order for non-memory
    // instructions currently.
    if (memory_order == AtomicMemoryOrder::kAcqRel) {
      memory_order = AtomicMemoryOrder::kSeqCst;
    }
    auto [explicit_null_check, implicit_null_check] =
        null_checks_for_struct_op(null_check, field_index);

    if (explicit_null_check) {
      __ TrapIf(__ IsNull(object, wasm::kWasmAnyRef), frame_state,
                TrapId::kTrapNullDereference);
    }

    StoreOp::Kind store_kind = implicit_null_check
                                   ? StoreOp::Kind::TrapOnNull()
                                   : StoreOp::Kind::TaggedBase();
    if (memory_order.has_value()) {
      store_kind = store_kind.Atomic();
    }
    MemoryRepresentation repr =
        RepresentationFor(type->field(field_index), true);

    // TODO(jkummerow): Be smarter when selecting the WriteBarrierKind:
    //  - if {value} is always an i31: kNoWriteBarrier
    //  - if {value} is never an i31: kPointerWriteBarrier
    // And apply the same logic to ArraySet.
    DCHECK_IMPLIES(write_barrier == kFullWriteBarrier,
                   type->field(field_index).is_ref());
    __ Store(object, value, store_kind, repr, write_barrier, memory_order,
             WasmStruct::FieldOffset(type, field_index), 0,
             kind == StructSetOp::Kind::kInitialize);

    return OpIndex::Invalid();
  }

  V<Word> REDUCE(StructAtomicRMW)(V<WasmStructNullable> object, OpIndex value,
                                  OptionalOpIndex expected,
                                  StructAtomicRMWOp::BinOp bin_op,
                                  const wasm::StructType* type,
                                  wasm::ModuleTypeIndex type_index,
                                  int field_index, CheckForNull null_check,
                                  AtomicMemoryOrder memory_order) {
    auto [explicit_null_check, implicit_null_check] =
        null_checks_for_struct_op(null_check, field_index);

    if (explicit_null_check) {
      __ TrapIf(__ IsNull(object, wasm::kWasmAnyRef),
                TrapId::kTrapNullDereference);
    }
    MemoryRepresentation repr =
        RepresentationFor(type->field(field_index), false);

    V<WordPtr> offset = __ WordPtrConstant(
        WasmStruct::FieldOffset(type, field_index) - kHeapObjectTag);
    MemoryAccessKind kind = implicit_null_check ? MemoryAccessKind::kTrapping
                                                : MemoryAccessKind::kNormal;
    if (bin_op == StructAtomicRMWOp::BinOp::kCompareExchange) {
      return __ AtomicCompareExchange(object, offset, expected.value(), value,
                                      repr.ToRegisterRepresentation(), repr,
                                      kind, RegisterRepresentation::Tagged());
    } else {
      return __ AtomicRMW(object, offset, value, bin_op,
                          repr.ToRegisterRepresentation(), repr, kind,
                          RegisterRepresentation::Tagged());
    }
  }

  // Converts the index of an array access for the address computation.
  // The index is less than the array length, which is less than 2^30 (see
  // {WasmArray::MaxLength}): the access passed its bounds check, or the
  // check was removed because it was shown to succeed. So the zero and
  // sign extensions of the index are equal (except with the test-only
  // --wasm-skip-bounds-checks). We use the zero extension, as for memory32
  // accesses: on x64 and arm64, 32-bit operations clear the upper half of
  // registers, so it is usually free. On riscv64, loong64 and mips64, which
  // keep 32-bit values sign-extended, it can cost an instruction.
  V<WordPtr> ArrayIndexToIntPtr(V<Word32> index) {
    return __ ChangeUint32ToUintPtr(index);
  }

  V<Any> REDUCE(ArrayGet)(V<WasmArrayNullable> array, V<Word32> index,
                          const wasm::ArrayType* array_type, bool is_signed,
                          std::optional<AtomicMemoryOrder> memory_order,
                          SharedFlag shared_base) {
    bool is_mutable = array_type->mutability();
    LoadOp::Kind load_kind = LoadOp::Kind::TaggedBase();
    if (!is_mutable) load_kind = load_kind.Immutable();
    if (memory_order.has_value()) load_kind = load_kind.Atomic();
    if (shared_base) load_kind = load_kind.SharedBase();
    return __ Load(array, ArrayIndexToIntPtr(index), load_kind,
                   RepresentationFor(array_type->element_type(), is_signed),
                   WasmArray::HeaderSize(array_type->is_shared()),
                   array_type->element_type().value_kind_size_log2());
  }

  V<None> REDUCE(ArraySet)(V<WasmArrayNullable> array, V<Word32> index,
                           V<Any> value, wasm::ValueType element_type,
                           SharedFlag is_shared,
                           std::optional<AtomicMemoryOrder> memory_order,
                           WriteBarrierKind write_barrier,
                           ArraySetOp::Kind kind) {
    // TODO(rezvan): We do not support AcqRel memory order for non-memory
    // instructions currently.
    if (memory_order == AtomicMemoryOrder::kAcqRel) {
      memory_order = AtomicMemoryOrder::kSeqCst;
    }
    StoreOp::Kind store_kind = StoreOp::Kind::TaggedBase();
    if (memory_order.has_value()) {
      store_kind = store_kind.Atomic();
    }
    DCHECK_IMPLIES(write_barrier == kFullWriteBarrier, element_type.is_ref());
    __ Store(array, ArrayIndexToIntPtr(index), value, store_kind,
             RepresentationFor(element_type, true), write_barrier, memory_order,
             WasmArray::HeaderSize(is_shared),
             element_type.value_kind_size_log2(),
             kind == ArraySetOp::Kind::kInitialize);
    return {};
  }

  OpIndex REDUCE(ArrayAtomicRMW)(V<WasmArrayNullable> array, V<Word32> index,
                                 OpIndex value, OptionalOpIndex expected,
                                 ArrayAtomicRMWOp::BinOp bin_op,
                                 wasm::ValueType element_type,
                                 SharedFlag is_shared,
                                 AtomicMemoryOrder memory_order) {
    MemoryRepresentation repr = RepresentationFor(element_type, false);
    V<WordPtr> index_scaled = __ WordPtrShiftLeft(
        ArrayIndexToIntPtr(index), element_type.value_kind_size_log2());
    V<WordPtr> offset = __ WordPtrAdd(
        index_scaled, WasmArray::HeaderSize(is_shared) - kHeapObjectTag);
    if (bin_op == StructAtomicRMWOp::BinOp::kCompareExchange) {
      return __ AtomicCompareExchange(array, offset, expected.value(), value,
                                      repr.ToRegisterRepresentation(), repr,
                                      MemoryAccessKind::kNormal,
                                      RegisterRepresentation::Tagged());
    } else {
      return __ AtomicRMW(
          array, offset, value, bin_op, repr.ToRegisterRepresentation(), repr,
          MemoryAccessKind::kNormal, RegisterRepresentation::Tagged());
    }
  }

  V<Word32> REDUCE(ArrayLength)(V<WasmArrayNullable> array,
                                OptionalV<EagerFrameState> frame_state,
                                CheckForNull null_check,
                                SharedFlag shared_base) {
    bool explicit_null_check =
        null_check == kWithNullCheck &&
        null_check_strategy_ == NullCheckStrategy::kExplicit;
    bool implicit_null_check =
        null_check == kWithNullCheck &&
        null_check_strategy_ == NullCheckStrategy::kTrapHandler;

    if (explicit_null_check) {
      __ TrapIf(__ IsNull(array, wasm::kWasmAnyRef), frame_state,
                TrapId::kTrapNullDereference);
    }

    LoadOp::Kind load_kind = implicit_null_check
                                 ? LoadOp::Kind::TrapOnNull().Immutable()
                                 : LoadOp::Kind::TaggedBase().Immutable();
    if (shared_base) load_kind = load_kind.SharedBase();

    return __ Load(array, load_kind, RepresentationFor(wasm::kWasmI32, true),
                   offsetof(WasmArray, length_));
  }

  V<WasmArray> REDUCE(WasmAllocateArray)(V<Map> rtt, V<Word32> length,
                                         const wasm::ArrayType* array_type) {
    __ TrapIfNot(
        __ Uint32LessThanOrEqual(length, WasmArray::MaxLength(array_type)),
        TrapId::kTrapArrayTooLarge);
    wasm::ValueType element_type = array_type->element_type();
    SharedFlag is_shared = array_type->is_shared();

    // RoundUp(length * value_size, kObjectAlignment) =
    //   RoundDown(length * value_size + kObjectAlignment - 1,
    //             kObjectAlignment);
    V<Word32> padded_length = __ Word32BitwiseAnd(
        __ Word32Add(__ Word32Mul(length, element_type.value_kind_size()),
                     int32_t{kObjectAlignment - 1}),
        int32_t{-kObjectAlignment});
    Uninitialized<WasmArray> a = __ template Allocate<WasmArray>(
        __ ChangeUint32ToUintPtr(
            __ Word32Add(padded_length, WasmArray::HeaderSize(is_shared))),
        is_shared ? AllocationType::kSharedOld : AllocationType::kYoung,
        is_shared ? kDoubleAligned : kTaggedAligned);

    // TODO(14108): The map and empty fixed array initialization should be an
    // immutable store.
    __ InitializeField(
        a,
        AccessBuilder::ForMap(
            is_shared ? compiler::kMapWriteBarrier : compiler::kNoWriteBarrier,
            is_shared /* not used for stores */),
        rtt);
    __ InitializeField(a, AccessBuilder::ForJSObjectPropertiesOrHash(),
                       __ RootConstant(RootIndex::kEmptyFixedArray));
    __ InitializeField(a, AccessBuilder::ForWasmArrayLength(), length);

    // Note: Only the array header initialization is finished here, the elements
    // still need to be initialized by other code.
    V<WasmArray> array = __ FinishInitialization(std::move(a));
    return array;
  }

  V<WasmStruct> REDUCE(WasmAllocateStruct)(V<Map> rtt,
                                           const wasm::StructType* struct_type,
                                           wasm::ModuleTypeIndex type_index) {
    // Only the WasmAllocateDescriptorStruct builtin can allocate descriptors.
    DCHECK(!struct_type->is_descriptor());
    int size = WasmStruct::Size(struct_type);
    Uninitialized<WasmStruct> s = __ template Allocate<WasmStruct>(
        size,
        struct_type->is_shared() ? AllocationType::kSharedOld
                                 : AllocationType::kYoung,
        struct_type->is_shared() ? kDoubleAligned : kTaggedAligned);
    // Objects allocated into old-space need a write barrier for initialization.
    __ InitializeField(s,
                       AccessBuilder::ForMap(
                           struct_type->is_shared() ? compiler::kMapWriteBarrier
                                                    : compiler::kNoWriteBarrier,
                           struct_type->is_shared() /* not used for stores */),
                       rtt);
    __ InitializeField(s, AccessBuilder::ForJSObjectPropertiesOrHash(),
                       __ RootConstant(RootIndex::kEmptyFixedArray));
    // Note: Struct initialization isn't finished here, the user defined fields
    // still need to be initialized by other operations.
    V<WasmStruct> struct_value = __ FinishInitialization(std::move(s));
    return struct_value;
  }

  V<WasmFuncRef> REDUCE(WasmRefFunc)(V<WasmTrustedInstanceData> wasm_instance,
                                     uint32_t function_index) {
    V<FixedArray> func_refs = LOAD_IMMUTABLE_INSTANCE_FIELD(
        wasm_instance, FuncRefs, MemoryRepresentation::TaggedPointer());
    V<Object> maybe_func_ref =
        __ LoadFixedArrayElement(func_refs, function_index);

    Label<WasmFuncRef> done(&Asm());
    IF (UNLIKELY(__ IsSmi(maybe_func_ref))) {
      V<WasmFuncRef> from_builtin =
          __ template CallWasmBuiltin<builtin::WasmRefFunc>(
              {.wasm_instance = wasm_instance,
               .function_index = __ Word32Constant(function_index)});

      GOTO(done, from_builtin);
    } ELSE {
      GOTO(done, V<WasmFuncRef>::Cast(maybe_func_ref));
    }

    BIND(done, result_value);
    return result_value;
  }

  V<String> REDUCE(StringAsWtf16)(V<String> string) {
    Label<String> done(&Asm());
    V<Word32> instance_type = __ LoadInstanceTypeField(__ LoadMapField(string));
    V<Word32> string_representation =
        __ Word32BitwiseAnd(instance_type, kStringRepresentationMask);
    GOTO_IF(__ Word32Equal(string_representation, kSeqStringTag), done, string);

    GOTO(done,
         __ template WasmCallBuiltinThroughJumptable<
             deprecated::BuiltinCallDescriptor::WasmStringAsWtf16>({string}));
    BIND(done, result);
    return result;
  }

  OpIndex REDUCE(StringPrepareForGetCodeUnit)(V<Object> original_string) {
    LoopLabel<Object /*string*/, Word32 /*instance type*/, Word32 /*offset*/>
        dispatch(&Asm());
    Label<Object /*string*/, Word32 /*instance type*/, Word32 /*offset*/>
        direct_string(&Asm());

    // These values will be used to replace the original node's projections.
    // The first, "string", is either a SeqString or Tagged<Smi>(0) (in case of
    // external string). Notably this makes it GC-safe: if that string moves,
    // this pointer will be updated accordingly. The second, "offset", has full
    // register width so that it can be used to store external pointers: for
    // external strings, we add up the character backing store's base address
    // and any slice offset. The third, "character width", is a shift width,
    // i.e. it is 0 for one-byte strings, 1 for two-byte strings,
    // kCharWidthBailoutSentinel for uncached external strings (for which
    // "string"/"offset" are invalid and unusable).
    Label<Object /*string*/, WordPtr /*offset*/, Word32 /*character width*/>
        done(&Asm());

    V<Word32> original_type =
        __ LoadInstanceTypeField(__ LoadMapField(original_string));
    GOTO(dispatch, original_string, original_type, 0);

    BIND_LOOP(dispatch, string, instance_type, offset) {
      Label<> thin_string(&Asm());
      Label<> cons_string(&Asm());

      static_assert(kIsIndirectStringTag == 1);
      static constexpr int kIsDirectStringTag = 0;
      GOTO_IF(__ Word32Equal(
                  __ Word32BitwiseAnd(instance_type, kIsIndirectStringMask),
                  kIsDirectStringTag),
              direct_string, string, instance_type, offset);

      // Handle indirect strings.
      V<Word32> string_representation =
          __ Word32BitwiseAnd(instance_type, kStringRepresentationMask);
      GOTO_IF(__ Word32Equal(string_representation, kThinStringTag),
              thin_string);
      GOTO_IF(__ Word32Equal(string_representation, kConsStringTag),
              cons_string);

      // Sliced string.
      V<Word32> new_offset = __ Word32Add(
          offset, __ UntagSmi(__ template LoadField<Smi>(
                      string, AccessBuilder::ForSlicedStringOffset())));
      V<Object> parent = __ template LoadField<Object>(
          string, AccessBuilder::ForSlicedStringParent());
      V<Word32> parent_type = __ LoadInstanceTypeField(__ LoadMapField(parent));
      GOTO(dispatch, parent, parent_type, new_offset);

      // Thin string.
      BIND(thin_string);
      V<Object> actual = __ template LoadField<Object>(
          string, AccessBuilder::ForThinStringActual());
      V<Word32> actual_type = __ LoadInstanceTypeField(__ LoadMapField(actual));
      // ThinStrings always reference (internalized) direct strings.
      GOTO(direct_string, actual, actual_type, offset);

      // Flat cons string. (Non-flat cons strings are ruled out by
      // string.as_wtf16.)
      BIND(cons_string);
      V<Object> first = __ template LoadField<Object>(
          string, AccessBuilder::ForConsStringFirst());
      V<Word32> first_type = __ LoadInstanceTypeField(__ LoadMapField(first));
      GOTO(dispatch, first, first_type, offset);
    }
    {
      BIND(direct_string, string, instance_type, offset);

      V<Word32> is_onebyte =
          __ Word32BitwiseAnd(instance_type, kStringEncodingMask);
      // Char width shift is 1 - (is_onebyte).
      static_assert(kStringEncodingMask == 1 << 3);
      V<Word32> charwidth_shift =
          __ Word32Sub(1, __ Word32ShiftRightLogical(is_onebyte, 3));

      Label<> external(&Asm());
      V<Word32> string_representation =
          __ Word32BitwiseAnd(instance_type, kStringRepresentationMask);
      GOTO_IF(__ Word32Equal(string_representation, kExternalStringTag),
              external);

      // Sequential string.
      DCHECK_EQ(AccessBuilder::ForSeqOneByteStringCharacter().header_size,
                AccessBuilder::ForSeqTwoByteStringCharacter().header_size);
      const int chars_start_offset =
          AccessBuilder::ForSeqOneByteStringCharacter().header_size;
      V<Word32> final_offset =
          __ Word32Add(chars_start_offset - kHeapObjectTag,
                       __ Word32ShiftLeft(offset, charwidth_shift));
      GOTO(done, string, __ ChangeInt32ToIntPtr(final_offset), charwidth_shift);

      // External string.
      BIND(external);
      GOTO_IF(__ Word32BitwiseAnd(instance_type, kUncachedExternalStringMask),
              done, string, /*offset*/ 0, kCharWidthBailoutSentinel);
      FieldAccess field_access = AccessBuilder::ForExternalStringResourceData();
      V<WordPtr> resource = __ LoadExternalPointerFromObject(
          string, field_access.offset, field_access.external_pointer_tag);
      V<Word32> shifted_offset = __ Word32ShiftLeft(offset, charwidth_shift);
      V<WordPtr> final_offset_external =
          __ WordPtrAdd(resource, __ ChangeInt32ToIntPtr(shifted_offset));
      GOTO(done, __ SmiConstant(Smi::FromInt(0)), final_offset_external,
           charwidth_shift);
    }
    {
      BIND(done, base, final_offset, charwidth_shift);
      return __ MakeTuple({base, final_offset, charwidth_shift});
    }
  }

 private:
  enum class GlobalMode { kLoad, kStore };

  static constexpr MemoryRepresentation kMaybeSandboxedPointer =
      V8_ENABLE_SANDBOX_BOOL ? MemoryRepresentation::SandboxedPointer()
                             : MemoryRepresentation::UintPtr();

  MemoryRepresentation RepresentationFor(wasm::ValueType type, bool is_signed) {
    switch (type.kind()) {
      case wasm::kI8:
        return is_signed ? MemoryRepresentation::Int8()
                         : MemoryRepresentation::Uint8();
      case wasm::kI16:
        return is_signed ? MemoryRepresentation::Int16()
                         : MemoryRepresentation::Uint16();
      case wasm::kI32:
        return is_signed ? MemoryRepresentation::Int32()
                         : MemoryRepresentation::Uint32();
      case wasm::kI64:
        return is_signed ? MemoryRepresentation::Int64()
                         : MemoryRepresentation::Uint64();
      case wasm::kF16:
        return MemoryRepresentation::Float16();
      case wasm::kF32:
        return MemoryRepresentation::Float32();
      case wasm::kF64:
        return MemoryRepresentation::Float64();
      case wasm::kS128:
        return MemoryRepresentation::Simd128();
      case wasm::kRef:
      case wasm::kRefNull:
        return MemoryRepresentation::AnyTagged();
      case wasm::kVoid:
      case wasm::kTop:
      case wasm::kBottom:
        UNREACHABLE();
    }
    UNREACHABLE();
  }

  V<Word32> ObjectIsUnshared(V<HeapObject> object) {
    V<WordPtr> flags = __ LoadPageFlags(object);
    V<WordPtr> is_shared = __ WordPtrBitwiseAnd(
        flags, static_cast<uintptr_t>(MemoryChunk::IN_WRITABLE_SHARED_SPACE));
    return __ WordPtrEqual(is_shared, 0);
  }

  void RejectSharedWasmObjectsIfUnshared(V<HeapObject> object,
                                         WasmTypeCheckConfig config,
                                         Label<Word32>& result_label) {
    if (!v8_flags.wasm_shared || config.to.is_shared() ||
        config.from.AsNullable() != wasm::kWasmAnyRef) {
      return;
    }
    GOTO_IF_NOT(LIKELY(ObjectIsUnshared(object)), result_label,
                __ Word32Constant(0));
  }

  V<Word32> ReduceWasmTypeCheckAbstract(V<Object> object,
                                        WasmTypeCheckConfig config) {
    const bool object_can_be_i31 =
        wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(),
                          config.from.AsNonShared(), module_) ||
        config.from.is_reference_to(wasm::GenericKind::kExtern);
    // anyref may contain shared objects. However, since we reject shared
    // objects before accessing object fields in that case, we can just pass
    // `config.from.is_shared()` as `shared_base`.
    SharedFlag shared_base =
        SharedFlag{v8_flags.wasm_shared && config.from.is_shared()};

    V<Word32> result;
    Label<Word32> end_label(&Asm());

    DCHECK(config.to.is_abstract_ref());
    wasm::GenericKind to_kind = config.to.generic_kind();
    do {
      // The none-types only perform a null check. They need no control flow.
      if (IsNullKind(to_kind)) {
        result = __ IsNull(object, config.from);
        break;
      }
      // Null checks performed by any other type check need control flow. We can
      // skip the null check if success is determined by a Smi check.
      if (config.from.is_nullable() && config.to != wasm::kWasmRefI31) {
        const int kResult = config.to.is_nullable() ? 1 : 0;
        GOTO_IF(UNLIKELY(__ IsNull(object, config.from)), end_label,
                __ Word32Constant(kResult));
      }
      // i31 is special in that the Smi check is the last thing to do.
      if (to_kind == wasm::GenericKind::kI31) {
        // If earlier optimization passes reached the limit of possible graph
        // transformations, we could DCHECK(object_can_be_i31) here.
        result = object_can_be_i31 ? __ IsSmi(object) : __ Word32Constant(0);
        break;
      }
      if (to_kind == wasm::GenericKind::kEq) {
        if (object_can_be_i31) {
          GOTO_IF(UNLIKELY(__ IsSmi(object)), end_label, 1);
        }
        RejectSharedWasmObjectsIfUnshared(V<HeapObject>::Cast(object), config,
                                          end_label);
        // TODO(jkummerow): Here and elsewhere in this file, use
        // {LoadImmutableMapField} whenever the input is guaranteed to be a
        // Wasm object (i.e. not a string).
        RejectNonWasmObject(__ LoadMapField(V<HeapObject>::Cast(object)),
                            end_label);
        GOTO(end_label, 1);
        break;
      }
      // array, struct, string: i31 fails.
      if (object_can_be_i31) {
        GOTO_IF(UNLIKELY(__ IsSmi(object)), end_label, 0);
      }
      if (to_kind == wasm::GenericKind::kArray) {
        RejectSharedWasmObjectsIfUnshared(V<HeapObject>::Cast(object), config,
                                          end_label);
        result = __ HasInstanceType(object, WASM_ARRAY_TYPE, shared_base);
        break;
      }
      if (to_kind == wasm::GenericKind::kStruct) {
        RejectSharedWasmObjectsIfUnshared(V<HeapObject>::Cast(object), config,
                                          end_label);
        GOTO_IF(
            LIKELY(__ HasInstanceType(object, WASM_STRUCT_TYPE, shared_base)),
            end_label, 1);
        // Only the first access to an object needs to set the right
        // right {shared_base} as long as that's only used for TSan, but let's
        // be consistent and future-proof.
        result = __ HasInstanceType(object, WASM_CUSTOM_MAP_TYPE, shared_base);
        break;
      }
      if (to_kind == wasm::GenericKind::kString ||
          to_kind == wasm::GenericKind::kExternString) {
        // TODO(manoskouk): Refine `shared_base` if needed.
        V<Word32> instance_type =
            __ LoadInstanceTypeField(__ LoadMapField(object, shared_base));
        result = __ Uint32LessThan(instance_type, FIRST_NONSTRING_TYPE);
        break;
      }
      UNREACHABLE();
    } while (false);

    DCHECK(__ generating_unreachable_operations() || result.valid());
    GOTO(end_label, result);
    BIND(end_label, final_result);
    return final_result;
  }

  void TrapOnSharedWasmObjectsIfUnshared(
      V<HeapObject> object, WasmTypeCheckConfig config,
      OptionalV<EagerFrameState> frame_state) {
    if (!v8_flags.wasm_shared || config.to.is_shared() ||
        config.from.AsNullable() != wasm::kWasmAnyRef) {
      return;
    }
    __ TrapIfNot(ObjectIsUnshared(object), frame_state,
                 TrapId::kTrapIllegalCast);
  }

  V<Object> ReduceWasmTypeCastAbstract(V<Object> object,
                                       OptionalV<EagerFrameState> frame_state,
                                       WasmTypeCheckConfig config) {
    const bool object_can_be_i31 =
        wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(),
                          config.from.AsNonShared(), module_) ||
        config.from.is_reference_to(wasm::GenericKind::kExtern);
    // anyref may contain shared objects. However, since we reject shared
    // objects before accessing object fields in that case, we can just pass
    // `config.from.is_shared()` as `shared_base`.
    SharedFlag shared_base =
        SharedFlag{v8_flags.wasm_shared && config.from.is_shared()};

    Label<> end_label(&Asm());

    DCHECK(config.to.is_abstract_ref());
    wasm::GenericKind to_kind = config.to.generic_kind();

    do {
      // The none-types only perform a null check.
      if (wasm::IsNullKind(to_kind)) {
        __ TrapIfNot(__ IsNull(object, config.from), frame_state,
                     TrapId::kTrapIllegalCast);
        break;
      }
      // Null checks performed by any other type cast can only be skipped if
      // a Smi check is the only operation we'll need (i.e. the target type
      // is non-nullable (ref i31).
      if (config.from.is_nullable() && config.to != wasm::kWasmRefI31) {
        V<Word32> is_null = __ IsNull(object, config.from);
        if (config.to.is_nullable()) {
          GOTO_IF(UNLIKELY(is_null), end_label);
        } else if (!v8_flags.wasm_skip_null_checks) {
          // TODO(jkummerow): Consider using a trapping map load instead, and
          // adjusting its message to be "illegal cast" instead of "null deref".
          __ TrapIf(is_null, frame_state, TrapId::kTrapIllegalCast);
        }
      }
      if (to_kind == wasm::GenericKind::kI31) {
        // If earlier optimization passes reached the limit of possible graph
        // transformations, we could DCHECK(object_can_be_i31) here.
        V<Word32> success =
            object_can_be_i31 ? __ IsSmi(object) : __ Word32Constant(0);
        __ TrapIfNot(success, frame_state, TrapId::kTrapIllegalCast);
        break;
      }
      if (to_kind == wasm::GenericKind::kEq) {
        if (object_can_be_i31) {
          GOTO_IF(UNLIKELY(__ IsSmi(object)), end_label);
        }
        TrapOnSharedWasmObjectsIfUnshared(V<HeapObject>::Cast(object), config,
                                          frame_state);
        TrapOnNonWasmObject(
            __ LoadMapField(V<HeapObject>::Cast(object), shared_base),
            frame_state);
        break;
      }
      // array, struct, string: i31 fails.
      if (object_can_be_i31) {
        __ TrapIf(__ IsSmi(object), frame_state, TrapId::kTrapIllegalCast);
      }
      if (to_kind == wasm::GenericKind::kArray) {
        TrapOnSharedWasmObjectsIfUnshared(V<HeapObject>::Cast(object), config,
                                          frame_state);
        // Same as above.
        __ TrapIfNot(__ HasInstanceType(object, WASM_ARRAY_TYPE, shared_base),
                     frame_state, TrapId::kTrapIllegalCast);
        break;
      }
      if (to_kind == wasm::GenericKind::kStruct) {
        TrapOnSharedWasmObjectsIfUnshared(V<HeapObject>::Cast(object), config,
                                          frame_state);
        Label<> fallthrough(&Asm());
        GOTO_IF(
            LIKELY(__ HasInstanceType(object, WASM_STRUCT_TYPE, shared_base)),
            fallthrough);
        __ TrapIfNot(
            __ HasInstanceType(object, WASM_CUSTOM_MAP_TYPE, shared_base),
            frame_state, TrapId::kTrapIllegalCast);
        GOTO(fallthrough);
        BIND(fallthrough);
        break;
      }
      if (to_kind == wasm::GenericKind::kString ||
          to_kind == wasm::GenericKind::kExternString) {
        // TODO(manoskouk): Refine `shared_base` if needed.
        V<Word32> instance_type =
            __ LoadInstanceTypeField(__ LoadMapField(object, shared_base));
        __ TrapIfNot(__ Uint32LessThan(instance_type, FIRST_NONSTRING_TYPE),
                     frame_state, TrapId::kTrapIllegalCast);
        break;
      }
      UNREACHABLE();
    } while (false);

    GOTO(end_label);
    BIND(end_label);
    return object;
  }

  V<Object> LoadImmediateSuperRTT(V<Map> map) {
    return __ Load(map, LoadOp::Kind::TaggedBase().Immutable(),
                   MemoryRepresentation::TaggedPointer(),
                   offsetof(Map, dependent_code_));
  }

  V<Object> ReduceWasmTypeCastRtt(V<Object> object, OptionalV<Map> rtt,
                                  OptionalV<EagerFrameState> frame_state,
                                  WasmTypeCheckConfig config) {
    DCHECK(rtt.has_value());
    Label<> end_label(&Asm());
    // TODO(jkummerow): Consider using a trapping map load instead of the null
    // check, and adjusting its message to be "illegal cast" instead of "null
    // deref".
    EmitWasmTypeCheckRtt(
        object, rtt.value(), config, v8_flags.wasm_skip_null_checks,
        [this, frame_state, &end_label](ConditionWithHint condition,
                                        EarlyExitConfig early_exit) {
          if (early_exit.result_value) {
            if (early_exit.negated) {
              GOTO_IF_NOT(condition, end_label);
            } else {
              GOTO_IF(condition, end_label);
            }
          } else if (early_exit.negated) {
            __ TrapIfNot(condition.condition(), frame_state,
                         TrapId::kTrapIllegalCast);
          } else {
            __ TrapIf(condition.condition(), frame_state,
                      TrapId::kTrapIllegalCast);
          }
        },
        [this, frame_state, &end_label](V<Word32> condition) {
          __ TrapIfNot(condition, frame_state, TrapId::kTrapIllegalCast);
          GOTO(end_label);
        });
    BIND(end_label);
    return object;
  }

  V<Word32> ReduceWasmTypeCheckRtt(V<Object> object, OptionalV<Map> rtt,
                                   WasmTypeCheckConfig config) {
    DCHECK(rtt.has_value());
    Label<Word32> end_label(&Asm());
    EmitWasmTypeCheckRtt(
        object, rtt.value(), config, false,
        [this, &end_label](ConditionWithHint condition,
                           EarlyExitConfig early_exit) {
          if (early_exit.negated) {
            GOTO_IF_NOT(condition, end_label, early_exit.result_value);
          } else {
            GOTO_IF(condition, end_label, early_exit.result_value);
          }
        },
        [this, &end_label](V<Word32> condition) {
          GOTO(end_label, condition);
        });
    BIND(end_label, result);
    return result;
  }

  // An early exit of a type check: when its condition is true (or false, if
  // {negated}), the result of the type check is {result_value}.
  struct EarlyExitConfig {
    bool negated;
    bool result_value;
  };

  // Checks that {map} is the map of a Wasm object (one of the Wasm object
  // instance types, or a WasmCustomMap); otherwise, the type check fails: this
  // early exit is passed to {early_exit_handler} (see EmitWasmTypeCheckRtt).
  template <typename EarlyExitHandler>
  void EmitWasmObjectCheck(V<Map> map, EarlyExitHandler&& early_exit_handler) {
    V<Word32> instance_type = __ LoadInstanceTypeField(map);
    // We're going to test a range of WasmObject instance types with a single
    // unsigned comparison.
    V<Word32> comparison_value =
        __ Word32Sub(instance_type, FIRST_WASM_OBJECT_TYPE);
    IF (UNLIKELY(
            __ Uint32LessThan(LAST_WASM_OBJECT_TYPE - FIRST_WASM_OBJECT_TYPE,
                              comparison_value))) {
      early_exit_handler(
          LIKELY(__ Word32Equal(comparison_value,
                                WASM_CUSTOM_MAP_TYPE - FIRST_WASM_OBJECT_TYPE)),
          {.negated = true, .result_value = false});
    }
  }

  // Emits the checks of a type check or cast against {rtt}. Each check that
  // can determine the result early is passed to {early_exit_handler}, with its
  // condition and an EarlyExitConfig. The last comparison, which determines
  // the result otherwise, is passed to {result_handler}.
  // With {skip_failing_null_check}, there is no check for null when null
  // fails the type check (for casts with --wasm-skip-null-checks).
  template <typename EarlyExitHandler, typename ResultHandler>
  void EmitWasmTypeCheckRtt(V<Object> object, V<Map> rtt,
                            WasmTypeCheckConfig config,
                            bool skip_failing_null_check,
                            EarlyExitHandler early_exit_handler,
                            ResultHandler result_handler) {
    int rtt_depth = wasm::GetSubtypingDepth(module_, config.to.ref_index());
    bool object_can_be_i31 = wasm::IsSubtypeOf(
        wasm::kWasmI31Ref.AsNonNull(), config.from.AsNonShared(), module_);
    bool is_cast_from_any =
        config.from.is_reference_to(wasm::GenericKind::kAny);
    // Non-shared anyref may contain shared objects.
    SharedFlag shared_base =
        SharedFlag{v8_flags.wasm_shared &&
                   (config.from.is_shared() ||
                    config.from.is_reference_to(wasm::GenericKind::kAny))};

    if (config.from.is_nullable() &&
        (config.to.is_nullable() || !skip_failing_null_check)) {
      early_exit_handler(
          UNLIKELY(__ IsNull(object, wasm::kWasmAnyRef)),
          {.negated = false, .result_value = config.to.is_nullable()});
    }

    if (object_can_be_i31) {
      early_exit_handler(__ IsSmi(object),
                         {.negated = false, .result_value = false});
    }

    V<Map> map = __ LoadMapField(object, shared_base);

    DCHECK_IMPLIES(module_->type(config.to.ref_index()).is_final,
                   config.exactness != kMayBeSubtype);

    if (config.exactness == kExactMatchOnly) {
      result_handler(__ TaggedEqual(map, rtt));
    } else if (config.exactness == kExactMatchLastSupertype) {
      // This only used for custom descriptors, and only structs can have them.
      DCHECK_EQ(config.to.ref_type_kind(), wasm::RefTypeKind::kStruct);
      // Check if map instance type identifies a wasm object.
      if (is_cast_from_any) {
        EmitWasmObjectCheck(map, early_exit_handler);
      }
      V<Object> maybe_match = LoadImmediateSuperRTT(map);
      result_handler(__ TaggedEqual(maybe_match, rtt));
    } else {
      DCHECK_EQ(config.exactness, kMayBeSubtype);
      // First, check if types happen to be equal. This has been shown to give
      // large speedups.
      early_exit_handler(LIKELY(__ TaggedEqual(map, rtt)),
                         {.negated = false, .result_value = true});

      // Check if map instance type identifies a wasm object.
      if (is_cast_from_any) {
        EmitWasmObjectCheck(map, early_exit_handler);
      }

      V<Object> type_info = __ LoadWasmTypeInfo(map);
      DCHECK_GE(rtt_depth, 0);
      // If the depth of the rtt is known to be less that the minimum supertype
      // array length, we can access the supertype without bounds-checking the
      // supertype array.
      if (static_cast<uint32_t>(rtt_depth) >=
          wasm::kMinimumSupertypeArraySize) {
        V<Word32> supertypes_length = __ UntagSmi(
            __ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                    MemoryRepresentation::TaggedSigned(),
                    offsetof(WasmTypeInfo, supertypes_length_)));
        early_exit_handler(
            LIKELY(__ Uint32LessThan(rtt_depth, supertypes_length)),
            {.negated = true, .result_value = false});
      }

      V<Object> maybe_match =
          __ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                  MemoryRepresentation::TaggedPointer(),
                  WasmTypeInfo::kSupertypesOffset + kTaggedSize * rtt_depth);

      result_handler(__ TaggedEqual(maybe_match, rtt));
    }
  }

  bool ShouldLowerIntoBranch(OpIndex ig_index, const WasmTypeCheckOp& check) {
    if (!check.rtt().has_value() || !check.saturated_use_count.Is(1)) {
      return false;
    }
    // The type check and the branch must be in the same block: this is the
    // only case where the branch using the type check can be found cheaply (it
    // ends the block). This function is called twice: when visiting the type
    // check, to decide whether to skip it, and when visiting the branch, to
    // decide whether to lower the type check into it. Without this condition,
    // a type check in a previous block would be emitted twice: in value form
    // when visited (a dead copy, left for dead-code elimination to remove),
    // and again in the branch.
    const Block* block = __ current_input_block();
    if (!block->Contains(ig_index)) return false;
    const BranchOp* branch =
        block->LastOperation(__ input_graph()).template TryCast<BranchOp>();
    return branch != nullptr && branch->condition() == ig_index;
  }

  // Same checks as ReduceWasmTypeCheckRtt, jumping to {if_true} or {if_false}
  // instead of producing a value.
  void BranchOnWasmTypeCheckRtt(V<Object> object, V<Map> rtt,
                                WasmTypeCheckConfig config, Block* if_true,
                                Block* if_false, BranchHint hint) {
    EmitWasmTypeCheckRtt(
        object, rtt, config, false,
        [this, if_true, if_false](ConditionWithHint condition,
                                  EarlyExitConfig early_exit) {
          Block* target = early_exit.result_value ? if_true : if_false;
          if (early_exit.negated) {
            __ GotoIfNot(condition, target);
          } else {
            __ GotoIf(condition, target);
          }
        },
        [this, if_true, if_false, hint](V<Word32> condition) {
          __ Branch(ConditionWithHint(condition, hint), if_true, if_false);
        });
  }

  OpIndex LowerGlobalSetOrGet(V<WasmTrustedInstanceData> instance, V<Any> value,
                              const wasm::WasmGlobal* global, GlobalMode mode) {
    bool is_mutable = global->mutability;
    DCHECK_IMPLIES(!is_mutable, mode == GlobalMode::kLoad);
    if (is_mutable && global->imported) {
      V<FixedArray> buffers =
          LOAD_IMMUTABLE_INSTANCE_FIELD(instance, ImportedMutableGlobalsBuffers,
                                        MemoryRepresentation::TaggedPointer());
      V<FixedArray> offsets =
          LOAD_IMMUTABLE_INSTANCE_FIELD(instance, ImportedMutableGlobalsOffsets,
                                        MemoryRepresentation::TaggedPointer());
      V<WasmGlobalObject::BufferType> buffer = __ Load(
          buffers, LoadOp::Kind::TaggedBase().Immutable(),
          MemoryRepresentation::AnyTagged(),
          FixedArray::OffsetOfElementAt(global->mutable_imported_global_index));
      V<WordPtr> offset_from_tagged_buffer = __ ChangeUint32ToUintPtr(
          __ Load(offsets, OpIndex::Invalid(), LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::Uint32(),
                  FixedUInt32Array::OffsetOfElementAt(
                      global->mutable_imported_global_index)));
      // Note: We need to use `{Load,Store}Op::Kind::TaggedBase()` below for
      // correct typing, but that automatically subtracts `kHeapObjectTag` so we
      // explicitly add it back as immediate offset.
      // This results in a more compact memory operand (no immediate).
      if (mode == GlobalMode::kLoad) {
        return __ Load(buffer, offset_from_tagged_buffer,
                       LoadOp::Kind::TaggedBase(),
                       RepresentationFor(global->type, true), kHeapObjectTag);
      } else {
        __ Store(buffer, offset_from_tagged_buffer, value,
                 StoreOp::Kind::TaggedBase(),
                 RepresentationFor(global->type, true),
                 global->type.is_ref() ? WriteBarrierKind::kFullWriteBarrier
                                       : WriteBarrierKind::kNoWriteBarrier,
                 kHeapObjectTag);
      }
      return OpIndex::Invalid();
    } else if (global->type.is_ref()) {
      V<FixedArray> buffer = LOAD_IMMUTABLE_INSTANCE_FIELD(
          instance, TaggedGlobalsBuffer, MemoryRepresentation::TaggedPointer());
      int offset = FixedArray::OffsetOfElementAt(global->index_in_buffer);
      if (mode == GlobalMode::kLoad) {
        LoadOp::Kind load_kind = LoadOp::Kind::TaggedBase();
        if (!is_mutable) load_kind = load_kind.Immutable();
        return __ Load(buffer, load_kind, MemoryRepresentation::AnyTagged(),
                       offset);
      } else {
        // TODO(jkummerow): Set {WriteBarrierKind::kPointerWriteBarrier} when
        // we know that {value} cannot be a Smi.
        __ Store(buffer, value, StoreOp::Kind::TaggedBase(),
                 MemoryRepresentation::AnyTagged(),
                 WriteBarrierKind::kFullWriteBarrier, offset);
        return OpIndex::Invalid();
      }
    } else {
      V<ByteArray> buffer =
          LOAD_IMMUTABLE_INSTANCE_FIELD(instance, UntaggedGlobalsBuffer,
                                        MemoryRepresentation::TaggedPointer());
      int offset = ByteArray::OffsetOfElementAt(global->index_in_buffer);
      if (mode == GlobalMode::kLoad) {
        LoadOp::Kind load_kind = LoadOp::Kind::TaggedBase();
        if (!is_mutable) load_kind = load_kind.Immutable();
        return __ Load(buffer, load_kind, RepresentationFor(global->type, true),
                       offset);
      } else {
        __ Store(buffer, value, StoreOp::Kind::TaggedBase(),
                 RepresentationFor(global->type, true),
                 WriteBarrierKind::kNoWriteBarrier, offset);
        return OpIndex::Invalid();
      }
    }
  }

  void RejectNonWasmObject(V<Map> map, Label<Word32>& result_label) {
    EmitWasmObjectCheck(map, [this, &result_label](ConditionWithHint condition,
                                                   EarlyExitConfig early_exit) {
      DCHECK(early_exit.negated && !early_exit.result_value);
      GOTO_IF_NOT(condition, result_label, 0);
    });
  }

  void TrapOnNonWasmObject(V<Map> map, OptionalV<EagerFrameState> frame_state) {
    EmitWasmObjectCheck(map, [this, frame_state](ConditionWithHint condition,
                                                 EarlyExitConfig early_exit) {
      DCHECK(early_exit.negated && !early_exit.result_value);
      __ TrapIfNot(condition.condition(), frame_state,
                   TrapId::kTrapIllegalCast);
    });
  }

  std::pair<bool, bool> null_checks_for_struct_op(CheckForNull null_check,
                                                  int field_index) {
    bool explicit_null_check =
        null_check == kWithNullCheck &&
        (null_check_strategy_ == NullCheckStrategy::kExplicit ||
         field_index > wasm::kMaxStructFieldIndexForImplicitNullCheck);
    bool implicit_null_check =
        null_check == kWithNullCheck && !explicit_null_check;
    return {explicit_null_check, implicit_null_check};
  }

  const wasm::WasmModule* module_ = __ data() -> wasm_module();

  // Wasm-in-JS inlining runs in the JS pipeline where we cannot use the
  // trap handler. For these cases, `is_wasm()` ensures we use explicit checks.
  const NullCheckStrategy null_check_strategy_ =
      trap_handler::IsTrapHandlerEnabled() && V8_STATIC_ROOTS_BOOL &&
              __ data() -> is_wasm()
          ? NullCheckStrategy::kTrapHandler
          : NullCheckStrategy::kExplicit;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_LOWERING_REDUCER_H_
