// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"

#include <memory>
#include <optional>

#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/codegen/assembler.h"
#include "src/codegen/compiler.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/diamond.h"
#include "src/compiler/fast-api-calls.h"
#include "src/compiler/graph-assembler.h"
#include "src/compiler/int64-lowering.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/turbofan-graph-visualizer.h"
#include "src/compiler/turbofan-graph.h"
#include "src/compiler/turboshaft/wasm-turboshaft-compiler.h"
#include "src/compiler/wasm-call-descriptors.h"
#include "src/compiler/wasm-compiler-definitions.h"
#include "src/compiler/wasm-graph-assembler.h"
#include "src/compiler/wasm-inlining-into-js.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/execution/simulator-base.h"
#include "src/heap/factory.h"
#include "src/logging/counters.h"
#include "src/objects/code-kind.h"
#include "src/objects/heap-number.h"
#include "src/objects/instance-type.h"
#include "src/objects/name.h"
#include "src/objects/string.h"
#include "src/roots/roots.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/wasm/wasm-tracing.h"

namespace v8::internal::compiler {

namespace {

bool ContainsInt64(const wasm::CanonicalSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmI64) return true;
  }
  return false;
}

}  // namespace

bool TryWasmInlining(int fct_index, wasm::NativeModule* native_module,
                     int inlining_id, MachineGraph* mcgraph,
                     SourcePositionTable* spt) {
#define TRACE(x)                         \
  do {                                   \
    if (v8_flags.trace_turbo_inlining) { \
      StdoutStream() << x << "\n";       \
    }                                    \
  } while (false)

  DCHECK(native_module->HasWireBytes());
  const wasm::WasmModule* module = native_module->module();
  const wasm::WasmFunction& inlinee = module->functions[fct_index];
  // TODO(mliedtke): What would be a proper maximum size?
  const uint32_t kMaxWasmInlineeSize = 30;
  if (inlinee.code.length() > kMaxWasmInlineeSize) {
    TRACE("- not inlining: function body is larger than max inlinee size ("
          << inlinee.code.length() << " > " << kMaxWasmInlineeSize << ")");
    return false;
  }
  if (inlinee.imported) {
    TRACE("- not inlining: function is imported");
    return false;
  }
  if (native_module->compilation_state()->detected_features().has_shared()) {
    // The Turbofan Wasm code has not been updated to support the experimental
    // shared-everything feature. It is unlikely that it will become
    // non-experimental any time soon while we plan to remove the Turbofan code
    // soon.
    TRACE("- not inlining: module uses experimental 'shared' feature");
    return false;
  }
  base::Vector<const uint8_t> bytes(native_module->wire_bytes().SubVector(
      inlinee.code.offset(), inlinee.code.end_offset()));
  const wasm::FunctionBody inlinee_body(inlinee.sig, inlinee.code.offset(),
                                        bytes.begin(), bytes.end());
  bool result =
      WasmIntoJSInliner::TryInlining(mcgraph->graph()->zone(), module, mcgraph,
                                     inlinee_body, bytes, spt, inlining_id);
  TRACE((
      result
          ? "- inlining Wasm function"
          : "- not inlining: function body contains unsupported instructions"));
  return result;
#undef TRACE
}

template <typename T>
Signature<MachineRepresentation>* CreateMachineSignature(
    Zone* zone, const Signature<T>* sig, wasm::CallOrigin origin) {
  Signature<MachineRepresentation>::Builder builder(zone, sig->return_count(),
                                                    sig->parameter_count());
  for (auto ret : sig->returns()) {
    if (origin == wasm::kCalledFromJS) {
      builder.AddReturn(MachineRepresentation::kTagged);
    } else {
      builder.AddReturn(ret.machine_representation());
    }
  }

  for (auto param : sig->parameters()) {
    if (origin == wasm::kCalledFromJS) {
      // Parameters coming from JavaScript are always tagged values. Especially
      // when the signature says that it's an I64 value, then a BigInt object is
      // provided by JavaScript, and not two 32-bit parameters.
      builder.AddParam(MachineRepresentation::kTagged);
    } else {
      builder.AddParam(param.machine_representation());
    }
  }
  return builder.Get();
}

template Signature<MachineRepresentation>* CreateMachineSignature(
    Zone*, const Signature<wasm::ValueType>*, wasm::CallOrigin);
