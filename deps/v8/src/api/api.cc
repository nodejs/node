// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api.h"

#include <algorithm>  // For min
#include <cmath>      // For isnan.
#include <limits>
#include <string>
#include <utility>  // For move
#include <vector>

#include "src/api/api-inl.h"

#include "include/v8-profiler.h"
#include "include/v8-util.h"
#include "src/api/api-natives.h"
#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/safe_conversions.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/accessors.h"
#include "src/builtins/builtins-utils.h"
#include "src/codegen/compiler.h"
#include "src/codegen/cpu-features.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/date/date.h"
#include "src/debug/debug-coverage.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/debug-type-profile.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/gdb-jit.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/runtime-profiler.h"
#include "src/execution/simulator.h"
#include "src/execution/v8threads.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/global-handles.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/heap-inl.h"
#include "src/init/bootstrapper.h"
#include "src/init/icu_util.h"
#include "src/init/startup-data-util.h"
#include "src/init/v8.h"
#include "src/json/json-parser.h"
#include "src/json/json-stringifier.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/contexts.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/embedder-data-slot-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/ordered-hash-table-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/property-details.h"
#include "src/objects/property.h"
#include "src/objects/prototype.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/templates.h"
#include "src/objects/value-serializer.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/pending-compilation-error-handler.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/profiler/cpu-profiler.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator-inl.h"
#include "src/profiler/profile-generator-inl.h"
#include "src/profiler/tick-sample.h"
#include "src/regexp/regexp-utils.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/partial-serializer.h"
#include "src/snapshot/read-only-serializer.h"
#include "src/snapshot/snapshot.h"
#include "src/snapshot/startup-serializer.h"
#include "src/strings/char-predicates-inl.h"
#include "src/strings/string-hasher.h"
#include "src/strings/unicode-inl.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/detachable-vector.h"
#include "src/utils/version.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-serialization.h"

#if V8_OS_LINUX || V8_OS_MACOSX || V8_OS_FREEBSD
#include <signal.h>
#include "include/v8-wasm-trap-handler-posix.h"
#include "src/trap-handler/handler-inside-posix.h"
#endif

#if V8_OS_WIN
#include <versionhelpers.h>
#include <windows.h>
#include "include/v8-wasm-trap-handler-win.h"
#include "src/trap-handler/handler-inside-win.h"
#if defined(V8_OS_WIN64)
#include "src/diagnostics/unwinding-info-win64.h"
#endif  // V8_OS_WIN64
#endif  // V8_OS_WIN

#define TRACE_BS(...)                                     \
  do {                                                    \
    if (i::FLAG_trace_backing_store) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {

/*
 * Most API methods should use one of the three macros:
 *
 * ENTER_V8, ENTER_V8_NO_SCRIPT, ENTER_V8_NO_SCRIPT_NO_EXCEPTION.
 *
 * The latter two assume that no script is executed, and no exceptions are
 * scheduled in addition (respectively). Creating a pending exception and
 * removing it before returning is ok.
 *
 * Exceptions should be handled either by invoking one of the
 * RETURN_ON_FAILED_EXECUTION* macros.
 *
 * Don't use macros with DO_NOT_USE in their name.
 *
 * TODO(jochen): Document debugger specific macros.
 * TODO(jochen): Document LOG_API and other RuntimeCallStats macros.
 * TODO(jochen): All API methods should invoke one of the ENTER_V8* macros.
 * TODO(jochen): Remove calls form API methods to DO_NOT_USE macros.
 */

#define LOG_API(isolate, class_name, function_name)                           \
  i::RuntimeCallTimerScope _runtime_timer(                                    \
      isolate, i::RuntimeCallCounterId::kAPI_##class_name##_##function_name); \
  LOG(isolate, ApiEntryCall("v8::" #class_name "::" #function_name))

#define ENTER_V8_DO_NOT_USE(isolate) i::VMState<v8::OTHER> __state__((isolate))

#define ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name,  \
                                   function_name, bailout_value,  \
                                   HandleScopeClass, do_callback) \
  if (IsExecutionTerminatingCheck(isolate)) {                     \
    return bailout_value;                                         \
  }                                                               \
  HandleScopeClass handle_scope(isolate);                         \
  CallDepthScope<do_callback> call_depth_scope(isolate, context); \
  LOG_API(isolate, class_name, function_name);                    \
  i::VMState<v8::OTHER> __state__((isolate));                     \
  bool has_pending_exception = false

#define PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE(isolate, T)       \
  if (IsExecutionTerminatingCheck(isolate)) {                                \
    return MaybeLocal<T>();                                                  \
  }                                                                          \
  InternalEscapableScope handle_scope(isolate);                              \
  CallDepthScope<false> call_depth_scope(isolate, v8::Local<v8::Context>()); \
  i::VMState<v8::OTHER> __state__((isolate));                                \
  bool has_pending_exception = false

#define PREPARE_FOR_EXECUTION_WITH_CONTEXT(context, class_name, function_name, \
                                           bailout_value, HandleScopeClass,    \
                                           do_callback)                        \
  auto isolate = context.IsEmpty()                                             \
                     ? i::Isolate::Current()                                   \
                     : reinterpret_cast<i::Isolate*>(context->GetIsolate());   \
  ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name, function_name,      \
                             bailout_value, HandleScopeClass, do_callback);

#define PREPARE_FOR_EXECUTION(context, class_name, function_name, T)          \
  PREPARE_FOR_EXECUTION_WITH_CONTEXT(context, class_name, function_name,      \
                                     MaybeLocal<T>(), InternalEscapableScope, \
                                     false)

#define ENTER_V8(isolate, context, class_name, function_name, bailout_value, \
                 HandleScopeClass)                                           \
  ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name, function_name,    \
                             bailout_value, HandleScopeClass, true)

#ifdef DEBUG
#define ENTER_V8_NO_SCRIPT(isolate, context, class_name, function_name,   \
                           bailout_value, HandleScopeClass)               \
  ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name, function_name, \
                             bailout_value, HandleScopeClass, false);     \
  i::DisallowJavascriptExecutionDebugOnly __no_script__((isolate))

#define ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate)                    \
  i::VMState<v8::OTHER> __state__((isolate));                       \
  i::DisallowJavascriptExecutionDebugOnly __no_script__((isolate)); \
  i::DisallowExceptions __no_exceptions__((isolate))

#define ENTER_V8_FOR_NEW_CONTEXT(isolate)     \
  i::VMState<v8::OTHER> __state__((isolate)); \
  i::DisallowExceptions __no_exceptions__((isolate))
#else
#define ENTER_V8_NO_SCRIPT(isolate, context, class_name, function_name,   \
                           bailout_value, HandleScopeClass)               \
  ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name, function_name, \
                             bailout_value, HandleScopeClass, false)

#define ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate) \
  i::VMState<v8::OTHER> __state__((isolate));

#define ENTER_V8_FOR_NEW_CONTEXT(isolate) \
  i::VMState<v8::OTHER> __state__((isolate));
#endif  // DEBUG

#define EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(isolate, value) \
  do {                                                            \
    if (has_pending_exception) {                                  \
      call_depth_scope.Escape();                                  \
      return value;                                               \
    }                                                             \
  } while (false)

#define RETURN_ON_FAILED_EXECUTION(T) \
  EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(isolate, MaybeLocal<T>())

#define RETURN_ON_FAILED_EXECUTION_PRIMITIVE(T) \
  EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(isolate, Nothing<T>())

#define RETURN_ESCAPED(value) return handle_scope.Escape(value);

namespace {

class InternalEscapableScope : public v8::EscapableHandleScope {
 public:
  explicit inline InternalEscapableScope(i::Isolate* isolate)
      : v8::EscapableHandleScope(reinterpret_cast<v8::Isolate*>(isolate)) {}
};

// TODO(jochen): This should be #ifdef DEBUG
#ifdef V8_CHECK_MICROTASKS_SCOPES_CONSISTENCY
void CheckMicrotasksScopesConsistency(i::MicrotaskQueue* microtask_queue) {
  if (microtask_queue &&
      microtask_queue->microtasks_policy() == v8::MicrotasksPolicy::kScoped) {
    DCHECK(microtask_queue->GetMicrotasksScopeDepth() ||
           !microtask_queue->DebugMicrotasksScopeDepthIsZero());
  }
}
#endif

template <bool do_callback>
class CallDepthScope {
 public:
  CallDepthScope(i::Isolate* isolate, Local<Context> context)
      : isolate_(isolate),
        context_(context),
        escaped_(false),
        safe_for_termination_(isolate->next_v8_call_is_safe_for_termination()),
        interrupts_scope_(isolate_, i::StackGuard::TERMINATE_EXECUTION,
                          isolate_->only_terminate_in_safe_scope()
                              ? (safe_for_termination_
                                     ? i::InterruptsScope::kRunInterrupts
                                     : i::InterruptsScope::kPostponeInterrupts)
                              : i::InterruptsScope::kNoop) {
    isolate_->thread_local_top()->IncrementCallDepth(this);
    isolate_->set_next_v8_call_is_safe_for_termination(false);
    if (!context.IsEmpty()) {
      i::Handle<i::Context> env = Utils::OpenHandle(*context);
      i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
      if (!isolate->context().is_null() &&
          isolate->context().native_context() == env->native_context()) {
        context_ = Local<Context>();
      } else {
        impl->SaveContext(isolate->context());
        isolate->set_context(*env);
      }
    }
    if (do_callback) isolate_->FireBeforeCallEnteredCallback();
  }
  ~CallDepthScope() {
    i::MicrotaskQueue* microtask_queue = isolate_->default_microtask_queue();
    if (!context_.IsEmpty()) {
      i::HandleScopeImplementer* impl = isolate_->handle_scope_implementer();
      isolate_->set_context(impl->RestoreContext());

      i::Handle<i::Context> env = Utils::OpenHandle(*context_);
      microtask_queue = env->native_context().microtask_queue();
    }
    if (!escaped_) isolate_->thread_local_top()->DecrementCallDepth(this);
    if (do_callback) isolate_->FireCallCompletedCallback(microtask_queue);
// TODO(jochen): This should be #ifdef DEBUG
#ifdef V8_CHECK_MICROTASKS_SCOPES_CONSISTENCY
    if (do_callback) CheckMicrotasksScopesConsistency(microtask_queue);
#endif
    DCHECK(CheckKeptObjectsClearedAfterMicrotaskCheckpoint(microtask_queue));
    isolate_->set_next_v8_call_is_safe_for_termination(safe_for_termination_);
  }

  void Escape() {
    DCHECK(!escaped_);
    escaped_ = true;
    auto thread_local_top = isolate_->thread_local_top();
    thread_local_top->DecrementCallDepth(this);
    bool clear_exception = thread_local_top->CallDepthIsZero() &&
                           thread_local_top->try_catch_handler_ == nullptr;
    isolate_->OptionalRescheduleException(clear_exception);
  }

 private:
  bool CheckKeptObjectsClearedAfterMicrotaskCheckpoint(
      i::MicrotaskQueue* microtask_queue) {
    bool did_perform_microtask_checkpoint =
        do_callback && microtask_queue &&
        microtask_queue->microtasks_policy() == MicrotasksPolicy::kAuto;
    return !did_perform_microtask_checkpoint ||
           isolate_->heap()->weak_refs_keep_during_job().IsUndefined(isolate_);
  }

  i::Isolate* const isolate_;
  Local<Context> context_;
  bool escaped_;
  bool do_callback_;
  bool safe_for_termination_;
  i::InterruptsScope interrupts_scope_;
  i::Address previous_stack_height_;

  friend class i::ThreadLocalTop;

  DISALLOW_NEW_AND_DELETE()
  DISALLOW_COPY_AND_ASSIGN(CallDepthScope);
};

}  // namespace

static ScriptOrigin GetScriptOriginForScript(i::Isolate* isolate,
                                             i::Handle<i::Script> script) {
  i::Handle<i::Object> scriptName(script->GetNameOrSourceURL(), isolate);
  i::Handle<i::Object> source_map_url(script->source_mapping_url(), isolate);
  i::Handle<i::FixedArray> host_defined_options(script->host_defined_options(),
                                                isolate);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  ScriptOriginOptions options(script->origin_options());
  v8::ScriptOrigin origin(
      Utils::ToLocal(scriptName),
      v8::Integer::New(v8_isolate, script->line_offset()),
      v8::Integer::New(v8_isolate, script->column_offset()),
      v8::Boolean::New(v8_isolate, options.IsSharedCrossOrigin()),
      v8::Integer::New(v8_isolate, script->id()),
      Utils::ToLocal(source_map_url),
      v8::Boolean::New(v8_isolate, options.IsOpaque()),
      v8::Boolean::New(v8_isolate, script->type() == i::Script::TYPE_WASM),
      v8::Boolean::New(v8_isolate, options.IsModule()),
      Utils::ToLocal(host_defined_options));
  return origin;
}

// --- E x c e p t i o n   B e h a v i o r ---

void i::FatalProcessOutOfMemory(i::Isolate* isolate, const char* location) {
  i::V8::FatalProcessOutOfMemory(isolate, location, false);
}

// When V8 cannot allocate memory FatalProcessOutOfMemory is called. The default
// OOM error handler is called and execution is stopped.
void i::V8::FatalProcessOutOfMemory(i::Isolate* isolate, const char* location,
                                    bool is_heap_oom) {
  char last_few_messages[Heap::kTraceRingBufferSize + 1];
  char js_stacktrace[Heap::kStacktraceBufferSize + 1];
  i::HeapStats heap_stats;

  if (isolate == nullptr) {
    isolate = Isolate::TryGetCurrent();
  }

  if (isolate == nullptr) {
    // If the Isolate is not available for the current thread we cannot retrieve
    // memory information from the Isolate. Write easy-to-recognize values on
    // the stack.
    memset(last_few_messages, 0x0BADC0DE, Heap::kTraceRingBufferSize + 1);
    memset(js_stacktrace, 0x0BADC0DE, Heap::kStacktraceBufferSize + 1);
    memset(&heap_stats, 0xBADC0DE, sizeof(heap_stats));
    // Note that the embedder's oom handler is also not available and therefore
    // won't be called in this case. We just crash.
    FATAL("Fatal process out of memory: %s", location);
    UNREACHABLE();
  }

  memset(last_few_messages, 0, Heap::kTraceRingBufferSize + 1);
  memset(js_stacktrace, 0, Heap::kStacktraceBufferSize + 1);

  intptr_t start_marker;
  heap_stats.start_marker = &start_marker;
  size_t ro_space_size;
  heap_stats.ro_space_size = &ro_space_size;
  size_t ro_space_capacity;
  heap_stats.ro_space_capacity = &ro_space_capacity;
  size_t new_space_size;
  heap_stats.new_space_size = &new_space_size;
  size_t new_space_capacity;
  heap_stats.new_space_capacity = &new_space_capacity;
  size_t old_space_size;
  heap_stats.old_space_size = &old_space_size;
  size_t old_space_capacity;
  heap_stats.old_space_capacity = &old_space_capacity;
  size_t code_space_size;
  heap_stats.code_space_size = &code_space_size;
  size_t code_space_capacity;
  heap_stats.code_space_capacity = &code_space_capacity;
  size_t map_space_size;
  heap_stats.map_space_size = &map_space_size;
  size_t map_space_capacity;
  heap_stats.map_space_capacity = &map_space_capacity;
  size_t lo_space_size;
  heap_stats.lo_space_size = &lo_space_size;
  size_t code_lo_space_size;
  heap_stats.code_lo_space_size = &code_lo_space_size;
  size_t global_handle_count;
  heap_stats.global_handle_count = &global_handle_count;
  size_t weak_global_handle_count;
  heap_stats.weak_global_handle_count = &weak_global_handle_count;
  size_t pending_global_handle_count;
  heap_stats.pending_global_handle_count = &pending_global_handle_count;
  size_t near_death_global_handle_count;
  heap_stats.near_death_global_handle_count = &near_death_global_handle_count;
  size_t free_global_handle_count;
  heap_stats.free_global_handle_count = &free_global_handle_count;
  size_t memory_allocator_size;
  heap_stats.memory_allocator_size = &memory_allocator_size;
  size_t memory_allocator_capacity;
  heap_stats.memory_allocator_capacity = &memory_allocator_capacity;
  size_t malloced_memory;
  heap_stats.malloced_memory = &malloced_memory;
  size_t malloced_peak_memory;
  heap_stats.malloced_peak_memory = &malloced_peak_memory;
  size_t objects_per_type[LAST_TYPE + 1] = {0};
  heap_stats.objects_per_type = objects_per_type;
  size_t size_per_type[LAST_TYPE + 1] = {0};
  heap_stats.size_per_type = size_per_type;
  int os_error;
  heap_stats.os_error = &os_error;
  heap_stats.last_few_messages = last_few_messages;
  heap_stats.js_stacktrace = js_stacktrace;
  intptr_t end_marker;
  heap_stats.end_marker = &end_marker;
  if (isolate->heap()->HasBeenSetUp()) {
    // BUG(1718): Don't use the take_snapshot since we don't support
    // HeapObjectIterator here without doing a special GC.
    isolate->heap()->RecordStats(&heap_stats, false);
    char* first_newline = strchr(last_few_messages, '\n');
    if (first_newline == nullptr || first_newline[1] == '\0')
      first_newline = last_few_messages;
    PrintF("\n<--- Last few GCs --->\n%s\n", first_newline);
    PrintF("\n<--- JS stacktrace --->\n%s\n", js_stacktrace);
  }
  Utils::ReportOOMFailure(isolate, location, is_heap_oom);
  // If the fatal error handler returns, we stop execution.
  FATAL("API fatal error handler returned after process out of memory");
}

void Utils::ReportApiFailure(const char* location, const char* message) {
  i::Isolate* isolate = i::Isolate::TryGetCurrent();
  FatalErrorCallback callback = nullptr;
  if (isolate != nullptr) {
    callback = isolate->exception_behavior();
  }
  if (callback == nullptr) {
    base::OS::PrintError("\n#\n# Fatal error in %s\n# %s\n#\n\n", location,
                         message);
    base::OS::Abort();
  } else {
    callback(location, message);
  }
  isolate->SignalFatalError();
}

void Utils::ReportOOMFailure(i::Isolate* isolate, const char* location,
                             bool is_heap_oom) {
  OOMErrorCallback oom_callback = isolate->oom_behavior();
  if (oom_callback == nullptr) {
    // TODO(wfh): Remove this fallback once Blink is setting OOM handler. See
    // crbug.com/614440.
    FatalErrorCallback fatal_callback = isolate->exception_behavior();
    if (fatal_callback == nullptr) {
      base::OS::PrintError("\n#\n# Fatal %s OOM in %s\n#\n\n",
                           is_heap_oom ? "javascript" : "process", location);
      base::OS::Abort();
    } else {
      fatal_callback(location,
                     is_heap_oom
                         ? "Allocation failed - JavaScript heap out of memory"
                         : "Allocation failed - process out of memory");
    }
  } else {
    oom_callback(location, is_heap_oom);
  }
  isolate->SignalFatalError();
}

static inline bool IsExecutionTerminatingCheck(i::Isolate* isolate) {
  if (isolate->has_scheduled_exception()) {
    return isolate->scheduled_exception() ==
           i::ReadOnlyRoots(isolate).termination_exception();
  }
  return false;
}

void V8::SetSnapshotDataBlob(StartupData* snapshot_blob) {
  i::V8::SetSnapshotBlob(snapshot_blob);
}

namespace {

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override {
#if V8_OS_AIX && _LINUX_SOURCE_COMPAT
    // Work around for GCC bug on AIX
    // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79839
    void* data = __linux_calloc(length, 1);
#else
    void* data = calloc(length, 1);
#endif
    return data;
  }

  void* AllocateUninitialized(size_t length) override {
#if V8_OS_AIX && _LINUX_SOURCE_COMPAT
    // Work around for GCC bug on AIX
    // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79839
    void* data = __linux_malloc(length);
#else
    void* data = malloc(length);
#endif
    return data;
  }

  void Free(void* data, size_t) override { free(data); }

  void* Reallocate(void* data, size_t old_length, size_t new_length) override {
#if V8_OS_AIX && _LINUX_SOURCE_COMPAT
    // Work around for GCC bug on AIX
    // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79839
    void* new_data = __linux_realloc(data, new_length);
#else
    void* new_data = realloc(data, new_length);
#endif
    if (new_length > old_length) {
      memset(reinterpret_cast<uint8_t*>(new_data) + old_length, 0,
             new_length - old_length);
    }
    return new_data;
  }
};

struct SnapshotCreatorData {
  explicit SnapshotCreatorData(Isolate* isolate)
      : isolate_(isolate),
        default_context_(),
        contexts_(isolate),
        created_(false) {}

  static SnapshotCreatorData* cast(void* data) {
    return reinterpret_cast<SnapshotCreatorData*>(data);
  }

  ArrayBufferAllocator allocator_;
  Isolate* isolate_;
  Persistent<Context> default_context_;
  SerializeInternalFieldsCallback default_embedder_fields_serializer_;
  PersistentValueVector<Context> contexts_;
  std::vector<SerializeInternalFieldsCallback> embedder_fields_serializers_;
  bool created_;
};

}  // namespace

SnapshotCreator::SnapshotCreator(Isolate* isolate,
                                 const intptr_t* external_references,
                                 StartupData* existing_snapshot) {
  SnapshotCreatorData* data = new SnapshotCreatorData(isolate);
  data->isolate_ = isolate;
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal_isolate->set_array_buffer_allocator(&data->allocator_);
  internal_isolate->set_api_external_references(external_references);
  internal_isolate->enable_serializer();
  isolate->Enter();
  const StartupData* blob = existing_snapshot
                                ? existing_snapshot
                                : i::Snapshot::DefaultSnapshotBlob();
  if (blob && blob->raw_size > 0) {
    internal_isolate->set_snapshot_blob(blob);
    i::Snapshot::Initialize(internal_isolate);
  } else {
    internal_isolate->InitWithoutSnapshot();
  }
  data_ = data;
}

SnapshotCreator::SnapshotCreator(const intptr_t* external_references,
                                 StartupData* existing_snapshot)
    : SnapshotCreator(Isolate::Allocate(), external_references,
                      existing_snapshot) {}

SnapshotCreator::~SnapshotCreator() {
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  DCHECK(data->created_);
  Isolate* isolate = data->isolate_;
  isolate->Exit();
  isolate->Dispose();
  delete data;
}

Isolate* SnapshotCreator::GetIsolate() {
  return SnapshotCreatorData::cast(data_)->isolate_;
}

void SnapshotCreator::SetDefaultContext(
    Local<Context> context, SerializeInternalFieldsCallback callback) {
  DCHECK(!context.IsEmpty());
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  DCHECK(!data->created_);
  DCHECK(data->default_context_.IsEmpty());
  Isolate* isolate = data->isolate_;
  CHECK_EQ(isolate, context->GetIsolate());
  data->default_context_.Reset(isolate, context);
  data->default_embedder_fields_serializer_ = callback;
}

size_t SnapshotCreator::AddContext(Local<Context> context,
                                   SerializeInternalFieldsCallback callback) {
  DCHECK(!context.IsEmpty());
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  DCHECK(!data->created_);
  Isolate* isolate = data->isolate_;
  CHECK_EQ(isolate, context->GetIsolate());
  size_t index = data->contexts_.Size();
  data->contexts_.Append(context);
  data->embedder_fields_serializers_.push_back(callback);
  return index;
}

size_t SnapshotCreator::AddData(i::Address object) {
  DCHECK_NE(object, i::kNullAddress);
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  DCHECK(!data->created_);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(data->isolate_);
  i::HandleScope scope(isolate);
  i::Handle<i::Object> obj(i::Object(object), isolate);
  i::Handle<i::ArrayList> list;
  if (!isolate->heap()->serialized_objects().IsArrayList()) {
    list = i::ArrayList::New(isolate, 1);
  } else {
    list = i::Handle<i::ArrayList>(
        i::ArrayList::cast(isolate->heap()->serialized_objects()), isolate);
  }
  size_t index = static_cast<size_t>(list->Length());
  list = i::ArrayList::Add(isolate, list, obj);
  isolate->heap()->SetSerializedObjects(*list);
  return index;
}

size_t SnapshotCreator::AddData(Local<Context> context, i::Address object) {
  DCHECK_NE(object, i::kNullAddress);
  DCHECK(!SnapshotCreatorData::cast(data_)->created_);
  i::Handle<i::Context> ctx = Utils::OpenHandle(*context);
  i::Isolate* isolate = ctx->GetIsolate();
  i::HandleScope scope(isolate);
  i::Handle<i::Object> obj(i::Object(object), isolate);
  i::Handle<i::ArrayList> list;
  if (!ctx->serialized_objects().IsArrayList()) {
    list = i::ArrayList::New(isolate, 1);
  } else {
    list = i::Handle<i::ArrayList>(
        i::ArrayList::cast(ctx->serialized_objects()), isolate);
  }
  size_t index = static_cast<size_t>(list->Length());
  list = i::ArrayList::Add(isolate, list, obj);
  ctx->set_serialized_objects(*list);
  return index;
}

namespace {
void ConvertSerializedObjectsToFixedArray(Local<Context> context) {
  i::Handle<i::Context> ctx = Utils::OpenHandle(*context);
  i::Isolate* isolate = ctx->GetIsolate();
  if (!ctx->serialized_objects().IsArrayList()) {
    ctx->set_serialized_objects(i::ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    i::Handle<i::ArrayList> list(i::ArrayList::cast(ctx->serialized_objects()),
                                 isolate);
    i::Handle<i::FixedArray> elements = i::ArrayList::Elements(isolate, list);
    ctx->set_serialized_objects(*elements);
  }
}

void ConvertSerializedObjectsToFixedArray(i::Isolate* isolate) {
  if (!isolate->heap()->serialized_objects().IsArrayList()) {
    isolate->heap()->SetSerializedObjects(
        i::ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    i::Handle<i::ArrayList> list(
        i::ArrayList::cast(isolate->heap()->serialized_objects()), isolate);
    i::Handle<i::FixedArray> elements = i::ArrayList::Elements(isolate, list);
    isolate->heap()->SetSerializedObjects(*elements);
  }
}
}  // anonymous namespace

StartupData SnapshotCreator::CreateBlob(
    SnapshotCreator::FunctionCodeHandling function_code_handling) {
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(data->isolate_);
  DCHECK(!data->created_);
  DCHECK(!data->default_context_.IsEmpty());

  int num_additional_contexts = static_cast<int>(data->contexts_.Size());

  {
    i::HandleScope scope(isolate);
    // Convert list of context-independent data to FixedArray.
    ConvertSerializedObjectsToFixedArray(isolate);

    // Convert lists of context-dependent data to FixedArray.
    ConvertSerializedObjectsToFixedArray(
        data->default_context_.Get(data->isolate_));
    for (int i = 0; i < num_additional_contexts; i++) {
      ConvertSerializedObjectsToFixedArray(data->contexts_.Get(i));
    }

    // We need to store the global proxy size upfront in case we need the
    // bootstrapper to create a global proxy before we deserialize the context.
    i::Handle<i::FixedArray> global_proxy_sizes =
        isolate->factory()->NewFixedArray(num_additional_contexts,
                                          i::AllocationType::kOld);
    for (int i = 0; i < num_additional_contexts; i++) {
      i::Handle<i::Context> context =
          v8::Utils::OpenHandle(*data->contexts_.Get(i));
      global_proxy_sizes->set(i,
                              i::Smi::FromInt(context->global_proxy().Size()));
    }
    isolate->heap()->SetSerializedGlobalProxySizes(*global_proxy_sizes);
  }

  // We might rehash strings and re-sort descriptors. Clear the lookup cache.
  isolate->descriptor_lookup_cache()->Clear();

  // If we don't do this then we end up with a stray root pointing at the
  // context even after we have disposed of the context.
  isolate->heap()->CollectAllAvailableGarbage(
      i::GarbageCollectionReason::kSnapshotCreator);
  {
    i::HandleScope scope(isolate);
    isolate->heap()->CompactWeakArrayLists(internal::AllocationType::kOld);
  }

  if (function_code_handling == FunctionCodeHandling::kClear) {
    // Clear out re-compilable data from all shared function infos. Any
    // JSFunctions using these SFIs will have their code pointers reset by the
    // partial serializer.
    //
    // We have to iterate the heap and collect handles to each clearable SFI,
    // before we disable allocation, since we have to allocate UncompiledDatas
    // to be able to recompile them.
    //
    // Compiled irregexp code is also flushed by collecting and clearing any
    // seen JSRegExp objects.
    i::HandleScope scope(isolate);
    std::vector<i::Handle<i::SharedFunctionInfo>> sfis_to_clear;

    {  // Heap allocation is disallowed within this scope.
      i::HeapObjectIterator heap_iterator(isolate->heap());
      for (i::HeapObject current_obj = heap_iterator.Next();
           !current_obj.is_null(); current_obj = heap_iterator.Next()) {
        if (current_obj.IsSharedFunctionInfo()) {
          i::SharedFunctionInfo shared =
              i::SharedFunctionInfo::cast(current_obj);
          if (shared.CanDiscardCompiled()) {
            sfis_to_clear.emplace_back(shared, isolate);
          }
        } else if (current_obj.IsJSRegExp()) {
          i::JSRegExp regexp = i::JSRegExp::cast(current_obj);
          if (regexp.HasCompiledCode()) {
            regexp.DiscardCompiledCodeForSerialization();
          }
        }
      }
    }

    // Must happen after heap iteration since SFI::DiscardCompiled may allocate.
    for (i::Handle<i::SharedFunctionInfo> shared : sfis_to_clear) {
      i::SharedFunctionInfo::DiscardCompiled(isolate, shared);
    }
  }

  i::DisallowHeapAllocation no_gc_from_here_on;

  int num_contexts = num_additional_contexts + 1;
  std::vector<i::Context> contexts;
  contexts.reserve(num_contexts);
  {
    i::HandleScope scope(isolate);
    contexts.push_back(
        *v8::Utils::OpenHandle(*data->default_context_.Get(data->isolate_)));
    data->default_context_.Reset();
    for (int i = 0; i < num_additional_contexts; i++) {
      i::Handle<i::Context> context =
          v8::Utils::OpenHandle(*data->contexts_.Get(i));
      contexts.push_back(*context);
    }
    data->contexts_.Clear();
  }

  // Check that values referenced by global/eternal handles are accounted for.
  i::SerializedHandleChecker handle_checker(isolate, &contexts);
  CHECK(handle_checker.CheckGlobalAndEternalHandles());

  i::HeapObjectIterator heap_iterator(isolate->heap());
  for (i::HeapObject current_obj = heap_iterator.Next(); !current_obj.is_null();
       current_obj = heap_iterator.Next()) {
    if (current_obj.IsJSFunction()) {
      i::JSFunction fun = i::JSFunction::cast(current_obj);

      // Complete in-object slack tracking for all functions.
      fun.CompleteInobjectSlackTrackingIfActive();

      // Also, clear out feedback vectors, or any optimized code.
      // Note that checking for fun.IsOptimized() || fun.IsInterpreted() is not
      // sufficient because the function can have a feedback vector even if it
      // is not compiled (e.g. when the bytecode was flushed). On the other
      // hand, only checking for the feedback vector is not sufficient because
      // there can be multiple functions sharing the same feedback vector. So we
      // need all these checks.
      if (fun.IsOptimized() || fun.IsInterpreted() ||
          !fun.raw_feedback_cell().value().IsUndefined()) {
        fun.raw_feedback_cell().set_value(
            i::ReadOnlyRoots(isolate).undefined_value());
        fun.set_code(isolate->builtins()->builtin(i::Builtins::kCompileLazy));
      }
      if (function_code_handling == FunctionCodeHandling::kClear) {
        DCHECK(fun.shared().HasWasmExportedFunctionData() ||
               fun.shared().HasBuiltinId() || fun.shared().IsApiFunction() ||
               fun.shared().HasUncompiledDataWithoutPreparseData());
      }
    }
  }

  i::ReadOnlySerializer read_only_serializer(isolate);
  read_only_serializer.SerializeReadOnlyRoots();

  i::StartupSerializer startup_serializer(isolate, &read_only_serializer);
  startup_serializer.SerializeStrongReferences();

  // Serialize each context with a new partial serializer.
  std::vector<i::SnapshotData*> context_snapshots;
  context_snapshots.reserve(num_contexts);

  // TODO(6593): generalize rehashing, and remove this flag.
  bool can_be_rehashed = true;

  for (int i = 0; i < num_contexts; i++) {
    bool is_default_context = i == 0;
    i::PartialSerializer partial_serializer(
        isolate, &startup_serializer,
        is_default_context ? data->default_embedder_fields_serializer_
                           : data->embedder_fields_serializers_[i - 1]);
    partial_serializer.Serialize(&contexts[i], !is_default_context);
    can_be_rehashed = can_be_rehashed && partial_serializer.can_be_rehashed();
    context_snapshots.push_back(new i::SnapshotData(&partial_serializer));
  }

  startup_serializer.SerializeWeakReferencesAndDeferred();
  can_be_rehashed = can_be_rehashed && startup_serializer.can_be_rehashed();

  read_only_serializer.FinalizeSerialization();
  can_be_rehashed = can_be_rehashed && read_only_serializer.can_be_rehashed();

  i::SnapshotData read_only_snapshot(&read_only_serializer);
  i::SnapshotData startup_snapshot(&startup_serializer);
  StartupData result =
      i::Snapshot::CreateSnapshotBlob(&startup_snapshot, &read_only_snapshot,
                                      context_snapshots, can_be_rehashed);

  // Delete heap-allocated context snapshot instances.
  for (const auto context_snapshot : context_snapshots) {
    delete context_snapshot;
  }
  data->created_ = true;

  DCHECK(i::Snapshot::VerifyChecksum(&result));
  return result;
}

bool StartupData::CanBeRehashed() const {
  DCHECK(i::Snapshot::VerifyChecksum(this));
  return i::Snapshot::ExtractRehashability(this);
}

void V8::SetDcheckErrorHandler(DcheckErrorCallback that) {
  v8::base::SetDcheckFunction(that);
}

void V8::SetFlagsFromString(const char* str) {
  SetFlagsFromString(str, strlen(str));
}

void V8::SetFlagsFromString(const char* str, size_t length) {
  i::FlagList::SetFlagsFromString(str, length);
  i::FlagList::EnforceFlagImplications();
}

void V8::SetFlagsFromCommandLine(int* argc, char** argv, bool remove_flags) {
  i::FlagList::SetFlagsFromCommandLine(argc, argv, remove_flags);
}

RegisteredExtension* RegisteredExtension::first_extension_ = nullptr;

RegisteredExtension::RegisteredExtension(std::unique_ptr<Extension> extension)
    : extension_(std::move(extension)) {}

// static
void RegisteredExtension::Register(std::unique_ptr<Extension> extension) {
  RegisteredExtension* new_extension =
      new RegisteredExtension(std::move(extension));
  new_extension->next_ = first_extension_;
  first_extension_ = new_extension;
}

// static
void RegisteredExtension::UnregisterAll() {
  RegisteredExtension* re = first_extension_;
  while (re != nullptr) {
    RegisteredExtension* next = re->next();
    delete re;
    re = next;
  }
  first_extension_ = nullptr;
}

namespace {
class ExtensionResource : public String::ExternalOneByteStringResource {
 public:
  ExtensionResource() : data_(nullptr), length_(0) {}
  ExtensionResource(const char* data, size_t length)
      : data_(data), length_(length) {}
  const char* data() const override { return data_; }
  size_t length() const override { return length_; }
  void Dispose() override {}

 private:
  const char* data_;
  size_t length_;
};
}  // anonymous namespace

void RegisterExtension(std::unique_ptr<Extension> extension) {
  RegisteredExtension::Register(std::move(extension));
}

Extension::Extension(const char* name, const char* source, int dep_count,
                     const char** deps, int source_length)
    : name_(name),
      source_length_(source_length >= 0
                         ? source_length
                         : (source ? static_cast<int>(strlen(source)) : 0)),
      dep_count_(dep_count),
      deps_(deps),
      auto_enable_(false) {
  source_ = new ExtensionResource(source, source_length_);
  CHECK(source != nullptr || source_length_ == 0);
}

void ResourceConstraints::ConfigureDefaultsFromHeapSize(
    size_t initial_heap_size_in_bytes, size_t maximum_heap_size_in_bytes) {
  CHECK_LE(initial_heap_size_in_bytes, maximum_heap_size_in_bytes);
  if (maximum_heap_size_in_bytes == 0) {
    return;
  }
  size_t young_generation, old_generation;
  i::Heap::GenerationSizesFromHeapSize(maximum_heap_size_in_bytes,
                                       &young_generation, &old_generation);
  set_max_young_generation_size_in_bytes(
      i::Max(young_generation, i::Heap::MinYoungGenerationSize()));
  set_max_old_generation_size_in_bytes(
      i::Max(old_generation, i::Heap::MinOldGenerationSize()));
  if (initial_heap_size_in_bytes > 0) {
    i::Heap::GenerationSizesFromHeapSize(initial_heap_size_in_bytes,
                                         &young_generation, &old_generation);
    // We do not set lower bounds for the initial sizes.
    set_initial_young_generation_size_in_bytes(young_generation);
    set_initial_old_generation_size_in_bytes(old_generation);
  }
  if (i::kRequiresCodeRange) {
    set_code_range_size_in_bytes(
        i::Min(i::kMaximalCodeRangeSize, maximum_heap_size_in_bytes));
  }
}

void ResourceConstraints::ConfigureDefaults(uint64_t physical_memory,
                                            uint64_t virtual_memory_limit) {
  size_t heap_size = i::Heap::HeapSizeFromPhysicalMemory(physical_memory);
  size_t young_generation, old_generation;
  i::Heap::GenerationSizesFromHeapSize(heap_size, &young_generation,
                                       &old_generation);
  set_max_young_generation_size_in_bytes(young_generation);
  set_max_old_generation_size_in_bytes(old_generation);

  if (virtual_memory_limit > 0 && i::kRequiresCodeRange) {
    set_code_range_size_in_bytes(
        i::Min(i::kMaximalCodeRangeSize,
               static_cast<size_t>(virtual_memory_limit / 8)));
  }
}

size_t ResourceConstraints::max_semi_space_size_in_kb() const {
  return i::Heap::SemiSpaceSizeFromYoungGenerationSize(
             max_young_generation_size_) /
         i::KB;
}

void ResourceConstraints::set_max_semi_space_size_in_kb(size_t limit_in_kb) {
  set_max_young_generation_size_in_bytes(
      i::Heap::YoungGenerationSizeFromSemiSpaceSize(limit_in_kb * i::KB));
}

i::Address* V8::GlobalizeReference(i::Isolate* isolate, i::Address* obj) {
  LOG_API(isolate, Persistent, New);
  i::Handle<i::Object> result = isolate->global_handles()->Create(*obj);
#ifdef VERIFY_HEAP
  if (i::FLAG_verify_heap) {
    i::Object(*obj).ObjectVerify(isolate);
  }
#endif  // VERIFY_HEAP
  return result.location();
}

i::Address* V8::GlobalizeTracedReference(i::Isolate* isolate, i::Address* obj,
                                         internal::Address* slot,
                                         bool has_destructor) {
  LOG_API(isolate, TracedGlobal, New);
#ifdef DEBUG
  Utils::ApiCheck((slot != nullptr), "v8::GlobalizeTracedReference",
                  "the address slot must be not null");
#endif
  i::Handle<i::Object> result =
      isolate->global_handles()->CreateTraced(*obj, slot, has_destructor);
#ifdef VERIFY_HEAP
  if (i::FLAG_verify_heap) {
    i::Object(*obj).ObjectVerify(isolate);
  }
#endif  // VERIFY_HEAP
  return result.location();
}

i::Address* V8::CopyGlobalReference(i::Address* from) {
  i::Handle<i::Object> result = i::GlobalHandles::CopyGlobal(from);
  return result.location();
}

void V8::MoveGlobalReference(internal::Address** from, internal::Address** to) {
  i::GlobalHandles::MoveGlobal(from, to);
}

void V8::MoveTracedGlobalReference(internal::Address** from,
                                   internal::Address** to) {
  i::GlobalHandles::MoveTracedGlobal(from, to);
}

void V8::CopyTracedGlobalReference(const internal::Address* const* from,
                                   internal::Address** to) {
  i::GlobalHandles::CopyTracedGlobal(from, to);
}

void V8::MakeWeak(i::Address* location, void* parameter,
                  WeakCallbackInfo<void>::Callback weak_callback,
                  WeakCallbackType type) {
  i::GlobalHandles::MakeWeak(location, parameter, weak_callback, type);
}

void V8::MakeWeak(i::Address** location_addr) {
  i::GlobalHandles::MakeWeak(location_addr);
}

void* V8::ClearWeak(i::Address* location) {
  return i::GlobalHandles::ClearWeakness(location);
}

void V8::AnnotateStrongRetainer(i::Address* location, const char* label) {
  i::GlobalHandles::AnnotateStrongRetainer(location, label);
}

void V8::DisposeGlobal(i::Address* location) {
  i::GlobalHandles::Destroy(location);
}

void V8::DisposeTracedGlobal(internal::Address* location) {
  i::GlobalHandles::DestroyTraced(location);
}

void V8::SetFinalizationCallbackTraced(
    internal::Address* location, void* parameter,
    WeakCallbackInfo<void>::Callback callback) {
  i::GlobalHandles::SetFinalizationCallbackForTraced(location, parameter,
                                                     callback);
}

Value* V8::Eternalize(Isolate* v8_isolate, Value* value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Object object = *Utils::OpenHandle(value);
  int index = -1;
  isolate->eternal_handles()->Create(isolate, object, &index);
  return reinterpret_cast<Value*>(
      isolate->eternal_handles()->Get(index).location());
}

void V8::FromJustIsNothing() {
  Utils::ApiCheck(false, "v8::FromJust", "Maybe value is Nothing.");
}

void V8::ToLocalEmpty() {
  Utils::ApiCheck(false, "v8::ToLocalChecked", "Empty MaybeLocal.");
}

void V8::InternalFieldOutOfBounds(int index) {
  Utils::ApiCheck(0 <= index && index < kInternalFieldsInWeakCallback,
                  "WeakCallbackInfo::GetInternalField",
                  "Internal field out of bounds.");
}

// --- H a n d l e s ---

HandleScope::HandleScope(Isolate* isolate) { Initialize(isolate); }

void HandleScope::Initialize(Isolate* isolate) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  // We do not want to check the correct usage of the Locker class all over the
  // place, so we do it only here: Without a HandleScope, an embedder can do
  // almost nothing, so it is enough to check in this central place.
  // We make an exception if the serializer is enabled, which means that the
  // Isolate is exclusively used to create a snapshot.
  Utils::ApiCheck(
      !v8::Locker::IsActive() ||
          internal_isolate->thread_manager()->IsLockedByCurrentThread() ||
          internal_isolate->serializer_enabled(),
      "HandleScope::HandleScope",
      "Entering the V8 API without proper locking in place");
  i::HandleScopeData* current = internal_isolate->handle_scope_data();
  isolate_ = internal_isolate;
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
}

HandleScope::~HandleScope() {
  i::HandleScope::CloseScope(isolate_, prev_next_, prev_limit_);
}

void* HandleScope::operator new(size_t) { base::OS::Abort(); }
void* HandleScope::operator new[](size_t) { base::OS::Abort(); }
void HandleScope::operator delete(void*, size_t) { base::OS::Abort(); }
void HandleScope::operator delete[](void*, size_t) { base::OS::Abort(); }

int HandleScope::NumberOfHandles(Isolate* isolate) {
  return i::HandleScope::NumberOfHandles(
      reinterpret_cast<i::Isolate*>(isolate));
}

i::Address* HandleScope::CreateHandle(i::Isolate* isolate, i::Address value) {
  return i::HandleScope::CreateHandle(isolate, value);
}

EscapableHandleScope::EscapableHandleScope(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  escape_slot_ =
      CreateHandle(isolate, i::ReadOnlyRoots(isolate).the_hole_value().ptr());
  Initialize(v8_isolate);
}

i::Address* EscapableHandleScope::Escape(i::Address* escape_value) {
  i::Heap* heap = reinterpret_cast<i::Isolate*>(GetIsolate())->heap();
  Utils::ApiCheck(i::Object(*escape_slot_).IsTheHole(heap->isolate()),
                  "EscapableHandleScope::Escape", "Escape value set twice");
  if (escape_value == nullptr) {
    *escape_slot_ = i::ReadOnlyRoots(heap).undefined_value().ptr();
    return nullptr;
  }
  *escape_slot_ = *escape_value;
  return escape_slot_;
}

void* EscapableHandleScope::operator new(size_t) { base::OS::Abort(); }
void* EscapableHandleScope::operator new[](size_t) { base::OS::Abort(); }
void EscapableHandleScope::operator delete(void*, size_t) { base::OS::Abort(); }
void EscapableHandleScope::operator delete[](void*, size_t) {
  base::OS::Abort();
}

SealHandleScope::SealHandleScope(Isolate* isolate)
    : isolate_(reinterpret_cast<i::Isolate*>(isolate)) {
  i::HandleScopeData* current = isolate_->handle_scope_data();
  prev_limit_ = current->limit;
  current->limit = current->next;
  prev_sealed_level_ = current->sealed_level;
  current->sealed_level = current->level;
}

SealHandleScope::~SealHandleScope() {
  i::HandleScopeData* current = isolate_->handle_scope_data();
  DCHECK_EQ(current->next, current->limit);
  current->limit = prev_limit_;
  DCHECK_EQ(current->level, current->sealed_level);
  current->sealed_level = prev_sealed_level_;
}

void* SealHandleScope::operator new(size_t) { base::OS::Abort(); }
void* SealHandleScope::operator new[](size_t) { base::OS::Abort(); }
void SealHandleScope::operator delete(void*, size_t) { base::OS::Abort(); }
void SealHandleScope::operator delete[](void*, size_t) { base::OS::Abort(); }

void Context::Enter() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  impl->EnterContext(*env);
  impl->SaveContext(isolate->context());
  isolate->set_context(*env);
}

void Context::Exit() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  if (!Utils::ApiCheck(impl->LastEnteredContextWas(*env), "v8::Context::Exit()",
                       "Cannot exit non-entered context")) {
    return;
  }
  impl->LeaveContext();
  isolate->set_context(impl->RestoreContext());
}

Context::BackupIncumbentScope::BackupIncumbentScope(
    Local<Context> backup_incumbent_context)
    : backup_incumbent_context_(backup_incumbent_context) {
  DCHECK(!backup_incumbent_context_.IsEmpty());

  i::Handle<i::Context> env = Utils::OpenHandle(*backup_incumbent_context_);
  i::Isolate* isolate = env->GetIsolate();

  js_stack_comparable_address_ =
      i::SimulatorStack::RegisterJSStackComparableAddress(isolate);

  prev_ = isolate->top_backup_incumbent_scope();
  isolate->set_top_backup_incumbent_scope(this);
}

Context::BackupIncumbentScope::~BackupIncumbentScope() {
  i::Handle<i::Context> env = Utils::OpenHandle(*backup_incumbent_context_);
  i::Isolate* isolate = env->GetIsolate();

  i::SimulatorStack::UnregisterJSStackComparableAddress(isolate);

  isolate->set_top_backup_incumbent_scope(prev_);
}

STATIC_ASSERT(i::Internals::kEmbedderDataSlotSize == i::kEmbedderDataSlotSize);

static i::Handle<i::EmbedderDataArray> EmbedderDataFor(Context* context,
                                                       int index, bool can_grow,
                                                       const char* location) {
  i::Handle<i::Context> env = Utils::OpenHandle(context);
  i::Isolate* isolate = env->GetIsolate();
  bool ok = Utils::ApiCheck(env->IsNativeContext(), location,
                            "Not a native context") &&
            Utils::ApiCheck(index >= 0, location, "Negative index");
  if (!ok) return i::Handle<i::EmbedderDataArray>();
  // TODO(ishell): remove cast once embedder_data slot has a proper type.
  i::Handle<i::EmbedderDataArray> data(
      i::EmbedderDataArray::cast(env->embedder_data()), isolate);
  if (index < data->length()) return data;
  if (!Utils::ApiCheck(can_grow && index < i::EmbedderDataArray::kMaxLength,
                       location, "Index too large")) {
    return i::Handle<i::EmbedderDataArray>();
  }
  data = i::EmbedderDataArray::EnsureCapacity(isolate, data, index);
  env->set_embedder_data(*data);
  return data;
}

uint32_t Context::GetNumberOfEmbedderDataFields() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  CHECK(context->IsNativeContext());
  // TODO(ishell): remove cast once embedder_data slot has a proper type.
  return static_cast<uint32_t>(
      i::EmbedderDataArray::cast(context->embedder_data()).length());
}

v8::Local<v8::Value> Context::SlowGetEmbedderData(int index) {
  const char* location = "v8::Context::GetEmbedderData()";
  i::Handle<i::EmbedderDataArray> data =
      EmbedderDataFor(this, index, false, location);
  if (data.is_null()) return Local<Value>();
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  i::Handle<i::Object> result(i::EmbedderDataSlot(*data, index).load_tagged(),
                              isolate);
  return Utils::ToLocal(result);
}

void Context::SetEmbedderData(int index, v8::Local<Value> value) {
  const char* location = "v8::Context::SetEmbedderData()";
  i::Handle<i::EmbedderDataArray> data =
      EmbedderDataFor(this, index, true, location);
  if (data.is_null()) return;
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  i::EmbedderDataSlot::store_tagged(*data, index, *val);
  DCHECK_EQ(*Utils::OpenHandle(*value),
            *Utils::OpenHandle(*GetEmbedderData(index)));
}

void* Context::SlowGetAlignedPointerFromEmbedderData(int index) {
  const char* location = "v8::Context::GetAlignedPointerFromEmbedderData()";
  HandleScope handle_scope(GetIsolate());
  i::Handle<i::EmbedderDataArray> data =
      EmbedderDataFor(this, index, false, location);
  if (data.is_null()) return nullptr;
  void* result;
  Utils::ApiCheck(i::EmbedderDataSlot(*data, index).ToAlignedPointer(&result),
                  location, "Pointer is not aligned");
  return result;
}

void Context::SetAlignedPointerInEmbedderData(int index, void* value) {
  const char* location = "v8::Context::SetAlignedPointerInEmbedderData()";
  i::Handle<i::EmbedderDataArray> data =
      EmbedderDataFor(this, index, true, location);
  bool ok = i::EmbedderDataSlot(*data, index).store_aligned_pointer(value);
  Utils::ApiCheck(ok, location, "Pointer is not aligned");
  DCHECK_EQ(value, GetAlignedPointerFromEmbedderData(index));
}

// --- T e m p l a t e ---

static void InitializeTemplate(i::Handle<i::TemplateInfo> that, int type) {
  that->set_number_of_properties(0);
  that->set_tag(i::Smi::FromInt(type));
}

void Template::Set(v8::Local<Name> name, v8::Local<Data> value,
                   v8::PropertyAttribute attribute) {
  auto templ = Utils::OpenHandle(this);
  i::Isolate* isolate = templ->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto value_obj = Utils::OpenHandle(*value);
  CHECK(!value_obj->IsJSReceiver() || value_obj->IsTemplateInfo());
  if (value_obj->IsObjectTemplateInfo()) {
    templ->set_serial_number(i::Smi::zero());
    if (templ->IsFunctionTemplateInfo()) {
      i::Handle<i::FunctionTemplateInfo>::cast(templ)->set_do_not_cache(true);
    }
  }
  i::ApiNatives::AddDataProperty(isolate, templ, Utils::OpenHandle(*name),
                                 value_obj,
                                 static_cast<i::PropertyAttributes>(attribute));
}

void Template::SetPrivate(v8::Local<Private> name, v8::Local<Data> value,
                          v8::PropertyAttribute attribute) {
  Set(Utils::ToLocal(Utils::OpenHandle(reinterpret_cast<Name*>(*name))), value,
      attribute);
}

void Template::SetAccessorProperty(v8::Local<v8::Name> name,
                                   v8::Local<FunctionTemplate> getter,
                                   v8::Local<FunctionTemplate> setter,
                                   v8::PropertyAttribute attribute,
                                   v8::AccessControl access_control) {
  // TODO(verwaest): Remove |access_control|.
  DCHECK_EQ(v8::DEFAULT, access_control);
  auto templ = Utils::OpenHandle(this);
  auto isolate = templ->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  DCHECK(!name.IsEmpty());
  DCHECK(!getter.IsEmpty() || !setter.IsEmpty());
  i::HandleScope scope(isolate);
  i::ApiNatives::AddAccessorProperty(
      isolate, templ, Utils::OpenHandle(*name),
      Utils::OpenHandle(*getter, true), Utils::OpenHandle(*setter, true),
      static_cast<i::PropertyAttributes>(attribute));
}

// --- F u n c t i o n   T e m p l a t e ---
static void InitializeFunctionTemplate(
    i::Handle<i::FunctionTemplateInfo> info) {
  InitializeTemplate(info, Consts::FUNCTION_TEMPLATE);
  info->set_flag(0);
}

static Local<ObjectTemplate> ObjectTemplateNew(
    i::Isolate* isolate, v8::Local<FunctionTemplate> constructor,
    bool do_not_cache);

Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> result(self->GetPrototypeTemplate(), i_isolate);
  if (result->IsUndefined(i_isolate)) {
    // Do not cache prototype objects.
    result = Utils::OpenHandle(
        *ObjectTemplateNew(i_isolate, Local<FunctionTemplate>(), true));
    i::FunctionTemplateInfo::SetPrototypeTemplate(i_isolate, self, result);
  }
  return ToApiHandle<ObjectTemplate>(result);
}

void FunctionTemplate::SetPrototypeProviderTemplate(
    Local<FunctionTemplate> prototype_provider) {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> result = Utils::OpenHandle(*prototype_provider);
  CHECK(self->GetPrototypeTemplate().IsUndefined(i_isolate));
  CHECK(self->GetParentTemplate().IsUndefined(i_isolate));
  i::FunctionTemplateInfo::SetPrototypeProviderTemplate(i_isolate, self,
                                                        result);
}

static void EnsureNotInstantiated(i::Handle<i::FunctionTemplateInfo> info,
                                  const char* func) {
  Utils::ApiCheck(!info->instantiated(), func,
                  "FunctionTemplate already instantiated");
}

void FunctionTemplate::Inherit(v8::Local<FunctionTemplate> value) {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::Inherit");
  i::Isolate* i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  CHECK(info->GetPrototypeProviderTemplate().IsUndefined(i_isolate));
  i::FunctionTemplateInfo::SetParentTemplate(i_isolate, info,
                                             Utils::OpenHandle(*value));
}

static Local<FunctionTemplate> FunctionTemplateNew(
    i::Isolate* isolate, FunctionCallback callback, v8::Local<Value> data,
    v8::Local<Signature> signature, int length, bool do_not_cache,
    v8::Local<Private> cached_property_name = v8::Local<Private>(),
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect) {
  i::Handle<i::Struct> struct_obj = isolate->factory()->NewStruct(
      i::FUNCTION_TEMPLATE_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::FunctionTemplateInfo> obj =
      i::Handle<i::FunctionTemplateInfo>::cast(struct_obj);
  {
    // Disallow GC until all fields of obj have acceptable types.
    i::DisallowHeapAllocation no_gc;
    InitializeFunctionTemplate(obj);
    obj->set_length(length);
    obj->set_do_not_cache(do_not_cache);
    int next_serial_number = i::FunctionTemplateInfo::kInvalidSerialNumber;
    if (!do_not_cache) {
      next_serial_number = isolate->heap()->GetNextTemplateSerialNumber();
    }
    obj->set_serial_number(i::Smi::FromInt(next_serial_number));
  }
  if (callback != nullptr) {
    Utils::ToLocal(obj)->SetCallHandler(callback, data, side_effect_type);
  }
  obj->set_undetectable(false);
  obj->set_needs_access_check(false);
  obj->set_accept_any_receiver(true);
  if (!signature.IsEmpty()) {
    obj->set_signature(*Utils::OpenHandle(*signature));
  }
  obj->set_cached_property_name(
      cached_property_name.IsEmpty()
          ? i::ReadOnlyRoots(isolate).the_hole_value()
          : *Utils::OpenHandle(*cached_property_name));
  return Utils::ToLocal(obj);
}

Local<FunctionTemplate> FunctionTemplate::New(
    Isolate* isolate, FunctionCallback callback, v8::Local<Value> data,
    v8::Local<Signature> signature, int length, ConstructorBehavior behavior,
    SideEffectType side_effect_type, const CFunction*) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  // Changes to the environment cannot be captured in the snapshot. Expect no
  // function templates when the isolate is created for serialization.
  LOG_API(i_isolate, FunctionTemplate, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  auto templ = FunctionTemplateNew(i_isolate, callback, data, signature, length,
                                   false, Local<Private>(), side_effect_type);
  if (behavior == ConstructorBehavior::kThrow) templ->RemovePrototype();
  return templ;
}

Local<FunctionTemplate> FunctionTemplate::NewWithCache(
    Isolate* isolate, FunctionCallback callback, Local<Private> cache_property,
    Local<Value> data, Local<Signature> signature, int length,
    SideEffectType side_effect_type) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, FunctionTemplate, NewWithCache);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return FunctionTemplateNew(i_isolate, callback, data, signature, length,
                             false, cache_property, side_effect_type);
}

Local<Signature> Signature::New(Isolate* isolate,
                                Local<FunctionTemplate> receiver) {
  return Utils::SignatureToLocal(Utils::OpenHandle(*receiver));
}

Local<AccessorSignature> AccessorSignature::New(
    Isolate* isolate, Local<FunctionTemplate> receiver) {
  return Utils::AccessorSignatureToLocal(Utils::OpenHandle(*receiver));
}

#define SET_FIELD_WRAPPED(isolate, obj, setter, cdata)        \
  do {                                                        \
    i::Handle<i::Object> foreign = FromCData(isolate, cdata); \
    (obj)->setter(*foreign);                                  \
  } while (false)

void FunctionTemplate::SetCallHandler(FunctionCallback callback,
                                      v8::Local<Value> data,
                                      SideEffectType side_effect_type,
                                      const CFunction*) {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::SetCallHandler");
  i::Isolate* isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::CallHandlerInfo> obj = isolate->factory()->NewCallHandlerInfo(
      side_effect_type == SideEffectType::kHasNoSideEffect);
  SET_FIELD_WRAPPED(isolate, obj, set_callback, callback);
  SET_FIELD_WRAPPED(isolate, obj, set_js_callback, obj->redirected_callback());
  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  info->set_call_code(*obj);
}

namespace {

template <typename Getter, typename Setter>
i::Handle<i::AccessorInfo> MakeAccessorInfo(
    i::Isolate* isolate, v8::Local<Name> name, Getter getter, Setter setter,
    v8::Local<Value> data, v8::AccessControl settings,
    v8::Local<AccessorSignature> signature, bool is_special_data_property,
    bool replace_on_access) {
  i::Handle<i::AccessorInfo> obj = isolate->factory()->NewAccessorInfo();
  SET_FIELD_WRAPPED(isolate, obj, set_getter, getter);
  DCHECK_IMPLIES(replace_on_access,
                 is_special_data_property && setter == nullptr);
  if (is_special_data_property && setter == nullptr) {
    setter = reinterpret_cast<Setter>(&i::Accessors::ReconfigureToDataProperty);
  }
  SET_FIELD_WRAPPED(isolate, obj, set_setter, setter);
  i::Address redirected = obj->redirected_getter();
  if (redirected != i::kNullAddress) {
    SET_FIELD_WRAPPED(isolate, obj, set_js_getter, redirected);
  }
  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  obj->set_is_special_data_property(is_special_data_property);
  obj->set_replace_on_access(replace_on_access);
  i::Handle<i::Name> accessor_name = Utils::OpenHandle(*name);
  if (!accessor_name->IsUniqueName()) {
    accessor_name = isolate->factory()->InternalizeString(
        i::Handle<i::String>::cast(accessor_name));
  }
  obj->set_name(*accessor_name);
  if (settings & ALL_CAN_READ) obj->set_all_can_read(true);
  if (settings & ALL_CAN_WRITE) obj->set_all_can_write(true);
  obj->set_initial_property_attributes(i::NONE);
  if (!signature.IsEmpty()) {
    obj->set_expected_receiver_type(*Utils::OpenHandle(*signature));
  }
  return obj;
}

}  // namespace

Local<ObjectTemplate> FunctionTemplate::InstanceTemplate() {
  i::Handle<i::FunctionTemplateInfo> handle = Utils::OpenHandle(this, true);
  if (!Utils::ApiCheck(!handle.is_null(),
                       "v8::FunctionTemplate::InstanceTemplate()",
                       "Reading from empty handle")) {
    return Local<ObjectTemplate>();
  }
  i::Isolate* isolate = handle->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (handle->GetInstanceTemplate().IsUndefined(isolate)) {
    Local<ObjectTemplate> templ =
        ObjectTemplate::New(isolate, ToApiHandle<FunctionTemplate>(handle));
    i::FunctionTemplateInfo::SetInstanceTemplate(isolate, handle,
                                                 Utils::OpenHandle(*templ));
  }
  i::Handle<i::ObjectTemplateInfo> result(
      i::ObjectTemplateInfo::cast(handle->GetInstanceTemplate()), isolate);
  return Utils::ToLocal(result);
}

void FunctionTemplate::SetLength(int length) {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::SetLength");
  auto isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  info->set_length(length);
}

void FunctionTemplate::SetClassName(Local<String> name) {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::SetClassName");
  auto isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  info->set_class_name(*Utils::OpenHandle(*name));
}

void FunctionTemplate::SetAcceptAnyReceiver(bool value) {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::SetAcceptAnyReceiver");
  auto isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  info->set_accept_any_receiver(value);
}

void FunctionTemplate::ReadOnlyPrototype() {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::ReadOnlyPrototype");
  auto isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  info->set_read_only_prototype(true);
}

void FunctionTemplate::RemovePrototype() {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::RemovePrototype");
  auto isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  info->set_remove_prototype(true);
}

// --- O b j e c t T e m p l a t e ---

Local<ObjectTemplate> ObjectTemplate::New(
    Isolate* isolate, v8::Local<FunctionTemplate> constructor) {
  return New(reinterpret_cast<i::Isolate*>(isolate), constructor);
}

static Local<ObjectTemplate> ObjectTemplateNew(
    i::Isolate* isolate, v8::Local<FunctionTemplate> constructor,
    bool do_not_cache) {
  LOG_API(isolate, ObjectTemplate, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Struct> struct_obj = isolate->factory()->NewStruct(
      i::OBJECT_TEMPLATE_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::ObjectTemplateInfo> obj =
      i::Handle<i::ObjectTemplateInfo>::cast(struct_obj);
  InitializeTemplate(obj, Consts::OBJECT_TEMPLATE);
  int next_serial_number = 0;
  if (!do_not_cache) {
    next_serial_number = isolate->heap()->GetNextTemplateSerialNumber();
  }
  obj->set_serial_number(i::Smi::FromInt(next_serial_number));
  if (!constructor.IsEmpty())
    obj->set_constructor(*Utils::OpenHandle(*constructor));
  obj->set_data(i::Smi::zero());
  return Utils::ToLocal(obj);
}

Local<ObjectTemplate> ObjectTemplate::New(
    i::Isolate* isolate, v8::Local<FunctionTemplate> constructor) {
  return ObjectTemplateNew(isolate, constructor, false);
}

// Ensure that the object template has a constructor.  If no
// constructor is available we create one.
static i::Handle<i::FunctionTemplateInfo> EnsureConstructor(
    i::Isolate* isolate, ObjectTemplate* object_template) {
  i::Object obj = Utils::OpenHandle(object_template)->constructor();
  if (!obj.IsUndefined(isolate)) {
    i::FunctionTemplateInfo info = i::FunctionTemplateInfo::cast(obj);
    return i::Handle<i::FunctionTemplateInfo>(info, isolate);
  }
  Local<FunctionTemplate> templ =
      FunctionTemplate::New(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::FunctionTemplateInfo> constructor = Utils::OpenHandle(*templ);
  i::FunctionTemplateInfo::SetInstanceTemplate(
      isolate, constructor, Utils::OpenHandle(object_template));
  Utils::OpenHandle(object_template)->set_constructor(*constructor);
  return constructor;
}

template <typename Getter, typename Setter, typename Data, typename Template>
static void TemplateSetAccessor(
    Template* template_obj, v8::Local<Name> name, Getter getter, Setter setter,
    Data data, AccessControl settings, PropertyAttribute attribute,
    v8::Local<AccessorSignature> signature, bool is_special_data_property,
    bool replace_on_access, SideEffectType getter_side_effect_type,
    SideEffectType setter_side_effect_type) {
  auto info = Utils::OpenHandle(template_obj);
  auto isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::AccessorInfo> accessor_info =
      MakeAccessorInfo(isolate, name, getter, setter, data, settings, signature,
                       is_special_data_property, replace_on_access);
  accessor_info->set_initial_property_attributes(
      static_cast<i::PropertyAttributes>(attribute));
  accessor_info->set_getter_side_effect_type(getter_side_effect_type);
  accessor_info->set_setter_side_effect_type(setter_side_effect_type);
  i::ApiNatives::AddNativeDataProperty(isolate, info, accessor_info);
}

void Template::SetNativeDataProperty(
    v8::Local<String> name, AccessorGetterCallback getter,
    AccessorSetterCallback setter, v8::Local<Value> data,
    PropertyAttribute attribute, v8::Local<AccessorSignature> signature,
    AccessControl settings, SideEffectType getter_side_effect_type,
    SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      signature, true, false, getter_side_effect_type,
                      setter_side_effect_type);
}

void Template::SetNativeDataProperty(
    v8::Local<Name> name, AccessorNameGetterCallback getter,
    AccessorNameSetterCallback setter, v8::Local<Value> data,
    PropertyAttribute attribute, v8::Local<AccessorSignature> signature,
    AccessControl settings, SideEffectType getter_side_effect_type,
    SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      signature, true, false, getter_side_effect_type,
                      setter_side_effect_type);
}

void Template::SetLazyDataProperty(v8::Local<Name> name,
                                   AccessorNameGetterCallback getter,
                                   v8::Local<Value> data,
                                   PropertyAttribute attribute,
                                   SideEffectType getter_side_effect_type,
                                   SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter,
                      static_cast<AccessorNameSetterCallback>(nullptr), data,
                      DEFAULT, attribute, Local<AccessorSignature>(), true,
                      true, getter_side_effect_type, setter_side_effect_type);
}

void Template::SetIntrinsicDataProperty(Local<Name> name, Intrinsic intrinsic,
                                        PropertyAttribute attribute) {
  auto templ = Utils::OpenHandle(this);
  i::Isolate* isolate = templ->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  i::ApiNatives::AddDataProperty(isolate, templ, Utils::OpenHandle(*name),
                                 intrinsic,
                                 static_cast<i::PropertyAttributes>(attribute));
}

void ObjectTemplate::SetAccessor(v8::Local<String> name,
                                 AccessorGetterCallback getter,
                                 AccessorSetterCallback setter,
                                 v8::Local<Value> data, AccessControl settings,
                                 PropertyAttribute attribute,
                                 v8::Local<AccessorSignature> signature,
                                 SideEffectType getter_side_effect_type,
                                 SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      signature, i::FLAG_disable_old_api_accessors, false,
                      getter_side_effect_type, setter_side_effect_type);
}

void ObjectTemplate::SetAccessor(v8::Local<Name> name,
                                 AccessorNameGetterCallback getter,
                                 AccessorNameSetterCallback setter,
                                 v8::Local<Value> data, AccessControl settings,
                                 PropertyAttribute attribute,
                                 v8::Local<AccessorSignature> signature,
                                 SideEffectType getter_side_effect_type,
                                 SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      signature, i::FLAG_disable_old_api_accessors, false,
                      getter_side_effect_type, setter_side_effect_type);
}

template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
static i::Handle<i::InterceptorInfo> CreateInterceptorInfo(
    i::Isolate* isolate, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  auto obj = i::Handle<i::InterceptorInfo>::cast(isolate->factory()->NewStruct(
      i::INTERCEPTOR_INFO_TYPE, i::AllocationType::kOld));
  obj->set_flags(0);

  if (getter != nullptr) SET_FIELD_WRAPPED(isolate, obj, set_getter, getter);
  if (setter != nullptr) SET_FIELD_WRAPPED(isolate, obj, set_setter, setter);
  if (query != nullptr) SET_FIELD_WRAPPED(isolate, obj, set_query, query);
  if (descriptor != nullptr)
    SET_FIELD_WRAPPED(isolate, obj, set_descriptor, descriptor);
  if (remover != nullptr) SET_FIELD_WRAPPED(isolate, obj, set_deleter, remover);
  if (enumerator != nullptr)
    SET_FIELD_WRAPPED(isolate, obj, set_enumerator, enumerator);
  if (definer != nullptr) SET_FIELD_WRAPPED(isolate, obj, set_definer, definer);
  obj->set_can_intercept_symbols(
      !(static_cast<int>(flags) &
        static_cast<int>(PropertyHandlerFlags::kOnlyInterceptStrings)));
  obj->set_all_can_read(static_cast<int>(flags) &
                        static_cast<int>(PropertyHandlerFlags::kAllCanRead));
  obj->set_non_masking(static_cast<int>(flags) &
                       static_cast<int>(PropertyHandlerFlags::kNonMasking));
  obj->set_has_no_side_effect(
      static_cast<int>(flags) &
      static_cast<int>(PropertyHandlerFlags::kHasNoSideEffect));

  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  return obj;
}

template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
static i::Handle<i::InterceptorInfo> CreateNamedInterceptorInfo(
    i::Isolate* isolate, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  auto interceptor =
      CreateInterceptorInfo(isolate, getter, setter, query, descriptor, remover,
                            enumerator, definer, data, flags);
  interceptor->set_is_named(true);
  return interceptor;
}

template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
static i::Handle<i::InterceptorInfo> CreateIndexedInterceptorInfo(
    i::Isolate* isolate, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  auto interceptor =
      CreateInterceptorInfo(isolate, getter, setter, query, descriptor, remover,
                            enumerator, definer, data, flags);
  interceptor->set_is_named(false);
  return interceptor;
}

template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
static void ObjectTemplateSetNamedPropertyHandler(
    ObjectTemplate* templ, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  i::Isolate* isolate = Utils::OpenHandle(templ)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto cons = EnsureConstructor(isolate, templ);
  EnsureNotInstantiated(cons, "ObjectTemplateSetNamedPropertyHandler");
  auto obj =
      CreateNamedInterceptorInfo(isolate, getter, setter, query, descriptor,
                                 remover, enumerator, definer, data, flags);
  i::FunctionTemplateInfo::SetNamedPropertyHandler(isolate, cons, obj);
}

void ObjectTemplate::SetHandler(
    const NamedPropertyHandlerConfiguration& config) {
  ObjectTemplateSetNamedPropertyHandler(
      this, config.getter, config.setter, config.query, config.descriptor,
      config.deleter, config.enumerator, config.definer, config.data,
      config.flags);
}

void ObjectTemplate::MarkAsUndetectable() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto cons = EnsureConstructor(isolate, this);
  EnsureNotInstantiated(cons, "v8::ObjectTemplate::MarkAsUndetectable");
  cons->set_undetectable(true);
}

void ObjectTemplate::SetAccessCheckCallback(AccessCheckCallback callback,
                                            Local<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto cons = EnsureConstructor(isolate, this);
  EnsureNotInstantiated(cons, "v8::ObjectTemplate::SetAccessCheckCallback");

  i::Handle<i::Struct> struct_info = isolate->factory()->NewStruct(
      i::ACCESS_CHECK_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::AccessCheckInfo> info =
      i::Handle<i::AccessCheckInfo>::cast(struct_info);

  SET_FIELD_WRAPPED(isolate, info, set_callback, callback);
  info->set_named_interceptor(i::Object());
  info->set_indexed_interceptor(i::Object());

  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  info->set_data(*Utils::OpenHandle(*data));

  i::FunctionTemplateInfo::SetAccessCheckInfo(isolate, cons, info);
  cons->set_needs_access_check(true);
}

void ObjectTemplate::SetAccessCheckCallbackAndHandler(
    AccessCheckCallback callback,
    const NamedPropertyHandlerConfiguration& named_handler,
    const IndexedPropertyHandlerConfiguration& indexed_handler,
    Local<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto cons = EnsureConstructor(isolate, this);
  EnsureNotInstantiated(
      cons, "v8::ObjectTemplate::SetAccessCheckCallbackWithHandler");

  i::Handle<i::Struct> struct_info = isolate->factory()->NewStruct(
      i::ACCESS_CHECK_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::AccessCheckInfo> info =
      i::Handle<i::AccessCheckInfo>::cast(struct_info);

  SET_FIELD_WRAPPED(isolate, info, set_callback, callback);
  auto named_interceptor = CreateNamedInterceptorInfo(
      isolate, named_handler.getter, named_handler.setter, named_handler.query,
      named_handler.descriptor, named_handler.deleter, named_handler.enumerator,
      named_handler.definer, named_handler.data, named_handler.flags);
  info->set_named_interceptor(*named_interceptor);
  auto indexed_interceptor = CreateIndexedInterceptorInfo(
      isolate, indexed_handler.getter, indexed_handler.setter,
      indexed_handler.query, indexed_handler.descriptor,
      indexed_handler.deleter, indexed_handler.enumerator,
      indexed_handler.definer, indexed_handler.data, indexed_handler.flags);
  info->set_indexed_interceptor(*indexed_interceptor);

  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  info->set_data(*Utils::OpenHandle(*data));

  i::FunctionTemplateInfo::SetAccessCheckInfo(isolate, cons, info);
  cons->set_needs_access_check(true);
}

void ObjectTemplate::SetHandler(
    const IndexedPropertyHandlerConfiguration& config) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto cons = EnsureConstructor(isolate, this);
  EnsureNotInstantiated(cons, "v8::ObjectTemplate::SetHandler");
  auto obj = CreateIndexedInterceptorInfo(
      isolate, config.getter, config.setter, config.query, config.descriptor,
      config.deleter, config.enumerator, config.definer, config.data,
      config.flags);
  i::FunctionTemplateInfo::SetIndexedPropertyHandler(isolate, cons, obj);
}

void ObjectTemplate::SetCallAsFunctionHandler(FunctionCallback callback,
                                              Local<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto cons = EnsureConstructor(isolate, this);
  EnsureNotInstantiated(cons, "v8::ObjectTemplate::SetCallAsFunctionHandler");
  i::Handle<i::CallHandlerInfo> obj = isolate->factory()->NewCallHandlerInfo();
  SET_FIELD_WRAPPED(isolate, obj, set_callback, callback);
  SET_FIELD_WRAPPED(isolate, obj, set_js_callback, obj->redirected_callback());
  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  i::FunctionTemplateInfo::SetInstanceCallHandler(isolate, cons, obj);
}

int ObjectTemplate::InternalFieldCount() {
  return Utils::OpenHandle(this)->embedder_field_count();
}

void ObjectTemplate::SetInternalFieldCount(int value) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (!Utils::ApiCheck(i::Smi::IsValid(value),
                       "v8::ObjectTemplate::SetInternalFieldCount()",
                       "Invalid embedder field count")) {
    return;
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (value > 0) {
    // The embedder field count is set by the constructor function's
    // construct code, so we ensure that there is a constructor
    // function to do the setting.
    EnsureConstructor(isolate, this);
  }
  Utils::OpenHandle(this)->set_embedder_field_count(value);
}

bool ObjectTemplate::IsImmutableProto() {
  return Utils::OpenHandle(this)->immutable_proto();
}

void ObjectTemplate::SetImmutableProto() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  self->set_immutable_proto(true);
}

// --- S c r i p t s ---

// Internally, UnboundScript is a SharedFunctionInfo, and Script is a
// JSFunction.

ScriptCompiler::CachedData::CachedData(const uint8_t* data_, int length_,
                                       BufferPolicy buffer_policy_)
    : data(data_),
      length(length_),
      rejected(false),
      buffer_policy(buffer_policy_) {}

ScriptCompiler::CachedData::~CachedData() {
  if (buffer_policy == BufferOwned) {
    delete[] data;
  }
}

bool ScriptCompiler::ExternalSourceStream::SetBookmark() { return false; }

void ScriptCompiler::ExternalSourceStream::ResetToBookmark() { UNREACHABLE(); }

ScriptCompiler::StreamedSource::StreamedSource(ExternalSourceStream* stream,
                                               Encoding encoding)
    : StreamedSource(std::unique_ptr<ExternalSourceStream>(stream), encoding) {}

ScriptCompiler::StreamedSource::StreamedSource(
    std::unique_ptr<ExternalSourceStream> stream, Encoding encoding)
    : impl_(new i::ScriptStreamingData(std::move(stream), encoding)) {}

ScriptCompiler::StreamedSource::~StreamedSource() = default;

Local<Script> UnboundScript::BindToCurrentContext() {
  auto function_info =
      i::Handle<i::SharedFunctionInfo>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = function_info->GetIsolate();
  i::Handle<i::JSFunction> function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          function_info, isolate->native_context());
  return ToApiHandle<Script>(function);
}

int UnboundScript::GetId() {
  auto function_info =
      i::Handle<i::SharedFunctionInfo>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = function_info->GetIsolate();
  LOG_API(isolate, UnboundScript, GetId);
  i::HandleScope scope(isolate);
  i::Handle<i::Script> script(i::Script::cast(function_info->script()),
                              isolate);
  return script->id();
}

int UnboundScript::GetLineNumber(int code_pos) {
  i::Handle<i::SharedFunctionInfo> obj =
      i::Handle<i::SharedFunctionInfo>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, UnboundScript, GetLineNumber);
  if (obj->script().IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(obj->script()), isolate);
    return i::Script::GetLineNumber(script, code_pos);
  } else {
    return -1;
  }
}

Local<Value> UnboundScript::GetScriptName() {
  i::Handle<i::SharedFunctionInfo> obj =
      i::Handle<i::SharedFunctionInfo>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, UnboundScript, GetName);
  if (obj->script().IsScript()) {
    i::Object name = i::Script::cast(obj->script()).name();
    return Utils::ToLocal(i::Handle<i::Object>(name, isolate));
  } else {
    return Local<String>();
  }
}

Local<Value> UnboundScript::GetSourceURL() {
  i::Handle<i::SharedFunctionInfo> obj =
      i::Handle<i::SharedFunctionInfo>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, UnboundScript, GetSourceURL);
  if (obj->script().IsScript()) {
    i::Object url = i::Script::cast(obj->script()).source_url();
    return Utils::ToLocal(i::Handle<i::Object>(url, isolate));
  } else {
    return Local<String>();
  }
}

Local<Value> UnboundScript::GetSourceMappingURL() {
  i::Handle<i::SharedFunctionInfo> obj =
      i::Handle<i::SharedFunctionInfo>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, UnboundScript, GetSourceMappingURL);
  if (obj->script().IsScript()) {
    i::Object url = i::Script::cast(obj->script()).source_mapping_url();
    return Utils::ToLocal(i::Handle<i::Object>(url, isolate));
  } else {
    return Local<String>();
  }
}

MaybeLocal<Value> Script::Run(Local<Context> context) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  ENTER_V8(isolate, context, Script, Run, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::HistogramTimerScope execute_timer(isolate->counters()->execute(), true);
  i::AggregatingHistogramTimerScope timer(isolate->counters()->compile_lazy());
  i::TimerEventScope<i::TimerEventExecute> timer_scope(isolate);
  auto fun = i::Handle<i::JSFunction>::cast(Utils::OpenHandle(this));

  i::Handle<i::Object> receiver = isolate->global_proxy();
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::Call(isolate, fun, receiver, 0, nullptr), &result);

  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

Local<Value> ScriptOrModule::GetResourceName() {
  i::Handle<i::Script> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Object> val(obj->name(), isolate);
  return ToApiHandle<Value>(val);
}

Local<PrimitiveArray> ScriptOrModule::GetHostDefinedOptions() {
  i::Handle<i::Script> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::FixedArray> val(obj->host_defined_options(), isolate);
  return ToApiHandle<PrimitiveArray>(val);
}

Local<UnboundScript> Script::GetUnboundScript() {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::SharedFunctionInfo sfi = i::JSFunction::cast(*obj).shared();
  i::Isolate* isolate = sfi.GetIsolate();
  return ToApiHandle<UnboundScript>(i::handle(sfi, isolate));
}

// static
Local<PrimitiveArray> PrimitiveArray::New(Isolate* v8_isolate, int length) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  Utils::ApiCheck(length >= 0, "v8::PrimitiveArray::New",
                  "length must be equal or greater than zero");
  i::Handle<i::FixedArray> array = isolate->factory()->NewFixedArray(length);
  return ToApiHandle<PrimitiveArray>(array);
}

int PrimitiveArray::Length() const {
  i::Handle<i::FixedArray> array = Utils::OpenHandle(this);
  return array->length();
}

void PrimitiveArray::Set(Isolate* v8_isolate, int index,
                         Local<Primitive> item) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::FixedArray> array = Utils::OpenHandle(this);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  Utils::ApiCheck(index >= 0 && index < array->length(),
                  "v8::PrimitiveArray::Set",
                  "index must be greater than or equal to 0 and less than the "
                  "array length");
  i::Handle<i::Object> i_item = Utils::OpenHandle(*item);
  array->set(index, *i_item);
}

Local<Primitive> PrimitiveArray::Get(Isolate* v8_isolate, int index) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::FixedArray> array = Utils::OpenHandle(this);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  Utils::ApiCheck(index >= 0 && index < array->length(),
                  "v8::PrimitiveArray::Get",
                  "index must be greater than or equal to 0 and less than the "
                  "array length");
  i::Handle<i::Object> i_item(array->get(index), isolate);
  return ToApiHandle<Primitive>(i_item);
}

Module::Status Module::GetStatus() const {
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  switch (self->status()) {
    case i::Module::kUninstantiated:
    case i::Module::kPreInstantiating:
      return kUninstantiated;
    case i::Module::kInstantiating:
      return kInstantiating;
    case i::Module::kInstantiated:
      return kInstantiated;
    case i::Module::kEvaluating:
      return kEvaluating;
    case i::Module::kEvaluated:
      return kEvaluated;
    case i::Module::kErrored:
      return kErrored;
  }
  UNREACHABLE();
}

Local<Value> Module::GetException() const {
  Utils::ApiCheck(GetStatus() == kErrored, "v8::Module::GetException",
                  "Module status must be kErrored");
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  return ToApiHandle<Value>(i::handle(self->GetException(), isolate));
}

int Module::GetModuleRequestsLength() const {
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  if (self->IsSyntheticModule()) return 0;
  return i::Handle<i::SourceTextModule>::cast(self)
      ->info()
      .module_requests()
      .length();
}

Local<String> Module::GetModuleRequest(int i) const {
  CHECK_GE(i, 0);
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  CHECK(self->IsSourceTextModule());
  i::Isolate* isolate = self->GetIsolate();
  i::Handle<i::FixedArray> module_requests(
      i::Handle<i::SourceTextModule>::cast(self)->info().module_requests(),
      isolate);
  CHECK_LT(i, module_requests->length());
  return ToApiHandle<String>(i::handle(module_requests->get(i), isolate));
}

Location Module::GetModuleRequestLocation(int i) const {
  CHECK_GE(i, 0);
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  i::HandleScope scope(isolate);
  CHECK(self->IsSourceTextModule());
  i::Handle<i::FixedArray> module_request_positions(
      i::Handle<i::SourceTextModule>::cast(self)
          ->info()
          .module_request_positions(),
      isolate);
  CHECK_LT(i, module_request_positions->length());
  int position = i::Smi::ToInt(module_request_positions->get(i));
  i::Handle<i::Script> script(
      i::Handle<i::SourceTextModule>::cast(self)->script(), isolate);
  i::Script::PositionInfo info;
  i::Script::GetPositionInfo(script, position, &info, i::Script::WITH_OFFSET);
  return v8::Location(info.line, info.column);
}

Local<Value> Module::GetModuleNamespace() {
  Utils::ApiCheck(
      GetStatus() >= kInstantiated, "v8::Module::GetModuleNamespace",
      "v8::Module::GetModuleNamespace must be used on an instantiated module");
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  i::Handle<i::JSModuleNamespace> module_namespace =
      i::Module::GetModuleNamespace(self->GetIsolate(), self);
  return ToApiHandle<Value>(module_namespace);
}

Local<UnboundModuleScript> Module::GetUnboundModuleScript() {
  Utils::ApiCheck(
      GetStatus() < kEvaluating, "v8::Module::GetUnboundScript",
      "v8::Module::GetUnboundScript must be used on an unevaluated module");
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  CHECK(self->IsSourceTextModule());
  return ToApiHandle<UnboundModuleScript>(i::Handle<i::SharedFunctionInfo>(
      i::Handle<i::SourceTextModule>::cast(self)->GetSharedFunctionInfo(),
      self->GetIsolate()));
}

int Module::GetIdentityHash() const { return Utils::OpenHandle(this)->hash(); }

Maybe<bool> Module::InstantiateModule(Local<Context> context,
                                      Module::ResolveCallback callback) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Module, InstantiateModule, Nothing<bool>(),
           i::HandleScope);
  has_pending_exception = !i::Module::Instantiate(
      isolate, Utils::OpenHandle(this), context, callback);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

MaybeLocal<Value> Module::Evaluate(Local<Context> context) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  ENTER_V8(isolate, context, Module, Evaluate, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::HistogramTimerScope execute_timer(isolate->counters()->execute(), true);
  i::AggregatingHistogramTimerScope timer(isolate->counters()->compile_lazy());
  i::TimerEventScope<i::TimerEventExecute> timer_scope(isolate);

  i::Handle<i::Module> self = Utils::OpenHandle(this);
  // It's an API error to call Evaluate before Instantiate.
  CHECK_GE(self->status(), i::Module::kInstantiated);

  Local<Value> result;
  has_pending_exception = !ToLocal(i::Module::Evaluate(isolate, self), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

Local<Module> Module::CreateSyntheticModule(
    Isolate* isolate, Local<String> module_name,
    const std::vector<Local<v8::String>>& export_names,
    v8::Module::SyntheticModuleEvaluationSteps evaluation_steps) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::String> i_module_name = Utils::OpenHandle(*module_name);
  i::Handle<i::FixedArray> i_export_names = i_isolate->factory()->NewFixedArray(
      static_cast<int>(export_names.size()));
  for (int i = 0; i < i_export_names->length(); ++i) {
    i::Handle<i::String> str = i_isolate->factory()->InternalizeString(
        Utils::OpenHandle(*export_names[i]));
    i_export_names->set(i, *str);
  }
  return v8::Utils::ToLocal(
      i::Handle<i::Module>(i_isolate->factory()->NewSyntheticModule(
          i_module_name, i_export_names, evaluation_steps)));
}

Maybe<bool> Module::SetSyntheticModuleExport(Isolate* isolate,
                                             Local<String> export_name,
                                             Local<v8::Value> export_value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::String> i_export_name = Utils::OpenHandle(*export_name);
  i::Handle<i::Object> i_export_value = Utils::OpenHandle(*export_value);
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  Utils::ApiCheck(self->IsSyntheticModule(),
                  "v8::Module::SyntheticModuleSetExport",
                  "v8::Module::SyntheticModuleSetExport must only be called on "
                  "a SyntheticModule");
  ENTER_V8_NO_SCRIPT(i_isolate, isolate->GetCurrentContext(), Module,
                     SetSyntheticModuleExport, Nothing<bool>(), i::HandleScope);
  has_pending_exception =
      i::SyntheticModule::SetExport(i_isolate,
                                    i::Handle<i::SyntheticModule>::cast(self),
                                    i_export_name, i_export_value)
          .IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

void Module::SetSyntheticModuleExport(Local<String> export_name,
                                      Local<v8::Value> export_value) {
  i::Handle<i::String> i_export_name = Utils::OpenHandle(*export_name);
  i::Handle<i::Object> i_export_value = Utils::OpenHandle(*export_value);
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  Utils::ApiCheck(self->IsSyntheticModule(),
                  "v8::Module::SetSyntheticModuleExport",
                  "v8::Module::SetSyntheticModuleExport must only be called on "
                  "a SyntheticModule");
  i::SyntheticModule::SetExportStrict(self->GetIsolate(),
                                      i::Handle<i::SyntheticModule>::cast(self),
                                      i_export_name, i_export_value);
}

namespace {

i::Compiler::ScriptDetails GetScriptDetails(
    i::Isolate* isolate, Local<Value> resource_name,
    Local<Integer> resource_line_offset, Local<Integer> resource_column_offset,
    Local<Value> source_map_url, Local<PrimitiveArray> host_defined_options) {
  i::Compiler::ScriptDetails script_details;
  if (!resource_name.IsEmpty()) {
    script_details.name_obj = Utils::OpenHandle(*(resource_name));
  }
  if (!resource_line_offset.IsEmpty()) {
    script_details.line_offset =
        static_cast<int>(resource_line_offset->Value());
  }
  if (!resource_column_offset.IsEmpty()) {
    script_details.column_offset =
        static_cast<int>(resource_column_offset->Value());
  }
  script_details.host_defined_options = isolate->factory()->empty_fixed_array();
  if (!host_defined_options.IsEmpty()) {
    script_details.host_defined_options =
        Utils::OpenHandle(*(host_defined_options));
  }
  if (!source_map_url.IsEmpty()) {
    script_details.source_map_url = Utils::OpenHandle(*(source_map_url));
  }
  return script_details;
}

}  // namespace

MaybeLocal<UnboundScript> ScriptCompiler::CompileUnboundInternal(
    Isolate* v8_isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  auto isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.ScriptCompiler");
  ENTER_V8_NO_SCRIPT(isolate, v8_isolate->GetCurrentContext(), ScriptCompiler,
                     CompileUnbound, MaybeLocal<UnboundScript>(),
                     InternalEscapableScope);

  i::ScriptData* script_data = nullptr;
  if (options == kConsumeCodeCache) {
    DCHECK(source->cached_data);
    // ScriptData takes care of pointer-aligning the data.
    script_data = new i::ScriptData(source->cached_data->data,
                                    source->cached_data->length);
  }

  i::Handle<i::String> str = Utils::OpenHandle(*(source->source_string));
  i::Handle<i::SharedFunctionInfo> result;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileScript");
  i::Compiler::ScriptDetails script_details = GetScriptDetails(
      isolate, source->resource_name, source->resource_line_offset,
      source->resource_column_offset, source->source_map_url,
      source->host_defined_options);
  i::MaybeHandle<i::SharedFunctionInfo> maybe_function_info =
      i::Compiler::GetSharedFunctionInfoForScript(
          isolate, str, script_details, source->resource_options, nullptr,
          script_data, options, no_cache_reason, i::NOT_NATIVES_CODE);
  if (options == kConsumeCodeCache) {
    source->cached_data->rejected = script_data->rejected();
  }
  delete script_data;
  has_pending_exception = !maybe_function_info.ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(UnboundScript);
  RETURN_ESCAPED(ToApiHandle<UnboundScript>(result));
}

MaybeLocal<UnboundScript> ScriptCompiler::CompileUnboundScript(
    Isolate* v8_isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  Utils::ApiCheck(
      !source->GetResourceOptions().IsModule(),
      "v8::ScriptCompiler::CompileUnboundScript",
      "v8::ScriptCompiler::CompileModule must be used to compile modules");
  return CompileUnboundInternal(v8_isolate, source, options, no_cache_reason);
}

MaybeLocal<Script> ScriptCompiler::Compile(Local<Context> context,
                                           Source* source,
                                           CompileOptions options,
                                           NoCacheReason no_cache_reason) {
  Utils::ApiCheck(
      !source->GetResourceOptions().IsModule(), "v8::ScriptCompiler::Compile",
      "v8::ScriptCompiler::CompileModule must be used to compile modules");
  auto isolate = context->GetIsolate();
  auto maybe =
      CompileUnboundInternal(isolate, source, options, no_cache_reason);
  Local<UnboundScript> result;
  if (!maybe.ToLocal(&result)) return MaybeLocal<Script>();
  v8::Context::Scope scope(context);
  return result->BindToCurrentContext();
}

MaybeLocal<Module> ScriptCompiler::CompileModule(
    Isolate* isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  CHECK(options == kNoCompileOptions || options == kConsumeCodeCache);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  Utils::ApiCheck(source->GetResourceOptions().IsModule(),
                  "v8::ScriptCompiler::CompileModule",
                  "Invalid ScriptOrigin: is_module must be true");
  auto maybe =
      CompileUnboundInternal(isolate, source, options, no_cache_reason);
  Local<UnboundScript> unbound;
  if (!maybe.ToLocal(&unbound)) return MaybeLocal<Module>();

  i::Handle<i::SharedFunctionInfo> shared = Utils::OpenHandle(*unbound);
  return ToApiHandle<Module>(i_isolate->factory()->NewSourceTextModule(shared));
}

namespace {
bool IsIdentifier(i::Isolate* isolate, i::Handle<i::String> string) {
  string = i::String::Flatten(isolate, string);
  const int length = string->length();
  if (length == 0) return false;
  if (!i::IsIdentifierStart(string->Get(0))) return false;
  i::DisallowHeapAllocation no_gc;
  i::String::FlatContent flat = string->GetFlatContent(no_gc);
  if (flat.IsOneByte()) {
    auto vector = flat.ToOneByteVector();
    for (int i = 1; i < length; i++) {
      if (!i::IsIdentifierPart(vector[i])) return false;
    }
  } else {
    auto vector = flat.ToUC16Vector();
    for (int i = 1; i < length; i++) {
      if (!i::IsIdentifierPart(vector[i])) return false;
    }
  }
  return true;
}
}  // anonymous namespace

MaybeLocal<Function> ScriptCompiler::CompileFunctionInContext(
    Local<Context> v8_context, Source* source, size_t arguments_count,
    Local<String> arguments[], size_t context_extension_count,
    Local<Object> context_extensions[], CompileOptions options,
    NoCacheReason no_cache_reason,
    Local<ScriptOrModule>* script_or_module_out) {
  Local<Function> result;

  {
    PREPARE_FOR_EXECUTION(v8_context, ScriptCompiler, CompileFunctionInContext,
                          Function);
    TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.ScriptCompiler");

    DCHECK(options == CompileOptions::kConsumeCodeCache ||
           options == CompileOptions::kEagerCompile ||
           options == CompileOptions::kNoCompileOptions);

    i::Handle<i::Context> context = Utils::OpenHandle(*v8_context);

    DCHECK(context->IsNativeContext());

    i::Handle<i::FixedArray> arguments_list =
        isolate->factory()->NewFixedArray(static_cast<int>(arguments_count));
    for (int i = 0; i < static_cast<int>(arguments_count); i++) {
      i::Handle<i::String> argument = Utils::OpenHandle(*arguments[i]);
      if (!IsIdentifier(isolate, argument)) return Local<Function>();
      arguments_list->set(i, *argument);
    }

    for (size_t i = 0; i < context_extension_count; ++i) {
      i::Handle<i::JSReceiver> extension =
          Utils::OpenHandle(*context_extensions[i]);
      if (!extension->IsJSObject()) return Local<Function>();
      context = isolate->factory()->NewWithContext(
          context,
          i::ScopeInfo::CreateForWithScope(
              isolate,
              context->IsNativeContext()
                  ? i::Handle<i::ScopeInfo>::null()
                  : i::Handle<i::ScopeInfo>(context->scope_info(), isolate)),
          extension);
    }

    i::Compiler::ScriptDetails script_details = GetScriptDetails(
        isolate, source->resource_name, source->resource_line_offset,
        source->resource_column_offset, source->source_map_url,
        source->host_defined_options);

    i::ScriptData* script_data = nullptr;
    if (options == kConsumeCodeCache) {
      DCHECK(source->cached_data);
      // ScriptData takes care of pointer-aligning the data.
      script_data = new i::ScriptData(source->cached_data->data,
                                      source->cached_data->length);
    }

    i::Handle<i::JSFunction> scoped_result;
    has_pending_exception =
        !i::Compiler::GetWrappedFunction(
             Utils::OpenHandle(*source->source_string), arguments_list, context,
             script_details, source->resource_options, script_data, options,
             no_cache_reason)
             .ToHandle(&scoped_result);
    if (options == kConsumeCodeCache) {
      source->cached_data->rejected = script_data->rejected();
    }
    delete script_data;
    RETURN_ON_FAILED_EXECUTION(Function);
    result = handle_scope.Escape(Utils::CallableToLocal(scoped_result));
  }

  if (script_or_module_out != nullptr) {
    i::Handle<i::JSFunction> function =
        i::Handle<i::JSFunction>::cast(Utils::OpenHandle(*result));
    i::Isolate* isolate = function->GetIsolate();
    i::Handle<i::SharedFunctionInfo> shared(function->shared(), isolate);
    i::Handle<i::Script> script(i::Script::cast(shared->script()), isolate);
    *script_or_module_out = v8::Utils::ScriptOrModuleToLocal(script);
  }

  return result;
}

void ScriptCompiler::ScriptStreamingTask::Run() { data_->task->Run(); }

ScriptCompiler::ScriptStreamingTask* ScriptCompiler::StartStreamingScript(
    Isolate* v8_isolate, StreamedSource* source, CompileOptions options) {
  if (!i::FLAG_script_streaming) {
    return nullptr;
  }
  // We don't support other compile options on streaming background compiles.
  // TODO(rmcilroy): remove CompileOptions from the API.
  CHECK(options == ScriptCompiler::kNoCompileOptions);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::ScriptStreamingData* data = source->impl();
  std::unique_ptr<i::BackgroundCompileTask> task =
      std::make_unique<i::BackgroundCompileTask>(data, isolate);
  data->task = std::move(task);
  return new ScriptCompiler::ScriptStreamingTask(data);
}

MaybeLocal<Script> ScriptCompiler::Compile(Local<Context> context,
                                           StreamedSource* v8_source,
                                           Local<String> full_source_string,
                                           const ScriptOrigin& origin) {
  PREPARE_FOR_EXECUTION(context, ScriptCompiler, Compile, Script);
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.ScriptCompiler");
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileStreamedScript");

  i::Handle<i::String> str = Utils::OpenHandle(*(full_source_string));
  i::Compiler::ScriptDetails script_details = GetScriptDetails(
      isolate, origin.ResourceName(), origin.ResourceLineOffset(),
      origin.ResourceColumnOffset(), origin.SourceMapUrl(),
      origin.HostDefinedOptions());
  i::ScriptStreamingData* data = v8_source->impl();

  i::MaybeHandle<i::SharedFunctionInfo> maybe_function_info =
      i::Compiler::GetSharedFunctionInfoForStreamedScript(
          isolate, str, script_details, origin.Options(), data);

  i::Handle<i::SharedFunctionInfo> result;
  has_pending_exception = !maybe_function_info.ToHandle(&result);
  if (has_pending_exception) isolate->ReportPendingMessages();

  RETURN_ON_FAILED_EXECUTION(Script);

  Local<UnboundScript> generic = ToApiHandle<UnboundScript>(result);
  if (generic.IsEmpty()) return Local<Script>();
  Local<Script> bound = generic->BindToCurrentContext();
  if (bound.IsEmpty()) return Local<Script>();
  RETURN_ESCAPED(bound);
}

uint32_t ScriptCompiler::CachedDataVersionTag() {
  return static_cast<uint32_t>(base::hash_combine(
      internal::Version::Hash(), internal::FlagList::Hash(),
      static_cast<uint32_t>(internal::CpuFeatures::SupportedFeatures())));
}

ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCache(
    Local<UnboundScript> unbound_script) {
  i::Handle<i::SharedFunctionInfo> shared =
      i::Handle<i::SharedFunctionInfo>::cast(
          Utils::OpenHandle(*unbound_script));
  DCHECK(shared->is_toplevel());
  return i::CodeSerializer::Serialize(shared);
}

// static
ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCache(
    Local<UnboundModuleScript> unbound_module_script) {
  i::Handle<i::SharedFunctionInfo> shared =
      i::Handle<i::SharedFunctionInfo>::cast(
          Utils::OpenHandle(*unbound_module_script));
  DCHECK(shared->is_toplevel());
  return i::CodeSerializer::Serialize(shared);
}

ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCacheForFunction(
    Local<Function> function) {
  auto js_function =
      i::Handle<i::JSFunction>::cast(Utils::OpenHandle(*function));
  i::Handle<i::SharedFunctionInfo> shared(js_function->shared(),
                                          js_function->GetIsolate());
  CHECK(shared->is_wrapped());
  return i::CodeSerializer::Serialize(shared);
}

MaybeLocal<Script> Script::Compile(Local<Context> context, Local<String> source,
                                   ScriptOrigin* origin) {
  if (origin) {
    ScriptCompiler::Source script_source(source, *origin);
    return ScriptCompiler::Compile(context, &script_source);
  }
  ScriptCompiler::Source script_source(source);
  return ScriptCompiler::Compile(context, &script_source);
}

// --- E x c e p t i o n s ---

v8::TryCatch::TryCatch(v8::Isolate* isolate)
    : isolate_(reinterpret_cast<i::Isolate*>(isolate)),
      next_(isolate_->try_catch_handler()),
      is_verbose_(false),
      can_continue_(true),
      capture_message_(true),
      rethrow_(false),
      has_terminated_(false) {
  ResetInternal();
  // Special handling for simulators which have a separate JS stack.
  js_stack_comparable_address_ = reinterpret_cast<void*>(
      i::SimulatorStack::RegisterJSStackComparableAddress(isolate_));
  isolate_->RegisterTryCatchHandler(this);
}

v8::TryCatch::~TryCatch() {
  if (rethrow_) {
    v8::Isolate* isolate = reinterpret_cast<Isolate*>(isolate_);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Value> exc = v8::Local<v8::Value>::New(isolate, Exception());
    if (HasCaught() && capture_message_) {
      // If an exception was caught and rethrow_ is indicated, the saved
      // message, script, and location need to be restored to Isolate TLS
      // for reuse.  capture_message_ needs to be disabled so that Throw()
      // does not create a new message.
      isolate_->thread_local_top()->rethrowing_message_ = true;
      isolate_->RestorePendingMessageFromTryCatch(this);
    }
    isolate_->UnregisterTryCatchHandler(this);
    i::SimulatorStack::UnregisterJSStackComparableAddress(isolate_);
    reinterpret_cast<Isolate*>(isolate_)->ThrowException(exc);
    DCHECK(!isolate_->thread_local_top()->rethrowing_message_);
  } else {
    if (HasCaught() && isolate_->has_scheduled_exception()) {
      // If an exception was caught but is still scheduled because no API call
      // promoted it, then it is canceled to prevent it from being propagated.
      // Note that this will not cancel termination exceptions.
      isolate_->CancelScheduledExceptionFromTryCatch(this);
    }
    isolate_->UnregisterTryCatchHandler(this);
    i::SimulatorStack::UnregisterJSStackComparableAddress(isolate_);
  }
}

void* v8::TryCatch::operator new(size_t) { base::OS::Abort(); }
void* v8::TryCatch::operator new[](size_t) { base::OS::Abort(); }
void v8::TryCatch::operator delete(void*, size_t) { base::OS::Abort(); }
void v8::TryCatch::operator delete[](void*, size_t) { base::OS::Abort(); }

bool v8::TryCatch::HasCaught() const {
  return !i::Object(reinterpret_cast<i::Address>(exception_))
              .IsTheHole(isolate_);
}

bool v8::TryCatch::CanContinue() const { return can_continue_; }

bool v8::TryCatch::HasTerminated() const { return has_terminated_; }

v8::Local<v8::Value> v8::TryCatch::ReThrow() {
  if (!HasCaught()) return v8::Local<v8::Value>();
  rethrow_ = true;
  return v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate_));
}

v8::Local<Value> v8::TryCatch::Exception() const {
  if (HasCaught()) {
    // Check for out of memory exception.
    i::Object exception(reinterpret_cast<i::Address>(exception_));
    return v8::Utils::ToLocal(i::Handle<i::Object>(exception, isolate_));
  } else {
    return v8::Local<Value>();
  }
}

MaybeLocal<Value> v8::TryCatch::StackTrace(Local<Context> context,
                                           Local<Value> exception) {
  i::Handle<i::Object> i_exception = Utils::OpenHandle(*exception);
  if (!i_exception->IsJSObject()) return v8::Local<Value>();
  PREPARE_FOR_EXECUTION(context, TryCatch, StackTrace, Value);
  auto obj = i::Handle<i::JSObject>::cast(i_exception);
  i::Handle<i::String> name = isolate->factory()->stack_string();
  Maybe<bool> maybe = i::JSReceiver::HasProperty(obj, name);
  has_pending_exception = maybe.IsNothing();
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!maybe.FromJust()) return v8::Local<Value>();
  Local<Value> result;
  has_pending_exception =
      !ToLocal<Value>(i::JSReceiver::GetProperty(isolate, obj, name), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<Value> v8::TryCatch::StackTrace(Local<Context> context) const {
  if (!HasCaught()) return v8::Local<Value>();
  return StackTrace(context, Exception());
}

v8::Local<v8::Message> v8::TryCatch::Message() const {
  i::Object message(reinterpret_cast<i::Address>(message_obj_));
  DCHECK(message.IsJSMessageObject() || message.IsTheHole(isolate_));
  if (HasCaught() && !message.IsTheHole(isolate_)) {
    return v8::Utils::MessageToLocal(i::Handle<i::Object>(message, isolate_));
  } else {
    return v8::Local<v8::Message>();
  }
}

void v8::TryCatch::Reset() {
  if (!rethrow_ && HasCaught() && isolate_->has_scheduled_exception()) {
    // If an exception was caught but is still scheduled because no API call
    // promoted it, then it is canceled to prevent it from being propagated.
    // Note that this will not cancel termination exceptions.
    isolate_->CancelScheduledExceptionFromTryCatch(this);
  }
  ResetInternal();
}

void v8::TryCatch::ResetInternal() {
  i::Object the_hole = i::ReadOnlyRoots(isolate_).the_hole_value();
  exception_ = reinterpret_cast<void*>(the_hole.ptr());
  message_obj_ = reinterpret_cast<void*>(the_hole.ptr());
}

void v8::TryCatch::SetVerbose(bool value) { is_verbose_ = value; }

bool v8::TryCatch::IsVerbose() const { return is_verbose_; }

void v8::TryCatch::SetCaptureMessage(bool value) { capture_message_ = value; }

// --- M e s s a g e ---

Local<String> Message::Get() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::String> raw_result =
      i::MessageHandler::GetMessage(isolate, self);
  Local<String> result = Utils::ToLocal(raw_result);
  return scope.Escape(result);
}

v8::Isolate* Message::GetIsolate() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  return reinterpret_cast<Isolate*>(isolate);
}

ScriptOrigin Message::GetScriptOrigin() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Script> script(self->script(), isolate);
  return GetScriptOriginForScript(isolate, script);
}

v8::Local<Value> Message::GetScriptResourceName() const {
  return GetScriptOrigin().ResourceName();
}

v8::Local<v8::StackTrace> Message::GetStackTrace() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::Object> stackFramesObj(self->stack_frames(), isolate);
  if (!stackFramesObj->IsFixedArray()) return v8::Local<v8::StackTrace>();
  auto stackTrace = i::Handle<i::FixedArray>::cast(stackFramesObj);
  return scope.Escape(Utils::StackTraceToLocal(stackTrace));
}

Maybe<int> Message::GetLineNumber(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(isolate, self);
  return Just(self->GetLineNumber());
}

int Message::GetStartPosition() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(isolate, self);
  return self->GetStartPosition();
}

int Message::GetEndPosition() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(isolate, self);
  return self->GetEndPosition();
}

int Message::ErrorLevel() const {
  auto self = Utils::OpenHandle(this);
  return self->error_level();
}

int Message::GetStartColumn() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(isolate, self);
  return self->GetColumnNumber();
}

int Message::GetWasmFunctionIndex() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(isolate, self);
  int start_position = self->GetColumnNumber();
  if (start_position == -1) return Message::kNoWasmFunctionIndexInfo;

  i::Handle<i::Script> script(self->script(), isolate);

  if (script->type() != i::Script::TYPE_WASM) {
    return Message::kNoWasmFunctionIndexInfo;
  }

  auto debug_script = ToApiHandle<debug::Script>(script);
  return Local<debug::WasmScript>::Cast(debug_script)
      ->GetContainingFunction(start_position);
}

Maybe<int> Message::GetStartColumn(Local<Context> context) const {
  return Just(GetStartColumn());
}

int Message::GetEndColumn() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(isolate, self);
  const int column_number = self->GetColumnNumber();
  if (column_number == -1) return -1;
  const int start = self->GetStartPosition();
  const int end = self->GetEndPosition();
  return column_number + (end - start);
}

Maybe<int> Message::GetEndColumn(Local<Context> context) const {
  return Just(GetEndColumn());
}

bool Message::IsSharedCrossOrigin() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  return self->script().origin_options().IsSharedCrossOrigin();
}

bool Message::IsOpaque() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  return self->script().origin_options().IsOpaque();
}

MaybeLocal<String> Message::GetSourceLine(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(isolate, self);
  RETURN_ESCAPED(Utils::ToLocal(self->GetSourceLine()));
}

void Message::PrintCurrentStackTrace(Isolate* isolate, FILE* out) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i_isolate->PrintCurrentStackTrace(out);
}

// --- S t a c k T r a c e ---

Local<StackFrame> StackTrace::GetFrame(Isolate* v8_isolate,
                                       uint32_t index) const {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope scope(v8_isolate);
  auto obj = handle(Utils::OpenHandle(this)->get(index), isolate);
  auto frame = i::Handle<i::StackTraceFrame>::cast(obj);
  return scope.Escape(Utils::StackFrameToLocal(frame));
}

int StackTrace::GetFrameCount() const {
  return Utils::OpenHandle(this)->length();
}

Local<StackTrace> StackTrace::CurrentStackTrace(Isolate* isolate,
                                                int frame_limit,
                                                StackTraceOptions options) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::FixedArray> stackTrace =
      i_isolate->CaptureCurrentStackTrace(frame_limit, options);
  return Utils::StackTraceToLocal(stackTrace);
}

// --- S t a c k F r a m e ---

int StackFrame::GetLineNumber() const {
  return i::StackTraceFrame::GetOneBasedLineNumber(Utils::OpenHandle(this));
}

int StackFrame::GetColumn() const {
  return i::StackTraceFrame::GetOneBasedColumnNumber(Utils::OpenHandle(this));
}

int StackFrame::GetScriptId() const {
  return i::StackTraceFrame::GetScriptId(Utils::OpenHandle(this));
}

Local<String> StackFrame::GetScriptName() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::Object> name = i::StackTraceFrame::GetFileName(self);
  return name->IsString()
             ? scope.Escape(Local<String>::Cast(Utils::ToLocal(name)))
             : Local<String>();
}

Local<String> StackFrame::GetScriptNameOrSourceURL() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::Object> name =
      i::StackTraceFrame::GetScriptNameOrSourceUrl(self);
  return name->IsString()
             ? scope.Escape(Local<String>::Cast(Utils::ToLocal(name)))
             : Local<String>();
}

Local<String> StackFrame::GetFunctionName() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::Object> name = i::StackTraceFrame::GetFunctionName(self);
  return name->IsString()
             ? scope.Escape(Local<String>::Cast(Utils::ToLocal(name)))
             : Local<String>();
}

bool StackFrame::IsEval() const {
  return i::StackTraceFrame::IsEval(Utils::OpenHandle(this));
}

bool StackFrame::IsConstructor() const {
  return i::StackTraceFrame::IsConstructor(Utils::OpenHandle(this));
}

bool StackFrame::IsWasm() const {
  return i::StackTraceFrame::IsWasm(Utils::OpenHandle(this));
}

bool StackFrame::IsUserJavaScript() const {
  return i::StackTraceFrame::IsUserJavaScript(Utils::OpenHandle(this));
}

// --- J S O N ---

MaybeLocal<Value> JSON::Parse(Local<Context> context,
                              Local<String> json_string) {
  PREPARE_FOR_EXECUTION(context, JSON, Parse, Value);
  i::Handle<i::String> string = Utils::OpenHandle(*json_string);
  i::Handle<i::String> source = i::String::Flatten(isolate, string);
  i::Handle<i::Object> undefined = isolate->factory()->undefined_value();
  auto maybe = source->IsOneByteRepresentation()
                   ? i::JsonParser<uint8_t>::Parse(isolate, source, undefined)
                   : i::JsonParser<uint16_t>::Parse(isolate, source, undefined);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(maybe, &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<String> JSON::Stringify(Local<Context> context,
                                   Local<Value> json_object,
                                   Local<String> gap) {
  PREPARE_FOR_EXECUTION(context, JSON, Stringify, String);
  i::Handle<i::Object> object = Utils::OpenHandle(*json_object);
  i::Handle<i::Object> replacer = isolate->factory()->undefined_value();
  i::Handle<i::String> gap_string = gap.IsEmpty()
                                        ? isolate->factory()->empty_string()
                                        : Utils::OpenHandle(*gap);
  i::Handle<i::Object> maybe;
  has_pending_exception =
      !i::JsonStringify(isolate, object, replacer, gap_string).ToHandle(&maybe);
  RETURN_ON_FAILED_EXECUTION(String);
  Local<String> result;
  has_pending_exception =
      !ToLocal<String>(i::Object::ToString(isolate, maybe), &result);
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(result);
}

// --- V a l u e   S e r i a l i z a t i o n ---

Maybe<bool> ValueSerializer::Delegate::WriteHostObject(Isolate* v8_isolate,
                                                       Local<Object> object) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->ScheduleThrow(*isolate->factory()->NewError(
      isolate->error_function(), i::MessageTemplate::kDataCloneError,
      Utils::OpenHandle(*object)));
  return Nothing<bool>();
}

Maybe<uint32_t> ValueSerializer::Delegate::GetSharedArrayBufferId(
    Isolate* v8_isolate, Local<SharedArrayBuffer> shared_array_buffer) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->ScheduleThrow(*isolate->factory()->NewError(
      isolate->error_function(), i::MessageTemplate::kDataCloneError,
      Utils::OpenHandle(*shared_array_buffer)));
  return Nothing<uint32_t>();
}

Maybe<uint32_t> ValueSerializer::Delegate::GetWasmModuleTransferId(
    Isolate* v8_isolate, Local<WasmModuleObject> module) {
  return Nothing<uint32_t>();
}

void* ValueSerializer::Delegate::ReallocateBufferMemory(void* old_buffer,
                                                        size_t size,
                                                        size_t* actual_size) {
  *actual_size = size;
  return realloc(old_buffer, size);
}

void ValueSerializer::Delegate::FreeBufferMemory(void* buffer) {
  return free(buffer);
}

struct ValueSerializer::PrivateData {
  explicit PrivateData(i::Isolate* i, ValueSerializer::Delegate* delegate)
      : isolate(i), serializer(i, delegate) {}
  i::Isolate* isolate;
  i::ValueSerializer serializer;
};

ValueSerializer::ValueSerializer(Isolate* isolate)
    : ValueSerializer(isolate, nullptr) {}

ValueSerializer::ValueSerializer(Isolate* isolate, Delegate* delegate)
    : private_(
          new PrivateData(reinterpret_cast<i::Isolate*>(isolate), delegate)) {}

ValueSerializer::~ValueSerializer() { delete private_; }

void ValueSerializer::WriteHeader() { private_->serializer.WriteHeader(); }

void ValueSerializer::SetTreatArrayBufferViewsAsHostObjects(bool mode) {
  private_->serializer.SetTreatArrayBufferViewsAsHostObjects(mode);
}

Maybe<bool> ValueSerializer::WriteValue(Local<Context> context,
                                        Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, ValueSerializer, WriteValue, Nothing<bool>(),
           i::HandleScope);
  i::Handle<i::Object> object = Utils::OpenHandle(*value);
  Maybe<bool> result = private_->serializer.WriteObject(object);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

std::pair<uint8_t*, size_t> ValueSerializer::Release() {
  return private_->serializer.Release();
}

void ValueSerializer::TransferArrayBuffer(uint32_t transfer_id,
                                          Local<ArrayBuffer> array_buffer) {
  private_->serializer.TransferArrayBuffer(transfer_id,
                                           Utils::OpenHandle(*array_buffer));
}

void ValueSerializer::WriteUint32(uint32_t value) {
  private_->serializer.WriteUint32(value);
}

void ValueSerializer::WriteUint64(uint64_t value) {
  private_->serializer.WriteUint64(value);
}

void ValueSerializer::WriteDouble(double value) {
  private_->serializer.WriteDouble(value);
}

void ValueSerializer::WriteRawBytes(const void* source, size_t length) {
  private_->serializer.WriteRawBytes(source, length);
}

MaybeLocal<Object> ValueDeserializer::Delegate::ReadHostObject(
    Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->ScheduleThrow(*isolate->factory()->NewError(
      isolate->error_function(),
      i::MessageTemplate::kDataCloneDeserializationError));
  return MaybeLocal<Object>();
}

MaybeLocal<WasmModuleObject> ValueDeserializer::Delegate::GetWasmModuleFromId(
    Isolate* v8_isolate, uint32_t id) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->ScheduleThrow(*isolate->factory()->NewError(
      isolate->error_function(),
      i::MessageTemplate::kDataCloneDeserializationError));
  return MaybeLocal<WasmModuleObject>();
}

MaybeLocal<SharedArrayBuffer>
ValueDeserializer::Delegate::GetSharedArrayBufferFromId(Isolate* v8_isolate,
                                                        uint32_t id) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->ScheduleThrow(*isolate->factory()->NewError(
      isolate->error_function(),
      i::MessageTemplate::kDataCloneDeserializationError));
  return MaybeLocal<SharedArrayBuffer>();
}

struct ValueDeserializer::PrivateData {
  PrivateData(i::Isolate* i, i::Vector<const uint8_t> data, Delegate* delegate)
      : isolate(i), deserializer(i, data, delegate) {}
  i::Isolate* isolate;
  i::ValueDeserializer deserializer;
  bool has_aborted = false;
  bool supports_legacy_wire_format = false;
};

ValueDeserializer::ValueDeserializer(Isolate* isolate, const uint8_t* data,
                                     size_t size)
    : ValueDeserializer(isolate, data, size, nullptr) {}

ValueDeserializer::ValueDeserializer(Isolate* isolate, const uint8_t* data,
                                     size_t size, Delegate* delegate) {
  if (base::IsValueInRangeForNumericType<int>(size)) {
    private_ = new PrivateData(
        reinterpret_cast<i::Isolate*>(isolate),
        i::Vector<const uint8_t>(data, static_cast<int>(size)), delegate);
  } else {
    private_ = new PrivateData(reinterpret_cast<i::Isolate*>(isolate),
                               i::Vector<const uint8_t>(nullptr, 0), nullptr);
    private_->has_aborted = true;
  }
}

ValueDeserializer::~ValueDeserializer() { delete private_; }

Maybe<bool> ValueDeserializer::ReadHeader(Local<Context> context) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, ValueDeserializer, ReadHeader,
                     Nothing<bool>(), i::HandleScope);

  // We could have aborted during the constructor.
  // If so, ReadHeader is where we report it.
  if (private_->has_aborted) {
    isolate->Throw(*isolate->factory()->NewError(
        i::MessageTemplate::kDataCloneDeserializationError));
    has_pending_exception = true;
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  }

  bool read_header = false;
  has_pending_exception = !private_->deserializer.ReadHeader().To(&read_header);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  DCHECK(read_header);

  static const uint32_t kMinimumNonLegacyVersion = 13;
  if (GetWireFormatVersion() < kMinimumNonLegacyVersion &&
      !private_->supports_legacy_wire_format) {
    isolate->Throw(*isolate->factory()->NewError(
        i::MessageTemplate::kDataCloneDeserializationVersionError));
    has_pending_exception = true;
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  }

  return Just(true);
}

void ValueDeserializer::SetSupportsLegacyWireFormat(
    bool supports_legacy_wire_format) {
  private_->supports_legacy_wire_format = supports_legacy_wire_format;
}

uint32_t ValueDeserializer::GetWireFormatVersion() const {
  CHECK(!private_->has_aborted);
  return private_->deserializer.GetWireFormatVersion();
}

MaybeLocal<Value> ValueDeserializer::ReadValue(Local<Context> context) {
  CHECK(!private_->has_aborted);
  PREPARE_FOR_EXECUTION(context, ValueDeserializer, ReadValue, Value);
  i::MaybeHandle<i::Object> result;
  if (GetWireFormatVersion() > 0) {
    result = private_->deserializer.ReadObject();
  } else {
    result =
        private_->deserializer.ReadObjectUsingEntireBufferForLegacyFormat();
  }
  Local<Value> value;
  has_pending_exception = !ToLocal(result, &value);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(value);
}

void ValueDeserializer::TransferArrayBuffer(uint32_t transfer_id,
                                            Local<ArrayBuffer> array_buffer) {
  CHECK(!private_->has_aborted);
  private_->deserializer.TransferArrayBuffer(transfer_id,
                                             Utils::OpenHandle(*array_buffer));
}

void ValueDeserializer::TransferSharedArrayBuffer(
    uint32_t transfer_id, Local<SharedArrayBuffer> shared_array_buffer) {
  CHECK(!private_->has_aborted);
  private_->deserializer.TransferArrayBuffer(
      transfer_id, Utils::OpenHandle(*shared_array_buffer));
}

bool ValueDeserializer::ReadUint32(uint32_t* value) {
  return private_->deserializer.ReadUint32(value);
}

bool ValueDeserializer::ReadUint64(uint64_t* value) {
  return private_->deserializer.ReadUint64(value);
}

bool ValueDeserializer::ReadDouble(double* value) {
  return private_->deserializer.ReadDouble(value);
}

bool ValueDeserializer::ReadRawBytes(size_t length, const void** data) {
  return private_->deserializer.ReadRawBytes(length, data);
}

// --- D a t a ---

bool Value::FullIsUndefined() const {
  i::Handle<i::Object> object = Utils::OpenHandle(this);
  bool result = object->IsUndefined();
  DCHECK_EQ(result, QuickIsUndefined());
  return result;
}

bool Value::FullIsNull() const {
  i::Handle<i::Object> object = Utils::OpenHandle(this);
  bool result = object->IsNull();
  DCHECK_EQ(result, QuickIsNull());
  return result;
}

bool Value::IsTrue() const {
  i::Handle<i::Object> object = Utils::OpenHandle(this);
  if (object->IsSmi()) return false;
  return object->IsTrue();
}

bool Value::IsFalse() const {
  i::Handle<i::Object> object = Utils::OpenHandle(this);
  if (object->IsSmi()) return false;
  return object->IsFalse();
}

bool Value::IsFunction() const { return Utils::OpenHandle(this)->IsCallable(); }

bool Value::IsName() const { return Utils::OpenHandle(this)->IsName(); }

bool Value::FullIsString() const {
  bool result = Utils::OpenHandle(this)->IsString();
  DCHECK_EQ(result, QuickIsString());
  return result;
}

bool Value::IsSymbol() const { return Utils::OpenHandle(this)->IsSymbol(); }

bool Value::IsArray() const { return Utils::OpenHandle(this)->IsJSArray(); }

bool Value::IsArrayBuffer() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsJSArrayBuffer() && !i::JSArrayBuffer::cast(*obj).is_shared();
}

bool Value::IsArrayBufferView() const {
  return Utils::OpenHandle(this)->IsJSArrayBufferView();
}

bool Value::IsTypedArray() const {
  return Utils::OpenHandle(this)->IsJSTypedArray();
}

#define VALUE_IS_TYPED_ARRAY(Type, typeName, TYPE, ctype)                   \
  bool Value::Is##Type##Array() const {                                     \
    i::Handle<i::Object> obj = Utils::OpenHandle(this);                     \
    return obj->IsJSTypedArray() &&                                         \
           i::JSTypedArray::cast(*obj).type() == i::kExternal##Type##Array; \
  }

TYPED_ARRAYS(VALUE_IS_TYPED_ARRAY)

#undef VALUE_IS_TYPED_ARRAY

bool Value::IsDataView() const {
  return Utils::OpenHandle(this)->IsJSDataView();
}

bool Value::IsSharedArrayBuffer() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsJSArrayBuffer() && i::JSArrayBuffer::cast(*obj).is_shared();
}

bool Value::IsObject() const { return Utils::OpenHandle(this)->IsJSReceiver(); }

bool Value::IsNumber() const { return Utils::OpenHandle(this)->IsNumber(); }

bool Value::IsBigInt() const { return Utils::OpenHandle(this)->IsBigInt(); }

bool Value::IsProxy() const { return Utils::OpenHandle(this)->IsJSProxy(); }

#define VALUE_IS_SPECIFIC_TYPE(Type, Check)             \
  bool Value::Is##Type() const {                        \
    i::Handle<i::Object> obj = Utils::OpenHandle(this); \
    return obj->Is##Check();                            \
  }

VALUE_IS_SPECIFIC_TYPE(ArgumentsObject, JSArgumentsObject)
VALUE_IS_SPECIFIC_TYPE(BigIntObject, BigIntWrapper)
VALUE_IS_SPECIFIC_TYPE(BooleanObject, BooleanWrapper)
VALUE_IS_SPECIFIC_TYPE(NumberObject, NumberWrapper)
VALUE_IS_SPECIFIC_TYPE(StringObject, StringWrapper)
VALUE_IS_SPECIFIC_TYPE(SymbolObject, SymbolWrapper)
VALUE_IS_SPECIFIC_TYPE(Date, JSDate)
VALUE_IS_SPECIFIC_TYPE(Map, JSMap)
VALUE_IS_SPECIFIC_TYPE(Set, JSSet)
VALUE_IS_SPECIFIC_TYPE(WasmModuleObject, WasmModuleObject)
VALUE_IS_SPECIFIC_TYPE(WeakMap, JSWeakMap)
VALUE_IS_SPECIFIC_TYPE(WeakSet, JSWeakSet)

#undef VALUE_IS_SPECIFIC_TYPE

bool Value::IsBoolean() const { return Utils::OpenHandle(this)->IsBoolean(); }

bool Value::IsExternal() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (!obj->IsHeapObject()) return false;
  i::Handle<i::HeapObject> heap_obj = i::Handle<i::HeapObject>::cast(obj);
  // Check the instance type is JS_OBJECT (instance type of Externals) before
  // attempting to get the Isolate since that guarantees the object is writable
  // and GetIsolate will work.
  if (heap_obj->map().instance_type() != i::JS_OBJECT_TYPE) return false;
  i::Isolate* isolate = i::JSObject::cast(*heap_obj).GetIsolate();
  return heap_obj->IsExternal(isolate);
}

bool Value::IsInt32() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return true;
  if (obj->IsNumber()) {
    return i::IsInt32Double(obj->Number());
  }
  return false;
}

bool Value::IsUint32() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return i::Smi::ToInt(*obj) >= 0;
  if (obj->IsNumber()) {
    double value = obj->Number();
    return !i::IsMinusZero(value) && value >= 0 && value <= i::kMaxUInt32 &&
           value == i::FastUI2D(i::FastD2UI(value));
  }
  return false;
}

bool Value::IsNativeError() const {
  return Utils::OpenHandle(this)->IsJSError();
}

bool Value::IsRegExp() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsJSRegExp();
}

bool Value::IsAsyncFunction() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (!obj->IsJSFunction()) return false;
  i::Handle<i::JSFunction> func = i::Handle<i::JSFunction>::cast(obj);
  return i::IsAsyncFunction(func->shared().kind());
}

bool Value::IsGeneratorFunction() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (!obj->IsJSFunction()) return false;
  i::Handle<i::JSFunction> func = i::Handle<i::JSFunction>::cast(obj);
  return i::IsGeneratorFunction(func->shared().kind());
}

bool Value::IsGeneratorObject() const {
  return Utils::OpenHandle(this)->IsJSGeneratorObject();
}

bool Value::IsMapIterator() const {
  return Utils::OpenHandle(this)->IsJSMapIterator();
}

bool Value::IsSetIterator() const {
  return Utils::OpenHandle(this)->IsJSSetIterator();
}

bool Value::IsPromise() const { return Utils::OpenHandle(this)->IsJSPromise(); }

bool Value::IsModuleNamespaceObject() const {
  return Utils::OpenHandle(this)->IsJSModuleNamespace();
}

MaybeLocal<String> Value::ToString(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsString()) return ToApiHandle<String>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToString, String);
  Local<String> result;
  has_pending_exception =
      !ToLocal<String>(i::Object::ToString(isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(result);
}

MaybeLocal<String> Value::ToDetailString(Local<Context> context) const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsString()) return ToApiHandle<String>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToDetailString, String);
  Local<String> result =
      Utils::ToLocal(i::Object::NoSideEffectsToString(isolate, obj));
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(result);
}

MaybeLocal<Object> Value::ToObject(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsJSReceiver()) return ToApiHandle<Object>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToObject, Object);
  Local<Object> result;
  has_pending_exception =
      !ToLocal<Object>(i::Object::ToObject(isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}

MaybeLocal<BigInt> Value::ToBigInt(Local<Context> context) const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsBigInt()) return ToApiHandle<BigInt>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToBigInt, BigInt);
  Local<BigInt> result;
  has_pending_exception =
      !ToLocal<BigInt>(i::BigInt::FromObject(isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(BigInt);
  RETURN_ESCAPED(result);
}

bool Value::BooleanValue(Isolate* v8_isolate) const {
  return Utils::OpenHandle(this)->BooleanValue(
      reinterpret_cast<i::Isolate*>(v8_isolate));
}

Local<Boolean> Value::ToBoolean(Isolate* v8_isolate) const {
  auto isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  return ToApiHandle<Boolean>(
      isolate->factory()->ToBoolean(BooleanValue(v8_isolate)));
}

MaybeLocal<Number> Value::ToNumber(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return ToApiHandle<Number>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToNumber, Number);
  Local<Number> result;
  has_pending_exception =
      !ToLocal<Number>(i::Object::ToNumber(isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Number);
  RETURN_ESCAPED(result);
}

MaybeLocal<Integer> Value::ToInteger(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return ToApiHandle<Integer>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToInteger, Integer);
  Local<Integer> result;
  has_pending_exception =
      !ToLocal<Integer>(i::Object::ToInteger(isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Integer);
  RETURN_ESCAPED(result);
}

MaybeLocal<Int32> Value::ToInt32(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return ToApiHandle<Int32>(obj);
  Local<Int32> result;
  PREPARE_FOR_EXECUTION(context, Object, ToInt32, Int32);
  has_pending_exception =
      !ToLocal<Int32>(i::Object::ToInt32(isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Int32);
  RETURN_ESCAPED(result);
}

MaybeLocal<Uint32> Value::ToUint32(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return ToApiHandle<Uint32>(obj);
  Local<Uint32> result;
  PREPARE_FOR_EXECUTION(context, Object, ToUint32, Uint32);
  has_pending_exception =
      !ToLocal<Uint32>(i::Object::ToUint32(isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Uint32);
  RETURN_ESCAPED(result);
}

i::Isolate* i::IsolateFromNeverReadOnlySpaceObject(i::Address obj) {
  return i::GetIsolateFromWritableObject(i::HeapObject::cast(i::Object(obj)));
}

bool i::ShouldThrowOnError(i::Isolate* isolate) {
  return i::GetShouldThrow(isolate, Nothing<i::ShouldThrow>()) ==
         i::ShouldThrow::kThrowOnError;
}

void i::Internals::CheckInitializedImpl(v8::Isolate* external_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  Utils::ApiCheck(isolate != nullptr && !isolate->IsDead(),
                  "v8::internal::Internals::CheckInitialized",
                  "Isolate is not initialized or V8 has died");
}

void External::CheckCast(v8::Value* that) {
  Utils::ApiCheck(that->IsExternal(), "v8::External::Cast",
                  "Value is not an External");
}

void v8::Object::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSReceiver(), "v8::Object::Cast",
                  "Value is not an Object");
}

void v8::Function::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsCallable(), "v8::Function::Cast",
                  "Value is not a Function");
}

void v8::Boolean::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsBoolean(), "v8::Boolean::Cast",
                  "Value is not a Boolean");
}

void v8::Name::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsName(), "v8::Name::Cast", "Value is not a Name");
}

void v8::String::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsString(), "v8::String::Cast", "Value is not a String");
}

void v8::Symbol::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsSymbol(), "v8::Symbol::Cast", "Value is not a Symbol");
}

void v8::Private::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(
      obj->IsSymbol() && i::Handle<i::Symbol>::cast(obj)->is_private(),
      "v8::Private::Cast", "Value is not a Private");
}

void v8::Number::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsNumber(), "v8::Number::Cast()",
                  "Value is not a Number");
}

void v8::Integer::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsNumber(), "v8::Integer::Cast",
                  "Value is not an Integer");
}

void v8::Int32::CheckCast(v8::Value* that) {
  Utils::ApiCheck(that->IsInt32(), "v8::Int32::Cast",
                  "Value is not a 32-bit signed integer");
}

void v8::Uint32::CheckCast(v8::Value* that) {
  Utils::ApiCheck(that->IsUint32(), "v8::Uint32::Cast",
                  "Value is not a 32-bit unsigned integer");
}

void v8::BigInt::CheckCast(v8::Value* that) {
  Utils::ApiCheck(that->IsBigInt(), "v8::BigInt::Cast",
                  "Value is not a BigInt");
}

void v8::Array::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSArray(), "v8::Array::Cast", "Value is not an Array");
}

void v8::Map::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSMap(), "v8::Map::Cast", "Value is not a Map");
}

void v8::Set::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSSet(), "v8_Set_Cast", "Value is not a Set");
}

void v8::Promise::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsPromise(), "v8::Promise::Cast",
                  "Value is not a Promise");
}

void v8::Promise::Resolver::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsPromise(), "v8::Promise::Resolver::Cast",
                  "Value is not a Promise::Resolver");
}

void v8::Proxy::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsProxy(), "v8::Proxy::Cast", "Value is not a Proxy");
}

void v8::WasmModuleObject::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsWasmModuleObject(), "v8::WasmModuleObject::Cast",
                  "Value is not a WasmModuleObject");
}

void v8::debug::AccessorPair::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsAccessorPair(), "v8::AccessorPair::Cast",
                  "Value is not a debug::AccessorPair");
}

v8::BackingStore::~BackingStore() {
  auto i_this = reinterpret_cast<const i::BackingStore*>(this);
  i_this->~BackingStore();  // manually call internal destructor
}

void* v8::BackingStore::Data() const {
  return reinterpret_cast<const i::BackingStore*>(this)->buffer_start();
}

size_t v8::BackingStore::ByteLength() const {
  return reinterpret_cast<const i::BackingStore*>(this)->byte_length();
}

bool v8::BackingStore::IsShared() const {
  return reinterpret_cast<const i::BackingStore*>(this)->is_shared();
}

// static
std::unique_ptr<v8::BackingStore> v8::BackingStore::Reallocate(
    v8::Isolate* isolate, std::unique_ptr<v8::BackingStore> backing_store,
    size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, ArrayBuffer, BackingStore_Reallocate);
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::BackingStore* i_backing_store =
      reinterpret_cast<i::BackingStore*>(backing_store.get());
  if (!i_backing_store->Reallocate(i_isolate, byte_length)) {
    i::FatalProcessOutOfMemory(i_isolate, "v8::BackingStore::Reallocate");
  }
  return backing_store;
}

std::shared_ptr<v8::BackingStore> v8::ArrayBuffer::GetBackingStore() {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  std::shared_ptr<i::BackingStore> backing_store = self->GetBackingStore();
  if (!backing_store) {
    backing_store =
        i::BackingStore::EmptyBackingStore(i::SharedFlag::kNotShared);
  }
  i::GlobalBackingStoreRegistry::Register(backing_store);
  std::shared_ptr<i::BackingStoreBase> bs_base = backing_store;
  return std::static_pointer_cast<v8::BackingStore>(bs_base);
}

std::shared_ptr<v8::BackingStore> v8::SharedArrayBuffer::GetBackingStore() {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  std::shared_ptr<i::BackingStore> backing_store = self->GetBackingStore();
  if (!backing_store) {
    backing_store = i::BackingStore::EmptyBackingStore(i::SharedFlag::kShared);
  }
  i::GlobalBackingStoreRegistry::Register(backing_store);
  std::shared_ptr<i::BackingStoreBase> bs_base = backing_store;
  return std::static_pointer_cast<v8::BackingStore>(bs_base);
}

void v8::ArrayBuffer::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(
      obj->IsJSArrayBuffer() && !i::JSArrayBuffer::cast(*obj).is_shared(),
      "v8::ArrayBuffer::Cast()", "Value is not an ArrayBuffer");
}

void v8::ArrayBufferView::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSArrayBufferView(), "v8::ArrayBufferView::Cast()",
                  "Value is not an ArrayBufferView");
}

constexpr size_t v8::TypedArray::kMaxLength;

void v8::TypedArray::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSTypedArray(), "v8::TypedArray::Cast()",
                  "Value is not a TypedArray");
}

#define CHECK_TYPED_ARRAY_CAST(Type, typeName, TYPE, ctype)                  \
  void v8::Type##Array::CheckCast(Value* that) {                             \
    i::Handle<i::Object> obj = Utils::OpenHandle(that);                      \
    Utils::ApiCheck(                                                         \
        obj->IsJSTypedArray() &&                                             \
            i::JSTypedArray::cast(*obj).type() == i::kExternal##Type##Array, \
        "v8::" #Type "Array::Cast()", "Value is not a " #Type "Array");      \
  }

TYPED_ARRAYS(CHECK_TYPED_ARRAY_CAST)

#undef CHECK_TYPED_ARRAY_CAST

void v8::DataView::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSDataView(), "v8::DataView::Cast()",
                  "Value is not a DataView");
}

void v8::SharedArrayBuffer::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(
      obj->IsJSArrayBuffer() && i::JSArrayBuffer::cast(*obj).is_shared(),
      "v8::SharedArrayBuffer::Cast()", "Value is not a SharedArrayBuffer");
}

void v8::Date::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSDate(), "v8::Date::Cast()", "Value is not a Date");
}

void v8::StringObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsStringWrapper(), "v8::StringObject::Cast()",
                  "Value is not a StringObject");
}

void v8::SymbolObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsSymbolWrapper(), "v8::SymbolObject::Cast()",
                  "Value is not a SymbolObject");
}

void v8::NumberObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsNumberWrapper(), "v8::NumberObject::Cast()",
                  "Value is not a NumberObject");
}

void v8::BigIntObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsBigIntWrapper(), "v8::BigIntObject::Cast()",
                  "Value is not a BigIntObject");
}

void v8::BooleanObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsBooleanWrapper(), "v8::BooleanObject::Cast()",
                  "Value is not a BooleanObject");
}

void v8::RegExp::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSRegExp(), "v8::RegExp::Cast()",
                  "Value is not a RegExp");
}

Maybe<double> Value::NumberValue(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return Just(obj->Number());
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Value, NumberValue, Nothing<double>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToNumber(isolate, obj).ToHandle(&num);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(double);
  return Just(num->Number());
}

Maybe<int64_t> Value::IntegerValue(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) {
    return Just(NumberToInt64(*obj));
  }
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Value, IntegerValue, Nothing<int64_t>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToInteger(isolate, obj).ToHandle(&num);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(int64_t);
  return Just(NumberToInt64(*num));
}

Maybe<int32_t> Value::Int32Value(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return Just(NumberToInt32(*obj));
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Value, Int32Value, Nothing<int32_t>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToInt32(isolate, obj).ToHandle(&num);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(int32_t);
  return Just(num->IsSmi() ? i::Smi::ToInt(*num)
                           : static_cast<int32_t>(num->Number()));
}

Maybe<uint32_t> Value::Uint32Value(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return Just(NumberToUint32(*obj));
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Value, Uint32Value, Nothing<uint32_t>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToUint32(isolate, obj).ToHandle(&num);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(uint32_t);
  return Just(num->IsSmi() ? static_cast<uint32_t>(i::Smi::ToInt(*num))
                           : static_cast<uint32_t>(num->Number()));
}

MaybeLocal<Uint32> Value::ToArrayIndex(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  if (self->IsSmi()) {
    if (i::Smi::ToInt(*self) >= 0) return Utils::Uint32ToLocal(self);
    return Local<Uint32>();
  }
  PREPARE_FOR_EXECUTION(context, Object, ToArrayIndex, Uint32);
  i::Handle<i::Object> string_obj;
  has_pending_exception =
      !i::Object::ToString(isolate, self).ToHandle(&string_obj);
  RETURN_ON_FAILED_EXECUTION(Uint32);
  i::Handle<i::String> str = i::Handle<i::String>::cast(string_obj);
  uint32_t index;
  if (str->AsArrayIndex(&index)) {
    i::Handle<i::Object> value;
    if (index <= static_cast<uint32_t>(i::Smi::kMaxValue)) {
      value = i::Handle<i::Object>(i::Smi::FromInt(index), isolate);
    } else {
      value = isolate->factory()->NewNumber(index);
    }
    RETURN_ESCAPED(Utils::Uint32ToLocal(value));
  }
  return Local<Uint32>();
}

Maybe<bool> Value::Equals(Local<Context> context, Local<Value> that) const {
  i::Isolate* isolate = Utils::OpenHandle(*context)->GetIsolate();
  auto self = Utils::OpenHandle(this);
  auto other = Utils::OpenHandle(*that);
  return i::Object::Equals(isolate, self, other);
}

bool Value::StrictEquals(Local<Value> that) const {
  auto self = Utils::OpenHandle(this);
  auto other = Utils::OpenHandle(*that);
  return self->StrictEquals(*other);
}

bool Value::SameValue(Local<Value> that) const {
  auto self = Utils::OpenHandle(this);
  auto other = Utils::OpenHandle(*that);
  return self->SameValue(*other);
}

Local<String> Value::TypeOf(v8::Isolate* external_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  LOG_API(isolate, Value, TypeOf);
  return Utils::ToLocal(i::Object::TypeOf(isolate, Utils::OpenHandle(this)));
}

Maybe<bool> Value::InstanceOf(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> object) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Value, InstanceOf, Nothing<bool>(),
           i::HandleScope);
  auto left = Utils::OpenHandle(this);
  auto right = Utils::OpenHandle(*object);
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::Object::InstanceOf(isolate, left, right).ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(isolate));
}

Maybe<bool> v8::Object::Set(v8::Local<v8::Context> context,
                            v8::Local<Value> key, v8::Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, Set, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  auto value_obj = Utils::OpenHandle(*value);
  has_pending_exception =
      i::Runtime::SetObjectProperty(isolate, self, key_obj, value_obj,
                                    i::StoreOrigin::kMaybeKeyed,
                                    Just(i::ShouldThrow::kDontThrow))
          .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

Maybe<bool> v8::Object::Set(v8::Local<v8::Context> context, uint32_t index,
                            v8::Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, Set, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto value_obj = Utils::OpenHandle(*value);
  has_pending_exception = i::Object::SetElement(isolate, self, index, value_obj,
                                                i::ShouldThrow::kDontThrow)
                              .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

Maybe<bool> v8::Object::CreateDataProperty(v8::Local<v8::Context> context,
                                           v8::Local<Name> key,
                                           v8::Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, CreateDataProperty, Nothing<bool>(),
           i::HandleScope);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);

  Maybe<bool> result = i::JSReceiver::CreateDataProperty(
      isolate, self, key_obj, value_obj, Just(i::kDontThrow));
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::CreateDataProperty(v8::Local<v8::Context> context,
                                           uint32_t index,
                                           v8::Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, CreateDataProperty, Nothing<bool>(),
           i::HandleScope);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);

  i::LookupIterator it(isolate, self, index, self, i::LookupIterator::OWN);
  Maybe<bool> result =
      i::JSReceiver::CreateDataProperty(&it, value_obj, Just(i::kDontThrow));
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

struct v8::PropertyDescriptor::PrivateData {
  PrivateData() : desc() {}
  i::PropertyDescriptor desc;
};

v8::PropertyDescriptor::PropertyDescriptor() : private_(new PrivateData()) {}

// DataDescriptor
v8::PropertyDescriptor::PropertyDescriptor(v8::Local<v8::Value> value)
    : private_(new PrivateData()) {
  private_->desc.set_value(Utils::OpenHandle(*value, true));
}

// DataDescriptor with writable field
v8::PropertyDescriptor::PropertyDescriptor(v8::Local<v8::Value> value,
                                           bool writable)
    : private_(new PrivateData()) {
  private_->desc.set_value(Utils::OpenHandle(*value, true));
  private_->desc.set_writable(writable);
}

// AccessorDescriptor
v8::PropertyDescriptor::PropertyDescriptor(v8::Local<v8::Value> get,
                                           v8::Local<v8::Value> set)
    : private_(new PrivateData()) {
  DCHECK(get.IsEmpty() || get->IsUndefined() || get->IsFunction());
  DCHECK(set.IsEmpty() || set->IsUndefined() || set->IsFunction());
  private_->desc.set_get(Utils::OpenHandle(*get, true));
  private_->desc.set_set(Utils::OpenHandle(*set, true));
}

v8::PropertyDescriptor::~PropertyDescriptor() { delete private_; }

v8::Local<Value> v8::PropertyDescriptor::value() const {
  DCHECK(private_->desc.has_value());
  return Utils::ToLocal(private_->desc.value());
}

v8::Local<Value> v8::PropertyDescriptor::get() const {
  DCHECK(private_->desc.has_get());
  return Utils::ToLocal(private_->desc.get());
}

v8::Local<Value> v8::PropertyDescriptor::set() const {
  DCHECK(private_->desc.has_set());
  return Utils::ToLocal(private_->desc.set());
}

bool v8::PropertyDescriptor::has_value() const {
  return private_->desc.has_value();
}
bool v8::PropertyDescriptor::has_get() const {
  return private_->desc.has_get();
}
bool v8::PropertyDescriptor::has_set() const {
  return private_->desc.has_set();
}

bool v8::PropertyDescriptor::writable() const {
  DCHECK(private_->desc.has_writable());
  return private_->desc.writable();
}

bool v8::PropertyDescriptor::has_writable() const {
  return private_->desc.has_writable();
}

void v8::PropertyDescriptor::set_enumerable(bool enumerable) {
  private_->desc.set_enumerable(enumerable);
}

bool v8::PropertyDescriptor::enumerable() const {
  DCHECK(private_->desc.has_enumerable());
  return private_->desc.enumerable();
}

bool v8::PropertyDescriptor::has_enumerable() const {
  return private_->desc.has_enumerable();
}

void v8::PropertyDescriptor::set_configurable(bool configurable) {
  private_->desc.set_configurable(configurable);
}

bool v8::PropertyDescriptor::configurable() const {
  DCHECK(private_->desc.has_configurable());
  return private_->desc.configurable();
}

bool v8::PropertyDescriptor::has_configurable() const {
  return private_->desc.has_configurable();
}

Maybe<bool> v8::Object::DefineOwnProperty(v8::Local<v8::Context> context,
                                          v8::Local<Name> key,
                                          v8::Local<Value> value,
                                          v8::PropertyAttribute attributes) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);

  i::PropertyDescriptor desc;
  desc.set_writable(!(attributes & v8::ReadOnly));
  desc.set_enumerable(!(attributes & v8::DontEnum));
  desc.set_configurable(!(attributes & v8::DontDelete));
  desc.set_value(value_obj);

  if (self->IsJSProxy()) {
    ENTER_V8(isolate, context, Object, DefineOwnProperty, Nothing<bool>(),
             i::HandleScope);
    Maybe<bool> success = i::JSReceiver::DefineOwnProperty(
        isolate, self, key_obj, &desc, Just(i::kDontThrow));
    // Even though we said kDontThrow, there might be accessors that do throw.
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return success;
  } else {
    // If it's not a JSProxy, i::JSReceiver::DefineOwnProperty should never run
    // a script.
    ENTER_V8_NO_SCRIPT(isolate, context, Object, DefineOwnProperty,
                       Nothing<bool>(), i::HandleScope);
    Maybe<bool> success = i::JSReceiver::DefineOwnProperty(
        isolate, self, key_obj, &desc, Just(i::kDontThrow));
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return success;
  }
}

Maybe<bool> v8::Object::DefineProperty(v8::Local<v8::Context> context,
                                       v8::Local<Name> key,
                                       PropertyDescriptor& descriptor) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, DefineOwnProperty, Nothing<bool>(),
           i::HandleScope);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);

  Maybe<bool> success = i::JSReceiver::DefineOwnProperty(
      isolate, self, key_obj, &descriptor.get_private()->desc,
      Just(i::kDontThrow));
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return success;
}

Maybe<bool> v8::Object::SetPrivate(Local<Context> context, Local<Private> key,
                                   Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, SetPrivate, Nothing<bool>(),
                     i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(reinterpret_cast<Name*>(*key));
  auto value_obj = Utils::OpenHandle(*value);
  if (self->IsJSProxy()) {
    i::PropertyDescriptor desc;
    desc.set_writable(true);
    desc.set_enumerable(false);
    desc.set_configurable(true);
    desc.set_value(value_obj);
    return i::JSProxy::SetPrivateSymbol(
        isolate, i::Handle<i::JSProxy>::cast(self),
        i::Handle<i::Symbol>::cast(key_obj), &desc, Just(i::kDontThrow));
  }
  auto js_object = i::Handle<i::JSObject>::cast(self);
  i::LookupIterator it(isolate, js_object, key_obj, js_object);
  has_pending_exception = i::JSObject::DefineOwnPropertyIgnoreAttributes(
                              &it, value_obj, i::DONT_ENUM)
                              .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

MaybeLocal<Value> v8::Object::Get(Local<v8::Context> context,
                                  Local<Value> key) {
  PREPARE_FOR_EXECUTION(context, Object, Get, Value);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::Runtime::GetObjectProperty(isolate, self, key_obj).ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(Utils::ToLocal(result));
}

MaybeLocal<Value> v8::Object::Get(Local<Context> context, uint32_t index) {
  PREPARE_FOR_EXECUTION(context, Object, Get, Value);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::JSReceiver::GetElement(isolate, self, index).ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(Utils::ToLocal(result));
}

MaybeLocal<Value> v8::Object::GetPrivate(Local<Context> context,
                                         Local<Private> key) {
  return Get(context, Local<Value>(reinterpret_cast<Value*>(*key)));
}

Maybe<PropertyAttribute> v8::Object::GetPropertyAttributes(
    Local<Context> context, Local<Value> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, GetPropertyAttributes,
           Nothing<PropertyAttribute>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  if (!key_obj->IsName()) {
    has_pending_exception =
        !i::Object::ToString(isolate, key_obj).ToHandle(&key_obj);
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(PropertyAttribute);
  }
  auto key_name = i::Handle<i::Name>::cast(key_obj);
  auto result = i::JSReceiver::GetPropertyAttributes(self, key_name);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(PropertyAttribute);
  if (result.FromJust() == i::ABSENT) {
    return Just(static_cast<PropertyAttribute>(i::NONE));
  }
  return Just(static_cast<PropertyAttribute>(result.FromJust()));
}

MaybeLocal<Value> v8::Object::GetOwnPropertyDescriptor(Local<Context> context,
                                                       Local<Name> key) {
  PREPARE_FOR_EXECUTION(context, Object, GetOwnPropertyDescriptor, Value);
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  i::Handle<i::Name> key_name = Utils::OpenHandle(*key);

  i::PropertyDescriptor desc;
  Maybe<bool> found =
      i::JSReceiver::GetOwnPropertyDescriptor(isolate, obj, key_name, &desc);
  has_pending_exception = found.IsNothing();
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!found.FromJust()) {
    return v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  RETURN_ESCAPED(Utils::ToLocal(desc.ToObject(isolate)));
}

Local<Value> v8::Object::GetPrototype() {
  auto self = Utils::OpenHandle(this);
  auto isolate = self->GetIsolate();
  i::PrototypeIterator iter(isolate, self);
  return Utils::ToLocal(i::PrototypeIterator::GetCurrent(iter));
}

Maybe<bool> v8::Object::SetPrototype(Local<Context> context,
                                     Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, SetPrototype, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto value_obj = Utils::OpenHandle(*value);
  // We do not allow exceptions thrown while setting the prototype
  // to propagate outside.
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  auto result =
      i::JSReceiver::SetPrototype(self, value_obj, false, i::kThrowOnError);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

Local<Object> v8::Object::FindInstanceInPrototypeChain(
    v8::Local<FunctionTemplate> tmpl) {
  auto self = Utils::OpenHandle(this);
  auto isolate = self->GetIsolate();
  i::PrototypeIterator iter(isolate, *self, i::kStartAtReceiver);
  auto tmpl_info = *Utils::OpenHandle(*tmpl);
  while (!tmpl_info.IsTemplateFor(iter.GetCurrent<i::JSObject>())) {
    iter.Advance();
    if (iter.IsAtEnd()) return Local<Object>();
    if (!iter.GetCurrent().IsJSObject()) return Local<Object>();
  }
  // IsTemplateFor() ensures that iter.GetCurrent() can't be a Proxy here.
  return Utils::ToLocal(i::handle(iter.GetCurrent<i::JSObject>(), isolate));
}

MaybeLocal<Array> v8::Object::GetPropertyNames(Local<Context> context) {
  return GetPropertyNames(
      context, v8::KeyCollectionMode::kIncludePrototypes,
      static_cast<v8::PropertyFilter>(ONLY_ENUMERABLE | SKIP_SYMBOLS),
      v8::IndexFilter::kIncludeIndices);
}

MaybeLocal<Array> v8::Object::GetPropertyNames(
    Local<Context> context, KeyCollectionMode mode,
    PropertyFilter property_filter, IndexFilter index_filter,
    KeyConversionMode key_conversion) {
  PREPARE_FOR_EXECUTION(context, Object, GetPropertyNames, Array);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::FixedArray> value;
  i::KeyAccumulator accumulator(
      isolate, static_cast<i::KeyCollectionMode>(mode),
      static_cast<i::PropertyFilter>(property_filter));
  accumulator.set_skip_indices(index_filter == IndexFilter::kSkipIndices);
  has_pending_exception = accumulator.CollectKeys(self, self).IsNothing();
  RETURN_ON_FAILED_EXECUTION(Array);
  value =
      accumulator.GetKeys(static_cast<i::GetKeysConversion>(key_conversion));
  DCHECK(self->map().EnumLength() == i::kInvalidEnumCacheSentinel ||
         self->map().EnumLength() == 0 ||
         self->map().instance_descriptors().enum_cache().keys() != *value);
  auto result = isolate->factory()->NewJSArrayWithElements(value);
  RETURN_ESCAPED(Utils::ToLocal(result));
}

MaybeLocal<Array> v8::Object::GetOwnPropertyNames(Local<Context> context) {
  return GetOwnPropertyNames(
      context, static_cast<v8::PropertyFilter>(ONLY_ENUMERABLE | SKIP_SYMBOLS));
}

MaybeLocal<Array> v8::Object::GetOwnPropertyNames(
    Local<Context> context, PropertyFilter filter,
    KeyConversionMode key_conversion) {
  return GetPropertyNames(context, KeyCollectionMode::kOwnOnly, filter,
                          v8::IndexFilter::kIncludeIndices, key_conversion);
}

MaybeLocal<String> v8::Object::ObjectProtoToString(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, Object, ObjectProtoToString, String);
  auto self = Utils::OpenHandle(this);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::CallBuiltin(isolate, isolate->object_to_string(), self, 0,
                                nullptr),
      &result);
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(Local<String>::Cast(result));
}

Local<String> v8::Object::GetConstructorName() {
  auto self = Utils::OpenHandle(this);
  i::Handle<i::String> name = i::JSReceiver::GetConstructorName(self);
  return Utils::ToLocal(name);
}

Maybe<bool> v8::Object::SetIntegrityLevel(Local<Context> context,
                                          IntegrityLevel level) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, SetIntegrityLevel, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::JSReceiver::IntegrityLevel i_level =
      level == IntegrityLevel::kFrozen ? i::FROZEN : i::SEALED;
  Maybe<bool> result =
      i::JSReceiver::SetIntegrityLevel(self, i_level, i::kThrowOnError);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::Delete(Local<Context> context, Local<Value> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  if (self->IsJSProxy()) {
    ENTER_V8(isolate, context, Object, Delete, Nothing<bool>(), i::HandleScope);
    Maybe<bool> result = i::Runtime::DeleteObjectProperty(
        isolate, self, key_obj, i::LanguageMode::kSloppy);
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return result;
  } else {
    // If it's not a JSProxy, i::Runtime::DeleteObjectProperty should never run
    // a script.
    ENTER_V8_NO_SCRIPT(isolate, context, Object, Delete, Nothing<bool>(),
                       i::HandleScope);
    Maybe<bool> result = i::Runtime::DeleteObjectProperty(
        isolate, self, key_obj, i::LanguageMode::kSloppy);
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return result;
  }
}

Maybe<bool> v8::Object::DeletePrivate(Local<Context> context,
                                      Local<Private> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  // In case of private symbols, i::Runtime::DeleteObjectProperty does not run
  // any author script.
  ENTER_V8_NO_SCRIPT(isolate, context, Object, Delete, Nothing<bool>(),
                     i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  Maybe<bool> result = i::Runtime::DeleteObjectProperty(
      isolate, self, key_obj, i::LanguageMode::kSloppy);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::Has(Local<Context> context, Local<Value> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, Has, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  Maybe<bool> maybe = Nothing<bool>();
  // Check if the given key is an array index.
  uint32_t index = 0;
  if (key_obj->ToArrayIndex(&index)) {
    maybe = i::JSReceiver::HasElement(self, index);
  } else {
    // Convert the key to a name - possibly by calling back into JavaScript.
    i::Handle<i::Name> name;
    if (i::Object::ToName(isolate, key_obj).ToHandle(&name)) {
      maybe = i::JSReceiver::HasProperty(self, name);
    }
  }
  has_pending_exception = maybe.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return maybe;
}

Maybe<bool> v8::Object::HasPrivate(Local<Context> context, Local<Private> key) {
  return HasOwnProperty(context, Local<Name>(reinterpret_cast<Name*>(*key)));
}

Maybe<bool> v8::Object::Delete(Local<Context> context, uint32_t index) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, Delete, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  Maybe<bool> result = i::JSReceiver::DeleteElement(self, index);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::Has(Local<Context> context, uint32_t index) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, Has, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto maybe = i::JSReceiver::HasElement(self, index);
  has_pending_exception = maybe.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return maybe;
}

template <typename Getter, typename Setter, typename Data>
static Maybe<bool> ObjectSetAccessor(
    Local<Context> context, Object* self, Local<Name> name, Getter getter,
    Setter setter, Data data, AccessControl settings,
    PropertyAttribute attributes, bool is_special_data_property,
    bool replace_on_access, SideEffectType getter_side_effect_type,
    SideEffectType setter_side_effect_type) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, SetAccessor, Nothing<bool>(),
                     i::HandleScope);
  if (!Utils::OpenHandle(self)->IsJSObject()) return Just(false);
  i::Handle<i::JSObject> obj =
      i::Handle<i::JSObject>::cast(Utils::OpenHandle(self));
  v8::Local<AccessorSignature> signature;
  i::Handle<i::AccessorInfo> info =
      MakeAccessorInfo(isolate, name, getter, setter, data, settings, signature,
                       is_special_data_property, replace_on_access);
  info->set_getter_side_effect_type(getter_side_effect_type);
  info->set_setter_side_effect_type(setter_side_effect_type);
  if (info.is_null()) return Nothing<bool>();
  bool fast = obj->HasFastProperties();
  i::Handle<i::Object> result;

  i::Handle<i::Name> accessor_name(info->name(), isolate);
  i::PropertyAttributes attrs = static_cast<i::PropertyAttributes>(attributes);
  has_pending_exception =
      !i::JSObject::SetAccessor(obj, accessor_name, info, attrs)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  if (result->IsUndefined(isolate)) return Just(false);
  if (fast) {
    i::JSObject::MigrateSlowToFast(obj, 0, "APISetAccessor");
  }
  return Just(true);
}

Maybe<bool> Object::SetAccessor(Local<Context> context, Local<Name> name,
                                AccessorNameGetterCallback getter,
                                AccessorNameSetterCallback setter,
                                MaybeLocal<Value> data, AccessControl settings,
                                PropertyAttribute attribute,
                                SideEffectType getter_side_effect_type,
                                SideEffectType setter_side_effect_type) {
  return ObjectSetAccessor(context, this, name, getter, setter,
                           data.FromMaybe(Local<Value>()), settings, attribute,
                           i::FLAG_disable_old_api_accessors, false,
                           getter_side_effect_type, setter_side_effect_type);
}

void Object::SetAccessorProperty(Local<Name> name, Local<Function> getter,
                                 Local<Function> setter,
                                 PropertyAttribute attribute,
                                 AccessControl settings) {
  // TODO(verwaest): Remove |settings|.
  DCHECK_EQ(v8::DEFAULT, settings);
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  if (!self->IsJSObject()) return;
  i::Handle<i::Object> getter_i = v8::Utils::OpenHandle(*getter);
  i::Handle<i::Object> setter_i = v8::Utils::OpenHandle(*setter, true);
  if (setter_i.is_null()) setter_i = isolate->factory()->null_value();
  i::JSObject::DefineAccessor(i::Handle<i::JSObject>::cast(self),
                              v8::Utils::OpenHandle(*name), getter_i, setter_i,
                              static_cast<i::PropertyAttributes>(attribute));
}

Maybe<bool> Object::SetNativeDataProperty(
    v8::Local<v8::Context> context, v8::Local<Name> name,
    AccessorNameGetterCallback getter, AccessorNameSetterCallback setter,
    v8::Local<Value> data, PropertyAttribute attributes,
    SideEffectType getter_side_effect_type,
    SideEffectType setter_side_effect_type) {
  return ObjectSetAccessor(context, this, name, getter, setter, data, DEFAULT,
                           attributes, true, false, getter_side_effect_type,
                           setter_side_effect_type);
}

Maybe<bool> Object::SetLazyDataProperty(
    v8::Local<v8::Context> context, v8::Local<Name> name,
    AccessorNameGetterCallback getter, v8::Local<Value> data,
    PropertyAttribute attributes, SideEffectType getter_side_effect_type,
    SideEffectType setter_side_effect_type) {
  return ObjectSetAccessor(context, this, name, getter,
                           static_cast<AccessorNameSetterCallback>(nullptr),
                           data, DEFAULT, attributes, true, true,
                           getter_side_effect_type, setter_side_effect_type);
}

Maybe<bool> v8::Object::HasOwnProperty(Local<Context> context,
                                       Local<Name> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, HasOwnProperty, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_val = Utils::OpenHandle(*key);
  auto result = i::JSReceiver::HasOwnProperty(self, key_val);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::HasOwnProperty(Local<Context> context, uint32_t index) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, HasOwnProperty, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto result = i::JSReceiver::HasOwnProperty(self, index);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::HasRealNamedProperty(Local<Context> context,
                                             Local<Name> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, HasRealNamedProperty,
                     Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return Just(false);
  auto key_val = Utils::OpenHandle(*key);
  auto result = i::JSObject::HasRealNamedProperty(
      i::Handle<i::JSObject>::cast(self), key_val);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::HasRealIndexedProperty(Local<Context> context,
                                               uint32_t index) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, HasRealIndexedProperty,
                     Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return Just(false);
  auto result = i::JSObject::HasRealElementProperty(
      i::Handle<i::JSObject>::cast(self), index);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::HasRealNamedCallbackProperty(Local<Context> context,
                                                     Local<Name> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, HasRealNamedCallbackProperty,
                     Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return Just(false);
  auto key_val = Utils::OpenHandle(*key);
  auto result = i::JSObject::HasRealNamedCallbackProperty(
      i::Handle<i::JSObject>::cast(self), key_val);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

bool v8::Object::HasNamedLookupInterceptor() {
  auto self = Utils::OpenHandle(this);
  return self->IsJSObject() &&
         i::Handle<i::JSObject>::cast(self)->HasNamedInterceptor();
}

bool v8::Object::HasIndexedLookupInterceptor() {
  auto self = Utils::OpenHandle(this);
  return self->IsJSObject() &&
         i::Handle<i::JSObject>::cast(self)->HasIndexedInterceptor();
}

MaybeLocal<Value> v8::Object::GetRealNamedPropertyInPrototypeChain(
    Local<Context> context, Local<Name> key) {
  PREPARE_FOR_EXECUTION(context, Object, GetRealNamedPropertyInPrototypeChain,
                        Value);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return MaybeLocal<Value>();
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::PrototypeIterator iter(isolate, self);
  if (iter.IsAtEnd()) return MaybeLocal<Value>();
  i::Handle<i::JSReceiver> proto =
      i::PrototypeIterator::GetCurrent<i::JSReceiver>(iter);
  i::LookupIterator::Key lookup_key(isolate, key_obj);
  i::LookupIterator it(isolate, self, lookup_key, proto,
                       i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(i::Object::GetProperty(&it), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!it.IsFound()) return MaybeLocal<Value>();
  RETURN_ESCAPED(result);
}

Maybe<PropertyAttribute>
v8::Object::GetRealNamedPropertyAttributesInPrototypeChain(
    Local<Context> context, Local<Name> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object,
                     GetRealNamedPropertyAttributesInPrototypeChain,
                     Nothing<PropertyAttribute>(), i::HandleScope);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return Nothing<PropertyAttribute>();
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::PrototypeIterator iter(isolate, self);
  if (iter.IsAtEnd()) return Nothing<PropertyAttribute>();
  i::Handle<i::JSReceiver> proto =
      i::PrototypeIterator::GetCurrent<i::JSReceiver>(iter);
  i::LookupIterator::Key lookup_key(isolate, key_obj);
  i::LookupIterator it(isolate, self, lookup_key, proto,
                       i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Maybe<i::PropertyAttributes> result =
      i::JSReceiver::GetPropertyAttributes(&it);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(PropertyAttribute);
  if (!it.IsFound()) return Nothing<PropertyAttribute>();
  if (result.FromJust() == i::ABSENT) return Just(None);
  return Just(static_cast<PropertyAttribute>(result.FromJust()));
}

MaybeLocal<Value> v8::Object::GetRealNamedProperty(Local<Context> context,
                                                   Local<Name> key) {
  PREPARE_FOR_EXECUTION(context, Object, GetRealNamedProperty, Value);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::LookupIterator::Key lookup_key(isolate, key_obj);
  i::LookupIterator it(isolate, self, lookup_key, self,
                       i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(i::Object::GetProperty(&it), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!it.IsFound()) return MaybeLocal<Value>();
  RETURN_ESCAPED(result);
}

Maybe<PropertyAttribute> v8::Object::GetRealNamedPropertyAttributes(
    Local<Context> context, Local<Name> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, GetRealNamedPropertyAttributes,
                     Nothing<PropertyAttribute>(), i::HandleScope);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::LookupIterator::Key lookup_key(isolate, key_obj);
  i::LookupIterator it(isolate, self, lookup_key, self,
                       i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  auto result = i::JSReceiver::GetPropertyAttributes(&it);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(PropertyAttribute);
  if (!it.IsFound()) return Nothing<PropertyAttribute>();
  if (result.FromJust() == i::ABSENT) {
    return Just(static_cast<PropertyAttribute>(i::NONE));
  }
  return Just<PropertyAttribute>(
      static_cast<PropertyAttribute>(result.FromJust()));
}

Local<v8::Object> v8::Object::Clone() {
  auto self = i::Handle<i::JSObject>::cast(Utils::OpenHandle(this));
  auto isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  auto result = isolate->factory()->CopyJSObject(self);
  CHECK(!result.is_null());
  return Utils::ToLocal(result);
}

Local<v8::Context> v8::Object::CreationContext() {
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Context> context = self->GetCreationContext();
  return Utils::ToLocal(context);
}

int v8::Object::GetIdentityHash() {
  i::DisallowHeapAllocation no_gc;
  auto self = Utils::OpenHandle(this);
  auto isolate = self->GetIsolate();
  i::HandleScope scope(isolate);
  return self->GetOrCreateIdentityHash(isolate).value();
}

bool v8::Object::IsCallable() {
  auto self = Utils::OpenHandle(this);
  return self->IsCallable();
}

bool v8::Object::IsConstructor() {
  auto self = Utils::OpenHandle(this);
  return self->IsConstructor();
}

bool v8::Object::IsApiWrapper() {
  auto self = i::Handle<i::JSObject>::cast(Utils::OpenHandle(this));
  return self->IsApiWrapper();
}

MaybeLocal<Value> Object::CallAsFunction(Local<Context> context,
                                         Local<Value> recv, int argc,
                                         Local<Value> argv[]) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  ENTER_V8(isolate, context, Object, CallAsFunction, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(isolate);
  auto self = Utils::OpenHandle(this);
  auto recv_obj = Utils::OpenHandle(*recv);
  STATIC_ASSERT(sizeof(v8::Local<v8::Value>) == sizeof(i::Handle<i::Object>));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::Call(isolate, self, recv_obj, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<Value> Object::CallAsConstructor(Local<Context> context, int argc,
                                            Local<Value> argv[]) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  ENTER_V8(isolate, context, Object, CallAsConstructor, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(isolate);
  auto self = Utils::OpenHandle(this);
  STATIC_ASSERT(sizeof(v8::Local<v8::Value>) == sizeof(i::Handle<i::Object>));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::New(isolate, self, self, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<Function> Function::New(Local<Context> context,
                                   FunctionCallback callback, Local<Value> data,
                                   int length, ConstructorBehavior behavior,
                                   SideEffectType side_effect_type) {
  i::Isolate* isolate = Utils::OpenHandle(*context)->GetIsolate();
  LOG_API(isolate, Function, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  auto templ =
      FunctionTemplateNew(isolate, callback, data, Local<Signature>(), length,
                          true, Local<Private>(), side_effect_type);
  if (behavior == ConstructorBehavior::kThrow) templ->RemovePrototype();
  return templ->GetFunction(context);
}

MaybeLocal<Object> Function::NewInstance(Local<Context> context, int argc,
                                         v8::Local<v8::Value> argv[]) const {
  return NewInstanceWithSideEffectType(context, argc, argv,
                                       SideEffectType::kHasSideEffect);
}

MaybeLocal<Object> Function::NewInstanceWithSideEffectType(
    Local<Context> context, int argc, v8::Local<v8::Value> argv[],
    SideEffectType side_effect_type) const {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  ENTER_V8(isolate, context, Function, NewInstance, MaybeLocal<Object>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(isolate);
  auto self = Utils::OpenHandle(this);
  STATIC_ASSERT(sizeof(v8::Local<v8::Value>) == sizeof(i::Handle<i::Object>));
  bool should_set_has_no_side_effect =
      side_effect_type == SideEffectType::kHasNoSideEffect &&
      isolate->debug_execution_mode() == i::DebugInfo::kSideEffects;
  if (should_set_has_no_side_effect) {
    CHECK(self->IsJSFunction() &&
          i::JSFunction::cast(*self).shared().IsApiFunction());
    i::Object obj =
        i::JSFunction::cast(*self).shared().get_api_func_data().call_code();
    if (obj.IsCallHandlerInfo()) {
      i::CallHandlerInfo handler_info = i::CallHandlerInfo::cast(obj);
      if (!handler_info.IsSideEffectFreeCallHandlerInfo()) {
        handler_info.SetNextCallHasNoSideEffect();
      }
    }
  }
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Object> result;
  has_pending_exception = !ToLocal<Object>(
      i::Execution::New(isolate, self, self, argc, args), &result);
  if (should_set_has_no_side_effect) {
    i::Object obj =
        i::JSFunction::cast(*self).shared().get_api_func_data().call_code();
    if (obj.IsCallHandlerInfo()) {
      i::CallHandlerInfo handler_info = i::CallHandlerInfo::cast(obj);
      if (has_pending_exception) {
        // Restore the map if an exception prevented restoration.
        handler_info.NextCallHasNoSideEffect();
      } else {
        DCHECK(handler_info.IsSideEffectCallHandlerInfo() ||
               handler_info.IsSideEffectFreeCallHandlerInfo());
      }
    }
  }
  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}

MaybeLocal<v8::Value> Function::Call(Local<Context> context,
                                     v8::Local<v8::Value> recv, int argc,
                                     v8::Local<v8::Value> argv[]) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  ENTER_V8(isolate, context, Function, Call, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(isolate);
  auto self = Utils::OpenHandle(this);
  Utils::ApiCheck(!self.is_null(), "v8::Function::Call",
                  "Function to be called is a null pointer");
  i::Handle<i::Object> recv_obj = Utils::OpenHandle(*recv);
  STATIC_ASSERT(sizeof(v8::Local<v8::Value>) == sizeof(i::Handle<i::Object>));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::Call(isolate, self, recv_obj, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

void Function::SetName(v8::Local<v8::String> name) {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) return;
  auto func = i::Handle<i::JSFunction>::cast(self);
  func->shared().SetName(*Utils::OpenHandle(*name));
}

Local<Value> Function::GetName() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  if (self->IsJSBoundFunction()) {
    auto func = i::Handle<i::JSBoundFunction>::cast(self);
    i::Handle<i::Object> name;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, name,
                                     i::JSBoundFunction::GetName(isolate, func),
                                     Local<Value>());
    return Utils::ToLocal(name);
  }
  if (self->IsJSFunction()) {
    auto func = i::Handle<i::JSFunction>::cast(self);
    return Utils::ToLocal(handle(func->shared().Name(), isolate));
  }
  return ToApiHandle<Primitive>(isolate->factory()->undefined_value());
}

Local<Value> Function::GetInferredName() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return ToApiHandle<Primitive>(
        self->GetIsolate()->factory()->undefined_value());
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  return Utils::ToLocal(
      i::Handle<i::Object>(func->shared().inferred_name(), func->GetIsolate()));
}

Local<Value> Function::GetDebugName() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return ToApiHandle<Primitive>(
        self->GetIsolate()->factory()->undefined_value());
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  i::Handle<i::String> name = i::JSFunction::GetDebugName(func);
  return Utils::ToLocal(i::Handle<i::Object>(*name, self->GetIsolate()));
}

Local<Value> Function::GetDisplayName() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (!self->IsJSFunction()) {
    return ToApiHandle<Primitive>(isolate->factory()->undefined_value());
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  i::Handle<i::String> property_name =
      isolate->factory()->display_name_string();
  i::Handle<i::Object> value =
      i::JSReceiver::GetDataProperty(func, property_name);
  if (value->IsString()) {
    i::Handle<i::String> name = i::Handle<i::String>::cast(value);
    if (name->length() > 0) return Utils::ToLocal(name);
  }
  return ToApiHandle<Primitive>(isolate->factory()->undefined_value());
}

ScriptOrigin Function::GetScriptOrigin() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return v8::ScriptOrigin(Local<Value>());
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  if (func->shared().script().IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared().script()),
                                func->GetIsolate());
    return GetScriptOriginForScript(func->GetIsolate(), script);
  }
  return v8::ScriptOrigin(Local<Value>());
}

const int Function::kLineOffsetNotFound = -1;

int Function::GetScriptLineNumber() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return kLineOffsetNotFound;
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  if (func->shared().script().IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared().script()),
                                func->GetIsolate());
    return i::Script::GetLineNumber(script, func->shared().StartPosition());
  }
  return kLineOffsetNotFound;
}

int Function::GetScriptColumnNumber() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return kLineOffsetNotFound;
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  if (func->shared().script().IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared().script()),
                                func->GetIsolate());
    return i::Script::GetColumnNumber(script, func->shared().StartPosition());
  }
  return kLineOffsetNotFound;
}

int Function::ScriptId() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return v8::UnboundScript::kNoScriptId;
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  if (!func->shared().script().IsScript()) {
    return v8::UnboundScript::kNoScriptId;
  }
  i::Handle<i::Script> script(i::Script::cast(func->shared().script()),
                              func->GetIsolate());
  return script->id();
}

Local<v8::Value> Function::GetBoundFunction() const {
  auto self = Utils::OpenHandle(this);
  if (self->IsJSBoundFunction()) {
    auto bound_function = i::Handle<i::JSBoundFunction>::cast(self);
    auto bound_target_function = i::handle(
        bound_function->bound_target_function(), bound_function->GetIsolate());
    return Utils::CallableToLocal(bound_target_function);
  }
  return v8::Undefined(reinterpret_cast<v8::Isolate*>(self->GetIsolate()));
}

int Name::GetIdentityHash() {
  auto self = Utils::OpenHandle(this);
  return static_cast<int>(self->Hash());
}

int String::Length() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  return str->length();
}

bool String::IsOneByte() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  return str->IsOneByteRepresentation();
}

// Helpers for ContainsOnlyOneByteHelper
template <size_t size>
struct OneByteMask;
template <>
struct OneByteMask<4> {
  static const uint32_t value = 0xFF00FF00;
};
template <>
struct OneByteMask<8> {
  static const uint64_t value = V8_2PART_UINT64_C(0xFF00FF00, FF00FF00);
};
static const uintptr_t kOneByteMask = OneByteMask<sizeof(uintptr_t)>::value;
static const uintptr_t kAlignmentMask = sizeof(uintptr_t) - 1;
static inline bool Unaligned(const uint16_t* chars) {
  return reinterpret_cast<const uintptr_t>(chars) & kAlignmentMask;
}

static inline const uint16_t* Align(const uint16_t* chars) {
  return reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(chars) &
                                     ~kAlignmentMask);
}

class ContainsOnlyOneByteHelper {
 public:
  ContainsOnlyOneByteHelper() : is_one_byte_(true) {}
  bool Check(i::String string) {
    i::ConsString cons_string = i::String::VisitFlat(this, string, 0);
    if (cons_string.is_null()) return is_one_byte_;
    return CheckCons(cons_string);
  }
  void VisitOneByteString(const uint8_t* chars, int length) {
    // Nothing to do.
  }
  void VisitTwoByteString(const uint16_t* chars, int length) {
    // Accumulated bits.
    uintptr_t acc = 0;
    // Align to uintptr_t.
    const uint16_t* end = chars + length;
    while (Unaligned(chars) && chars != end) {
      acc |= *chars++;
    }
    // Read word aligned in blocks,
    // checking the return value at the end of each block.
    const uint16_t* aligned_end = Align(end);
    const int increment = sizeof(uintptr_t) / sizeof(uint16_t);
    const int inner_loops = 16;
    while (chars + inner_loops * increment < aligned_end) {
      for (int i = 0; i < inner_loops; i++) {
        acc |= *reinterpret_cast<const uintptr_t*>(chars);
        chars += increment;
      }
      // Check for early return.
      if ((acc & kOneByteMask) != 0) {
        is_one_byte_ = false;
        return;
      }
    }
    // Read the rest.
    while (chars != end) {
      acc |= *chars++;
    }
    // Check result.
    if ((acc & kOneByteMask) != 0) is_one_byte_ = false;
  }

 private:
  bool CheckCons(i::ConsString cons_string) {
    while (true) {
      // Check left side if flat.
      i::String left = cons_string.first();
      i::ConsString left_as_cons = i::String::VisitFlat(this, left, 0);
      if (!is_one_byte_) return false;
      // Check right side if flat.
      i::String right = cons_string.second();
      i::ConsString right_as_cons = i::String::VisitFlat(this, right, 0);
      if (!is_one_byte_) return false;
      // Standard recurse/iterate trick.
      if (!left_as_cons.is_null() && !right_as_cons.is_null()) {
        if (left.length() < right.length()) {
          CheckCons(left_as_cons);
          cons_string = right_as_cons;
        } else {
          CheckCons(right_as_cons);
          cons_string = left_as_cons;
        }
        // Check fast return.
        if (!is_one_byte_) return false;
        continue;
      }
      // Descend left in place.
      if (!left_as_cons.is_null()) {
        cons_string = left_as_cons;
        continue;
      }
      // Descend right in place.
      if (!right_as_cons.is_null()) {
        cons_string = right_as_cons;
        continue;
      }
      // Terminate.
      break;
    }
    return is_one_byte_;
  }
  bool is_one_byte_;
  DISALLOW_COPY_AND_ASSIGN(ContainsOnlyOneByteHelper);
};

bool String::ContainsOnlyOneByte() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (str->IsOneByteRepresentation()) return true;
  ContainsOnlyOneByteHelper helper;
  return helper.Check(*str);
}

int String::Utf8Length(Isolate* isolate) const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  str = i::String::Flatten(reinterpret_cast<i::Isolate*>(isolate), str);
  int length = str->length();
  if (length == 0) return 0;
  i::DisallowHeapAllocation no_gc;
  i::String::FlatContent flat = str->GetFlatContent(no_gc);
  DCHECK(flat.IsFlat());
  int utf8_length = 0;
  if (flat.IsOneByte()) {
    for (uint8_t c : flat.ToOneByteVector()) {
      utf8_length += c >> 7;
    }
    utf8_length += length;
  } else {
    int last_character = unibrow::Utf16::kNoPreviousCharacter;
    for (uint16_t c : flat.ToUC16Vector()) {
      utf8_length += unibrow::Utf8::Length(c, last_character);
      last_character = c;
    }
  }
  return utf8_length;
}

namespace {
// Writes the flat content of a string to a buffer. This is done in two phases.
// The first phase calculates a pessimistic estimate (writable_length) on how
// many code units can be safely written without exceeding the buffer capacity
// and without leaving at a lone surrogate. The estimated number of code units
// is then written out in one go, and the reported byte usage is used to
// correct the estimate. This is repeated until the estimate becomes <= 0 or
// all code units have been written out. The second phase writes out code
// units until the buffer capacity is reached, would be exceeded by the next
// unit, or all code units have been written out.
template <typename Char>
static int WriteUtf8Impl(i::Vector<const Char> string, char* write_start,
                         int write_capacity, int options,
                         int* utf16_chars_read_out) {
  bool write_null = !(options & v8::String::NO_NULL_TERMINATION);
  bool replace_invalid_utf8 = (options & v8::String::REPLACE_INVALID_UTF8);
  char* current_write = write_start;
  const Char* read_start = string.begin();
  int read_index = 0;
  int read_length = string.length();
  int prev_char = unibrow::Utf16::kNoPreviousCharacter;
  // Do a fast loop where there is no exit capacity check.
  // Need enough space to write everything but one character.
  STATIC_ASSERT(unibrow::Utf16::kMaxExtraUtf8BytesForOneUtf16CodeUnit == 3);
  static const int kMaxSizePerChar = sizeof(Char) == 1 ? 2 : 3;
  while (read_index < read_length) {
    int up_to = read_length;
    if (write_capacity != -1) {
      int remaining_capacity =
          write_capacity - static_cast<int>(current_write - write_start);
      int writable_length =
          (remaining_capacity - kMaxSizePerChar) / kMaxSizePerChar;
      // Need to drop into slow loop.
      if (writable_length <= 0) break;
      up_to = std::min(up_to, read_index + writable_length);
    }
    // Write the characters to the stream.
    if (sizeof(Char) == 1) {
      // Simply memcpy if we only have ASCII characters.
      uint8_t char_mask = 0;
      for (int i = read_index; i < up_to; i++) char_mask |= read_start[i];
      if ((char_mask & 0x80) == 0) {
        int copy_length = up_to - read_index;
        memcpy(current_write, read_start + read_index, copy_length);
        current_write += copy_length;
        read_index = up_to;
      } else {
        for (; read_index < up_to; read_index++) {
          current_write += unibrow::Utf8::EncodeOneByte(
              current_write, static_cast<uint8_t>(read_start[read_index]));
          DCHECK(write_capacity == -1 ||
                 (current_write - write_start) <= write_capacity);
        }
      }
    } else {
      for (; read_index < up_to; read_index++) {
        uint16_t character = read_start[read_index];
        current_write += unibrow::Utf8::Encode(current_write, character,
                                               prev_char, replace_invalid_utf8);
        prev_char = character;
        DCHECK(write_capacity == -1 ||
               (current_write - write_start) <= write_capacity);
      }
    }
  }
  if (read_index < read_length) {
    DCHECK_NE(-1, write_capacity);
    // Aborted due to limited capacity. Check capacity on each iteration.
    int remaining_capacity =
        write_capacity - static_cast<int>(current_write - write_start);
    DCHECK_GE(remaining_capacity, 0);
    for (; read_index < read_length && remaining_capacity > 0; read_index++) {
      uint32_t character = read_start[read_index];
      int written = 0;
      // We can't use a local buffer here because Encode needs to modify
      // previous characters in the stream.  We know, however, that
      // exactly one character will be advanced.
      if (unibrow::Utf16::IsSurrogatePair(prev_char, character)) {
        written = unibrow::Utf8::Encode(current_write, character, prev_char,
                                        replace_invalid_utf8);
        DCHECK_EQ(written, 1);
      } else {
        // Use a scratch buffer to check the required characters.
        char temp_buffer[unibrow::Utf8::kMaxEncodedSize];
        // Encoding a surrogate pair to Utf8 always takes 4 bytes.
        static const int kSurrogatePairEncodedSize =
            static_cast<int>(unibrow::Utf8::kMaxEncodedSize);
        // For REPLACE_INVALID_UTF8, catch the case where we cut off in the
        // middle of a surrogate pair. Abort before encoding the pair instead.
        if (replace_invalid_utf8 &&
            remaining_capacity < kSurrogatePairEncodedSize &&
            unibrow::Utf16::IsLeadSurrogate(character) &&
            read_index + 1 < read_length &&
            unibrow::Utf16::IsTrailSurrogate(read_start[read_index + 1])) {
          write_null = false;
          break;
        }
        // Can't encode using prev_char as gcc has array bounds issues.
        written = unibrow::Utf8::Encode(temp_buffer, character,
                                        unibrow::Utf16::kNoPreviousCharacter,
                                        replace_invalid_utf8);
        if (written > remaining_capacity) {
          // Won't fit. Abort and do not null-terminate the result.
          write_null = false;
          break;
        }
        // Copy over the character from temp_buffer.
        for (int i = 0; i < written; i++) current_write[i] = temp_buffer[i];
      }

      current_write += written;
      remaining_capacity -= written;
      prev_char = character;
    }
  }

  // Write out number of utf16 characters written to the stream.
  if (utf16_chars_read_out != nullptr) *utf16_chars_read_out = read_index;

  // Only null-terminate if there's space.
  if (write_null && (write_capacity == -1 ||
                     (current_write - write_start) < write_capacity)) {
    *current_write++ = '\0';
  }
  return static_cast<int>(current_write - write_start);
}
}  // anonymous namespace

int String::WriteUtf8(Isolate* v8_isolate, char* buffer, int capacity,
                      int* nchars_ref, int options) const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  LOG_API(isolate, String, WriteUtf8);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  str = i::String::Flatten(isolate, str);
  i::DisallowHeapAllocation no_gc;
  i::String::FlatContent content = str->GetFlatContent(no_gc);
  if (content.IsOneByte()) {
    return WriteUtf8Impl<uint8_t>(content.ToOneByteVector(), buffer, capacity,
                                  options, nchars_ref);
  } else {
    return WriteUtf8Impl<uint16_t>(content.ToUC16Vector(), buffer, capacity,
                                   options, nchars_ref);
  }
}

template <typename CharType>
static inline int WriteHelper(i::Isolate* isolate, const String* string,
                              CharType* buffer, int start, int length,
                              int options) {
  LOG_API(isolate, String, Write);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  DCHECK(start >= 0 && length >= -1);
  i::Handle<i::String> str = Utils::OpenHandle(string);
  str = i::String::Flatten(isolate, str);
  int end = start + length;
  if ((length == -1) || (length > str->length() - start)) end = str->length();
  if (end < 0) return 0;
  if (start < end) i::String::WriteToFlat(*str, buffer, start, end);
  if (!(options & String::NO_NULL_TERMINATION) &&
      (length == -1 || end - start < length)) {
    buffer[end - start] = '\0';
  }
  return end - start;
}

int String::WriteOneByte(Isolate* isolate, uint8_t* buffer, int start,
                         int length, int options) const {
  return WriteHelper(reinterpret_cast<i::Isolate*>(isolate), this, buffer,
                     start, length, options);
}

int String::Write(Isolate* isolate, uint16_t* buffer, int start, int length,
                  int options) const {
  return WriteHelper(reinterpret_cast<i::Isolate*>(isolate), this, buffer,
                     start, length, options);
}

bool v8::String::IsExternal() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  return i::StringShape(*str).IsExternalTwoByte();
}

bool v8::String::IsExternalOneByte() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  return i::StringShape(*str).IsExternalOneByte();
}

void v8::String::VerifyExternalStringResource(
    v8::String::ExternalStringResource* value) const {
  i::DisallowHeapAllocation no_allocation;
  i::String str = *Utils::OpenHandle(this);
  const v8::String::ExternalStringResource* expected;

  if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
  }

  if (i::StringShape(str).IsExternalTwoByte()) {
    const void* resource = i::ExternalTwoByteString::cast(str).resource();
    expected = reinterpret_cast<const ExternalStringResource*>(resource);
  } else {
    expected = nullptr;
  }
  CHECK_EQ(expected, value);
}

void v8::String::VerifyExternalStringResourceBase(
    v8::String::ExternalStringResourceBase* value, Encoding encoding) const {
  i::DisallowHeapAllocation no_allocation;
  i::String str = *Utils::OpenHandle(this);
  const v8::String::ExternalStringResourceBase* expected;
  Encoding expectedEncoding;

  if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
  }

  if (i::StringShape(str).IsExternalOneByte()) {
    const void* resource = i::ExternalOneByteString::cast(str).resource();
    expected = reinterpret_cast<const ExternalStringResourceBase*>(resource);
    expectedEncoding = ONE_BYTE_ENCODING;
  } else if (i::StringShape(str).IsExternalTwoByte()) {
    const void* resource = i::ExternalTwoByteString::cast(str).resource();
    expected = reinterpret_cast<const ExternalStringResourceBase*>(resource);
    expectedEncoding = TWO_BYTE_ENCODING;
  } else {
    expected = nullptr;
    expectedEncoding =
        str.IsOneByteRepresentation() ? ONE_BYTE_ENCODING : TWO_BYTE_ENCODING;
  }
  CHECK_EQ(expected, value);
  CHECK_EQ(expectedEncoding, encoding);
}

String::ExternalStringResource* String::GetExternalStringResourceSlow() const {
  i::DisallowHeapAllocation no_allocation;
  using I = internal::Internals;
  i::String str = *Utils::OpenHandle(this);

  if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
  }

  if (i::StringShape(str).IsExternalTwoByte()) {
    void* value = I::ReadRawField<void*>(str.ptr(), I::kStringResourceOffset);
    return reinterpret_cast<String::ExternalStringResource*>(value);
  }
  return nullptr;
}

String::ExternalStringResourceBase* String::GetExternalStringResourceBaseSlow(
    String::Encoding* encoding_out) const {
  i::DisallowHeapAllocation no_allocation;
  using I = internal::Internals;
  ExternalStringResourceBase* resource = nullptr;
  i::String str = *Utils::OpenHandle(this);

  if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
  }

  internal::Address string = str.ptr();
  int type = I::GetInstanceType(string) & I::kFullStringRepresentationMask;
  *encoding_out = static_cast<Encoding>(type & I::kStringEncodingMask);
  if (i::StringShape(str).IsExternalOneByte() ||
      i::StringShape(str).IsExternalTwoByte()) {
    void* value = I::ReadRawField<void*>(string, I::kStringResourceOffset);
    resource = static_cast<ExternalStringResourceBase*>(value);
  }
  return resource;
}

const v8::String::ExternalOneByteStringResource*
v8::String::GetExternalOneByteStringResource() const {
  i::DisallowHeapAllocation no_allocation;
  i::String str = *Utils::OpenHandle(this);
  if (i::StringShape(str).IsExternalOneByte()) {
    return i::ExternalOneByteString::cast(str).resource();
  } else if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
    if (i::StringShape(str).IsExternalOneByte()) {
      return i::ExternalOneByteString::cast(str).resource();
    }
  }
  return nullptr;
}

Local<Value> Symbol::Description() const {
  i::Handle<i::Symbol> sym = Utils::OpenHandle(this);

  i::Isolate* isolate;
  if (!i::GetIsolateFromHeapObject(*sym, &isolate)) {
    // Symbol is in RO_SPACE, which means that its description is also in
    // RO_SPACE. Since RO_SPACE objects are immovable we can use the
    // Handle(Address*) constructor with the address of the description
    // field in the Symbol object without needing an isolate.
    DCHECK(!COMPRESS_POINTERS_BOOL);
    i::Handle<i::HeapObject> ro_description(reinterpret_cast<i::Address*>(
        sym->GetFieldAddress(i::Symbol::kDescriptionOffset)));
    return Utils::ToLocal(ro_description);
  }

  i::Handle<i::Object> description(sym->description(), isolate);

  return Utils::ToLocal(description);
}

Local<Value> Private::Name() const {
  return reinterpret_cast<const Symbol*>(this)->Description();
}

double Number::Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->Number();
}

bool Boolean::Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsTrue();
}

int64_t Integer::Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    return i::Smi::ToInt(*obj);
  } else {
    return static_cast<int64_t>(obj->Number());
  }
}

int32_t Int32::Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    return i::Smi::ToInt(*obj);
  } else {
    return static_cast<int32_t>(obj->Number());
  }
}

uint32_t Uint32::Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    return i::Smi::ToInt(*obj);
  } else {
    return static_cast<uint32_t>(obj->Number());
  }
}

int v8::Object::InternalFieldCount() {
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return 0;
  return i::Handle<i::JSObject>::cast(self)->GetEmbedderFieldCount();
}

static bool InternalFieldOK(i::Handle<i::JSReceiver> obj, int index,
                            const char* location) {
  return Utils::ApiCheck(
      obj->IsJSObject() &&
          (index < i::Handle<i::JSObject>::cast(obj)->GetEmbedderFieldCount()),
      location, "Internal field out of bounds");
}

Local<Value> v8::Object::SlowGetInternalField(int index) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::GetInternalField()";
  if (!InternalFieldOK(obj, index, location)) return Local<Value>();
  i::Handle<i::Object> value(i::JSObject::cast(*obj).GetEmbedderField(index),
                             obj->GetIsolate());
  return Utils::ToLocal(value);
}

void v8::Object::SetInternalField(int index, v8::Local<Value> value) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::SetInternalField()";
  if (!InternalFieldOK(obj, index, location)) return;
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  i::Handle<i::JSObject>::cast(obj)->SetEmbedderField(index, *val);
}

void* v8::Object::SlowGetAlignedPointerFromInternalField(int index) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::GetAlignedPointerFromInternalField()";
  if (!InternalFieldOK(obj, index, location)) return nullptr;
  void* result;
  Utils::ApiCheck(i::EmbedderDataSlot(i::JSObject::cast(*obj), index)
                      .ToAlignedPointer(&result),
                  location, "Unaligned pointer");
  return result;
}

void v8::Object::SetAlignedPointerInInternalField(int index, void* value) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::SetAlignedPointerInInternalField()";
  if (!InternalFieldOK(obj, index, location)) return;
  Utils::ApiCheck(i::EmbedderDataSlot(i::JSObject::cast(*obj), index)
                      .store_aligned_pointer(value),
                  location, "Unaligned pointer");
  DCHECK_EQ(value, GetAlignedPointerFromInternalField(index));
}

void v8::Object::SetAlignedPointerInInternalFields(int argc, int indices[],
                                                   void* values[]) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::SetAlignedPointerInInternalFields()";
  i::DisallowHeapAllocation no_gc;
  i::JSObject js_obj = i::JSObject::cast(*obj);
  int nof_embedder_fields = js_obj.GetEmbedderFieldCount();
  for (int i = 0; i < argc; i++) {
    int index = indices[i];
    if (!Utils::ApiCheck(index < nof_embedder_fields, location,
                         "Internal field out of bounds")) {
      return;
    }
    void* value = values[i];
    Utils::ApiCheck(
        i::EmbedderDataSlot(js_obj, index).store_aligned_pointer(value),
        location, "Unaligned pointer");
    DCHECK_EQ(value, GetAlignedPointerFromInternalField(index));
  }
}

static void* ExternalValue(i::Object obj) {
  // Obscure semantics for undefined, but somehow checked in our unit tests...
  if (obj.IsUndefined()) {
    return nullptr;
  }
  i::Object foreign = i::JSObject::cast(obj).GetEmbedderField(0);
  return reinterpret_cast<void*>(i::Foreign::cast(foreign).foreign_address());
}

// --- E n v i r o n m e n t ---

void v8::V8::InitializePlatform(Platform* platform) {
  i::V8::InitializePlatform(platform);
}

void v8::V8::ShutdownPlatform() { i::V8::ShutdownPlatform(); }

bool v8::V8::Initialize(const int build_config) {
  const bool kEmbedderPointerCompression =
      (build_config & kPointerCompression) != 0;
  if (kEmbedderPointerCompression != COMPRESS_POINTERS_BOOL) {
    FATAL(
        "Embedder-vs-V8 build configuration mismatch. On embedder side "
        "pointer compression is %s while on V8 side it's %s.",
        kEmbedderPointerCompression ? "ENABLED" : "DISABLED",
        COMPRESS_POINTERS_BOOL ? "ENABLED" : "DISABLED");
  }

  const int kEmbedderSmiValueSize = (build_config & k31BitSmis) ? 31 : 32;
  if (kEmbedderSmiValueSize != internal::kSmiValueSize) {
    FATAL(
        "Embedder-vs-V8 build configuration mismatch. On embedder side "
        "Smi value size is %d while on V8 side it's %d.",
        kEmbedderSmiValueSize, internal::kSmiValueSize);
  }

  i::V8::Initialize();
  return true;
}

#if V8_OS_LINUX || V8_OS_MACOSX
bool TryHandleWebAssemblyTrapPosix(int sig_code, siginfo_t* info,
                                   void* context) {
#if V8_TARGET_ARCH_X64 && !V8_OS_ANDROID
  return i::trap_handler::TryHandleSignal(sig_code, info, context);
#else
  return false;
#endif
}

bool V8::TryHandleSignal(int signum, void* info, void* context) {
  return TryHandleWebAssemblyTrapPosix(
      signum, reinterpret_cast<siginfo_t*>(info), context);
}
#endif

#if V8_OS_WIN
bool TryHandleWebAssemblyTrapWindows(EXCEPTION_POINTERS* exception) {
#if V8_TARGET_ARCH_X64
  return i::trap_handler::TryHandleWasmTrap(exception);
#endif
  return false;
}
#endif

bool V8::EnableWebAssemblyTrapHandler(bool use_v8_signal_handler) {
  return v8::internal::trap_handler::EnableTrapHandler(use_v8_signal_handler);
}

#if defined(V8_OS_WIN)
void V8::SetUnhandledExceptionCallback(
    UnhandledExceptionCallback unhandled_exception_callback) {
#if defined(V8_OS_WIN64)
  v8::internal::win64_unwindinfo::SetUnhandledExceptionCallback(
      unhandled_exception_callback);
#else
  // Not implemented, port needed.
#endif  // V8_OS_WIN64
}
#endif  // V8_OS_WIN

void v8::V8::SetEntropySource(EntropySource entropy_source) {
  base::RandomNumberGenerator::SetEntropySource(entropy_source);
}

void v8::V8::SetReturnAddressLocationResolver(
    ReturnAddressLocationResolver return_address_resolver) {
  i::StackFrame::SetReturnAddressLocationResolver(return_address_resolver);
}

bool v8::V8::Dispose() {
  i::V8::TearDown();
  return true;
}

SharedMemoryStatistics::SharedMemoryStatistics()
    : read_only_space_size_(0),
      read_only_space_used_size_(0),
      read_only_space_physical_size_(0) {}

HeapStatistics::HeapStatistics()
    : total_heap_size_(0),
      total_heap_size_executable_(0),
      total_physical_size_(0),
      total_available_size_(0),
      used_heap_size_(0),
      heap_size_limit_(0),
      malloced_memory_(0),
      external_memory_(0),
      peak_malloced_memory_(0),
      does_zap_garbage_(false),
      number_of_native_contexts_(0),
      number_of_detached_contexts_(0),
      total_global_handles_size_(0),
      used_global_handles_size_(0) {}

HeapSpaceStatistics::HeapSpaceStatistics()
    : space_name_(nullptr),
      space_size_(0),
      space_used_size_(0),
      space_available_size_(0),
      physical_space_size_(0) {}

HeapObjectStatistics::HeapObjectStatistics()
    : object_type_(nullptr),
      object_sub_type_(nullptr),
      object_count_(0),
      object_size_(0) {}

HeapCodeStatistics::HeapCodeStatistics()
    : code_and_metadata_size_(0),
      bytecode_and_metadata_size_(0),
      external_script_source_size_(0) {}

bool v8::V8::InitializeICU(const char* icu_data_file) {
  return i::InitializeICU(icu_data_file);
}

bool v8::V8::InitializeICUDefaultLocation(const char* exec_path,
                                          const char* icu_data_file) {
  return i::InitializeICUDefaultLocation(exec_path, icu_data_file);
}

void v8::V8::InitializeExternalStartupData(const char* directory_path) {
  i::InitializeExternalStartupData(directory_path);
}

// static
void v8::V8::InitializeExternalStartupDataFromFile(const char* snapshot_blob) {
  i::InitializeExternalStartupDataFromFile(snapshot_blob);
}

const char* v8::V8::GetVersion() { return i::Version::GetVersion(); }

void V8::GetSharedMemoryStatistics(SharedMemoryStatistics* statistics) {
#ifdef V8_SHARED_RO_HEAP
  i::ReadOnlySpace* ro_space = i::ReadOnlyHeap::Instance()->read_only_space();
  statistics->read_only_space_size_ = ro_space->CommittedMemory();
  statistics->read_only_space_used_size_ = ro_space->SizeOfObjects();
  statistics->read_only_space_physical_size_ =
      ro_space->CommittedPhysicalMemory();
#else
  statistics->read_only_space_size_ = 0;
  statistics->read_only_space_used_size_ = 0;
  statistics->read_only_space_physical_size_ = 0;
#endif  // V8_SHARED_RO_HEAP
}

template <typename ObjectType>
struct InvokeBootstrapper;

template <>
struct InvokeBootstrapper<i::Context> {
  i::Handle<i::Context> Invoke(
      i::Isolate* isolate, i::MaybeHandle<i::JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
      v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
      v8::MicrotaskQueue* microtask_queue) {
    return isolate->bootstrapper()->CreateEnvironment(
        maybe_global_proxy, global_proxy_template, extensions,
        context_snapshot_index, embedder_fields_deserializer, microtask_queue);
  }
};

template <>
struct InvokeBootstrapper<i::JSGlobalProxy> {
  i::Handle<i::JSGlobalProxy> Invoke(
      i::Isolate* isolate, i::MaybeHandle<i::JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
      v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
      v8::MicrotaskQueue* microtask_queue) {
    USE(extensions);
    USE(context_snapshot_index);
    return isolate->bootstrapper()->NewRemoteContext(maybe_global_proxy,
                                                     global_proxy_template);
  }
};

template <typename ObjectType>
static i::Handle<ObjectType> CreateEnvironment(
    i::Isolate* isolate, v8::ExtensionConfiguration* extensions,
    v8::MaybeLocal<ObjectTemplate> maybe_global_template,
    v8::MaybeLocal<Value> maybe_global_proxy, size_t context_snapshot_index,
    v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
    v8::MicrotaskQueue* microtask_queue) {
  i::Handle<ObjectType> result;

  {
    ENTER_V8_FOR_NEW_CONTEXT(isolate);
    v8::Local<ObjectTemplate> proxy_template;
    i::Handle<i::FunctionTemplateInfo> proxy_constructor;
    i::Handle<i::FunctionTemplateInfo> global_constructor;
    i::Handle<i::Object> named_interceptor(
        isolate->factory()->undefined_value());
    i::Handle<i::Object> indexed_interceptor(
        isolate->factory()->undefined_value());

    if (!maybe_global_template.IsEmpty()) {
      v8::Local<v8::ObjectTemplate> global_template =
          maybe_global_template.ToLocalChecked();
      // Make sure that the global_template has a constructor.
      global_constructor = EnsureConstructor(isolate, *global_template);

      // Create a fresh template for the global proxy object.
      proxy_template =
          ObjectTemplate::New(reinterpret_cast<v8::Isolate*>(isolate));
      proxy_constructor = EnsureConstructor(isolate, *proxy_template);

      // Set the global template to be the prototype template of
      // global proxy template.
      i::FunctionTemplateInfo::SetPrototypeTemplate(
          isolate, proxy_constructor, Utils::OpenHandle(*global_template));

      proxy_template->SetInternalFieldCount(
          global_template->InternalFieldCount());

      // Migrate security handlers from global_template to
      // proxy_template.  Temporarily removing access check
      // information from the global template.
      if (!global_constructor->GetAccessCheckInfo().IsUndefined(isolate)) {
        i::FunctionTemplateInfo::SetAccessCheckInfo(
            isolate, proxy_constructor,
            i::handle(global_constructor->GetAccessCheckInfo(), isolate));
        proxy_constructor->set_needs_access_check(
            global_constructor->needs_access_check());
        global_constructor->set_needs_access_check(false);
        i::FunctionTemplateInfo::SetAccessCheckInfo(
            isolate, global_constructor,
            i::ReadOnlyRoots(isolate).undefined_value_handle());
      }

      // Same for other interceptors. If the global constructor has
      // interceptors, we need to replace them temporarily with noop
      // interceptors, so the map is correctly marked as having interceptors,
      // but we don't invoke any.
      if (!global_constructor->GetNamedPropertyHandler().IsUndefined(isolate)) {
        named_interceptor =
            handle(global_constructor->GetNamedPropertyHandler(), isolate);
        i::FunctionTemplateInfo::SetNamedPropertyHandler(
            isolate, global_constructor,
            i::ReadOnlyRoots(isolate).noop_interceptor_info_handle());
      }
      if (!global_constructor->GetIndexedPropertyHandler().IsUndefined(
              isolate)) {
        indexed_interceptor =
            handle(global_constructor->GetIndexedPropertyHandler(), isolate);
        i::FunctionTemplateInfo::SetIndexedPropertyHandler(
            isolate, global_constructor,
            i::ReadOnlyRoots(isolate).noop_interceptor_info_handle());
      }
    }

    i::MaybeHandle<i::JSGlobalProxy> maybe_proxy;
    if (!maybe_global_proxy.IsEmpty()) {
      maybe_proxy = i::Handle<i::JSGlobalProxy>::cast(
          Utils::OpenHandle(*maybe_global_proxy.ToLocalChecked()));
    }
    // Create the environment.
    InvokeBootstrapper<ObjectType> invoke;
    result = invoke.Invoke(isolate, maybe_proxy, proxy_template, extensions,
                           context_snapshot_index, embedder_fields_deserializer,
                           microtask_queue);

    // Restore the access check info and interceptors on the global template.
    if (!maybe_global_template.IsEmpty()) {
      DCHECK(!global_constructor.is_null());
      DCHECK(!proxy_constructor.is_null());
      i::FunctionTemplateInfo::SetAccessCheckInfo(
          isolate, global_constructor,
          i::handle(proxy_constructor->GetAccessCheckInfo(), isolate));
      global_constructor->set_needs_access_check(
          proxy_constructor->needs_access_check());
      i::FunctionTemplateInfo::SetNamedPropertyHandler(
          isolate, global_constructor, named_interceptor);
      i::FunctionTemplateInfo::SetIndexedPropertyHandler(
          isolate, global_constructor, indexed_interceptor);
    }
  }
  // Leave V8.

  return result;
}

Local<Context> NewContext(
    v8::Isolate* external_isolate, v8::ExtensionConfiguration* extensions,
    v8::MaybeLocal<ObjectTemplate> global_template,
    v8::MaybeLocal<Value> global_object, size_t context_snapshot_index,
    v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
    v8::MicrotaskQueue* microtask_queue) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  // TODO(jkummerow): This is for crbug.com/713699. Remove it if it doesn't
  // fail.
  // Sanity-check that the isolate is initialized and usable.
  CHECK(isolate->builtins()->builtin(i::Builtins::kIllegal).IsCode());

  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.NewContext");
  LOG_API(isolate, Context, New);
  i::HandleScope scope(isolate);
  ExtensionConfiguration no_extensions;
  if (extensions == nullptr) extensions = &no_extensions;
  i::Handle<i::Context> env = CreateEnvironment<i::Context>(
      isolate, extensions, global_template, global_object,
      context_snapshot_index, embedder_fields_deserializer, microtask_queue);
  if (env.is_null()) {
    if (isolate->has_pending_exception()) isolate->clear_pending_exception();
    return Local<Context>();
  }
  return Utils::ToLocal(scope.CloseAndEscape(env));
}

Local<Context> v8::Context::New(
    v8::Isolate* external_isolate, v8::ExtensionConfiguration* extensions,
    v8::MaybeLocal<ObjectTemplate> global_template,
    v8::MaybeLocal<Value> global_object,
    DeserializeInternalFieldsCallback internal_fields_deserializer,
    v8::MicrotaskQueue* microtask_queue) {
  return NewContext(external_isolate, extensions, global_template,
                    global_object, 0, internal_fields_deserializer,
                    microtask_queue);
}

MaybeLocal<Context> v8::Context::FromSnapshot(
    v8::Isolate* external_isolate, size_t context_snapshot_index,
    v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
    v8::ExtensionConfiguration* extensions, MaybeLocal<Value> global_object,
    v8::MicrotaskQueue* microtask_queue) {
  size_t index_including_default_context = context_snapshot_index + 1;
  if (!i::Snapshot::HasContextSnapshot(
          reinterpret_cast<i::Isolate*>(external_isolate),
          index_including_default_context)) {
    return MaybeLocal<Context>();
  }
  return NewContext(external_isolate, extensions, MaybeLocal<ObjectTemplate>(),
                    global_object, index_including_default_context,
                    embedder_fields_deserializer, microtask_queue);
}

MaybeLocal<Object> v8::Context::NewRemoteContext(
    v8::Isolate* external_isolate, v8::Local<ObjectTemplate> global_template,
    v8::MaybeLocal<v8::Value> global_object) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  LOG_API(isolate, Context, NewRemoteContext);
  i::HandleScope scope(isolate);
  i::Handle<i::FunctionTemplateInfo> global_constructor =
      EnsureConstructor(isolate, *global_template);
  Utils::ApiCheck(global_constructor->needs_access_check(),
                  "v8::Context::NewRemoteContext",
                  "Global template needs to have access checks enabled.");
  i::Handle<i::AccessCheckInfo> access_check_info = i::handle(
      i::AccessCheckInfo::cast(global_constructor->GetAccessCheckInfo()),
      isolate);
  Utils::ApiCheck(access_check_info->named_interceptor() != i::Object(),
                  "v8::Context::NewRemoteContext",
                  "Global template needs to have access check handlers.");
  i::Handle<i::JSObject> global_proxy = CreateEnvironment<i::JSGlobalProxy>(
      isolate, nullptr, global_template, global_object, 0,
      DeserializeInternalFieldsCallback(), nullptr);
  if (global_proxy.is_null()) {
    if (isolate->has_pending_exception()) isolate->clear_pending_exception();
    return MaybeLocal<Object>();
  }
  return Utils::ToLocal(scope.CloseAndEscape(global_proxy));
}

void v8::Context::SetSecurityToken(Local<Value> token) {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Handle<i::Object> token_handle = Utils::OpenHandle(*token);
  env->set_security_token(*token_handle);
}

void v8::Context::UseDefaultSecurityToken() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  env->set_security_token(env->global_object());
}

Local<Value> v8::Context::GetSecurityToken() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  i::Object security_token = env->security_token();
  i::Handle<i::Object> token_handle(security_token, isolate);
  return Utils::ToLocal(token_handle);
}

v8::Isolate* Context::GetIsolate() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  return reinterpret_cast<Isolate*>(env->GetIsolate());
}

v8::Local<v8::Object> Context::Global() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* isolate = context->GetIsolate();
  i::Handle<i::Object> global(context->global_proxy(), isolate);
  // TODO(dcarney): This should always return the global proxy
  // but can't presently as calls to GetProtoype will return the wrong result.
  if (i::Handle<i::JSGlobalProxy>::cast(global)->IsDetachedFrom(
          context->global_object())) {
    global = i::Handle<i::Object>(context->global_object(), isolate);
  }
  return Utils::ToLocal(i::Handle<i::JSObject>::cast(global));
}

void Context::DetachGlobal() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* isolate = context->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  isolate->bootstrapper()->DetachGlobal(context);
}

Local<v8::Object> Context::GetExtrasBindingObject() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* isolate = context->GetIsolate();
  i::Handle<i::JSObject> binding(context->extras_binding_object(), isolate);
  return Utils::ToLocal(binding);
}

void Context::AllowCodeGenerationFromStrings(bool allow) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* isolate = context->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  context->set_allow_code_gen_from_strings(
      allow ? i::ReadOnlyRoots(isolate).true_value()
            : i::ReadOnlyRoots(isolate).false_value());
}

bool Context::IsCodeGenerationFromStringsAllowed() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  return !context->allow_code_gen_from_strings().IsFalse(context->GetIsolate());
}

void Context::SetErrorMessageForCodeGenerationFromStrings(Local<String> error) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Handle<i::String> error_handle = Utils::OpenHandle(*error);
  context->set_error_message_for_code_gen_from_strings(*error_handle);
}

void Context::SetAbortScriptExecution(
    Context::AbortScriptExecutionCallback callback) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* isolate = context->GetIsolate();
  if (callback == nullptr) {
    context->set_script_execution_callback(
        i::ReadOnlyRoots(isolate).undefined_value());
  } else {
    SET_FIELD_WRAPPED(isolate, context, set_script_execution_callback,
                      callback);
  }
}

namespace {
i::Address* GetSerializedDataFromFixedArray(i::Isolate* isolate,
                                            i::FixedArray list, size_t index) {
  if (index < static_cast<size_t>(list.length())) {
    int int_index = static_cast<int>(index);
    i::Object object = list.get(int_index);
    if (!object.IsTheHole(isolate)) {
      list.set_the_hole(isolate, int_index);
      // Shrink the list so that the last element is not the hole (unless it's
      // the first element, because we don't want to end up with a non-canonical
      // empty FixedArray).
      int last = list.length() - 1;
      while (last >= 0 && list.is_the_hole(isolate, last)) last--;
      if (last != -1) list.Shrink(isolate, last + 1);
      return i::Handle<i::Object>(object, isolate).location();
    }
  }
  return nullptr;
}
}  // anonymous namespace

i::Address* Context::GetDataFromSnapshotOnce(size_t index) {
  auto context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  i::FixedArray list = context->serialized_objects();
  return GetSerializedDataFromFixedArray(i_isolate, list, index);
}

MaybeLocal<v8::Object> ObjectTemplate::NewInstance(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, ObjectTemplate, NewInstance, Object);
  auto self = Utils::OpenHandle(this);
  Local<Object> result;
  has_pending_exception = !ToLocal<Object>(
      i::ApiNatives::InstantiateObject(isolate, self), &result);
  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}

void v8::ObjectTemplate::CheckCast(Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsObjectTemplateInfo(), "v8::ObjectTemplate::Cast",
                  "Value is not an ObjectTemplate");
}

void v8::FunctionTemplate::CheckCast(Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsFunctionTemplateInfo(), "v8::FunctionTemplate::Cast",
                  "Value is not a FunctionTemplate");
}

void v8::Signature::CheckCast(Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsFunctionTemplateInfo(), "v8::Signature::Cast",
                  "Value is not a Signature");
}

void v8::AccessorSignature::CheckCast(Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsFunctionTemplateInfo(), "v8::AccessorSignature::Cast",
                  "Value is not an AccessorSignature");
}

MaybeLocal<v8::Function> FunctionTemplate::GetFunction(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, FunctionTemplate, GetFunction, Function);
  auto self = Utils::OpenHandle(this);
  Local<Function> result;
  has_pending_exception =
      !ToLocal<Function>(i::ApiNatives::InstantiateFunction(self), &result);
  RETURN_ON_FAILED_EXECUTION(Function);
  RETURN_ESCAPED(result);
}

MaybeLocal<v8::Object> FunctionTemplate::NewRemoteInstance() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  LOG_API(isolate, FunctionTemplate, NewRemoteInstance);
  i::HandleScope scope(isolate);
  i::Handle<i::FunctionTemplateInfo> constructor =
      EnsureConstructor(isolate, *InstanceTemplate());
  Utils::ApiCheck(constructor->needs_access_check(),
                  "v8::FunctionTemplate::NewRemoteInstance",
                  "InstanceTemplate needs to have access checks enabled.");
  i::Handle<i::AccessCheckInfo> access_check_info = i::handle(
      i::AccessCheckInfo::cast(constructor->GetAccessCheckInfo()), isolate);
  Utils::ApiCheck(access_check_info->named_interceptor() != i::Object(),
                  "v8::FunctionTemplate::NewRemoteInstance",
                  "InstanceTemplate needs to have access check handlers.");
  i::Handle<i::JSObject> object;
  if (!i::ApiNatives::InstantiateRemoteObject(
           Utils::OpenHandle(*InstanceTemplate()))
           .ToHandle(&object)) {
    if (isolate->has_pending_exception()) {
      isolate->OptionalRescheduleException(true);
    }
    return MaybeLocal<Object>();
  }
  return Utils::ToLocal(scope.CloseAndEscape(object));
}

bool FunctionTemplate::HasInstance(v8::Local<v8::Value> value) {
  auto self = Utils::OpenHandle(this);
  auto obj = Utils::OpenHandle(*value);
  if (obj->IsJSObject() && self->IsTemplateFor(i::JSObject::cast(*obj))) {
    return true;
  }
  if (obj->IsJSGlobalProxy()) {
    // If it's a global proxy, then test with the global object. Note that the
    // inner global object may not necessarily be a JSGlobalObject.
    i::PrototypeIterator iter(self->GetIsolate(),
                              i::JSObject::cast(*obj).map());
    // The global proxy should always have a prototype, as it is a bug to call
    // this on a detached JSGlobalProxy.
    DCHECK(!iter.IsAtEnd());
    return self->IsTemplateFor(iter.GetCurrent<i::JSObject>());
  }
  return false;
}

Local<External> v8::External::New(Isolate* isolate, void* value) {
  STATIC_ASSERT(sizeof(value) == sizeof(i::Address));
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, External, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSObject> external = i_isolate->factory()->NewExternal(value);
  return Utils::ExternalToLocal(external);
}

void* External::Value() const {
  return ExternalValue(*Utils::OpenHandle(this));
}

// anonymous namespace for string creation helper functions
namespace {

inline int StringLength(const char* string) {
  size_t len = strlen(string);
  CHECK_GE(i::kMaxInt, len);
  return static_cast<int>(len);
}

inline int StringLength(const uint8_t* string) {
  return StringLength(reinterpret_cast<const char*>(string));
}

inline int StringLength(const uint16_t* string) {
  size_t length = 0;
  while (string[length] != '\0') length++;
  CHECK_GE(i::kMaxInt, length);
  return static_cast<int>(length);
}

V8_WARN_UNUSED_RESULT
inline i::MaybeHandle<i::String> NewString(i::Factory* factory,
                                           NewStringType type,
                                           i::Vector<const char> string) {
  if (type == NewStringType::kInternalized) {
    return factory->InternalizeUtf8String(string);
  }
  return factory->NewStringFromUtf8(string);
}

V8_WARN_UNUSED_RESULT
inline i::MaybeHandle<i::String> NewString(i::Factory* factory,
                                           NewStringType type,
                                           i::Vector<const uint8_t> string) {
  if (type == NewStringType::kInternalized) {
    return factory->InternalizeString(string);
  }
  return factory->NewStringFromOneByte(string);
}

V8_WARN_UNUSED_RESULT
inline i::MaybeHandle<i::String> NewString(i::Factory* factory,
                                           NewStringType type,
                                           i::Vector<const uint16_t> string) {
  if (type == NewStringType::kInternalized) {
    return factory->InternalizeString(string);
  }
  return factory->NewStringFromTwoByte(string);
}

STATIC_ASSERT(v8::String::kMaxLength == i::String::kMaxLength);

}  // anonymous namespace

// TODO(dcarney): throw a context free exception.
#define NEW_STRING(isolate, class_name, function_name, Char, data, type,   \
                   length)                                                 \
  MaybeLocal<String> result;                                               \
  if (length == 0) {                                                       \
    result = String::Empty(isolate);                                       \
  } else if (length > i::String::kMaxLength) {                             \
    result = MaybeLocal<String>();                                         \
  } else {                                                                 \
    i::Isolate* i_isolate = reinterpret_cast<internal::Isolate*>(isolate); \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);                            \
    LOG_API(i_isolate, class_name, function_name);                         \
    if (length < 0) length = StringLength(data);                           \
    i::Handle<i::String> handle_result =                                   \
        NewString(i_isolate->factory(), type,                              \
                  i::Vector<const Char>(data, length))                     \
            .ToHandleChecked();                                            \
    result = Utils::ToLocal(handle_result);                                \
  }

MaybeLocal<String> String::NewFromUtf8(Isolate* isolate, const char* data,
                                       NewStringType type, int length) {
  NEW_STRING(isolate, String, NewFromUtf8, char, data, type, length);
  return result;
}

MaybeLocal<String> String::NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                          NewStringType type, int length) {
  NEW_STRING(isolate, String, NewFromOneByte, uint8_t, data, type, length);
  return result;
}

MaybeLocal<String> String::NewFromTwoByte(Isolate* isolate,
                                          const uint16_t* data,
                                          NewStringType type, int length) {
  NEW_STRING(isolate, String, NewFromTwoByte, uint16_t, data, type, length);
  return result;
}

Local<String> v8::String::Concat(Isolate* v8_isolate, Local<String> left,
                                 Local<String> right) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::String> left_string = Utils::OpenHandle(*left);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  LOG_API(isolate, String, Concat);
  i::Handle<i::String> right_string = Utils::OpenHandle(*right);
  // If we are steering towards a range error, do not wait for the error to be
  // thrown, and return the null handle instead.
  if (left_string->length() + right_string->length() > i::String::kMaxLength) {
    return Local<String>();
  }
  i::Handle<i::String> result = isolate->factory()
                                    ->NewConsString(left_string, right_string)
                                    .ToHandleChecked();
  return Utils::ToLocal(result);
}

MaybeLocal<String> v8::String::NewExternalTwoByte(
    Isolate* isolate, v8::String::ExternalStringResource* resource) {
  CHECK(resource && resource->data());
  // TODO(dcarney): throw a context free exception.
  if (resource->length() > static_cast<size_t>(i::String::kMaxLength)) {
    return MaybeLocal<String>();
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  LOG_API(i_isolate, String, NewExternalTwoByte);
  if (resource->length() > 0) {
    i::Handle<i::String> string = i_isolate->factory()
                                      ->NewExternalStringFromTwoByte(resource)
                                      .ToHandleChecked();
    return Utils::ToLocal(string);
  } else {
    // The resource isn't going to be used, free it immediately.
    resource->Dispose();
    return Utils::ToLocal(i_isolate->factory()->empty_string());
  }
}

MaybeLocal<String> v8::String::NewExternalOneByte(
    Isolate* isolate, v8::String::ExternalOneByteStringResource* resource) {
  CHECK_NOT_NULL(resource);
  // TODO(dcarney): throw a context free exception.
  if (resource->length() > static_cast<size_t>(i::String::kMaxLength)) {
    return MaybeLocal<String>();
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  LOG_API(i_isolate, String, NewExternalOneByte);
  if (resource->length() == 0) {
    // The resource isn't going to be used, free it immediately.
    resource->Dispose();
    return Utils::ToLocal(i_isolate->factory()->empty_string());
  }
  CHECK_NOT_NULL(resource->data());
  i::Handle<i::String> string = i_isolate->factory()
                                    ->NewExternalStringFromOneByte(resource)
                                    .ToHandleChecked();
  return Utils::ToLocal(string);
}

bool v8::String::MakeExternal(v8::String::ExternalStringResource* resource) {
  i::DisallowHeapAllocation no_allocation;

  i::String obj = *Utils::OpenHandle(this);

  if (obj.IsThinString()) {
    obj = i::ThinString::cast(obj).actual();
  }

  if (!obj.SupportsExternalization()) {
    return false;
  }

  // It is safe to call GetIsolateFromWritableHeapObject because
  // SupportsExternalization already checked that the object is writable.
  i::Isolate* isolate = i::GetIsolateFromWritableObject(obj);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);

  CHECK(resource && resource->data());

  bool result = obj.MakeExternal(resource);
  DCHECK(result);
  DCHECK(obj.IsExternalString());
  return result;
}

bool v8::String::MakeExternal(
    v8::String::ExternalOneByteStringResource* resource) {
  i::DisallowHeapAllocation no_allocation;

  i::String obj = *Utils::OpenHandle(this);

  if (obj.IsThinString()) {
    obj = i::ThinString::cast(obj).actual();
  }

  if (!obj.SupportsExternalization()) {
    return false;
  }

  // It is safe to call GetIsolateFromWritableHeapObject because
  // SupportsExternalization already checked that the object is writable.
  i::Isolate* isolate = i::GetIsolateFromWritableObject(obj);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);

  CHECK(resource && resource->data());

  bool result = obj.MakeExternal(resource);
  DCHECK_IMPLIES(result, obj.IsExternalString());
  return result;
}

bool v8::String::CanMakeExternal() {
  i::DisallowHeapAllocation no_allocation;
  i::String obj = *Utils::OpenHandle(this);

  if (obj.IsThinString()) {
    obj = i::ThinString::cast(obj).actual();
  }

  if (!obj.SupportsExternalization()) {
    return false;
  }

  // Only old space strings should be externalized.
  return !i::Heap::InYoungGeneration(obj);
}

bool v8::String::StringEquals(Local<String> that) {
  auto self = Utils::OpenHandle(this);
  auto other = Utils::OpenHandle(*that);
  return self->Equals(*other);
}

Isolate* v8::Object::GetIsolate() {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  return reinterpret_cast<Isolate*>(i_isolate);
}

Local<v8::Object> v8::Object::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Object, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSObject> obj =
      i_isolate->factory()->NewJSObject(i_isolate->object_function());
  return Utils::ToLocal(obj);
}

Local<v8::Object> v8::Object::New(Isolate* isolate,
                                  Local<Value> prototype_or_null,
                                  Local<Name>* names, Local<Value>* values,
                                  size_t length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::Object> proto = Utils::OpenHandle(*prototype_or_null);
  if (!Utils::ApiCheck(proto->IsNull() || proto->IsJSReceiver(),
                       "v8::Object::New", "prototype must be null or object")) {
    return Local<v8::Object>();
  }
  LOG_API(i_isolate, Object, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

  // We assume that this API is mostly used to create objects with named
  // properties, and so we default to creating a properties backing store
  // large enough to hold all of them, while we start with no elements
  // (see http://bit.ly/v8-fast-object-create-cpp for the motivation).
  i::Handle<i::NameDictionary> properties =
      i::NameDictionary::New(i_isolate, static_cast<int>(length));
  i::Handle<i::FixedArrayBase> elements =
      i_isolate->factory()->empty_fixed_array();
  for (size_t i = 0; i < length; ++i) {
    i::Handle<i::Name> name = Utils::OpenHandle(*names[i]);
    i::Handle<i::Object> value = Utils::OpenHandle(*values[i]);

    // See if the {name} is a valid array index, in which case we need to
    // add the {name}/{value} pair to the {elements}, otherwise they end
    // up in the {properties} backing store.
    uint32_t index;
    if (name->AsArrayIndex(&index)) {
      // If this is the first element, allocate a proper
      // dictionary elements backing store for {elements}.
      if (!elements->IsNumberDictionary()) {
        elements =
            i::NumberDictionary::New(i_isolate, static_cast<int>(length));
      }
      elements = i::NumberDictionary::Set(
          i_isolate, i::Handle<i::NumberDictionary>::cast(elements), index,
          value);
    } else {
      // Internalize the {name} first.
      name = i_isolate->factory()->InternalizeName(name);
      i::InternalIndex const entry = properties->FindEntry(i_isolate, name);
      if (entry.is_not_found()) {
        // Add the {name}/{value} pair as a new entry.
        properties = i::NameDictionary::Add(i_isolate, properties, name, value,
                                            i::PropertyDetails::Empty());
      } else {
        // Overwrite the {entry} with the {value}.
        properties->ValueAtPut(entry, *value);
      }
    }
  }
  i::Handle<i::JSObject> obj =
      i_isolate->factory()->NewSlowJSObjectWithPropertiesAndElements(
          i::Handle<i::HeapObject>::cast(proto), properties, elements);
  return Utils::ToLocal(obj);
}

Local<v8::Value> v8::NumberObject::New(Isolate* isolate, double value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, NumberObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> number = i_isolate->factory()->NewNumber(value);
  i::Handle<i::Object> obj =
      i::Object::ToObject(i_isolate, number).ToHandleChecked();
  return Utils::ToLocal(obj);
}

double v8::NumberObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSPrimitiveWrapper> js_primitive_wrapper =
      i::Handle<i::JSPrimitiveWrapper>::cast(obj);
  i::Isolate* isolate = js_primitive_wrapper->GetIsolate();
  LOG_API(isolate, NumberObject, NumberValue);
  return js_primitive_wrapper->value().Number();
}

Local<v8::Value> v8::BigIntObject::New(Isolate* isolate, int64_t value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, BigIntObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> bigint = i::BigInt::FromInt64(i_isolate, value);
  i::Handle<i::Object> obj =
      i::Object::ToObject(i_isolate, bigint).ToHandleChecked();
  return Utils::ToLocal(obj);
}

Local<v8::BigInt> v8::BigIntObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSPrimitiveWrapper> js_primitive_wrapper =
      i::Handle<i::JSPrimitiveWrapper>::cast(obj);
  i::Isolate* isolate = js_primitive_wrapper->GetIsolate();
  LOG_API(isolate, BigIntObject, BigIntValue);
  return Utils::ToLocal(i::Handle<i::BigInt>(
      i::BigInt::cast(js_primitive_wrapper->value()), isolate));
}

Local<v8::Value> v8::BooleanObject::New(Isolate* isolate, bool value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, BooleanObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> boolean(value
                                   ? i::ReadOnlyRoots(i_isolate).true_value()
                                   : i::ReadOnlyRoots(i_isolate).false_value(),
                               i_isolate);
  i::Handle<i::Object> obj =
      i::Object::ToObject(i_isolate, boolean).ToHandleChecked();
  return Utils::ToLocal(obj);
}

bool v8::BooleanObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSPrimitiveWrapper> js_primitive_wrapper =
      i::Handle<i::JSPrimitiveWrapper>::cast(obj);
  i::Isolate* isolate = js_primitive_wrapper->GetIsolate();
  LOG_API(isolate, BooleanObject, BooleanValue);
  return js_primitive_wrapper->value().IsTrue(isolate);
}

Local<v8::Value> v8::StringObject::New(Isolate* v8_isolate,
                                       Local<String> value) {
  i::Handle<i::String> string = Utils::OpenHandle(*value);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  LOG_API(isolate, StringObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Object> obj =
      i::Object::ToObject(isolate, string).ToHandleChecked();
  return Utils::ToLocal(obj);
}

Local<v8::String> v8::StringObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSPrimitiveWrapper> js_primitive_wrapper =
      i::Handle<i::JSPrimitiveWrapper>::cast(obj);
  i::Isolate* isolate = js_primitive_wrapper->GetIsolate();
  LOG_API(isolate, StringObject, StringValue);
  return Utils::ToLocal(i::Handle<i::String>(
      i::String::cast(js_primitive_wrapper->value()), isolate));
}

Local<v8::Value> v8::SymbolObject::New(Isolate* isolate, Local<Symbol> value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, SymbolObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> obj =
      i::Object::ToObject(i_isolate, Utils::OpenHandle(*value))
          .ToHandleChecked();
  return Utils::ToLocal(obj);
}

Local<v8::Symbol> v8::SymbolObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSPrimitiveWrapper> js_primitive_wrapper =
      i::Handle<i::JSPrimitiveWrapper>::cast(obj);
  i::Isolate* isolate = js_primitive_wrapper->GetIsolate();
  LOG_API(isolate, SymbolObject, SymbolValue);
  return Utils::ToLocal(i::Handle<i::Symbol>(
      i::Symbol::cast(js_primitive_wrapper->value()), isolate));
}

MaybeLocal<v8::Value> v8::Date::New(Local<Context> context, double time) {
  if (std::isnan(time)) {
    // Introduce only canonical NaN value into the VM, to avoid signaling NaNs.
    time = std::numeric_limits<double>::quiet_NaN();
  }
  PREPARE_FOR_EXECUTION(context, Date, New, Value);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::JSDate::New(isolate->date_function(), isolate->date_function(), time),
      &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

double v8::Date::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSDate> jsdate = i::Handle<i::JSDate>::cast(obj);
  i::Isolate* isolate = jsdate->GetIsolate();
  LOG_API(isolate, Date, NumberValue);
  return jsdate->value().Number();
}

// Assert that the static TimeZoneDetection cast in
// DateTimeConfigurationChangeNotification is valid.
#define TIME_ZONE_DETECTION_ASSERT_EQ(value)                     \
  STATIC_ASSERT(                                                 \
      static_cast<int>(v8::Isolate::TimeZoneDetection::value) == \
      static_cast<int>(base::TimezoneCache::TimeZoneDetection::value));
TIME_ZONE_DETECTION_ASSERT_EQ(kSkip)
TIME_ZONE_DETECTION_ASSERT_EQ(kRedetect)
#undef TIME_ZONE_DETECTION_ASSERT_EQ

MaybeLocal<v8::RegExp> v8::RegExp::New(Local<Context> context,
                                       Local<String> pattern, Flags flags) {
  PREPARE_FOR_EXECUTION(context, RegExp, New, RegExp);
  Local<v8::RegExp> result;
  has_pending_exception =
      !ToLocal<RegExp>(i::JSRegExp::New(isolate, Utils::OpenHandle(*pattern),
                                        static_cast<i::JSRegExp::Flags>(flags)),
                       &result);
  RETURN_ON_FAILED_EXECUTION(RegExp);
  RETURN_ESCAPED(result);
}

MaybeLocal<v8::RegExp> v8::RegExp::NewWithBacktrackLimit(
    Local<Context> context, Local<String> pattern, Flags flags,
    uint32_t backtrack_limit) {
  CHECK(i::Smi::IsValid(backtrack_limit));
  CHECK_NE(backtrack_limit, i::JSRegExp::kNoBacktrackLimit);
  PREPARE_FOR_EXECUTION(context, RegExp, New, RegExp);
  Local<v8::RegExp> result;
  has_pending_exception = !ToLocal<RegExp>(
      i::JSRegExp::New(isolate, Utils::OpenHandle(*pattern),
                       static_cast<i::JSRegExp::Flags>(flags), backtrack_limit),
      &result);
  RETURN_ON_FAILED_EXECUTION(RegExp);
  RETURN_ESCAPED(result);
}

Local<v8::String> v8::RegExp::GetSource() const {
  i::Handle<i::JSRegExp> obj = Utils::OpenHandle(this);
  return Utils::ToLocal(
      i::Handle<i::String>(obj->Pattern(), obj->GetIsolate()));
}

// Assert that the static flags cast in GetFlags is valid.
#define REGEXP_FLAG_ASSERT_EQ(flag)                   \
  STATIC_ASSERT(static_cast<int>(v8::RegExp::flag) == \
                static_cast<int>(i::JSRegExp::flag))
REGEXP_FLAG_ASSERT_EQ(kNone);
REGEXP_FLAG_ASSERT_EQ(kGlobal);
REGEXP_FLAG_ASSERT_EQ(kIgnoreCase);
REGEXP_FLAG_ASSERT_EQ(kMultiline);
REGEXP_FLAG_ASSERT_EQ(kSticky);
REGEXP_FLAG_ASSERT_EQ(kUnicode);
#undef REGEXP_FLAG_ASSERT_EQ

v8::RegExp::Flags v8::RegExp::GetFlags() const {
  i::Handle<i::JSRegExp> obj = Utils::OpenHandle(this);
  return RegExp::Flags(static_cast<int>(obj->GetFlags()));
}

MaybeLocal<v8::Object> v8::RegExp::Exec(Local<Context> context,
                                        Local<v8::String> subject) {
  PREPARE_FOR_EXECUTION(context, RegExp, Exec, Object);

  i::Handle<i::JSRegExp> regexp = Utils::OpenHandle(this);
  i::Handle<i::String> subject_string = Utils::OpenHandle(*subject);

  // TODO(jgruber): RegExpUtils::RegExpExec was not written with efficiency in
  // mind. It fetches the 'exec' property and then calls it through JSEntry.
  // Unfortunately, this is currently the only full implementation of
  // RegExp.prototype.exec available in C++.
  Local<v8::Object> result;
  has_pending_exception = !ToLocal<Object>(
      i::RegExpUtils::RegExpExec(isolate, regexp, subject_string,
                                 isolate->factory()->undefined_value()),
      &result);

  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}

Local<v8::Array> v8::Array::New(Isolate* isolate, int length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Array, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  int real_length = length > 0 ? length : 0;
  i::Handle<i::JSArray> obj = i_isolate->factory()->NewJSArray(real_length);
  i::Handle<i::Object> length_obj =
      i_isolate->factory()->NewNumberFromInt(real_length);
  obj->set_length(*length_obj);
  return Utils::ToLocal(obj);
}

Local<v8::Array> v8::Array::New(Isolate* isolate, Local<Value>* elements,
                                size_t length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Factory* factory = i_isolate->factory();
  LOG_API(i_isolate, Array, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  int len = static_cast<int>(length);

  i::Handle<i::FixedArray> result = factory->NewFixedArray(len);
  for (int i = 0; i < len; i++) {
    i::Handle<i::Object> element = Utils::OpenHandle(*elements[i]);
    result->set(i, *element);
  }

  return Utils::ToLocal(
      factory->NewJSArrayWithElements(result, i::PACKED_ELEMENTS, len));
}

uint32_t v8::Array::Length() const {
  i::Handle<i::JSArray> obj = Utils::OpenHandle(this);
  i::Object length = obj->length();
  if (length.IsSmi()) {
    return i::Smi::ToInt(length);
  } else {
    return static_cast<uint32_t>(length.Number());
  }
}

Local<v8::Map> v8::Map::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Map, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSMap> obj = i_isolate->factory()->NewJSMap();
  return Utils::ToLocal(obj);
}

size_t v8::Map::Size() const {
  i::Handle<i::JSMap> obj = Utils::OpenHandle(this);
  return i::OrderedHashMap::cast(obj->table()).NumberOfElements();
}

void Map::Clear() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  LOG_API(isolate, Map, Clear);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::JSMap::Clear(isolate, self);
}

MaybeLocal<Value> Map::Get(Local<Context> context, Local<Value> key) {
  PREPARE_FOR_EXECUTION(context, Map, Get, Value);
  auto self = Utils::OpenHandle(this);
  Local<Value> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !ToLocal<Value>(i::Execution::CallBuiltin(isolate, isolate->map_get(),
                                                self, arraysize(argv), argv),
                      &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<Map> Map::Set(Local<Context> context, Local<Value> key,
                         Local<Value> value) {
  PREPARE_FOR_EXECUTION(context, Map, Set, Map);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key),
                                 Utils::OpenHandle(*value)};
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->map_set(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Map);
  RETURN_ESCAPED(Local<Map>::Cast(Utils::ToLocal(result)));
}

Maybe<bool> Map::Has(Local<Context> context, Local<Value> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Map, Has, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->map_has(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(isolate));
}

Maybe<bool> Map::Delete(Local<Context> context, Local<Value> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Map, Delete, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->map_delete(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(isolate));
}

namespace {

enum class MapAsArrayKind {
  kEntries = i::JS_MAP_KEY_VALUE_ITERATOR_TYPE,
  kKeys = i::JS_MAP_KEY_ITERATOR_TYPE,
  kValues = i::JS_MAP_VALUE_ITERATOR_TYPE
};

enum class SetAsArrayKind {
  kEntries = i::JS_SET_KEY_VALUE_ITERATOR_TYPE,
  kValues = i::JS_SET_VALUE_ITERATOR_TYPE
};

i::Handle<i::JSArray> MapAsArray(i::Isolate* isolate, i::Object table_obj,
                                 int offset, MapAsArrayKind kind) {
  i::Factory* factory = isolate->factory();
  i::Handle<i::OrderedHashMap> table(i::OrderedHashMap::cast(table_obj),
                                     isolate);
  const bool collect_keys =
      kind == MapAsArrayKind::kEntries || kind == MapAsArrayKind::kKeys;
  const bool collect_values =
      kind == MapAsArrayKind::kEntries || kind == MapAsArrayKind::kValues;
  int capacity = table->UsedCapacity();
  int max_length =
      (capacity - offset) * ((collect_keys && collect_values) ? 2 : 1);
  i::Handle<i::FixedArray> result = factory->NewFixedArray(max_length);
  int result_index = 0;
  {
    i::DisallowHeapAllocation no_gc;
    i::Oddball the_hole = i::ReadOnlyRoots(isolate).the_hole_value();
    for (int i = offset; i < capacity; ++i) {
      i::Object key = table->KeyAt(i);
      if (key == the_hole) continue;
      if (collect_keys) result->set(result_index++, key);
      if (collect_values) result->set(result_index++, table->ValueAt(i));
    }
  }
  DCHECK_GE(max_length, result_index);
  if (result_index == 0) return factory->NewJSArray(0);
  result->Shrink(isolate, result_index);
  return factory->NewJSArrayWithElements(result, i::PACKED_ELEMENTS,
                                         result_index);
}

}  // namespace

Local<Array> Map::AsArray() const {
  i::Handle<i::JSMap> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, Map, AsArray);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  return Utils::ToLocal(
      MapAsArray(isolate, obj->table(), 0, MapAsArrayKind::kEntries));
}

Local<v8::Set> v8::Set::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Set, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSSet> obj = i_isolate->factory()->NewJSSet();
  return Utils::ToLocal(obj);
}

size_t v8::Set::Size() const {
  i::Handle<i::JSSet> obj = Utils::OpenHandle(this);
  return i::OrderedHashSet::cast(obj->table()).NumberOfElements();
}

void Set::Clear() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  LOG_API(isolate, Set, Clear);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::JSSet::Clear(isolate, self);
}

MaybeLocal<Set> Set::Add(Local<Context> context, Local<Value> key) {
  PREPARE_FOR_EXECUTION(context, Set, Add, Set);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->set_add(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Set);
  RETURN_ESCAPED(Local<Set>::Cast(Utils::ToLocal(result)));
}

Maybe<bool> Set::Has(Local<Context> context, Local<Value> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Set, Has, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->set_has(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(isolate));
}

Maybe<bool> Set::Delete(Local<Context> context, Local<Value> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Set, Delete, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->set_delete(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(isolate));
}

namespace {
i::Handle<i::JSArray> SetAsArray(i::Isolate* isolate, i::Object table_obj,
                                 int offset, SetAsArrayKind kind) {
  i::Factory* factory = isolate->factory();
  i::Handle<i::OrderedHashSet> table(i::OrderedHashSet::cast(table_obj),
                                     isolate);
  // Elements skipped by |offset| may already be deleted.
  int capacity = table->UsedCapacity();
  const bool collect_key_values = kind == SetAsArrayKind::kEntries;
  int max_length = (capacity - offset) * (collect_key_values ? 2 : 1);
  if (max_length == 0) return factory->NewJSArray(0);
  i::Handle<i::FixedArray> result = factory->NewFixedArray(max_length);
  int result_index = 0;
  {
    i::DisallowHeapAllocation no_gc;
    i::Oddball the_hole = i::ReadOnlyRoots(isolate).the_hole_value();
    for (int i = offset; i < capacity; ++i) {
      i::Object key = table->KeyAt(i);
      if (key == the_hole) continue;
      result->set(result_index++, key);
      if (collect_key_values) result->set(result_index++, key);
    }
  }
  DCHECK_GE(max_length, result_index);
  if (result_index == 0) return factory->NewJSArray(0);
  result->Shrink(isolate, result_index);
  return factory->NewJSArrayWithElements(result, i::PACKED_ELEMENTS,
                                         result_index);
}
}  // namespace

Local<Array> Set::AsArray() const {
  i::Handle<i::JSSet> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, Set, AsArray);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  return Utils::ToLocal(
      SetAsArray(isolate, obj->table(), 0, SetAsArrayKind::kValues));
}

MaybeLocal<Promise::Resolver> Promise::Resolver::New(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, Promise_Resolver, New, Resolver);
  Local<Promise::Resolver> result;
  has_pending_exception =
      !ToLocal<Promise::Resolver>(isolate->factory()->NewJSPromise(), &result);
  RETURN_ON_FAILED_EXECUTION(Promise::Resolver);
  RETURN_ESCAPED(result);
}

Local<Promise> Promise::Resolver::GetPromise() {
  i::Handle<i::JSReceiver> promise = Utils::OpenHandle(this);
  return Local<Promise>::Cast(Utils::ToLocal(promise));
}

Maybe<bool> Promise::Resolver::Resolve(Local<Context> context,
                                       Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Promise_Resolver, Resolve, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto promise = i::Handle<i::JSPromise>::cast(self);

  if (promise->status() != Promise::kPending) {
    return Just(true);
  }

  has_pending_exception =
      i::JSPromise::Resolve(promise, Utils::OpenHandle(*value)).is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

Maybe<bool> Promise::Resolver::Reject(Local<Context> context,
                                      Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Promise_Resolver, Reject, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto promise = i::Handle<i::JSPromise>::cast(self);

  if (promise->status() != Promise::kPending) {
    return Just(true);
  }

  has_pending_exception =
      i::JSPromise::Reject(promise, Utils::OpenHandle(*value)).is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

MaybeLocal<Promise> Promise::Catch(Local<Context> context,
                                   Local<Function> handler) {
  PREPARE_FOR_EXECUTION(context, Promise, Catch, Promise);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*handler)};
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->promise_catch(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Promise);
  RETURN_ESCAPED(Local<Promise>::Cast(Utils::ToLocal(result)));
}

MaybeLocal<Promise> Promise::Then(Local<Context> context,
                                  Local<Function> handler) {
  PREPARE_FOR_EXECUTION(context, Promise, Then, Promise);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*handler)};
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->promise_then(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Promise);
  RETURN_ESCAPED(Local<Promise>::Cast(Utils::ToLocal(result)));
}

MaybeLocal<Promise> Promise::Then(Local<Context> context,
                                  Local<Function> on_fulfilled,
                                  Local<Function> on_rejected) {
  PREPARE_FOR_EXECUTION(context, Promise, Then, Promise);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*on_fulfilled),
                                 Utils::OpenHandle(*on_rejected)};
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->promise_then(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Promise);
  RETURN_ESCAPED(Local<Promise>::Cast(Utils::ToLocal(result)));
}

bool Promise::HasHandler() {
  i::Handle<i::JSReceiver> promise = Utils::OpenHandle(this);
  i::Isolate* isolate = promise->GetIsolate();
  LOG_API(isolate, Promise, HasRejectHandler);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (promise->IsJSPromise()) {
    i::Handle<i::JSPromise> js_promise = i::Handle<i::JSPromise>::cast(promise);
    return js_promise->has_handler();
  }
  return false;
}

Local<Value> Promise::Result() {
  i::Handle<i::JSReceiver> promise = Utils::OpenHandle(this);
  i::Isolate* isolate = promise->GetIsolate();
  LOG_API(isolate, Promise, Result);
  i::Handle<i::JSPromise> js_promise = i::Handle<i::JSPromise>::cast(promise);
  Utils::ApiCheck(js_promise->status() != kPending, "v8_Promise_Result",
                  "Promise is still pending");
  i::Handle<i::Object> result(js_promise->result(), isolate);
  return Utils::ToLocal(result);
}

Promise::PromiseState Promise::State() {
  i::Handle<i::JSReceiver> promise = Utils::OpenHandle(this);
  i::Isolate* isolate = promise->GetIsolate();
  LOG_API(isolate, Promise, Status);
  i::Handle<i::JSPromise> js_promise = i::Handle<i::JSPromise>::cast(promise);
  return static_cast<PromiseState>(js_promise->status());
}

void Promise::MarkAsHandled() {
  i::Handle<i::JSPromise> js_promise = Utils::OpenHandle(this);
  js_promise->set_has_handler(true);
}

Local<Value> Proxy::GetTarget() {
  i::Handle<i::JSProxy> self = Utils::OpenHandle(this);
  i::Handle<i::Object> target(self->target(), self->GetIsolate());
  return Utils::ToLocal(target);
}

Local<Value> Proxy::GetHandler() {
  i::Handle<i::JSProxy> self = Utils::OpenHandle(this);
  i::Handle<i::Object> handler(self->handler(), self->GetIsolate());
  return Utils::ToLocal(handler);
}

bool Proxy::IsRevoked() {
  i::Handle<i::JSProxy> self = Utils::OpenHandle(this);
  return self->IsRevoked();
}

void Proxy::Revoke() {
  i::Handle<i::JSProxy> self = Utils::OpenHandle(this);
  i::JSProxy::Revoke(self);
}

MaybeLocal<Proxy> Proxy::New(Local<Context> context, Local<Object> local_target,
                             Local<Object> local_handler) {
  PREPARE_FOR_EXECUTION(context, Proxy, New, Proxy);
  i::Handle<i::JSReceiver> target = Utils::OpenHandle(*local_target);
  i::Handle<i::JSReceiver> handler = Utils::OpenHandle(*local_handler);
  Local<Proxy> result;
  has_pending_exception =
      !ToLocal<Proxy>(i::JSProxy::New(isolate, target, handler), &result);
  RETURN_ON_FAILED_EXECUTION(Proxy);
  RETURN_ESCAPED(result);
}

CompiledWasmModule::CompiledWasmModule(
    std::shared_ptr<internal::wasm::NativeModule> native_module)
    : native_module_(std::move(native_module)) {
  CHECK_NOT_NULL(native_module_);
}

OwnedBuffer CompiledWasmModule::Serialize() {
  i::wasm::WasmSerializer wasm_serializer(native_module_.get());
  size_t buffer_size = wasm_serializer.GetSerializedNativeModuleSize();
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  if (!wasm_serializer.SerializeNativeModule({buffer.get(), buffer_size}))
    return {};
  return {std::move(buffer), buffer_size};
}

MemorySpan<const uint8_t> CompiledWasmModule::GetWireBytesRef() {
  i::Vector<const uint8_t> bytes_vec = native_module_->wire_bytes();
  return {bytes_vec.begin(), bytes_vec.size()};
}

CompiledWasmModule WasmModuleObject::GetCompiledModule() {
  i::Handle<i::WasmModuleObject> obj =
      i::Handle<i::WasmModuleObject>::cast(Utils::OpenHandle(this));
  return Utils::Convert(obj->shared_native_module());
}

MaybeLocal<WasmModuleObject> WasmModuleObject::FromCompiledModule(
    Isolate* isolate, const CompiledWasmModule& compiled_module) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::WasmModuleObject> module_object =
      i_isolate->wasm_engine()->ImportNativeModule(
          i_isolate, Utils::Open(compiled_module));
  return Local<WasmModuleObject>::Cast(
      Utils::ToLocal(i::Handle<i::JSObject>::cast(module_object)));
}

WasmModuleObjectBuilderStreaming::WasmModuleObjectBuilderStreaming(
    Isolate* isolate) {
  USE(isolate_);
}

Local<Promise> WasmModuleObjectBuilderStreaming::GetPromise() { return {}; }

void WasmModuleObjectBuilderStreaming::OnBytesReceived(const uint8_t* bytes,
                                                       size_t size) {}

void WasmModuleObjectBuilderStreaming::Finish() {}

void WasmModuleObjectBuilderStreaming::Abort(MaybeLocal<Value> exception) {}

void* v8::ArrayBuffer::Allocator::Reallocate(void* data, size_t old_length,
                                             size_t new_length) {
  if (old_length == new_length) return data;
  uint8_t* new_data =
      reinterpret_cast<uint8_t*>(AllocateUninitialized(new_length));
  if (new_data == nullptr) return nullptr;
  size_t bytes_to_copy = std::min(old_length, new_length);
  memcpy(new_data, data, bytes_to_copy);
  if (new_length > bytes_to_copy) {
    memset(new_data + bytes_to_copy, 0, new_length - bytes_to_copy);
  }
  Free(data, old_length);
  return new_data;
}

// static
v8::ArrayBuffer::Allocator* v8::ArrayBuffer::Allocator::NewDefaultAllocator() {
  return new ArrayBufferAllocator();
}

bool v8::ArrayBuffer::IsExternal() const {
  return Utils::OpenHandle(this)->is_external();
}

bool v8::ArrayBuffer::IsDetachable() const {
  return Utils::OpenHandle(this)->is_detachable();
}

namespace {
// The backing store deleter just deletes the indirection, which downrefs
// the shared pointer. It will get collected normally.
void BackingStoreDeleter(void* buffer, size_t length, void* info) {
  std::shared_ptr<i::BackingStore>* bs_indirection =
      reinterpret_cast<std::shared_ptr<i::BackingStore>*>(info);
  if (bs_indirection) {
    i::BackingStore* backing_store = bs_indirection->get();
    TRACE_BS("API:delete bs=%p mem=%p (length=%zu)\n", backing_store,
             backing_store->buffer_start(), backing_store->byte_length());
    USE(backing_store);
  }
  delete bs_indirection;
}

void* MakeDeleterData(std::shared_ptr<i::BackingStore> backing_store) {
  if (!backing_store) return nullptr;
  TRACE_BS("API:extern bs=%p mem=%p (length=%zu)\n", backing_store.get(),
           backing_store->buffer_start(), backing_store->byte_length());
  return new std::shared_ptr<i::BackingStore>(backing_store);
}

std::shared_ptr<i::BackingStore> LookupOrCreateBackingStore(
    i::Isolate* i_isolate, void* data, size_t byte_length, i::SharedFlag shared,
    ArrayBufferCreationMode mode) {
  // "internalized" means that the storage was allocated by the
  // ArrayBufferAllocator and thus should be freed upon destruction.
  bool free_on_destruct = mode == ArrayBufferCreationMode::kInternalized;

  // Try to lookup a previously-registered backing store in the global
  // registry. If found, use that instead of wrapping an embedder allocation.
  std::shared_ptr<i::BackingStore> backing_store =
      i::GlobalBackingStoreRegistry::Lookup(data, byte_length);

  if (backing_store) {
    // Check invariants for a previously-found backing store.

    // 1. We cannot allow an embedder to first allocate a backing store that
    // should not be freed upon destruct, and then allocate an alias that should
    // destruct it. The other order is fine.
    bool changing_destruct_mode =
        free_on_destruct && !backing_store->free_on_destruct();
    Utils::ApiCheck(
        !changing_destruct_mode, "v8_[Shared]ArrayBuffer_New",
        "previous backing store found that should not be freed on destruct");

    // 2. We cannot allow embedders to use the same backing store for both
    // SharedArrayBuffers and regular ArrayBuffers.
    bool changing_shared_flag =
        (shared == i::SharedFlag::kShared) != backing_store->is_shared();
    Utils::ApiCheck(
        !changing_shared_flag, "v8_[Shared]ArrayBuffer_New",
        "previous backing store found that does not match shared flag");
  } else {
    // No previous backing store found.
    backing_store = i::BackingStore::WrapAllocation(
        i_isolate, data, byte_length, shared, free_on_destruct);

    // The embedder already has a direct pointer to the buffer start, so
    // globally register the backing store in case they come back with the
    // same buffer start and the backing store is marked as free_on_destruct.
    i::GlobalBackingStoreRegistry::Register(backing_store);
  }
  return backing_store;
}

std::shared_ptr<i::BackingStore> ToInternal(
    std::shared_ptr<i::BackingStoreBase> backing_store) {
  return std::static_pointer_cast<i::BackingStore>(backing_store);
}
}  // namespace

v8::ArrayBuffer::Contents::Contents(void* data, size_t byte_length,
                                    void* allocation_base,
                                    size_t allocation_length,
                                    Allocator::AllocationMode allocation_mode,
                                    DeleterCallback deleter, void* deleter_data)
    : data_(data),
      byte_length_(byte_length),
      allocation_base_(allocation_base),
      allocation_length_(allocation_length),
      allocation_mode_(allocation_mode),
      deleter_(deleter),
      deleter_data_(deleter_data) {
  DCHECK_LE(allocation_base_, data_);
  DCHECK_LE(byte_length_, allocation_length_);
}

v8::ArrayBuffer::Contents v8::ArrayBuffer::Externalize() {
  return GetContents(true);
}

void v8::ArrayBuffer::Externalize(
    const std::shared_ptr<BackingStore>& backing_store) {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  Utils::ApiCheck(!self->is_external(), "v8_ArrayBuffer_Externalize",
                  "ArrayBuffer already externalized");
  self->set_is_external(true);
  DCHECK_EQ(self->backing_store(), backing_store->Data());
}

v8::ArrayBuffer::Contents v8::ArrayBuffer::GetContents() {
  return GetContents(false);
}

v8::ArrayBuffer::Contents v8::ArrayBuffer::GetContents(bool externalize) {
  // TODO(titzer): reduce duplication between shared/unshared GetContents()
  using BufferType = v8::ArrayBuffer;

  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);

  std::shared_ptr<i::BackingStore> backing_store = self->GetBackingStore();

  void* deleter_data = nullptr;
  if (externalize) {
    Utils::ApiCheck(!self->is_external(), "v8_ArrayBuffer_Externalize",
                    "ArrayBuffer already externalized");
    self->set_is_external(true);
    // When externalizing, upref the shared pointer to the backing store
    // and store that as the deleter data. When the embedder calls the deleter
    // callback, we will delete the additional (on-heap) shared_ptr.
    deleter_data = MakeDeleterData(backing_store);
  }

  if (!backing_store) {
    // If the array buffer has zero length or was detached, return empty
    // contents.
    DCHECK_EQ(0, self->byte_length());
    BufferType::Contents contents(
        nullptr, 0, nullptr, 0,
        v8::ArrayBuffer::Allocator::AllocationMode::kNormal,
        BackingStoreDeleter, deleter_data);
    return contents;
  }

  // Backing stores that given to the embedder might be passed back through
  // the API using only the start of the buffer. We need to find such
  // backing stores using global registration until the API is changed.
  i::GlobalBackingStoreRegistry::Register(backing_store);

  auto allocation_mode =
      backing_store->is_wasm_memory()
          ? v8::ArrayBuffer::Allocator::AllocationMode::kReservation
          : v8::ArrayBuffer::Allocator::AllocationMode::kNormal;

  BufferType::Contents contents(backing_store->buffer_start(),  // --
                                backing_store->byte_length(),   // --
                                backing_store->buffer_start(),  // --
                                backing_store->byte_length(),   // --
                                allocation_mode,                // --
                                BackingStoreDeleter,            // --
                                deleter_data);
  return contents;
}

void v8::ArrayBuffer::Detach() {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  Utils::ApiCheck(obj->is_detachable(), "v8::ArrayBuffer::Detach",
                  "Only detachable ArrayBuffers can be detached");
  LOG_API(isolate, ArrayBuffer, Detach);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  obj->Detach();
}

size_t v8::ArrayBuffer::ByteLength() const {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return obj->byte_length();
}

Local<ArrayBuffer> v8::ArrayBuffer::New(Isolate* isolate, size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, ArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::MaybeHandle<i::JSArrayBuffer> result =
      i_isolate->factory()->NewJSArrayBufferAndBackingStore(
          byte_length, i::InitializedFlag::kZeroInitialized);

  i::Handle<i::JSArrayBuffer> array_buffer;
  if (!result.ToHandle(&array_buffer)) {
    // TODO(jbroman): It may be useful in the future to provide a MaybeLocal
    // version that throws an exception or otherwise does not crash.
    i::FatalProcessOutOfMemory(i_isolate, "v8::ArrayBuffer::New");
  }

  return Utils::ToLocal(array_buffer);
}

Local<ArrayBuffer> v8::ArrayBuffer::New(Isolate* isolate, void* data,
                                        size_t byte_length,
                                        ArrayBufferCreationMode mode) {
  // Embedders must guarantee that the external backing store is valid.
  CHECK_IMPLIES(byte_length != 0, data != nullptr);
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, ArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

  std::shared_ptr<i::BackingStore> backing_store = LookupOrCreateBackingStore(
      i_isolate, data, byte_length, i::SharedFlag::kNotShared, mode);

  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSArrayBuffer(std::move(backing_store));
  if (mode == ArrayBufferCreationMode::kExternalized) {
    obj->set_is_external(true);
  }
  return Utils::ToLocal(obj);
}

Local<ArrayBuffer> v8::ArrayBuffer::New(
    Isolate* isolate, std::shared_ptr<BackingStore> backing_store) {
  CHECK_IMPLIES(backing_store->ByteLength() != 0,
                backing_store->Data() != nullptr);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, ArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  std::shared_ptr<i::BackingStore> i_backing_store(
      ToInternal(std::move(backing_store)));
  Utils::ApiCheck(
      !i_backing_store->is_shared(), "v8_ArrayBuffer_New",
      "Cannot construct ArrayBuffer with a BackingStore of SharedArrayBuffer");
  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSArrayBuffer(std::move(i_backing_store));
  return Utils::ToLocal(obj);
}

std::unique_ptr<v8::BackingStore> v8::ArrayBuffer::NewBackingStore(
    Isolate* isolate, size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, ArrayBuffer, NewBackingStore);
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::Allocate(i_isolate, byte_length,
                                i::SharedFlag::kNotShared,
                                i::InitializedFlag::kZeroInitialized);
  if (!backing_store) {
    i::FatalProcessOutOfMemory(i_isolate, "v8::ArrayBuffer::NewBackingStore");
  }
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}

std::unique_ptr<v8::BackingStore> v8::ArrayBuffer::NewBackingStore(
    void* data, size_t byte_length, BackingStoreDeleterCallback deleter,
    void* deleter_data) {
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::WrapAllocation(data, byte_length, deleter, deleter_data,
                                      i::SharedFlag::kNotShared);
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}

Local<ArrayBuffer> v8::ArrayBufferView::Buffer() {
  i::Handle<i::JSArrayBufferView> obj = Utils::OpenHandle(this);
  i::Handle<i::JSArrayBuffer> buffer;
  if (obj->IsJSDataView()) {
    i::Handle<i::JSDataView> data_view(i::JSDataView::cast(*obj),
                                       obj->GetIsolate());
    DCHECK(data_view->buffer().IsJSArrayBuffer());
    buffer = i::handle(i::JSArrayBuffer::cast(data_view->buffer()),
                       data_view->GetIsolate());
  } else {
    DCHECK(obj->IsJSTypedArray());
    buffer = i::JSTypedArray::cast(*obj).GetBuffer();
  }
  return Utils::ToLocal(buffer);
}

size_t v8::ArrayBufferView::CopyContents(void* dest, size_t byte_length) {
  i::Handle<i::JSArrayBufferView> self = Utils::OpenHandle(this);
  size_t byte_offset = self->byte_offset();
  size_t bytes_to_copy = i::Min(byte_length, self->byte_length());
  if (bytes_to_copy) {
    i::DisallowHeapAllocation no_gc;
    i::Isolate* isolate = self->GetIsolate();
    i::Handle<i::JSArrayBuffer> buffer(i::JSArrayBuffer::cast(self->buffer()),
                                       isolate);
    const char* source = reinterpret_cast<char*>(buffer->backing_store());
    if (source == nullptr) {
      DCHECK(self->IsJSTypedArray());
      i::Handle<i::JSTypedArray> typed_array(i::JSTypedArray::cast(*self),
                                             isolate);
      source = reinterpret_cast<char*>(typed_array->DataPtr());
    }
    memcpy(dest, source + byte_offset, bytes_to_copy);
  }
  return bytes_to_copy;
}

bool v8::ArrayBufferView::HasBuffer() const {
  i::Handle<i::JSArrayBufferView> self = Utils::OpenHandle(this);
  if (!self->IsJSTypedArray()) return true;
  auto typed_array = i::Handle<i::JSTypedArray>::cast(self);
  return !typed_array->is_on_heap();
}

size_t v8::ArrayBufferView::ByteOffset() {
  i::Handle<i::JSArrayBufferView> obj = Utils::OpenHandle(this);
  return obj->WasDetached() ? 0 : obj->byte_offset();
}

size_t v8::ArrayBufferView::ByteLength() {
  i::Handle<i::JSArrayBufferView> obj = Utils::OpenHandle(this);
  return obj->WasDetached() ? 0 : obj->byte_length();
}

size_t v8::TypedArray::Length() {
  i::Handle<i::JSTypedArray> obj = Utils::OpenHandle(this);
  return obj->WasDetached() ? 0 : obj->length();
}

static_assert(
    v8::TypedArray::kMaxLength == i::JSTypedArray::kMaxLength,
    "v8::TypedArray::kMaxLength must match i::JSTypedArray::kMaxLength");

#define TYPED_ARRAY_NEW(Type, type, TYPE, ctype)                           \
  Local<Type##Array> Type##Array::New(Local<ArrayBuffer> array_buffer,     \
                                      size_t byte_offset, size_t length) { \
    i::Isolate* isolate = Utils::OpenHandle(*array_buffer)->GetIsolate();  \
    LOG_API(isolate, Type##Array, New);                                    \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);                              \
    if (!Utils::ApiCheck(length <= kMaxLength,                             \
                         "v8::" #Type                                      \
                         "Array::New(Local<ArrayBuffer>, size_t, size_t)", \
                         "length exceeds max allowed value")) {            \
      return Local<Type##Array>();                                         \
    }                                                                      \
    i::Handle<i::JSArrayBuffer> buffer = Utils::OpenHandle(*array_buffer); \
    i::Handle<i::JSTypedArray> obj = isolate->factory()->NewJSTypedArray(  \
        i::kExternal##Type##Array, buffer, byte_offset, length);           \
    return Utils::ToLocal##Type##Array(obj);                               \
  }                                                                        \
  Local<Type##Array> Type##Array::New(                                     \
      Local<SharedArrayBuffer> shared_array_buffer, size_t byte_offset,    \
      size_t length) {                                                     \
    CHECK(i::FLAG_harmony_sharedarraybuffer);                              \
    i::Isolate* isolate =                                                  \
        Utils::OpenHandle(*shared_array_buffer)->GetIsolate();             \
    LOG_API(isolate, Type##Array, New);                                    \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);                              \
    if (!Utils::ApiCheck(                                                  \
            length <= kMaxLength,                                          \
            "v8::" #Type                                                   \
            "Array::New(Local<SharedArrayBuffer>, size_t, size_t)",        \
            "length exceeds max allowed value")) {                         \
      return Local<Type##Array>();                                         \
    }                                                                      \
    i::Handle<i::JSArrayBuffer> buffer =                                   \
        Utils::OpenHandle(*shared_array_buffer);                           \
    i::Handle<i::JSTypedArray> obj = isolate->factory()->NewJSTypedArray(  \
        i::kExternal##Type##Array, buffer, byte_offset, length);           \
    return Utils::ToLocal##Type##Array(obj);                               \
  }

TYPED_ARRAYS(TYPED_ARRAY_NEW)
#undef TYPED_ARRAY_NEW

Local<DataView> DataView::New(Local<ArrayBuffer> array_buffer,
                              size_t byte_offset, size_t byte_length) {
  i::Handle<i::JSArrayBuffer> buffer = Utils::OpenHandle(*array_buffer);
  i::Isolate* isolate = buffer->GetIsolate();
  LOG_API(isolate, DataView, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::JSDataView> obj =
      isolate->factory()->NewJSDataView(buffer, byte_offset, byte_length);
  return Utils::ToLocal(obj);
}

Local<DataView> DataView::New(Local<SharedArrayBuffer> shared_array_buffer,
                              size_t byte_offset, size_t byte_length) {
  CHECK(i::FLAG_harmony_sharedarraybuffer);
  i::Handle<i::JSArrayBuffer> buffer = Utils::OpenHandle(*shared_array_buffer);
  i::Isolate* isolate = buffer->GetIsolate();
  LOG_API(isolate, DataView, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::JSDataView> obj =
      isolate->factory()->NewJSDataView(buffer, byte_offset, byte_length);
  return Utils::ToLocal(obj);
}

namespace {
i::Handle<i::JSArrayBuffer> SetupSharedArrayBuffer(
    Isolate* isolate, void* data, size_t byte_length,
    ArrayBufferCreationMode mode) {
  CHECK(i::FLAG_harmony_sharedarraybuffer);
  // Embedders must guarantee that the external backing store is valid.
  CHECK(byte_length == 0 || data != nullptr);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, SharedArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

  std::shared_ptr<i::BackingStore> backing_store = LookupOrCreateBackingStore(
      i_isolate, data, byte_length, i::SharedFlag::kShared, mode);

  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSSharedArrayBuffer(std::move(backing_store));

  if (mode == ArrayBufferCreationMode::kExternalized) {
    obj->set_is_external(true);
  }
  return obj;
}

}  // namespace

bool v8::SharedArrayBuffer::IsExternal() const {
  return Utils::OpenHandle(this)->is_external();
}

v8::SharedArrayBuffer::Contents::Contents(
    void* data, size_t byte_length, void* allocation_base,
    size_t allocation_length, Allocator::AllocationMode allocation_mode,
    DeleterCallback deleter, void* deleter_data)
    : data_(data),
      byte_length_(byte_length),
      allocation_base_(allocation_base),
      allocation_length_(allocation_length),
      allocation_mode_(allocation_mode),
      deleter_(deleter),
      deleter_data_(deleter_data) {
  DCHECK_LE(allocation_base_, data_);
  DCHECK_LE(byte_length_, allocation_length_);
}

v8::SharedArrayBuffer::Contents v8::SharedArrayBuffer::Externalize() {
  return GetContents(true);
}

void v8::SharedArrayBuffer::Externalize(
    const std::shared_ptr<BackingStore>& backing_store) {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  Utils::ApiCheck(!self->is_external(), "v8_SharedArrayBuffer_Externalize",
                  "SharedArrayBuffer already externalized");
  self->set_is_external(true);

  DCHECK_EQ(self->backing_store(), backing_store->Data());
}

v8::SharedArrayBuffer::Contents v8::SharedArrayBuffer::GetContents() {
  return GetContents(false);
}

v8::SharedArrayBuffer::Contents v8::SharedArrayBuffer::GetContents(
    bool externalize) {
  // TODO(titzer): reduce duplication between shared/unshared GetContents()
  using BufferType = v8::SharedArrayBuffer;

  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);

  std::shared_ptr<i::BackingStore> backing_store = self->GetBackingStore();

  void* deleter_data = nullptr;
  if (externalize) {
    Utils::ApiCheck(!self->is_external(), "v8_SharedArrayBuffer_Externalize",
                    "SharedArrayBuffer already externalized");
    self->set_is_external(true);
    // When externalizing, upref the shared pointer to the backing store
    // and store that as the deleter data. When the embedder calls the deleter
    // callback, we will delete the additional (on-heap) shared_ptr.
    deleter_data = MakeDeleterData(backing_store);
  }

  if (!backing_store) {
    // If the array buffer has zero length or was detached, return empty
    // contents.
    DCHECK_EQ(0, self->byte_length());
    BufferType::Contents contents(
        nullptr, 0, nullptr, 0,
        v8::ArrayBuffer::Allocator::AllocationMode::kNormal,
        BackingStoreDeleter, deleter_data);
    return contents;
  }

  // Backing stores that given to the embedder might be passed back through
  // the API using only the start of the buffer. We need to find such
  // backing stores using global registration until the API is changed.
  i::GlobalBackingStoreRegistry::Register(backing_store);

  auto allocation_mode =
      backing_store->is_wasm_memory()
          ? v8::ArrayBuffer::Allocator::AllocationMode::kReservation
          : v8::ArrayBuffer::Allocator::AllocationMode::kNormal;

  BufferType::Contents contents(backing_store->buffer_start(),  // --
                                backing_store->byte_length(),   // --
                                backing_store->buffer_start(),  // --
                                backing_store->byte_length(),   // --
                                allocation_mode,                // --
                                BackingStoreDeleter,            // --
                                deleter_data);
  return contents;
}

size_t v8::SharedArrayBuffer::ByteLength() const {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return obj->byte_length();
}

Local<SharedArrayBuffer> v8::SharedArrayBuffer::New(Isolate* isolate,
                                                    size_t byte_length) {
  CHECK(i::FLAG_harmony_sharedarraybuffer);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, SharedArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

  std::unique_ptr<i::BackingStore> backing_store =
      i::BackingStore::Allocate(i_isolate, byte_length, i::SharedFlag::kShared,
                                i::InitializedFlag::kZeroInitialized);

  if (!backing_store) {
    // TODO(jbroman): It may be useful in the future to provide a MaybeLocal
    // version that throws an exception or otherwise does not crash.
    i::FatalProcessOutOfMemory(i_isolate, "v8::SharedArrayBuffer::New");
  }

  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSSharedArrayBuffer(std::move(backing_store));
  return Utils::ToLocalShared(obj);
}

Local<SharedArrayBuffer> v8::SharedArrayBuffer::New(
    Isolate* isolate, void* data, size_t byte_length,
    ArrayBufferCreationMode mode) {
  i::Handle<i::JSArrayBuffer> buffer =
      SetupSharedArrayBuffer(isolate, data, byte_length, mode);
  return Utils::ToLocalShared(buffer);
}

Local<SharedArrayBuffer> v8::SharedArrayBuffer::New(
    Isolate* isolate, std::shared_ptr<BackingStore> backing_store) {
  CHECK(i::FLAG_harmony_sharedarraybuffer);
  CHECK_IMPLIES(backing_store->ByteLength() != 0,
                backing_store->Data() != nullptr);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, SharedArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  std::shared_ptr<i::BackingStore> i_backing_store(ToInternal(backing_store));
  Utils::ApiCheck(
      i_backing_store->is_shared(), "v8_SharedArrayBuffer_New",
      "Cannot construct SharedArrayBuffer with BackingStore of ArrayBuffer");
  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSSharedArrayBuffer(std::move(i_backing_store));
  return Utils::ToLocalShared(obj);
}

Local<SharedArrayBuffer> v8::SharedArrayBuffer::New(
    Isolate* isolate, const SharedArrayBuffer::Contents& contents,
    ArrayBufferCreationMode mode) {
  i::Handle<i::JSArrayBuffer> buffer = SetupSharedArrayBuffer(
      isolate, contents.Data(), contents.ByteLength(), mode);
  return Utils::ToLocalShared(buffer);
}

std::unique_ptr<v8::BackingStore> v8::SharedArrayBuffer::NewBackingStore(
    Isolate* isolate, size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, SharedArrayBuffer, NewBackingStore);
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::Allocate(i_isolate, byte_length, i::SharedFlag::kShared,
                                i::InitializedFlag::kZeroInitialized);
  if (!backing_store) {
    i::FatalProcessOutOfMemory(i_isolate,
                               "v8::SharedArrayBuffer::NewBackingStore");
  }
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}

std::unique_ptr<v8::BackingStore> v8::SharedArrayBuffer::NewBackingStore(
    void* data, size_t byte_length, BackingStoreDeleterCallback deleter,
    void* deleter_data) {
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::WrapAllocation(data, byte_length, deleter, deleter_data,
                                      i::SharedFlag::kShared);
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}

Local<Symbol> v8::Symbol::New(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Symbol, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Symbol> result = i_isolate->factory()->NewSymbol();
  if (!name.IsEmpty()) result->set_description(*Utils::OpenHandle(*name));
  return Utils::ToLocal(result);
}

Local<Symbol> v8::Symbol::For(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::String> i_name = Utils::OpenHandle(*name);
  return Utils::ToLocal(
      i_isolate->SymbolFor(i::RootIndex::kPublicSymbolTable, i_name, false));
}

Local<Symbol> v8::Symbol::ForApi(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::String> i_name = Utils::OpenHandle(*name);
  return Utils::ToLocal(
      i_isolate->SymbolFor(i::RootIndex::kApiSymbolTable, i_name, false));
}

#define WELL_KNOWN_SYMBOLS(V)                 \
  V(AsyncIterator, async_iterator)            \
  V(HasInstance, has_instance)                \
  V(IsConcatSpreadable, is_concat_spreadable) \
  V(Iterator, iterator)                       \
  V(Match, match)                             \
  V(Replace, replace)                         \
  V(Search, search)                           \
  V(Split, split)                             \
  V(ToPrimitive, to_primitive)                \
  V(ToStringTag, to_string_tag)               \
  V(Unscopables, unscopables)

#define SYMBOL_GETTER(Name, name)                                   \
  Local<Symbol> v8::Symbol::Get##Name(Isolate* isolate) {           \
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate); \
    return Utils::ToLocal(i_isolate->factory()->name##_symbol());   \
  }

WELL_KNOWN_SYMBOLS(SYMBOL_GETTER)

#undef SYMBOL_GETTER
#undef WELL_KNOWN_SYMBOLS

Local<Private> v8::Private::New(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Private, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Symbol> symbol = i_isolate->factory()->NewPrivateSymbol();
  if (!name.IsEmpty()) symbol->set_description(*Utils::OpenHandle(*name));
  Local<Symbol> result = Utils::ToLocal(symbol);
  return v8::Local<Private>(reinterpret_cast<Private*>(*result));
}

Local<Private> v8::Private::ForApi(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::String> i_name = Utils::OpenHandle(*name);
  Local<Symbol> result = Utils::ToLocal(
      i_isolate->SymbolFor(i::RootIndex::kApiPrivateSymbolTable, i_name, true));
  return v8::Local<Private>(reinterpret_cast<Private*>(*result));
}

Local<Number> v8::Number::New(Isolate* isolate, double value) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (std::isnan(value)) {
    // Introduce only canonical NaN value into the VM, to avoid signaling NaNs.
    value = std::numeric_limits<double>::quiet_NaN();
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(internal_isolate);
  i::Handle<i::Object> result = internal_isolate->factory()->NewNumber(value);
  return Utils::NumberToLocal(result);
}

Local<Integer> v8::Integer::New(Isolate* isolate, int32_t value) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (i::Smi::IsValid(value)) {
    return Utils::IntegerToLocal(
        i::Handle<i::Object>(i::Smi::FromInt(value), internal_isolate));
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(internal_isolate);
  i::Handle<i::Object> result = internal_isolate->factory()->NewNumber(value);
  return Utils::IntegerToLocal(result);
}

Local<Integer> v8::Integer::NewFromUnsigned(Isolate* isolate, uint32_t value) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  bool fits_into_int32_t = (value & (1 << 31)) == 0;
  if (fits_into_int32_t) {
    return Integer::New(isolate, static_cast<int32_t>(value));
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(internal_isolate);
  i::Handle<i::Object> result = internal_isolate->factory()->NewNumber(value);
  return Utils::IntegerToLocal(result);
}

Local<BigInt> v8::BigInt::New(Isolate* isolate, int64_t value) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(internal_isolate);
  i::Handle<i::BigInt> result = i::BigInt::FromInt64(internal_isolate, value);
  return Utils::ToLocal(result);
}

Local<BigInt> v8::BigInt::NewFromUnsigned(Isolate* isolate, uint64_t value) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(internal_isolate);
  i::Handle<i::BigInt> result = i::BigInt::FromUint64(internal_isolate, value);
  return Utils::ToLocal(result);
}

MaybeLocal<BigInt> v8::BigInt::NewFromWords(Local<Context> context,
                                            int sign_bit, int word_count,
                                            const uint64_t* words) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, BigInt, NewFromWords,
                     MaybeLocal<BigInt>(), InternalEscapableScope);
  i::MaybeHandle<i::BigInt> result =
      i::BigInt::FromWords64(isolate, sign_bit, word_count, words);
  has_pending_exception = result.is_null();
  RETURN_ON_FAILED_EXECUTION(BigInt);
  RETURN_ESCAPED(Utils::ToLocal(result.ToHandleChecked()));
}

uint64_t v8::BigInt::Uint64Value(bool* lossless) const {
  i::Handle<i::BigInt> handle = Utils::OpenHandle(this);
  return handle->AsUint64(lossless);
}

int64_t v8::BigInt::Int64Value(bool* lossless) const {
  i::Handle<i::BigInt> handle = Utils::OpenHandle(this);
  return handle->AsInt64(lossless);
}

int BigInt::WordCount() const {
  i::Handle<i::BigInt> handle = Utils::OpenHandle(this);
  return handle->Words64Count();
}

void BigInt::ToWordsArray(int* sign_bit, int* word_count,
                          uint64_t* words) const {
  i::Handle<i::BigInt> handle = Utils::OpenHandle(this);
  return handle->ToWordsArray64(sign_bit, word_count, words);
}

void Isolate::ReportExternalAllocationLimitReached() {
  i::Heap* heap = reinterpret_cast<i::Isolate*>(this)->heap();
  if (heap->gc_state() != i::Heap::NOT_IN_GC) return;
  heap->ReportExternalMemoryPressure();
}

void Isolate::CheckMemoryPressure() {
  i::Heap* heap = reinterpret_cast<i::Isolate*>(this)->heap();
  if (heap->gc_state() != i::Heap::NOT_IN_GC) return;
  heap->CheckMemoryPressure();
}

HeapProfiler* Isolate::GetHeapProfiler() {
  i::HeapProfiler* heap_profiler =
      reinterpret_cast<i::Isolate*>(this)->heap_profiler();
  return reinterpret_cast<HeapProfiler*>(heap_profiler);
}

void Isolate::SetIdle(bool is_idle) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetIdle(is_idle);
}

ArrayBuffer::Allocator* Isolate::GetArrayBufferAllocator() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->array_buffer_allocator();
}

bool Isolate::InContext() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return !isolate->context().is_null();
}

void Isolate::ClearKeptObjects() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->ClearKeptObjects();
}

v8::Local<v8::Context> Isolate::GetCurrentContext() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Context context = isolate->context();
  if (context.is_null()) return Local<Context>();
  i::Context native_context = context.native_context();
  if (native_context.is_null()) return Local<Context>();
  return Utils::ToLocal(i::Handle<i::Context>(native_context, isolate));
}

v8::Local<v8::Context> Isolate::GetEnteredContext() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::Object> last =
      isolate->handle_scope_implementer()->LastEnteredContext();
  if (last.is_null()) return Local<Context>();
  return Utils::ToLocal(i::Handle<i::Context>::cast(last));
}

v8::Local<v8::Context> Isolate::GetEnteredOrMicrotaskContext() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::Object> last =
      isolate->handle_scope_implementer()->LastEnteredOrMicrotaskContext();
  if (last.is_null()) return Local<Context>();
  DCHECK(last->IsNativeContext());
  return Utils::ToLocal(i::Handle<i::Context>::cast(last));
}

v8::Local<v8::Context> Isolate::GetIncumbentContext() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::Context> context = isolate->GetIncumbentContext();
  return Utils::ToLocal(context);
}

v8::Local<Value> Isolate::ThrowException(v8::Local<v8::Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  ENTER_V8_DO_NOT_USE(isolate);
  // If we're passed an empty handle, we throw an undefined exception
  // to deal more gracefully with out of memory situations.
  if (value.IsEmpty()) {
    isolate->ScheduleThrow(i::ReadOnlyRoots(isolate).undefined_value());
  } else {
    isolate->ScheduleThrow(*Utils::OpenHandle(*value));
  }
  return v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
}

void Isolate::AddGCPrologueCallback(GCCallbackWithData callback, void* data,
                                    GCType gc_type) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->AddGCPrologueCallback(callback, gc_type, data);
}

void Isolate::RemoveGCPrologueCallback(GCCallbackWithData callback,
                                       void* data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->RemoveGCPrologueCallback(callback, data);
}

void Isolate::AddGCEpilogueCallback(GCCallbackWithData callback, void* data,
                                    GCType gc_type) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->AddGCEpilogueCallback(callback, gc_type, data);
}

void Isolate::RemoveGCEpilogueCallback(GCCallbackWithData callback,
                                       void* data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->RemoveGCEpilogueCallback(callback, data);
}

static void CallGCCallbackWithoutData(Isolate* isolate, GCType type,
                                      GCCallbackFlags flags, void* data) {
  reinterpret_cast<Isolate::GCCallback>(data)(isolate, type, flags);
}

void Isolate::AddGCPrologueCallback(GCCallback callback, GCType gc_type) {
  void* data = reinterpret_cast<void*>(callback);
  AddGCPrologueCallback(CallGCCallbackWithoutData, data, gc_type);
}

void Isolate::RemoveGCPrologueCallback(GCCallback callback) {
  void* data = reinterpret_cast<void*>(callback);
  RemoveGCPrologueCallback(CallGCCallbackWithoutData, data);
}

void Isolate::AddGCEpilogueCallback(GCCallback callback, GCType gc_type) {
  void* data = reinterpret_cast<void*>(callback);
  AddGCEpilogueCallback(CallGCCallbackWithoutData, data, gc_type);
}

void Isolate::RemoveGCEpilogueCallback(GCCallback callback) {
  void* data = reinterpret_cast<void*>(callback);
  RemoveGCEpilogueCallback(CallGCCallbackWithoutData, data);
}

void Isolate::SetEmbedderHeapTracer(EmbedderHeapTracer* tracer) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->SetEmbedderHeapTracer(tracer);
}

EmbedderHeapTracer* Isolate::GetEmbedderHeapTracer() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->heap()->GetEmbedderHeapTracer();
}

void Isolate::SetGetExternallyAllocatedMemoryInBytesCallback(
    GetExternallyAllocatedMemoryInBytesCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->SetGetExternallyAllocatedMemoryInBytesCallback(callback);
}

void Isolate::TerminateExecution() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->stack_guard()->RequestTerminateExecution();
}

bool Isolate::IsExecutionTerminating() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return IsExecutionTerminatingCheck(isolate);
}

void Isolate::CancelTerminateExecution() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->stack_guard()->ClearTerminateExecution();
  isolate->CancelTerminateExecution();
}

void Isolate::RequestInterrupt(InterruptCallback callback, void* data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->RequestInterrupt(callback, data);
}

void Isolate::RequestGarbageCollectionForTesting(GarbageCollectionType type) {
  CHECK(i::FLAG_expose_gc);
  if (type == kMinorGarbageCollection) {
    reinterpret_cast<i::Isolate*>(this)->heap()->CollectGarbage(
        i::NEW_SPACE, i::GarbageCollectionReason::kTesting,
        kGCCallbackFlagForced);
  } else {
    DCHECK_EQ(kFullGarbageCollection, type);
    reinterpret_cast<i::Isolate*>(this)->heap()->PreciseCollectAllGarbage(
        i::Heap::kNoGCFlags, i::GarbageCollectionReason::kTesting,
        kGCCallbackFlagForced);
  }
}

Isolate* Isolate::GetCurrent() {
  i::Isolate* isolate = i::Isolate::Current();
  return reinterpret_cast<Isolate*>(isolate);
}

// static
Isolate* Isolate::Allocate() {
  return reinterpret_cast<Isolate*>(i::Isolate::New());
}

// static
// This is separate so that tests can provide a different |isolate|.
void Isolate::Initialize(Isolate* isolate,
                         const v8::Isolate::CreateParams& params) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (auto allocator = params.array_buffer_allocator_shared) {
    CHECK(params.array_buffer_allocator == nullptr ||
          params.array_buffer_allocator == allocator.get());
    i_isolate->set_array_buffer_allocator(allocator.get());
    i_isolate->set_array_buffer_allocator_shared(std::move(allocator));
  } else {
    CHECK_NOT_NULL(params.array_buffer_allocator);
    i_isolate->set_array_buffer_allocator(params.array_buffer_allocator);
  }
  if (params.snapshot_blob != nullptr) {
    i_isolate->set_snapshot_blob(params.snapshot_blob);
  } else {
    i_isolate->set_snapshot_blob(i::Snapshot::DefaultSnapshotBlob());
  }
  auto code_event_handler = params.code_event_handler;
#ifdef ENABLE_GDB_JIT_INTERFACE
  if (code_event_handler == nullptr && i::FLAG_gdbjit) {
    code_event_handler = i::GDBJITInterface::EventHandler;
  }
#endif  // ENABLE_GDB_JIT_INTERFACE
  if (code_event_handler) {
    i_isolate->InitializeLoggingAndCounters();
    i_isolate->logger()->SetCodeEventHandler(kJitCodeEventDefault,
                                             code_event_handler);
  }
  if (params.counter_lookup_callback) {
    isolate->SetCounterFunction(params.counter_lookup_callback);
  }

  if (params.create_histogram_callback) {
    isolate->SetCreateHistogramFunction(params.create_histogram_callback);
  }

  if (params.add_histogram_sample_callback) {
    isolate->SetAddHistogramSampleFunction(
        params.add_histogram_sample_callback);
  }

  i_isolate->set_api_external_references(params.external_references);
  i_isolate->set_allow_atomics_wait(params.allow_atomics_wait);

  i_isolate->heap()->ConfigureHeap(params.constraints);
  if (params.constraints.stack_limit() != nullptr) {
    uintptr_t limit =
        reinterpret_cast<uintptr_t>(params.constraints.stack_limit());
    i_isolate->stack_guard()->SetStackLimit(limit);
  }
  // TODO(jochen): Once we got rid of Isolate::Current(), we can remove this.
  Isolate::Scope isolate_scope(isolate);
  if (!i::Snapshot::Initialize(i_isolate)) {
    // If snapshot data was provided and we failed to deserialize it must
    // have been corrupted.
    if (i_isolate->snapshot_blob() != nullptr) {
      FATAL(
          "Failed to deserialize the V8 snapshot blob. This can mean that the "
          "snapshot blob file is corrupted or missing.");
    }
    base::ElapsedTimer timer;
    if (i::FLAG_profile_deserialization) timer.Start();
    i_isolate->InitWithoutSnapshot();
    if (i::FLAG_profile_deserialization) {
      double ms = timer.Elapsed().InMillisecondsF();
      i::PrintF("[Initializing isolate from scratch took %0.3f ms]\n", ms);
    }
  }
  i_isolate->set_only_terminate_in_safe_scope(
      params.only_terminate_in_safe_scope);

  if (!i::V8::GetCurrentPlatform()
           ->GetForegroundTaskRunner(isolate)
           ->NonNestableTasksEnabled()) {
    FATAL(
        "The current platform's foreground task runner does not have "
        "non-nestable tasks enabled. The embedder must provide one.");
  }
}

Isolate* Isolate::New(const Isolate::CreateParams& params) {
  Isolate* isolate = Allocate();
  Initialize(isolate, params);
  return isolate;
}

void Isolate::Dispose() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  if (!Utils::ApiCheck(!isolate->IsInUse(), "v8::Isolate::Dispose()",
                       "Disposing the isolate that is entered by a thread.")) {
    return;
  }
  i::Isolate::Delete(isolate);
}

void Isolate::DumpAndResetStats() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->DumpAndResetStats();
}

void Isolate::DiscardThreadSpecificMetadata() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->DiscardPerThreadDataForThisThread();
}

void Isolate::Enter() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->Enter();
}

void Isolate::Exit() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->Exit();
}

void Isolate::SetAbortOnUncaughtExceptionCallback(
    AbortOnUncaughtExceptionCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetAbortOnUncaughtExceptionCallback(callback);
}

void Isolate::SetHostCleanupFinalizationGroupCallback(
    HostCleanupFinalizationGroupCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetHostCleanupFinalizationGroupCallback(callback);
}

Maybe<bool> FinalizationGroup::Cleanup(
    Local<FinalizationGroup> finalization_group) {
  i::Handle<i::JSFinalizationGroup> fg = Utils::OpenHandle(*finalization_group);
  i::Isolate* isolate = fg->native_context().GetIsolate();
  i::Handle<i::Context> i_context(fg->native_context(), isolate);
  Local<Context> context = Utils::ToLocal(i_context);
  ENTER_V8(isolate, context, FinalizationGroup, Cleanup, Nothing<bool>(),
           i::HandleScope);
  i::Handle<i::Object> callback(fg->cleanup(), isolate);
  fg->set_scheduled_for_cleanup(false);
  has_pending_exception =
      i::JSFinalizationGroup::Cleanup(isolate, fg, callback).IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

void Isolate::SetHostImportModuleDynamicallyCallback(
    HostImportModuleDynamicallyCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetHostImportModuleDynamicallyCallback(callback);
}

void Isolate::SetHostInitializeImportMetaObjectCallback(
    HostInitializeImportMetaObjectCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetHostInitializeImportMetaObjectCallback(callback);
}

void Isolate::SetPrepareStackTraceCallback(PrepareStackTraceCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetPrepareStackTraceCallback(callback);
}

Isolate::DisallowJavascriptExecutionScope::DisallowJavascriptExecutionScope(
    Isolate* isolate,
    Isolate::DisallowJavascriptExecutionScope::OnFailure on_failure)
    : on_failure_(on_failure) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  switch (on_failure_) {
    case CRASH_ON_FAILURE:
      internal_ = reinterpret_cast<void*>(
          new i::DisallowJavascriptExecution(i_isolate));
      break;
    case THROW_ON_FAILURE:
      DCHECK_EQ(THROW_ON_FAILURE, on_failure);
      internal_ =
          reinterpret_cast<void*>(new i::ThrowOnJavascriptExecution(i_isolate));
      break;
    case DUMP_ON_FAILURE:
      internal_ =
          reinterpret_cast<void*>(new i::DumpOnJavascriptExecution(i_isolate));
      break;
    default:
      UNREACHABLE();
  }
}

Isolate::DisallowJavascriptExecutionScope::~DisallowJavascriptExecutionScope() {
  switch (on_failure_) {
    case CRASH_ON_FAILURE:
      delete reinterpret_cast<i::DisallowJavascriptExecution*>(internal_);
      break;
    case THROW_ON_FAILURE:
      delete reinterpret_cast<i::ThrowOnJavascriptExecution*>(internal_);
      break;
    case DUMP_ON_FAILURE:
      delete reinterpret_cast<i::DumpOnJavascriptExecution*>(internal_);
      break;
    default:
      UNREACHABLE();
  }
}

Isolate::AllowJavascriptExecutionScope::AllowJavascriptExecutionScope(
    Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal_assert_ =
      reinterpret_cast<void*>(new i::AllowJavascriptExecution(i_isolate));
  internal_throws_ =
      reinterpret_cast<void*>(new i::NoThrowOnJavascriptExecution(i_isolate));
  internal_dump_ =
      reinterpret_cast<void*>(new i::NoDumpOnJavascriptExecution(i_isolate));
}

Isolate::AllowJavascriptExecutionScope::~AllowJavascriptExecutionScope() {
  delete reinterpret_cast<i::AllowJavascriptExecution*>(internal_assert_);
  delete reinterpret_cast<i::NoThrowOnJavascriptExecution*>(internal_throws_);
  delete reinterpret_cast<i::NoDumpOnJavascriptExecution*>(internal_dump_);
}

Isolate::SuppressMicrotaskExecutionScope::SuppressMicrotaskExecutionScope(
    Isolate* isolate, MicrotaskQueue* microtask_queue)
    : isolate_(reinterpret_cast<i::Isolate*>(isolate)),
      microtask_queue_(microtask_queue
                           ? static_cast<i::MicrotaskQueue*>(microtask_queue)
                           : isolate_->default_microtask_queue()) {
  isolate_->thread_local_top()->IncrementCallDepth(this);
  microtask_queue_->IncrementMicrotasksSuppressions();
}

Isolate::SuppressMicrotaskExecutionScope::~SuppressMicrotaskExecutionScope() {
  microtask_queue_->DecrementMicrotasksSuppressions();
  isolate_->thread_local_top()->DecrementCallDepth(this);
}

Isolate::SafeForTerminationScope::SafeForTerminationScope(v8::Isolate* isolate)
    : isolate_(reinterpret_cast<i::Isolate*>(isolate)),
      prev_value_(isolate_->next_v8_call_is_safe_for_termination()) {
  isolate_->set_next_v8_call_is_safe_for_termination(true);
}

Isolate::SafeForTerminationScope::~SafeForTerminationScope() {
  isolate_->set_next_v8_call_is_safe_for_termination(prev_value_);
}

i::Address* Isolate::GetDataFromSnapshotOnce(size_t index) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::FixedArray list = i_isolate->heap()->serialized_objects();
  return GetSerializedDataFromFixedArray(i_isolate, list, index);
}

void Isolate::GetHeapStatistics(HeapStatistics* heap_statistics) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = isolate->heap();

  heap_statistics->total_heap_size_ = heap->CommittedMemory();
  heap_statistics->total_physical_size_ = heap->CommittedPhysicalMemory();
  heap_statistics->total_available_size_ = heap->Available();
  heap_statistics->used_heap_size_ = heap->SizeOfObjects();

#ifndef V8_SHARED_RO_HEAP
  i::ReadOnlySpace* ro_space = heap->read_only_space();
  heap_statistics->total_heap_size_ += ro_space->CommittedMemory();
  heap_statistics->total_physical_size_ += ro_space->CommittedPhysicalMemory();
  heap_statistics->total_available_size_ += ro_space->Available();
  heap_statistics->used_heap_size_ += ro_space->SizeOfObjects();
#endif  // V8_SHARED_RO_HEAP

  heap_statistics->total_heap_size_executable_ =
      heap->CommittedMemoryExecutable();
  heap_statistics->heap_size_limit_ = heap->MaxReserved();
  // TODO(7424): There is no public API for the {WasmEngine} yet. Once such an
  // API becomes available we should report the malloced memory separately. For
  // now we just add the values, thereby over-approximating the peak slightly.
  heap_statistics->malloced_memory_ =
      isolate->allocator()->GetCurrentMemoryUsage() +
      isolate->wasm_engine()->allocator()->GetCurrentMemoryUsage();
  heap_statistics->external_memory_ = isolate->heap()->backing_store_bytes();
  heap_statistics->peak_malloced_memory_ =
      isolate->allocator()->GetMaxMemoryUsage() +
      isolate->wasm_engine()->allocator()->GetMaxMemoryUsage();
  heap_statistics->number_of_native_contexts_ = heap->NumberOfNativeContexts();
  heap_statistics->number_of_detached_contexts_ =
      heap->NumberOfDetachedContexts();
  heap_statistics->does_zap_garbage_ = heap->ShouldZapGarbage();
}

size_t Isolate::NumberOfHeapSpaces() {
  return i::LAST_SPACE - i::FIRST_SPACE + 1;
}

bool Isolate::GetHeapSpaceStatistics(HeapSpaceStatistics* space_statistics,
                                     size_t index) {
  if (!space_statistics) return false;
  if (!i::Heap::IsValidAllocationSpace(static_cast<i::AllocationSpace>(index)))
    return false;

  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = isolate->heap();
  i::Space* space = heap->space(static_cast<int>(index));

  i::AllocationSpace allocation_space = static_cast<i::AllocationSpace>(index);
  space_statistics->space_name_ = i::Heap::GetSpaceName(allocation_space);

  if (allocation_space == i::RO_SPACE && V8_SHARED_RO_HEAP_BOOL) {
    // RO_SPACE memory is accounted for elsewhere when ReadOnlyHeap is shared.
    space_statistics->space_size_ = 0;
    space_statistics->space_used_size_ = 0;
    space_statistics->space_available_size_ = 0;
    space_statistics->physical_space_size_ = 0;
  } else {
    space_statistics->space_size_ = space->CommittedMemory();
    space_statistics->space_used_size_ = space->SizeOfObjects();
    space_statistics->space_available_size_ = space->Available();
    space_statistics->physical_space_size_ = space->CommittedPhysicalMemory();
  }
  return true;
}

size_t Isolate::NumberOfTrackedHeapObjectTypes() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = isolate->heap();
  return heap->NumberOfTrackedHeapObjectTypes();
}

bool Isolate::GetHeapObjectStatisticsAtLastGC(
    HeapObjectStatistics* object_statistics, size_t type_index) {
  if (!object_statistics) return false;
  if (V8_LIKELY(!i::TracingFlags::is_gc_stats_enabled())) return false;

  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = isolate->heap();
  if (type_index >= heap->NumberOfTrackedHeapObjectTypes()) return false;

  const char* object_type;
  const char* object_sub_type;
  size_t object_count = heap->ObjectCountAtLastGC(type_index);
  size_t object_size = heap->ObjectSizeAtLastGC(type_index);
  if (!heap->GetObjectTypeName(type_index, &object_type, &object_sub_type)) {
    // There should be no objects counted when the type is unknown.
    DCHECK_EQ(object_count, 0U);
    DCHECK_EQ(object_size, 0U);
    return false;
  }

  object_statistics->object_type_ = object_type;
  object_statistics->object_sub_type_ = object_sub_type;
  object_statistics->object_count_ = object_count;
  object_statistics->object_size_ = object_size;
  return true;
}

bool Isolate::GetHeapCodeAndMetadataStatistics(
    HeapCodeStatistics* code_statistics) {
  if (!code_statistics) return false;

  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->CollectCodeStatistics();

  code_statistics->code_and_metadata_size_ = isolate->code_and_metadata_size();
  code_statistics->bytecode_and_metadata_size_ =
      isolate->bytecode_and_metadata_size();
  code_statistics->external_script_source_size_ =
      isolate->external_script_source_size();
  return true;
}

v8::MaybeLocal<v8::Promise> Isolate::MeasureMemory(
    v8::Local<v8::Context> context, MeasureMemoryMode mode) {
  return v8::MaybeLocal<v8::Promise>();
}

bool Isolate::MeasureMemory(std::unique_ptr<MeasureMemoryDelegate> delegate,
                            MeasureMemoryExecution execution) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->heap()->MeasureMemory(std::move(delegate), execution);
}

std::unique_ptr<MeasureMemoryDelegate> MeasureMemoryDelegate::Default(
    Isolate* isolate, Local<Context> context,
    Local<Promise::Resolver> promise_resolver, MeasureMemoryMode mode) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::NativeContext> native_context =
      handle(Utils::OpenHandle(*context)->native_context(), i_isolate);
  i::Handle<i::JSPromise> js_promise =
      i::Handle<i::JSPromise>::cast(Utils::OpenHandle(*promise_resolver));
  return i_isolate->heap()->MeasureMemoryDelegate(native_context, js_promise,
                                                  mode);
}

void Isolate::GetStackSample(const RegisterState& state, void** frames,
                             size_t frames_limit, SampleInfo* sample_info) {
  RegisterState regs = state;
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  if (i::TickSample::GetStackSample(isolate, &regs,
                                    i::TickSample::kSkipCEntryFrame, frames,
                                    frames_limit, sample_info)) {
    return;
  }
  sample_info->frames_count = 0;
  sample_info->vm_state = OTHER;
  sample_info->external_callback_entry = nullptr;
}

size_t Isolate::NumberOfPhantomHandleResetsSinceLastCall() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->global_handles()->GetAndResetGlobalHandleResetCount();
}

void Isolate::SetEventLogger(LogEventCallback that) {
  // Do not overwrite the event logger if we want to log explicitly.
  if (i::FLAG_log_internal_timer_events) return;
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->set_event_logger(that);
}

void Isolate::AddBeforeCallEnteredCallback(BeforeCallEnteredCallback callback) {
  if (callback == nullptr) return;
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->AddBeforeCallEnteredCallback(callback);
}

void Isolate::RemoveBeforeCallEnteredCallback(
    BeforeCallEnteredCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->RemoveBeforeCallEnteredCallback(callback);
}

void Isolate::AddCallCompletedCallback(CallCompletedCallback callback) {
  if (callback == nullptr) return;
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->AddCallCompletedCallback(callback);
}

void Isolate::RemoveCallCompletedCallback(CallCompletedCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->RemoveCallCompletedCallback(callback);
}

void Isolate::AtomicsWaitWakeHandle::Wake() {
  reinterpret_cast<i::AtomicsWaitWakeHandle*>(this)->Wake();
}

void Isolate::SetAtomicsWaitCallback(AtomicsWaitCallback callback, void* data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetAtomicsWaitCallback(callback, data);
}

void Isolate::SetPromiseHook(PromiseHook hook) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetPromiseHook(hook);
}

void Isolate::SetPromiseRejectCallback(PromiseRejectCallback callback) {
  if (callback == nullptr) return;
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetPromiseRejectCallback(callback);
}

void Isolate::PerformMicrotaskCheckpoint() {
  DCHECK_NE(MicrotasksPolicy::kScoped, GetMicrotasksPolicy());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->default_microtask_queue()->PerformCheckpoint(this);
}

void Isolate::EnqueueMicrotask(Local<Function> v8_function) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::JSReceiver> function = Utils::OpenHandle(*v8_function);
  i::Handle<i::NativeContext> handler_context;
  if (!i::JSReceiver::GetContextForMicrotask(function).ToHandle(
          &handler_context))
    handler_context = isolate->native_context();
  MicrotaskQueue* microtask_queue = handler_context->microtask_queue();
  if (microtask_queue) microtask_queue->EnqueueMicrotask(this, v8_function);
}

void Isolate::EnqueueMicrotask(MicrotaskCallback callback, void* data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::HandleScope scope(isolate);
  i::Handle<i::CallbackTask> microtask = isolate->factory()->NewCallbackTask(
      isolate->factory()->NewForeign(reinterpret_cast<i::Address>(callback)),
      isolate->factory()->NewForeign(reinterpret_cast<i::Address>(data)));
  isolate->default_microtask_queue()->EnqueueMicrotask(*microtask);
}

void Isolate::SetMicrotasksPolicy(MicrotasksPolicy policy) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->default_microtask_queue()->set_microtasks_policy(policy);
}

MicrotasksPolicy Isolate::GetMicrotasksPolicy() const {
  i::Isolate* isolate =
      reinterpret_cast<i::Isolate*>(const_cast<Isolate*>(this));
  return isolate->default_microtask_queue()->microtasks_policy();
}

namespace {

void MicrotasksCompletedCallbackAdapter(v8::Isolate* isolate, void* data) {
  auto callback = reinterpret_cast<void (*)(v8::Isolate*)>(data);
  callback(isolate);
}

}  // namespace

void Isolate::AddMicrotasksCompletedCallback(
    MicrotasksCompletedCallback callback) {
  DCHECK(callback);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->default_microtask_queue()->AddMicrotasksCompletedCallback(
      &MicrotasksCompletedCallbackAdapter, reinterpret_cast<void*>(callback));
}

void Isolate::AddMicrotasksCompletedCallback(
    MicrotasksCompletedCallbackWithData callback, void* data) {
  DCHECK(callback);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->default_microtask_queue()->AddMicrotasksCompletedCallback(callback,
                                                                     data);
}

void Isolate::RemoveMicrotasksCompletedCallback(
    MicrotasksCompletedCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->default_microtask_queue()->RemoveMicrotasksCompletedCallback(
      &MicrotasksCompletedCallbackAdapter, reinterpret_cast<void*>(callback));
}

void Isolate::RemoveMicrotasksCompletedCallback(
    MicrotasksCompletedCallbackWithData callback, void* data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->default_microtask_queue()->RemoveMicrotasksCompletedCallback(
      callback, data);
}

void Isolate::SetUseCounterCallback(UseCounterCallback callback) {
  reinterpret_cast<i::Isolate*>(this)->SetUseCounterCallback(callback);
}

void Isolate::SetCounterFunction(CounterLookupCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->counters()->ResetCounterFunction(callback);
}

void Isolate::SetCreateHistogramFunction(CreateHistogramCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->counters()->ResetCreateHistogramFunction(callback);
}

void Isolate::SetAddHistogramSampleFunction(
    AddHistogramSampleCallback callback) {
  reinterpret_cast<i::Isolate*>(this)
      ->counters()
      ->SetAddHistogramSampleFunction(callback);
}

void Isolate::SetAddCrashKeyCallback(AddCrashKeyCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetAddCrashKeyCallback(callback);
}

bool Isolate::IdleNotificationDeadline(double deadline_in_seconds) {
  // Returning true tells the caller that it need not
  // continue to call IdleNotification.
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  if (!i::FLAG_use_idle_notification) return true;
  return isolate->heap()->IdleNotification(deadline_in_seconds);
}

void Isolate::LowMemoryNotification() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  {
    i::HistogramTimerScope idle_notification_scope(
        isolate->counters()->gc_low_memory_notification());
    TRACE_EVENT0("v8", "V8.GCLowMemoryNotification");
    isolate->heap()->CollectAllAvailableGarbage(
        i::GarbageCollectionReason::kLowMemoryNotification);
  }
}

int Isolate::ContextDisposedNotification(bool dependant_context) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  if (!dependant_context) {
    if (!isolate->context().is_null()) {
      // We left the current context, we can abort all WebAssembly compilations
      // of that context.
      // A handle scope for the native context.
      i::HandleScope handle_scope(isolate);
      isolate->wasm_engine()->DeleteCompileJobsOnContext(
          isolate->native_context());
    }
  }
  // TODO(ahaas): move other non-heap activity out of the heap call.
  return isolate->heap()->NotifyContextDisposed(dependant_context);
}

void Isolate::IsolateInForegroundNotification() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->IsolateInForegroundNotification();
}

void Isolate::IsolateInBackgroundNotification() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->IsolateInBackgroundNotification();
}

void Isolate::MemoryPressureNotification(MemoryPressureLevel level) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  bool on_isolate_thread =
      v8::Locker::IsActive()
          ? isolate->thread_manager()->IsLockedByCurrentThread()
          : i::ThreadId::Current() == isolate->thread_id();
  isolate->heap()->MemoryPressureNotification(level, on_isolate_thread);
}

void Isolate::EnableMemorySavingsMode() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->EnableMemorySavingsMode();
}

void Isolate::DisableMemorySavingsMode() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->DisableMemorySavingsMode();
}

void Isolate::SetRAILMode(RAILMode rail_mode) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->SetRAILMode(rail_mode);
}

void Isolate::IncreaseHeapLimitForDebugging() {
  // No-op.
}

void Isolate::RestoreOriginalHeapLimit() {
  // No-op.
}

bool Isolate::IsHeapLimitIncreasedForDebugging() { return false; }

void Isolate::SetJitCodeEventHandler(JitCodeEventOptions options,
                                     JitCodeEventHandler event_handler) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  // Ensure that logging is initialized for our isolate.
  isolate->InitializeLoggingAndCounters();
  isolate->logger()->SetCodeEventHandler(options, event_handler);
}

void Isolate::SetStackLimit(uintptr_t stack_limit) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  CHECK(stack_limit);
  isolate->stack_guard()->SetStackLimit(stack_limit);
}

void Isolate::GetCodeRange(void** start, size_t* length_in_bytes) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  const base::AddressRegion& code_range =
      isolate->heap()->memory_allocator()->code_range();
  *start = reinterpret_cast<void*>(code_range.begin());
  *length_in_bytes = code_range.size();
}

UnwindState Isolate::GetUnwindState() {
  UnwindState unwind_state;
  void* code_range_start;
  GetCodeRange(&code_range_start, &unwind_state.code_range.length_in_bytes);
  unwind_state.code_range.start = code_range_start;

  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  unwind_state.embedded_code_range.start =
      reinterpret_cast<const void*>(isolate->embedded_blob());
  unwind_state.embedded_code_range.length_in_bytes =
      isolate->embedded_blob_size();

  std::array<std::pair<i::Builtins::Name, JSEntryStub*>, 3> entry_stubs = {
      {{i::Builtins::kJSEntry, &unwind_state.js_entry_stub},
       {i::Builtins::kJSConstructEntry, &unwind_state.js_construct_entry_stub},
       {i::Builtins::kJSRunMicrotasksEntry,
        &unwind_state.js_run_microtasks_entry_stub}}};
  for (auto& pair : entry_stubs) {
    i::Code js_entry = isolate->heap()->builtin(pair.first);
    pair.second->code.start =
        reinterpret_cast<const void*>(js_entry.InstructionStart());
    pair.second->code.length_in_bytes = js_entry.InstructionSize();
  }

  return unwind_state;
}

JSEntryStubs Isolate::GetJSEntryStubs() {
  JSEntryStubs entry_stubs;

  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  std::array<std::pair<i::Builtins::Name, JSEntryStub*>, 3> stubs = {
      {{i::Builtins::kJSEntry, &entry_stubs.js_entry_stub},
       {i::Builtins::kJSConstructEntry, &entry_stubs.js_construct_entry_stub},
       {i::Builtins::kJSRunMicrotasksEntry,
        &entry_stubs.js_run_microtasks_entry_stub}}};
  for (auto& pair : stubs) {
    i::Code js_entry = isolate->heap()->builtin(pair.first);
    pair.second->code.start =
        reinterpret_cast<const void*>(js_entry.InstructionStart());
    pair.second->code.length_in_bytes = js_entry.InstructionSize();
  }

  return entry_stubs;
}

size_t Isolate::CopyCodePages(size_t capacity, MemoryRange* code_pages_out) {
#if !defined(V8_TARGET_ARCH_64_BIT) && !defined(V8_TARGET_ARCH_ARM)
  // Not implemented on other platforms.
  UNREACHABLE();
#else

  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  std::vector<MemoryRange>* code_pages = isolate->GetCodePages();

  DCHECK_NOT_NULL(code_pages);

  // Copy as many elements into the output vector as we can. If the
  // caller-provided buffer is not big enough, we fill it, and the caller can
  // provide a bigger one next time. We do it this way because allocation is not
  // allowed in signal handlers.
  size_t limit = std::min(capacity, code_pages->size());
  for (size_t i = 0; i < limit; i++) {
    code_pages_out[i] = code_pages->at(i);
  }
  return code_pages->size();
#endif
}

#define CALLBACK_SETTER(ExternalName, Type, InternalName)      \
  void Isolate::Set##ExternalName(Type callback) {             \
    i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this); \
    isolate->set_##InternalName(callback);                     \
  }

CALLBACK_SETTER(FatalErrorHandler, FatalErrorCallback, exception_behavior)
CALLBACK_SETTER(OOMErrorHandler, OOMErrorCallback, oom_behavior)
CALLBACK_SETTER(AllowCodeGenerationFromStringsCallback,
                AllowCodeGenerationFromStringsCallback, allow_code_gen_callback)
CALLBACK_SETTER(ModifyCodeGenerationFromStringsCallback,
                ModifyCodeGenerationFromStringsCallback,
                modify_code_gen_callback)
CALLBACK_SETTER(AllowWasmCodeGenerationCallback,
                AllowWasmCodeGenerationCallback, allow_wasm_code_gen_callback)

CALLBACK_SETTER(WasmModuleCallback, ExtensionCallback, wasm_module_callback)
CALLBACK_SETTER(WasmInstanceCallback, ExtensionCallback, wasm_instance_callback)

CALLBACK_SETTER(WasmStreamingCallback, WasmStreamingCallback,
                wasm_streaming_callback)

CALLBACK_SETTER(WasmThreadsEnabledCallback, WasmThreadsEnabledCallback,
                wasm_threads_enabled_callback)

CALLBACK_SETTER(WasmLoadSourceMapCallback, WasmLoadSourceMapCallback,
                wasm_load_source_map_callback)

void Isolate::AddNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                       void* data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->AddNearHeapLimitCallback(callback, data);
}

void Isolate::RemoveNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                          size_t heap_limit) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->RemoveNearHeapLimitCallback(callback, heap_limit);
}

void Isolate::AutomaticallyRestoreInitialHeapLimit(double threshold_percent) {
  DCHECK_GT(threshold_percent, 0.0);
  DCHECK_LT(threshold_percent, 1.0);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->AutomaticallyRestoreInitialHeapLimit(threshold_percent);
}

bool Isolate::IsDead() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->IsDead();
}

bool Isolate::AddMessageListener(MessageCallback that, Local<Value> data) {
  return AddMessageListenerWithErrorLevel(that, kMessageError, data);
}

bool Isolate::AddMessageListenerWithErrorLevel(MessageCallback that,
                                               int message_levels,
                                               Local<Value> data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::TemplateList> list = isolate->factory()->message_listeners();
  i::Handle<i::FixedArray> listener = isolate->factory()->NewFixedArray(3);
  i::Handle<i::Foreign> foreign =
      isolate->factory()->NewForeign(FUNCTION_ADDR(that));
  listener->set(0, *foreign);
  listener->set(1, data.IsEmpty() ? i::ReadOnlyRoots(isolate).undefined_value()
                                  : *Utils::OpenHandle(*data));
  listener->set(2, i::Smi::FromInt(message_levels));
  list = i::TemplateList::Add(isolate, list, listener);
  isolate->heap()->SetMessageListeners(*list);
  return true;
}

void Isolate::RemoveMessageListeners(MessageCallback that) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  i::DisallowHeapAllocation no_gc;
  i::TemplateList listeners = isolate->heap()->message_listeners();
  for (int i = 0; i < listeners.length(); i++) {
    if (listeners.get(i).IsUndefined(isolate)) continue;  // skip deleted ones
    i::FixedArray listener = i::FixedArray::cast(listeners.get(i));
    i::Foreign callback_obj = i::Foreign::cast(listener.get(0));
    if (callback_obj.foreign_address() == FUNCTION_ADDR(that)) {
      listeners.set(i, i::ReadOnlyRoots(isolate).undefined_value());
    }
  }
}

void Isolate::SetFailedAccessCheckCallbackFunction(
    FailedAccessCheckCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetFailedAccessCheckCallback(callback);
}

void Isolate::SetCaptureStackTraceForUncaughtExceptions(
    bool capture, int frame_limit, StackTrace::StackTraceOptions options) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->SetCaptureStackTraceForUncaughtExceptions(capture, frame_limit,
                                                     options);
}

void Isolate::VisitExternalResources(ExternalResourceVisitor* visitor) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->VisitExternalResources(visitor);
}

bool Isolate::IsInUse() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->IsInUse();
}

void Isolate::VisitHandlesWithClassIds(PersistentHandleVisitor* visitor) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::DisallowHeapAllocation no_allocation;
  isolate->global_handles()->IterateAllRootsWithClassIds(visitor);
}

void Isolate::VisitWeakHandles(PersistentHandleVisitor* visitor) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::DisallowHeapAllocation no_allocation;
  isolate->global_handles()->IterateYoungWeakRootsWithClassIds(visitor);
}

void Isolate::SetAllowAtomicsWait(bool allow) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->set_allow_atomics_wait(allow);
}

void v8::Isolate::DateTimeConfigurationChangeNotification(
    TimeZoneDetection time_zone_detection) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  LOG_API(i_isolate, Isolate, DateTimeConfigurationChangeNotification);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i_isolate->date_cache()->ResetDateCache(
      static_cast<base::TimezoneCache::TimeZoneDetection>(time_zone_detection));
#ifdef V8_INTL_SUPPORT
  i_isolate->clear_cached_icu_object(
      i::Isolate::ICUObjectCacheType::kDefaultSimpleDateFormat);
  i_isolate->clear_cached_icu_object(
      i::Isolate::ICUObjectCacheType::kDefaultSimpleDateFormatForTime);
  i_isolate->clear_cached_icu_object(
      i::Isolate::ICUObjectCacheType::kDefaultSimpleDateFormatForDate);
#endif  // V8_INTL_SUPPORT
}

void v8::Isolate::LocaleConfigurationChangeNotification() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  LOG_API(i_isolate, Isolate, LocaleConfigurationChangeNotification);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

#ifdef V8_INTL_SUPPORT
  i_isolate->ResetDefaultLocale();
#endif  // V8_INTL_SUPPORT
}

// static
std::unique_ptr<MicrotaskQueue> MicrotaskQueue::New(Isolate* isolate,
                                                    MicrotasksPolicy policy) {
  auto microtask_queue =
      i::MicrotaskQueue::New(reinterpret_cast<i::Isolate*>(isolate));
  microtask_queue->set_microtasks_policy(policy);
  std::unique_ptr<MicrotaskQueue> ret(std::move(microtask_queue));
  return ret;
}

MicrotasksScope::MicrotasksScope(Isolate* isolate, MicrotasksScope::Type type)
    : MicrotasksScope(isolate, nullptr, type) {}

MicrotasksScope::MicrotasksScope(Isolate* isolate,
                                 MicrotaskQueue* microtask_queue,
                                 MicrotasksScope::Type type)
    : isolate_(reinterpret_cast<i::Isolate*>(isolate)),
      microtask_queue_(microtask_queue
                           ? static_cast<i::MicrotaskQueue*>(microtask_queue)
                           : isolate_->default_microtask_queue()),
      run_(type == MicrotasksScope::kRunMicrotasks) {
  if (run_) microtask_queue_->IncrementMicrotasksScopeDepth();
#ifdef DEBUG
  if (!run_) microtask_queue_->IncrementDebugMicrotasksScopeDepth();
#endif
}

MicrotasksScope::~MicrotasksScope() {
  if (run_) {
    microtask_queue_->DecrementMicrotasksScopeDepth();
    if (MicrotasksPolicy::kScoped == microtask_queue_->microtasks_policy() &&
        !isolate_->has_scheduled_exception()) {
      DCHECK_IMPLIES(isolate_->has_scheduled_exception(),
                     isolate_->scheduled_exception() ==
                         i::ReadOnlyRoots(isolate_).termination_exception());
      microtask_queue_->PerformCheckpoint(reinterpret_cast<Isolate*>(isolate_));
    }
  }
#ifdef DEBUG
  if (!run_) microtask_queue_->DecrementDebugMicrotasksScopeDepth();
#endif
}

void MicrotasksScope::PerformCheckpoint(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  auto* microtask_queue = isolate->default_microtask_queue();
  microtask_queue->PerformCheckpoint(v8_isolate);
}

int MicrotasksScope::GetCurrentDepth(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  auto* microtask_queue = isolate->default_microtask_queue();
  return microtask_queue->GetMicrotasksScopeDepth();
}

bool MicrotasksScope::IsRunningMicrotasks(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  auto* microtask_queue = isolate->default_microtask_queue();
  return microtask_queue->IsRunningMicrotasks();
}

String::Utf8Value::Utf8Value(v8::Isolate* isolate, v8::Local<v8::Value> obj)
    : str_(nullptr), length_(0) {
  if (obj.IsEmpty()) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_DO_NOT_USE(i_isolate);
  i::HandleScope scope(i_isolate);
  Local<Context> context = isolate->GetCurrentContext();
  TryCatch try_catch(isolate);
  Local<String> str;
  if (!obj->ToString(context).ToLocal(&str)) return;
  length_ = str->Utf8Length(isolate);
  str_ = i::NewArray<char>(length_ + 1);
  str->WriteUtf8(isolate, str_);
}

String::Utf8Value::~Utf8Value() { i::DeleteArray(str_); }

String::Value::Value(v8::Isolate* isolate, v8::Local<v8::Value> obj)
    : str_(nullptr), length_(0) {
  if (obj.IsEmpty()) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_DO_NOT_USE(i_isolate);
  i::HandleScope scope(i_isolate);
  Local<Context> context = isolate->GetCurrentContext();
  TryCatch try_catch(isolate);
  Local<String> str;
  if (!obj->ToString(context).ToLocal(&str)) return;
  length_ = str->Length();
  str_ = i::NewArray<uint16_t>(length_ + 1);
  str->Write(isolate, str_);
}

String::Value::~Value() { i::DeleteArray(str_); }

#define DEFINE_ERROR(NAME, name)                                         \
  Local<Value> Exception::NAME(v8::Local<v8::String> raw_message) {      \
    i::Isolate* isolate = i::Isolate::Current();                         \
    LOG_API(isolate, NAME, New);                                         \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);                            \
    i::Object error;                                                     \
    {                                                                    \
      i::HandleScope scope(isolate);                                     \
      i::Handle<i::String> message = Utils::OpenHandle(*raw_message);    \
      i::Handle<i::JSFunction> constructor = isolate->name##_function(); \
      error = *isolate->factory()->NewError(constructor, message);       \
    }                                                                    \
    i::Handle<i::Object> result(error, isolate);                         \
    return Utils::ToLocal(result);                                       \
  }

DEFINE_ERROR(RangeError, range_error)
DEFINE_ERROR(ReferenceError, reference_error)
DEFINE_ERROR(SyntaxError, syntax_error)
DEFINE_ERROR(TypeError, type_error)
DEFINE_ERROR(Error, error)

#undef DEFINE_ERROR

Local<Message> Exception::CreateMessage(Isolate* isolate,
                                        Local<Value> exception) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*exception);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  return Utils::MessageToLocal(
      scope.CloseAndEscape(i_isolate->CreateMessage(obj, nullptr)));
}

Local<StackTrace> Exception::GetStackTrace(Local<Value> exception) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*exception);
  if (!obj->IsJSObject()) return Local<StackTrace>();
  i::Handle<i::JSObject> js_obj = i::Handle<i::JSObject>::cast(obj);
  i::Isolate* isolate = js_obj->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  return Utils::StackTraceToLocal(isolate->GetDetailedStackTrace(js_obj));
}

// --- D e b u g   S u p p o r t ---

void debug::SetContextId(Local<Context> context, int id) {
  Utils::OpenHandle(*context)->set_debug_context_id(i::Smi::FromInt(id));
}

int debug::GetContextId(Local<Context> context) {
  i::Object value = Utils::OpenHandle(*context)->debug_context_id();
  return (value.IsSmi()) ? i::Smi::ToInt(value) : 0;
}

void debug::SetInspector(Isolate* isolate,
                         v8_inspector::V8Inspector* inspector) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->set_inspector(inspector);
}

v8_inspector::V8Inspector* debug::GetInspector(Isolate* isolate) {
  return reinterpret_cast<i::Isolate*>(isolate)->inspector();
}

void debug::SetBreakOnNextFunctionCall(Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)->debug()->SetBreakOnNextFunctionCall();
}

void debug::ClearBreakOnNextFunctionCall(Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)
      ->debug()
      ->ClearBreakOnNextFunctionCall();
}

MaybeLocal<Array> debug::GetInternalProperties(Isolate* v8_isolate,
                                               Local<Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  i::Handle<i::JSArray> result;
  if (!i::Runtime::GetInternalProperties(isolate, val).ToHandle(&result))
    return MaybeLocal<Array>();
  return Utils::ToLocal(result);
}

bool debug::GetPrivateMembers(Local<Context> context, Local<Object> value,
                              std::vector<Local<Value>>* names_out,
                              std::vector<Local<Value>>* values_out) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  LOG_API(isolate, debug, GetPrivateMembers);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::JSReceiver> receiver = Utils::OpenHandle(*value);
  i::Handle<i::JSArray> names;
  i::Handle<i::FixedArray> values;

  i::PropertyFilter key_filter =
      static_cast<i::PropertyFilter>(i::PropertyFilter::PRIVATE_NAMES_ONLY);
  i::Handle<i::FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      i::KeyAccumulator::GetKeys(receiver, i::KeyCollectionMode::kOwnOnly,
                                 key_filter,
                                 i::GetKeysConversion::kConvertToString),
      false);

  // Estimate number of private entries to return in the FixedArray.
  int private_entries_count = 0;
  for (int i = 0; i < keys->length(); ++i) {
    // Exclude the private brand symbols.
    i::Handle<i::Symbol> key(i::Symbol::cast(keys->get(i)), isolate);
    if (key->is_private_brand()) {
      i::Handle<i::Object> value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, value, i::Object::GetProperty(isolate, receiver, key),
          false);

      i::Handle<i::Context> context(i::Context::cast(*value), isolate);
      i::Handle<i::ScopeInfo> scope_info(context->scope_info(), isolate);
      // At least one slot contains the brand symbol so it does not count.
      private_entries_count += (scope_info->ContextLocalCount() - 1);
    } else {
      private_entries_count++;
    }
  }

  DCHECK(names_out->empty());
  names_out->reserve(private_entries_count);
  DCHECK(values_out->empty());
  values_out->reserve(private_entries_count);
  for (int i = 0; i < keys->length(); ++i) {
    i::Handle<i::Object> obj_key(keys->get(i), isolate);
    i::Handle<i::Symbol> key(i::Symbol::cast(*obj_key), isolate);
    CHECK(key->is_private_name());
    i::Handle<i::Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, i::Object::GetProperty(isolate, receiver, key), false);

    if (key->is_private_brand()) {
      DCHECK(value->IsContext());
      i::Handle<i::Context> context(i::Context::cast(*value), isolate);
      i::Handle<i::ScopeInfo> scope_info(context->scope_info(), isolate);
      int local_count = scope_info->ContextLocalCount();

      for (int j = 0; j < local_count; ++j) {
        i::VariableMode mode = scope_info->ContextLocalMode(j);
        if (!i::IsPrivateMethodOrAccessorVariableMode(mode)) {
          continue;
        }

        i::Handle<i::String> name(scope_info->ContextLocalName(j), isolate);
        int context_index = scope_info->ContextHeaderLength() + j;
        i::Handle<i::Object> slot_value(context->get(context_index), isolate);
        DCHECK_IMPLIES(mode == i::VariableMode::kPrivateMethod,
                       slot_value->IsJSFunction());
        DCHECK_IMPLIES(mode != i::VariableMode::kPrivateMethod,
                       slot_value->IsAccessorPair());
        names_out->push_back(Utils::ToLocal(name));
        values_out->push_back(Utils::ToLocal(slot_value));
      }
    } else {  // Private fields
      i::Handle<i::String> name(
          i::String::cast(i::Symbol::cast(*key).description()), isolate);
      names_out->push_back(Utils::ToLocal(name));
      values_out->push_back(Utils::ToLocal(value));
    }
  }

  DCHECK_EQ(names_out->size(), values_out->size());
  DCHECK_LE(names_out->size(), private_entries_count);
  return true;
}

Local<Context> debug::GetCreationContext(Local<Object> value) {
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  if (val->IsJSGlobalProxy()) {
    return Local<Context>();
  }
  return value->CreationContext();
}

void debug::ChangeBreakOnException(Isolate* isolate, ExceptionBreakState type) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal_isolate->debug()->ChangeBreakOnException(
      i::BreakException, type == BreakOnAnyException);
  internal_isolate->debug()->ChangeBreakOnException(i::BreakUncaughtException,
                                                    type != NoBreakOnException);
}

void debug::SetBreakPointsActive(Isolate* v8_isolate, bool is_active) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  isolate->debug()->set_break_points_active(is_active);
}

void debug::PrepareStep(Isolate* v8_isolate, StepAction action) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_DO_NOT_USE(isolate);
  CHECK(isolate->debug()->CheckExecutionState());
  // Clear all current stepping setup.
  isolate->debug()->ClearStepping();
  // Prepare step.
  isolate->debug()->PrepareStep(static_cast<i::StepAction>(action));
}

void debug::ClearStepping(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  // Clear all current stepping setup.
  isolate->debug()->ClearStepping();
}

void debug::BreakRightNow(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_DO_NOT_USE(isolate);
  isolate->debug()->HandleDebugBreak(i::kIgnoreIfAllFramesBlackboxed);
}

bool debug::AllFramesOnStackAreBlackboxed(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_DO_NOT_USE(isolate);
  return isolate->debug()->AllFramesOnStackAreBlackboxed();
}

v8::Isolate* debug::Script::GetIsolate() const {
  return reinterpret_cast<v8::Isolate*>(Utils::OpenHandle(this)->GetIsolate());
}

ScriptOriginOptions debug::Script::OriginOptions() const {
  return Utils::OpenHandle(this)->origin_options();
}

bool debug::Script::WasCompiled() const {
  return Utils::OpenHandle(this)->compilation_state() ==
         i::Script::COMPILATION_STATE_COMPILED;
}

bool debug::Script::IsEmbedded() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  return script->context_data() ==
         script->GetReadOnlyRoots().uninitialized_symbol();
}

int debug::Script::Id() const { return Utils::OpenHandle(this)->id(); }

int debug::Script::LineOffset() const {
  return Utils::OpenHandle(this)->line_offset();
}

int debug::Script::ColumnOffset() const {
  return Utils::OpenHandle(this)->column_offset();
}

std::vector<int> debug::Script::LineEnds() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  if (script->type() == i::Script::TYPE_WASM) return std::vector<int>();

  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope scope(isolate);
  i::Script::InitLineEnds(script);
  CHECK(script->line_ends().IsFixedArray());
  i::Handle<i::FixedArray> line_ends(i::FixedArray::cast(script->line_ends()),
                                     isolate);
  std::vector<int> result(line_ends->length());
  for (int i = 0; i < line_ends->length(); ++i) {
    i::Smi line_end = i::Smi::cast(line_ends->get(i));
    result[i] = line_end.value();
  }
  return result;
}

MaybeLocal<String> debug::Script::Name() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> value(script->name(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

MaybeLocal<String> debug::Script::SourceURL() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> value(script->source_url(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

MaybeLocal<String> debug::Script::SourceMappingURL() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> value(script->source_mapping_url(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

Maybe<int> debug::Script::ContextId() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Object value = script->context_data();
  if (value.IsSmi()) return Just(i::Smi::ToInt(value));
  return Nothing<int>();
}

MaybeLocal<String> debug::Script::Source() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> value(script->source(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

bool debug::Script::IsWasm() const {
  return Utils::OpenHandle(this)->type() == i::Script::TYPE_WASM;
}

bool debug::Script::IsModule() const {
  return Utils::OpenHandle(this)->origin_options().IsModule();
}

namespace {
int GetSmiValue(i::Handle<i::FixedArray> array, int index) {
  return i::Smi::ToInt(array->get(index));
}

bool CompareBreakLocation(const i::BreakLocation& loc1,
                          const i::BreakLocation& loc2) {
  return loc1.position() < loc2.position();
}
}  // namespace

bool debug::Script::GetPossibleBreakpoints(
    const debug::Location& start, const debug::Location& end,
    bool restrict_to_function,
    std::vector<debug::BreakLocation>* locations) const {
  CHECK(!start.IsEmpty());
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  if (script->type() == i::Script::TYPE_WASM) {
    i::wasm::NativeModule* native_module = script->wasm_native_module();
    return i::WasmScript::GetPossibleBreakpoints(native_module, start, end,
                                                 locations);
  }

  i::Script::InitLineEnds(script);
  CHECK(script->line_ends().IsFixedArray());
  i::Isolate* isolate = script->GetIsolate();
  i::Handle<i::FixedArray> line_ends =
      i::Handle<i::FixedArray>::cast(i::handle(script->line_ends(), isolate));
  CHECK(line_ends->length());

  int start_offset = GetSourceOffset(start);
  int end_offset = end.IsEmpty()
                       ? GetSmiValue(line_ends, line_ends->length() - 1) + 1
                       : GetSourceOffset(end);
  if (start_offset >= end_offset) return true;

  std::vector<i::BreakLocation> v8_locations;
  if (!isolate->debug()->GetPossibleBreakpoints(
          script, start_offset, end_offset, restrict_to_function,
          &v8_locations)) {
    return false;
  }

  std::sort(v8_locations.begin(), v8_locations.end(), CompareBreakLocation);
  int current_line_end_index = 0;
  for (const auto& v8_location : v8_locations) {
    int offset = v8_location.position();
    while (offset > GetSmiValue(line_ends, current_line_end_index)) {
      ++current_line_end_index;
      CHECK(current_line_end_index < line_ends->length());
    }
    int line_offset = 0;

    if (current_line_end_index > 0) {
      line_offset = GetSmiValue(line_ends, current_line_end_index - 1) + 1;
    }
    locations->emplace_back(
        current_line_end_index + script->line_offset(),
        offset - line_offset +
            (current_line_end_index == 0 ? script->column_offset() : 0),
        v8_location.type());
  }
  return true;
}

int debug::Script::GetSourceOffset(const debug::Location& location) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  if (script->type() == i::Script::TYPE_WASM) {
    DCHECK_EQ(0, location.GetLineNumber());
    return location.GetColumnNumber();
  }

  int line = std::max(location.GetLineNumber() - script->line_offset(), 0);
  int column = location.GetColumnNumber();
  if (line == 0) {
    column = std::max(0, column - script->column_offset());
  }

  i::Script::InitLineEnds(script);
  CHECK(script->line_ends().IsFixedArray());
  i::Handle<i::FixedArray> line_ends = i::Handle<i::FixedArray>::cast(
      i::handle(script->line_ends(), script->GetIsolate()));
  CHECK(line_ends->length());
  if (line >= line_ends->length())
    return GetSmiValue(line_ends, line_ends->length() - 1);
  int line_offset = GetSmiValue(line_ends, line);
  if (line == 0) return std::min(column, line_offset);
  int prev_line_offset = GetSmiValue(line_ends, line - 1);
  return std::min(prev_line_offset + column + 1, line_offset);
}

v8::debug::Location debug::Script::GetSourceLocation(int offset) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Script::PositionInfo info;
  i::Script::GetPositionInfo(script, offset, &info, i::Script::WITH_OFFSET);
  return debug::Location(info.line, info.column);
}

bool debug::Script::SetScriptSource(v8::Local<v8::String> newSource,
                                    bool preview,
                                    debug::LiveEditResult* result) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  return isolate->debug()->SetScriptSource(
      script, Utils::OpenHandle(*newSource), preview, result);
}

bool debug::Script::SetBreakpoint(v8::Local<v8::String> condition,
                                  debug::Location* location,
                                  debug::BreakpointId* id) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  int offset = GetSourceOffset(*location);
  if (!isolate->debug()->SetBreakPointForScript(
          script, Utils::OpenHandle(*condition), &offset, id)) {
    return false;
  }
  *location = GetSourceLocation(offset);
  return true;
}

bool debug::Script::SetBreakpointOnScriptEntry(BreakpointId* id) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::SharedFunctionInfo::ScriptIterator it(isolate, *script);
  for (i::SharedFunctionInfo sfi = it.Next(); !sfi.is_null(); sfi = it.Next()) {
    if (sfi.is_toplevel()) {
      return isolate->debug()->SetBreakpointForFunction(
          handle(sfi, isolate), isolate->factory()->empty_string(), id);
    }
  }
  return false;
}

void debug::Script::RemoveWasmBreakpoint(debug::BreakpointId id) {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  isolate->debug()->RemoveBreakpointForWasmScript(script, id);
}

void debug::RemoveBreakpoint(Isolate* v8_isolate, BreakpointId id) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::HandleScope handle_scope(isolate);
  isolate->debug()->RemoveBreakpoint(id);
}

v8::Platform* debug::GetCurrentPlatform() {
  return i::V8::GetCurrentPlatform();
}

debug::WasmScript* debug::WasmScript::Cast(debug::Script* script) {
  CHECK(script->IsWasm());
  return static_cast<WasmScript*>(script);
}

int debug::WasmScript::NumFunctions() const {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_GE(i::kMaxInt, module->functions.size());
  return static_cast<int>(module->functions.size());
}

int debug::WasmScript::NumImportedFunctions() const {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_GE(i::kMaxInt, module->num_imported_functions);
  return static_cast<int>(module->num_imported_functions);
}

MemorySpan<const uint8_t> debug::WasmScript::Bytecode() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Vector<const uint8_t> wire_bytes =
      script->wasm_native_module()->wire_bytes();
  return {wire_bytes.begin(), wire_bytes.size()};
}

std::pair<int, int> debug::WasmScript::GetFunctionRange(
    int function_index) const {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_LE(0, function_index);
  DCHECK_GT(module->functions.size(), function_index);
  const i::wasm::WasmFunction& func = module->functions[function_index];
  DCHECK_GE(i::kMaxInt, func.code.offset());
  DCHECK_GE(i::kMaxInt, func.code.end_offset());
  return std::make_pair(static_cast<int>(func.code.offset()),
                        static_cast<int>(func.code.end_offset()));
}

int debug::WasmScript::GetContainingFunction(int byte_offset) const {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_LE(0, byte_offset);

  return i::wasm::GetContainingWasmFunction(module, byte_offset);
}

uint32_t debug::WasmScript::GetFunctionHash(int function_index) {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_LE(0, function_index);
  DCHECK_GT(module->functions.size(), function_index);
  const i::wasm::WasmFunction& func = module->functions[function_index];
  i::wasm::ModuleWireBytes wire_bytes(native_module->wire_bytes());
  i::Vector<const i::byte> function_bytes = wire_bytes.GetFunctionBytes(&func);
  // TODO(herhut): Maybe also take module, name and signature into account.
  return i::StringHasher::HashSequentialString(function_bytes.begin(),
                                               function_bytes.length(), 0);
}

debug::Location::Location(int line_number, int column_number)
    : line_number_(line_number),
      column_number_(column_number),
      is_empty_(false) {}

debug::Location::Location()
    : line_number_(v8::Function::kLineOffsetNotFound),
      column_number_(v8::Function::kLineOffsetNotFound),
      is_empty_(true) {}

int debug::Location::GetLineNumber() const {
  DCHECK(!IsEmpty());
  return line_number_;
}

int debug::Location::GetColumnNumber() const {
  DCHECK(!IsEmpty());
  return column_number_;
}

bool debug::Location::IsEmpty() const { return is_empty_; }

void debug::GetLoadedScripts(v8::Isolate* v8_isolate,
                             PersistentValueVector<debug::Script>& scripts) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  {
    i::DisallowHeapAllocation no_gc;
    i::Script::Iterator iterator(isolate);
    for (i::Script script = iterator.Next(); !script.is_null();
         script = iterator.Next()) {
      if (script.type() == i::Script::TYPE_NORMAL ||
          script.type() == i::Script::TYPE_WASM) {
        if (script.HasValidSource()) {
          i::HandleScope handle_scope(isolate);
          i::Handle<i::Script> script_handle(script, isolate);
          scripts.Append(ToApiHandle<Script>(script_handle));
        }
      }
    }
  }
}

MaybeLocal<UnboundScript> debug::CompileInspectorScript(Isolate* v8_isolate,
                                                        Local<String> source) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE(isolate, UnboundScript);
  i::Handle<i::String> str = Utils::OpenHandle(*source);
  i::Handle<i::SharedFunctionInfo> result;
  {
    ScriptOriginOptions origin_options;
    i::ScriptData* script_data = nullptr;
    i::MaybeHandle<i::SharedFunctionInfo> maybe_function_info =
        i::Compiler::GetSharedFunctionInfoForScript(
            isolate, str, i::Compiler::ScriptDetails(), origin_options, nullptr,
            script_data, ScriptCompiler::kNoCompileOptions,
            ScriptCompiler::kNoCacheBecauseInspector,
            i::FLAG_expose_inspector_scripts ? i::NOT_NATIVES_CODE
                                             : i::INSPECTOR_CODE);
    has_pending_exception = !maybe_function_info.ToHandle(&result);
    RETURN_ON_FAILED_EXECUTION(UnboundScript);
  }
  RETURN_ESCAPED(ToApiHandle<UnboundScript>(result));
}

void debug::SetDebugDelegate(Isolate* v8_isolate,
                             debug::DebugDelegate* delegate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->debug()->SetDebugDelegate(delegate);
}

void debug::SetAsyncEventDelegate(Isolate* v8_isolate,
                                  debug::AsyncEventDelegate* delegate) {
  reinterpret_cast<i::Isolate*>(v8_isolate)->set_async_event_delegate(delegate);
}

void debug::ResetBlackboxedStateCache(Isolate* v8_isolate,
                                      v8::Local<debug::Script> script) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::DisallowHeapAllocation no_gc;
  i::SharedFunctionInfo::ScriptIterator iter(isolate,
                                             *Utils::OpenHandle(*script));
  for (i::SharedFunctionInfo info = iter.Next(); !info.is_null();
       info = iter.Next()) {
    if (info.HasDebugInfo()) {
      info.GetDebugInfo().set_computed_debug_is_blackboxed(false);
    }
  }
}

int debug::EstimatedValueSize(Isolate* v8_isolate, v8::Local<v8::Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Object> object = Utils::OpenHandle(*value);
  if (object->IsSmi()) return i::kTaggedSize;
  CHECK(object->IsHeapObject());
  return i::Handle<i::HeapObject>::cast(object)->Size();
}

v8::MaybeLocal<v8::Array> v8::Object::PreviewEntries(bool* is_key_value) {
  if (IsMap()) {
    *is_key_value = true;
    return Map::Cast(this)->AsArray();
  }
  if (IsSet()) {
    *is_key_value = false;
    return Set::Cast(this)->AsArray();
  }

  i::Handle<i::JSReceiver> object = Utils::OpenHandle(this);
  i::Isolate* isolate = object->GetIsolate();
  Isolate* v8_isolate = reinterpret_cast<Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (object->IsJSWeakCollection()) {
    *is_key_value = object->IsJSWeakMap();
    return Utils::ToLocal(i::JSWeakCollection::GetEntries(
        i::Handle<i::JSWeakCollection>::cast(object), 0));
  }
  if (object->IsJSMapIterator()) {
    i::Handle<i::JSMapIterator> it = i::Handle<i::JSMapIterator>::cast(object);
    MapAsArrayKind const kind =
        static_cast<MapAsArrayKind>(it->map().instance_type());
    *is_key_value = kind == MapAsArrayKind::kEntries;
    if (!it->HasMore()) return v8::Array::New(v8_isolate);
    return Utils::ToLocal(
        MapAsArray(isolate, it->table(), i::Smi::ToInt(it->index()), kind));
  }
  if (object->IsJSSetIterator()) {
    i::Handle<i::JSSetIterator> it = i::Handle<i::JSSetIterator>::cast(object);
    SetAsArrayKind const kind =
        static_cast<SetAsArrayKind>(it->map().instance_type());
    *is_key_value = kind == SetAsArrayKind::kEntries;
    if (!it->HasMore()) return v8::Array::New(v8_isolate);
    return Utils::ToLocal(
        SetAsArray(isolate, it->table(), i::Smi::ToInt(it->index()), kind));
  }
  return v8::MaybeLocal<v8::Array>();
}

Local<Function> debug::GetBuiltin(Isolate* v8_isolate, Builtin builtin) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope handle_scope(isolate);
  i::Builtins::Name builtin_id;
  switch (builtin) {
    case kStringToLowerCase:
      builtin_id = i::Builtins::kStringPrototypeToLocaleLowerCase;
      break;
    default:
      UNREACHABLE();
  }

  i::Handle<i::String> name = isolate->factory()->empty_string();
  i::NewFunctionArgs args = i::NewFunctionArgs::ForBuiltinWithoutPrototype(
      name, builtin_id, i::LanguageMode::kStrict);
  i::Handle<i::JSFunction> fun = isolate->factory()->NewFunction(args);

  fun->shared().set_internal_formal_parameter_count(0);
  fun->shared().set_length(0);
  return Utils::ToLocal(handle_scope.CloseAndEscape(fun));
}

void debug::SetConsoleDelegate(Isolate* v8_isolate, ConsoleDelegate* delegate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  isolate->set_console_delegate(delegate);
}

debug::ConsoleCallArguments::ConsoleCallArguments(
    const v8::FunctionCallbackInfo<v8::Value>& info)
    : v8::FunctionCallbackInfo<v8::Value>(nullptr, info.values_, info.length_) {
}

debug::ConsoleCallArguments::ConsoleCallArguments(
    const internal::BuiltinArguments& args)
    : v8::FunctionCallbackInfo<v8::Value>(
          nullptr,
          // Drop the first argument (receiver, i.e. the "console" object).
          args.address_of_arg_at(args.length() > 1 ? 1 : 0),
          args.length() - 1) {}

int debug::GetStackFrameId(v8::Local<v8::StackFrame> frame) {
  return Utils::OpenHandle(*frame)->id();
}

v8::Local<v8::StackTrace> debug::GetDetailedStackTrace(
    Isolate* v8_isolate, v8::Local<v8::Object> v8_error) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::JSReceiver> error = Utils::OpenHandle(*v8_error);
  if (!error->IsJSObject()) {
    return v8::Local<v8::StackTrace>();
  }
  i::Handle<i::FixedArray> stack_trace =
      isolate->GetDetailedStackTrace(i::Handle<i::JSObject>::cast(error));
  return Utils::StackTraceToLocal(stack_trace);
}

MaybeLocal<debug::Script> debug::GeneratorObject::Script() {
  i::Handle<i::JSGeneratorObject> obj = Utils::OpenHandle(this);
  i::Object maybe_script = obj->function().shared().script();
  if (!maybe_script.IsScript()) return MaybeLocal<debug::Script>();
  i::Handle<i::Script> script(i::Script::cast(maybe_script), obj->GetIsolate());
  return ToApiHandle<debug::Script>(script);
}

Local<Function> debug::GeneratorObject::Function() {
  i::Handle<i::JSGeneratorObject> obj = Utils::OpenHandle(this);
  return Utils::ToLocal(handle(obj->function(), obj->GetIsolate()));
}

debug::Location debug::GeneratorObject::SuspendedLocation() {
  i::Handle<i::JSGeneratorObject> obj = Utils::OpenHandle(this);
  CHECK(obj->is_suspended());
  i::Object maybe_script = obj->function().shared().script();
  if (!maybe_script.IsScript()) return debug::Location();
  i::Isolate* isolate = obj->GetIsolate();
  i::Handle<i::Script> script(i::Script::cast(maybe_script), isolate);
  i::Script::PositionInfo info;
  i::SharedFunctionInfo::EnsureSourcePositionsAvailable(
      isolate, i::handle(obj->function().shared(), isolate));
  i::Script::GetPositionInfo(script, obj->source_position(), &info,
                             i::Script::WITH_OFFSET);
  return debug::Location(info.line, info.column);
}

bool debug::GeneratorObject::IsSuspended() {
  return Utils::OpenHandle(this)->is_suspended();
}

v8::Local<debug::GeneratorObject> debug::GeneratorObject::Cast(
    v8::Local<v8::Value> value) {
  CHECK(value->IsGeneratorObject());
  return ToApiHandle<debug::GeneratorObject>(Utils::OpenHandle(*value));
}

MaybeLocal<v8::Value> debug::EvaluateGlobal(v8::Isolate* isolate,
                                            v8::Local<v8::String> source,
                                            EvaluateGlobalMode mode,
                                            bool repl) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE(internal_isolate, Value);
  i::REPLMode repl_mode = repl ? i::REPLMode::kYes : i::REPLMode::kNo;
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::DebugEvaluate::Global(internal_isolate, Utils::OpenHandle(*source),
                               mode, repl_mode),
      &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

void debug::QueryObjects(v8::Local<v8::Context> v8_context,
                         QueryObjectPredicate* predicate,
                         PersistentValueVector<v8::Object>* objects) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_context->GetIsolate());
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  isolate->heap_profiler()->QueryObjects(Utils::OpenHandle(*v8_context),
                                         predicate, objects);
}

void debug::GlobalLexicalScopeNames(
    v8::Local<v8::Context> v8_context,
    v8::PersistentValueVector<v8::String>* names) {
  i::Handle<i::Context> context = Utils::OpenHandle(*v8_context);
  i::Isolate* isolate = context->GetIsolate();
  i::Handle<i::ScriptContextTable> table(
      context->global_object().native_context().script_context_table(),
      isolate);
  for (int i = 0; i < table->used(); i++) {
    i::Handle<i::Context> context =
        i::ScriptContextTable::GetContext(isolate, table, i);
    DCHECK(context->IsScriptContext());
    i::Handle<i::ScopeInfo> scope_info(context->scope_info(), isolate);
    int local_count = scope_info->ContextLocalCount();
    for (int j = 0; j < local_count; ++j) {
      i::String name = scope_info->ContextLocalName(j);
      if (i::ScopeInfo::VariableIsSynthetic(name)) continue;
      names->Append(Utils::ToLocal(handle(name, isolate)));
    }
  }
}

void debug::SetReturnValue(v8::Isolate* v8_isolate,
                           v8::Local<v8::Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->debug()->set_return_value(*Utils::OpenHandle(*value));
}

int64_t debug::GetNextRandomInt64(v8::Isolate* v8_isolate) {
  return reinterpret_cast<i::Isolate*>(v8_isolate)
      ->random_number_generator()
      ->NextInt64();
}

int debug::GetDebuggingId(v8::Local<v8::Function> function) {
  i::Handle<i::JSReceiver> callable = v8::Utils::OpenHandle(*function);
  if (!callable->IsJSFunction()) return i::DebugInfo::kNoDebuggingId;
  i::Handle<i::JSFunction> func = i::Handle<i::JSFunction>::cast(callable);
  int id = func->GetIsolate()->debug()->GetFunctionDebuggingId(func);
  DCHECK_NE(i::DebugInfo::kNoDebuggingId, id);
  return id;
}

bool debug::SetFunctionBreakpoint(v8::Local<v8::Function> function,
                                  v8::Local<v8::String> condition,
                                  BreakpointId* id) {
  i::Handle<i::JSReceiver> callable = Utils::OpenHandle(*function);
  if (!callable->IsJSFunction()) return false;
  i::Handle<i::JSFunction> jsfunction =
      i::Handle<i::JSFunction>::cast(callable);
  i::Isolate* isolate = jsfunction->GetIsolate();
  i::Handle<i::String> condition_string =
      condition.IsEmpty() ? isolate->factory()->empty_string()
                          : Utils::OpenHandle(*condition);
  return isolate->debug()->SetBreakpointForFunction(
      handle(jsfunction->shared(), isolate), condition_string, id);
}

debug::PostponeInterruptsScope::PostponeInterruptsScope(v8::Isolate* isolate)
    : scope_(
          new i::PostponeInterruptsScope(reinterpret_cast<i::Isolate*>(isolate),
                                         i::StackGuard::API_INTERRUPT)) {}

debug::PostponeInterruptsScope::~PostponeInterruptsScope() = default;

Local<String> CpuProfileNode::GetFunctionName() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  i::Isolate* isolate = node->isolate();
  const i::CodeEntry* entry = node->entry();
  i::Handle<i::String> name =
      isolate->factory()->InternalizeUtf8String(entry->name());
  return ToApiHandle<String>(name);
}

int debug::Coverage::BlockData::StartOffset() const { return block_->start; }
int debug::Coverage::BlockData::EndOffset() const { return block_->end; }
uint32_t debug::Coverage::BlockData::Count() const { return block_->count; }

int debug::Coverage::FunctionData::StartOffset() const {
  return function_->start;
}
int debug::Coverage::FunctionData::EndOffset() const { return function_->end; }
uint32_t debug::Coverage::FunctionData::Count() const {
  return function_->count;
}

MaybeLocal<String> debug::Coverage::FunctionData::Name() const {
  return ToApiHandle<String>(function_->name);
}

size_t debug::Coverage::FunctionData::BlockCount() const {
  return function_->blocks.size();
}

bool debug::Coverage::FunctionData::HasBlockCoverage() const {
  return function_->has_block_coverage;
}

debug::Coverage::BlockData debug::Coverage::FunctionData::GetBlockData(
    size_t i) const {
  return BlockData(&function_->blocks.at(i), coverage_);
}

Local<debug::Script> debug::Coverage::ScriptData::GetScript() const {
  return ToApiHandle<debug::Script>(script_->script);
}

size_t debug::Coverage::ScriptData::FunctionCount() const {
  return script_->functions.size();
}

debug::Coverage::FunctionData debug::Coverage::ScriptData::GetFunctionData(
    size_t i) const {
  return FunctionData(&script_->functions.at(i), coverage_);
}

debug::Coverage::ScriptData::ScriptData(size_t index,
                                        std::shared_ptr<i::Coverage> coverage)
    : script_(&coverage->at(index)), coverage_(std::move(coverage)) {}

size_t debug::Coverage::ScriptCount() const { return coverage_->size(); }

debug::Coverage::ScriptData debug::Coverage::GetScriptData(size_t i) const {
  return ScriptData(i, coverage_);
}

debug::Coverage debug::Coverage::CollectPrecise(Isolate* isolate) {
  return Coverage(
      i::Coverage::CollectPrecise(reinterpret_cast<i::Isolate*>(isolate)));
}

debug::Coverage debug::Coverage::CollectBestEffort(Isolate* isolate) {
  return Coverage(
      i::Coverage::CollectBestEffort(reinterpret_cast<i::Isolate*>(isolate)));
}

void debug::Coverage::SelectMode(Isolate* isolate, debug::CoverageMode mode) {
  i::Coverage::SelectMode(reinterpret_cast<i::Isolate*>(isolate), mode);
}

int debug::TypeProfile::Entry::SourcePosition() const {
  return entry_->position;
}

std::vector<MaybeLocal<String>> debug::TypeProfile::Entry::Types() const {
  std::vector<MaybeLocal<String>> result;
  for (const internal::Handle<internal::String>& type : entry_->types) {
    result.emplace_back(ToApiHandle<String>(type));
  }
  return result;
}

debug::TypeProfile::ScriptData::ScriptData(
    size_t index, std::shared_ptr<i::TypeProfile> type_profile)
    : script_(&type_profile->at(index)),
      type_profile_(std::move(type_profile)) {}

Local<debug::Script> debug::TypeProfile::ScriptData::GetScript() const {
  return ToApiHandle<debug::Script>(script_->script);
}

std::vector<debug::TypeProfile::Entry> debug::TypeProfile::ScriptData::Entries()
    const {
  std::vector<debug::TypeProfile::Entry> result;
  for (const internal::TypeProfileEntry& entry : script_->entries) {
    result.push_back(debug::TypeProfile::Entry(&entry, type_profile_));
  }
  return result;
}

debug::TypeProfile debug::TypeProfile::Collect(Isolate* isolate) {
  return TypeProfile(
      i::TypeProfile::Collect(reinterpret_cast<i::Isolate*>(isolate)));
}

void debug::TypeProfile::SelectMode(Isolate* isolate,
                                    debug::TypeProfileMode mode) {
  i::TypeProfile::SelectMode(reinterpret_cast<i::Isolate*>(isolate), mode);
}

size_t debug::TypeProfile::ScriptCount() const { return type_profile_->size(); }

debug::TypeProfile::ScriptData debug::TypeProfile::GetScriptData(
    size_t i) const {
  return ScriptData(i, type_profile_);
}

v8::MaybeLocal<v8::Value> debug::WeakMap::Get(v8::Local<v8::Context> context,
                                              v8::Local<v8::Value> key) {
  PREPARE_FOR_EXECUTION(context, WeakMap, Get, Value);
  auto self = Utils::OpenHandle(this);
  Local<Value> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !ToLocal<Value>(i::Execution::CallBuiltin(isolate, isolate->weakmap_get(),
                                                self, arraysize(argv), argv),
                      &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

v8::MaybeLocal<debug::WeakMap> debug::WeakMap::Set(
    v8::Local<v8::Context> context, v8::Local<v8::Value> key,
    v8::Local<v8::Value> value) {
  PREPARE_FOR_EXECUTION(context, WeakMap, Set, WeakMap);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key),
                                 Utils::OpenHandle(*value)};
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->weakmap_set(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(WeakMap);
  RETURN_ESCAPED(Local<WeakMap>::Cast(Utils::ToLocal(result)));
}

Local<debug::WeakMap> debug::WeakMap::New(v8::Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, WeakMap, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSWeakMap> obj = i_isolate->factory()->NewJSWeakMap();
  return ToApiHandle<debug::WeakMap>(obj);
}

debug::WeakMap* debug::WeakMap::Cast(v8::Value* value) {
  return static_cast<debug::WeakMap*>(value);
}

Local<Value> debug::AccessorPair::getter() {
  i::Handle<i::AccessorPair> accessors = Utils::OpenHandle(this);
  i::Isolate* isolate = accessors->GetIsolate();
  i::Handle<i::Object> getter(accessors->getter(), isolate);
  return Utils::ToLocal(getter);
}

Local<Value> debug::AccessorPair::setter() {
  i::Handle<i::AccessorPair> accessors = Utils::OpenHandle(this);
  i::Isolate* isolate = accessors->GetIsolate();
  i::Handle<i::Object> setter(accessors->setter(), isolate);
  return Utils::ToLocal(setter);
}

bool debug::AccessorPair::IsAccessorPair(Local<Value> that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*that);
  return obj->IsAccessorPair();
}

MaybeLocal<Message> debug::GetMessageFromPromise(Local<Promise> p) {
  i::Handle<i::JSPromise> promise = Utils::OpenHandle(*p);
  i::Isolate* isolate = promise->GetIsolate();

  i::Handle<i::Symbol> key = isolate->factory()->promise_debug_message_symbol();
  i::Handle<i::Object> maybeMessage =
      i::JSReceiver::GetDataProperty(promise, key);

  if (!maybeMessage->IsJSMessageObject(isolate)) return MaybeLocal<Message>();
  return ToApiHandle<Message>(
      i::Handle<i::JSMessageObject>::cast(maybeMessage));
}

const char* CpuProfileNode::GetFunctionNameStr() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return node->entry()->name();
}

int CpuProfileNode::GetScriptId() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  const i::CodeEntry* entry = node->entry();
  return entry->script_id();
}

Local<String> CpuProfileNode::GetScriptResourceName() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  i::Isolate* isolate = node->isolate();
  return ToApiHandle<String>(isolate->factory()->InternalizeUtf8String(
      node->entry()->resource_name()));
}

const char* CpuProfileNode::GetScriptResourceNameStr() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return node->entry()->resource_name();
}

bool CpuProfileNode::IsScriptSharedCrossOrigin() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return node->entry()->is_shared_cross_origin();
}

int CpuProfileNode::GetLineNumber() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->line_number();
}

int CpuProfileNode::GetColumnNumber() const {
  return reinterpret_cast<const i::ProfileNode*>(this)
      ->entry()
      ->column_number();
}

unsigned int CpuProfileNode::GetHitLineCount() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return node->GetHitLineCount();
}

bool CpuProfileNode::GetLineTicks(LineTick* entries,
                                  unsigned int length) const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return node->GetLineTicks(entries, length);
}

const char* CpuProfileNode::GetBailoutReason() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return node->entry()->bailout_reason();
}

unsigned CpuProfileNode::GetHitCount() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->self_ticks();
}

unsigned CpuProfileNode::GetNodeId() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->id();
}

CpuProfileNode::SourceType CpuProfileNode::GetSourceType() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->source_type();
}

int CpuProfileNode::GetChildrenCount() const {
  return static_cast<int>(
      reinterpret_cast<const i::ProfileNode*>(this)->children()->size());
}

const CpuProfileNode* CpuProfileNode::GetChild(int index) const {
  const i::ProfileNode* child =
      reinterpret_cast<const i::ProfileNode*>(this)->children()->at(index);
  return reinterpret_cast<const CpuProfileNode*>(child);
}

const CpuProfileNode* CpuProfileNode::GetParent() const {
  const i::ProfileNode* parent =
      reinterpret_cast<const i::ProfileNode*>(this)->parent();
  return reinterpret_cast<const CpuProfileNode*>(parent);
}

const std::vector<CpuProfileDeoptInfo>& CpuProfileNode::GetDeoptInfos() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return node->deopt_infos();
}

void CpuProfile::Delete() {
  i::CpuProfile* profile = reinterpret_cast<i::CpuProfile*>(this);
  i::CpuProfiler* profiler = profile->cpu_profiler();
  DCHECK_NOT_NULL(profiler);
  profiler->DeleteProfile(profile);
}

Local<String> CpuProfile::GetTitle() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  i::Isolate* isolate = profile->top_down()->isolate();
  return ToApiHandle<String>(
      isolate->factory()->InternalizeUtf8String(profile->title()));
}

const CpuProfileNode* CpuProfile::GetTopDownRoot() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->top_down()->root());
}

const CpuProfileNode* CpuProfile::GetSample(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->sample(index).node);
}

int64_t CpuProfile::GetSampleTimestamp(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return profile->sample(index).timestamp.since_origin().InMicroseconds();
}

int64_t CpuProfile::GetStartTime() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return profile->start_time().since_origin().InMicroseconds();
}

int64_t CpuProfile::GetEndTime() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return profile->end_time().since_origin().InMicroseconds();
}

int CpuProfile::GetSamplesCount() const {
  return reinterpret_cast<const i::CpuProfile*>(this)->samples_count();
}

CpuProfiler* CpuProfiler::New(Isolate* isolate,
                              CpuProfilingNamingMode naming_mode,
                              CpuProfilingLoggingMode logging_mode) {
  return reinterpret_cast<CpuProfiler*>(new i::CpuProfiler(
      reinterpret_cast<i::Isolate*>(isolate), naming_mode, logging_mode));
}

CpuProfilingOptions::CpuProfilingOptions(CpuProfilingMode mode,
                                         unsigned max_samples,
                                         int sampling_interval_us,
                                         MaybeLocal<Context> filter_context)
    : mode_(mode),
      max_samples_(max_samples),
      sampling_interval_us_(sampling_interval_us) {
  if (!filter_context.IsEmpty()) {
    Local<Context> local_filter_context = filter_context.ToLocalChecked();
    filter_context_.Reset(local_filter_context->GetIsolate(),
                          local_filter_context);
  }
}

void* CpuProfilingOptions::raw_filter_context() const {
  return reinterpret_cast<void*>(
      i::Context::cast(*Utils::OpenPersistent(filter_context_))
          .native_context()
          .address());
}

void CpuProfiler::Dispose() { delete reinterpret_cast<i::CpuProfiler*>(this); }

// static
void CpuProfiler::CollectSample(Isolate* isolate) {
  i::CpuProfiler::CollectSample(reinterpret_cast<i::Isolate*>(isolate));
}

void CpuProfiler::SetSamplingInterval(int us) {
  DCHECK_GE(us, 0);
  return reinterpret_cast<i::CpuProfiler*>(this)->set_sampling_interval(
      base::TimeDelta::FromMicroseconds(us));
}

void CpuProfiler::SetUsePreciseSampling(bool use_precise_sampling) {
  reinterpret_cast<i::CpuProfiler*>(this)->set_use_precise_sampling(
      use_precise_sampling);
}

void CpuProfiler::StartProfiling(Local<String> title,
                                 CpuProfilingOptions options) {
  reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      *Utils::OpenHandle(*title), options);
}

void CpuProfiler::StartProfiling(Local<String> title, bool record_samples) {
  CpuProfilingOptions options(
      kLeafNodeLineNumbers,
      record_samples ? CpuProfilingOptions::kNoSampleLimit : 0);
  reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      *Utils::OpenHandle(*title), options);
}

void CpuProfiler::StartProfiling(Local<String> title, CpuProfilingMode mode,
                                 bool record_samples, unsigned max_samples) {
  CpuProfilingOptions options(mode, record_samples ? max_samples : 0);
  reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      *Utils::OpenHandle(*title), options);
}

CpuProfile* CpuProfiler::StopProfiling(Local<String> title) {
  return reinterpret_cast<CpuProfile*>(
      reinterpret_cast<i::CpuProfiler*>(this)->StopProfiling(
          *Utils::OpenHandle(*title)));
}

void CpuProfiler::UseDetailedSourcePositionsForProfiling(Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)
      ->set_detailed_source_positions_for_profiling(true);
}

uintptr_t CodeEvent::GetCodeStartAddress() {
  return reinterpret_cast<i::CodeEvent*>(this)->code_start_address;
}

size_t CodeEvent::GetCodeSize() {
  return reinterpret_cast<i::CodeEvent*>(this)->code_size;
}

Local<String> CodeEvent::GetFunctionName() {
  return ToApiHandle<String>(
      reinterpret_cast<i::CodeEvent*>(this)->function_name);
}

Local<String> CodeEvent::GetScriptName() {
  return ToApiHandle<String>(
      reinterpret_cast<i::CodeEvent*>(this)->script_name);
}

int CodeEvent::GetScriptLine() {
  return reinterpret_cast<i::CodeEvent*>(this)->script_line;
}

int CodeEvent::GetScriptColumn() {
  return reinterpret_cast<i::CodeEvent*>(this)->script_column;
}

CodeEventType CodeEvent::GetCodeType() {
  return reinterpret_cast<i::CodeEvent*>(this)->code_type;
}

const char* CodeEvent::GetComment() {
  return reinterpret_cast<i::CodeEvent*>(this)->comment;
}

uintptr_t CodeEvent::GetPreviousCodeStartAddress() {
  return reinterpret_cast<i::CodeEvent*>(this)->previous_code_start_address;
}

const char* CodeEvent::GetCodeEventTypeName(CodeEventType code_event_type) {
  switch (code_event_type) {
    case kUnknownType:
      return "Unknown";
#define V(Name)       \
  case k##Name##Type: \
    return #Name;
      CODE_EVENTS_LIST(V)
#undef V
  }
  // The execution should never pass here
  UNREACHABLE();
  // NOTE(mmarchini): Workaround to fix a compiler failure on GCC 4.9
  return "Unknown";
}

CodeEventHandler::CodeEventHandler(Isolate* isolate) {
  internal_listener_ =
      new i::ExternalCodeEventListener(reinterpret_cast<i::Isolate*>(isolate));
}

CodeEventHandler::~CodeEventHandler() {
  delete reinterpret_cast<i::ExternalCodeEventListener*>(internal_listener_);
}

void CodeEventHandler::Enable() {
  reinterpret_cast<i::ExternalCodeEventListener*>(internal_listener_)
      ->StartListening(this);
}

void CodeEventHandler::Disable() {
  reinterpret_cast<i::ExternalCodeEventListener*>(internal_listener_)
      ->StopListening();
}

static i::HeapGraphEdge* ToInternal(const HeapGraphEdge* edge) {
  return const_cast<i::HeapGraphEdge*>(
      reinterpret_cast<const i::HeapGraphEdge*>(edge));
}

HeapGraphEdge::Type HeapGraphEdge::GetType() const {
  return static_cast<HeapGraphEdge::Type>(ToInternal(this)->type());
}

Local<Value> HeapGraphEdge::GetName() const {
  i::HeapGraphEdge* edge = ToInternal(this);
  i::Isolate* isolate = edge->isolate();
  switch (edge->type()) {
    case i::HeapGraphEdge::kContextVariable:
    case i::HeapGraphEdge::kInternal:
    case i::HeapGraphEdge::kProperty:
    case i::HeapGraphEdge::kShortcut:
    case i::HeapGraphEdge::kWeak:
      return ToApiHandle<String>(
          isolate->factory()->InternalizeUtf8String(edge->name()));
    case i::HeapGraphEdge::kElement:
    case i::HeapGraphEdge::kHidden:
      return ToApiHandle<Number>(
          isolate->factory()->NewNumberFromInt(edge->index()));
    default:
      UNREACHABLE();
  }
  return v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
}

const HeapGraphNode* HeapGraphEdge::GetFromNode() const {
  const i::HeapEntry* from = ToInternal(this)->from();
  return reinterpret_cast<const HeapGraphNode*>(from);
}

const HeapGraphNode* HeapGraphEdge::GetToNode() const {
  const i::HeapEntry* to = ToInternal(this)->to();
  return reinterpret_cast<const HeapGraphNode*>(to);
}

static i::HeapEntry* ToInternal(const HeapGraphNode* entry) {
  return const_cast<i::HeapEntry*>(
      reinterpret_cast<const i::HeapEntry*>(entry));
}

HeapGraphNode::Type HeapGraphNode::GetType() const {
  return static_cast<HeapGraphNode::Type>(ToInternal(this)->type());
}

Local<String> HeapGraphNode::GetName() const {
  i::Isolate* isolate = ToInternal(this)->isolate();
  return ToApiHandle<String>(
      isolate->factory()->InternalizeUtf8String(ToInternal(this)->name()));
}

SnapshotObjectId HeapGraphNode::GetId() const { return ToInternal(this)->id(); }

size_t HeapGraphNode::GetShallowSize() const {
  return ToInternal(this)->self_size();
}

int HeapGraphNode::GetChildrenCount() const {
  return ToInternal(this)->children_count();
}

const HeapGraphEdge* HeapGraphNode::GetChild(int index) const {
  return reinterpret_cast<const HeapGraphEdge*>(ToInternal(this)->child(index));
}

static i::HeapSnapshot* ToInternal(const HeapSnapshot* snapshot) {
  return const_cast<i::HeapSnapshot*>(
      reinterpret_cast<const i::HeapSnapshot*>(snapshot));
}

void HeapSnapshot::Delete() {
  i::Isolate* isolate = ToInternal(this)->profiler()->isolate();
  if (isolate->heap_profiler()->GetSnapshotsCount() > 1) {
    ToInternal(this)->Delete();
  } else {
    // If this is the last snapshot, clean up all accessory data as well.
    isolate->heap_profiler()->DeleteAllSnapshots();
  }
}

const HeapGraphNode* HeapSnapshot::GetRoot() const {
  return reinterpret_cast<const HeapGraphNode*>(ToInternal(this)->root());
}

const HeapGraphNode* HeapSnapshot::GetNodeById(SnapshotObjectId id) const {
  return reinterpret_cast<const HeapGraphNode*>(
      ToInternal(this)->GetEntryById(id));
}

int HeapSnapshot::GetNodesCount() const {
  return static_cast<int>(ToInternal(this)->entries().size());
}

const HeapGraphNode* HeapSnapshot::GetNode(int index) const {
  return reinterpret_cast<const HeapGraphNode*>(
      &ToInternal(this)->entries().at(index));
}

SnapshotObjectId HeapSnapshot::GetMaxSnapshotJSObjectId() const {
  return ToInternal(this)->max_snapshot_js_object_id();
}

void HeapSnapshot::Serialize(OutputStream* stream,
                             HeapSnapshot::SerializationFormat format) const {
  Utils::ApiCheck(format == kJSON, "v8::HeapSnapshot::Serialize",
                  "Unknown serialization format");
  Utils::ApiCheck(stream->GetChunkSize() > 0, "v8::HeapSnapshot::Serialize",
                  "Invalid stream chunk size");
  i::HeapSnapshotJSONSerializer serializer(ToInternal(this));
  serializer.Serialize(stream);
}

// static
STATIC_CONST_MEMBER_DEFINITION const SnapshotObjectId
    HeapProfiler::kUnknownObjectId;

int HeapProfiler::GetSnapshotCount() {
  return reinterpret_cast<i::HeapProfiler*>(this)->GetSnapshotsCount();
}

const HeapSnapshot* HeapProfiler::GetHeapSnapshot(int index) {
  return reinterpret_cast<const HeapSnapshot*>(
      reinterpret_cast<i::HeapProfiler*>(this)->GetSnapshot(index));
}

SnapshotObjectId HeapProfiler::GetObjectId(Local<Value> value) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*value);
  return reinterpret_cast<i::HeapProfiler*>(this)->GetSnapshotObjectId(obj);
}

SnapshotObjectId HeapProfiler::GetObjectId(NativeObject value) {
  return reinterpret_cast<i::HeapProfiler*>(this)->GetSnapshotObjectId(value);
}

Local<Value> HeapProfiler::FindObjectById(SnapshotObjectId id) {
  i::Handle<i::Object> obj =
      reinterpret_cast<i::HeapProfiler*>(this)->FindHeapObjectById(id);
  if (obj.is_null()) return Local<Value>();
  return Utils::ToLocal(obj);
}

void HeapProfiler::ClearObjectIds() {
  reinterpret_cast<i::HeapProfiler*>(this)->ClearHeapObjectMap();
}

const HeapSnapshot* HeapProfiler::TakeHeapSnapshot(
    ActivityControl* control, ObjectNameResolver* resolver,
    bool treat_global_objects_as_roots) {
  return reinterpret_cast<const HeapSnapshot*>(
      reinterpret_cast<i::HeapProfiler*>(this)->TakeSnapshot(
          control, resolver, treat_global_objects_as_roots));
}

void HeapProfiler::StartTrackingHeapObjects(bool track_allocations) {
  reinterpret_cast<i::HeapProfiler*>(this)->StartHeapObjectsTracking(
      track_allocations);
}

void HeapProfiler::StopTrackingHeapObjects() {
  reinterpret_cast<i::HeapProfiler*>(this)->StopHeapObjectsTracking();
}

SnapshotObjectId HeapProfiler::GetHeapStats(OutputStream* stream,
                                            int64_t* timestamp_us) {
  i::HeapProfiler* heap_profiler = reinterpret_cast<i::HeapProfiler*>(this);
  return heap_profiler->PushHeapObjectsStats(stream, timestamp_us);
}

bool HeapProfiler::StartSamplingHeapProfiler(uint64_t sample_interval,
                                             int stack_depth,
                                             SamplingFlags flags) {
  return reinterpret_cast<i::HeapProfiler*>(this)->StartSamplingHeapProfiler(
      sample_interval, stack_depth, flags);
}

void HeapProfiler::StopSamplingHeapProfiler() {
  reinterpret_cast<i::HeapProfiler*>(this)->StopSamplingHeapProfiler();
}

AllocationProfile* HeapProfiler::GetAllocationProfile() {
  return reinterpret_cast<i::HeapProfiler*>(this)->GetAllocationProfile();
}

void HeapProfiler::DeleteAllHeapSnapshots() {
  reinterpret_cast<i::HeapProfiler*>(this)->DeleteAllSnapshots();
}

void HeapProfiler::AddBuildEmbedderGraphCallback(
    BuildEmbedderGraphCallback callback, void* data) {
  reinterpret_cast<i::HeapProfiler*>(this)->AddBuildEmbedderGraphCallback(
      callback, data);
}

void HeapProfiler::RemoveBuildEmbedderGraphCallback(
    BuildEmbedderGraphCallback callback, void* data) {
  reinterpret_cast<i::HeapProfiler*>(this)->RemoveBuildEmbedderGraphCallback(
      callback, data);
}

void EmbedderHeapTracer::SetStackStart(void* stack_start) {
  CHECK(isolate_);
  reinterpret_cast<i::Isolate*>(isolate_)->global_handles()->SetStackStart(
      stack_start);
}

void EmbedderHeapTracer::NotifyEmptyEmbedderStack() {
  CHECK(isolate_);
  reinterpret_cast<i::Isolate*>(isolate_)
      ->global_handles()
      ->NotifyEmptyEmbedderStack();
}

void EmbedderHeapTracer::FinalizeTracing() {
  if (isolate_) {
    i::Isolate* isolate = reinterpret_cast<i::Isolate*>(isolate_);
    if (isolate->heap()->incremental_marking()->IsMarking()) {
      isolate->heap()->FinalizeIncrementalMarkingAtomically(
          i::GarbageCollectionReason::kExternalFinalize);
    }
  }
}

void EmbedderHeapTracer::GarbageCollectionForTesting(
    EmbedderStackState stack_state) {
  CHECK(isolate_);
  CHECK(i::FLAG_expose_gc);
  i::Heap* const heap = reinterpret_cast<i::Isolate*>(isolate_)->heap();
  heap->SetEmbedderStackStateForNextFinalizaton(stack_state);
  heap->PreciseCollectAllGarbage(i::Heap::kNoGCFlags,
                                 i::GarbageCollectionReason::kTesting,
                                 kGCCallbackFlagForced);
}

void EmbedderHeapTracer::IncreaseAllocatedSize(size_t bytes) {
  if (isolate_) {
    i::LocalEmbedderHeapTracer* const tracer =
        reinterpret_cast<i::Isolate*>(isolate_)
            ->heap()
            ->local_embedder_heap_tracer();
    DCHECK_NOT_NULL(tracer);
    tracer->IncreaseAllocatedSize(bytes);
  }
}

void EmbedderHeapTracer::DecreaseAllocatedSize(size_t bytes) {
  if (isolate_) {
    i::LocalEmbedderHeapTracer* const tracer =
        reinterpret_cast<i::Isolate*>(isolate_)
            ->heap()
            ->local_embedder_heap_tracer();
    DCHECK_NOT_NULL(tracer);
    tracer->DecreaseAllocatedSize(bytes);
  }
}

void EmbedderHeapTracer::RegisterEmbedderReference(
    const TracedReferenceBase<v8::Data>& ref) {
  if (ref.IsEmpty()) return;

  i::Heap* const heap = reinterpret_cast<i::Isolate*>(isolate_)->heap();
  heap->RegisterExternallyReferencedObject(
      reinterpret_cast<i::Address*>(ref.val_));
}

void EmbedderHeapTracer::IterateTracedGlobalHandles(
    TracedGlobalHandleVisitor* visitor) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(isolate_);
  i::DisallowHeapAllocation no_allocation;
  isolate->global_handles()->IterateTracedNodes(visitor);
}

bool EmbedderHeapTracer::IsRootForNonTracingGC(
    const v8::TracedReference<v8::Value>& handle) {
  return true;
}

bool EmbedderHeapTracer::IsRootForNonTracingGC(
    const v8::TracedGlobal<v8::Value>& handle) {
  return true;
}

void EmbedderHeapTracer::ResetHandleInNonTracingGC(
    const v8::TracedReference<v8::Value>& handle) {
  UNREACHABLE();
}

namespace internal {

const size_t HandleScopeImplementer::kEnteredContextsOffset =
    offsetof(HandleScopeImplementer, entered_contexts_);
const size_t HandleScopeImplementer::kIsMicrotaskContextOffset =
    offsetof(HandleScopeImplementer, is_microtask_context_);

void HandleScopeImplementer::FreeThreadResources() { Free(); }

char* HandleScopeImplementer::ArchiveThread(char* storage) {
  HandleScopeData* current = isolate_->handle_scope_data();
  handle_scope_data_ = *current;
  MemCopy(storage, this, sizeof(*this));

  ResetAfterArchive();
  current->Initialize();

  return storage + ArchiveSpacePerThread();
}

int HandleScopeImplementer::ArchiveSpacePerThread() {
  return sizeof(HandleScopeImplementer);
}

char* HandleScopeImplementer::RestoreThread(char* storage) {
  MemCopy(this, storage, sizeof(*this));
  *isolate_->handle_scope_data() = handle_scope_data_;
  return storage + ArchiveSpacePerThread();
}

void HandleScopeImplementer::IterateThis(RootVisitor* v) {
#ifdef DEBUG
  bool found_block_before_deferred = false;
#endif
  // Iterate over all handles in the blocks except for the last.
  for (int i = static_cast<int>(blocks()->size()) - 2; i >= 0; --i) {
    Address* block = blocks()->at(i);
    // Cast possibly-unrelated pointers to plain Address before comparing them
    // to avoid undefined behavior.
    if (last_handle_before_deferred_block_ != nullptr &&
        (reinterpret_cast<Address>(last_handle_before_deferred_block_) <=
         reinterpret_cast<Address>(&block[kHandleBlockSize])) &&
        (reinterpret_cast<Address>(last_handle_before_deferred_block_) >=
         reinterpret_cast<Address>(block))) {
      v->VisitRootPointers(Root::kHandleScope, nullptr, FullObjectSlot(block),
                           FullObjectSlot(last_handle_before_deferred_block_));
      DCHECK(!found_block_before_deferred);
#ifdef DEBUG
      found_block_before_deferred = true;
#endif
    } else {
      v->VisitRootPointers(Root::kHandleScope, nullptr, FullObjectSlot(block),
                           FullObjectSlot(&block[kHandleBlockSize]));
    }
  }

  DCHECK(last_handle_before_deferred_block_ == nullptr ||
         found_block_before_deferred);

  // Iterate over live handles in the last block (if any).
  if (!blocks()->empty()) {
    v->VisitRootPointers(Root::kHandleScope, nullptr,
                         FullObjectSlot(blocks()->back()),
                         FullObjectSlot(handle_scope_data_.next));
  }

  DetachableVector<Context>* context_lists[2] = {&saved_contexts_,
                                                 &entered_contexts_};
  for (unsigned i = 0; i < arraysize(context_lists); i++) {
    context_lists[i]->shrink_to_fit();
    if (context_lists[i]->empty()) continue;
    FullObjectSlot start(&context_lists[i]->front());
    v->VisitRootPointers(Root::kHandleScope, nullptr, start,
                         start + static_cast<int>(context_lists[i]->size()));
  }
}

void HandleScopeImplementer::Iterate(RootVisitor* v) {
  HandleScopeData* current = isolate_->handle_scope_data();
  handle_scope_data_ = *current;
  IterateThis(v);
}

char* HandleScopeImplementer::Iterate(RootVisitor* v, char* storage) {
  HandleScopeImplementer* scope_implementer =
      reinterpret_cast<HandleScopeImplementer*>(storage);
  scope_implementer->IterateThis(v);
  return storage + ArchiveSpacePerThread();
}

std::unique_ptr<DeferredHandles> HandleScopeImplementer::Detach(
    Address* prev_limit) {
  std::unique_ptr<DeferredHandles> deferred(
      new DeferredHandles(isolate()->handle_scope_data()->next, isolate()));

  while (!blocks_.empty()) {
    Address* block_start = blocks_.back();
    Address* block_limit = &block_start[kHandleBlockSize];
    // We should not need to check for SealHandleScope here. Assert this.
    DCHECK(prev_limit == block_limit ||
           !(block_start <= prev_limit && prev_limit <= block_limit));
    if (prev_limit == block_limit) break;
    deferred->blocks_.push_back(blocks_.back());
    blocks_.pop_back();
  }

  // deferred->blocks_ now contains the blocks installed on the
  // HandleScope stack since BeginDeferredScope was called, but in
  // reverse order.

  DCHECK(prev_limit == nullptr || !blocks_.empty());

  DCHECK(!blocks_.empty() && prev_limit != nullptr);
  DCHECK_NOT_NULL(last_handle_before_deferred_block_);
  last_handle_before_deferred_block_ = nullptr;
  return deferred;
}

void HandleScopeImplementer::BeginDeferredScope() {
  DCHECK_NULL(last_handle_before_deferred_block_);
  last_handle_before_deferred_block_ = isolate()->handle_scope_data()->next;
}

DeferredHandles::~DeferredHandles() {
  isolate_->UnlinkDeferredHandles(this);

  for (size_t i = 0; i < blocks_.size(); i++) {
#ifdef ENABLE_HANDLE_ZAPPING
    HandleScope::ZapRange(blocks_[i], &blocks_[i][kHandleBlockSize]);
#endif
    isolate_->handle_scope_implementer()->ReturnBlock(blocks_[i]);
  }
}

void DeferredHandles::Iterate(RootVisitor* v) {
  DCHECK(!blocks_.empty());

  // Comparing pointers that do not point into the same array is undefined
  // behavior, which means if we didn't cast everything to plain Address
  // before comparing, the compiler would be allowed to assume that all
  // comparisons evaluate to true and drop the entire check.
  DCHECK((reinterpret_cast<Address>(first_block_limit_) >=
          reinterpret_cast<Address>(blocks_.front())) &&
         (reinterpret_cast<Address>(first_block_limit_) <=
          reinterpret_cast<Address>(&(blocks_.front())[kHandleBlockSize])));

  v->VisitRootPointers(Root::kHandleScope, nullptr,
                       FullObjectSlot(blocks_.front()),
                       FullObjectSlot(first_block_limit_));

  for (size_t i = 1; i < blocks_.size(); i++) {
    v->VisitRootPointers(Root::kHandleScope, nullptr,
                         FullObjectSlot(blocks_[i]),
                         FullObjectSlot(&blocks_[i][kHandleBlockSize]));
  }
}

void InvokeAccessorGetterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    v8::AccessorNameGetterCallback getter) {
  // Leaving JavaScript.
  Isolate* isolate = reinterpret_cast<Isolate*>(info.GetIsolate());
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kAccessorGetterCallback);
  Address getter_address = reinterpret_cast<Address>(getter);
  VMState<EXTERNAL> state(isolate);
  ExternalCallbackScope call_scope(isolate, getter_address);
  getter(property, info);
}

void InvokeFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                            v8::FunctionCallback callback) {
  Isolate* isolate = reinterpret_cast<Isolate*>(info.GetIsolate());
  RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::kFunctionCallback);
  Address callback_address = reinterpret_cast<Address>(callback);
  VMState<EXTERNAL> state(isolate);
  ExternalCallbackScope call_scope(isolate, callback_address);
  callback(info);
}

void InvokeFinalizationGroupCleanupFromTask(
    Handle<Context> context, Handle<JSFinalizationGroup> finalization_group,
    Handle<Object> callback) {
  Isolate* isolate = finalization_group->native_context().GetIsolate();
  RuntimeCallTimerScope timer(
      isolate, RuntimeCallCounterId::kFinalizationGroupCleanupFromTask);
  // Do not use ENTER_V8 because this is always called from a running
  // FinalizationGroupCleanupTask within V8 and we should not log it as an API
  // call. This method is implemented here to avoid duplication of the exception
  // handling and microtask running logic in CallDepthScope.
  if (IsExecutionTerminatingCheck(isolate)) return;
  Local<v8::Context> api_context = Utils::ToLocal(context);
  CallDepthScope<true> call_depth_scope(isolate, api_context);
  VMState<OTHER> state(isolate);
  if (JSFinalizationGroup::Cleanup(isolate, finalization_group, callback)
          .IsNothing()) {
    call_depth_scope.Escape();
  }
}

// Undefine macros for jumbo build.
#undef LOG_API
#undef ENTER_V8_DO_NOT_USE
#undef ENTER_V8_HELPER_DO_NOT_USE
#undef PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE
#undef PREPARE_FOR_EXECUTION_WITH_CONTEXT
#undef PREPARE_FOR_EXECUTION
#undef ENTER_V8
#undef ENTER_V8_NO_SCRIPT
#undef ENTER_V8_NO_SCRIPT_NO_EXCEPTION
#undef ENTER_V8_FOR_NEW_CONTEXT
#undef EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE
#undef RETURN_ON_FAILED_EXECUTION
#undef RETURN_ON_FAILED_EXECUTION_PRIMITIVE
#undef RETURN_ESCAPED
#undef SET_FIELD_WRAPPED
#undef NEW_STRING
#undef CALLBACK_SETTER

}  // namespace internal
}  // namespace v8

#undef TRACE_BS
