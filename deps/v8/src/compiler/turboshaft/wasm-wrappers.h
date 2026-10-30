// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_WRAPPERS_H_
#define V8_COMPILER_TURBOSHAFT_WASM_WRAPPERS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/codegen/source-position.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/wasm-assembler-helpers.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/objects/shared-function-info.h"
#include "src/wasm/turboshaft-graph-interface.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"

namespace v8::internal::compiler::turboshaft {

struct WasmInlinedFunctionData {
  wasm::NativeModule* native_module = nullptr;
  uint32_t function_index = 0;
  V<EagerFrameState> js_caller_frame_state;
  Handle<SharedFunctionInfo> shared_fct_info;
  SourcePosition call_pos;
};

struct WasmBodyInliningResult {
  bool success = false;
  OptionalV<Any> result = OptionalV<Any>::Nullopt();

  static WasmBodyInliningResult Failed() { return {}; }
};

#include "src/compiler/turboshaft/define-assembler-macros.inc"

const compiler::turboshaft::TSCallDescriptor* GetBuiltinCallDescriptor(
    Builtin name, Zone* zone);

template <typename Assembler>
class WasmWrapperTSGraphBuilder : public wasm::WasmGraphBuilderBase<Assembler> {
  using typename wasm::WasmGraphBuilderBase<Assembler>::Any;

  using CallDescriptor = compiler::CallDescriptor;
  using Operator = compiler::Operator;

  template <typename T>
  using ScopedVar = compiler::turboshaft::ScopedVar<T, Assembler>;

  using CanonicalValueType = wasm::CanonicalValueType;
  using NumericKind = wasm::NumericKind;
  using GenericKind = wasm::GenericKind;

 public:
  using wasm::WasmGraphBuilderBase<Assembler>::Asm;

  WasmWrapperTSGraphBuilder(
      Zone* zone, Assembler& assembler, const wasm::CanonicalSig* sig,
      bool is_inlining_into_js,
      std::optional<compiler::turboshaft::WasmInlinedFunctionData>
          inlined_function_data = {})
      : wasm::WasmGraphBuilderBase<Assembler>(zone, assembler),
        is_inlining_into_js_(is_inlining_into_js),
        sig_(sig),
        inlined_function_data_(std::move(inlined_function_data)) {
    DCHECK_IMPLIES(is_inlining_into_js_, __ data()->isolate());
    DCHECK_IMPLIES(inlined_function_data_, __ data()->isolate());
  }

  void AbortIfNot(V<Word32> condition, AbortReason abort_reason);

  V<WordPtr> GetTargetForBuiltinCall(Builtin builtin) {
    return wasm::WasmGraphBuilderBase<Assembler>::GetTargetForBuiltinCall(
        builtin, StubCallMode::kCallBuiltinPointer);
  }

  template <typename Descriptor, typename... Args>
  OpIndex CallBuiltin(Builtin name, Operator::Properties properties,
                      Args... args) {
    auto call_descriptor = compiler::Linkage::GetStubCallDescriptor(
        __ graph_zone(), Descriptor(), 0, CallDescriptor::kNoFlags, properties,
        StubCallMode::kCallBuiltinPointer);
    compiler::CanThrow can_throw = (properties & Operator::kNoThrow)
                                       ? compiler::CanThrow{false}
                                       : compiler::CanThrow{true};
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, can_throw, compiler::LazyDeoptOnThrow{false},
        __ graph_zone());
    V<WordPtr> call_target = GetTargetForBuiltinCall(name);
    return __ Call(call_target, {args...}, ts_call_descriptor);
  }

  V<Number> BuildChangeInt32ToNumber(V<Word32> value);

  V<Object> ToJS(OpIndex ret, CanonicalValueType type, V<Context> context);
  V<Object> BuildToJSFunctionRef(V<WasmFuncRef> ret, V<Context> context);

  // Generate a call to the AllocateJSArray builtin.
  V<JSArray> BuildCallAllocateJSArray(V<Number> array_length,
                                      V<Object> context) {
    // Since we don't check that args will fit in an array,
    // we make sure this is true based on statically known limits.
    static_assert(wasm::kV8MaxWasmFunctionReturns <=
                  JSArray::kInitialMaxFastElementArray);
    return CallBuiltin<WasmAllocateJSArrayDescriptor>(
        Builtin::kWasmAllocateJSArray, Operator::kEliminatable, array_length,
        context);
  }

  void BuildCallWasmFromWrapper(Zone* zone, const wasm::CanonicalSig* sig,
                                V<Word32> callee,
                                const base::Vector<OpIndex> args,
                                base::Vector<OpIndex> returns,
                                OptionalV<LazyFrameState> frame_state,
                                compiler::LazyDeoptOnThrow lazy_deopt_on_throw);

  V<Object> ConvertWasmResultsToJS(base::Vector<OpIndex> returns,
                                   V<Context> js_context);

  // Overload for the inlined JS-to-Wasm wrapper.
  // Returns the result of the Wasm function converted to a JS value.
  V<Any> BuildJSToWasmWrapper(V<JSFunction> js_closure, V<Context> js_context,
                              base::Vector<const OpIndex> arguments,
                              OptionalV<LazyFrameState> lazy_frame_state,
                              compiler::LazyDeoptOnThrow lazy_deopt_on_throw,
                              OptionalV<EagerFrameState> caller_frame_state);