template Signature<MachineRepresentation>* CreateMachineSignature(
    Zone*, const Signature<wasm::CanonicalValueType>*, wasm::CallOrigin);

namespace {

class WasmWrapperGraphBuilder {
 public:
  WasmWrapperGraphBuilder(Zone* zone, MachineGraph* mcgraph,
                          const wasm::CanonicalSig* wrapper_sig,
                          Isolate* isolate)
      : gasm_(mcgraph, zone),
        zone_(zone),
        mcgraph_(mcgraph),
        wrapper_sig_(wrapper_sig),
        isolate_(isolate) {
    DCHECK_NOT_NULL(mcgraph_);
    DCHECK_NOT_NULL(isolate_);
  }

  void Start(unsigned params) {
    Node* start = graph()->NewNode(mcgraph_->common()->Start(params));
    graph()->SetStart(start);
    SetEffectControl(start);
    // Initialize parameter nodes.
    parameters_ = zone_->AllocateArray<Node*>(params);
    for (unsigned i = 0; i < params; i++) {
      parameters_[i] = nullptr;
    }
    // Initialize closure check.
    Node* param = Param(Linkage::kJSCallClosureParamIndex, "%closure");
    if (v8_flags.debug_code) {
      Assert(gasm_.HasInstanceTypeInRange(param, FIRST_JS_FUNCTION_TYPE,
                                          LAST_JS_FUNCTION_TYPE),
             AbortReason::kUnexpectedInstanceType);
    }
    graph()->SetEnd(graph()->NewNode(mcgraph_->common()->End(0)));
  }

  Node* Param(int index, const char* debug_name = nullptr) {
    DCHECK_NOT_NULL(graph()->start());
    // Turbofan allows negative parameter indices.
    DCHECK_GE(index, kMinParameterIndex);
    int array_index = index - kMinParameterIndex;
    if (parameters_[array_index] == nullptr) {
      parameters_[array_index] = graph()->NewNode(
          mcgraph_->common()->Parameter(index, debug_name), graph()->start());
    }
    return parameters_[array_index];
  }

  void TerminateThrow(Node* effect, Node* control) {
    Node* terminate =
        graph()->NewNode(mcgraph_->common()->Throw(), effect, control);
    gasm_.MergeControlToEnd(terminate);
    gasm_.InitializeEffectControl(nullptr, nullptr);
  }

  Node* NoContextConstant() { return mcgraph_->IntPtrConstant(0); }

  Node* BuildLoadIsolateRoot() {
    return mcgraph_->IntPtrConstant(isolate_->isolate_root());
  }

  Node* Int32Constant(int32_t value) { return mcgraph_->Int32Constant(value); }

  Node* UndefinedValue() {
    return graph()->NewNode(mcgraph_->common()->HeapConstant(
        isolate_->factory()->undefined_value()));
  }

  Node* Return(Node* val) {
    Node* buf[] = {Int32Constant(0), val, effect(), control()};
    Node* ret = graph()->NewNode(mcgraph_->common()->Return(1), 4, buf);
    gasm_.MergeControlToEnd(ret);
    return ret;
  }

  Node* effect() { return gasm_.effect(); }
  Node* control() { return gasm_.control(); }

  Node* SetEffect(Node* node) {
    SetEffectControl(node, control());
    return node;
  }

  Node* SetControl(Node* node) {
    SetEffectControl(effect(), node);
    return node;
  }

  void SetEffectControl(Node* effect, Node* control) {
    gasm_.InitializeEffectControl(effect, control);
  }

  Node* SetEffectControl(Node* effect_and_control) {
    SetEffectControl(effect_and_control, effect_and_control);
    return effect_and_control;
  }

  Node* BuildCallNode(size_t param_count, base::Vector<Node*> args,
                      Node* implicit_first_arg, const Operator* op,
                      Node* frame_state) {
    const size_t has_frame_state = frame_state != nullptr ? 1 : 0;
    const size_t extra = 3;  // instance_node, effect, and control.
    const size_t count = 1 + param_count + extra + has_frame_state;

    // Reallocate the buffer to make space for extra inputs.
    base::SmallVector<Node*, 16 + extra> inputs(count);
    DCHECK_EQ(1 + param_count, args.size());

    // Make room for the first argument at index 1, just after code.
    inputs[0] = args[0];  // code
    inputs[1] = implicit_first_arg;
    if (param_count > 0) {
      memcpy(&inputs[2], &args[1], param_count * sizeof(Node*));
    }

    // Add effect and control inputs.
    if (has_frame_state != 0) inputs[param_count + 2] = frame_state;
    inputs[param_count + has_frame_state + 2] = effect();
    inputs[param_count + has_frame_state + 3] = control();

    Node* call = graph()->NewNode(op, static_cast<int>(count), inputs.begin());
    // Return calls have no effect output. Other calls are the new effect node.
    if (op->EffectOutputCount() > 0) SetEffect(call);

    return call;
  }

