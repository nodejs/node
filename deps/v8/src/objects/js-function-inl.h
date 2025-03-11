// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_FUNCTION_INL_H_
#define V8_OBJECTS_JS_FUNCTION_INL_H_

#include <optional>

#include "src/objects/js-function.h"

// Include other inline headers *after* including js-function.h, such that e.g.
// the definition of JSFunction is available (and this comment prevents
// clang-format from merging that include into the following ones).
#include "src/diagnostics/code-tracer.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/objects/abstract-code-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-updater.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/sandbox/js-dispatch-table-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/js-function-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(JSFunctionOrBoundFunctionOrWrappedFunction)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSBoundFunction)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSWrappedFunction)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSFunction)

ACCESSORS(JSFunction, raw_feedback_cell, Tagged<FeedbackCell>,
          kFeedbackCellOffset)
RELEASE_ACQUIRE_ACCESSORS(JSFunction, raw_feedback_cell, Tagged<FeedbackCell>,
                          kFeedbackCellOffset)

DEF_GETTER(JSFunction, feedback_vector, Tagged<FeedbackVector>) {
  DCHECK(has_feedback_vector(cage_base));
  return Cast<FeedbackVector>(raw_feedback_cell(cage_base)->value(cage_base));
}

Tagged<ClosureFeedbackCellArray> JSFunction::closure_feedback_cell_array()
    const {
  DCHECK(has_closure_feedback_cell_array());
  return Cast<ClosureFeedbackCellArray>(raw_feedback_cell()->value());
}

void JSFunction::reset_tiering_state() {
  DCHECK(has_feedback_vector());
  feedback_vector()->reset_tiering_state();
}

bool JSFunction::ChecksTieringState(IsolateForSandbox isolate) {
  return code(isolate)->checks_tiering_state();
}

void JSFunction::CompleteInobjectSlackTrackingIfActive() {
  if (!has_prototype_slot()) return;
  if (has_initial_map() && initial_map()->IsInobjectSlackTrackingInProgress()) {
    MapUpdater::CompleteInobjectSlackTracking(GetIsolate(), initial_map());
  }
}

template <typename IsolateT>
Tagged<AbstractCode> JSFunction::abstract_code(IsolateT* isolate) {
  if (ActiveTierIsIgnition(isolate)) {
    return Cast<AbstractCode>(shared()->GetBytecodeArray(isolate));
  } else {
    return Cast<AbstractCode>(code(isolate, kAcquireLoad));
  }
}

int JSFunction::length() { return shared()->length(); }

void JSFunction::UpdateMaybeContextSpecializedCode(Isolate* isolate,
                                                   Tagged<Code> value,
                                                   WriteBarrierMode mode) {
  if (value->is_context_specialized()) {
    UpdateContextSpecializedCode(isolate, value, mode);
  } else {
    UpdateCode(value, mode);
  }
}

void JSFunction::UpdateContextSpecializedCode(Isolate* isolate,
                                              Tagged<Code> value,
                                              WriteBarrierMode mode) {
  DisallowGarbageCollection no_gc;
  DCHECK(value->is_context_specialized());

#ifdef V8_ENABLE_LEAPTIERING
  JSDispatchHandle handle = dispatch_handle();
  JSDispatchHandle canonical_handle = raw_feedback_cell()->dispatch_handle();

  // For specialized code we allocate their own dispatch entry, which is
  // different from the one in the dispatch cell.
  // TODO(olivf): In case we have a NoClosuresFeedbackCell we could steal the
  // existing dispatch entry and install a yet to be implemented shared lazy
  // updating dispatch entry on the feedback cell.
  DCHECK_NE(canonical_handle, kNullJSDispatchHandle);
  DCHECK(value->is_context_specialized());
  DCHECK(value->is_optimized_code());
  bool has_context_specialized_dispatch_entry = handle != canonical_handle;
  if (has_context_specialized_dispatch_entry) {
    set_code(value, mode);
  } else {
    allocate_dispatch_handle(isolate, value->parameter_count(), value, mode);
  }
#else
  WriteCodePointerField(kCodeOffset, value);
  CONDITIONAL_CODE_POINTER_WRITE_BARRIER(*this, kCodeOffset, value, mode);
#endif  // V8_ENABLE_LEAPTIERING

  if (V8_UNLIKELY(v8_flags.log_function_events && has_feedback_vector())) {
    feedback_vector()->set_log_next_execution(true);
  }
}

