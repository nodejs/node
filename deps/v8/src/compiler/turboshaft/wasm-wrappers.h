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
          // The source position here is needed for asm.js, see the comment on
          // the source position of the call to JavaScript in the wasm-to-js
          // wrapper.
          __ output_graph().source_positions()[result] = SourcePosition(1);
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
          // The source position here is needed for asm.js, see the comment on
          // the source position of the call to JavaScript in the wasm-to-js
          // wrapper.
          __ output_graph().source_positions()[result] = SourcePosition(1);
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
      // The source position here is needed for asm.js, see the comment on
      // the source position of the call to JavaScript in the wasm-to-js
      // wrapper.
      __ output_graph().source_positions()[result] = SourcePosition(1);
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
  OpIndex FromJS(V<Object> input, V<Context> context, CanonicalValueType type,
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
        // TODO(14034): Add more fast paths?
        case GenericKind::kExtern: {
          if (type.is_non_nullable()) {
            IF (UNLIKELY(__ TaggedEqual(
                    input, __ template LoadRoot<RootIndex::kNullValue>()))) {
              __ WasmCallRuntime(__ phase_zone(),
                                 Runtime::kWasmThrowJSTypeError, {}, context);
              __ Unreachable();
            }
          }
          if (v8_flags.wasm_shared && type.is_shared()) {
            Label<Object> done(&Asm());
            IF (__ IsSmi(input)) {
              GOTO(done, input);
            }
#if CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
            // Bail out for read-only objects.
            V<Word32> lower32 = __ TruncateWordPtrToWord32(
                __ BitcastTaggedToWordPtr(V<HeapObject>::Cast(input)));
            IF (__ Uint32LessThan(lower32,
                                  __ Word32Constant(static_cast<uint32_t>(
                                      kContiguousReadOnlyReservationSize)))) {
              GOTO(done, input);
            }
            // Bail out for already-shared objects.
            V<WordPtr> flags = __ LoadPageFlags(V<HeapObject>::Cast(input));
            V<WordPtr> page_flags = __ WordPtrBitwiseAnd(
                flags, static_cast<uintptr_t>(MemoryChunk::kInSharedHeap));
#else   // !CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
            V<WordPtr> flags = __ LoadPageFlags(V<HeapObject>::Cast(input));
            V<WordPtr> page_flags = __ WordPtrBitwiseAnd(
                flags, static_cast<uintptr_t>(
                           MemoryChunk::kIsReadOnlyOrSharedHeapMask));
#endif  // !CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
            IF (UNLIKELY(__ WordPtrEqual(page_flags, 0))) {
              // If it isn't shared, yet, use the runtime function.
              std::initializer_list<const OpIndex> inputs = {
                  input, __ IntPtrConstant(
                             IntToSmi(static_cast<int>(type.raw_bit_field())))};
              GOTO(done, __ WasmCallRuntime(__ phase_zone(),
                                            Runtime::kWasmJSToWasmObject,
                                            inputs, context));
            }
            GOTO(done, input);
            BIND(done, result);
            return result;
          }
          return input;
        }
        case GenericKind::kString:
          return BuildCheckString(input, context, type);

        case GenericKind::kNoExtern:
        case GenericKind::kNoFunc:
        case GenericKind::kNone:
        case GenericKind::kFunc:
        case GenericKind::kAny:
        case GenericKind::kEq:
        case GenericKind::kI31:
        case GenericKind::kStruct:
        case GenericKind::kArray:
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
    // Both indexed and allow-listed generic references get here.

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
