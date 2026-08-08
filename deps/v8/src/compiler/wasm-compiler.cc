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

// Use MachineType::Pointer() over Tagged() to load root pointers because they
// do not get compressed.
#define LOAD_ROOT(RootName, factory_name)                         \
  (isolate_ ? graph()->NewNode(mcgraph()->common()->HeapConstant( \
                  isolate_->factory()->factory_name()))           \
            : gasm_->LoadImmutable(                               \
                  MachineType::Pointer(), BuildLoadIsolateRoot(), \
                  IsolateData::root_slot_offset(RootIndex::k##RootName)))

template <typename T>
bool ContainsSimd(const Signature<T>* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmS128) return true;
  }
  return false;
}

bool ContainsInt64(const wasm::CanonicalSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmI64) return true;
  }
  return false;
}

}  // namespace

WasmGraphBuilder::WasmGraphBuilder(
    wasm::CompilationEnv* env, Zone* zone, MachineGraph* mcgraph,
    const wasm::FunctionSig* sig,
    compiler::SourcePositionTable* source_position_table,
    ParameterMode parameter_mode, Isolate* isolate,
    wasm::WasmEnabledFeatures enabled_features,
    const wasm::CanonicalSig* wrapper_sig)
    : gasm_(std::make_unique<WasmGraphAssembler>(mcgraph, zone)),
      zone_(zone),
      mcgraph_(mcgraph),
      env_(env),
      enabled_features_(enabled_features),
      has_simd_(sig ? ContainsSimd(sig) : ContainsSimd(wrapper_sig)),
      function_sig_(sig),
      wrapper_sig_(wrapper_sig),
      source_position_table_(source_position_table),
      parameter_mode_(parameter_mode),
      isolate_(isolate),
      null_check_strategy_(trap_handler::IsTrapHandlerEnabled() &&
                                   V8_STATIC_ROOTS_BOOL
                               ? NullCheckStrategy::kTrapHandler
                               : NullCheckStrategy::kExplicit) {
  // This code is only used for inlining js-to-wasm wrappers into
  // Turbofan-compiled JS functions.
  CHECK(parameter_mode != ParameterMode::kInstanceParameterMode);

  // There are two kinds of isolate-specific code: JS-to-JS wrappers (passing
  // kNoSpecialParameterMode) and JS-to-Wasm wrappers (passing
  // kJSFunctionAbiMode).
  DCHECK_IMPLIES(isolate != nullptr,
                 parameter_mode_ == kJSFunctionAbiMode ||
                     parameter_mode_ == kNoSpecialParameterMode);
  DCHECK_IMPLIES(env && env->module &&
                     std::any_of(env->module->memories.begin(),
                                 env->module->memories.end(),
                                 [](auto& memory) {
                                   return memory.bounds_checks ==
                                          wasm::kTrapHandler;
                                 }),
                 trap_handler::IsTrapHandlerEnabled());
  DCHECK_NOT_NULL(mcgraph_);
}

// Destructor define here where the definition of {WasmGraphAssembler} is
// available.
WasmGraphBuilder::~WasmGraphBuilder() = default;

bool WasmGraphBuilder::TryWasmInlining(int fct_index,
                                       wasm::NativeModule* native_module,
                                       int inlining_id) {
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
  base::Vector<const uint8_t> bytes(native_module->wire_bytes().SubVector(
      inlinee.code.offset(), inlinee.code.end_offset()));
  SharedFlag is_shared = module->type(inlinee.sig_index).is_shared;
  const wasm::FunctionBody inlinee_body(inlinee.sig, inlinee.code.offset(),
                                        bytes.begin(), bytes.end(), is_shared);
  // If the inlinee was not validated before, do that now.
  if (V8_UNLIKELY(!module->function_was_validated(fct_index))) {
    wasm::WasmDetectedFeatures unused_detected_features;
    if (ValidateFunctionBody(graph()->zone(), enabled_features_, module,
                             &unused_detected_features, inlinee_body)
            .failed()) {
      // At this point we cannot easily raise a compilation error any more.
      // Since this situation is highly unlikely though, we just ignore this
      // inlinee and move on. The same validation error will be triggered
      // again when actually compiling the invalid function.
      TRACE("- not inlining: function body is invalid");
      return false;
    }
    module->set_function_validated(fct_index);
  }
  bool result = WasmIntoJSInliner::TryInlining(
      graph()->zone(), module, mcgraph_, inlinee_body, bytes,
      source_position_table_, inlining_id);
  TRACE((
      result
          ? "- inlining Wasm function"
          : "- not inlining: function body contains unsupported instructions"));
  return result;
#undef TRACE
}

