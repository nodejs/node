// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-compilation-unit.h"

#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/js-function-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

MaglevCompilationUnit::MaglevCompilationUnit(MaglevCompilationInfo* info,
                                             DirectHandle<JSFunction> function)
    : MaglevCompilationUnit(
          info, nullptr,
          MakeRef(info->broker(), info->broker()->CanonicalPersistentHandle(
                                      function->shared())),
          MakeRef(info->broker(), info->broker()->CanonicalPersistentHandle(
                                      function->raw_feedback_cell()))) {}

MaglevCompilationUnit::MaglevCompilationUnit(
    MaglevCompilationInfo* info, const MaglevCompilationUnit* caller,
    compiler::SharedFunctionInfoRef shared_function_info,
    compiler::FeedbackCellRef feedback_cell, ValueNode* context,
    ValueNode* function)
    : info_(info),
      caller_(caller),
      shared_function_info_(shared_function_info),
      bytecode_(shared_function_info.GetBytecodeArray(broker())),
      feedback_cell_(feedback_cell),
      register_count_(bytecode_->register_count()),
      parameter_count_(bytecode_->parameter_count()),
      max_arguments_(bytecode_->max_arguments()),
      inlining_depth_(caller == nullptr ? 0 : caller->inlining_depth_ + 1) {
  // Check that the parameter count in the bytecode and in the shared function
  // info are consistent.
  DCHECK_EQ(bytecode_->parameter_count(),
            shared_function_info
                .internal_formal_parameter_count_with_receiver_deprecated());
  if (caller != nullptr) {
    InitializeSpecializationContextForInlined(context, function);
  }
}

namespace {
std::pair<compiler::ScopeInfoRef, bool> GetIncomingContextScopeInfo(
    compiler::JSHeapBroker* broker,
    compiler::SharedFunctionInfoRef shared_function_info) {
  compiler::ScopeInfoRef scope_info = shared_function_info.scope_info(broker);
  if (scope_info.HasOuterScopeInfo()) {
    scope_info = scope_info.OuterScopeInfo(broker);
    CHECK(scope_info.HasContext());
    return {scope_info, true};
  }
  return {scope_info,
          shared_function_info.is_toplevel() && scope_info.HasContext()};
}

std::optional<std::pair<compiler::ContextRef, size_t>>
FindModuleOrScriptContext(compiler::JSHeapBroker* broker,
                          compiler::ContextRef current) {
  size_t dist = 0;
  while (true) {
    InstanceType instance_type = current.map(broker).instance_type();
    if (instance_type == NATIVE_CONTEXT_TYPE) {
      return std::nullopt;
    }
    if (instance_type == MODULE_CONTEXT_TYPE ||
        instance_type == SCRIPT_CONTEXT_TYPE) {
      return std::make_pair(current, dist);
    }
    size_t step = 1;
    current = current.previous(broker, &step);
    if (step != 0) return std::nullopt;
    dist++;
  }
}
}  // namespace

void MaglevCompilationUnit::InitializeSpecializationContextFromKnownContext(
    compiler::ContextRef current, compiler::ScopeInfoRef scope_info,
    bool has_incoming_context_scope, bool specialize_to_function_context) {
  std::optional<size_t> distance;
  if (has_incoming_context_scope) {
    if (specialize_to_function_context) {
      if (current.map(broker()).instance_type() != NATIVE_CONTEXT_TYPE) {
        specialization_context_ = current;
        distance = 0;
      }
    } else if (v8_flags.always_specialize_for_script_context) {
      if (auto found = FindModuleOrScriptContext(broker(), current)) {
        specialization_context_ = found->first;
        distance = found->second;
      }
    }
  }
  incoming_context_scope_info_ = ContextScopeInfo(scope_info, distance);
}

void MaglevCompilationUnit::InitializeSpecializationContextForTopLevel() {
  DCHECK(!is_inline());
  DCHECK(!specialization_context_initialized_);
#ifdef DEBUG
  specialization_context_initialized_ = true;
#endif
  auto [scope_info, has_incoming_context_scope] =
      GetIncomingContextScopeInfo(broker(), *shared_function_info_);
  if (v8_flags.always_specialize_for_script_context) {
    compiler::JSFunctionRef func_ref = compiler::MakeRefAssumeMemoryFence(
        broker(),
        broker()->CanonicalPersistentHandle(info_->toplevel_function()));
    InitializeSpecializationContextFromKnownContext(
        func_ref.context(broker()), scope_info, has_incoming_context_scope,
        info_->specialize_to_function_context());
  } else {
    incoming_context_scope_info_ = ContextScopeInfo(scope_info);
  }
}