  // Overload for the "regular" non-inlined compiled JS-to-Wasm wrapper.
  void BuildJSToWasmWrapper();

  void BuildWasmToJSWrapper(wasm::ImportCallKind kind, int expected_arity,
                            wasm::Suspend suspend);

  void BuildJSFastApiCallWrapper(DirectHandle<JSReceiver> callable);

  void BuildWasmStackEntryWrapper();

  void BuildCapiCallWrapper();

  void BuildCWasmEntryWrapper();

  V<Float64> HeapNumberToFloat64(V<HeapNumber> input) {
    return __ template LoadField<Float64>(
        input, compiler::AccessBuilder::ForHeapNumberValue());
  }

  // Jumps to `done` with the Smi value equal to `input` if `input` is in i31
  // range, otherwise falls through.
  void CanonicalizeHeapNumber(V<HeapNumber> input, Label<Object>& done) {
    Label<> not_i31(&Asm());
    // The heap number might be shared, but we already checked its map, so no
    // need to mark the Load as shared here.
    V<Float64> float_value = __ LoadHeapNumberValue(input);

    // Check if value is integral.
    V<Word32> int_value =
        __ TruncateFloat64ToInt32OverflowUndefined(float_value);
    V<Word32> is_integral =
        __ Float64Equal(float_value, __ ChangeInt32ToFloat64(int_value));
    GOTO_IF_NOT(is_integral, not_i31);

    // Check if value is -0.
    IF (__ Word32Equal(int_value, 0)) {
      V<Word32> is_minus_zero =
          __ Int32LessThan(__ Float64ExtractHighWord32(float_value), 0);
      GOTO_IF(is_minus_zero, not_i31);
      GOTO(done, __ TagSmi(int_value));
    }

    // Check i31 range.
    GOTO_IF_NOT(__ Int32LessThanOrEqual(wasm::kInt31MinValue, int_value),
                not_i31);
    GOTO_IF_NOT(__ Int32LessThanOrEqual(int_value, wasm::kInt31MaxValue),
                not_i31);
    GOTO(done, __ TagSmi(int_value));

    BIND(not_i31);
  }

  // If the object is shared, jumps to `done` with `input`, otherwise falls back
  // to `fallback`.
  void EnsureObjectSharedness(V<HeapObject> input, Label<Object>& done,
                              Label<>& fallback) {
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
    // Bail out for read-only objects.
    V<Word32> lower32 =
        __ TruncateWordPtrToWord32(__ BitcastTaggedToWordPtr(input));
    GOTO_IF(
        __ Uint32LessThan(lower32, __ Word32Constant(static_cast<uint32_t>(
                                       kContiguousReadOnlyReservationSize))),
        done, input);
    // Bail out for already-shared objects.
    V<WordPtr> flags = __ LoadPageFlags(input);
    V<WordPtr> page_flags = __ WordPtrBitwiseAnd(
        flags, static_cast<uintptr_t>(MemoryChunk::kInSharedHeap));
#else   // !CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
    V<WordPtr> flags = __ LoadPageFlags(input);
    V<WordPtr> page_flags = __ WordPtrBitwiseAnd(
        flags,
        static_cast<uintptr_t>(MemoryChunk::kIsReadOnlyOrSharedHeapMask));
#endif  // !CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
    // If it isn't shared yet, use the runtime function.
    GOTO_IF(UNLIKELY(__ WordPtrEqual(page_flags, 0)), fallback);
    GOTO(done, input);
  }

  // Checks that the object is shared or not according to `type`. If not, jumps
  // to `type_error`. Assumes the object cannot be allocated in read-only space.
  void CheckWasmObjectSharedness(V<HeapObject> input, CanonicalValueType type,
                                 Label<>& type_error) {
    if (v8_flags.wasm_shared) {
      V<WordPtr> flags = __ LoadPageFlags(input);
      V<WordPtr> page_flags = __ WordPtrBitwiseAnd(
          flags, static_cast<uintptr_t>(MemoryChunk::kInSharedHeap));
      if (type.is_shared()) {
        GOTO_IF(__ WordPtrEqual(page_flags, 0), type_error);
      } else {
        GOTO_IF_NOT(__ WordPtrEqual(page_flags, 0), type_error);
      }
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
      // Wasm GC objects (structs/arrays) are currently never allocated in
      // read-only space. Verify this invariant to guard against future changes
      // where constant Wasm objects might be placed in RO space.
      // TSA_DCHECK is only usable in isolate-dependent code.
      if (__ data()->isolate() != nullptr) {
        TSA_DCHECK(
            this,
            __ Uint32LessThanOrEqual(
                __ Word32Constant(
                    static_cast<uint32_t>(kContiguousReadOnlyReservationSize)),
                __ TruncateWordPtrToWord32(__ BitcastTaggedToWordPtr(input))));
      }
#endif  // CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
    }
  }

