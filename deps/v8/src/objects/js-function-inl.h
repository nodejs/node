// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_FUNCTION_INL_H_
#define V8_OBJECTS_JS_FUNCTION_INL_H_

#include "src/objects/js-function.h"

// Include other inline headers *after* including js-function.h, such that e.g.
// the definition of JSFunction is available (and this comment prevents
// clang-format from merging that include into the following ones).
#include "src/codegen/compiler.h"
#include "src/diagnostics/code-tracer.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/shared-function-info-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-function-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(JSFunctionOrBoundFunction)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSBoundFunction)
OBJECT_CONSTRUCTORS_IMPL(JSFunction, JSFunctionOrBoundFunction)

CAST_ACCESSOR(JSFunction)

ACCESSORS(JSFunction, raw_feedback_cell, FeedbackCell, kFeedbackCellOffset)
RELEASE_ACQUIRE_ACCESSORS(JSFunction, raw_feedback_cell, FeedbackCell,
                          kFeedbackCellOffset)

FeedbackVector JSFunction::feedback_vector() const {
  DCHECK(has_feedback_vector());
  return FeedbackVector::cast(raw_feedback_cell().value());
}

ClosureFeedbackCellArray JSFunction::closure_feedback_cell_array() const {
  DCHECK(has_closure_feedback_cell_array());
  return ClosureFeedbackCellArray::cast(raw_feedback_cell().value());
}

bool JSFunction::HasOptimizationMarker() {
  return has_feedback_vector() && feedback_vector().has_optimization_marker();
}

void JSFunction::ClearOptimizationMarker() {
  DCHECK(has_feedback_vector());
  feedback_vector().ClearOptimizationMarker();
}

bool JSFunction::ChecksOptimizationMarker() {
  return code(kAcquireLoad).checks_optimization_marker();
}

bool JSFunction::IsMarkedForOptimization() {
  return has_feedback_vector() && feedback_vector().optimization_marker() ==
                                      OptimizationMarker::kCompileOptimized;
}

bool JSFunction::IsMarkedForConcurrentOptimization() {
  return has_feedback_vector() &&
         feedback_vector().optimization_marker() ==
             OptimizationMarker::kCompileOptimizedConcurrent;
}

void JSFunction::SetInterruptBudget() {
  if (!has_feedback_vector()) {
    DCHECK(shared().is_compiled());
    int budget = FLAG_budget_for_feedback_vector_allocation;
    if (FLAG_feedback_allocation_on_bytecode_size) {
      budget = shared().GetBytecodeArray(GetIsolate()).length() *
               FLAG_scale_factor_for_feedback_allocation;
    }
    raw_feedback_cell().set_interrupt_budget(budget);
    return;
  }
  FeedbackVector::SetInterruptBudget(raw_feedback_cell());
}

void JSFunction::MarkForOptimization(ConcurrencyMode mode) {
  Isolate* isolate = GetIsolate();
  if (!isolate->concurrent_recompilation_enabled() ||
      isolate->bootstrapper()->IsActive()) {
    mode = ConcurrencyMode::kNotConcurrent;
  }

  DCHECK(!is_compiled() || ActiveTierIsIgnition() ||
         ActiveTierIsMidtierTurboprop() || ActiveTierIsBaseline());
  DCHECK(!ActiveTierIsTurbofan());
  DCHECK(shared().IsInterpreted());
  DCHECK(shared().allows_lazy_compilation() ||
         !shared().optimization_disabled());

  if (mode == ConcurrencyMode::kConcurrent) {
    if (IsInOptimizationQueue()) {
      if (FLAG_trace_concurrent_recompilation) {
        PrintF("  ** Not marking ");
        ShortPrint();
        PrintF(" -- already in optimization queue.\n");
      }
      return;
    }
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Marking ");
      ShortPrint();
      PrintF(" for concurrent recompilation.\n");
    }
  }

  SetOptimizationMarker(mode == ConcurrencyMode::kConcurrent
                            ? OptimizationMarker::kCompileOptimizedConcurrent
                            : OptimizationMarker::kCompileOptimized);
}

bool JSFunction::IsInOptimizationQueue() {
  if (!has_feedback_vector()) return false;
  return IsInOptimizationQueueMarker(feedback_vector().optimization_marker());
}

void JSFunction::CompleteInobjectSlackTrackingIfActive() {
  if (!has_prototype_slot()) return;
  if (has_initial_map() && initial_map().IsInobjectSlackTrackingInProgress()) {
    initial_map().CompleteInobjectSlackTracking(GetIsolate());
  }
}

template <typename IsolateT>
AbstractCode JSFunction::abstract_code(IsolateT* isolate) {
  if (ActiveTierIsIgnition()) {
    return AbstractCode::cast(shared().GetBytecodeArray(isolate));
  } else {
    return AbstractCode::cast(code(kAcquireLoad));
  }
}

int JSFunction::length() { return shared().length(); }