  void BuildWasmCall(base::Vector<Node*> args, base::Vector<Node*> rets,
                     Node* implicit_first_arg, Node* frame_state) {
    CallDescriptor* call_descriptor =
        GetWasmCallDescriptor(mcgraph_->zone(), wrapper_sig_,
                              kWasmIndirectFunction, frame_state != nullptr);
    const Operator* op = mcgraph_->common()->Call(call_descriptor);
    Node* call = BuildCallNode(wrapper_sig_->parameter_count(), args,
                               implicit_first_arg, op, frame_state);
    // TODO(manoskouk): These assume the call has control and effect outputs.
    DCHECK_GT(op->ControlOutputCount(), 0);
    DCHECK_GT(op->EffectOutputCount(), 0);
    SetEffectControl(call);

    size_t ret_count = wrapper_sig_->return_count();
    if (ret_count == 0) return;  // No return value.

    DCHECK_EQ(ret_count, rets.size());
    if (ret_count == 1) {
      // Only a single return value.
      rets[0] = call;
    } else {
      // Create projections for all return values.
      for (size_t i = 0; i < ret_count; i++) {
        rets[i] = graph()->NewNode(mcgraph_->common()->Projection(i), call,
                                   graph()->start());
      }
    }
  }

  // Only call this function for code which is not reused across instantiations,
  // as we do not patch the embedded js_context.
  Node* BuildCallToRuntimeWithContext(Runtime::FunctionId f, Node* js_context,
                                      Node** parameters, int parameter_count) {
    const Runtime::Function* fun = Runtime::FunctionForId(f);
    auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
        mcgraph_->zone(), f, fun->nargs, Operator::kNoProperties,
        CallDescriptor::kNoFlags);
    // The CEntryStub is loaded from the IsolateRoot so that generated code is
    // Isolate independent. At the moment this is only done for CEntryStub(1).
    Node* isolate_root = BuildLoadIsolateRoot();
    DCHECK_EQ(1, fun->result_size);
    auto centry_id = Builtin::kWasmCEntry;
    int builtin_slot_offset = IsolateData::BuiltinSlotOffset(centry_id);
    Node* centry_stub =
        gasm_.Load(MachineType::Pointer(), isolate_root, builtin_slot_offset);
    // TODO(titzer): allow arbitrary number of runtime arguments
    // At the moment we only allow 5 parameters. If more parameters are needed,
    // increase this constant accordingly.
    static const int kMaxParams = 5;
    DCHECK_GE(kMaxParams, parameter_count);
    Node* inputs[kMaxParams + 6];
    int count = 0;
    inputs[count++] = centry_stub;
    for (int i = 0; i < parameter_count; i++) {
      inputs[count++] = parameters[i];
    }
    inputs[count++] =
        mcgraph_->ExternalConstant(ExternalReference::Create(f));  // ref
    inputs[count++] = Int32Constant(fun->nargs);                   // arity
    inputs[count++] = js_context;                                  // js_context
    inputs[count++] = effect();
    inputs[count++] = control();

    return gasm_.Call(call_descriptor, count, inputs);
  }

  TFGraph* graph() { return mcgraph_->graph(); }

  void LowerInt64() {
    if (mcgraph_->machine()->Is64()) return;
    Int64Lowering r(mcgraph_->graph(), mcgraph_->machine(), mcgraph_->common(),
                    gasm_.simplified(), mcgraph_->zone(),
                    CreateMachineSignature(mcgraph_->zone(), wrapper_sig_,
                                           wasm::kCalledFromJS));
    r.LowerGraph();
  }