  V<WasmCustomMap> UnwrapCustomMap(V<Object> wrapper) {
    V<WasmCustomMap> custom_map =
        __ Load(wrapper, LoadOp::Kind::TaggedBase().Immutable(),
                MemoryRepresentation::TaggedPointer(),
                offsetof(WasmCustomMapWrapper, wrapped_));
#if DEBUG
    AbortIfNot(
        __ HasInstanceType(custom_map, WASM_CUSTOM_MAP_TYPE, SharedFlag{false}),
        AbortReason::kOperandIsNotAMap);
#endif
    return custom_map;
  }

  // {input} must already be non-i31.
  void BuildCheckIndexedStructOrArray(V<Object> input, CanonicalValueType type,
                                      Label<Object>& done,
                                      Label<>& type_error) {
    wasm::TypeCanonicalizer* type_canonicalizer = wasm::GetTypeCanonicalizer();
    bool is_descriptor = v8_flags.wasm_merged_descriptors &&
                         type_canonicalizer->is_descriptor(type.ref_index());
    if (is_descriptor) {
      GOTO_IF_NOT(__ HasInstanceType(input, WASM_CUSTOM_MAP_WRAPPER_TYPE,
                                     SharedFlag{false}),
                  type_error);
      input = UnwrapCustomMap(input);
    }

    V<Map> object_map = LoadMap(input);
    // Fetch the canonical-types array from isolate roots.
    V<WeakFixedArray> canonical_rtts =
        __ template LoadRoot<RootIndex::kWasmCanonicalRtts>();
    V<Object> cached_map = V<Object>::Cast(__ LoadField(
        canonical_rtts, compiler::AccessBuilder::ForWeakFixedArraySlot(
                            type.ref_index().index)));
    V<Map> supertype_rtt =
        __ template BitcastWordPtrToTagged<Map>(__ WordPtrBitwiseAnd(
            __ BitcastTaggedToWordPtr(cached_map), ~kWeakHeapObjectMask));

    GOTO_IF(__ TaggedEqual(object_map, supertype_rtt), done, input);

    if (type.is_exact()) {
      GOTO(type_error);
      return;
    }

    if (!is_descriptor) {
      V<Word32> instance_type = __ LoadInstanceTypeField(object_map);
      InstanceType expected_instance_type;
      switch (type.ref_type_kind()) {
        case wasm::RefTypeKind::kStruct:
          expected_instance_type = WASM_STRUCT_TYPE;
          break;
        case wasm::RefTypeKind::kArray:
          expected_instance_type = WASM_ARRAY_TYPE;
          break;
        case wasm::RefTypeKind::kCont:
        case wasm::RefTypeKind::kFunction:
        case wasm::RefTypeKind::kOther:
          UNREACHABLE();
      }
      GOTO_IF_NOT(__ Word32Equal(instance_type, expected_instance_type),
                  type_error);
    }

    uint8_t rtt_depth =
        type_canonicalizer->GetSubtypingDepth_Slow(type.ref_index());

    V<Object> type_info = __ LoadWasmTypeInfo(object_map);
    if (rtt_depth >= wasm::kMinimumSupertypeArraySize) {
      V<Word32> supertypes_length =
          __ UntagSmi(__ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                              MemoryRepresentation::TaggedSigned(),
                              offsetof(WasmTypeInfo, supertypes_length_)));

      GOTO_IF_NOT(__ Uint32LessThan(rtt_depth, supertypes_length), type_error);
    }
    V<Object> maybe_match =
        __ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                MemoryRepresentation::TaggedPointer(),
                WasmTypeInfo::kSupertypesOffset + kTaggedSize * rtt_depth);
    GOTO_IF(__ TaggedEqual(maybe_match, supertype_rtt), done, input);
    GOTO(type_error);
  }

  V<Float32> BuildChangeTaggedToFloat32(
      V<Object> value, V<Context> context,
      OptionalV<EagerFrameState> caller_frame_state) {
    CHECK_EQ(is_inlining_into_js_, caller_frame_state.valid());
    ScopedVar<Float32> result(this, V<Float32>::Invalid());
    IF (__ IsSmi(value)) {
      // TODO(dlehmann,wasm-runtime): If `ChangeInt32ToFloat32(x)` is exactly
      // equivalent to `TruncateFloat64ToFloat32(ChangeInt32ToFloat64(x))`, we
      // could `TruncateFloat64ToFloat32(BuildChangeTaggedToFloat64(x))` and
      // get rid of this separate function, but I am not 100% sure whether that
      // is a valid optimization so we conservatively keep it.
      result = __ ChangeInt32ToFloat32(__ UntagSmi(V<Smi>::Cast(value)));
    } ELSE {
      // A a shared wasm object could flow here, but we don't need to mark it as
      // shared for TSAN purposes, because we already did so when it flowed into
      // JS.
      V<Map> map = LoadMap(value);
      // TODO(thibaudm): Handle map packing.
      V<Word32> is_heap_number = __ IsHeapNumberMap(map);
      if (caller_frame_state.valid()) {
        // When inlining JS-to-Wasm wrappers, eagerly deopt for values that
        // are not Smi or HeapNumber to avoid calling conversion builtins
        // that may throw (crbug.com/498709150).
        __ DeoptimizeIfNot(is_heap_number, caller_frame_state.value(),
                           DeoptimizeReason::kNotANumber,
                           compiler::FeedbackSource{});
        result = __ TruncateFloat64ToFloat32(
            HeapNumberToFloat64(V<HeapNumber>::Cast(value)));
      } else {
        IF (LIKELY(is_heap_number)) {
          result = __ TruncateFloat64ToFloat32(
              HeapNumberToFloat64(V<HeapNumber>::Cast(value)));
        } ELSE {
          result = __ TruncateFloat64ToFloat32(
              CallBuiltin<WasmTaggedToFloat64Descriptor>(
                  Builtin::kWasmTaggedToFloat64, Operator::kNoProperties, value,
                  context));
        }
      }
    }
    return result;
  }

  V<Float64> BuildChangeTaggedToFloat64(
      V<Object> value, V<Context> context,
      OptionalV<EagerFrameState> caller_frame_state) {
    CHECK_EQ(is_inlining_into_js_, caller_frame_state.valid());
    ScopedVar<Float64> result(this, V<Float64>::Invalid());
    IF (__ IsSmi(value)) {
      result = __ ChangeInt32ToFloat64(__ UntagSmi(V<Smi>::Cast(value)));
    } ELSE {
      // A a shared wasm object could flow here, but we don't need to mark it as
      // shared for TSAN purposes, because we already did so when it flowed into
      // JS.
      V<Map> map = LoadMap(value);
      // TODO(thibaudm): Handle map packing.
      V<Word32> is_heap_number = __ IsHeapNumberMap(map);
      if (caller_frame_state.valid()) {
        // When inlining JS-to-Wasm wrappers, eagerly deopt for values that
        // are not Smi or HeapNumber to avoid calling conversion builtins
        // that may throw (crbug.com/498709150).
        __ DeoptimizeIfNot(is_heap_number, caller_frame_state.value(),
                           DeoptimizeReason::kNotANumber,
                           compiler::FeedbackSource{});
        result = HeapNumberToFloat64(V<HeapNumber>::Cast(value));
      } else {
        IF (LIKELY(is_heap_number)) {
          result = HeapNumberToFloat64(V<HeapNumber>::Cast(value));
        } ELSE {
          result = CallBuiltin<WasmTaggedToFloat64Descriptor>(
              Builtin::kWasmTaggedToFloat64, Operator::kNoProperties, value,
              context);
        }
      }
    }
    return result;
  }

  V<Word32> BuildChangeTaggedToInt32(
      V<Object> value, V<Context> context,
      OptionalV<EagerFrameState> caller_frame_state) {
    CHECK_EQ(is_inlining_into_js_, caller_frame_state.valid());
    if (is_inlining_into_js_) {
      // When inlining into JS, emit a "high-level" JS conversion to allow
      // further optimizations. These are lowered in the MachineLoweringPhase
      // in the JS pipeline.
      // Also, this makes sure we eagerly deopt for values that are not Smi or
      // HeapNumber to avoid calling conversion builtins that may throw
      // (crbug.com/498709150).
      return __ TruncateJSPrimitiveToWord32OrDeopt(
          V<JSPrimitive>::Cast(value), caller_frame_state.value(),
          TruncateJSPrimitiveToWord32OrDeoptOp::InputRequirement::kNumber,
          FeedbackSource{});
    }

    // We expect most integers at runtime to be Smis, so it is important for
    // wrapper performance that Smi conversion be inlined.
    ScopedVar<Word32> result(this, V<Word32>::Invalid());
    IF (LIKELY(__ IsSmi(value))) {
      result = __ UntagSmi(V<Smi>::Cast(value));
    } ELSE {
      result = CallBuiltin<WasmTaggedNonSmiToInt32Descriptor>(
          Builtin::kWasmTaggedNonSmiToInt32, Operator::kNoProperties, value,
          context);
    }
    return result;
  }