ACCESSORS_RELAXED(JSFunction, raw_code, CodeT, kCodeOffset)
RELEASE_ACQUIRE_ACCESSORS(JSFunction, raw_code, CodeT, kCodeOffset)

DEF_GETTER(JSFunction, code, Code) { return FromCodeT(raw_code(cage_base)); }

void JSFunction::set_code(Code code, WriteBarrierMode mode) {
  set_raw_code(ToCodeT(code), mode);
}

DEF_ACQUIRE_GETTER(JSFunction, code, Code) {
  return FromCodeT(raw_code(cage_base, kAcquireLoad));
}

void JSFunction::set_code(Code code, ReleaseStoreTag, WriteBarrierMode mode) {
  set_raw_code(ToCodeT(code), kReleaseStore, mode);
}

Address JSFunction::code_entry_point() const {
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    return CodeDataContainer::cast(raw_code()).code_entry_point();
  } else {
    return code().InstructionStart();
  }
}

// TODO(ishell): Why relaxed read but release store?
DEF_GETTER(JSFunction, shared, SharedFunctionInfo) {
  return shared(cage_base, kRelaxedLoad);
}

DEF_RELAXED_GETTER(JSFunction, shared, SharedFunctionInfo) {
  return TaggedField<SharedFunctionInfo,
                     kSharedFunctionInfoOffset>::Relaxed_Load(cage_base, *this);
}

void JSFunction::set_shared(SharedFunctionInfo value, WriteBarrierMode mode) {
  // Release semantics to support acquire read in NeedsResetDueToFlushedBytecode
  RELEASE_WRITE_FIELD(*this, kSharedFunctionInfoOffset, value);
  CONDITIONAL_WRITE_BARRIER(*this, kSharedFunctionInfoOffset, value, mode);
}

void JSFunction::SetOptimizationMarker(OptimizationMarker marker) {
  DCHECK(has_feedback_vector());
  DCHECK(ChecksOptimizationMarker());
  DCHECK(!ActiveTierIsTurbofan());

  feedback_vector().SetOptimizationMarker(marker);
}

bool JSFunction::has_feedback_vector() const {
  return shared().is_compiled() &&
         raw_feedback_cell().value().IsFeedbackVector();
}

bool JSFunction::has_closure_feedback_cell_array() const {
  return shared().is_compiled() &&
         raw_feedback_cell().value().IsClosureFeedbackCellArray();
}

Context JSFunction::context() {
  return TaggedField<Context, kContextOffset>::load(*this);
}

DEF_RELAXED_GETTER(JSFunction, context, Context) {
  return TaggedField<Context, kContextOffset>::Relaxed_Load(cage_base, *this);
}

bool JSFunction::has_context() const {
  return TaggedField<HeapObject, kContextOffset>::load(*this).IsContext();
}

JSGlobalProxy JSFunction::global_proxy() { return context().global_proxy(); }

NativeContext JSFunction::native_context() {
  return context().native_context();
}

void JSFunction::set_context(HeapObject value, WriteBarrierMode mode) {
  DCHECK(value.IsUndefined() || value.IsContext());
  WRITE_FIELD(*this, kContextOffset, value);
  CONDITIONAL_WRITE_BARRIER(*this, kContextOffset, value, mode);
}

RELEASE_ACQUIRE_ACCESSORS_CHECKED(JSFunction, prototype_or_initial_map,
                                  HeapObject, kPrototypeOrInitialMapOffset,
                                  map().has_prototype_slot())

DEF_GETTER(JSFunction, has_prototype_slot, bool) {
  return map(cage_base).has_prototype_slot();
}

DEF_GETTER(JSFunction, initial_map, Map) {
  return Map::cast(prototype_or_initial_map(cage_base, kAcquireLoad));
}

DEF_GETTER(JSFunction, has_initial_map, bool) {
  DCHECK(has_prototype_slot(cage_base));
  return prototype_or_initial_map(cage_base, kAcquireLoad).IsMap(cage_base);
}

DEF_GETTER(JSFunction, has_instance_prototype, bool) {
  DCHECK(has_prototype_slot(cage_base));
  return has_initial_map(cage_base) ||
         !prototype_or_initial_map(cage_base, kAcquireLoad)
              .IsTheHole(GetReadOnlyRoots(cage_base));
}

DEF_GETTER(JSFunction, has_prototype, bool) {
  DCHECK(has_prototype_slot(cage_base));
  return map(cage_base).has_non_instance_prototype() ||
         has_instance_prototype(cage_base);
}

DEF_GETTER(JSFunction, has_prototype_property, bool) {
  return (has_prototype_slot(cage_base) && IsConstructor(cage_base)) ||
         IsGeneratorFunction(shared(cage_base).kind());
}

DEF_GETTER(JSFunction, PrototypeRequiresRuntimeLookup, bool) {
  return !has_prototype_property(cage_base) ||
         map(cage_base).has_non_instance_prototype();
}

