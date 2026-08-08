// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_WRAPPERS_INL_H_
#define V8_COMPILER_TURBOSHAFT_WASM_WRAPPERS_INL_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/compiler/turboshaft/wasm-wrappers.h"
// Include the non-inl header before the rest of the headers.

#include "src/execution/simulator-base.h"
#include "src/objects/js-function.h"
#include "src/objects/object-predicates-inl.h"

namespace v8::internal::compiler::turboshaft {

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
  if constexpr (SmiValuesAre32Bits()) {
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
  } ELSE {
    // Otherwise, call builtin, to convert to a HeapNumber.
    result = CallBuiltin<WasmInt32ToHeapNumberDescriptor>(
        Builtin::kWasmInt32ToHeapNumber, Operator::kNoProperties, value);
  }
  return result;
}

template <typename Assembler>
auto WasmWrapperTSGraphBuilder<Assembler>::BuildToJSFunctionRef(
    V<WasmFuncRef> ret, V<Context> context) -> V<Object> {
  V<WasmInternalFunction> internal = V<WasmInternalFunction>::Cast(
      __ LoadTrustedPointer(ret, LoadOp::Kind::TaggedBase(),
                            kWasmInternalFunctionIndirectPointerTag,
                            WasmFuncRef::kTrustedInternalOffset));
  V<Object> maybe_external = __ Load(internal, LoadOp::Kind::TaggedBase(),
                                     MemoryRepresentation::TaggedPointer(),
                                     WasmInternalFunction::kExternalOffset);
  ScopedVar<Object> result(this, OpIndex::Invalid());
  IF (__ TaggedEqual(maybe_external,
                     __ template LoadRoot<RootIndex::kUndefinedValue>())) {
    result = CallBuiltin<WasmInternalFunctionCreateExternalDescriptor>(
        Builtin::kWasmInternalFunctionCreateExternal, Operator::kNoProperties,
        internal, context);
  } ELSE {
    result = maybe_external;
  }
  return result;
}