#ifdef V8_ENABLE_TURBOFAN
  CallDescriptor* GetBigIntToI64CallDescriptor(bool needs_frame_state) {
    return wasm::GetWasmEngine()->call_descriptors()->GetBigIntToI64Descriptor(
        needs_frame_state);
  }

  OpIndex BuildChangeBigIntToInt64(
      V<Object> input, V<Context> context,
      OptionalV<EagerFrameState> caller_frame_state) {
    CHECK_EQ(is_inlining_into_js_, caller_frame_state.valid());
    // When inlining JS-to-Wasm wrappers, eagerly deopt for values that are
    // not BigInt to avoid calling ToBigInt, which could trigger user JS via
    // valueOf/Symbol.toPrimitive (same rationale as for i32/f32/f64).
    // Once the eager check passes, the BigIntToI64 builtin cannot throw:
    // ToBigInt short-circuits for BigInt inputs, and BigIntToRawBytes does
    // modular truncation (ToBigInt64) which never fails.
    // (crbug.com/498709150, crbug.com/504030766).
    if (caller_frame_state.valid()) {
      __ DeoptimizeIfNot(__ ObjectIsBigInt(input), caller_frame_state.value(),
                         DeoptimizeReason::kNotABigInt,
                         compiler::FeedbackSource{});
    }

    OpIndex target;
    if (Is64()) {
      target = GetTargetForBuiltinCall(Builtin::kBigIntToI64);
    } else {
      // On 32-bit platforms we already set the target to the
      // BigIntToI32Pair builtin here, so that we don't have to replace the
      // target in the int64-lowering.
      target = GetTargetForBuiltinCall(Builtin::kBigIntToI32Pair);
    }

    // When inlining (caller_frame_state valid), the eager deopt above
    // guarantees the input is a BigInt, so the builtin cannot throw.
    // No frame state or lazy deopt needed for the call.
    // When not inlining, no frame state is available either.
    CallDescriptor* call_descriptor =
        GetBigIntToI64CallDescriptor(/*needs_frame_state=*/false);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor,
        caller_frame_state.valid() ? compiler::CanThrow{false}
                                   : compiler::CanThrow{true},
        compiler::LazyDeoptOnThrow{false}, __ graph_zone());
    OpIndex call_args[] = {input, context};
    return __ Call(target, {}, base::VectorOf(call_args), ts_call_descriptor);
  }