  void Assert(Node* condition, AbortReason abort_reason) {
    if (!v8_flags.debug_code) return;

    Diamond check(graph(), mcgraph_->common(), condition, BranchHint::kTrue);
    check.Chain(control());
    SetControl(check.if_false);
    Node* message_id = gasm_.NumberConstant(static_cast<int32_t>(abort_reason));
    Node* old_effect = effect();
    Node* call = BuildCallToRuntimeWithContext(
        Runtime::kAbort, NoContextConstant(), &message_id, 1);
    check.merge->ReplaceInput(1, call);
    SetEffectControl(check.EffectPhi(old_effect, effect()), check.merge);
  }

  Node* BuildCallAndReturn(Node* js_context, Node* function_data,
                           base::SmallVector<Node*, 16> args,
                           Node* frame_state) {
    const int rets_count = static_cast<int>(wrapper_sig_->return_count());
    base::SmallVector<Node*, 1> rets(rets_count);

    // Call to an import or a wasm function defined in this module.
    // The (cached) call target is the jump table slot for that function.
    // We do not use the imports dispatch table here so that the wrapper is
    // target independent, in particular for tier-up.
    Node* internal = gasm_.LoadImmutableProtectedPointerFromObject(
        function_data,
        offsetof(WasmFunctionData, protected_internal_) - kHeapObjectTag);
    args[0] = gasm_.LoadFromObject(
        MachineType::Uint32(), internal,
        offsetof(WasmInternalFunction, raw_call_target_) - kHeapObjectTag);
    Node* implicit_arg = gasm_.LoadImmutableProtectedPointerFromObject(
        internal, offsetof(WasmInternalFunction, protected_implicit_arg_) -
                      kHeapObjectTag);
    BuildWasmCall(base::VectorOf(args), base::VectorOf(rets), implicit_arg,
                  frame_state);

    Node* jsval;
    if (wrapper_sig_->return_count() == 0) {
      jsval = UndefinedValue();
    } else if (wrapper_sig_->return_count() == 1) {
      jsval = rets[0];
    } else {
      UNREACHABLE();
    }
    return jsval;
  }

  void BuildJSToWasmWrapper(Node* frame_state) {
    const int wasm_param_count =
        static_cast<int>(wrapper_sig_->parameter_count());

    // Build the start and the JS parameter nodes.
    // TODO(saelo): this should probably be a constant with a descriptive name.
    // As far as I understand, it's the number of additional parameters in the
    // JS calling convention. Also there should be a static_assert here that it
    // matches the number of parameters in the JSTrampolineDescriptor?
    // static_assert
    Start(wasm_param_count + 6);

    // Create the js_closure and js_context parameters.
    Node* js_closure = Param(Linkage::kJSCallClosureParamIndex, "%closure");
    Node* js_context = Param(
        Linkage::GetJSCallContextParamIndex(wasm_param_count + 1), "%context");
    Node* function_data = gasm_.LoadFunctionDataFromJSFunction(js_closure);

    if (!wasm::IsJSCompatibleSignature(wrapper_sig_)) {
      // Throw a TypeError. Use the js_context of the calling javascript
      // function (passed as a parameter), such that the generated code is
      // js_context independent.
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError, js_context,
                                    nullptr, 0);
      TerminateThrow(effect(), control());
      return;
    }

#if V8_ENABLE_DRUMBRAKE
    if (v8_flags.wasm_enable_exec_time_histograms && v8_flags.slow_histograms &&
        !v8_flags.wasm_jitless) {
      Node* runtime_call = BuildCallToRuntimeWithContext(
          Runtime::kWasmTraceBeginExecution, js_context, nullptr, 0);
      SetControl(runtime_call);
    }
#endif  // V8_ENABLE_DRUMBRAKE

    const int args_count = wasm_param_count + 1;  // +1 for wasm_code.

    // Prepare Param() nodes. Param() nodes can only be created once,
    // so we need to use the same nodes along all possible transformation paths.
    base::SmallVector<Node*, 16> params(args_count);
    for (int i = 0; i < wasm_param_count; ++i) {
      params[i] = Param(i + 1);  // Skip the receiver.
    }

    // Convert JS parameters to wasm numbers using the default transformation
    // and build the call.
    base::SmallVector<Node*, 16> args(args_count);
    for (int i = 0; i < wasm_param_count; ++i) {
      Node* wasm_param = params[i];

      // For Float32 parameters
      // we set UseInfo::CheckedNumberOrOddballAsFloat64 in
      // simplified-lowering and we need to add here a conversion from Float64
      // to Float32.
      if (wrapper_sig_->GetParam(i).kind() == wasm::kF32) {
        wasm_param = gasm_.TruncateFloat64ToFloat32(wasm_param);
      }

      args[i + 1] = wasm_param;
    }

