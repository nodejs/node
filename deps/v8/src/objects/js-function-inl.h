// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_FUNCTION_INL_H_
#define V8_OBJECTS_JS_FUNCTION_INL_H_

#include "src/objects/js-function.h"
// Include the non-inl header before the rest of the headers.

#include <atomic>
#include <optional>

// Include other inline headers *after* including js-function.h, such that e.g.
// the definition of JSFunction is available (and this comment prevents
// clang-format from merging that include into the following ones).
#include "src/debug/debug.h"
#include "src/diagnostics/code-tracer.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/objects/abstract-code.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-updater.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/sandbox/js-dispatch-table-inl.h"
#include "src/snapshot/embedded/embedded-data.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

// JSBoundFunction accessors.
Tagged<JSCallable> JSBoundFunction::bound_target_function() const {
  return bound_target_function_.load();
}
void JSBoundFunction::set_bound_target_function(Tagged<JSCallable> value,
                                                WriteBarrierMode mode) {
  bound_target_function_.store(this, value, mode);
}
Tagged<Object> JSBoundFunction::bound_this() const {
  return bound_this_.load();
}
void JSBoundFunction::set_bound_this(Tagged<Object> value,
                                     WriteBarrierMode mode) {
  bound_this_.store(this, value, mode);
}
Tagged<FixedArray> JSBoundFunction::bound_arguments() const {
  return Cast<FixedArray>(bound_arguments_.load());
}
void JSBoundFunction::set_bound_arguments(Tagged<FixedArray> value,
                                          WriteBarrierMode mode) {
  bound_arguments_.store(this, value, mode);
}

// JSWrappedFunction accessors.
Tagged<JSCallable> JSWrappedFunction::wrapped_target_function() const {
  return wrapped_target_function_.load();
}
void JSWrappedFunction::set_wrapped_target_function(Tagged<JSCallable> value,
                                                    WriteBarrierMode mode) {
  wrapped_target_function_.store(this, value, mode);
}
Tagged<NativeContext> JSWrappedFunction::context() const {
  return Cast<NativeContext>(context_.load());
}
void JSWrappedFunction::set_context(Tagged<NativeContext> value,
                                    WriteBarrierMode mode) {
  context_.store(this, value, mode);
}

Tagged<FeedbackCell> JSFunction::raw_feedback_cell() const {
  return Cast<FeedbackCell>(feedback_cell_.load());
}
void JSFunction::set_raw_feedback_cell(Tagged<FeedbackCell> value,
                                       WriteBarrierMode mode) {
  feedback_cell_.store(this, value, mode);
}
Tagged<FeedbackCell> JSFunction::raw_feedback_cell(AcquireLoadTag) const {
  return Cast<FeedbackCell>(feedback_cell_.Acquire_Load());
}
void JSFunction::set_raw_feedback_cell(Tagged<FeedbackCell> value,
                                       ReleaseStoreTag, WriteBarrierMode mode) {
  feedback_cell_.Release_Store(this, value, mode);
}

Tagged<FeedbackVector> JSFunction::feedback_vector() const {
  DCHECK(has_feedback_vector());
  return Cast<FeedbackVector>(raw_feedback_cell()->value());
}

Tagged<ClosureFeedbackCellArray> JSFunction::closure_feedback_cell_array()
    const {
  DCHECK(has_closure_feedback_cell_array());
  return Cast<ClosureFeedbackCellArray>(raw_feedback_cell()->value());
}

bool JSFunction::ChecksTieringState(IsolateForSandbox isolate) {
  return code(isolate)->checks_tiering_state();
}