#endif

  // Converts a JS value to the appropriate Wasm value.
  // If {caller_frame_state} is valid (i.e., when inlining JS-to-Wasm wrappers):
  // - For i32/f32/f64: Eager deopt guard ensures the value is Smi or
  //   HeapNumber, making the conversion trivially inlineable without calling
  //   any builtin. (crbug.com/493307329)
  // - For BigInt->i64: Eager deopt guard ensures the value is a BigInt,
  //   preventing ToBigInt from running user JS (valueOf). Once the guard
  //   passes, the BigIntToI64 builtin cannot throw (ToBigInt short-circuits
  //   for BigInt inputs, and the conversion is modular truncation).
  //   (crbug.com/498709150)
  OptionalOpIndex FromJS(V<Object> input, V<Context> context,
                         CanonicalValueType type,
                         OptionalV<EagerFrameState> caller_frame_state = {}) {
    if (type.is_numeric()) {
      switch (type.numeric_kind()) {
        case NumericKind::kI32:
          return BuildChangeTaggedToInt32(input, context, caller_frame_state);
        case NumericKind::kI64:
#ifdef V8_ENABLE_TURBOFAN
          // i64 values can only come from BigInt.
          return BuildChangeBigIntToInt64(input, context, caller_frame_state);
#else
          UNREACHABLE();
#endif
        case NumericKind::kF32:
          return BuildChangeTaggedToFloat32(input, context, caller_frame_state);
        case NumericKind::kF64:
          return BuildChangeTaggedToFloat64(input, context, caller_frame_state);
        case NumericKind::kS128:
        case NumericKind::kI8:
        case NumericKind::kI16:
        case NumericKind::kF16:
          UNREACHABLE();
      }
    }

    // When inlining JS-to-Wasm wrappers, CanInlineJSToWasmCall() only
    // allows nullable, non-shared externref, so none of the paths below
    // that call runtime functions (which can throw) are reachable.
    // If CanInlineJSToWasmCall() is ever extended to allow more reference
    // types, the throwing paths would need a frame state to support
    // lazy deopt on throw.
    DCHECK_IMPLIES(caller_frame_state.valid(), type == wasm::kWasmExternRef);

    if (type == wasm::kWasmExternRef) {
      return input;
    }

    bool want_extern = false;
    bool want_any = false;
    bool want_eq = false;
    bool want_i31 = false;
    bool want_struct = false;
    bool want_array = false;
    bool want_string = false;
    bool want_none = false;
    bool want_func = false;
    if (type.is_abstract_ref()) {
      switch (type.generic_kind()) {
        case GenericKind::kExtern:
          want_extern = true;
          break;
        case GenericKind::kString:
          want_string = true;
          break;
        case GenericKind::kStruct:
          want_struct = true;
          break;
        case GenericKind::kArray:
          want_array = true;
          break;
        case GenericKind::kI31:
          want_i31 = true;
          break;
        case GenericKind::kEq:
          want_eq = true;
          break;
        case GenericKind::kAny:
          want_any = true;
          break;
        case GenericKind::kNoExtern:
        case GenericKind::kNoFunc:
        case GenericKind::kNone:
          want_none = true;
          break;
        case GenericKind::kFunc:
          want_func = true;
          break;
        case GenericKind::kVoid:
        case GenericKind::kTop:
        case GenericKind::kBottom:
        case GenericKind::kExternString:
        case GenericKind::kExn:
        case GenericKind::kNoExn:
        case GenericKind::kNoCont:
        case GenericKind::kCont:
        case GenericKind::kWaitqueue:
        case GenericKind::kNoWaitqueue:
        case GenericKind::kStringViewWtf8:
        case GenericKind::kStringViewWtf16:
        case GenericKind::kStringViewIter:
          // If this is reached, then IsJSCompatibleSignature() is too
          // permissive.
          UNREACHABLE();
      }
    }

    // Make sure ValueType fits in a Smi, for Runtime::kWasmJSToWasmObject.
    static_assert(wasm::ValueType::kLastUsedBit + 1 <= kSmiValueSize);

    // TODO(548685083): Add fast paths for function/continuation types?
    // TODO(manoskouk, jkummerow): Also implement descriptor support.
    if (want_func ||
        (type.has_index() &&
         ((type.ref_type_kind() != wasm::RefTypeKind::kStruct &&
           type.ref_type_kind() != wasm::RefTypeKind::kArray) ||
          wasm::GetTypeCanonicalizer()->has_descriptor(type.ref_index())))) {
      std::initializer_list<const OpIndex> inputs = {
          input,
          __ SmiConstant(Smi::FromInt(static_cast<int>(type.raw_bit_field())))};
      return __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmJSToWasmObject,
                                inputs, context);
    }

    Label<Object> done(&Asm());
    Label<> type_error(&Asm());
    Label<> runtime(&Asm());
    Label<> process_smi(&Asm());
    Label<> process_heap_number(&Asm());
    Label<> unwrap_custom_map(&Asm());
    Label<HeapObject> check_sharedness(&Asm());

    // We could skip the Smi check when {want_none}, but that case is not
    // relevant for real-world performance.
    IF (__ IsSmi(input)) {
      if (want_extern) {
        GOTO(done, input);
      } else if (want_any || want_eq || want_i31) {
        if constexpr (SmiValuesAre31Bits()) {
          GOTO(done, input);
        } else {
          GOTO(process_smi);
        }
      } else {
        GOTO(type_error);
      }
    }

    // For non-nullable target types, only perform explicit null checks if
    // they're not implicitly handled by the instance type check later.
    if (type.is_nullable()) {
      GOTO_IF(
          __ TaggedEqual(input, __ template LoadRoot<RootIndex::kNullValue>()),
          done,
          type.use_wasm_null() ? __ template LoadRoot<RootIndex::kWasmNull>()
                               : input);
    } else if (want_any || want_extern) {
      GOTO_IF(UNLIKELY(__ TaggedEqual(
                  input, __ template LoadRoot<RootIndex::kNullValue>())),
              type_error);
    }

    if (want_none) {
      // Null values have already been handled, anything else fails.
      GOTO(type_error);
    } else if (want_extern) {
      GOTO(check_sharedness, V<HeapObject>::Cast(input));
    } else if (type.has_index()) {
      BuildCheckIndexedStructOrArray(input, type, done, type_error);
    } else {
      V<Map> map = LoadMap(input);
      V<Word32> instance_type = __ LoadInstanceTypeField(map);

      if (want_string) {
        V<Word32> is_str =
            __ Uint32LessThan(instance_type, FIRST_NONSTRING_TYPE);
        GOTO_IF(LIKELY(is_str), done, input);
      }
      if (want_struct || want_eq) {
        GOTO_IF(__ Word32Equal(instance_type, WASM_STRUCT_TYPE),
                check_sharedness, V<HeapObject>::Cast(input));
      }
      if (want_array || want_eq) {
        GOTO_IF(__ Word32Equal(instance_type, WASM_ARRAY_TYPE),
                check_sharedness, V<HeapObject>::Cast(input));
      }
      if (want_any || want_eq || want_i31) {
        GOTO_IF(__ Word32Equal(instance_type, HEAP_NUMBER_TYPE),
                process_heap_number);
      }
      if (v8_flags.wasm_merged_descriptors &&
          (want_any || want_eq || want_struct)) {
        GOTO_IF(__ Word32Equal(instance_type, WASM_CUSTOM_MAP_WRAPPER_TYPE),
                unwrap_custom_map);
      }
      if (want_any) {
        GOTO(check_sharedness, V<HeapObject>::Cast(input));
      } else {
        GOTO(type_error);
      }
    }

    if constexpr (!SmiValuesAre31Bits()) {
      if (BIND(process_smi)) {
        V<Word32> val = __ UntagSmi(V<Smi>::Cast(input));
        Label<>& out_of_range = want_any ? runtime : type_error;
        GOTO_IF_NOT(__ Int32LessThanOrEqual(wasm::kInt31MinValue, val),
                    out_of_range);
        GOTO_IF_NOT(__ Int32LessThanOrEqual(val, wasm::kInt31MaxValue),
                    out_of_range);
        GOTO(done, input);
      }
    }

    if (BIND(process_heap_number)) {
      CanonicalizeHeapNumber(V<HeapNumber>::Cast(input), done);
      if (want_any) {
        GOTO(check_sharedness, V<HeapObject>::Cast(input));
      } else {
        GOTO(type_error);
      }
    }

    if (BIND(unwrap_custom_map)) {
      GOTO(check_sharedness, UnwrapCustomMap(input));
    }

    if (BIND(check_sharedness, heap_obj)) {
      if (want_any || want_extern) {
        if (v8_flags.wasm_shared && type.is_shared()) {
          EnsureObjectSharedness(heap_obj, done, runtime);
        } else {
          GOTO(done, heap_obj);
        }
      } else {
        CheckWasmObjectSharedness(heap_obj, type, type_error);
        GOTO(done, heap_obj);
      }
    }

    if (BIND(runtime)) {
      std::initializer_list<const OpIndex> inputs = {
          input,
          __ SmiConstant(Smi::FromInt(static_cast<int>(type.raw_bit_field())))};
      GOTO(done,
           __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmJSToWasmObject,
                              inputs, context));
    }

    if (BIND(type_error)) {
      __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                         context);
      __ Unreachable();
    }

    BIND(done, result);
    return result;
  }

  V<Map> LoadMap(V<Object> object) {
    // TODO(thibaudm): Handle map packing.
    V<Map> map_word = __ LoadMapField(object, SharedFlag{false});
#ifdef V8_MAP_PACKING
    map_word = __ BitcastTaggedToWordPtrForTagAndSmiBits(map_word);
    // TODO(wenyuzhao): Clear header metadata.
    OpIndex map = __ WordBitwiseXor(
        map_word, __ IntPtrConstant(Internals::kMapWordXorMask),
        WordRepresentation::UintPtr());
    return __ BitcastWordPtrToTagged<Map>(map);
#else
    return map_word;
#endif
  }

  int AddArgumentNodes(base::Vector<OpIndex> args, int pos,
                       base::SmallVector<OpIndex, 16> wasm_params,
                       const wasm::CanonicalSig* sig, V<Context> context) {
    // Convert wasm numbers to JS values.
    for (size_t i = 0; i < wasm_params.size(); ++i) {
      args[pos++] = ToJS(wasm_params[i], sig->GetParam(i), context);
    }
    return pos;
  }

  V<SharedFunctionInfo> LoadSharedFunctionInfo(V<Object> js_function) {
    return __ template LoadField<SharedFunctionInfo>(
        js_function,
        compiler::AccessBuilder::ForJSFunctionSharedFunctionInfo());
  }

  V<Object> BuildReceiverNode(V<JSFunction> callable_node,
                              V<Context> callee_context,
                              V<Undefined> undefined_node) {
    // Check function strict bit.
    V<SharedFunctionInfo> shared_function_info =
        LoadSharedFunctionInfo(callable_node);
    OpIndex flags = __ Load(shared_function_info, LoadOp::Kind::TaggedBase(),
                            MemoryRepresentation::Int32(),
                            offsetof(SharedFunctionInfo, flags_));
    OpIndex strict_check = __ Word32BitwiseAnd(
        flags,
        __ Word32Constant(SharedFunctionInfo::Flags::IsNativeBit::kMask |
                          SharedFunctionInfo::Flags::IsStrictBit::kMask));

    // Load global receiver if sloppy else use undefined.
    ScopedVar<Object> strict_d(this, OpIndex::Invalid());
    IF (strict_check) {
      strict_d = undefined_node;
    } ELSE {
      V<Map> context_map = LoadMap(callee_context);
      V<NativeContext> callee_native_context =
          __ template LoadField<NativeContext>(
              context_map, compiler::AccessBuilder::ForMapNativeContext());
      strict_d = __ template LoadField<Object>(
          callee_native_context,
          compiler::AccessBuilder::ForContextSlotKnownPointer(
              Context::GLOBAL_PROXY_INDEX));
    }
    return strict_d;
  }

  V<Context> LoadContextFromJSFunction(V<JSFunction> js_function) {
    return __ template LoadField<Context>(
        js_function, compiler::AccessBuilder::ForJSFunctionContext());
  }

  V<Object> BuildSuspend(V<Object> value, V<Object> import_data,
                         V<Object> suspender, V<WordPtr>* old_sp,
                         V<WordPtr> old_limit) {
    // If value is a promise, suspend to the js-to-wasm prompt, and resume later
    // with the promise's resolved value.
    ScopedVar<Object> result(this, value);
    ScopedVar<WordPtr> old_sp_var(this, *old_sp);

    OpIndex native_context = __ Load(import_data, LoadOp::Kind::TaggedBase(),
                                     MemoryRepresentation::TaggedPointer(),
                                     offsetof(WasmImportData, native_context_));

    OpIndex promise_ctor = __ LoadFixedArrayElement(
        native_context, Context::PROMISE_FUNCTION_INDEX);

    OpIndex promise_resolve =
        this->GetBuiltinPointerTarget(Builtin::kPromiseResolve);
    auto* resolve_call_desc =
        GetBuiltinCallDescriptor(Builtin::kPromiseResolve, __ graph_zone());
    base::SmallVector<OpIndex, 16> resolve_args{promise_ctor, value,
                                                native_context};
    OpIndex promise = __ Call(promise_resolve, OpIndex::Invalid(),
                              base::VectorOf(resolve_args), resolve_call_desc);

    V<Object> on_fulfilled = __ Load(suspender, LoadOp::Kind::TaggedBase(),
                                     MemoryRepresentation::TaggedPointer(),
                                     offsetof(WasmSuspenderObject, resume_));
    V<Object> on_rejected = __ Load(suspender, LoadOp::Kind::TaggedBase(),
                                    MemoryRepresentation::TaggedPointer(),
                                    offsetof(WasmSuspenderObject, reject_));

    OpIndex promise_then =
        this->GetBuiltinPointerTarget(Builtin::kPerformPromiseThen);
    auto* then_call_desc =
        GetBuiltinCallDescriptor(Builtin::kPerformPromiseThen, __ graph_zone());
    V<WordPtr> isolate = __ IsolateField(IsolateFieldId::kIsolateAddress);
    V<Word32> promise_hook_flags = __ Load(
        isolate, LoadOp::Kind::RawAligned().NotLoadEliminable(),
        MemoryRepresentation::Uint32(), Isolate::promise_hook_flags_offset());
    // LINT.IfChange(PromiseHookFlags)
    constexpr uint32_t kHookMask =
        Isolate::PromiseHookFields::HasIsolatePromiseHook::kMask |
        Isolate::PromiseHookFields::HasAsyncEventDelegate::kMask |
        Isolate::PromiseHookFields::IsDebugActive::kMask;
    // LINT.ThenChange(../../codegen/code-stub-assembler.cc:PromiseHookFlags)
    V<Word32> needs_hook = __ Word32BitwiseAnd(promise_hook_flags, kHookMask);

    ScopedVar<Object> var_throwaway(this, __ UndefinedConstant());
    IF (UNLIKELY(needs_hook)) {
      var_throwaway =
          __ WasmCallRuntime(__ graph_zone(), Runtime::kWasmSuspended,
                             {promise, suspender}, native_context);
    }

    base::SmallVector<OpIndex, 16> args{promise, on_fulfilled, on_rejected,
                                        var_throwaway, native_context};
    __ Call(promise_then, OpIndex::Invalid(), base::VectorOf(args),
            then_call_desc);

    OpIndex suspend = GetTargetForBuiltinCall(Builtin::kWasmSuspend);
    auto* suspend_call_descriptor =
        GetBuiltinCallDescriptor(Builtin::kWasmSuspend, __ graph_zone());
    this->BuildSwitchBackFromCentralStack(*old_sp, old_limit);
    V<Object> resolved =
        __ template Call<Object>(suspend, {suspender}, suspend_call_descriptor);
    old_sp_var = this->BuildSwitchToTheCentralStack(old_limit);
    result = resolved;

    *old_sp = old_sp_var;
    return result;
  }

  V<FixedArray> BuildMultiReturnFixedArrayFromIterable(OpIndex iterable,
                                                       V<Context> context) {
    V<Smi> length = __ SmiConstant(Smi::FromIntptr(sig_->return_count()));
    return CallBuiltin<IterableToFixedArrayForWasmDescriptor>(
        Builtin::kIterableToFixedArrayForWasm, Operator::kNoProperties,
        iterable, length, context);
  }

  void SafeStore(int offset, CanonicalValueType type, OpIndex base,
                 OpIndex value) {
    int alignment = offset % type.value_kind_full_size();
    auto rep = MemoryRepresentation::FromMachineRepresentation(
        type.machine_representation());
    if (COMPRESS_POINTERS_BOOL && rep.IsCompressibleTagged()) {
      // We are storing tagged value to off-heap location, so we need to store
      // it as a full word otherwise we will not be able to decompress it.
      rep = MemoryRepresentation::UintPtr();
      value = __ BitcastTaggedToWordPtr(value);
    }
    StoreOp::Kind store_kind =
        alignment == 0 || compiler::turboshaft::SupportedOperations::
                              IsUnalignedStoreSupported(rep)
            ? StoreOp::Kind::RawAligned()
            : StoreOp::Kind::RawUnaligned();
    __ Store(base, value, store_kind, rep, compiler::kNoWriteBarrier, offset);
  }

  V<WordPtr> BuildLoadCallTargetFromExportedFunctionData(
      V<WasmFunctionData> function_data) {
    // TODO(sroettger): this code should do a signature check, but it's only
    // used for CAPI.
    V<WasmInternalFunction> internal =
        V<WasmInternalFunction>::Cast(__ LoadProtectedPointerField(
            function_data, LoadOp::Kind::TaggedBase().Immutable(),
            offsetof(WasmFunctionData, protected_internal_)));
    V<Word32> code_pointer = __ Load(
        internal, LoadOp::Kind::TaggedBase(), MemoryRepresentation::Uint32(),
        offsetof(WasmInternalFunction, raw_call_target_));
    constexpr size_t entry_size_log2 =
        std::bit_width(sizeof(wasm::WasmCodePointerTableEntry)) - 1;
    return __ Load(
        __ ExternalConstant(ExternalReference::wasm_code_pointer_table()),
        __ ChangeUint32ToUintPtr(code_pointer), LoadOp::Kind::RawAligned(),
        MemoryRepresentation::UintPtr(), 0, entry_size_log2);
  }

  const OpIndex SafeLoad(OpIndex base, int offset, CanonicalValueType type) {
    int alignment = offset % type.value_kind_full_size();
    auto rep = MemoryRepresentation::FromMachineRepresentation(
        type.machine_representation());
    if (COMPRESS_POINTERS_BOOL && rep.IsCompressibleTagged()) {
      // We are loading tagged value from off-heap location, so we need to load
      // it as a full word otherwise we will not be able to decompress it.
      rep = MemoryRepresentation::UintPtr();
    }
    LoadOp::Kind load_kind = alignment == 0 ||
                                     compiler::turboshaft::SupportedOperations::
                                         IsUnalignedLoadSupported(rep)
                                 ? LoadOp::Kind::RawAligned()
                                 : LoadOp::Kind::RawUnaligned();
    return __ Load(base, load_kind, rep, offset);
  }

 private:
  bool is_inlining_into_js_;
  const wasm::CanonicalSig* const sig_;
  std::optional<compiler::turboshaft::WasmInlinedFunctionData>
      inlined_function_data_;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

void BuildWasmWrapper(PipelineData* data, Graph& graph,
                      const wasm::CanonicalSig* sig,
                      wasm::WrapperCompilationInfo wrapper_info,
                      DirectHandle<JSReceiver> callable = {});

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_WRAPPERS_H_