template <typename Assembler>
auto WasmWrapperTSGraphBuilder<Assembler>::ToJS(OpIndex ret,
                                                CanonicalValueType type,
                                                V<Context> context)
    -> V<Object> {
  if (type.is_numeric()) {
    switch (type.numeric_kind()) {
      case NumericKind::kI32:
        // When inlining into JS, emit a "high-level" JS conversion to allow
        // further optimizations. These are lowered in the MachineLoweringPhase
        // in the JS pipeline.
        return is_inlining_into_js_ ? __ ConvertInt32ToNumber(ret)
                                    : BuildChangeInt32ToNumber(ret);
      case NumericKind::kI64:
        return this->BuildChangeInt64ToBigInt(
            ret, StubCallMode::kCallBuiltinPointer);
      case NumericKind::kF32:
        return BuildChangeFloat32ToNumber(ret);
      case NumericKind::kF64:
        return BuildChangeFloat64ToNumber(ret);
      case NumericKind::kS128:
      case NumericKind::kI8:
      case NumericKind::kI16:
      case NumericKind::kF16:
        UNREACHABLE();
    }
  }

  if (type.ref_type_kind() == wasm::RefTypeKind::kFunction) {
    // Function reference. Extract the external function.
    if (type.is_nullable()) {
      ScopedVar<Object> result(this, OpIndex::Invalid());
      IF (__ TaggedEqual(ret, __ template LoadRoot<RootIndex::kWasmNull>())) {
        result = __ template LoadRoot<RootIndex::kNullValue>();
      } ELSE {
        result = BuildToJSFunctionRef(V<WasmFuncRef>::Cast(ret), context);
      }
      return result;
    } else {
      // Non-nullable funcref.
      return BuildToJSFunctionRef(V<WasmFuncRef>::Cast(ret), context);
    }
  }

  // Always null.
  if (type.is_none_type()) return __ template LoadRoot<RootIndex::kNullValue>();

  if (wasm::IsSubtypeOf(type, wasm::kWasmSharedExternRef)) {
    // We have to unshare shared strings before passing them to JS.
    ScopedVar<Object> result(this, OpIndex::Invalid());
    IF (__ IsSmi(ret)) {
      result = ret;
    } ELSE {
      OpIndex instance_type = LoadInstanceType(LoadMap(ret));
      V<Word32> is_string = __ Uint32LessThan(
          instance_type, __ Word32Constant(FIRST_NONSTRING_TYPE));
      IF (is_string) {
        // Since this is a shared externref, the string will be shared.
        result =
            __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmWasmToJSObject,
                               {ret}, __ NoContextConstant());
      } ELSE {
        result = ret;
      }
    }
    return result;
  }

  // At this point, we only have to process WasmNull.
  // These two cases are never WasmNull.
  if (!type.is_nullable()) return ret;
  if (!type.use_wasm_null()) return ret;

  // Nullable reference. Convert WasmNull if needed.
  ScopedVar<Object> result(this, OpIndex::Invalid());
  IF_NOT (__ TaggedEqual(ret, __ template LoadRoot<RootIndex::kWasmNull>())) {
    result = ret;
  } ELSE {
    result = __ template LoadRoot<RootIndex::kNullValue>();
  }
  return result;
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildCallWasmFromWrapper(
    Zone* zone, const wasm::CanonicalSig* sig, V<Word32> callee,
    const base::Vector<OpIndex> args, base::Vector<OpIndex> returns,
    OptionalV<FrameState> frame_state,
    compiler::LazyDeoptOnThrow lazy_deopt_on_throw) {
  const bool needs_frame_state = frame_state.valid();

  // If we have a current_catch_block() there is a catch handler, the reduction
  // was triggered from AssembleOutputGraphCheckException, and we don't need to
  // lazy-deoptimize. Otherwise we use the LazyDeoptOnThrow value from the call
  // descriptor of the Call we are inlining.
  DCHECK_IMPLIES(lazy_deopt_on_throw == compiler::LazyDeoptOnThrow::kYes,
                 !__ current_catch_block());
  const TSCallDescriptor* descriptor = TSCallDescriptor::Create(
      compiler::GetWasmCallDescriptor(
          __ graph_zone(), sig, compiler::WasmCallKind::kWasmIndirectFunction,
          needs_frame_state),
      compiler::CanThrow::kYes, lazy_deopt_on_throw, __ graph_zone());

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
auto WasmWrapperTSGraphBuilder<Assembler>::ConvertWasmResultsToJS(
    base::Vector<OpIndex> returns, V<Context> js_context) -> V<Object> {
  if (sig_->return_count() == 0) {
    return __ template LoadRoot<RootIndex::kUndefinedValue>();
  } else if (sig_->return_count() == 1) {
    return ToJS(returns[0], sig_->GetReturn(), js_context);
  } else {
    // We currently cannot inline Wasm functions with multi-return.
    DCHECK(!is_inlining_into_js_);
    int32_t return_count = static_cast<int32_t>(sig_->return_count());
    V<Smi> size = __ SmiConstant(Smi::FromInt(return_count));

    V<Object> jsval = BuildCallAllocateJSArray(size, js_context);

    V<FixedArray> fixed_array = __ Load(jsval, LoadOp::Kind::TaggedBase(),
                                        MemoryRepresentation::TaggedPointer(),
                                        JSObject::kElementsOffset);

    for (int i = 0; i < return_count; ++i) {
      V<Object> value = ToJS(returns[i], sig_->GetReturn(i), js_context);
      __ StoreFixedArrayElement(fixed_array, i, value,
                                compiler::kFullWriteBarrier);
    }
    return jsval;
  }
}

template <typename Assembler>
auto WasmWrapperTSGraphBuilder<Assembler>::BuildJSToWasmWrapper(
    V<JSFunction> js_closure, V<Context> js_context,
    base::Vector<const OpIndex> arguments,
    OptionalV<FrameState> lazy_frame_state,
    compiler::LazyDeoptOnThrow lazy_deopt_on_throw,
    OptionalV<FrameState> caller_frame_state) -> V<Any> {
  // JS-to-Wasm wrappers are compiled per isolate, so they can emit
  // isolate-dependent code.
  DCHECK_NOT_NULL(__ data()->isolate());

  // All parameters are set if and only if we are inlining the wrapper.
  DCHECK_EQ(is_inlining_into_js_, js_closure.valid());
  DCHECK_EQ(is_inlining_into_js_, js_context.valid());
  DCHECK_EQ(is_inlining_into_js_, lazy_frame_state.valid());
  DCHECK_IMPLIES(!is_inlining_into_js_, arguments.empty());
  DCHECK_IMPLIES(!is_inlining_into_js_,
                 lazy_deopt_on_throw == compiler::LazyDeoptOnThrow::kNo);
  // We only need a `caller_frame_state` if there actually are arguments to
  // convert.
  const int wasm_param_count = static_cast<int>(sig_->parameter_count());
  DCHECK_EQ(is_inlining_into_js_ && wasm_param_count > 0,
            caller_frame_state.valid());

  __ Bind(__ NewBlock());

  base::SmallVector<OpIndex, 16> params(wasm_param_count);
  const int param_offset = 1;  // Skip the JS receiver.
  if (is_inlining_into_js_) {
    // We only marked this call for inlining in Turbolev if the argument counts
    // matched already there.
    // Note that the JS `CallOp` in Turboshaft contains extra arguments at the
    // end (e.g. argument count, context), see `turbolev-graph-builder.cc`,
    // hence we `CHECK_GE` here.
    CHECK_GE(arguments.size(), wasm_param_count + param_offset);

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
    // Throw a TypeError. Use the js_context of the calling JavaScript
    // function (passed as a parameter), such that the generated code is
    // js_context independent.
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       js_context);
    __ Unreachable();
    return OpIndex::Invalid();
  }

  V<SharedFunctionInfo> sfi =
      __ Load(js_closure, LoadOp::Kind::TaggedBase().Immutable(),
              MemoryRepresentation::TaggedPointer(),
              JSFunction::kSharedFunctionInfoOffset);
  V<WasmFunctionData> function_data = V<WasmFunctionData>::Cast(
      __ LoadTrustedPointer(sfi, LoadOp::Kind::TaggedBase().Immutable(),
                            kWasmExportedFunctionDataIndirectPointerTag,
                            SharedFunctionInfo::kTrustedFunctionDataOffset));

  // If we are not inlining the Wasm body, we don't need the Wasm instance.
  V<WasmTrustedInstanceData> instance_data =
      inlined_function_data_.has_value()
          ? V<WasmTrustedInstanceData>::Cast(__ LoadProtectedPointerField(
                function_data, LoadOp::Kind::TaggedBase().Immutable(),
                WasmExportedFunctionData::kProtectedInstanceDataOffset))
          : OpIndex::Invalid();

  // Convert JS parameters to wasm numbers using the default transformation
  // and build the call.
  const int args_count = wasm_param_count + /* instance_data */ 1;
  base::SmallVector<OpIndex, 16> args(args_count);
  args[0] = instance_data;
  for (int i = 0; i < wasm_param_count; ++i) {
    args[i + 1] =
        FromJS(params[i], js_context, sig_->GetParam(i), caller_frame_state);
  }

  // Inline the Wasm function body, if possible.
  base::SmallVector<OpIndex, 1> returns(sig_->return_count());
  compiler::turboshaft::WasmBodyInliningResult inlining_result;
  if constexpr (requires { &Assembler::TryInlineWasmBody; }) {
    if (inlined_function_data_.has_value()) {
      CHECK(v8_flags.turboshaft_wasm_in_js_inlining);
      inlining_result = __ TryInlineWasmBody(
          inlined_function_data_.value(), VectorOf(args), lazy_deopt_on_throw);
      // If the body inlining traps unconditionally (e.g., due to an
      // unreachable) no need to produce/convert a result.
      if (__ generating_unreachable_operations()) {
        DCHECK(inlining_result.success);
        return OpIndex::Invalid();
      } else if (inlining_result.result.valid()) {
        DCHECK(inlining_result.success);
        DCHECK_EQ(returns.size(), 1);
        returns[0] = inlining_result.result.value();
      }
    }
  }

  // If the wasm function was not inlined, we need to call it.
  if (!inlining_result.success) {
    V<WasmInternalFunction> internal =
        V<WasmInternalFunction>::Cast(__ LoadProtectedPointerField(
            function_data, LoadOp::Kind::TaggedBase().Immutable(),
            WasmExportedFunctionData::kProtectedInternalOffset));
    auto [target, implicit_arg] =
        this->BuildFunctionTargetAndImplicitArg(internal);
    args[0] = implicit_arg;
    BuildCallWasmFromWrapper(__ phase_zone(), sig_, target, VectorOf(args),
                             base::VectorOf(returns), lazy_frame_state,
                             lazy_deopt_on_throw);
  }

  // In either case (Wasm body was inlined or not), we need to convert the
  // result(s) to JavaScript value(s).
  return ConvertWasmResultsToJS(base::VectorOf(returns), js_context);
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildJSToWasmWrapper() {
  DCHECK(!is_inlining_into_js_);
  V<Any> result =
      BuildJSToWasmWrapper(OpIndex::Invalid(), OpIndex::Invalid(), {}, {},
                           compiler::LazyDeoptOnThrow::kNo, OpIndex::Invalid());
  if (result.valid()) {  // Invalid signature.
    __ Return(result);
  }
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildWasmToJSWrapper(
    wasm::ImportCallKind kind, int expected_arity, wasm::Suspend suspend) {
  // Wasm-to-JS wrappers need to be isolate-independent (as of now).
  DCHECK_NULL(__ data()->isolate());
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

  if (kind == wasm::ImportCallKind::kRuntimeTypeError) {
    // =======================================================================
    // === Runtime TypeError =================================================
    // =======================================================================
    __ WasmCallRuntime(this->zone_, Runtime::kWasmThrowJSTypeError, {},
                       native_context);
    __ Unreachable();
    return;
  }

  V<Undefined> undefined_node =
      __ template LoadRoot<RootIndex::kUndefinedValue>();
  int pushed_count = std::max(expected_arity, wasm_count);
  // 5 extra arguments: receiver, new target, arg count, dispatch handle and
  // context.
  bool has_dispatch_handle = kind == wasm::ImportCallKind::kUseCallBuiltin
                                 ? false
                                 : V8_JS_LINKAGE_INCLUDES_DISPATCH_HANDLE_BOOL;
  base::SmallVector<OpIndex, 16> args(pushed_count + 4 +
                                      (has_dispatch_handle ? 1 : 0));
  SBXCHECK_LT(
      args.size(),
      std::numeric_limits<
          decltype(compiler::turboshaft::Operation::input_count)>::max());
  // Position of the first wasm argument in the JS arguments.
  int pos = kind == wasm::ImportCallKind::kUseCallBuiltin ? 3 : 1;
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
  if (suspend == wasm::kSuspend) {
    suspender = __ Load(__ LoadRootRegister(), LoadOp::Kind::RawAligned(),
                        MemoryRepresentation::AnyUncompressedTagged(),
                        IsolateData::active_suspender_offset());

    if (v8_flags.stress_wasm_stack_switching) {
      V<Word32> for_stress_testing = __ TaggedEqual(
          __ LoadTaggedField(suspender, WasmSuspenderObject::kResumeOffset),
          __ template LoadRoot<RootIndex::kUndefinedValue>());
      IF (for_stress_testing) {
        __ WasmCallRuntime(__ phase_zone(), Runtime::kThrowWasmJSPISuspendError,
                           {}, native_context);
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
      __ WasmCallRuntime(__ phase_zone(), Runtime::kThrowWasmJSPISuspendError,
                         {}, native_context);
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
        __ WasmCallRuntime(__ phase_zone(), Runtime::kThrowWasmJSPISuspendError,
                           {}, native_context);
        __ Unreachable();
      }
    }
  }

  OpIndex call = OpIndex::Invalid();
  switch (kind) {
    // =======================================================================
    // === JS Functions ======================================================
    // =======================================================================
    case wasm::ImportCallKind::kJSFunction: {
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
    case wasm::ImportCallKind::kUseCallBuiltin: {
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

  if (suspend == wasm::kSuspend) {
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
  V<WordPtr> arg_buffer = __ Parameter(1, RegisterRepresentation::WordPtr());
  V<WasmFuncRef> func_ref =
      __ Load(stack_metadata, LoadOp::Kind::RawAligned(),
              MemoryRepresentation::UncompressedTaggedPointer(),
              wasm::StackMemory::func_ref_offset());
  AbortIfNot(__ HasInstanceType(func_ref, WASM_FUNC_REF_TYPE),
             AbortReason::kUnexpectedInstanceType);
  V<WasmInternalFunction> internal_function = V<WasmInternalFunction>::Cast(
      __ LoadTrustedPointer(func_ref, LoadOp::Kind::TaggedBase().Immutable(),
                            kWasmInternalFunctionIndirectPointerTag,
                            WasmFuncRef::kTrustedInternalOffset));
  auto [target, instance] =
      this->BuildFunctionTargetAndImplicitArg(internal_function);

  base::Vector<OpIndex> args =
      __ phase_zone()
          -> template AllocateVector<OpIndex>(1 + sig_->parameter_count());
  args[0] = instance;
  // Unpack continuation params.
  IterateWasmFXArgBuffer(sig_->parameters(), [&](size_t index, int offset) {
    CanonicalValueType type = sig_->GetParam(index);
    // On-stack refs are uncompressed.
    MemoryRepresentation rep =
        type.is_ref()
            ? MemoryRepresentation::AnyUncompressedTagged()
            : MemoryRepresentation::FromMachineType(type.machine_type());
    args[index + 1] = __ LoadOffHeap(arg_buffer, offset, rep);
  });

  base::Vector<OpIndex> returns =
      __ phase_zone() -> template AllocateVector<OpIndex>(sig_->return_count());
  BuildCallWasmFromWrapper(__ phase_zone(), sig_, target, args, returns, {},
                           compiler::LazyDeoptOnThrow::kNo);

  auto [size, alignment] = GetBufferSizeAndAlignmentFor(sig_->returns());
  // The stack is not freed immediately on return, so the pointer stays valid
  // until its use in the parent stack.
  OpIndex result_buffer =
      __ StackSlot(size, std::max(2 * kSystemPointerSize, alignment));
  IterateWasmFXArgBuffer(sig_->returns(), [&](size_t index, int offset) {
    CanonicalValueType type = sig_->GetReturn(index);
    // On-stack refs are uncompressed.
    MemoryRepresentation rep =
        type.is_ref()
            ? MemoryRepresentation::AnyUncompressedTagged()
            : MemoryRepresentation::FromMachineType(type.machine_type());
    __ StoreOffHeap(result_buffer, returns[index], rep, offset);
  });

  CallBuiltin<WasmFXReturnDescriptor>(Builtin::kWasmFXReturn,
                                      Operator::kNoProperties, result_buffer);
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
  V<WasmFunctionData> function_data = V<WasmFunctionData>::Cast(
      __ LoadTrustedPointer(shared, LoadOp::Kind::TaggedBase(),
                            kWasmCapiFunctionDataIndirectPointerTag,
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

  DCHECK_LT(sig_->return_count(), wasm::kV8MaxWasmFunctionReturns);
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

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildCWasmEntryWrapper() {
  __ Bind(__ NewBlock());

  V<Word32> code_entry = __ Parameter(wasm::CWasmEntryParameters::kCodeEntry,
                                      RegisterRepresentation::Word32());
  V<Object> object_ref = __ Parameter(wasm::CWasmEntryParameters::kObjectRef,
                                      RegisterRepresentation::Tagged());
  V<WordPtr> arg_buffer =
      __ Parameter(wasm::CWasmEntryParameters::kArgumentsBuffer,
                   RegisterRepresentation::WordPtr());
  V<WordPtr> c_entry_fp = __ Parameter(wasm::CWasmEntryParameters::kCEntryFp,
                                       RegisterRepresentation::WordPtr());

  V<WordPtr> fp_value = __ FramePointer();
  __ Store(fp_value, c_entry_fp, StoreOp::Kind::RawAligned(),
           MemoryRepresentation::UintPtr(), compiler::kNoWriteBarrier,
           TypedFrameConstants::kFirstPushedFrameValueOffset);

  size_t wasm_arg_count = sig_->parameter_count();
  base::SmallVector<OpIndex, 16> args(wasm_arg_count + 1);  // +1 for instance.

  size_t pos = 0;
  V<WasmTrustedInstanceData> instance_data = V<WasmTrustedInstanceData>::Cast(
      __ LoadTrustedPointer(V<HeapObject>::Cast(object_ref),
                            LoadOp::Kind::TaggedBase().Immutable(),
                            kWasmTrustedInstanceDataIndirectPointerTag,
                            WasmInstanceObject::kTrustedDataOffset));
  args[pos++] = instance_data;

  int offset = 0;
  for (CanonicalValueType type : sig_->parameters()) {
    args[pos++] = SafeLoad(arg_buffer, offset, type);
    offset += type.value_kind_size();
  }

  const TSCallDescriptor* descriptor = TSCallDescriptor::Create(
      compiler::GetWasmCallDescriptor(
          __ graph_zone(), sig_, compiler::WasmCallKind::kWasmIndirectFunction,
          false),
      compiler::CanThrow::kYes, compiler::LazyDeoptOnThrow::kNo,
      __ graph_zone());

  Block* catch_block = __ NewBlock();
  OpIndex call;
  {
    typename Assembler::CatchScope scope(Asm(), catch_block);
    OpIndex frame_state = OpIndex::Invalid();
    call = __ Call(code_entry, frame_state, base::VectorOf(args), descriptor,
                   OpEffects().CanCallAnything());
  }

  // Handle success: store the return value(s).
  size_t return_count = sig_->return_count();
  if (return_count == 1) {
    SafeStore(0, sig_->GetReturn(), arg_buffer, call);
  } else if (return_count > 1) {
    offset = 0;
    for (size_t i = 0; i < return_count; ++i) {
      CanonicalValueType type = sig_->GetReturn(i);
      OpIndex val = __ Projection(call, i, this->RepresentationFor(type));
      SafeStore(offset, type, arg_buffer, val);
      offset += type.value_kind_size();
    }
  }
  __ Return(__ IntPtrConstant(0));

  __ Bind(catch_block);
  V<Object> exception = __ CatchBlockBegin();
  __ Return(exception);
}

template <typename Assembler>
void WasmWrapperTSGraphBuilder<Assembler>::BuildJSFastApiCallWrapper(
    DirectHandle<JSReceiver> callable) {
  __ Bind(__ NewBlock());

  V<WasmImportData> import_data =
      __ Parameter(0, RegisterRepresentation::Tagged());
  base::SmallVector<OpIndex, 16> wasm_params;
  for (size_t i = 0; i < sig_->parameter_count(); ++i) {
    wasm_params.push_back(__ Parameter(
        static_cast<int>(i + 1), this->RepresentationFor(sig_->GetParam(i))));
  }

  V<NativeContext> native_context = __ template LoadTaggedField<NativeContext>(
      import_data, WasmImportData::kNativeContextOffset);

  __ StoreOffHeap(__ LoadRootRegister(),
                  __ BitcastHeapObjectToWordPtr(native_context),
                  MemoryRepresentation::UintPtr(), Isolate::context_offset());

  Tagged<JSFunction> target;
  V<JSFunction> target_node;
  V<Object> receiver_node;
  V<JSReceiver> callable_node = __ template LoadTaggedField<JSReceiver>(
      import_data, WasmImportData::kCallableOffset);

  if (IsJSBoundFunction(*callable)) {
    target = Cast<JSFunction>(
        Cast<JSBoundFunction>(callable)->bound_target_function());
    target_node = __ template LoadTaggedField<JSFunction>(
        callable_node, JSBoundFunction::kBoundTargetFunctionOffset);
    receiver_node =
        __ LoadTaggedField(callable_node, JSBoundFunction::kBoundThisOffset);
  } else {
    target = Cast<JSFunction>(*callable);
    target_node = V<JSFunction>::Cast(callable_node);
    receiver_node = __ template LoadRoot<RootIndex::kUndefinedValue>();
  }

  Tagged<SharedFunctionInfo> shared = target->shared();
  Tagged<FunctionTemplateInfo> api_func_data = shared->api_func_data();
  // !!! Warning !!! This relies on the preceding logic to validate that this
  // overload matches the expected signature, which is generally unsafe in the
  // sandbox attacker model.
  CHECK(v8_flags.wasm_unsafe_fast_api_wrapper);
  const CFunctionWithSignature c_function = api_func_data->GetCFunction(0);

#ifdef V8_USE_SIMULATOR_WITH_GENERIC_C_CALLS
  Address c_functions[] = {c_function.address};
  const v8::CFunctionInfo* const c_signatures[] = {c_function.signature};
  Isolate::Current()->simulator_data()->RegisterFunctionsAndSignatures(
      c_functions, c_signatures, 1);
#endif  //  V8_USE_SIMULATOR_WITH_GENERIC_C_CALLS

  V<SharedFunctionInfo> shared_function_info =
      LoadSharedFunctionInfo(target_node);
  V<FunctionTemplateInfo> function_template_info =
      __ template LoadTaggedField<FunctionTemplateInfo>(
          shared_function_info,
          SharedFunctionInfo::kUntrustedFunctionDataOffset);

  V<Object> api_data_argument = __ LoadTaggedField(
      function_template_info, offsetof(FunctionTemplateInfo, callback_data_));

  compiler::FastApiCallFunction call_function{c_function.address,
                                              c_function.signature};

  auto [old_sp, old_limit] = this->BuildSwitchToTheCentralStackIfNeeded();

  base::SmallVector<OpIndex, 16> args;
  for (unsigned i = 0; i < c_function.signature->ArgumentCount(); ++i) {
    if (i == 0) {
      args.push_back(receiver_node);
    } else {
      args.push_back(wasm_params[i - 1]);
    }
  }

  const compiler::turboshaft::FastApiCallParameters* params =
      compiler::turboshaft::FastApiCallParameters::Create(call_function,
                                                          __ graph_zone());

  // It doesn't really matter what we put into {out_reps}, because we have the
  // FastApiCallLoweringReducer in the assembler stack, and it lowers the node
  // on the fly while ignoring the {out_reps} parameter.
  base::SmallVector<RegisterRepresentation, 2> out_reps;
  out_reps.push_back(RegisterRepresentation::Word32());
  CTypeInfo return_type = c_function.signature->ReturnInfo();
  out_reps.push_back(RegisterRepresentation::FromCTypeInfo(
      return_type, params->c_signature()->GetInt64Representation()));

  V<Tuple<Word32, Any>> fast_call_result =
      __ FastApiCall(OpIndex::Invalid(), api_data_argument, native_context,
                     base::VectorOf(args), params, base::VectorOf(out_reps));

  // Projection<0> indicates parameter conversion errors. For Wasm, we only
  // choose to use Fast API Calls when parameter conversion cannot fail, so
  // we don't need to check for that here.
  if (v8_flags.debug_code) {
    V<Word32> success = __ template Projection<0>(fast_call_result);
    uint32_t kFailure = compiler::turboshaft::FastApiCallOp::kFailureValue;
    IF (UNLIKELY(__ Word32Equal(success, kFailure))) {
      __ Unreachable();
    }
  }

  OpIndex value = __ template Projection<1>(fast_call_result, out_reps.back());

  this->BuildSwitchBackFromCentralStack(old_sp, old_limit);
  __ Return(value);
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_WRAPPERS_INL_H_
