// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_TURBOSHAFT_WASM_LOWERING_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_WASM_LOWERING_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

#define LOAD_INSTANCE_FIELD(instance_node, name, representation)     \
  __ Load(instance_node, LoadOp::Kind::TaggedBase(), representation, \
          WasmInstanceObject::k##name##Offset);

template <class Next>
class WasmLoweringReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  OpIndex REDUCE(GlobalGet)(OpIndex instance, const wasm::WasmGlobal* global) {
    return LowerGlobalSetOrGet(instance, OpIndex::Invalid(), global,
                               GlobalMode::kLoad);
  }

  OpIndex REDUCE(GlobalSet)(OpIndex instance, OpIndex value,
                            const wasm::WasmGlobal* global) {
    return LowerGlobalSetOrGet(instance, value, global, GlobalMode::kStore);
  }

  OpIndex REDUCE(Null)(wasm::ValueType type) { return Null(type); }

  OpIndex REDUCE(IsNull)(OpIndex object, wasm::ValueType type) {
    // TODO(14108): Can this be done simpler for static-roots nowadays?
    Tagged_t static_null =
        wasm::GetWasmEngine()->compressed_wasm_null_value_or_zero();
    OpIndex null_value =
        !wasm::IsSubtypeOf(type, wasm::kWasmExternRef, module_) &&
                static_null != 0
            ? __ UintPtrConstant(static_null)
            : Null(type);
    return __ TaggedEqual(object, null_value);
  }

  OpIndex REDUCE(AssertNotNull)(OpIndex object, wasm::ValueType type,
                                TrapId trap_id) {
    if (trap_id == TrapId::kTrapNullDereference) {
      // Skip the check altogether if null checks are turned off.
      if (!v8_flags.experimental_wasm_skip_null_checks) {
        // Use an explicit null check if
        // (1) we cannot use trap handler or
        // (2) the object might be a Smi or
        // (3) the object might be a JS object.
        if (null_check_strategy_ == NullCheckStrategy::kExplicit ||
            wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), type, module_) ||
            wasm::IsSubtypeOf(type, wasm::kWasmExternRef, module_)) {
          __ TrapIf(__ IsNull(object, type), OpIndex::Invalid(), trap_id);
        } else {
          // Otherwise, load the word after the map word.
          static_assert(WasmStruct::kHeaderSize > kTaggedSize);
          static_assert(WasmArray::kHeaderSize > kTaggedSize);
          static_assert(WasmInternalFunction::kHeaderSize > kTaggedSize);
          __ Load(object, LoadOp::Kind::TrapOnNull(),
                  MemoryRepresentation::Int32(), kTaggedSize);
        }
      }
    } else {
      __ TrapIf(__ IsNull(object, type), OpIndex::Invalid(), trap_id);
    }
    return object;
  }

  OpIndex REDUCE(RttCanon)(OpIndex instance, uint32_t type_index) {
    OpIndex maps_list = LOAD_INSTANCE_FIELD(
        instance, ManagedObjectMaps, MemoryRepresentation::TaggedPointer());
    int map_offset = FixedArray::kHeaderSize + type_index * kTaggedSize;
    return __ Load(maps_list, LoadOp::Kind::TaggedBase(),
                   MemoryRepresentation::AnyTagged(), map_offset);
  }

  OpIndex REDUCE(WasmTypeCheck)(V<Tagged> object, V<Tagged> rtt,
                                WasmTypeCheckConfig config) {
    if (rtt != OpIndex::Invalid()) {
      return ReduceWasmTypeCheckRtt(object, rtt, config);
    } else {
      return ReduceWasmTypeCheckAbstract(object, config);
    }
  }

  OpIndex REDUCE(WasmTypeCast)(V<Tagged> object, V<Tagged> rtt,
                               WasmTypeCheckConfig config) {
    if (rtt != OpIndex::Invalid()) {
      return ReduceWasmTypeCastRtt(object, rtt, config);
    } else {
      return ReduceWasmTypeCastAbstract(object, config);
    }
  }

  OpIndex REDUCE(StructGet)(OpIndex object, const wasm::StructType* type,
                            int field_index, bool is_signed,
                            CheckForNull null_check) {
    auto [explicit_null_check, implicit_null_check] =
        null_checks_for_struct_op(null_check, field_index);

    if (explicit_null_check) {
      __ TrapIf(__ IsNull(object, wasm::kWasmAnyRef), OpIndex::Invalid(),
                TrapId::kTrapNullDereference);
    }

    LoadOp::Kind load_kind = implicit_null_check ? LoadOp::Kind::TrapOnNull()
                                                 : LoadOp::Kind::TaggedBase();
    MemoryRepresentation repr =
        RepresentationFor(type->field(field_index), is_signed);

    return __ Load(object, load_kind, repr, field_offset(type, field_index));
  }

  OpIndex REDUCE(StructSet)(OpIndex object, OpIndex value,
                            const wasm::StructType* type, int field_index,
                            CheckForNull null_check) {
    auto [explicit_null_check, implicit_null_check] =
        null_checks_for_struct_op(null_check, field_index);

    if (explicit_null_check) {
      __ TrapIf(__ IsNull(object, wasm::kWasmAnyRef), OpIndex::Invalid(),
                TrapId::kTrapNullDereference);
    }

    StoreOp::Kind store_kind = implicit_null_check
                                   ? StoreOp::Kind::TrapOnNull()
                                   : StoreOp::Kind::TaggedBase();
    MemoryRepresentation repr =
        RepresentationFor(type->field(field_index), true);

    __ Store(object, value, store_kind, repr,
             type->field(field_index).is_reference() ? kFullWriteBarrier
                                                     : kNoWriteBarrier,
             field_offset(type, field_index));

    return OpIndex::Invalid();
  }

  OpIndex REDUCE(ArrayGet)(OpIndex array, OpIndex index,
                           wasm::ValueType element_type, bool is_signed) {
    return __ Load(array, __ ChangeInt32ToIntPtr(index),
                   LoadOp::Kind::TaggedBase(),
                   RepresentationFor(element_type, is_signed),
                   WasmArray::kHeaderSize, element_type.value_kind_size_log2());
  }

  OpIndex REDUCE(ArraySet)(OpIndex array, OpIndex index, OpIndex value,
                           wasm::ValueType element_type) {
    __ Store(array, __ ChangeInt32ToIntPtr(index), value,
             LoadOp::Kind::TaggedBase(), RepresentationFor(element_type, true),
             element_type.is_reference() ? kFullWriteBarrier : kNoWriteBarrier,
             WasmArray::kHeaderSize, element_type.value_kind_size_log2());
    return OpIndex::Invalid();
  }

  OpIndex REDUCE(ArrayLength)(OpIndex array, CheckForNull null_check) {
    bool explicit_null_check =
        null_check == kWithNullCheck &&
        null_check_strategy_ == NullCheckStrategy::kExplicit;
    bool implicit_null_check =
        null_check == kWithNullCheck &&
        null_check_strategy_ == NullCheckStrategy::kTrapHandler;

    if (explicit_null_check) {
      __ TrapIf(__ IsNull(array, wasm::kWasmAnyRef), OpIndex::Invalid(),
                TrapId::kTrapNullDereference);
    }

    LoadOp::Kind load_kind = implicit_null_check ? LoadOp::Kind::TrapOnNull()
                                                 : LoadOp::Kind::TaggedBase();

    return __ Load(array, load_kind, RepresentationFor(wasm::kWasmI32, true),
                   WasmArray::kLengthOffset);
  }

 private:
  enum class GlobalMode { kLoad, kStore };

  static constexpr MemoryRepresentation kMaybeSandboxedPointer =
      V8_ENABLE_SANDBOX_BOOL ? MemoryRepresentation::SandboxedPointer()
                             : MemoryRepresentation::PointerSized();

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
      case wasm::kF32:
        return MemoryRepresentation::Float32();
      case wasm::kF64:
        return MemoryRepresentation::Float64();
      case wasm::kS128:
        return MemoryRepresentation::Simd128();
      case wasm::kRtt:
      case wasm::kRef:
      case wasm::kRefNull:
        return MemoryRepresentation::AnyTagged();
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }
  }

  OpIndex ReduceWasmTypeCheckAbstract(V<Tagged> object,
                                      WasmTypeCheckConfig config) {
    const bool object_can_be_null = config.from.is_nullable();
    const bool null_succeeds = config.to.is_nullable();
    const bool object_can_be_i31 =
        wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), config.from,
                          module_) ||
        config.from.heap_representation() == wasm::HeapType::kExtern;

    OpIndex result;
    Label<Word32> end_label(&Asm());

    wasm::HeapType::Representation to_rep = config.to.heap_representation();
    do {
      // The none-types only perform a null check. They need no control flow.
      if (to_rep == wasm::HeapType::kNone ||
          to_rep == wasm::HeapType::kNoExtern ||
          to_rep == wasm::HeapType::kNoFunc) {
        result = __ IsNull(object, config.from);
        break;
      }
      // Null checks performed by any other type check need control flow. We can
      // skip the null check if null fails, because it's covered by the Smi
      // check or instance type check we'll do later.
      if (object_can_be_null && null_succeeds) {
        const int kResult = 1;
        GOTO_IF(UNLIKELY(__ IsNull(object, wasm::kWasmAnyRef)), end_label,
                __ Word32Constant(kResult));
      }
      // i31 is special in that the Smi check is the last thing to do.
      if (to_rep == wasm::HeapType::kI31) {
        // If earlier optimization passes reached the limit of possible graph
        // transformations, we could DCHECK(object_can_be_i31) here.
        result = object_can_be_i31 ? __ IsSmi(object) : __ Word32Constant(0);
        break;
      }
      if (to_rep == wasm::HeapType::kEq) {
        if (object_can_be_i31) {
          GOTO_IF(UNLIKELY(__ IsSmi(object)), end_label, __ Word32Constant(1));
        }
        // TODO(mliedtke): Ideally we'd be able to mark the map load as
        // immutable.
        result = IsDataRefMap(__ LoadMapField(object));
        break;
      }
      // array, struct, string: i31 fails.
      if (object_can_be_i31) {
        GOTO_IF(UNLIKELY(__ IsSmi(object)), end_label, __ Word32Constant(0));
      }
      if (to_rep == wasm::HeapType::kArray) {
        result = HasInstanceType(object, WASM_ARRAY_TYPE);
        break;
      }
      if (to_rep == wasm::HeapType::kStruct) {
        result = HasInstanceType(object, WASM_STRUCT_TYPE);
        break;
      }
      if (to_rep == wasm::HeapType::kString) {
        V<Word32> instance_type =
            __ LoadInstanceTypeField(__ LoadMapField(object));
        result = __ Uint32LessThan(instance_type,
                                   __ Word32Constant(FIRST_NONSTRING_TYPE));
        break;
      }
      UNREACHABLE();
    } while (false);

    DCHECK(result.valid());
    GOTO(end_label, result);
    BIND(end_label, final_result);
    return final_result;
  }

  OpIndex ReduceWasmTypeCastAbstract(V<Tagged> object,
                                     WasmTypeCheckConfig config) {
    const bool object_can_be_null = config.from.is_nullable();
    const bool null_succeeds = config.to.is_nullable();
    const bool object_can_be_i31 =
        wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), config.from,
                          module_) ||
        config.from.heap_representation() == wasm::HeapType::kExtern;

    Label<> end_label(&Asm());

    wasm::HeapType::Representation to_rep = config.to.heap_representation();

    do {
      // The none-types only perform a null check.
      if (to_rep == wasm::HeapType::kNone ||
          to_rep == wasm::HeapType::kNoExtern ||
          to_rep == wasm::HeapType::kNoFunc) {
        __ TrapIfNot(__ IsNull(object, config.from), OpIndex::Invalid(),
                     TrapId::kTrapIllegalCast);
        break;
      }
      // Null checks performed by any other type cast can be skipped if null
      // fails, because it's covered by the Smi check
      // or instance type check we'll do later.
      if (object_can_be_null && null_succeeds &&
          !v8_flags.experimental_wasm_skip_null_checks) {
        GOTO_IF(UNLIKELY(__ IsNull(object, config.from)), end_label);
      }
      if (to_rep == wasm::HeapType::kI31) {
        // If earlier optimization passes reached the limit of possible graph
        // transformations, we could DCHECK(object_can_be_i31) here.
        V<Word32> success =
            object_can_be_i31 ? __ IsSmi(object) : __ Word32Constant(0);
        __ TrapIfNot(success, OpIndex::Invalid(), TrapId::kTrapIllegalCast);
        break;
      }
      if (to_rep == wasm::HeapType::kEq) {
        if (object_can_be_i31) {
          GOTO_IF(UNLIKELY(__ IsSmi(object)), end_label);
        }
        __ TrapIfNot(IsDataRefMap(__ LoadMapField(object)), OpIndex::Invalid(),
                     TrapId::kTrapIllegalCast);
        break;
      }
      // array, struct, string: i31 fails.
      if (object_can_be_i31) {
        __ TrapIf(__ IsSmi(object), OpIndex::Invalid(),
                  TrapId::kTrapIllegalCast);
      }
      if (to_rep == wasm::HeapType::kArray) {
        __ TrapIfNot(HasInstanceType(object, WASM_ARRAY_TYPE),
                     OpIndex::Invalid(), TrapId::kTrapIllegalCast);
        break;
      }
      if (to_rep == wasm::HeapType::kStruct) {
        __ TrapIfNot(__ HasInstanceType(object, WASM_STRUCT_TYPE),
                     OpIndex::Invalid(), TrapId::kTrapIllegalCast);
        break;
      }
      if (to_rep == wasm::HeapType::kString) {
        V<Word32> instance_type =
            __ LoadInstanceTypeField(__ LoadMapField(object));
        __ TrapIfNot(__ Uint32LessThan(instance_type,
                                       __ Word32Constant(FIRST_NONSTRING_TYPE)),
                     OpIndex::Invalid(), TrapId::kTrapIllegalCast);
        break;
      }
      UNREACHABLE();
    } while (false);

    GOTO(end_label);
    BIND(end_label);
    return object;
  }

  OpIndex ReduceWasmTypeCastRtt(V<Tagged> object, V<Tagged> rtt,
                                WasmTypeCheckConfig config) {
    int rtt_depth = wasm::GetSubtypingDepth(module_, config.to.ref_index());
    bool object_can_be_null = config.from.is_nullable();
    bool object_can_be_i31 =
        wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), config.from, module_);

    Label<> end_label(&Asm());
    bool is_cast_from_any = config.from.is_reference_to(wasm::HeapType::kAny);

    // If we are casting from any and null results in check failure, then the
    // {IsDataRefMap} check below subsumes the null check. Otherwise, perform
    // an explicit null check now.
    if (object_can_be_null && (!is_cast_from_any || config.to.is_nullable())) {
      V<Word32> is_null = __ IsNull(object, wasm::kWasmAnyRef);
      if (config.to.is_nullable()) {
        GOTO_IF(UNLIKELY(is_null), end_label);
      } else if (!v8_flags.experimental_wasm_skip_null_checks) {
        __ TrapIf(is_null, OpIndex::Invalid(), TrapId::kTrapIllegalCast);
      }
    }

    if (object_can_be_i31) {
      __ TrapIf(__ IsSmi(object), OpIndex::Invalid(), TrapId::kTrapIllegalCast);
    }

    // TODO(mliedtke): Ideally we'd be able to mark this as immutable as well.
    V<Map> map = __ LoadMapField(object);

    if (module_->types[config.to.ref_index()].is_final) {
      __ TrapIfNot(__ TaggedEqual(map, rtt), OpIndex::Invalid(),
                   TrapId::kTrapIllegalCast);
      GOTO(end_label);
    } else {
      // First, check if types happen to be equal. This has been shown to give
      // large speedups.
      GOTO_IF(LIKELY(__ TaggedEqual(map, rtt)), end_label);

      // Check if map instance type identifies a wasm object.
      if (is_cast_from_any) {
        V<Word32> is_wasm_obj = IsDataRefMap(map);
        __ TrapIfNot(is_wasm_obj, OpIndex::Invalid(), TrapId::kTrapIllegalCast);
      }

      V<Tagged> type_info = LoadWasmTypeInfo(map);
      DCHECK_GE(rtt_depth, 0);
      // If the depth of the rtt is known to be less that the minimum supertype
      // array length, we can access the supertype without bounds-checking the
      // supertype array.
      if (static_cast<uint32_t>(rtt_depth) >=
          wasm::kMinimumSupertypeArraySize) {
        V<WordPtr> supertypes_length = ChangeSmiToWordPtr(
            __ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                    MemoryRepresentation::TaggedSigned(),
                    WasmTypeInfo::kSupertypesLengthOffset));
        __ TrapIfNot(
            __ UintPtrLessThan(__ IntPtrConstant(rtt_depth), supertypes_length),
            OpIndex::Invalid(), TrapId::kTrapIllegalCast);
      }

      V<Tagged> maybe_match =
          __ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                  MemoryRepresentation::TaggedPointer(),
                  WasmTypeInfo::kSupertypesOffset + kTaggedSize * rtt_depth);

      __ TrapIfNot(__ TaggedEqual(maybe_match, rtt), OpIndex::Invalid(),
                   TrapId::kTrapIllegalCast);
      GOTO(end_label);
    }

    BIND(end_label);
    return object;
  }

  OpIndex ReduceWasmTypeCheckRtt(V<Tagged> object, V<Tagged> rtt,
                                 WasmTypeCheckConfig config) {
    int rtt_depth = wasm::GetSubtypingDepth(module_, config.to.ref_index());
    bool object_can_be_null = config.from.is_nullable();
    bool object_can_be_i31 =
        wasm::IsSubtypeOf(wasm::kWasmI31Ref.AsNonNull(), config.from, module_);
    bool is_cast_from_any = config.from.is_reference_to(wasm::HeapType::kAny);

    Label<Word32> end_label(&Asm());

    // If we are casting from any and null results in check failure, then the
    // {IsDataRefMap} check below subsumes the null check. Otherwise, perform
    // an explicit null check now.
    if (object_can_be_null && (!is_cast_from_any || config.to.is_nullable())) {
      const int kResult = config.to.is_nullable() ? 1 : 0;
      GOTO_IF(UNLIKELY(__ IsNull(object, wasm::kWasmAnyRef)), end_label,
              __ Word32Constant(kResult));
    }

    if (object_can_be_i31) {
      GOTO_IF(__ IsSmi(object), end_label, __ Word32Constant(0));
    }

    // TODO(mliedtke): Ideally we'd be able to mark this as immutable as well.
    V<Map> map = __ LoadMapField(object);

    if (module_->types[config.to.ref_index()].is_final) {
      GOTO(end_label, __ TaggedEqual(map, rtt));
    } else {
      // First, check if types happen to be equal. This has been shown to give
      // large speedups.
      GOTO_IF(LIKELY(__ TaggedEqual(map, rtt)), end_label,
              __ Word32Constant(1));

      // Check if map instance type identifies a wasm object.
      if (is_cast_from_any) {
        V<Word32> is_wasm_obj = IsDataRefMap(map);
        GOTO_IF_NOT(LIKELY(is_wasm_obj), end_label, __ Word32Constant(0));
      }

      V<Tagged> type_info = LoadWasmTypeInfo(map);
      DCHECK_GE(rtt_depth, 0);
      // If the depth of the rtt is known to be less that the minimum supertype
      // array length, we can access the supertype without bounds-checking the
      // supertype array.
      if (static_cast<uint32_t>(rtt_depth) >=
          wasm::kMinimumSupertypeArraySize) {
        // TODO(mliedtke): Why do we convert to word size and not just do a 32
        // bit operation? (The same applies for WasmTypeCast below.)
        V<WordPtr> supertypes_length = ChangeSmiToWordPtr(
            __ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                    MemoryRepresentation::TaggedSigned(),
                    WasmTypeInfo::kSupertypesLengthOffset));
        GOTO_IF_NOT(LIKELY(__ UintPtrLessThan(__ IntPtrConstant(rtt_depth),
                                              supertypes_length)),
                    end_label, __ Word32Constant(0));
      }

      V<Tagged> maybe_match =
          __ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                  MemoryRepresentation::TaggedPointer(),
                  WasmTypeInfo::kSupertypesOffset + kTaggedSize * rtt_depth);

      GOTO(end_label, __ TaggedEqual(maybe_match, rtt));
    }

    BIND(end_label, result);
    return result;
  }

  V<Word32> HasInstanceType(V<Tagged> object, InstanceType instance_type) {
    // TODO(mliedtke): These loads should be immutable.
    return __ Word32Equal(__ LoadInstanceTypeField(__ LoadMapField(object)),
                          __ Word32Constant(instance_type));
  }

  OpIndex LowerGlobalSetOrGet(OpIndex instance, OpIndex value,
                              const wasm::WasmGlobal* global, GlobalMode mode) {
    if (global->mutability && global->imported) {
      OpIndex imported_mutable_globals =
          LOAD_INSTANCE_FIELD(instance, ImportedMutableGlobals,
                              MemoryRepresentation::TaggedPointer());
      int field_offset =
          FixedAddressArray::kHeaderSize + global->index * kSystemPointerSize;
      if (global->type.is_reference()) {
        OpIndex buffers =
            LOAD_INSTANCE_FIELD(instance, ImportedMutableGlobalsBuffers,
                                MemoryRepresentation::TaggedPointer());
        int offset_in_buffers =
            FixedArray::kHeaderSize + global->offset * kTaggedSize;
        OpIndex base =
            __ Load(buffers, LoadOp::Kind::TaggedBase(),
                    MemoryRepresentation::AnyTagged(), offset_in_buffers);
        V<Word32> index = __ Load(imported_mutable_globals, OpIndex::Invalid(),
                                  LoadOp::Kind::TaggedBase(),
                                  MemoryRepresentation::Int32(), field_offset);
        V<WordPtr> index_ptr = __ ChangeInt32ToIntPtr(index);
        if (mode == GlobalMode::kLoad) {
          return __ Load(base, index_ptr, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::AnyTagged(),
                         FixedArray::kObjectsOffset, kTaggedSizeLog2);
        } else {
          __ Store(base, index_ptr, value, StoreOp::Kind::TaggedBase(),
                   MemoryRepresentation::AnyTagged(),
                   WriteBarrierKind::kFullWriteBarrier,
                   FixedArray::kObjectsOffset, kTaggedSizeLog2);
          return OpIndex::Invalid();
        }
      } else {
        // Global is imported mutable but not a reference.
        OpIndex base = __ Load(imported_mutable_globals, OpIndex::Invalid(),
                               LoadOp::Kind::TaggedBase(),
                               kMaybeSandboxedPointer, field_offset);
        if (mode == GlobalMode::kLoad) {
          return __ Load(base, LoadOp::Kind::RawAligned(),
                         RepresentationFor(global->type, true), 0);
        } else {
          __ Store(base, value, StoreOp::Kind::RawAligned(),
                   RepresentationFor(global->type, true),
                   WriteBarrierKind::kNoWriteBarrier, 0);
          return OpIndex::Invalid();
        }
      }
    } else if (global->type.is_reference()) {
      OpIndex base = LOAD_INSTANCE_FIELD(instance, TaggedGlobalsBuffer,
                                         MemoryRepresentation::TaggedPointer());
      int offset = FixedArray::kHeaderSize + global->offset * kTaggedSize;
      if (mode == GlobalMode::kLoad) {
        return __ Load(base, LoadOp::Kind::TaggedBase(),
                       MemoryRepresentation::AnyTagged(), offset);
      } else {
        __ Store(base, value, StoreOp::Kind::TaggedBase(),
                 MemoryRepresentation::AnyTagged(),
                 WriteBarrierKind::kFullWriteBarrier, offset);
        return OpIndex::Invalid();
      }
    } else {
      OpIndex base =
          LOAD_INSTANCE_FIELD(instance, GlobalsStart, kMaybeSandboxedPointer);
      if (mode == GlobalMode::kLoad) {
        return __ Load(base, LoadOp::Kind::RawAligned(),
                       RepresentationFor(global->type, true), global->offset);
      } else {
        __ Store(base, value, StoreOp::Kind::RawAligned(),
                 RepresentationFor(global->type, true),
                 WriteBarrierKind::kNoWriteBarrier, global->offset);
        return OpIndex::Invalid();
      }
    }
  }

  OpIndex Null(wasm::ValueType type) {
    OpIndex roots = __ LoadRootRegister();
    RootIndex index = wasm::IsSubtypeOf(type, wasm::kWasmExternRef, module_)
                          ? RootIndex::kNullValue
                          : RootIndex::kWasmNull;
    return __ Load(roots, LoadOp::Kind::RawAligned().Immutable(),
                   MemoryRepresentation::PointerSized(),
                   IsolateData::root_slot_offset(index));
  }

  V<WordPtr> ChangeSmiToWordPtr(V<Tagged> smi) {
    return __ ChangeInt32ToIntPtr(__ UntagSmi(smi));
  }

  V<Word32> IsDataRefMap(V<Map> map) {
    // TODO(mliedtke): LoadInstanceTypeField should emit an immutable load for
    // wasm.
    V<Word32> instance_type = __ LoadInstanceTypeField(map);
    // We're going to test a range of WasmObject instance types with a single
    // unsigned comparison.
    V<Word32> comparison_value =
        __ Word32Sub(instance_type, FIRST_WASM_OBJECT_TYPE);
    return __ Uint32LessThanOrEqual(
        comparison_value, LAST_WASM_OBJECT_TYPE - FIRST_WASM_OBJECT_TYPE);
  }

  V<Tagged> LoadWasmTypeInfo(V<Map> map) {
    int offset = Map::kConstructorOrBackPointerOrNativeContextOffset;
    return __ Load(map, LoadOp::Kind::TaggedBase().Immutable(),
                   MemoryRepresentation::TaggedPointer(), offset);
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

  int field_offset(const wasm::StructType* type, int field_index) {
    return WasmStruct::kHeaderSize + type->field_offset(field_index);
  }

  const wasm::WasmModule* module_ = PipelineData::Get().wasm_module();
  const NullCheckStrategy null_check_strategy_ =
      trap_handler::IsTrapHandlerEnabled() && V8_STATIC_ROOTS_BOOL
          ? NullCheckStrategy::kTrapHandler
          : NullCheckStrategy::kExplicit;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_LOWERING_REDUCER_H_