void MaglevCompilationUnit::InitializeSpecializationContextForInlined(
    ValueNode* context, ValueNode* function) {
  DCHECK(is_inline());
  DCHECK_NOT_NULL(context);
  DCHECK_NOT_NULL(function);
  DCHECK(!specialization_context_initialized_);
#ifdef DEBUG
  specialization_context_initialized_ = true;
#endif
  auto [scope_info, has_incoming_context_scope] =
      GetIncomingContextScopeInfo(broker(), *shared_function_info_);
  if (!v8_flags.always_specialize_for_script_context ||
      !has_incoming_context_scope) {
    incoming_context_scope_info_ = ContextScopeInfo(scope_info);
    return;
  }

  // Case 1: Inlined unit with a known constant context (e.g. a known constant
  // JSFunctionRef, which could even be from another script).
  if (HeapConstant* constant_context = context->TryCast<HeapConstant>()) {
    InitializeSpecializationContextFromKnownContext(
        constant_context->ref().AsContext(), scope_info,
        has_incoming_context_scope,
        v8_flags.maglev_function_context_specialization);
    return;
  }

  // Case 2: Closure created by us (`FastCreateClosure` / `CreateClosure`)
  // inherits the specialization context and current scope info from its
  // creation site. Note that `caller_` is the compilation unit where the
  // closure is called, which may differ from the compilation unit where the
  // closure was created (e.g. when a factory function is inlined into `caller_`
  // and returns a fresh closure allocated in a new FunctionContext with the
  // same ScopeInfo as `caller_->specialization_context()`).
  if (FastCreateClosure* fast_closure =
          function->TryCast<FastCreateClosure>()) {
    DCHECK_EQ(fast_closure->context_scope_info().value(), scope_info);
    specialization_context_ = fast_closure->specialization_context();
    incoming_context_scope_info_ = fast_closure->context_scope_info();
    return;
  }
  if (CreateClosure* slow_closure = function->TryCast<CreateClosure>()) {
    DCHECK_EQ(slow_closure->context_scope_info().value(), scope_info);
    specialization_context_ = slow_closure->specialization_context();
    incoming_context_scope_info_ = slow_closure->context_scope_info();
    return;
  }

  // Case 3: Dynamic closure via `FeedbackCell` cannot specialize to a
  // `FunctionContext`, so walk `caller_->specialization_context()` outward to
  // find an enclosing `SCRIPT_CONTEXT_TYPE` or `MODULE_CONTEXT_TYPE`.
  std::optional<size_t> distance;
  if (caller_->specialization_context().has_value()) {
    if (auto found = FindModuleOrScriptContext(
            broker(), *caller_->specialization_context())) {
      compiler::ScopeInfoRef outer_scope = found->first.scope_info(broker());
      compiler::ScopeInfoRef curr = scope_info;
      size_t dist = 0;
      while (true) {
        if (curr.equals(outer_scope)) {
          specialization_context_ = found->first;
          distance = dist;
          break;
        }
        if (!curr.HasOuterScopeInfo()) break;
        curr = curr.OuterScopeInfo(broker());
        dist++;
      }
    }
  }
  incoming_context_scope_info_ = ContextScopeInfo(scope_info, distance);
}

MaglevCompilationUnit::MaglevCompilationUnit(
    MaglevCompilationInfo* info, const MaglevCompilationUnit* caller,
    int register_count, uint16_t parameter_count, uint16_t max_arguments)
    : info_(info),
      caller_(caller),
      register_count_(register_count),
      parameter_count_(parameter_count),
      max_arguments_(max_arguments),
      inlining_depth_(caller == nullptr ? 0 : caller->inlining_depth_ + 1) {}

compiler::JSHeapBroker* MaglevCompilationUnit::broker() const {
  return info_->broker();
}

Zone* MaglevCompilationUnit::zone() const { return info_->zone(); }

bool MaglevCompilationUnit::has_graph_labeller() const {
  return info_->has_graph_labeller();
}

MaglevGraphLabeller* MaglevCompilationUnit::graph_labeller() const {
  DCHECK(has_graph_labeller());
  return info_->graph_labeller();
}

bool MaglevCompilationUnit::is_tracing_enabled() const {
  return info_->is_tracing_enabled();
}

void MaglevCompilationUnit::RegisterNodeInGraphLabeller(const Node* node) {
  if (has_graph_labeller()) {
    graph_labeller()->RegisterNode(node);
  }
}

const MaglevCompilationUnit* MaglevCompilationUnit::GetTopLevelCompilationUnit()
    const {
  const MaglevCompilationUnit* unit = this;
  while (unit->is_inline()) {
    unit = unit->caller();
  }
  return unit;
}

bool MaglevCompilationUnit::is_osr() const {
  return inlining_depth_ == 0 && info_->toplevel_is_osr();
}

BytecodeOffset MaglevCompilationUnit::osr_offset() const {
  return is_osr() ? info_->toplevel_osr_offset() : BytecodeOffset::None();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