void WasmGraphBuilder::Start(unsigned params) {
  Node* start = graph()->NewNode(mcgraph()->common()->Start(params));
  graph()->SetStart(start);
  SetEffectControl(start);
  // Initialize parameter nodes.
  parameters_ = zone_->AllocateArray<Node*>(params);
  for (unsigned i = 0; i < params; i++) {
    parameters_[i] = nullptr;
  }
  // Initialize instance node.
  switch (parameter_mode_) {
    case kInstanceParameterMode: {
      Node* param = Param(wasm::kWasmInstanceDataParameterIndex);
      if (v8_flags.debug_code) {
        Assert(gasm_->HasInstanceType(param, WASM_TRUSTED_INSTANCE_DATA_TYPE),
               AbortReason::kUnexpectedInstanceType);
      }
      instance_data_node_ = param;
      break;
    }
    case kWasmImportDataMode: {
      Node* param = Param(0);
      if (v8_flags.debug_code) {
        Assert(gasm_->HasInstanceType(param, WASM_IMPORT_DATA_TYPE),
               AbortReason::kUnexpectedInstanceType);
      }
      instance_data_node_ = gasm_->LoadProtectedPointerFromObject(
          param, WasmImportData::kProtectedInstanceDataOffset - kHeapObjectTag);
      break;
    }
    case kJSFunctionAbiMode: {
      Node* param = Param(Linkage::kJSCallClosureParamIndex, "%closure");
      if (v8_flags.debug_code) {
        Assert(gasm_->HasInstanceTypeInRange(param, FIRST_JS_FUNCTION_TYPE,
                                             LAST_JS_FUNCTION_TYPE),
               AbortReason::kUnexpectedInstanceType);
      }
      instance_data_node_ = gasm_->LoadExportedFunctionInstanceData(
          gasm_->LoadFunctionDataFromJSFunction(param));
      break;
    }
    case kNoSpecialParameterMode:
      break;
  }
  graph()->SetEnd(graph()->NewNode(mcgraph()->common()->End(0)));
}

Node* WasmGraphBuilder::Param(int index, const char* debug_name) {
  DCHECK_NOT_NULL(graph()->start());
  // Turbofan allows negative parameter indices.
  DCHECK_GE(index, kMinParameterIndex);
  int array_index = index - kMinParameterIndex;
  if (parameters_[array_index] == nullptr) {
    parameters_[array_index] = graph()->NewNode(
        mcgraph()->common()->Parameter(index, debug_name), graph()->start());
  }
  return parameters_[array_index];
}

void WasmGraphBuilder::TerminateThrow(Node* effect, Node* control) {
  Node* terminate =
      graph()->NewNode(mcgraph()->common()->Throw(), effect, control);
  gasm_->MergeControlToEnd(terminate);
  gasm_->InitializeEffectControl(nullptr, nullptr);
}

Node* WasmGraphBuilder::NoContextConstant() {
  return mcgraph()->IntPtrConstant(0);
}

Node* WasmGraphBuilder::BuildLoadIsolateRoot() {
  return isolate_ ? mcgraph()->IntPtrConstant(isolate_->isolate_root())
                  : gasm_->LoadRootRegister();
}

Node* WasmGraphBuilder::Int32Constant(int32_t value) {
  return mcgraph()->Int32Constant(value);
}

Node* WasmGraphBuilder::UndefinedValue() {
  return LOAD_ROOT(UndefinedValue, undefined_value);
}

Node* WasmGraphBuilder::Return(base::Vector<Node*> vals) {
  unsigned count = static_cast<unsigned>(vals.size());
  base::SmallVector<Node*, 8> buf(count + 3);

  // TODOC: What is the meaning of the 0-constant?
  buf[0] = Int32Constant(0);
  if (count > 0) {
    memcpy(buf.data() + 1, vals.begin(), sizeof(void*) * count);
  }
  buf[count + 1] = effect();
  buf[count + 2] = control();
  Node* ret = graph()->NewNode(mcgraph()->common()->Return(count), count + 3,
                               buf.data());

  gasm_->MergeControlToEnd(ret);
  return ret;
}

Node* WasmGraphBuilder::effect() { return gasm_->effect(); }

Node* WasmGraphBuilder::control() { return gasm_->control(); }

Node* WasmGraphBuilder::SetEffect(Node* node) {
  SetEffectControl(node, control());
  return node;
}

