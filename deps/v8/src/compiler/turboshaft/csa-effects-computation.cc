// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/csa-effects-computation.h"

#include "src/builtins/builtins-effects-analyzer.h"
#include "src/builtins/builtins.h"
#include "src/codegen/external-reference.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/objects/code-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/utils/ostreams.h"

namespace v8::internal::compiler::turboshaft {

namespace {

bool IsCEntryBuiltin(Builtin builtin) {
  switch (builtin) {
    case Builtin::kCEntry_Return1_ArgvInRegister_NoBuiltinExit:
    case Builtin::kCEntry_Return1_ArgvOnStack_BuiltinExit:
    case Builtin::kCEntry_Return1_ArgvOnStack_NoBuiltinExit:
    case Builtin::kCEntry_Return2_ArgvInRegister_NoBuiltinExit:
    case Builtin::kCEntry_Return2_ArgvOnStack_BuiltinExit:
    case Builtin::kCEntry_Return2_ArgvOnStack_NoBuiltinExit:
    case Builtin::kWasmCEntry:
      return true;
    default:
      return false;
  }
}

}  // namespace

// Just a wrapper around calls to unify CallOp and TailCallOp.
struct CallView {
  V<CallTarget> target;
  base::Vector<const OpIndex> arguments;

  explicit CallView(const Operation& op) {
    switch (op.opcode) {
      case Opcode::kCall: {
        const CallOp& call = op.Cast<CallOp>();
        this->target = call.callee();
        this->arguments = call.arguments();
        return;
      }
      case Opcode::kTailCall: {
        const TailCallOp& call = op.Cast<TailCallOp>();
        this->target = call.callee();
        this->arguments = call.arguments();
        return;
      }

      default:
        UNREACHABLE();
    }
  }
};

std::optional<Builtin> TryGetBuiltin(V<Any> node, Graph& graph) {
  if (const ConstantOp* target_obj_cst =
          graph.Get(node).TryCast<Opmask::kHeapConstant>()) {
    IndirectHandle<HeapObject> target_obj = target_obj_cst->handle();
    if (IsCode(*target_obj)) {
      IndirectHandle<Code> target_code = Cast<Code>(target_obj);
      if (target_code->is_builtin()) {
        return target_code->builtin_id();
      }
    }
  }
  return {};
}

bool AnalyzeRuntimeCall(V<Any> node, BuiltinAllocateEffect* can_allocate,
                        Graph& graph) {
  if (const ConstantOp* cst =
          graph.Get(node).TryCast<Opmask::kExternalConstant>()) {
    ExternalReference reference = cst->external_reference();
    CHECK(!reference.IsIsolateFieldId());

#ifdef USE_SIMULATOR
    const Runtime::Function* fn = Runtime::FunctionForEntry(
        ExternalReference::UnwrapRedirection(reference.address()));
#else
    const Runtime::Function* fn =
        Runtime::FunctionForEntry(reference.address());
#endif

    if (Runtime::kCanTriggerGC[fn->function_id]) {
      *can_allocate = BuiltinAllocateEffect::kYes;
      return true;
    } else {
      // Not updating {can_allocate}, since other calls could make it become
      // true.
      return true;
    }
  }
  return false;
}

void AnalyzeCall(CallView call, std::unordered_set<Builtin>& called_builtins,
                 BuiltinAllocateEffect* can_allocate, Graph& graph,
                 Builtin current_builtin) {
  if (*can_allocate == BuiltinAllocateEffect::kYes) {
    // No need to bother with allocations through builtin calls: this builtin
    // allocates directly.
    return;
  }

  V<CallTarget> target = call.target;
  if (std::optional<Builtin> builtin = TryGetBuiltin(target, graph)) {
    if (*builtin == current_builtin) {
      // Recursive call, no need to record anything.
      return;
    } else if (IsCEntryBuiltin(*builtin)) {
      // The builtin that is being called is the 3rd argument from the end
      // (it's followed by argc and context).
      // TODO(dmercadier): The builtin that's being called is not always the 3rd
      // argument from the end, like for instance in
      // AdaptorWithBuiltinExitFrame0. We have 3 choices to figure out which
      // builtin is really being called: 1) look at the call descriptor here,
      // mirroring what instruction-selector.cc does (this is assuming that the
      // ISEL does indeed know what the target is, but it's possible that it
      // doesn't and just put whatever in whichever register), 2) lower Runtime
      // calls only later in the pipeline and 3) preserve the "target" part of
      // runtime calls on the side.
      CHECK_GE(call.arguments.size(), 2);
      V<Any> actual_callee = call.arguments[call.arguments.size() - 3];
      if (AnalyzeRuntimeCall(actual_callee, can_allocate, graph)) {
        return;
      }
    } else {
      called_builtins.insert(*builtin);
      *can_allocate = BuiltinAllocateEffect::kMaybeWithBuiltinCall;
      return;
    }
  } else if (graph.Get(target).Is<Opmask::kExternalConstant>()) {
    // External functions cannot allocate without conservative stack scanning.
    return;
  }

  *can_allocate = BuiltinAllocateEffect::kYes;
}

void CsaEffectsComputationPhase::Run(PipelineData* data, Zone* temp_zone) {
  // We need to unpark the local heap so that we can look if HeapConstants are
  // builtin code or not.
  UnparkedScopeIfNeeded scope(data->broker());

  Builtin builtin = data->info()->builtin();

  BuiltinAllocateEffect can_allocate = BuiltinAllocateEffect::kNo;
  std::unordered_set<Builtin> called_builtins;
  for (const Operation& op : data->graph().AllOperations()) {
    switch (op.opcode) {
      case Opcode::kAllocate:
        can_allocate = BuiltinAllocateEffect::kYes;
        break;

      case Opcode::kCall:
      case Opcode::kTailCall:
        AnalyzeCall(CallView{op}, called_builtins, &can_allocate, data->graph(),
                    builtin);
        break;

      default:
        break;
    }
  }

  data->isolate()->builtins_effects_analyzer()->RegisterInitialEffects(
      builtin, can_allocate, called_builtins);
}

}  // namespace v8::internal::compiler::turboshaft
