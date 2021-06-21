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
#include "src/heap/heap-inl.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/strings/string-builder-inl.h"

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

  DCHECK(!is_compiled() || ActiveTierIsIgnition() || ActiveTierIsNCI() ||
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

template <typename LocalIsolate>
AbstractCode JSFunction::abstract_code(LocalIsolate* isolate) {
  if (ActiveTierIsIgnition()) {
    return AbstractCode::cast(shared().GetBytecodeArray(isolate));
  } else {
    return AbstractCode::cast(code(kAcquireLoad));
  }
}

int JSFunction::length() { return shared().length(); }

Code JSFunction::code() const {
  return Code::cast(RELAXED_READ_FIELD(*this, kCodeOffset));
}

void JSFunction::set_code(Code value) {
  DCHECK(!ObjectInYoungGeneration(value));
  RELAXED_WRITE_FIELD(*this, kCodeOffset, value);
#ifndef V8_DISABLE_WRITE_BARRIERS
  WriteBarrier::Marking(*this, RawField(kCodeOffset), value);
#endif
}

RELEASE_ACQUIRE_ACCESSORS(JSFunction, code, Code, kCodeOffset)

// TODO(ishell): Why relaxed read but release store?
DEF_GETTER(JSFunction, shared, SharedFunctionInfo) {
  return SharedFunctionInfo::cast(
      RELAXED_READ_FIELD(*this, kSharedFunctionInfoOffset));
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

ACCESSORS_CHECKED(JSFunction, prototype_or_initial_map, HeapObject,
                  kPrototypeOrInitialMapOffset, map().has_prototype_slot())

DEF_GETTER(JSFunction, has_prototype_slot, bool) {
  return map(cage_base).has_prototype_slot();
}

DEF_GETTER(JSFunction, initial_map, Map) {
  return Map::cast(prototype_or_initial_map(cage_base));
}

DEF_GETTER(JSFunction, has_initial_map, bool) {
  DCHECK(has_prototype_slot(cage_base));
  return prototype_or_initial_map(cage_base).IsMap(cage_base);
}

DEF_GETTER(JSFunction, has_instance_prototype, bool) {
  DCHECK(has_prototype_slot(cage_base));
  return has_initial_map(cage_base) ||
         !prototype_or_initial_map(cage_base).IsTheHole(
             GetReadOnlyRoots(cage_base));
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
  if (has_initial_map(cage_base))
    return initial_map(cage_base).prototype(cage_base);
  // When there is no initial map and the prototype is a JSReceiver, the
  // initial map field is used for the prototype field.
  return HeapObject::cast(prototype_or_initial_map(cage_base));
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
  return code(kAcquireLoad).builtin_index() != Builtins::kCompileLazy &&
         shared().is_compiled();
}

bool JSFunction::NeedsResetDueToFlushedBytecode() {
  // Do a raw read for shared and code fields here since this function may be
  // called on a concurrent thread and the JSFunction might not be fully
  // initialized yet.
  Object maybe_shared = ACQUIRE_READ_FIELD(*this, kSharedFunctionInfoOffset);
  Object maybe_code = RELAXED_READ_FIELD(*this, kCodeOffset);

  if (!maybe_shared.IsSharedFunctionInfo() || !maybe_code.IsCode()) {
    return false;
  }

  SharedFunctionInfo shared = SharedFunctionInfo::cast(maybe_shared);
  Code code = Code::cast(maybe_code);
  return !shared.is_compiled() &&
         code.builtin_index() != Builtins::kCompileLazy;
}

void JSFunction::ResetIfBytecodeFlushed(
    base::Optional<std::function<void(HeapObject object, ObjectSlot slot,
                                      HeapObject target)>>
        gc_notify_updated_slot) {
  if (FLAG_flush_bytecode && NeedsResetDueToFlushedBytecode()) {
    // Bytecode was flushed and function is now uncompiled, reset JSFunction
    // by setting code to CompileLazy and clearing the feedback vector.
    set_code(GetIsolate()->builtins()->builtin(i::Builtins::kCompileLazy));
    raw_feedback_cell().reset_feedback_vector(gc_notify_updated_slot);
  }
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_FUNCTION_INL_H_