    Node* jsval =
        BuildCallAndReturn(js_context, function_data, args, frame_state);

#if V8_ENABLE_DRUMBRAKE
    if (v8_flags.wasm_enable_exec_time_histograms && v8_flags.slow_histograms &&
        !v8_flags.wasm_jitless) {
      Node* runtime_call = BuildCallToRuntimeWithContext(
          Runtime::kWasmTraceEndExecution, js_context, nullptr, 0);
      SetControl(runtime_call);
    }
#endif  // V8_ENABLE_DRUMBRAKE

    Return(jsval);
    if (ContainsInt64(wrapper_sig_)) LowerInt64();
  }

 private:
  WasmGraphAssembler gasm_;
  Zone* const zone_;
  MachineGraph* const mcgraph_;
  Node** parameters_ = nullptr;
  const wasm::CanonicalSig* const wrapper_sig_;
  Isolate* const isolate_;
};

}  // namespace

void BuildInlinedJSToWasmWrapper(Zone* zone, MachineGraph* mcgraph,
                                 const wasm::CanonicalSig* signature,
                                 Isolate* isolate, Node* frame_state) {
  WasmWrapperGraphBuilder builder(zone, mcgraph, signature, isolate);
  builder.BuildJSToWasmWrapper(frame_state);
}

std::unique_ptr<OptimizedCompilationJob> NewJSToWasmCompilationJob(
    Isolate* isolate, const wasm::CanonicalSig* sig) {
  wasm::WrapperCompilationInfo info{.code_kind = CodeKind::JS_TO_WASM_FUNCTION};
  return Pipeline::NewWasmTurboshaftWrapperCompilationJob(
      isolate, sig, info, WasmExportedFunction::GetDebugName(sig),
      WasmAssemblerOptions());
}

wasm::WasmCompilationResult CompileWasmImportCallWrapper(
    wasm::ImportCallKind kind, const wasm::CanonicalSig* sig,
    int expected_arity, wasm::Suspend suspend) {
  DCHECK_NE(wasm::ImportCallKind::kLinkError, kind);
  DCHECK_NE(wasm::ImportCallKind::kWasmToWasm, kind);
  DCHECK_NE(wasm::ImportCallKind::kReexportedJSFunction, kind);
  DCHECK_NE(wasm::ImportCallKind::kWasmToJSFastApi, kind);

  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
              "wasm.CompileWasmImportCallWrapper");
  base::TimeTicks start_time;
  if (V8_UNLIKELY(v8_flags.trace_wasm_compilation_times)) {
    start_time = base::TimeTicks::Now();
  }

  // Build a name in the form "wasm-to-js-<kind>-<signature>".
  constexpr size_t kMaxNameLen = 128;
  char func_name[kMaxNameLen];
  int name_prefix_len = SNPrintF(base::VectorOf(func_name, kMaxNameLen),
                                 "wasm-to-js-%d-", static_cast<int>(kind));
  PrintSignature(base::VectorOf(func_name, kMaxNameLen) + name_prefix_len, sig,
                 '-');

  auto result = Pipeline::GenerateCodeForWasmNativeStubFromTurboshaft(
      sig,
      wasm::WrapperCompilationInfo{CodeKind::WASM_TO_JS_FUNCTION, kind,
                                   expected_arity, suspend},
      func_name, WasmStubAssemblerOptions());

  if (V8_UNLIKELY(v8_flags.trace_wasm_compilation_times)) {
    base::TimeDelta time = base::TimeTicks::Now() - start_time;
    int codesize = result.code_desc.body_size();
    StdoutStream{} << "Compiled WasmToJS wrapper " << func_name << ", took "
                   << time.InMicroseconds() << " μs; codesize " << codesize
                   << std::endl;
  }

  return result;
}