Node* WasmGraphBuilder::SetControl(Node* node) {
  SetEffectControl(effect(), node);
  return node;
}

void WasmGraphBuilder::SetEffectControl(Node* effect, Node* control) {
  gasm_->InitializeEffectControl(effect, control);
}

Node* WasmGraphBuilder::BuildCallNode(size_t param_count,
                                      base::Vector<Node*> args,
                                      Node* implicit_first_arg,
                                      const Operator* op, Node* frame_state) {
  needs_stack_check_ = true;
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

template <typename T>
Node* WasmGraphBuilder::BuildWasmCall(const Signature<T>* sig,
                                      base::Vector<Node*> args,
                                      base::Vector<Node*> rets,
                                      Node* implicit_first_arg, bool indirect,
                                      Node* frame_state) {
  WasmCallKind call_kind = indirect ? kWasmIndirectFunction : kWasmFunction;
  CallDescriptor* call_descriptor = GetWasmCallDescriptor(
      mcgraph()->zone(), sig, call_kind, frame_state != nullptr);
  const Operator* op = mcgraph()->common()->Call(call_descriptor);
  Node* call = BuildCallNode(sig->parameter_count(), args, implicit_first_arg,
                             op, frame_state);
  // TODO(manoskouk): These assume the call has control and effect outputs.
  DCHECK_GT(op->ControlOutputCount(), 0);
  DCHECK_GT(op->EffectOutputCount(), 0);
  SetEffectControl(call);

  size_t ret_count = sig->return_count();
  if (ret_count == 0) return call;  // No return value.

  DCHECK_EQ(ret_count, rets.size());
  if (ret_count == 1) {
    // Only a single return value.
    rets[0] = call;
  } else {
    // Create projections for all return values.
    for (size_t i = 0; i < ret_count; i++) {
      rets[i] = graph()->NewNode(mcgraph()->common()->Projection(i), call,
                                 graph()->start());
    }
  }
  return call;
}

// Only call this function for code which is not reused across instantiations,
// as we do not patch the embedded js_context.
Node* WasmGraphBuilder::BuildCallToRuntimeWithContext(Runtime::FunctionId f,
                                                      Node* js_context,
                                                      Node** parameters,
                                                      int parameter_count) {
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
      mcgraph()->zone(), f, fun->nargs, Operator::kNoProperties,
      CallDescriptor::kNoFlags);
  // The CEntryStub is loaded from the IsolateRoot so that generated code is
  // Isolate independent. At the moment this is only done for CEntryStub(1).
  Node* isolate_root = BuildLoadIsolateRoot();
  DCHECK_EQ(1, fun->result_size);
  auto centry_id = Builtin::kWasmCEntry;
  int builtin_slot_offset = IsolateData::BuiltinSlotOffset(centry_id);
  Node* centry_stub =
      gasm_->Load(MachineType::Pointer(), isolate_root, builtin_slot_offset);
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
      mcgraph()->ExternalConstant(ExternalReference::Create(f));  // ref
  inputs[count++] = Int32Constant(fun->nargs);                    // arity
  inputs[count++] = js_context;                                   // js_context
  inputs[count++] = effect();
  inputs[count++] = control();

  return gasm_->Call(call_descriptor, count, inputs);
}

TFGraph* WasmGraphBuilder::graph() { return mcgraph()->graph(); }

Zone* WasmGraphBuilder::graph_zone() { return graph()->zone(); }

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

void WasmGraphBuilder::LowerInt64(Signature<MachineRepresentation>* sig) {
  if (mcgraph()->machine()->Is64()) return;
  Int64Lowering r(mcgraph()->graph(), mcgraph()->machine(), mcgraph()->common(),
                  gasm_->simplified(), mcgraph()->zone(), sig);
  r.LowerGraph();
}

void WasmGraphBuilder::LowerInt64(wasm::CallOrigin origin) {
  Signature<MachineRepresentation>* machine_sig =
      function_sig_ != nullptr
          ? CreateMachineSignature(mcgraph()->zone(), function_sig_, origin)
          : CreateMachineSignature(mcgraph()->zone(), wrapper_sig_, origin);
  LowerInt64(machine_sig);
}