void JSFunction::UpdateCode(Tagged<Code> value, WriteBarrierMode mode) {
  DisallowGarbageCollection no_gc;
  DCHECK(!value->is_context_specialized());

#ifdef V8_ENABLE_LEAPTIERING
  JSDispatchHandle canonical_handle = raw_feedback_cell()->dispatch_handle();

#ifdef DEBUG
  bool has_context_specialized_dispatch_entry =
      canonical_handle != kNullJSDispatchHandle &&
      dispatch_handle() != canonical_handle;
  if (has_context_specialized_dispatch_entry) {
    auto jdt = GetProcessWideJSDispatchTable();
    DCHECK_IMPLIES(jdt->GetCode(dispatch_handle())->kind() != CodeKind::BUILTIN,
                   jdt->GetCode(dispatch_handle())->is_context_specialized());
  }
  DCHECK_NE(dispatch_handle(), kNullJSDispatchHandle);
#endif  // DEBUG

  if (canonical_handle != kNullJSDispatchHandle) {
    // Ensure we are using the canonical dispatch handle (needed in case this
    // function was specialized before).
    set_dispatch_handle(canonical_handle, mode);
  }
  set_code(value, mode);

#else
  WriteCodePointerField(kCodeOffset, value);
  CONDITIONAL_CODE_POINTER_WRITE_BARRIER(*this, kCodeOffset, value, mode);
#endif  // V8_ENABLE_LEAPTIERING

  if (V8_UNLIKELY(v8_flags.log_function_events && has_feedback_vector())) {
    feedback_vector()->set_log_next_execution(true);
  }
}

Tagged<Code> JSFunction::code(IsolateForSandbox isolate) const {
#ifdef V8_ENABLE_LEAPTIERING
  return GetProcessWideJSDispatchTable()->GetCode(dispatch_handle());
#else
  return ReadCodePointerField(kCodeOffset, isolate);
#endif
}

Tagged<Code> JSFunction::code(IsolateForSandbox isolate,
                              AcquireLoadTag tag) const {
#ifdef V8_ENABLE_LEAPTIERING
  return GetProcessWideJSDispatchTable()->GetCode(dispatch_handle(tag));
#else
  return ReadCodePointerField(kCodeOffset, isolate);
#endif
}

Tagged<Object> JSFunction::raw_code(IsolateForSandbox isolate) const {
#if V8_ENABLE_LEAPTIERING
  JSDispatchHandle handle = dispatch_handle();
  if (handle == kNullJSDispatchHandle) return Smi::zero();
  return GetProcessWideJSDispatchTable()->GetCode(handle);
#elif V8_ENABLE_SANDBOX
  return RawIndirectPointerField(kCodeOffset, kCodeIndirectPointerTag)
      .Relaxed_Load(isolate);
#else
  return RELAXED_READ_FIELD(*this, JSFunction::kCodeOffset);
#endif  // V8_ENABLE_SANDBOX
}

Tagged<Object> JSFunction::raw_code(IsolateForSandbox isolate,
                                    AcquireLoadTag tag) const {
#if V8_ENABLE_LEAPTIERING
  JSDispatchHandle handle = dispatch_handle(tag);
  if (handle == kNullJSDispatchHandle) return Smi::zero();
  return GetProcessWideJSDispatchTable()->GetCode(handle);
#elif V8_ENABLE_SANDBOX
  return RawIndirectPointerField(kCodeOffset, kCodeIndirectPointerTag)
      .Acquire_Load(isolate);
#else
  return ACQUIRE_READ_FIELD(*this, JSFunction::kCodeOffset);
#endif  // V8_ENABLE_SANDBOX
}

#ifdef V8_ENABLE_LEAPTIERING
void JSFunction::allocate_dispatch_handle(IsolateForSandbox isolate,
                                          uint16_t parameter_count,
                                          Tagged<Code> code,
                                          WriteBarrierMode mode) {
  AllocateAndInstallJSDispatchHandle(kDispatchHandleOffset, isolate,
                                     parameter_count, code, mode);
}

void JSFunction::clear_dispatch_handle() {
  WriteField<JSDispatchHandle>(kDispatchHandleOffset, kNullJSDispatchHandle);
}
void JSFunction::set_dispatch_handle(JSDispatchHandle handle,
                                     WriteBarrierMode mode) {
  Relaxed_WriteField<JSDispatchHandle>(kDispatchHandleOffset, handle);
  CONDITIONAL_JS_DISPATCH_HANDLE_WRITE_BARRIER(*this, handle, mode);
}
void JSFunction::set_code(Tagged<Code> new_code, WriteBarrierMode mode) {
  JSDispatchHandle handle = dispatch_handle();
  GetProcessWideJSDispatchTable()->SetCode(handle, new_code);
  CONDITIONAL_JS_DISPATCH_HANDLE_WRITE_BARRIER(*this, handle, mode);
}
JSDispatchHandle JSFunction::dispatch_handle() const {
  return Relaxed_ReadField<JSDispatchHandle>(kDispatchHandleOffset);
}