wasm::WasmCompilationResult CompileWasmStackEntryWrapper(
    const wasm::CanonicalSig* sig) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
              "wasm.CompileWasmStackEntryWrapper");
  base::TimeTicks start_time;
  if (V8_UNLIKELY(v8_flags.trace_wasm_compilation_times)) {
    start_time = base::TimeTicks::Now();
  }

  // Build a name in the form "wasm-continuation-<signature>".
  constexpr size_t kMaxNameLen = 128;
  char func_name[kMaxNameLen];
  int name_prefix_len =
      SNPrintF(base::ArrayVector(func_name), "wasm-continuation-");
  PrintSignature(base::ArrayVector(func_name) + name_prefix_len, sig, '-');
  wasm::WasmCompilationResult result =
      Pipeline::GenerateCodeForWasmNativeStubFromTurboshaft(
          sig, wasm::WrapperCompilationInfo{CodeKind::WASM_STACK_ENTRY},
          func_name, WasmStubAssemblerOptions());

  if (V8_UNLIKELY(v8_flags.trace_wasm_compilation_times)) {
    base::TimeDelta time = base::TimeTicks::Now() - start_time;
    int codesize = result.code_desc.body_size();
    StdoutStream{} << "Compiled WasmContinuation wrapper " << func_name
                   << ", took " << time.InMicroseconds() << " μs; codesize "
                   << codesize << std::endl;
  }

  return result;
}

wasm::WasmCompilationResult CompileWasmCapiCallWrapper(
    const wasm::CanonicalSig* sig) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
              "wasm.CompileWasmCapiFunction");

  return Pipeline::GenerateCodeForWasmNativeStubFromTurboshaft(
      sig, wasm::WrapperCompilationInfo{CodeKind::WASM_TO_CAPI_FUNCTION},
      "WasmCapiCall", WasmStubAssemblerOptions());
}

wasm::WasmCompilationResult CompileWasmJSFastCallWrapper(
    const wasm::CanonicalSig* sig, DirectHandle<JSReceiver> callable) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
              "wasm.CompileWasmJSFastCallWrapper");

  return Pipeline::GenerateCodeForWasmNativeStubFromTurboshaft(
      sig,
      wasm::WrapperCompilationInfo{CodeKind::WASM_TO_JS_FUNCTION,
                                   wasm::ImportCallKind::kWasmToJSFastApi, 0,
                                   wasm::kNoSuspend},
      "WasmJSFastApiCall", WasmStubAssemblerOptions(), callable);
}

Handle<Code> CompileCWasmEntry(Isolate* isolate,
                               const wasm::CanonicalSig* sig) {
  DCHECK(!v8_flags.wasm_jitless);

  // Build a name in the form "c-wasm-entry:<params>:<returns>".
  constexpr size_t kMaxNameLen = 128;
  constexpr size_t kNamePrefixLen = 13;
  auto name_buffer = std::unique_ptr<char[]>(new char[kMaxNameLen]);
  memcpy(name_buffer.get(), "c-wasm-entry:", kNamePrefixLen);
  PrintSignature(
      base::VectorOf(name_buffer.get(), kMaxNameLen) + kNamePrefixLen, sig);

  // Run the compilation job synchronously.
  std::unique_ptr<turboshaft::TurboshaftCompilationJob> job(
      Pipeline::NewWasmTurboshaftWrapperCompilationJob(
          isolate, sig, wasm::WrapperCompilationInfo{CodeKind::C_WASM_ENTRY},
          std::move(name_buffer), AssemblerOptions::Default(isolate)));

  CHECK_NE(job->ExecuteJob(isolate->counters()->runtime_call_stats(), nullptr),
           CompilationJob::FAILED);
  CHECK_NE(job->FinalizeJob(isolate), CompilationJob::FAILED);

  return job->compilation_info()->code();
}

AssemblerOptions WasmAssemblerOptions() {
  return AssemblerOptions{
      // Relocation info required to serialize {WasmCode} for proper functions.
      .record_reloc_info_for_serialization = true,
      .enable_root_relative_access = false,
      .is_wasm = true,
  };
}

AssemblerOptions WasmStubAssemblerOptions() {
  return AssemblerOptions{
      // Relocation info not necessary because stubs are not serialized.
      .record_reloc_info_for_serialization = false,
      .enable_root_relative_access = false,
      // TODO(jkummerow): Would it be better to have a far jump table in
      // the wrapper cache's code space, and call builtins through that?
      .builtin_call_jump_mode = BuiltinCallJumpMode::kIndirect,
      .is_wasm = true,
  };
}

}  // namespace v8::internal::compiler
