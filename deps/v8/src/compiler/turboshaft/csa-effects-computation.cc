// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/csa-effects-computation.h"

#include "src/builtins/builtins-effects-analyzer.h"
#include "src/builtins/builtins.h"
#include "src/codegen/external-reference.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/objects/code-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/utils/ostreams.h"

namespace v8::internal::compiler::turboshaft {

// Just a wrapper around calls to unify CallOp and TailCallOp.
struct CallView {
  V<CallTarget> target;
  base::Vector<const OpIndex> arguments;
  const TSCallDescriptor* descriptor;

  explicit CallView(const Operation& op) {
    switch (op.opcode) {
      case Opcode::kCall: {
        const CallOp& call = op.Cast<CallOp>();
        this->target = call.callee();
        this->arguments = call.arguments();
        this->descriptor = call.descriptor;
        return;
      }
      case Opcode::kTailCall: {
        const TailCallOp& call = op.Cast<TailCallOp>();
        this->target = call.callee();
        this->arguments = call.arguments();
        this->descriptor = call.descriptor;
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
    if (IndirectHandle<Code> target_code; TryCast(target_obj, &target_code)) {
      if (target_code->is_builtin()) {
        return target_code->builtin_id();
      }
    }
  }
  return {};
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
    } else if (std::optional<Runtime::FunctionId> runtime_function_id =
                   call.descriptor->descriptor->runtime_function_id()) {
      // We special case Abort, which can allocate, but always result from
      // something unexpected already having happened, and never resumes
      // execution afterwards. This could lead to the GC seeing uninitialized
      // memory, which is unfortunate, but it's better this way rather than
      // having over-conservative effects because we try to terminate execution
      // cleanly through Abort rather than just segfaulting abruptly.
      if (Runtime::kCanTriggerGC[runtime_function_id.value()] &&
          runtime_function_id.value() != Runtime::kAbort) {
        *can_allocate = BuiltinAllocateEffect::kYes;
      }
      return;
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