JSDispatchHandle JSFunction::dispatch_handle(AcquireLoadTag tag) const {
  return Acquire_ReadField<JSDispatchHandle>(kDispatchHandleOffset);
}
#endif  // V8_ENABLE_LEAPTIERING

RELEASE_ACQUIRE_ACCESSORS(JSFunction, context, Tagged<Context>, kContextOffset)

Address JSFunction::instruction_start(IsolateForSandbox isolate) const {
  return code(isolate)->instruction_start();
}

// TODO(ishell): Why relaxed read but release store?
DEF_GETTER(JSFunction, shared, Tagged<SharedFunctionInfo>) {
  return shared(cage_base, kRelaxedLoad);
}

DEF_RELAXED_GETTER(JSFunction, shared, Tagged<SharedFunctionInfo>) {
  return TaggedField<SharedFunctionInfo,
                     kSharedFunctionInfoOffset>::Relaxed_Load(cage_base, *this);
}

void JSFunction::set_shared(Tagged<SharedFunctionInfo> value,
                            WriteBarrierMode mode) {
  // Release semantics to support acquire read in NeedsResetDueToFlushedBytecode
  RELEASE_WRITE_FIELD(*this, kSharedFunctionInfoOffset, value);
  CONDITIONAL_WRITE_BARRIER(*this, kSharedFunctionInfoOffset, value, mode);
}

TieringState JSFunction::tiering_state() const {
  if (!has_feedback_vector()) return TieringState::kNone;
  return feedback_vector()->tiering_state();
}

void JSFunction::set_tiering_state(IsolateForSandbox isolate,
                                   TieringState state) {
  DCHECK(has_feedback_vector());
  DCHECK(IsNone(state) || ChecksTieringState(isolate));
  feedback_vector()->set_tiering_state(state);
}

bool JSFunction::osr_tiering_in_progress() {
  DCHECK(has_feedback_vector());
  return feedback_vector()->osr_tiering_in_progress();
}

void JSFunction::set_osr_tiering_in_progress(bool osr_in_progress) {
  DCHECK(has_feedback_vector());
  feedback_vector()->set_osr_tiering_in_progress(osr_in_progress);
}

DEF_GETTER(JSFunction, has_feedback_vector, bool) {
  return shared(cage_base)->is_compiled() &&
         IsFeedbackVector(raw_feedback_cell(cage_base)->value(cage_base),
                          cage_base);
}

bool JSFunction::has_closure_feedback_cell_array() const {
  return shared()->is_compiled() &&
         IsClosureFeedbackCellArray(raw_feedback_cell()->value());
}

Tagged<Context> JSFunction::context() {
  return TaggedField<Context, kContextOffset>::load(*this);
}

DEF_RELAXED_GETTER(JSFunction, context, Tagged<Context>) {
  return TaggedField<Context, kContextOffset>::Relaxed_Load(cage_base, *this);
}

bool JSFunction::has_context() const {
  return IsContext(TaggedField<HeapObject, kContextOffset>::load(*this));
}

Tagged<JSGlobalProxy> JSFunction::global_proxy() {
  return context()->global_proxy();
}

Tagged<NativeContext> JSFunction::native_context() {
  return context()->native_context();
}

RELEASE_ACQUIRE_ACCESSORS_CHECKED(JSFunction, prototype_or_initial_map,
                                  Tagged<HeapObject>,
                                  kPrototypeOrInitialMapOffset,
                                  map()->has_prototype_slot())

DEF_GETTER(JSFunction, has_prototype_slot, bool) {
  return map(cage_base)->has_prototype_slot();
}

DEF_GETTER(JSFunction, initial_map, Tagged<Map>) {
  return Cast<Map>(prototype_or_initial_map(cage_base, kAcquireLoad));
}

DEF_GETTER(JSFunction, has_initial_map, bool) {
  DCHECK(has_prototype_slot(cage_base));
  return IsMap(prototype_or_initial_map(cage_base, kAcquireLoad), cage_base);
}

DEF_GETTER(JSFunction, has_instance_prototype, bool) {
  DCHECK(has_prototype_slot(cage_base));
  return has_initial_map(cage_base) ||
         !IsTheHole(prototype_or_initial_map(cage_base, kAcquireLoad),
                    GetReadOnlyRoots(cage_base));
}