void JSFunction::CompleteInobjectSlackTrackingIfActive(Isolate* isolate) {
  if (!has_prototype_slot()) return;
  Tagged<Map> initial_map;
  if (TryGetInitialMap(&initial_map) &&
      initial_map->IsInobjectSlackTrackingInProgress()) {
    MapUpdater::CompleteInobjectSlackTracking(isolate, initial_map);
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

uint32_t JSFunction::length() { return shared()->length(); }

void JSFunction::UpdateOptimizedCode(Isolate* isolate, Tagged<Code> code,
                                     WriteBarrierMode mode) {
  DisallowGarbageCollection no_gc;
  DCHECK(code->is_optimized_code());
  if (code->is_context_specialized()) {
    // We can only set context-specialized code for single-closure cells.
    if (raw_feedback_cell()->map() !=
        ReadOnlyRoots(isolate).one_closure_cell_map()) {
      return;
    }
  }
  // Required for being able to deoptimize this code.
  code->set_js_dispatch_handle(dispatch_handle());
  UpdateCodeImpl(isolate, code, mode, false);
}

void JSFunction::UpdateCodeImpl(Isolate* isolate, Tagged<Code> value,
                                WriteBarrierMode mode,
                                bool keep_tiering_request) {
  DisallowGarbageCollection no_gc;

  JSDispatchHandle handle = dispatch_handle();
  if (handle == kNullJSDispatchHandle) {
    handle = raw_feedback_cell()->dispatch_handle();
    DCHECK_NE(handle, kNullJSDispatchHandle);
    set_dispatch_handle(handle, mode);
  }
  if (keep_tiering_request) {
    UpdateDispatchEntryKeepTieringRequest(isolate, value, mode);
  } else {
    UpdateDispatchEntry(isolate, value, mode);
  }

  if (V8_UNLIKELY(v8_flags.log_function_events)) {
    isolate->js_dispatch_table().SetTieringRequest(
        dispatch_handle(), TieringBuiltin::kFunctionLogNextExecution, isolate);
  }
}

void JSFunction::UpdateCode(Isolate* isolate, Tagged<Code> code,
                            WriteBarrierMode mode) {
  // Optimized code must go through UpdateOptimized code, which sets a
  // back-reference in the code object to the dispatch handle for
  // deoptimization.
  CHECK(!code->is_optimized_code());
  UpdateCodeImpl(isolate, code, mode, false);
}

inline void JSFunction::UpdateCodeKeepTieringRequests(Isolate* isolate,
                                                      Tagged<Code> code,
                                                      WriteBarrierMode mode) {
  CHECK(!code->is_optimized_code());
  UpdateCodeImpl(isolate, code, mode, true);
}

Tagged<Code> JSFunction::code(IsolateForSandbox isolate) const {
  return Isolate::Current()->js_dispatch_table().GetCode(dispatch_handle());
}

Tagged<Code> JSFunction::code(IsolateForSandbox isolate,
                              AcquireLoadTag tag) const {
  return Isolate::Current()->js_dispatch_table().GetCode(dispatch_handle(tag));
}

Tagged<Union<Smi, Code>> JSFunction::raw_code(IsolateForSandbox isolate) const {
  JSDispatchHandle handle = dispatch_handle();
  if (handle == kNullJSDispatchHandle) return Smi::zero();
  return Isolate::Current()->js_dispatch_table().GetCode(handle);
}

Tagged<Union<Smi, Code>> JSFunction::raw_code(IsolateForSandbox isolate,
                                              AcquireLoadTag tag) const {
  JSDispatchHandle handle = dispatch_handle(tag);
  if (handle == kNullJSDispatchHandle) return Smi::zero();
  return Isolate::Current()->js_dispatch_table().GetCode(handle);
}

// static
JSDispatchHandle JSFunction::AllocateDispatchHandle(
    DirectHandle<JSFunction> function, Isolate* isolate,
    uint16_t parameter_count, DirectHandle<Code> code, WriteBarrierMode mode) {
  DCHECK_EQ(function->raw_feedback_cell()->dispatch_handle(),
            kNullJSDispatchHandle);
  return HeapObject::AllocateAndInstallJSDispatchHandle(
      function, offsetof(JSFunction, dispatch_handle_), isolate,
      parameter_count, code, mode);
}

void JSFunction::clear_dispatch_handle() { dispatch_handle_.Relaxed_Clear(); }
void JSFunction::set_dispatch_handle(JSDispatchHandle handle,
                                     WriteBarrierMode mode) {
  dispatch_handle_.Relaxed_Store(this, handle, mode);
}
void JSFunction::UpdateDispatchEntry(Isolate* isolate, Tagged<Code> new_code,
                                     WriteBarrierMode mode) {
  JSDispatchHandle handle = dispatch_handle();
  isolate->js_dispatch_table().SetCodeNoWriteBarrier(handle, new_code);
  WriteBarrier::ForJSDispatchHandle(this, handle, mode);
}
void JSFunction::UpdateDispatchEntryKeepTieringRequest(Isolate* isolate,
                                                       Tagged<Code> new_code,
                                                       WriteBarrierMode mode) {
  JSDispatchHandle handle = dispatch_handle();
  isolate->js_dispatch_table().SetCodeKeepTieringRequestNoWriteBarrier(
      handle, new_code);
  WriteBarrier::ForJSDispatchHandle(this, handle, mode);
}
JSDispatchHandle JSFunction::dispatch_handle() const {
  return dispatch_handle_.Relaxed_Load();
}

JSDispatchHandle JSFunction::dispatch_handle(AcquireLoadTag) const {
  return dispatch_handle_.Acquire_Load();
}

Tagged<Context> JSFunction::context(AcquireLoadTag) const {
  return Cast<Context>(context_.Acquire_Load());
}
void JSFunction::set_context(Tagged<Context> value, ReleaseStoreTag,
                             WriteBarrierMode mode) {
  context_.Release_Store(this, value, mode);
}
void JSFunction::set_context(Tagged<Context> value, WriteBarrierMode mode) {
  context_.store(this, value, mode);
}

Address JSFunction::instruction_start(IsolateForSandbox isolate) const {
  return code(isolate)->instruction_start();
}

// TODO(ishell): Why relaxed read but release store?
Tagged<SharedFunctionInfo> JSFunction::shared() const {
  return Cast<SharedFunctionInfo>(shared_function_info_.Relaxed_Load());
}

Tagged<SharedFunctionInfo> JSFunction::shared(RelaxedLoadTag) const {
  return Cast<SharedFunctionInfo>(shared_function_info_.Relaxed_Load());
}

void JSFunction::set_shared(Tagged<SharedFunctionInfo> value,
                            WriteBarrierMode mode) {
  // Release semantics to support acquire read in NeedsResetDueToFlushedBytecode
  shared_function_info_.Release_Store(this, value, mode);
}

bool JSFunction::tiering_in_progress() const {
  if (!has_feedback_vector()) return false;
  return feedback_vector()->tiering_in_progress();
}

bool JSFunction::IsTieringRequestedOrInProgress(Isolate* isolate) const {
  if (!has_feedback_vector()) return false;
  return tiering_in_progress() ||
         isolate->js_dispatch_table().IsTieringRequested(dispatch_handle());
}

bool JSFunction::IsLoggingRequested(Isolate* isolate) const {
  return isolate->js_dispatch_table().IsTieringRequested(
      dispatch_handle(), TieringBuiltin::kFunctionLogNextExecution, isolate);
}

bool JSFunction::IsMaglevRequested(Isolate* isolate) const {
  JSDispatchTable& jdt = isolate->js_dispatch_table();
  Address entrypoint = jdt.GetEntrypoint(dispatch_handle());
  const EmbeddedData& embedded_data = EmbeddedData::FromBlob(isolate);
#define CASE(name, ...)                                                       \
  if (entrypoint == embedded_data.InstructionStartOf(Builtin::k##name)) {     \
    DCHECK(jdt.IsTieringRequested(dispatch_handle(), TieringBuiltin::k##name, \
                                  isolate));                                  \
    return TieringBuiltin::k##name !=                                         \
           TieringBuiltin::kFunctionLogNextExecution;                         \
  }
  BUILTIN_LIST_BASE_TIERING_MAGLEV(CASE)
#undef CASE
  return {};
}

bool JSFunction::IsTurbofanRequested(Isolate* isolate) const {
  JSDispatchTable& jdt = isolate->js_dispatch_table();
  Address entrypoint = jdt.GetEntrypoint(dispatch_handle());
  const EmbeddedData& embedded_data = EmbeddedData::FromBlob(isolate);
#define CASE(name, ...)                                                       \
  if (entrypoint == embedded_data.InstructionStartOf(Builtin::k##name)) {     \
    DCHECK(jdt.IsTieringRequested(dispatch_handle(), TieringBuiltin::k##name, \
                                  isolate));                                  \
    return TieringBuiltin::k##name !=                                         \
           TieringBuiltin::kFunctionLogNextExecution;                         \
  }
  BUILTIN_LIST_BASE_TIERING_TURBOFAN(CASE)
#undef CASE
  return {};
}

bool JSFunction::IsOptimizationRequested(Isolate* isolate) const {
  return IsMaglevRequested(isolate) || IsTurbofanRequested(isolate);
}

std::optional<CodeKind> JSFunction::GetRequestedOptimizationIfAny(
    Isolate* isolate, ConcurrencyMode mode) const {
  JSDispatchTable& jdt = isolate->js_dispatch_table();
  Address entrypoint = jdt.GetEntrypoint(dispatch_handle());
  const EmbeddedData& embedded_data = EmbeddedData::FromBlob(isolate);
  auto builtin = ([&]() -> std::optional<TieringBuiltin> {
#define CASE(name, ...)                                                       \
  if (entrypoint == embedded_data.InstructionStartOf(Builtin::k##name)) {     \
    DCHECK(jdt.IsTieringRequested(dispatch_handle(), TieringBuiltin::k##name, \
                                  isolate));                                  \
    return TieringBuiltin::k##name;                                           \
  }
    BUILTIN_LIST_BASE_TIERING(CASE)
#undef CASE
    DCHECK(!jdt.IsTieringRequested(dispatch_handle()));
    return {};
  })();
  if (V8_LIKELY(!builtin)) return {};
  switch (*builtin) {
    case TieringBuiltin::kOptimizeMaglevEager:
      if (mode == ConcurrencyMode::kSynchronous) return CodeKind::MAGLEV;
      break;
    case TieringBuiltin::kStartMaglevOptimizeJob:
      if (mode == ConcurrencyMode::kConcurrent) return CodeKind::MAGLEV;
      break;
    case TieringBuiltin::kOptimizeTurbofanEager:
      if (mode == ConcurrencyMode::kSynchronous) return CodeKind::TURBOFAN_JS;
      break;
    case TieringBuiltin::kStartTurbofanOptimizeJob:
      if (mode == ConcurrencyMode::kConcurrent) return CodeKind::TURBOFAN_JS;
      break;
    case TieringBuiltin::kMarkLazyDeoptimized:
    case TieringBuiltin::kMarkReoptimizeLazyDeoptimized:
    case TieringBuiltin::kFunctionLogNextExecution:
      break;
  }
  return {};
}

void JSFunction::ResetTieringRequests(Isolate* isolate) {
  isolate->js_dispatch_table().ResetTieringRequest(dispatch_handle());
}

void JSFunction::SetTieringInProgress(Isolate* isolate, bool in_progress,
                                      BytecodeOffset osr_offset) {
  if (!has_feedback_vector()) return;
  if (osr_offset.IsNone()) {
    feedback_vector()->set_tiering_in_progress(in_progress);
    SetInterruptBudget(isolate, BudgetModification::kReset);
  } else {
    feedback_vector()->set_osr_tiering_in_progress(in_progress);
  }
}

bool JSFunction::osr_tiering_in_progress() {
  DCHECK(has_feedback_vector());
  return feedback_vector()->osr_tiering_in_progress();
}

bool JSFunction::has_feedback_vector() const {
  return shared()->is_compiled() &&
         IsFeedbackVector(raw_feedback_cell()->value());
}

bool JSFunction::has_closure_feedback_cell_array() const {
  return shared()->is_compiled() &&
         IsClosureFeedbackCellArray(raw_feedback_cell()->value());
}

Tagged<Context> JSFunction::context() { return Cast<Context>(context_.load()); }

Tagged<Context> JSFunction::context(RelaxedLoadTag) const {
  return Cast<Context>(context_.Relaxed_Load());
}

bool JSFunction::has_context() const { return IsContext(context_.load()); }

Tagged<JSGlobalProxy> JSFunction::global_proxy() {
  return context()->global_proxy();
}

Tagged<NativeContext> JSFunction::native_context() {
  return context()->native_context();
}

Tagged<UnionOf<JSReceiver, Map, Tuple2, TheHole>>
JSFunctionWithPrototype::prototype_or_initial_map(AcquireLoadTag) const {
  return Cast<UnionOf<JSReceiver, Map, Tuple2, TheHole>>(
      prototype_or_initial_map_.Acquire_Load());
}
void JSFunctionWithPrototype::set_prototype_or_initial_map(
    Tagged<UnionOf<JSReceiver, Map, Tuple2, TheHole>> value, ReleaseStoreTag,
    WriteBarrierMode mode) {
  prototype_or_initial_map_.Release_Store(this, value, mode);
}

Tagged<UnionOf<JSReceiver, Map, Tuple2, TheHole>>
JSFunction::prototype_or_initial_map(AcquireLoadTag tag) const {
  DCHECK(has_prototype_slot());
  return Cast<JSFunctionWithPrototype>(this)->prototype_or_initial_map(tag);
}
void JSFunction::set_prototype_or_initial_map(
    Tagged<UnionOf<JSReceiver, Map, Tuple2, TheHole>> value,
    ReleaseStoreTag tag, WriteBarrierMode mode) {
  DCHECK(has_prototype_slot());
  Cast<JSFunctionWithPrototype>(this)->set_prototype_or_initial_map(value, tag,
                                                                    mode);
}

bool JSFunction::has_prototype_slot() const {
  // It's slightly cheaper to check for JSFunctionWithoutPrototype because
  // there's only one such instance type.
  return !IsJSFunctionWithoutPrototypeMap(map());
}

// This struct is defined outside of JSFunction class because the V8_OBJECT
// macro enables "-Wpadded" warning (requirement for explicit padding) which
// we don't need for this struct.
struct JSFunction::PrototypeOrInitialMapData {
  Tagged<JSPrototype> instance_prototype;
  Tagged<Tuple2> non_instance_prototype_tuple;
  Tagged<JSAny> non_instance_prototype;
  Tagged<Map> initial_map;
  bool has_instance_prototype = false;
  bool has_non_instance_prototype = false;
  bool has_initial_map = false;
};

bool JSFunction::TryGetPrototypeOrInitialMap(
    PrototypeOrInitialMapData* out) const {
  DCHECK(has_prototype_slot());
  Tagged<HeapObject> proto_or_map = prototype_or_initial_map(kAcquireLoad);
  if (IsTheHole(proto_or_map)) return false;

  out->has_instance_prototype = false;
  out->has_non_instance_prototype = false;
  out->has_initial_map = false;

  if (Tagged<Tuple2> tuple; TryCast<Tuple2>(proto_or_map, &tuple)) {
    out->non_instance_prototype_tuple = tuple;
    out->has_non_instance_prototype = true;
    out->non_instance_prototype = Cast<JSAny>(tuple->value2());
    proto_or_map = Cast<HeapObject>(tuple->value1());
  }
  if (Tagged<Map> initial_map; TryCast<Map>(proto_or_map, &initial_map)) {
    out->instance_prototype = initial_map->prototype();
    out->has_initial_map = true;
    out->initial_map = initial_map;
    // Constructors from https://tc39.es/proposal-structs/ do not have
    // instance prototypes yet. Per-Realm prototypes is currently an open
    // design question and not included in this draft. Thus, once bootstrapper
    // is done, this is the only "legitimate" case of a function without an
    // instance prototype.
    out->has_instance_prototype = !IsNull(out->instance_prototype);
    DCHECK_IMPLIES(!out->has_instance_prototype,
                   Isolate::Current()->bootstrapper()->IsActive() ||
                       v8_flags.harmony_struct);
    return true;
  }
  DCHECK(IsJSReceiver(proto_or_map));
  out->has_instance_prototype = true;
  out->instance_prototype = Cast<JSReceiver>(proto_or_map);
  return true;
}

bool JSFunction::TryGetInitialMap(Tagged<Map>* out_initial_map) const {
  PrototypeOrInitialMapData pomd;
  if (!TryGetPrototypeOrInitialMap(&pomd)) return false;
  if (pomd.has_initial_map) {
    *out_initial_map = pomd.initial_map;
    return true;
  }
  return false;
}

Tagged<Map> JSFunction::initial_map() const {
  DCHECK(has_prototype_slot());
  Tagged<Map> initial_map;
  bool succeeded = TryGetInitialMap(&initial_map);
  DCHECK(succeeded);
  USE(succeeded);
  return initial_map;
}

bool JSFunction::has_initial_map() const {
  DCHECK(has_prototype_slot());
  Tagged<Map> initial_map;
  return TryGetInitialMap(&initial_map);
}

bool JSFunction::has_instance_prototype() const {
  DCHECK(has_prototype_slot());
  PrototypeOrInitialMapData pomd;
  if (!TryGetPrototypeOrInitialMap(&pomd)) return false;
  return pomd.has_instance_prototype;
}

bool JSFunction::has_non_instance_prototype() const {
  DCHECK(has_prototype_slot());
  PrototypeOrInitialMapData pomd;
  if (!TryGetPrototypeOrInitialMap(&pomd)) return false;
  return pomd.has_non_instance_prototype;
}

bool JSFunction::has_prototype() const {
  DCHECK(has_prototype_slot());
  PrototypeOrInitialMapData pomd;
  return TryGetPrototypeOrInitialMap(&pomd);
}

bool JSFunction::has_prototype_property() const {
  return (has_prototype_slot() && map()->is_constructor()) ||
         IsGeneratorFunction(shared()->kind());
}

bool JSFunction::PrototypeRequiresRuntimeLookup() const {
  if (!has_prototype_property()) return true;
  Tagged<Object> proto_or_map = prototype_or_initial_map(kAcquireLoad);
  return IsTuple2(proto_or_map);
}

Tagged<JSReceiver> JSFunction::GetIntrinsicDefaultProto() const {
  FunctionKind kind = shared()->kind();
  Tagged<NativeContext> native_context =
      context(kAcquireLoad)->native_context();
  return IsGeneratorFunction(kind)
             ? IsAsyncFunction(kind)
                   ? native_context->initial_async_generator_prototype()
                   : native_context->initial_generator_prototype()
             : native_context->initial_object_prototype();
}

Tagged<JSReceiver> JSFunction::instance_prototype() const {
  DCHECK(has_instance_prototype());
  PrototypeOrInitialMapData pomd;
  bool succeeded = TryGetPrototypeOrInitialMap(&pomd);
  DCHECK(succeeded);
  USE(succeeded);
  // By the time of this query, the instance prototype must already be fully
  // set (initial map's prototype might be temporarily set to Null during
  // bootstrapping and creation of builtins prototypes).
  DCHECK(IsJSReceiver(pomd.instance_prototype));
  return Cast<JSReceiver>(pomd.instance_prototype);
}

Tagged<JSAny> JSFunction::prototype() const {
  DCHECK(has_prototype());
  PrototypeOrInitialMapData pomd;
  bool succeeded = TryGetPrototypeOrInitialMap(&pomd);
  DCHECK(succeeded);
  USE(succeeded);
  if (pomd.has_non_instance_prototype) {
    return pomd.non_instance_prototype;
  }
  return pomd.instance_prototype;
}

bool JSFunction::is_compiled(IsolateForSandbox isolate) const {
  return code(isolate, kAcquireLoad)->builtin_id() != Builtin::kCompileLazy &&
         shared()->is_compiled();
}

bool JSFunction::NeedsResetDueToFlushedBytecode(Isolate* isolate) {
  // The function is only used sequentially. Concurrent cases need to take care
  // of loading the fields themselves.
  Tagged<SharedFunctionInfo> sfi = TrustedCast<SharedFunctionInfo>(shared());
  Tagged<Code> code = TrustedCast<Code>(raw_code(isolate));
  return NeedsResetDueToFlushedBytecode(isolate, sfi, code);
}

bool JSFunction::NeedsResetDueToFlushedBytecode(Isolate* isolate,
                                                Tagged<SharedFunctionInfo> sfi,
                                                Tagged<Code> code) {
  return !sfi->is_compiled() &&
         (code->builtin_id() != Builtin::kCompileLazy ||
          // With leaptiering we can have CompileLazy as the code object but
          // still an optimization trampoline installed.
          IsOptimizationRequested(isolate));
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
    ResetTieringRequests(isolate);
    UpdateCode(isolate, *BUILTIN_CODE(isolate, CompileLazy),
               SKIP_WRITE_BARRIER);
    raw_feedback_cell()->reset_feedback_vector(gc_notify_updated_slot);
    return;
  }

  DCHECK_IMPLIES(NeedsResetDueToFlushedBaselineCode(isolate),
                 kBaselineCodeCanFlush);
  if (kBaselineCodeCanFlush && NeedsResetDueToFlushedBaselineCode(isolate)) {
    // Flush baseline code from the closure if required
    ResetTieringRequests(isolate);
    UpdateCode(isolate, *BUILTIN_CODE(isolate, InterpreterEntryTrampoline),
               SKIP_WRITE_BARRIER);
  }
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_FUNCTION_INL_H_
