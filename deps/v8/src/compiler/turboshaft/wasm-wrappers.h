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

  void CheckAndConvertSharedString(V<Object> ret, ScopedVar<Object>& result);

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

  V<Object> BuildCheckString(V<Object> input, V<Context> js_context,
                             CanonicalValueType type) {
    Block* done = __ NewBlock();
    Block* type_error = __ NewBlock();
    ScopedVar<Object> result(this,
                             __ template LoadRoot<RootIndex::kWasmNull>());
    __ GotoIf(__ IsSmi(input), type_error, BranchHint::kFalse);
    if (type.is_nullable()) {
      Block* not_null = __ NewBlock();
      __ GotoIfNot(
          __ TaggedEqual(input, __ template LoadRoot<RootIndex::kNullValue>()),
          not_null);
      __ Goto(done);
      __ Bind(not_null);
    }
    V<Map> map = LoadMap(input);
    V<Word32> instance_type = __ LoadInstanceTypeField(map);
    V<Word32> check = __ Uint32LessThan(
        instance_type, __ Word32Constant(FIRST_NONSTRING_TYPE));
    result = input;
    __ GotoIf(check, done, BranchHint::kTrue);
    __ Goto(type_error);
    __ Bind(type_error);
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       js_context);
    __ Unreachable();
    __ Bind(done);
    return result;
  }

  // Returns the Word32 value equal to `input` if `input` is in i31 range,
  // otherwise jumps to `not_i31`.
  void CanonicalizeHeapNumber(V<HeapNumber> input, Block* not_i31,
                              ScopedVar<Object>& result) {
    V<Float64> float_value = __ LoadHeapNumberValue(input);

    // Check if value is integral.
    V<Word32> int_value =
        __ TruncateFloat64ToInt32OverflowUndefined(float_value);
    V<Word32> is_integral =
        __ Float64Equal(float_value, __ ChangeInt32ToFloat64(int_value));
    __ GotoIfNot(is_integral, not_i31);

    // Check if value is -0.
    Block* is_zero = __ NewBlock();
    Block* is_not_zero = __ NewBlock();
    Block* done = __ NewBlock();
    __ Branch(__ Word32Equal(int_value, 0), is_zero, is_not_zero);

    __ Bind(is_zero);
    V<Word32> is_minus_zero =
        __ Int32LessThan(__ Float64ExtractHighWord32(float_value), 0);
    __ Branch(is_minus_zero, not_i31, done);

    __ Bind(is_not_zero);
    // Check range of float value.
    V<Word32> in_range = __ Word32BitwiseAnd(
        __ Float64LessThanOrEqual(__ Float64Constant(wasm::kInt31MinValue),
                                  float_value),
        __ Float64LessThanOrEqual(float_value,
                                  __ Float64Constant(wasm::kInt31MaxValue)));
    __ Branch(in_range, done, not_i31);

    __ Bind(done);
    result = __ TagSmi(int_value);
  }

  // If the object is shared, sets `result = input`, otherwise falls back to
  // `fallback`.
  void EnsureObjectShareness(V<HeapObject> input, Block* done, Block* fallback,
                             ScopedVar<Object>& result) {
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
    // Bail out for read-only objects.
    V<Word32> lower32 =
        __ TruncateWordPtrToWord32(__ BitcastTaggedToWordPtr(input));
    IF (__ Uint32LessThan(lower32, __ Word32Constant(static_cast<uint32_t>(
                                       kContiguousReadOnlyReservationSize)))) {
      result = input;
      __ Goto(done);
    }
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
    IF (UNLIKELY(__ WordPtrEqual(page_flags, 0))) {
      // If it isn't shared, yet, use the runtime function.
      __ Goto(fallback);
    }
    result = input;
    __ Goto(done);
  }

  // Checks that the object is shared or not according to `type`. If not, jumps
  // to `type_error`. Assumes the object cannot be allocated in read-only space.
  void CheckWasmObjectSharedness(V<Object> input, CanonicalValueType type,
                                 Block* type_error) {
    if (v8_flags.wasm_shared) {
      V<WordPtr> flags = __ LoadPageFlags(V<HeapObject>::Cast(input));
      V<WordPtr> page_flags = __ WordPtrBitwiseAnd(
          flags, static_cast<uintptr_t>(MemoryChunk::kInSharedHeap));
      if (type.is_shared()) {
        __ GotoIf(__ WordPtrEqual(page_flags, 0), type_error);
      } else {
        __ GotoIfNot(__ WordPtrEqual(page_flags, 0), type_error);
      }
#ifdef DEBUG
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
      // Wasm GC objects (structs/arrays) are currently never allocated in
      // read-only space. Verify this invariant to guard against future changes
      // where constant Wasm objects might be placed in RO space.
      V<Word32> lower32 = __ TruncateWordPtrToWord32(
          __ BitcastTaggedToWordPtr(V<HeapObject>::Cast(input)));
      // TSA_DCHECK is only usable in isolate-dependent code.
      if (__ data()->isolate() != nullptr) {
        TSA_DCHECK(this, __ Uint32LessThanOrEqual(
                             __ Word32Constant(static_cast<uint32_t>(
                                 kContiguousReadOnlyReservationSize)),
                             lower32));
      }
#endif  // CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
#endif  // DEBUG
    }
  }

  void CheckSmiInI31Range(V<Smi> input, Block* done, Block* not_in_range,
                          ScopedVar<Object>& result) {
    if constexpr (SmiValuesAre31Bits()) {
      result = input;
      __ Goto(done);
    } else {
      V<Word32> val = __ UntagSmi(input);
      V<Word32> in_range = __ Word32BitwiseAnd(
          __ Int32LessThanOrEqual(wasm::kInt31MinValue, val),
          __ Int32LessThanOrEqual(val, wasm::kInt31MaxValue));
      result = input;
      __ GotoIf(in_range, done);
      __ Goto(not_in_range);
    }
  }

  V<Object> BuildCheckExternRef(V<Object> input, V<Context> context,
                                CanonicalValueType type) {
    if (type.is_non_nullable()) {
      IF (UNLIKELY(__ TaggedEqual(
              input, __ template LoadRoot<RootIndex::kNullValue>()))) {
        __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                           context);
        __ Unreachable();
      }
    }
    if (v8_flags.wasm_shared && type.is_shared()) {
      Block* done = __ NewBlock();
      Block* fallback = __ NewBlock();
      ScopedVar<Object> result(this, input);
      IF (__ IsSmi(input)) {
        __ Goto(done);
      }

      EnsureObjectShareness(V<HeapObject>::Cast(input), done, fallback, result);

      __ Bind(fallback);
      std::initializer_list<const OpIndex> inputs = {
          input,
          __ SmiConstant(Smi::FromInt(static_cast<int>(type.raw_bit_field())))};
      result = __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmJSToWasmObject,
                                  inputs, context);
      __ Goto(done);

      __ Bind(done);
      return result;
    } else {
      return input;
    }
  }

  V<Object> BuildCheckAnyRef(V<Object> input, V<Context> context,
                             CanonicalValueType type) {
    Block* done = __ NewBlock();
    Block* type_error = __ NewBlock();
    Block* process_smi = __ NewBlock();
    Block* process_heap_number = __ NewBlock();
    Block* ensure_sharedness = __ NewBlock();
    Block* fallback = __ NewBlock();

    DCHECK(type.use_wasm_null());
    ScopedVar<Object> result(this,
                             __ template LoadRoot<RootIndex::kWasmNull>());

    // null is not allowed for non-nullable (ref any).
    __ GotoIf(
        __ TaggedEqual(input, __ template LoadRoot<RootIndex::kNullValue>()),
        type.is_nullable() ? done : type_error);

    __ GotoIf(__ IsSmi(input), process_smi);

    __ GotoIf(__ HasInstanceType(input, HEAP_NUMBER_TYPE), process_heap_number);

    __ Goto(ensure_sharedness);

    __ Bind(process_smi);
    CheckSmiInI31Range(V<Smi>::Cast(input), done, fallback, result);

    __ Bind(process_heap_number);
    CanonicalizeHeapNumber(V<HeapNumber>::Cast(input), ensure_sharedness,
                           result);
    __ Goto(done);

    __ Bind(ensure_sharedness);
    if (v8_flags.wasm_shared && type.is_shared()) {
      EnsureObjectShareness(V<HeapObject>::Cast(input), done, fallback, result);
    } else {
      result = input;
      __ Goto(done);
    }

    __ Bind(fallback);
    // Make sure ValueType fits in a Smi.
    static_assert(wasm::ValueType::kLastUsedBit + 1 <= kSmiValueSize);
    std::initializer_list<const OpIndex> inputs = {
        input,
        __ SmiConstant(Smi::FromInt(static_cast<int>(type.raw_bit_field())))};
    result = __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmJSToWasmObject,
                                inputs, context);
    __ Goto(done);

    __ Bind(type_error);
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       context);
    __ Unreachable();

    __ Bind(done);
    return result;
  }

  V<Object> BuildCheckEqRef(V<Object> input, V<Context> js_context,
                            CanonicalValueType type) {
    Block* done = __ NewBlock();
    Block* type_error = __ NewBlock();
    Block* check_number = __ NewBlock();
    DCHECK(type.use_wasm_null());
    ScopedVar<Object> result(this,
                             __ template LoadRoot<RootIndex::kWasmNull>());

    if (type.is_nullable()) {
      __ GotoIf(
          __ TaggedEqual(input, __ template LoadRoot<RootIndex::kNullValue>()),
          done);
    }

    __ GotoIf(__ IsSmi(input), check_number);

    V<Map> map = LoadMap(input);
    V<Word32> instance_type = __ LoadInstanceTypeField(map);
    V<Word32> is_wasm_object =
        __ Word32BitwiseOr(__ Word32Equal(instance_type, WASM_STRUCT_TYPE),
                           __ Word32Equal(instance_type, WASM_ARRAY_TYPE));
    __ GotoIfNot(is_wasm_object, check_number, BranchHint::kTrue);

    CheckWasmObjectSharedness(input, type, type_error);

    result = input;
    __ Goto(done);

    __ Bind(check_number);
    BuildCheckI31Impl(input, result, done, type_error);

    __ Bind(type_error);
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       js_context);
    __ Unreachable();

    __ Bind(done);
    return result;
  }

  void BuildCheckI31Impl(V<Object> input, ScopedVar<Object>& result,
                         Block* done, Block* type_error) {
    Block* is_heap_object = __ NewBlock();
    __ GotoIfNot(__ IsSmi(input), is_heap_object);

    CheckSmiInI31Range(V<Smi>::Cast(input), done, type_error, result);

    __ Bind(is_heap_object);
    __ GotoIfNot(__ HasInstanceType(input, HEAP_NUMBER_TYPE), type_error);

    CanonicalizeHeapNumber(V<HeapNumber>::Cast(input), type_error, result);
    __ Goto(done);
  }

  V<Object> BuildCheckI31Ref(V<Object> input, V<Context> js_context,
                             CanonicalValueType type) {
    Block* done = __ NewBlock();
    Block* type_error = __ NewBlock();
    ScopedVar<Object> result(this,
                             __ template LoadRoot<RootIndex::kWasmNull>());

    if (type.is_nullable()) {
      __ GotoIf(
          __ TaggedEqual(input, __ template LoadRoot<RootIndex::kNullValue>()),
          done);
    }

    BuildCheckI31Impl(input, result, done, type_error);

    __ Bind(type_error);
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       js_context);
    __ Unreachable();

    __ Bind(done);
    return result;
  }

  V<Object> BuildCheckWasmObject(V<Object> input, V<Context> js_context,
                                 CanonicalValueType type,
                                 InstanceType instance_type) {
    Block* done = __ NewBlock();
    Block* type_error = __ NewBlock();
    DCHECK(type.use_wasm_null());
    ScopedVar<Object> result(this,
                             __ template LoadRoot<RootIndex::kWasmNull>());

    __ GotoIf(__ IsSmi(input), type_error, BranchHint::kFalse);

    if (type.is_nullable()) {
      __ GotoIf(
          __ TaggedEqual(input, __ template LoadRoot<RootIndex::kNullValue>()),
          done);
    }

    V<Map> map = LoadMap(input);
    V<Word32> is_wasm_object_of_instance_type =
        __ Word32Equal(__ LoadInstanceTypeField(map), instance_type);
    __ GotoIfNot(is_wasm_object_of_instance_type, type_error,
                 BranchHint::kTrue);

    CheckWasmObjectSharedness(input, type, type_error);

    result = input;
    __ Goto(done);

    __ Bind(type_error);
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       js_context);
    __ Unreachable();

    __ Bind(done);
    return result;
  }

  V<Object> BuildCheckIndexedStructOrArray(V<Object> input, V<Context> context,
                                           CanonicalValueType type) {
    Block* done = __ NewBlock();
    Block* mismatch = __ NewBlock();

    ScopedVar<Object> result(this,
                             __ template LoadRoot<RootIndex::kWasmNull>());

    __ GotoIf(__ IsSmi(input), mismatch, BranchHint::kFalse);

    if (type.is_nullable()) {
      __ GotoIf(
          __ TaggedEqual(input, __ template LoadRoot<RootIndex::kNullValue>()),
          done);
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

    IF (__ TaggedEqual(object_map, supertype_rtt)) {
      result = input;
      __ Goto(done);
    }

    if (type.is_exact()) {
      __ Goto(mismatch);
    } else {
      wasm::TypeCanonicalizer* type_canonicalizer =
          wasm::GetTypeCanonicalizer();

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

      V<Word32> has_type_info =
          __ Word32Equal(instance_type, expected_instance_type);

      IF (has_type_info) {
        uint8_t rtt_depth =
            type_canonicalizer->GetSubtypingDepth_Slow(type.ref_index());

        V<Object> type_info = __ LoadWasmTypeInfo(object_map);
        V<Word32> supertypes_length = __ UntagSmi(
            __ Load(type_info, LoadOp::Kind::TaggedBase().Immutable(),
                    MemoryRepresentation::TaggedSigned(),
                    offsetof(WasmTypeInfo, supertypes_length_)));

        IF (__ Uint32LessThan(rtt_depth, supertypes_length)) {
          V<Object> maybe_match = __ Load(
              type_info, LoadOp::Kind::TaggedBase().Immutable(),
              MemoryRepresentation::TaggedPointer(),
              WasmTypeInfo::kSupertypesOffset + kTaggedSize * rtt_depth);
          IF (__ TaggedEqual(maybe_match, supertype_rtt)) {
            result = input;
            __ Goto(done);
          }
        }
      }
      __ Goto(mismatch);
    }

    __ Bind(mismatch);
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       context);
    __ Unreachable();

    __ Bind(done);
    return result;
  }

  V<Float32> BuildChangeTaggedToFloat32(
      V<Object> value, V<Context> context,
      OptionalV<EagerFrameState> caller_frame_state) {
    DCHECK_EQ(is_inlining_into_js_, caller_frame_state.valid());
    ScopedVar<Float32> result(this, V<Float32>::Invalid());
    IF (__ IsSmi(value)) {
      // TODO(dlehmann,wasm-runtime): If `ChangeInt32ToFloat32(x)` is exactly
      // equivalent to `TruncateFloat64ToFloat32(ChangeInt32ToFloat64(x))`, we
      // could `TruncateFloat64ToFloat32(BuildChangeTaggedToFloat64(x))` and
      // get rid of this separate function, but I am not 100% sure whether that
      // is a valid optimization so we conservatively keep it.
      result = __ ChangeInt32ToFloat32(__ UntagSmi(V<Smi>::Cast(value)));
    } ELSE {
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
    DCHECK_EQ(is_inlining_into_js_, caller_frame_state.valid());
    ScopedVar<Float64> result(this, V<Float64>::Invalid());
    IF (__ IsSmi(value)) {
      result = __ ChangeInt32ToFloat64(__ UntagSmi(V<Smi>::Cast(value)));
    } ELSE {
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
    DCHECK_EQ(is_inlining_into_js_, caller_frame_state.valid());
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
    DCHECK_EQ(is_inlining_into_js_, caller_frame_state.valid());
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
    if (type.is_abstract_ref()) {
      // When inlining JS-to-Wasm wrappers, CanInlineJSToWasmCall() only
      // allows nullable, non-shared externref, so none of the paths below
      // that call runtime functions (which can throw) are reachable.
      // If CanInlineJSToWasmCall() is ever extended to allow more reference
      // types, the throwing paths would need a frame state to support
      // lazy deopt on throw.
      DCHECK_IMPLIES(caller_frame_state.valid(), type == wasm::kWasmExternRef);
      switch (type.generic_kind()) {
        // TODO(548685083): Add fast paths for function/continuation types?
        case GenericKind::kExtern:
          return BuildCheckExternRef(input, context, type);
        case GenericKind::kString:
          return BuildCheckString(input, context, type);
        case GenericKind::kStruct:
          return BuildCheckWasmObject(input, context, type, WASM_STRUCT_TYPE);
        case GenericKind::kArray:
          return BuildCheckWasmObject(input, context, type, WASM_ARRAY_TYPE);
        case GenericKind::kI31:
          return BuildCheckI31Ref(input, context, type);
        case GenericKind::kEq:
          return BuildCheckEqRef(input, context, type);
        case GenericKind::kAny:
          return BuildCheckAnyRef(input, context, type);
        case GenericKind::kNoExtern:
        case GenericKind::kNoFunc:
        case GenericKind::kNone: {
          if (type.is_nullable()) {
            Block* done = __ NewBlock();
            V<Object> js_null = __ template LoadRoot<RootIndex::kNullValue>();
            __ GotoIf(__ TaggedEqual(input, js_null), done);
            __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError,
                               {}, context);
            __ Unreachable();
            __ Bind(done);
            return type.use_wasm_null()
                       ? __ template LoadRoot<RootIndex::kWasmNull>()
                       : js_null;
          } else {
            __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError,
                               {}, context);
            __ Unreachable();
            return OptionalOpIndex::Nullopt();
          }
        }
        case GenericKind::kFunc:
          break;  // Fall through.

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
    // Both indexed types and remaining references to abstract types get here.

    if (type.has_index() &&
        (type.ref_type_kind() == wasm::RefTypeKind::kStruct ||
         type.ref_type_kind() == wasm::RefTypeKind::kArray) &&
        // TODO(manoskouk, jkummerow): Also implement descriptor support.
        !wasm::GetTypeCanonicalizer()->has_descriptor(type.ref_index())) {
      return BuildCheckIndexedStructOrArray(input, context, type);
    }

    // Make sure ValueType fits in a Smi.
    static_assert(wasm::ValueType::kLastUsedBit + 1 <= kSmiValueSize);

    std::initializer_list<const OpIndex> inputs = {
        input,
        __ IntPtrConstant(IntToSmi(static_cast<int>(type.raw_bit_field())))};
    return __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmJSToWasmObject,
                              inputs, context);
  }

  V<Map> LoadMap(V<Object> object) {
    // TODO(thibaudm): Handle map packing.
    V<Map> map_word = __ LoadMapField(object);
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

  // Must be called in the first block to emit the Parameter ops.
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

  OpIndex BuildReceiverNode(OpIndex callable_node, OpIndex native_context,
                            V<Undefined> undefined_node) {
    // Check function strict bit.
    V<SharedFunctionInfo> shared_function_info =
        LoadSharedFunctionInfo(callable_node);
    OpIndex flags = __ Load(shared_function_info, LoadOp::Kind::TaggedBase(),
                            MemoryRepresentation::Int32(),
                            offsetof(SharedFunctionInfo, flags_));
    OpIndex strict_check = __ Word32BitwiseAnd(
        flags, __ Word32Constant(SharedFunctionInfo::IsNativeBit::kMask |
                                 SharedFunctionInfo::IsStrictBit::kMask));

    // Load global receiver if sloppy else use undefined.
    ScopedVar<Object> strict_d(this, OpIndex::Invalid());
    IF (strict_check) {
      strict_d = undefined_node;
    } ELSE {
      strict_d =
          __ LoadFixedArrayElement(native_context, Context::GLOBAL_PROXY_INDEX);
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
    int alignment = offset % type.value_kind_size();
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
    int alignment = offset % type.value_kind_size();
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