DEF_GETTER(JSFunction, has_prototype, bool) {
  DCHECK(has_prototype_slot(cage_base));
  return map(cage_base)->has_non_instance_prototype() ||
         has_instance_prototype(cage_base);
}

DEF_GETTER(JSFunction, has_prototype_property, bool) {
  return (has_prototype_slot(cage_base) && IsConstructor(*this, cage_base)) ||
         IsGeneratorFunction(shared(cage_base)->kind());
}

DEF_GETTER(JSFunction, PrototypeRequiresRuntimeLookup, bool) {
  return !has_prototype_property(cage_base) ||
         map(cage_base)->has_non_instance_prototype();
}

DEF_GETTER(JSFunction, instance_prototype, Tagged<HeapObject>) {
  DCHECK(has_instance_prototype(cage_base));
  if (has_initial_map(cage_base)) {
    return initial_map(cage_base)->prototype(cage_base);
  }
  // When there is no initial map and the prototype is a JSReceiver, the
  // initial map field is used for the prototype field.
  return Cast<HeapObject>(prototype_or_initial_map(cage_base, kAcquireLoad));
}

DEF_GETTER(JSFunction, prototype, Tagged<Object>) {
  DCHECK(has_prototype(cage_base));
  // If the function's prototype property has been set to a non-JSReceiver
  // value, that value is stored in the constructor field of the map.
  Tagged<Map> map = this->map(cage_base);
  if (map->has_non_instance_prototype()) {
    return map->GetNonInstancePrototype(cage_base);
  }
  return instance_prototype(cage_base);
}

bool JSFunction::is_compiled(IsolateForSandbox isolate) const {
  return code(isolate, kAcquireLoad)->builtin_id() != Builtin::kCompileLazy &&
         shared()->is_compiled();
}

bool JSFunction::NeedsResetDueToFlushedBytecode(IsolateForSandbox isolate) {
  // Do a raw read for shared and code fields here since this function may be
  // called on a concurrent thread. JSFunction itself should be fully
  // initialized here but the SharedFunctionInfo, Code objects may not be
  // initialized. We read using acquire loads to defend against that.
  // TODO(v8) the branches for !IsSharedFunctionInfo() and !IsCode() are
  // probably dead code by now. Investigate removing them or replacing them
  // with CHECKs.
  Tagged<Object> maybe_shared =
      ACQUIRE_READ_FIELD(*this, kSharedFunctionInfoOffset);
  if (!IsSharedFunctionInfo(maybe_shared)) return false;

  Tagged<Object> maybe_code = raw_code(isolate, kAcquireLoad);
  if (!IsCode(maybe_code)) return false;
  Tagged<Code> code = Cast<Code>(maybe_code);

  Tagged<SharedFunctionInfo> shared = Cast<SharedFunctionInfo>(maybe_shared);
  return !shared->is_compiled() && code->builtin_id() != Builtin::kCompileLazy;
}

bool JSFunction::NeedsResetDueToFlushedBaselineCode(IsolateForSandbox isolate) {
  return code(isolate)->kind() == CodeKind::BASELINE &&
         !shared()->HasBaselineCode();
}

void JSFunction::ResetIfCodeFlushed(
    Isolate* isolate,
    std::optional<std::function<void(Tagged<HeapObject> object, ObjectSlot slot,
                                     Tagged<HeapObject> target)>>
        gc_notify_updated_slot) {
  const bool kBytecodeCanFlush =
      v8_flags.flush_bytecode || v8_flags.stress_snapshot;
  const bool kBaselineCodeCanFlush =
      v8_flags.flush_baseline_code || v8_flags.stress_snapshot;
  if (!kBytecodeCanFlush && !kBaselineCodeCanFlush) return;

  DCHECK_IMPLIES(NeedsResetDueToFlushedBytecode(isolate), kBytecodeCanFlush);
  if (kBytecodeCanFlush && NeedsResetDueToFlushedBytecode(isolate)) {
    // Bytecode was flushed and function is now uncompiled, reset JSFunction
    // by setting code to CompileLazy and clearing the feedback vector.
    UpdateCode(*BUILTIN_CODE(isolate, CompileLazy));
    raw_feedback_cell()->reset_feedback_vector(gc_notify_updated_slot);
    return;
  }

  DCHECK_IMPLIES(NeedsResetDueToFlushedBaselineCode(isolate),
                 kBaselineCodeCanFlush);
  if (kBaselineCodeCanFlush && NeedsResetDueToFlushedBaselineCode(isolate)) {
    // Flush baseline code from the closure if required
    UpdateCode(*BUILTIN_CODE(isolate, InterpreterEntryTrampoline));
  }
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_FUNCTION_INL_H_