void WasmGraphBuilder::Assert(Node* condition, AbortReason abort_reason) {
  if (!v8_flags.debug_code) return;

  Diamond check(graph(), mcgraph()->common(), condition, BranchHint::kTrue);
  check.Chain(control());
  SetControl(check.if_false);
  Node* message_id = gasm_->NumberConstant(static_cast<int32_t>(abort_reason));
  Node* old_effect = effect();
  Node* call = BuildCallToRuntimeWithContext(
      Runtime::kAbort, NoContextConstant(), &message_id, 1);
  check.merge->ReplaceInput(1, call);
  SetEffectControl(check.EffectPhi(old_effect, effect()), check.merge);
}

namespace {

// A non-null {isolate} signifies that the generated code is treated as being in
// a JS frame for functions like BuildLoadIsolateRoot().
class WasmWrapperGraphBuilder : public WasmGraphBuilder {
 public:
  WasmWrapperGraphBuilder(Zone* zone, MachineGraph* mcgraph,
                          const wasm::CanonicalSig* sig,
                          ParameterMode parameter_mode, Isolate* isolate,
                          compiler::SourcePositionTable* spt)
      : WasmGraphBuilder(nullptr, zone, mcgraph, nullptr, spt, parameter_mode,
                         isolate, wasm::WasmEnabledFeatures::All(), sig) {}

  Node* BuildCallAndReturn(Node* js_context, Node* function_data,
                           base::SmallVector<Node*, 16> args, Node* frame_state,
                           bool set_in_wasm_flag) {
    const int rets_count = static_cast<int>(wrapper_sig_->return_count());
    base::SmallVector<Node*, 1> rets(rets_count);

    // Call to an import or a wasm function defined in this module.
    // The (cached) call target is the jump table slot for that function.
    // We do not use the imports dispatch table here so that the wrapper is
    // target independent, in particular for tier-up.
    Node* internal = gasm_->LoadImmutableProtectedPointerFromObject(
        function_data,
        WasmFunctionData::kProtectedInternalOffset - kHeapObjectTag);
    args[0] = gasm_->LoadFromObject(
        MachineType::Uint32(), internal,
        WasmInternalFunction::kRawCallTargetOffset - kHeapObjectTag);
    Node* implicit_arg = gasm_->LoadImmutableProtectedPointerFromObject(
        internal,
        WasmInternalFunction::kProtectedImplicitArgOffset - kHeapObjectTag);
    BuildWasmCall(wrapper_sig_, base::VectorOf(args), base::VectorOf(rets),
                  implicit_arg, true, frame_state);

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

  void BuildJSToWasmWrapper(Node* frame_state, bool set_in_wasm_flag) {
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
    Node* function_data = gasm_->LoadFunctionDataFromJSFunction(js_closure);

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

    auto done = gasm_->MakeLabel(MachineRepresentation::kTagged);
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
        wasm_param = gasm_->TruncateFloat64ToFloat32(wasm_param);
      }

      args[i + 1] = wasm_param;
    }

    Node* jsval = BuildCallAndReturn(js_context, function_data, args,
                                     frame_state, set_in_wasm_flag);

#if V8_ENABLE_DRUMBRAKE
    if (v8_flags.wasm_enable_exec_time_histograms && v8_flags.slow_histograms &&
        !v8_flags.wasm_jitless) {
      Node* runtime_call = BuildCallToRuntimeWithContext(
          Runtime::kWasmTraceEndExecution, js_context, nullptr, 0);
      SetControl(runtime_call);
    }
#endif  // V8_ENABLE_DRUMBRAKE

    Return(jsval);
    if (ContainsInt64(wrapper_sig_)) LowerInt64(wasm::kCalledFromJS);
  }

 private:
  SetOncePointer<const Operator> int32_to_heapnumber_operator_;
  SetOncePointer<const Operator> tagged_non_smi_to_int32_operator_;
  SetOncePointer<const Operator> float32_to_number_operator_;
  SetOncePointer<const Operator> float64_to_number_operator_;
  SetOncePointer<const Operator> tagged_to_float64_operator_;
};

}  // namespace

void BuildInlinedJSToWasmWrapper(Zone* zone, MachineGraph* mcgraph,
                                 const wasm::CanonicalSig* signature,
                                 Isolate* isolate,
                                 compiler::SourcePositionTable* spt,
                                 Node* frame_state, bool set_in_wasm_flag) {
  WasmWrapperGraphBuilder builder(zone, mcgraph, signature,
                                  WasmGraphBuilder::kJSFunctionAbiMode, isolate,
                                  spt);
  builder.BuildJSToWasmWrapper(frame_state, set_in_wasm_flag);
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

#undef LOAD_ROOT

}  // namespace v8::internal::compiler
