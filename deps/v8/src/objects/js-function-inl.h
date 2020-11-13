// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_FUNCTION_INL_H_
#define V8_OBJECTS_JS_FUNCTION_INL_H_

#include "src/codegen/compiler.h"
#include "src/diagnostics/code-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/js-function.h"
#include "src/strings/string-builder-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(JSFunctionOrBoundFunction)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSBoundFunction)
OBJECT_CONSTRUCTORS_IMPL(JSFunction, JSFunctionOrBoundFunction)

CAST_ACCESSOR(JSFunction)

ACCESSORS(JSFunction, raw_feedback_cell, FeedbackCell, kFeedbackCellOffset)

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
  return code().checks_optimization_marker();
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

void JSFunction::MarkForOptimization(ConcurrencyMode mode) {
  Isolate* isolate = GetIsolate();
  if (!isolate->concurrent_recompilation_enabled() ||
      isolate->bootstrapper()->IsActive()) {
    mode = ConcurrencyMode::kNotConcurrent;
  }

  DCHECK(!is_compiled() || ActiveTierIsIgnition() || ActiveTierIsNCI());
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
  return has_feedback_vector() && feedback_vector().optimization_marker() ==
                                      OptimizationMarker::kInOptimizationQueue;
}

void JSFunction::CompleteInobjectSlackTrackingIfActive() {
  if (!has_prototype_slot()) return;
  if (has_initial_map() && initial_map().IsInobjectSlackTrackingInProgress()) {
    initial_map().CompleteInobjectSlackTracking(GetIsolate());
  }
}

AbstractCode JSFunction::abstract_code() {
  if (ActiveTierIsIgnition()) {
    return AbstractCode::cast(shared().GetBytecodeArray());
  } else {
    return AbstractCode::cast(code());
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

void JSFunction::set_code_no_write_barrier(Code value) {
  DCHECK(!ObjectInYoungGeneration(value));
  RELAXED_WRITE_FIELD(*this, kCodeOffset, value);
}

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

void JSFunction::ClearOptimizedCodeSlot(const char* reason) {
  if (has_feedback_vector() && feedback_vector().has_optimized_code()) {
    if (FLAG_trace_opt) {
      CodeTracer::Scope scope(GetIsolate()->GetCodeTracer());
      PrintF(scope.file(),
             "[evicting entry from optimizing code feedback slot (%s) for ",
             reason);
      ShortPrint(scope.file());
      PrintF(scope.file(), "]\n");
    }
    feedback_vector().ClearOptimizedCode();
  }
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

void JSFunction::set_context(HeapObject value) {
  DCHECK(value.IsUndefined() || value.IsContext());
  WRITE_FIELD(*this, kContextOffset, value);
  WRITE_BARRIER(*this, kContextOffset, value);
}

ACCESSORS_CHECKED(JSFunction, prototype_or_initial_map, HeapObject,
                  kPrototypeOrInitialMapOffset, map().has_prototype_slot())

DEF_GETTER(JSFunction, has_prototype_slot, bool) {
  return map(isolate).has_prototype_slot();
}

DEF_GETTER(JSFunction, initial_map, Map) {
  return Map::cast(prototype_or_initial_map(isolate));
}

DEF_GETTER(JSFunction, has_initial_map, bool) {
  DCHECK(has_prototype_slot(isolate));
  return prototype_or_initial_map(isolate).IsMap(isolate);
}

DEF_GETTER(JSFunction, has_instance_prototype, bool) {
  DCHECK(has_prototype_slot(isolate));
  // Can't use ReadOnlyRoots(isolate) as this isolate could be produced by
  // i::GetIsolateForPtrCompr(HeapObject).
  return has_initial_map(isolate) ||
         !prototype_or_initial_map(isolate).IsTheHole(
             GetReadOnlyRoots(isolate));
}

DEF_GETTER(JSFunction, has_prototype, bool) {
  DCHECK(has_prototype_slot(isolate));
  return map(isolate).has_non_instance_prototype() ||
         has_instance_prototype(isolate);
}

DEF_GETTER(JSFunction, has_prototype_property, bool) {
  return (has_prototype_slot(isolate) && IsConstructor(isolate)) ||
         IsGeneratorFunction(shared(isolate).kind());
}

DEF_GETTER(JSFunction, PrototypeRequiresRuntimeLookup, bool) {
  return !has_prototype_property(isolate) ||
         map(isolate).has_non_instance_prototype();
}

DEF_GETTER(JSFunction, instance_prototype, HeapObject) {
  DCHECK(has_instance_prototype(isolate));
  if (has_initial_map(isolate)) return initial_map(isolate).prototype(isolate);
  // When there is no initial map and the prototype is a JSReceiver, the
  // initial map field is used for the prototype field.
  return HeapObject::cast(prototype_or_initial_map(isolate));
}

DEF_GETTER(JSFunction, prototype, Object) {
  DCHECK(has_prototype(isolate));
  // If the function's prototype property has been set to a non-JSReceiver
  // value, that value is stored in the constructor field of the map.
  if (map(isolate).has_non_instance_prototype()) {
    Object prototype = map(isolate).GetConstructor(isolate);
    // The map must have a prototype in that field, not a back pointer.
    DCHECK(!prototype.IsMap(isolate));
    DCHECK(!prototype.IsFunctionTemplateInfo(isolate));
    return prototype;
  }
  return instance_prototype(isolate);
}

bool JSFunction::is_compiled() const {
  return code().builtin_index() != Builtins::kCompileLazy &&
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
