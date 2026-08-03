// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WRAPPERS_INL_H_
#define V8_WASM_WRAPPERS_INL_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/wasm/wrappers.h"

namespace v8::internal::wasm {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::AbortIfNot(
    V<Word32> condition, AbortReason abort_reason) {
  if (!v8_flags.debug_code) return;
  IF_NOT (condition) {
    V<Number> message_id =
        __ NumberConstant(static_cast<int32_t>(abort_reason));
    __ WasmCallRuntime(__ phase_zone(), Runtime::kAbort, {message_id},
                       __ NoContextConstant());
  }
}

template <typename Assembler>
auto WasmWrapperTSGraphBuilder<Assembler>::BuildChangeInt32ToNumber(
    compiler::turboshaft::V<Word32> value) -> V<Number> {
  // We expect most integers at runtime to be Smis, so it is important for
  // wrapper performance that Smi conversion be inlined.
  if (SmiValuesAre32Bits()) {
    return BuildChangeInt32ToSmi(value);
  }
  DCHECK(SmiValuesAre31Bits());

  // Double value to test if value can be a Smi, and if so, to convert it.
  V<Tuple<Word32, Word32>> add = __ Int32AddCheckOverflow(value, value);
  V<Word32> ovf = __ template Projection<1>(add);
  ScopedVar<Number> result(this, OpIndex::Invalid());
  IF_NOT (UNLIKELY(ovf)) {
    // If it didn't overflow, the result is {2 * value} as pointer-sized
    // value.
    result = __ BitcastWordPtrToSmi(
        __ ChangeInt32ToIntPtr(__ template Projection<0>(add)));
  } ELSE{
    // Otherwise, call builtin, to convert to a HeapNumber.
    result = CallBuiltin<WasmInt32ToHeapNumberDescriptor>(
        Builtin::kWasmInt32ToHeapNumber, Operator::kNoProperties, value);
  }
  return result;
}

template <typename Assembler>
auto WasmWrapperTSGraphBuilder<Assembler>::ToJS(OpIndex ret,
                                                CanonicalValueType type,
                                                V<Context> context)
    -> V<Object> {
  switch (type.kind()) {
    case kI32:
      return BuildChangeInt32ToNumber(ret);
    case kI64:
      return this->BuildChangeInt64ToBigInt(ret,
                                            StubCallMode::kCallBuiltinPointer);
    case kF32:
      return BuildChangeFloat32ToNumber(ret);
    case kF64:
      return BuildChangeFloat64ToNumber(ret);
    case kRef:
      switch (type.heap_representation_non_shared()) {
        case HeapType::kEq:
        case HeapType::kI31:
        case HeapType::kStruct:
        case HeapType::kArray:
        case HeapType::kAny:
        case HeapType::kExtern:
        case HeapType::kString:
        case HeapType::kNone:
        case HeapType::kNoFunc:
        case HeapType::kNoExtern:
          return ret;
        case HeapType::kExn:
        case HeapType::kNoExn:
        case HeapType::kBottom:
        case HeapType::kTop:
        case HeapType::kStringViewWtf8:
        case HeapType::kStringViewWtf16:
        case HeapType::kStringViewIter:
          UNREACHABLE();
        case HeapType::kFunc:
        default:
          if (type.heap_representation_non_shared() == HeapType::kFunc ||
              GetTypeCanonicalizer()->IsFunctionSignature(type.ref_index())) {
            // Function reference. Extract the external function.
            V<WasmInternalFunction> internal =
                V<WasmInternalFunction>::Cast(__ LoadTrustedPointerField(
                    ret, LoadOp::Kind::TaggedBase(),
                    kWasmInternalFunctionIndirectPointerTag,
                    WasmFuncRef::kTrustedInternalOffset));
            ScopedVar<Object> maybe_external(
                this, __ Load(internal, LoadOp::Kind::TaggedBase(),
                              MemoryRepresentation::TaggedPointer(),
                              WasmInternalFunction::kExternalOffset));
            IF (__ TaggedEqual(maybe_external, LOAD_ROOT(UndefinedValue))) {
              maybe_external =
                  CallBuiltin<WasmInternalFunctionCreateExternalDescriptor>(
                      Builtin::kWasmInternalFunctionCreateExternal,
                      Operator::kNoProperties, internal, context);
            }
            return maybe_external;
          } else {
            return ret;
          }
      }
    case kRefNull:
      switch (type.heap_representation_non_shared()) {
        case HeapType::kExtern:
        case HeapType::kNoExtern:
          return ret;
        case HeapType::kNone:
        case HeapType::kNoFunc:
          return LOAD_ROOT(NullValue);
        case HeapType::kExn:
        case HeapType::kNoExn:
          UNREACHABLE();
        case HeapType::kEq:
        case HeapType::kStruct:
        case HeapType::kArray:
        case HeapType::kString:
        case HeapType::kI31:
        case HeapType::kAny: {
          ScopedVar<Object> result(this, OpIndex::Invalid());
          IF_NOT (__ TaggedEqual(ret, LOAD_ROOT(WasmNull))) {
            result = ret;
          } ELSE{
            result = LOAD_ROOT(NullValue);
          }
          return result;
        }
        case HeapType::kFunc:
        default: {
          if (type.heap_representation_non_shared() == HeapType::kFunc ||
              GetTypeCanonicalizer()->IsFunctionSignature(type.ref_index())) {
            ScopedVar<Object> result(this, OpIndex::Invalid());
            IF (__ TaggedEqual(ret, LOAD_ROOT(WasmNull))) {
              result = LOAD_ROOT(NullValue);
            } ELSE{
              V<WasmInternalFunction> internal =
                  V<WasmInternalFunction>::Cast(__ LoadTrustedPointerField(
                      ret, LoadOp::Kind::TaggedBase(),
                      kWasmInternalFunctionIndirectPointerTag,
                      WasmFuncRef::kTrustedInternalOffset));
              V<Object> maybe_external =
                  __ Load(internal, LoadOp::Kind::TaggedBase(),
                          MemoryRepresentation::AnyTagged(),
                          WasmInternalFunction::kExternalOffset);
              IF (__ TaggedEqual(maybe_external, LOAD_ROOT(UndefinedValue))) {
                V<Object> from_builtin =
                    CallBuiltin<WasmInternalFunctionCreateExternalDescriptor>(
                        Builtin::kWasmInternalFunctionCreateExternal,
                        Operator::kNoProperties, internal, context);
                result = from_builtin;
              } ELSE{
                result = maybe_external;
              }
            }
            return result;
          } else {
            ScopedVar<Object> result(this, OpIndex::Invalid());
            IF (__ TaggedEqual(ret, LOAD_ROOT(WasmNull))) {
              result = LOAD_ROOT(NullValue);
            } ELSE{
              result = ret;
            }
            return result;
          }
        }
      }
    case kI8:
    case kI16:
    case kF16:
    case kS128:
    case kVoid:
    case kTop:
    case kBottom:
      // If this is reached, then IsJSCompatibleSignature() is too permissive.
      UNREACHABLE();
  }
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildCallWasmFromWrapper(
    Zone* zone, const CanonicalSig* sig, V<Word32> callee,
    const base::Vector<OpIndex> args, base::Vector<OpIndex> returns,
    OptionalV<FrameState> frame_state) {
  const bool needs_frame_state = frame_state.valid();
  const TSCallDescriptor* descriptor = TSCallDescriptor::Create(
      compiler::GetWasmCallDescriptor(
          __ graph_zone(), sig, compiler::WasmCallKind::kWasmIndirectFunction,
          needs_frame_state),
      compiler::CanThrow::kYes, compiler::LazyDeoptOnThrow::kNo,
      __ graph_zone());

  OpIndex call = __ Call(callee, frame_state, base::VectorOf(args), descriptor,
                         OpEffects().CanCallAnything());

  if (sig->return_count() == 1) {
    returns[0] = call;
  } else if (sig->return_count() > 1) {
    for (uint32_t i = 0; i < sig->return_count(); i++) {
      CanonicalValueType type = sig->GetReturn(i);
      returns[i] = __ Projection(call, i, this->RepresentationFor(type));
    }
  }
}

template <typename Assembler>
auto WasmWrapperTSGraphBuilder<Assembler>::BuildCallAndReturn(
    V<Context> js_context, V<HeapObject> function_data,
    base::Vector<OpIndex> args, bool do_conversion,
    OptionalV<FrameState> frame_state) -> OpIndex {
  const int rets_count = static_cast<int>(sig_->return_count());
  base::SmallVector<OpIndex, 1> rets(rets_count);

  V<WasmInternalFunction> internal =
      V<WasmInternalFunction>::Cast(__ LoadProtectedPointerField(
          function_data, LoadOp::Kind::TaggedBase().Immutable(),
          WasmExportedFunctionData::kProtectedInternalOffset));
  auto [target, implicit_arg] =
      this->BuildFunctionTargetAndImplicitArg(internal);
  args[0] = implicit_arg;
  BuildCallWasmFromWrapper(__ phase_zone(), sig_, target, args,
                           base::VectorOf(rets), frame_state);

  V<Object> jsval;
  if (sig_->return_count() == 0) {
    jsval = LOAD_ROOT(UndefinedValue);
  } else if (sig_->return_count() == 1) {
    jsval =
        do_conversion ? ToJS(rets[0], sig_->GetReturn(), js_context) : rets[0];
  } else {
    int32_t return_count = static_cast<int32_t>(sig_->return_count());
    V<Smi> size = __ SmiConstant(Smi::FromInt(return_count));

    jsval = BuildCallAllocateJSArray(size, js_context);

    V<FixedArray> fixed_array = __ Load(jsval, LoadOp::Kind::TaggedBase(),
                                        MemoryRepresentation::TaggedPointer(),
                                        JSObject::kElementsOffset);

    for (int i = 0; i < return_count; ++i) {
      V<Object> value = ToJS(rets[i], sig_->GetReturn(i), js_context);
      __ StoreFixedArrayElement(fixed_array, i, value,
                                compiler::kFullWriteBarrier);
    }
  }
  return jsval;
}

template <typename Assembler>
auto WasmWrapperTSGraphBuilder<Assembler>::InlineWasmFunctionInsideWrapper(
    V<Context> js_context, V<WasmFunctionData> function_data,
    base::Vector<OpIndex> inlined_args, bool do_conversion,
    OptionalV<FrameState> frame_state) -> V<Object> {
  if constexpr (requires(const Assembler& assembler) {
                  assembler.has_wasm_in_js_inlining_reducer;
                }) {
    if (inlined_function_data_.has_value()) {
      CHECK(v8_flags.turboshaft_wasm_in_js_inlining);
      OptionalV<Any> wasmval =
          static_cast<Assembler*>(&Asm())->TryInlineWasmCall(
              inlined_function_data_->native_module,
              inlined_function_data_->function_index, inlined_args);
      if (wasmval.has_value()) {
        DCHECK_LE(sig_->return_count(), 1);
        if (sig_->return_count() == 0) {
          return LOAD_ROOT(UndefinedValue);
        } else {  // sig_->return_count() == 1.
          return ToJS(wasmval.value(), sig_->GetReturn(), js_context);
        }
      }
    }
  }

  // If the wasm function was not inlined, we need to call it.
  return BuildCallAndReturn(js_context, function_data, inlined_args,
                            do_conversion, frame_state);
}

template <typename Assembler>
auto WasmWrapperTSGraphBuilder<Assembler>::BuildJSToWasmWrapperImpl(
    bool receiver_is_first_param, V<JSFunction> js_closure,
    V<Context> js_context, base::Vector<const OpIndex> arguments,
    OptionalV<FrameState> frame_state) -> V<Any> {
  const bool do_conversion = true;
  const int wasm_param_count = static_cast<int>(sig_->parameter_count());
  const int args_count = wasm_param_count + 1;  // +1 for wasm_code.

  __ Bind(__ NewBlock());

  base::SmallVector<OpIndex, 16> params(args_count);
  const int param_offset = receiver_is_first_param ? 0 : 1;
  if (js_closure.valid()) {
    DCHECK(js_context.valid());

    // Prepare Param() nodes. Param() nodes can only be created once, so we
    // need to use the same nodes along all possible transformation paths.
    for (int i = 0; i < wasm_param_count; ++i) {
      params[i] = arguments[i + param_offset];
    }
  } else {
    // Create the js_closure and js_context parameters.
    js_closure = __ Parameter(compiler::Linkage::kJSCallClosureParamIndex,
                              RegisterRepresentation::Tagged());
    js_context = __ Parameter(
        compiler::Linkage::GetJSCallContextParamIndex(wasm_param_count + 1),
        RegisterRepresentation::Tagged());

    // Prepare Param() nodes.
    for (int i = 0; i < wasm_param_count; ++i) {
      params[i] =
          __ Parameter(i + param_offset, RegisterRepresentation::Tagged());
    }
  }

  if (!IsJSCompatibleSignature(sig_)) {
    // Throw a TypeError. Use the js_context of the calling javascript
    // function (passed as a parameter), such that the generated code is
    // js_context independent.
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       js_context);
    __ Unreachable();
    return OpIndex::Invalid();
  }

  // Check whether the signature of the function allows for a fast
  // transformation (if any params exist that need transformation).
  // Create a fast transformation path, only if it does.
  bool include_fast_path =
      do_conversion && wasm_param_count > 0 && QualifiesForFastTransform();

  V<SharedFunctionInfo> shared = __ Load(js_closure, LoadOp::Kind::TaggedBase(),
                                         MemoryRepresentation::TaggedPointer(),
                                         JSFunction::kSharedFunctionInfoOffset);
  V<WasmFunctionData> function_data =
      V<WasmFunctionData>::Cast(__ LoadTrustedPointerField(
          shared, LoadOp::Kind::TaggedBase(),
          kWasmFunctionDataIndirectPointerTag,
          SharedFunctionInfo::kTrustedFunctionDataOffset));
  // If we are not inlining, we don't need the wasm instance.
  V<WasmTrustedInstanceData> instance_data =
      inlined_function_data_.has_value()
          ? V<WasmTrustedInstanceData>::Cast(__ LoadProtectedPointerField(
                function_data, LoadOp::Kind::TaggedBase(),
                WasmExportedFunctionData::kProtectedInstanceDataOffset))
          : OpIndex::Invalid();

  Label<Object> done(&Asm());
  V<Object> jsval;
  if (include_fast_path) {
    TSBlock* slow_path = __ NewBlock();
    // Check if the params received on runtime can be actually transformed
    // using the fast transformation. When a param that cannot be transformed
    // fast is encountered, skip checking the rest and fall back to the slow
    // path.
    for (int i = 0; i < wasm_param_count; ++i) {
      CanTransformFast(params[i], sig_->GetParam(i), slow_path);
    }
    // Convert JS parameters to wasm numbers using the fast transformation
    // and build the call.
    base::SmallVector<OpIndex, 16> args(args_count);
    args[0] = instance_data;
    for (int i = 0; i < wasm_param_count; ++i) {
      OpIndex wasm_param = FromJSFast(params[i], sig_->GetParam(i));
      args[i + 1] = wasm_param;
    }

    // Inline the wasm function, if possible.
    jsval = InlineWasmFunctionInsideWrapper(
        js_context, function_data, VectorOf(args), do_conversion, frame_state);

    GOTO(done, jsval);
    __ Bind(slow_path);
  }

  // Convert JS parameters to wasm numbers using the default transformation
  // and build the call.
  base::SmallVector<OpIndex, 16> args(args_count);
  args[0] = instance_data;
  for (int i = 0; i < wasm_param_count; ++i) {
    if (do_conversion) {
      args[i + 1] =
          FromJS(params[i], js_context, sig_->GetParam(i), frame_state);
    } else {
      OpIndex wasm_param = params[i];

      // For Float32 parameters
      // we set UseInfo::CheckedNumberOrOddballAsFloat64 in
      // simplified-lowering and we need to add here a conversion from Float64
      // to Float32.
      if (sig_->GetParam(i).kind() == kF32) {
        wasm_param = __ TruncateFloat64ToFloat32(wasm_param);
      }
      args[i + 1] = wasm_param;
    }
  }

  // Inline the wasm function, if possible.
  jsval = InlineWasmFunctionInsideWrapper(
      js_context, function_data, VectorOf(args), do_conversion, frame_state);

  // If both the default and a fast transformation paths are present,
  // get the return value based on the path used.
  if (include_fast_path) {
    GOTO(done, jsval);
    BIND(done, result);
    return result;
  } else {
    return jsval;
  }
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildJSToWasmWrapper(
    bool receiver_is_first_param) {
  V<Any> result = BuildJSToWasmWrapperImpl(
      receiver_is_first_param, OpIndex::Invalid(), OpIndex::Invalid(), {}, {});
  if (result != OpIndex::Invalid()) {  // Invalid signature.
    __ Return(result);
  }
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildWasmToJSWrapper(
    ImportCallKind kind, int expected_arity, Suspend suspend) {
  int wasm_count = static_cast<int>(sig_->parameter_count());

  __ Bind(__ NewBlock());
  base::SmallVector<OpIndex, 16> wasm_params(wasm_count);
  OpIndex ref = __ Parameter(0, RegisterRepresentation::Tagged());
  for (int i = 0; i < wasm_count; ++i) {
    RegisterRepresentation rep = this->RepresentationFor(sig_->GetParam(i));
    wasm_params[i] = (__ Parameter(1 + i, rep));
  }

  V<Context> native_context = __ Load(ref, LoadOp::Kind::TaggedBase(),
                                      MemoryRepresentation::TaggedPointer(),
                                      WasmImportData::kNativeContextOffset);

  if (kind == ImportCallKind::kRuntimeTypeError) {
    // =======================================================================
    // === Runtime TypeError =================================================
    // =======================================================================
    __ WasmCallRuntime(this->zone_, Runtime::kWasmThrowJSTypeError, {},
                       native_context);
    __ Unreachable();
    return;
  }

  V<Undefined> undefined_node = LOAD_ROOT(UndefinedValue);
  int pushed_count = std::max(expected_arity, wasm_count);
  // 5 extra arguments: receiver, new target, arg count, dispatch handle and
  // context.
  bool has_dispatch_handle = kind == ImportCallKind::kUseCallBuiltin
                                 ? false
                                 : V8_JS_LINKAGE_INCLUDES_DISPATCH_HANDLE_BOOL;
  base::SmallVector<OpIndex, 16> args(pushed_count + 4 +
                                      (has_dispatch_handle ? 1 : 0));
  SBXCHECK_LT(
      args.size(),
      std::numeric_limits<
          decltype(compiler::turboshaft::Operation::input_count)>::max());
  // Position of the first wasm argument in the JS arguments.
  int pos = kind == ImportCallKind::kUseCallBuiltin ? 3 : 1;
  pos = AddArgumentNodes(base::VectorOf(args), pos, wasm_params, sig_,
                         native_context);
  for (int i = wasm_count; i < expected_arity; ++i) {
    args[pos++] = undefined_node;
  }

  V<JSFunction> callable_node = __ Load(ref, LoadOp::Kind::TaggedBase(),
                                        MemoryRepresentation::TaggedPointer(),
                                        WasmImportData::kCallableOffset);
  auto [old_sp, old_limit] = this->BuildSwitchToTheCentralStackIfNeeded();
  OpIndex suspender = OpIndex::Invalid();
  if (suspend == kSuspend) {
    suspender = __ Load(__ LoadRootRegister(), LoadOp::Kind::RawAligned(),
                        MemoryRepresentation::AnyUncompressedTagged(),
                        IsolateData::active_suspender_offset());

    if (v8_flags.stress_wasm_stack_switching) {
      V<Word32> for_stress_testing = __ TaggedEqual(
          __ LoadTaggedField(suspender, WasmSuspenderObject::kResumeOffset),
          LOAD_ROOT(UndefinedValue));
      IF (for_stress_testing) {
        __ WasmCallRuntime(__ phase_zone(), Runtime::kThrowWasmSuspendError, {},
                           native_context);
        __ Unreachable();
      }
    }

    // If {old_sp} is null, it must be that we were on the central stack
    // before entering the wasm-to-js wrapper, which means that there are JS
    // frames in the current suspender. JS frames cannot be suspended, so
    // trap.
    OpIndex active_stack_has_js_frames =
        __ WordPtrEqual(__ IntPtrConstant(0), old_sp);
    IF (active_stack_has_js_frames) {
      __ WasmCallRuntime(__ phase_zone(), Runtime::kThrowWasmSuspendError, {},
                         native_context);
      __ Unreachable();
    }
    if (v8_flags.experimental_wasm_wasmfx) {
      // Also check that potential inactive WasmFX stacks don't contain host
      // frames.
      OpIndex isolate = __ IsolateField(IsolateFieldId::kIsolateAddress);
      auto sig = FixedSizeSignature<MachineType>::Returns(MachineType::Int32())
                     .Params(MachineType::Pointer());
      OpIndex has_js_frames = this->CallC(
          &sig, ExternalReference::wasm_suspender_has_js_frames(), {isolate});
      IF (has_js_frames) {
        __ WasmCallRuntime(__ phase_zone(), Runtime::kThrowWasmSuspendError, {},
                           native_context);
        __ Unreachable();
      }
    }
  }

  OpIndex call = OpIndex::Invalid();
  switch (kind) {
    // =======================================================================
    // === JS Functions ======================================================
    // =======================================================================
    case ImportCallKind::kJSFunction: {
      auto call_descriptor = compiler::Linkage::GetJSCallDescriptor(
          __ graph_zone(), false, pushed_count + 1, CallDescriptor::kNoFlags);
      const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
          call_descriptor, compiler::CanThrow::kYes,
          compiler::LazyDeoptOnThrow::kNo, __ graph_zone());

      // Determine receiver at runtime.
      args[0] =
          BuildReceiverNode(callable_node, native_context, undefined_node);
      DCHECK_EQ(pos, pushed_count + 1);
      args[pos++] = undefined_node;  // new target
      args[pos++] =
          __ Word32Constant(JSParameterCount(wasm_count));  // argument count
#ifdef V8_JS_LINKAGE_INCLUDES_DISPATCH_HANDLE
      args[pos++] = __ Word32Constant(kPlaceholderDispatchHandle.value());
#endif
      args[pos++] = LoadContextFromJSFunction(callable_node);
      call = __ Call(callable_node, OpIndex::Invalid(), base::VectorOf(args),
                     ts_call_descriptor);
      break;
    }
    // =======================================================================
    // === General case of unknown callable ==================================
    // =======================================================================
    case ImportCallKind::kUseCallBuiltin: {
      DCHECK_EQ(expected_arity, wasm_count);
      OpIndex target =
          this->GetBuiltinPointerTarget(Builtin::kCall_ReceiverIsAny);
      args[0] = callable_node;
      args[1] =
          __ Word32Constant(JSParameterCount(wasm_count));  // argument count
      args[2] = undefined_node;                             // receiver

      auto call_descriptor = compiler::Linkage::GetStubCallDescriptor(
          __ graph_zone(), CallTrampolineDescriptor{}, wasm_count + 1,
          CallDescriptor::kNoFlags, Operator::kNoProperties,
          StubCallMode::kCallBuiltinPointer);
      const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
          call_descriptor, compiler::CanThrow::kYes,
          compiler::LazyDeoptOnThrow::kNo, __ graph_zone());

      // The native_context is sufficient here, because all kind of callables
      // which depend on the context provide their own context. The context
      // here is only needed if the target is a constructor to throw a
      // TypeError, if the target is a native function, or if the target is a
      // callable JSObject, which can only be constructed by the runtime.
      args[pos++] = native_context;
      call = __ Call(target, OpIndex::Invalid(), base::VectorOf(args),
                     ts_call_descriptor);
      break;
    }
    default:
      UNIMPLEMENTED();
  }
  // For asm.js the error location can differ depending on whether an
  // exception was thrown in imported JS code or an exception was thrown in
  // the ToNumber builtin that converts the result of the JS code a
  // WebAssembly value. The source position allows asm.js to determine the
  // correct error location. Source position 1 encodes the call to ToNumber,
  // source position 0 encodes the call to the imported JS code.
  __ output_graph().source_positions()[call] = SourcePosition(0);
  DCHECK(call.valid());

  if (suspend == kSuspend) {
    DCHECK_IMPLIES(!suspender.valid(), __ generating_unreachable_operations());
    call = BuildSuspend(call, ref, suspender, &old_sp, old_limit);
  }

  // Convert the return value(s) back.
  OpIndex val;
  base::SmallVector<OpIndex, 8> wasm_values;
  if (sig_->return_count() <= 1) {
    val = sig_->return_count() == 0
              ? __ Word32Constant(0)
              : FromJS(call, native_context, sig_->GetReturn());
  } else {
    V<FixedArray> fixed_array =
        BuildMultiReturnFixedArrayFromIterable(call, native_context);
    wasm_values.resize(sig_->return_count());
    for (unsigned i = 0; i < sig_->return_count(); ++i) {
      wasm_values[i] = FromJS(__ LoadFixedArrayElement(fixed_array, i),
                              native_context, sig_->GetReturn(i));
    }
  }
  this->BuildSwitchBackFromCentralStack(old_sp, old_limit);
  if (sig_->return_count() <= 1) {
    __ Return(val);
  } else {
    __ Return(__ Word32Constant(0), base::VectorOf(wasm_values));
  }
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildWasmStackEntryWrapper() {
  __ Bind(__ NewBlock());
  V<WordPtr> stack_metadata =
      __ Parameter(0, RegisterRepresentation::WordPtr());
  V<WasmFuncRef> func_ref = __ Load(stack_metadata, LoadOp::Kind::RawAligned(),
                                    MemoryRepresentation::TaggedPointer(),
                                    StackMemory::func_ref_offset());
  AbortIfNot(__ HasInstanceType(func_ref, WASM_FUNC_REF_TYPE),
             AbortReason::kUnexpectedInstanceType);
  V<WasmInternalFunction> internal_function =
      V<WasmInternalFunction>::Cast(__ LoadTrustedPointerField(
          func_ref, LoadOp::Kind::TaggedBase().Immutable(),
          kWasmInternalFunctionIndirectPointerTag,
          WasmFuncRef::kTrustedInternalOffset));
  auto [target, instance] =
      this->BuildFunctionTargetAndImplicitArg(internal_function);
  OpIndex arg = instance;
  BuildCallWasmFromWrapper(__ phase_zone(), sig_, target,
                           base::VectorOf(&arg, 1), {}, {});
  CallBuiltin<WasmFXReturnDescriptor>(Builtin::kWasmFXReturn,
                                      Operator::kNoProperties);
  __ Unreachable();
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildCapiCallWrapper() {
  __ Bind(__ NewBlock());
  base::SmallVector<OpIndex, 8> incoming_params;
  // Instance.
  incoming_params.push_back(__ Parameter(0, RegisterRepresentation::Tagged()));
  // Wasm parameters.
  for (int i = 0; i < static_cast<int>(sig_->parameter_count()); ++i) {
    incoming_params.push_back(
        __ Parameter(i + 1, this->RepresentationFor(sig_->GetParam(i))));
  }
  // Store arguments on our stack, then align the stack for calling to C.
  int param_bytes = 0;
  for (CanonicalValueType type : sig_->parameters()) {
    param_bytes += type.value_kind_size();
  }
  int return_bytes = 0;
  for (CanonicalValueType type : sig_->returns()) {
    return_bytes += type.value_kind_size();
  }

  int stack_slot_bytes = std::max(param_bytes, return_bytes);
  OpIndex values = stack_slot_bytes == 0
                       ? __ IntPtrConstant(0)
                       : __ StackSlot(stack_slot_bytes, kDoubleAlignment);

  int offset = 0;
  for (size_t i = 0; i < sig_->parameter_count(); ++i) {
    CanonicalValueType type = sig_->GetParam(i);
    // Start from the parameter with index 1 to drop the instance_node.
    // TODO(jkummerow): When a values is a reference type, we should pass it
    // in a GC-safe way, not just as a raw pointer.
    SafeStore(offset, type, values, incoming_params[i + 1]);
    offset += type.value_kind_size();
  }

  V<Object> function_node =
      __ LoadTaggedField(incoming_params[0], WasmImportData::kCallableOffset);
  V<HeapObject> shared = LoadSharedFunctionInfo(function_node);
  V<WasmFunctionData> function_data =
      V<WasmFunctionData>::Cast(__ LoadTrustedPointerField(
          shared, LoadOp::Kind::TaggedBase(),
          kWasmFunctionDataIndirectPointerTag,
          SharedFunctionInfo::kTrustedFunctionDataOffset));
  V<Object> host_data_foreign = __ LoadTaggedField(
      function_data, WasmCapiFunctionData::kEmbedderDataOffset);

  OpIndex isolate_root = __ LoadRootRegister();
  OpIndex fp_value = __ FramePointer();
  __ Store(isolate_root, fp_value, StoreOp::Kind::RawAligned(),
           MemoryRepresentation::UintPtr(), compiler::kNoWriteBarrier,
           Isolate::c_entry_fp_offset());

  V<WordPtr> call_target =
      BuildLoadCallTargetFromExportedFunctionData(function_data);

  // Parameters: Address host_data_foreign, Address arguments.
  auto host_sig =
      FixedSizeSignature<MachineType>::Returns(MachineType::Pointer())
          .Params(MachineType::AnyTagged(), MachineType::Pointer());
  OpIndex return_value =
      this->CallC(&host_sig, call_target, {host_data_foreign, values});

  IF_NOT (__ WordPtrEqual(return_value, __ IntPtrConstant(0))) {
    WasmRethrowExplicitContextDescriptor interface_descriptor;
    auto call_descriptor = compiler::Linkage::GetStubCallDescriptor(
        __ graph_zone(), interface_descriptor,
        interface_descriptor.GetStackParameterCount(), CallDescriptor::kNoFlags,
        Operator::kNoProperties, StubCallMode::kCallBuiltinPointer);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kYes,
        compiler::LazyDeoptOnThrow::kNo, __ graph_zone());
    OpIndex rethrow_call_target =
        GetTargetForBuiltinCall(Builtin::kWasmRethrowExplicitContext);
    V<Context> context = __ Load(incoming_params[0], LoadOp::Kind::TaggedBase(),
                                 MemoryRepresentation::TaggedPointer(),
                                 WasmImportData::kNativeContextOffset);
    __ Call(rethrow_call_target, {return_value, context}, ts_call_descriptor);
    __ Unreachable();
  }

  DCHECK_LT(sig_->return_count(), kV8MaxWasmFunctionReturns);
  size_t return_count = sig_->return_count();
  if (return_count == 0) {
    __ Return(__ Word32Constant(0));
  } else {
    base::SmallVector<OpIndex, 8> returns(return_count);
    offset = 0;
    for (size_t i = 0; i < return_count; ++i) {
      CanonicalValueType type = sig_->GetReturn(i);
      OpIndex val = SafeLoad(values, offset, type);
      returns[i] = val;
      offset += type.value_kind_size();
    }
    __ Return(__ Word32Constant(0), base::VectorOf(returns));
  }
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WRAPPERS_INL_H_