DEF_GETTER(JSFunction, instance_prototype, HeapObject) {
  DCHECK(has_instance_prototype(cage_base));
  if (has_initial_map(cage_base)) {
    return initial_map(cage_base).prototype(cage_base);
  }
  // When there is no initial map and the prototype is a JSReceiver, the
  // initial map field is used for the prototype field.
  return HeapObject::cast(prototype_or_initial_map(cage_base, kAcquireLoad));
}

DEF_GETTER(JSFunction, prototype, Object) {
  DCHECK(has_prototype(cage_base));
  // If the function's prototype property has been set to a non-JSReceiver
  // value, that value is stored in the constructor field of the map.
  if (map(cage_base).has_non_instance_prototype()) {
    Object prototype = map(cage_base).GetConstructor(cage_base);
    // The map must have a prototype in that field, not a back pointer.
    DCHECK(!prototype.IsMap(cage_base));
    DCHECK(!prototype.IsFunctionTemplateInfo(cage_base));
    return prototype;
  }
  return instance_prototype(cage_base);
}

bool JSFunction::is_compiled() const {
  return code(kAcquireLoad).builtin_id() != Builtin::kCompileLazy &&
         shared().is_compiled();
}

bool JSFunction::ShouldFlushBaselineCode(
    base::EnumSet<CodeFlushMode> code_flush_mode) {
  if (!IsBaselineCodeFlushingEnabled(code_flush_mode)) return false;
  // Do a raw read for shared and code fields here since this function may be
  // called on a concurrent thread. JSFunction itself should be fully
  // initialized here but the SharedFunctionInfo, Code objects may not be
  // initialized. We read using acquire loads to defend against that.
  Object maybe_shared = ACQUIRE_READ_FIELD(*this, kSharedFunctionInfoOffset);
  if (!maybe_shared.IsSharedFunctionInfo()) return false;

  // See crbug.com/v8/11972 for more details on acquire / release semantics for
  // code field. We don't use release stores when copying code pointers from
  // SFI / FV to JSFunction but it is safe in practice.
  Object maybe_code = ACQUIRE_READ_FIELD(*this, kCodeOffset);
  if (!maybe_code.IsCodeT()) return false;
  Code code = FromCodeT(CodeT::cast(maybe_code));
  if (code.kind() != CodeKind::BASELINE) return false;

  SharedFunctionInfo shared = SharedFunctionInfo::cast(maybe_shared);
  return shared.ShouldFlushCode(code_flush_mode);
}

bool JSFunction::NeedsResetDueToFlushedBytecode() {
  // Do a raw read for shared and code fields here since this function may be
  // called on a concurrent thread. JSFunction itself should be fully
  // initialized here but the SharedFunctionInfo, Code objects may not be
  // initialized. We read using acquire loads to defend against that.
  Object maybe_shared = ACQUIRE_READ_FIELD(*this, kSharedFunctionInfoOffset);
  if (!maybe_shared.IsSharedFunctionInfo()) return false;

  Object maybe_code = ACQUIRE_READ_FIELD(*this, kCodeOffset);
  if (!maybe_code.IsCodeT()) return false;
  Code code = FromCodeT(CodeT::cast(maybe_code), kRelaxedLoad);

  SharedFunctionInfo shared = SharedFunctionInfo::cast(maybe_shared);
  return !shared.is_compiled() && code.builtin_id() != Builtin::kCompileLazy;
}

bool JSFunction::NeedsResetDueToFlushedBaselineCode() {
  return code().kind() == CodeKind::BASELINE && !shared().HasBaselineData();
}

void JSFunction::ResetIfCodeFlushed(
    base::Optional<std::function<void(HeapObject object, ObjectSlot slot,
                                      HeapObject target)>>
        gc_notify_updated_slot) {
  if (!FLAG_flush_bytecode && !FLAG_flush_baseline_code) return;

  DCHECK_IMPLIES(NeedsResetDueToFlushedBytecode(), FLAG_flush_bytecode);
  if (FLAG_flush_bytecode && NeedsResetDueToFlushedBytecode()) {
    // Bytecode was flushed and function is now uncompiled, reset JSFunction
    // by setting code to CompileLazy and clearing the feedback vector.
    set_code(*BUILTIN_CODE(GetIsolate(), CompileLazy));
    raw_feedback_cell().reset_feedback_vector(gc_notify_updated_slot);
    return;
  }

  DCHECK_IMPLIES(NeedsResetDueToFlushedBaselineCode(),
                 FLAG_flush_baseline_code);
  if (FLAG_flush_baseline_code && NeedsResetDueToFlushedBaselineCode()) {
    DCHECK(FLAG_flush_baseline_code);
    // Flush baseline code from the closure if required
    set_code(*BUILTIN_CODE(GetIsolate(), InterpreterEntryTrampoline));
  }
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_FUNCTION_INL_H_
