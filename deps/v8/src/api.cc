// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"

#include <string.h>  // For memcpy, strlen.
#ifdef V8_USE_ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif  // V8_USE_ADDRESS_SANITIZER
#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif            // defined(LEAK_SANITIZER)
#include <cmath>  // For isnan.
#include <limits>
#include <vector>
#include "include/v8-debug.h"
#include "include/v8-profiler.h"
#include "include/v8-testing.h"
#include "include/v8-util.h"
#include "src/accessors.h"
#include "src/api-natives.h"
#include "src/assert-scope.h"
#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/safe_conversions.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/builtins/builtins-utils.h"
#include "src/char-predicates-inl.h"
#include "src/code-stubs.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/compiler.h"
#include "src/contexts.h"
#include "src/conversions-inl.h"
#include "src/counters.h"
#include "src/debug/debug-coverage.h"
#include "src/debug/debug-type-profile.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/detachable-vector.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/gdb-jit.h"
#include "src/global-handles.h"
#include "src/globals.h"
#include "src/icu_util.h"
#include "src/isolate-inl.h"
#include "src/json-parser.h"
#include "src/json-stringifier.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/parsing/background-parsing-task.h"
#include "src/parsing/parser.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/pending-compilation-error-handler.h"
#include "src/profiler/cpu-profiler.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator-inl.h"
#include "src/profiler/profile-generator-inl.h"
#include "src/profiler/tick-sample.h"
#include "src/property-descriptor.h"
#include "src/property-details.h"
#include "src/property.h"
#include "src/prototype.h"
#include "src/runtime-profiler.h"
#include "src/runtime/runtime.h"
#include "src/simulator.h"
#include "src/snapshot/builtin-serializer.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"
#include "src/startup-data-util.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/unicode-cache-inl.h"
#include "src/unicode-inl.h"
#include "src/v8.h"
#include "src/v8threads.h"
#include "src/value-serializer.h"
#include "src/version.h"
#include "src/vm-state-inl.h"
#include "src/wasm/compilation-manager.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-serialization.h"

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

#define LOG_API(isolate, class_name, function_name)                       \
  i::RuntimeCallTimerScope _runtime_timer(                                \
      isolate, &i::RuntimeCallStats::API_##class_name##_##function_name); \
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

#define RETURN_TO_LOCAL_UNCHECKED(maybe_local, T) \
  return maybe_local.FromMaybe(Local<T>());


#define RETURN_ESCAPED(value) return handle_scope.Escape(value);


namespace {

Local<Context> ContextFromHeapObject(i::Handle<i::Object> obj) {
  return reinterpret_cast<v8::Isolate*>(i::HeapObject::cast(*obj)->GetIsolate())
      ->GetCurrentContext();
}

class InternalEscapableScope : public v8::EscapableHandleScope {
 public:
  explicit inline InternalEscapableScope(i::Isolate* isolate)
      : v8::EscapableHandleScope(reinterpret_cast<v8::Isolate*>(isolate)) {}
};

// TODO(jochen): This should be #ifdef DEBUG
#ifdef V8_CHECK_MICROTASKS_SCOPES_CONSISTENCY
void CheckMicrotasksScopesConsistency(i::Isolate* isolate) {
  auto handle_scope_implementer = isolate->handle_scope_implementer();
  if (handle_scope_implementer->microtasks_policy() ==
      v8::MicrotasksPolicy::kScoped) {
    DCHECK(handle_scope_implementer->GetMicrotasksScopeDepth() ||
           !handle_scope_implementer->DebugMicrotasksScopeDepthIsZero());
  }
}
#endif

template <bool do_callback>
class CallDepthScope {
 public:
  explicit CallDepthScope(i::Isolate* isolate, Local<Context> context)
      : isolate_(isolate), context_(context), escaped_(false) {
    // TODO(dcarney): remove this when blink stops crashing.
    DCHECK(!isolate_->external_caught_exception());
    isolate_->handle_scope_implementer()->IncrementCallDepth();
    if (!context.IsEmpty()) {
      i::Handle<i::Context> env = Utils::OpenHandle(*context);
      i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
      if (isolate->context() != nullptr &&
          isolate->context()->native_context() == env->native_context()) {
        context_ = Local<Context>();
      } else {
        impl->SaveContext(isolate->context());
        isolate->set_context(*env);
      }
    }
    if (do_callback) isolate_->FireBeforeCallEnteredCallback();
  }
  ~CallDepthScope() {
    if (!context_.IsEmpty()) {
      i::HandleScopeImplementer* impl = isolate_->handle_scope_implementer();
      isolate_->set_context(impl->RestoreContext());
    }
    if (!escaped_) isolate_->handle_scope_implementer()->DecrementCallDepth();
    if (do_callback) isolate_->FireCallCompletedCallback();
// TODO(jochen): This should be #ifdef DEBUG
#ifdef V8_CHECK_MICROTASKS_SCOPES_CONSISTENCY
    if (do_callback) CheckMicrotasksScopesConsistency(isolate_);
#endif
  }

  void Escape() {
    DCHECK(!escaped_);
    escaped_ = true;
    auto handle_scope_implementer = isolate_->handle_scope_implementer();
    handle_scope_implementer->DecrementCallDepth();
    bool call_depth_is_zero = handle_scope_implementer->CallDepthIsZero();
    isolate_->OptionalRescheduleException(call_depth_is_zero);
  }

 private:
  i::Isolate* const isolate_;
  Local<Context> context_;
  bool escaped_;
  bool do_callback_;
};

}  // namespace


static ScriptOrigin GetScriptOriginForScript(i::Isolate* isolate,
                                             i::Handle<i::Script> script) {
  i::Handle<i::Object> scriptName(script->GetNameOrSourceURL(), isolate);
  i::Handle<i::Object> source_map_url(script->source_mapping_url(), isolate);
  i::Handle<i::FixedArray> host_defined_options(script->host_defined_options(),
                                                isolate);
  v8::Isolate* v8_isolate =
      reinterpret_cast<v8::Isolate*>(script->GetIsolate());
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


void i::FatalProcessOutOfMemory(const char* location) {
  i::V8::FatalProcessOutOfMemory(location, false);
}

// When V8 cannot allocate memory FatalProcessOutOfMemory is called. The default
// OOM error handler is called and execution is stopped.
void i::V8::FatalProcessOutOfMemory(const char* location, bool is_heap_oom) {
  i::Isolate* isolate = i::Isolate::Current();
  char last_few_messages[Heap::kTraceRingBufferSize + 1];
  char js_stacktrace[Heap::kStacktraceBufferSize + 1];
  i::HeapStats heap_stats;

  if (isolate == nullptr) {
    // On a background thread -> we cannot retrieve memory information from the
    // Isolate. Write easy-to-recognize values on the stack.
    memset(last_few_messages, 0x0badc0de, Heap::kTraceRingBufferSize + 1);
    memset(js_stacktrace, 0x0badc0de, Heap::kStacktraceBufferSize + 1);
    memset(&heap_stats, 0xbadc0de, sizeof(heap_stats));
    // Note that the embedder's oom handler won't be called in this case. We
    // just crash.
    FATAL("API fatal error handler returned after process out of memory");
    return;
  }

  memset(last_few_messages, 0, Heap::kTraceRingBufferSize + 1);
  memset(js_stacktrace, 0, Heap::kStacktraceBufferSize + 1);

  intptr_t start_marker;
  heap_stats.start_marker = &start_marker;
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
    // HeapIterator here without doing a special GC.
    isolate->heap()->RecordStats(&heap_stats, false);
    char* first_newline = strchr(last_few_messages, '\n');
    if (first_newline == nullptr || first_newline[1] == '\0')
      first_newline = last_few_messages;
    PrintF("\n<--- Last few GCs --->\n%s\n", first_newline);
    PrintF("\n<--- JS stacktrace --->\n%s\n", js_stacktrace);
  }
  Utils::ReportOOMFailure(location, is_heap_oom);
  // If the fatal error handler returns, we stop execution.
  FATAL("API fatal error handler returned after process out of memory");
}


void Utils::ReportApiFailure(const char* location, const char* message) {
  i::Isolate* isolate = i::Isolate::Current();
  FatalErrorCallback callback = isolate->exception_behavior();
  if (callback == nullptr) {
    base::OS::PrintError("\n#\n# Fatal error in %s\n# %s\n#\n\n", location,
                         message);
    base::OS::Abort();
  } else {
    callback(location, message);
  }
  isolate->SignalFatalError();
}

void Utils::ReportOOMFailure(const char* location, bool is_heap_oom) {
  i::Isolate* isolate = i::Isolate::Current();
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
        isolate->heap()->termination_exception();
  }
  return false;
}


void V8::SetNativesDataBlob(StartupData* natives_blob) {
  i::V8::SetNativesBlob(natives_blob);
}


void V8::SetSnapshotDataBlob(StartupData* snapshot_blob) {
  i::V8::SetSnapshotBlob(snapshot_blob);
}

void* v8::ArrayBuffer::Allocator::Reserve(size_t length) { UNIMPLEMENTED(); }

void v8::ArrayBuffer::Allocator::Free(void* data, size_t length,
                                      AllocationMode mode) {
  switch (mode) {
    case AllocationMode::kNormal: {
      Free(data, length);
      return;
    }
    case AllocationMode::kReservation: {
      UNIMPLEMENTED();
      return;
    }
  }
}

void v8::ArrayBuffer::Allocator::SetProtection(
    void* data, size_t length,
    v8::ArrayBuffer::Allocator::Protection protection) {
  UNIMPLEMENTED();
}

namespace {

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == nullptr ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }

  virtual void* Reserve(size_t length) {
    size_t page_size = base::OS::AllocatePageSize();
    size_t allocated = RoundUp(length, page_size);
    void* address =
        base::OS::Allocate(base::OS::GetRandomMmapAddr(), allocated, page_size,
                           base::OS::MemoryPermission::kNoAccess);
#if defined(LEAK_SANITIZER)
    if (address != nullptr) {
      __lsan_register_root_region(address, allocated);
    }
#endif
    return address;
  }

  virtual void Free(void* data, size_t length,
                    v8::ArrayBuffer::Allocator::AllocationMode mode) {
    switch (mode) {
      case v8::ArrayBuffer::Allocator::AllocationMode::kNormal: {
        return Free(data, length);
      }
      case v8::ArrayBuffer::Allocator::AllocationMode::kReservation: {
        CHECK(base::OS::Free(data, length));
        return;
      }
    }
  }

  virtual void SetProtection(
      void* data, size_t length,
      v8::ArrayBuffer::Allocator::Protection protection) {
    DCHECK(protection == v8::ArrayBuffer::Allocator::Protection::kNoAccess ||
           protection == v8::ArrayBuffer::Allocator::Protection::kReadWrite);
    base::OS::MemoryPermission permission =
        (protection == v8::ArrayBuffer::Allocator::Protection::kReadWrite)
            ? base::OS::MemoryPermission::kReadWrite
            : base::OS::MemoryPermission::kNoAccess;
    CHECK(base::OS::SetPermissions(data, length, permission));
  }
};

bool RunExtraCode(Isolate* isolate, Local<Context> context,
                  const char* utf8_source, const char* name) {
  base::ElapsedTimer timer;
  timer.Start();
  Context::Scope context_scope(context);
  TryCatch try_catch(isolate);
  Local<String> source_string;
  if (!String::NewFromUtf8(isolate, utf8_source, NewStringType::kNormal)
           .ToLocal(&source_string)) {
    return false;
  }
  Local<String> resource_name =
      String::NewFromUtf8(isolate, name, NewStringType::kNormal)
          .ToLocalChecked();
  ScriptOrigin origin(resource_name);
  ScriptCompiler::Source source(source_string, origin);
  Local<Script> script;
  if (!ScriptCompiler::Compile(context, &source).ToLocal(&script)) return false;
  if (script->Run(context).IsEmpty()) return false;
  if (i::FLAG_profile_deserialization) {
    i::PrintF("Executing custom snapshot script %s took %0.3f ms\n", name,
              timer.Elapsed().InMillisecondsF());
  }
  timer.Stop();
  CHECK(!try_catch.HasCaught());
  return true;
}

struct SnapshotCreatorData {
  explicit SnapshotCreatorData(Isolate* isolate)
      : isolate_(isolate),
        default_context_(),
        contexts_(isolate),
        templates_(isolate),
        created_(false) {}

  static SnapshotCreatorData* cast(void* data) {
    return reinterpret_cast<SnapshotCreatorData*>(data);
  }

  ArrayBufferAllocator allocator_;
  Isolate* isolate_;
  Persistent<Context> default_context_;
  SerializeInternalFieldsCallback default_embedder_fields_serializer_;
  PersistentValueVector<Context> contexts_;
  PersistentValueVector<Template> templates_;
  std::vector<SerializeInternalFieldsCallback> embedder_fields_serializers_;
  bool created_;
};

}  // namespace

SnapshotCreator::SnapshotCreator(const intptr_t* external_references,
                                 StartupData* existing_snapshot) {
  i::Isolate* internal_isolate = new i::Isolate(true);
  Isolate* isolate = reinterpret_cast<Isolate*>(internal_isolate);
  SnapshotCreatorData* data = new SnapshotCreatorData(isolate);
  data->isolate_ = isolate;
  internal_isolate->set_array_buffer_allocator(&data->allocator_);
  internal_isolate->set_api_external_references(external_references);
  isolate->Enter();
  const StartupData* blob = existing_snapshot
                                ? existing_snapshot
                                : i::Snapshot::DefaultSnapshotBlob();
  if (blob && blob->raw_size > 0) {
    internal_isolate->set_snapshot_blob(blob);
    i::Snapshot::Initialize(internal_isolate);
  } else {
    internal_isolate->Init(nullptr);
  }
  data_ = data;
}

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
  size_t index = static_cast<int>(data->contexts_.Size());
  data->contexts_.Append(context);
  data->embedder_fields_serializers_.push_back(callback);
  return index;
}

size_t SnapshotCreator::AddTemplate(Local<Template> template_obj) {
  DCHECK(!template_obj.IsEmpty());
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  DCHECK(!data->created_);
  DCHECK_EQ(reinterpret_cast<i::Isolate*>(data->isolate_),
            Utils::OpenHandle(*template_obj)->GetIsolate());
  size_t index = static_cast<int>(data->templates_.Size());
  data->templates_.Append(template_obj);
  return index;
}

StartupData SnapshotCreator::CreateBlob(
    SnapshotCreator::FunctionCodeHandling function_code_handling) {
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(data->isolate_);
  DCHECK(!data->created_);
  DCHECK(!data->default_context_.IsEmpty());

  int num_additional_contexts = static_cast<int>(data->contexts_.Size());

  {
    int num_templates = static_cast<int>(data->templates_.Size());
    i::HandleScope scope(isolate);
    i::Handle<i::FixedArray> templates =
        isolate->factory()->NewFixedArray(num_templates, i::TENURED);
    for (int i = 0; i < num_templates; i++) {
      templates->set(i, *v8::Utils::OpenHandle(*data->templates_.Get(i)));
    }
    isolate->heap()->SetSerializedTemplates(*templates);
    data->templates_.Clear();

    // We need to store the global proxy size upfront in case we need the
    // bootstrapper to create a global proxy before we deserialize the context.
    i::Handle<i::FixedArray> global_proxy_sizes =
        isolate->factory()->NewFixedArray(num_additional_contexts, i::TENURED);
    for (int i = 0; i < num_additional_contexts; i++) {
      i::Handle<i::Context> context =
          v8::Utils::OpenHandle(*data->contexts_.Get(i));
      global_proxy_sizes->set(i,
                              i::Smi::FromInt(context->global_proxy()->Size()));
    }
    isolate->heap()->SetSerializedGlobalProxySizes(*global_proxy_sizes);
  }

  // We might rehash strings and re-sort descriptors. Clear the lookup cache.
  isolate->descriptor_lookup_cache()->Clear();

  // If we don't do this then we end up with a stray root pointing at the
  // context even after we have disposed of the context.
  isolate->heap()->CollectAllAvailableGarbage(
      i::GarbageCollectionReason::kSnapshotCreator);
  isolate->heap()->CompactWeakFixedArrays();

  i::DisallowHeapAllocation no_gc_from_here_on;

  std::vector<i::Object*> contexts;
  contexts.reserve(num_additional_contexts);
  i::Object* default_context;
  {
    i::HandleScope scope(isolate);
    default_context =
        *v8::Utils::OpenHandle(*data->default_context_.Get(data->isolate_));
    data->default_context_.Reset();
    for (int i = 0; i < num_additional_contexts; i++) {
      i::Handle<i::Context> context =
          v8::Utils::OpenHandle(*data->contexts_.Get(i));
      contexts.push_back(*context);
    }
    data->contexts_.Clear();
  }

  // Complete in-object slack tracking for all functions.
  i::HeapIterator heap_iterator(isolate->heap());
  while (i::HeapObject* current_obj = heap_iterator.next()) {
    if (!current_obj->IsJSFunction()) continue;
    i::JSFunction* fun = i::JSFunction::cast(current_obj);
    fun->CompleteInobjectSlackTrackingIfActive();
  }

  i::StartupSerializer startup_serializer(isolate, function_code_handling);
  startup_serializer.SerializeStrongReferences();

  // Serialize each context with a new partial serializer.
  std::vector<i::SnapshotData*> context_snapshots;
  context_snapshots.reserve(num_additional_contexts + 1);

  // TODO(6593): generalize rehashing, and remove this flag.
  bool can_be_rehashed = true;

  {
    // The default context is created with a handler for embedder fields which
    // determines how they are handled if encountered during serialization.
    i::PartialSerializer partial_serializer(
        isolate, &startup_serializer,
        data->default_embedder_fields_serializer_);
    partial_serializer.Serialize(&default_context, false);
    can_be_rehashed = can_be_rehashed && partial_serializer.can_be_rehashed();
    context_snapshots.push_back(new i::SnapshotData(&partial_serializer));
  }

  for (int i = 0; i < num_additional_contexts; i++) {
    i::PartialSerializer partial_serializer(
        isolate, &startup_serializer, data->embedder_fields_serializers_[i]);
    partial_serializer.Serialize(&contexts[i], true);
    can_be_rehashed = can_be_rehashed && partial_serializer.can_be_rehashed();
    context_snapshots.push_back(new i::SnapshotData(&partial_serializer));
  }

  // Builtin serialization places additional objects into the partial snapshot
  // cache and thus needs to happen before SerializeWeakReferencesAndDeferred
  // is called below.
  i::BuiltinSerializer builtin_serializer(isolate, &startup_serializer);
  builtin_serializer.SerializeBuiltinsAndHandlers();

  startup_serializer.SerializeWeakReferencesAndDeferred();
  can_be_rehashed = can_be_rehashed && startup_serializer.can_be_rehashed();

  i::SnapshotData startup_snapshot(&startup_serializer);
  i::BuiltinSnapshotData builtin_snapshot(&builtin_serializer);
  StartupData result = i::Snapshot::CreateSnapshotBlob(
      &startup_snapshot, &builtin_snapshot, context_snapshots, can_be_rehashed);

  // Delete heap-allocated context snapshot instances.
  for (const auto context_snapshot : context_snapshots) {
    delete context_snapshot;
  }
  data->created_ = true;
  return result;
}

StartupData V8::CreateSnapshotDataBlob(const char* embedded_source) {
  // Create a new isolate and a new context from scratch, optionally run
  // a script to embed, and serialize to create a snapshot blob.
  StartupData result = {nullptr, 0};
  base::ElapsedTimer timer;
  timer.Start();
  {
    SnapshotCreator snapshot_creator;
    Isolate* isolate = snapshot_creator.GetIsolate();
    {
      HandleScope scope(isolate);
      Local<Context> context = Context::New(isolate);
      if (embedded_source != nullptr &&
          !RunExtraCode(isolate, context, embedded_source, "<embedded>")) {
        return result;
      }
      snapshot_creator.SetDefaultContext(context);
    }
    result = snapshot_creator.CreateBlob(
        SnapshotCreator::FunctionCodeHandling::kClear);
  }

  if (i::FLAG_profile_deserialization) {
    i::PrintF("Creating snapshot took %0.3f ms\n",
              timer.Elapsed().InMillisecondsF());
  }
  timer.Stop();
  return result;
}

StartupData V8::WarmUpSnapshotDataBlob(StartupData cold_snapshot_blob,
                                       const char* warmup_source) {
  CHECK(cold_snapshot_blob.raw_size > 0 && cold_snapshot_blob.data != nullptr);
  CHECK_NOT_NULL(warmup_source);
  // Use following steps to create a warmed up snapshot blob from a cold one:
  //  - Create a new isolate from the cold snapshot.
  //  - Create a new context to run the warmup script. This will trigger
  //    compilation of executed functions.
  //  - Create a new context. This context will be unpolluted.
  //  - Serialize the isolate and the second context into a new snapshot blob.
  StartupData result = {nullptr, 0};
  base::ElapsedTimer timer;
  timer.Start();
  {
    SnapshotCreator snapshot_creator(nullptr, &cold_snapshot_blob);
    Isolate* isolate = snapshot_creator.GetIsolate();
    {
      HandleScope scope(isolate);
      Local<Context> context = Context::New(isolate);
      if (!RunExtraCode(isolate, context, warmup_source, "<warm-up>")) {
        return result;
      }
    }
    {
      HandleScope handle_scope(isolate);
      isolate->ContextDisposedNotification(false);
      Local<Context> context = Context::New(isolate);
      snapshot_creator.SetDefaultContext(context);
    }
    result = snapshot_creator.CreateBlob(
        SnapshotCreator::FunctionCodeHandling::kKeep);
  }

  if (i::FLAG_profile_deserialization) {
    i::PrintF("Warming up snapshot took %0.3f ms\n",
              timer.Elapsed().InMillisecondsF());
  }
  timer.Stop();
  return result;
}

void V8::SetDcheckErrorHandler(DcheckErrorCallback that) {
  v8::base::SetDcheckFunction(that);
}

void V8::SetFlagsFromString(const char* str, int length) {
  i::FlagList::SetFlagsFromString(str, length);
  i::FlagList::EnforceFlagImplications();
}


void V8::SetFlagsFromCommandLine(int* argc, char** argv, bool remove_flags) {
  i::FlagList::SetFlagsFromCommandLine(argc, argv, remove_flags);
}

RegisteredExtension* RegisteredExtension::first_extension_ = nullptr;

RegisteredExtension::RegisteredExtension(Extension* extension)
    : extension_(extension) { }


void RegisteredExtension::Register(RegisteredExtension* that) {
  that->next_ = first_extension_;
  first_extension_ = that;
}


void RegisteredExtension::UnregisterAll() {
  RegisteredExtension* re = first_extension_;
  while (re != nullptr) {
    RegisteredExtension* next = re->next();
    delete re;
    re = next;
  }
  first_extension_ = nullptr;
}


void RegisterExtension(Extension* that) {
  RegisteredExtension* extension = new RegisteredExtension(that);
  RegisteredExtension::Register(extension);
}


Extension::Extension(const char* name,
                     const char* source,
                     int dep_count,
                     const char** deps,
                     int source_length)
    : name_(name),
      source_length_(source_length >= 0 ?
                     source_length :
                     (source ? static_cast<int>(strlen(source)) : 0)),
      source_(source, source_length_),
      dep_count_(dep_count),
      deps_(deps),
      auto_enable_(false) {
  CHECK(source != nullptr || source_length_ == 0);
}

ResourceConstraints::ResourceConstraints()
    : max_semi_space_size_in_kb_(0),
      max_old_space_size_(0),
      stack_limit_(nullptr),
      code_range_size_(0),
      max_zone_pool_size_(0) {}

void ResourceConstraints::ConfigureDefaults(uint64_t physical_memory,
                                            uint64_t virtual_memory_limit) {
  set_max_semi_space_size_in_kb(
      i::Heap::ComputeMaxSemiSpaceSize(physical_memory));
  set_max_old_space_size(
      static_cast<int>(i::Heap::ComputeMaxOldGenerationSize(physical_memory)));
  set_max_zone_pool_size(i::AccountingAllocator::kMaxPoolSize);

  if (virtual_memory_limit > 0 && i::kRequiresCodeRange) {
    // Reserve no more than 1/8 of the memory for the code range, but at most
    // kMaximalCodeRangeSize.
    set_code_range_size(
        i::Min(i::kMaximalCodeRangeSize / i::MB,
               static_cast<size_t>((virtual_memory_limit >> 3) / i::MB)));
  }
}

void SetResourceConstraints(i::Isolate* isolate,
                            const ResourceConstraints& constraints) {
  size_t semi_space_size = constraints.max_semi_space_size_in_kb();
  int old_space_size = constraints.max_old_space_size();
  size_t code_range_size = constraints.code_range_size();
  size_t max_pool_size = constraints.max_zone_pool_size();
  if (semi_space_size != 0 || old_space_size != 0 || code_range_size != 0) {
    isolate->heap()->ConfigureHeap(semi_space_size, old_space_size,
                                   code_range_size);
  }
  isolate->allocator()->ConfigureSegmentPool(max_pool_size);

  if (constraints.stack_limit() != nullptr) {
    uintptr_t limit = reinterpret_cast<uintptr_t>(constraints.stack_limit());
    isolate->stack_guard()->SetStackLimit(limit);
  }
}


i::Object** V8::GlobalizeReference(i::Isolate* isolate, i::Object** obj) {
  LOG_API(isolate, Persistent, New);
  i::Handle<i::Object> result = isolate->global_handles()->Create(*obj);
#ifdef VERIFY_HEAP
  if (i::FLAG_verify_heap) {
    (*obj)->ObjectVerify();
  }
#endif  // VERIFY_HEAP
  return result.location();
}


i::Object** V8::CopyPersistent(i::Object** obj) {
  i::Handle<i::Object> result = i::GlobalHandles::CopyGlobal(obj);
#ifdef VERIFY_HEAP
  if (i::FLAG_verify_heap) {
    (*obj)->ObjectVerify();
  }
#endif  // VERIFY_HEAP
  return result.location();
}

void V8::RegisterExternallyReferencedObject(i::Object** object,
                                            i::Isolate* isolate) {
  isolate->heap()->RegisterExternallyReferencedObject(object);
}

void V8::MakeWeak(i::Object** location, void* parameter,
                  int embedder_field_index1, int embedder_field_index2,
                  WeakCallbackInfo<void>::Callback weak_callback) {
  WeakCallbackType type = WeakCallbackType::kParameter;
  if (embedder_field_index1 == 0) {
    if (embedder_field_index2 == 1) {
      type = WeakCallbackType::kInternalFields;
    } else {
      DCHECK_EQ(embedder_field_index2, -1);
      type = WeakCallbackType::kInternalFields;
    }
  } else {
    DCHECK_EQ(embedder_field_index1, -1);
    DCHECK_EQ(embedder_field_index2, -1);
  }
  i::GlobalHandles::MakeWeak(location, parameter, weak_callback, type);
}

void V8::MakeWeak(i::Object** location, void* parameter,
                  WeakCallbackInfo<void>::Callback weak_callback,
                  WeakCallbackType type) {
  i::GlobalHandles::MakeWeak(location, parameter, weak_callback, type);
}

void V8::MakeWeak(i::Object*** location_addr) {
  i::GlobalHandles::MakeWeak(location_addr);
}

void* V8::ClearWeak(i::Object** location) {
  return i::GlobalHandles::ClearWeakness(location);
}

void V8::DisposeGlobal(i::Object** location) {
  i::GlobalHandles::Destroy(location);
}

Value* V8::Eternalize(Isolate* v8_isolate, Value* value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Object* object = *Utils::OpenHandle(value);
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


HandleScope::HandleScope(Isolate* isolate) {
  Initialize(isolate);
}


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


i::Object** HandleScope::CreateHandle(i::Isolate* isolate, i::Object* value) {
  return i::HandleScope::CreateHandle(isolate, value);
}


i::Object** HandleScope::CreateHandle(i::HeapObject* heap_object,
                                      i::Object* value) {
  DCHECK(heap_object->IsHeapObject());
  return i::HandleScope::CreateHandle(heap_object->GetIsolate(), value);
}


EscapableHandleScope::EscapableHandleScope(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  escape_slot_ = CreateHandle(isolate, isolate->heap()->the_hole_value());
  Initialize(v8_isolate);
}


i::Object** EscapableHandleScope::Escape(i::Object** escape_value) {
  i::Heap* heap = reinterpret_cast<i::Isolate*>(GetIsolate())->heap();
  Utils::ApiCheck((*escape_slot_)->IsTheHole(heap->isolate()),
                  "EscapableHandleScope::Escape", "Escape value set twice");
  if (escape_value == nullptr) {
    *escape_slot_ = heap->undefined_value();
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
  impl->EnterContext(env);
  impl->SaveContext(isolate->context());
  isolate->set_context(*env);
}

void Context::Exit() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  if (!Utils::ApiCheck(impl->LastEnteredContextWas(env),
                       "v8::Context::Exit()",
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
  prev_ = isolate->top_backup_incumbent_scope();
  isolate->set_top_backup_incumbent_scope(this);
}

Context::BackupIncumbentScope::~BackupIncumbentScope() {
  i::Handle<i::Context> env = Utils::OpenHandle(*backup_incumbent_context_);
  i::Isolate* isolate = env->GetIsolate();
  isolate->set_top_backup_incumbent_scope(prev_);
}

static void* DecodeSmiToAligned(i::Object* value, const char* location) {
  Utils::ApiCheck(value->IsSmi(), location, "Not a Smi");
  return reinterpret_cast<void*>(value);
}


static i::Smi* EncodeAlignedAsSmi(void* value, const char* location) {
  i::Smi* smi = reinterpret_cast<i::Smi*>(value);
  Utils::ApiCheck(smi->IsSmi(), location, "Pointer is not aligned");
  return smi;
}


static i::Handle<i::FixedArray> EmbedderDataFor(Context* context,
                                                int index,
                                                bool can_grow,
                                                const char* location) {
  i::Handle<i::Context> env = Utils::OpenHandle(context);
  i::Isolate* isolate = env->GetIsolate();
  bool ok =
      Utils::ApiCheck(env->IsNativeContext(),
                      location,
                      "Not a native context") &&
      Utils::ApiCheck(index >= 0, location, "Negative index");
  if (!ok) return i::Handle<i::FixedArray>();
  i::Handle<i::FixedArray> data(env->embedder_data());
  if (index < data->length()) return data;
  if (!Utils::ApiCheck(can_grow, location, "Index too large")) {
    return i::Handle<i::FixedArray>();
  }
  int new_size = i::Max(index, data->length() << 1) + 1;
  int grow_by = new_size - data->length();
  data = isolate->factory()->CopyFixedArrayAndGrow(data, grow_by);
  env->set_embedder_data(*data);
  return data;
}


v8::Local<v8::Value> Context::SlowGetEmbedderData(int index) {
  const char* location = "v8::Context::GetEmbedderData()";
  i::Handle<i::FixedArray> data = EmbedderDataFor(this, index, false, location);
  if (data.is_null()) return Local<Value>();
  i::Handle<i::Object> result(data->get(index), data->GetIsolate());
  return Utils::ToLocal(result);
}


void Context::SetEmbedderData(int index, v8::Local<Value> value) {
  const char* location = "v8::Context::SetEmbedderData()";
  i::Handle<i::FixedArray> data = EmbedderDataFor(this, index, true, location);
  if (data.is_null()) return;
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  data->set(index, *val);
  DCHECK_EQ(*Utils::OpenHandle(*value),
            *Utils::OpenHandle(*GetEmbedderData(index)));
}


void* Context::SlowGetAlignedPointerFromEmbedderData(int index) {
  const char* location = "v8::Context::GetAlignedPointerFromEmbedderData()";
  i::Handle<i::FixedArray> data = EmbedderDataFor(this, index, false, location);
  if (data.is_null()) return nullptr;
  return DecodeSmiToAligned(data->get(index), location);
}


void Context::SetAlignedPointerInEmbedderData(int index, void* value) {
  const char* location = "v8::Context::SetAlignedPointerInEmbedderData()";
  i::Handle<i::FixedArray> data = EmbedderDataFor(this, index, true, location);
  data->set(index, EncodeAlignedAsSmi(value, location));
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
    templ->set_serial_number(i::Smi::kZero);
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

void Template::SetAccessorProperty(
    v8::Local<v8::Name> name,
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
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> result(Utils::OpenHandle(this)->prototype_template(),
                              i_isolate);
  if (result->IsUndefined(i_isolate)) {
    // Do not cache prototype objects.
    result = Utils::OpenHandle(
        *ObjectTemplateNew(i_isolate, Local<FunctionTemplate>(), true));
    Utils::OpenHandle(this)->set_prototype_template(*result);
  }
  return ToApiHandle<ObjectTemplate>(result);
}

void FunctionTemplate::SetPrototypeProviderTemplate(
    Local<FunctionTemplate> prototype_provider) {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> result = Utils::OpenHandle(*prototype_provider);
  auto info = Utils::OpenHandle(this);
  CHECK(info->prototype_template()->IsUndefined(i_isolate));
  CHECK(info->parent_template()->IsUndefined(i_isolate));
  info->set_prototype_provider_template(*result);
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
  CHECK(info->prototype_provider_template()->IsUndefined(i_isolate));
  info->set_parent_template(*Utils::OpenHandle(*value));
}

static Local<FunctionTemplate> FunctionTemplateNew(
    i::Isolate* isolate, FunctionCallback callback, v8::Local<Value> data,
    v8::Local<Signature> signature, int length, bool do_not_cache,
    v8::Local<Private> cached_property_name = v8::Local<Private>()) {
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::FUNCTION_TEMPLATE_INFO_TYPE, i::TENURED);
  i::Handle<i::FunctionTemplateInfo> obj =
      i::Handle<i::FunctionTemplateInfo>::cast(struct_obj);
  InitializeFunctionTemplate(obj);
  obj->set_do_not_cache(do_not_cache);
  int next_serial_number = i::FunctionTemplateInfo::kInvalidSerialNumber;
  if (!do_not_cache) {
    next_serial_number = isolate->heap()->GetNextTemplateSerialNumber();
  }
  obj->set_serial_number(i::Smi::FromInt(next_serial_number));
  if (callback != 0) {
    Utils::ToLocal(obj)->SetCallHandler(callback, data);
  }
  obj->set_length(length);
  obj->set_undetectable(false);
  obj->set_needs_access_check(false);
  obj->set_accept_any_receiver(true);
  if (!signature.IsEmpty()) {
    obj->set_signature(*Utils::OpenHandle(*signature));
  }
  obj->set_cached_property_name(
      cached_property_name.IsEmpty()
          ? isolate->heap()->the_hole_value()
          : *Utils::OpenHandle(*cached_property_name));
  return Utils::ToLocal(obj);
}

Local<FunctionTemplate> FunctionTemplate::New(
    Isolate* isolate, FunctionCallback callback, v8::Local<Value> data,
    v8::Local<Signature> signature, int length, ConstructorBehavior behavior) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  // Changes to the environment cannot be captured in the snapshot. Expect no
  // function templates when the isolate is created for serialization.
  LOG_API(i_isolate, FunctionTemplate, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  auto templ =
      FunctionTemplateNew(i_isolate, callback, data, signature, length, false);
  if (behavior == ConstructorBehavior::kThrow) templ->RemovePrototype();
  return templ;
}

MaybeLocal<FunctionTemplate> FunctionTemplate::FromSnapshot(Isolate* isolate,
                                                            size_t index) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::FixedArray* templates = i_isolate->heap()->serialized_templates();
  int int_index = static_cast<int>(index);
  if (int_index < templates->length()) {
    i::Object* info = templates->get(int_index);
    if (info->IsFunctionTemplateInfo()) {
      return Utils::ToLocal(i::Handle<i::FunctionTemplateInfo>(
          i::FunctionTemplateInfo::cast(info)));
    }
  }
  return Local<FunctionTemplate>();
}

Local<FunctionTemplate> FunctionTemplate::NewWithCache(
    Isolate* isolate, FunctionCallback callback, Local<Private> cache_property,
    Local<Value> data, Local<Signature> signature, int length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, FunctionTemplate, NewWithCache);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return FunctionTemplateNew(i_isolate, callback, data, signature, length,
                             false, cache_property);
}

Local<Signature> Signature::New(Isolate* isolate,
                                Local<FunctionTemplate> receiver) {
  return Utils::SignatureToLocal(Utils::OpenHandle(*receiver));
}


Local<AccessorSignature> AccessorSignature::New(
    Isolate* isolate, Local<FunctionTemplate> receiver) {
  return Utils::AccessorSignatureToLocal(Utils::OpenHandle(*receiver));
}


#define SET_FIELD_WRAPPED(obj, setter, cdata) do {                      \
    i::Handle<i::Object> foreign = FromCData(obj->GetIsolate(), cdata); \
    (obj)->setter(*foreign);                                            \
  } while (false)

void FunctionTemplate::SetCallHandler(FunctionCallback callback,
                                      v8::Local<Value> data) {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::SetCallHandler");
  i::Isolate* isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::TUPLE3_TYPE, i::TENURED);
  i::Handle<i::CallHandlerInfo> obj =
      i::Handle<i::CallHandlerInfo>::cast(struct_obj);
  SET_FIELD_WRAPPED(obj, set_callback, callback);
  SET_FIELD_WRAPPED(obj, set_js_callback, obj->redirected_callback());
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
  SET_FIELD_WRAPPED(obj, set_getter, getter);
  DCHECK_IMPLIES(replace_on_access,
                 is_special_data_property && setter == nullptr);
  if (is_special_data_property && setter == nullptr) {
    setter = reinterpret_cast<Setter>(&i::Accessors::ReconfigureToDataProperty);
  }
  SET_FIELD_WRAPPED(obj, set_setter, setter);
  i::Address redirected = obj->redirected_getter();
  if (redirected != nullptr) SET_FIELD_WRAPPED(obj, set_js_getter, redirected);
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
  if (handle->instance_template()->IsUndefined(isolate)) {
    Local<ObjectTemplate> templ =
        ObjectTemplate::New(isolate, ToApiHandle<FunctionTemplate>(handle));
    handle->set_instance_template(*Utils::OpenHandle(*templ));
  }
  i::Handle<i::ObjectTemplateInfo> result(
      i::ObjectTemplateInfo::cast(handle->instance_template()));
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


void FunctionTemplate::SetHiddenPrototype(bool value) {
  auto info = Utils::OpenHandle(this);
  EnsureNotInstantiated(info, "v8::FunctionTemplate::SetHiddenPrototype");
  auto isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  info->set_hidden_prototype(value);
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


Local<ObjectTemplate> ObjectTemplate::New() {
  return New(i::Isolate::Current(), Local<FunctionTemplate>());
}

static Local<ObjectTemplate> ObjectTemplateNew(
    i::Isolate* isolate, v8::Local<FunctionTemplate> constructor,
    bool do_not_cache) {
  LOG_API(isolate, ObjectTemplate, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::OBJECT_TEMPLATE_INFO_TYPE, i::TENURED);
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
  obj->set_data(i::Smi::kZero);
  return Utils::ToLocal(obj);
}

Local<ObjectTemplate> ObjectTemplate::New(
    i::Isolate* isolate, v8::Local<FunctionTemplate> constructor) {
  return ObjectTemplateNew(isolate, constructor, false);
}

MaybeLocal<ObjectTemplate> ObjectTemplate::FromSnapshot(Isolate* isolate,
                                                        size_t index) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::FixedArray* templates = i_isolate->heap()->serialized_templates();
  int int_index = static_cast<int>(index);
  if (int_index < templates->length()) {
    i::Object* info = templates->get(int_index);
    if (info->IsObjectTemplateInfo()) {
      return Utils::ToLocal(
          i::Handle<i::ObjectTemplateInfo>(i::ObjectTemplateInfo::cast(info)));
    }
  }
  return Local<ObjectTemplate>();
}

// Ensure that the object template has a constructor.  If no
// constructor is available we create one.
static i::Handle<i::FunctionTemplateInfo> EnsureConstructor(
    i::Isolate* isolate,
    ObjectTemplate* object_template) {
  i::Object* obj = Utils::OpenHandle(object_template)->constructor();
  if (!obj->IsUndefined(isolate)) {
    i::FunctionTemplateInfo* info = i::FunctionTemplateInfo::cast(obj);
    return i::Handle<i::FunctionTemplateInfo>(info, isolate);
  }
  Local<FunctionTemplate> templ =
      FunctionTemplate::New(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::FunctionTemplateInfo> constructor = Utils::OpenHandle(*templ);
  constructor->set_instance_template(*Utils::OpenHandle(object_template));
  Utils::OpenHandle(object_template)->set_constructor(*constructor);
  return constructor;
}

template <typename Getter, typename Setter, typename Data, typename Template>
static void TemplateSetAccessor(Template* template_obj, v8::Local<Name> name,
                                Getter getter, Setter setter, Data data,
                                AccessControl settings,
                                PropertyAttribute attribute,
                                v8::Local<AccessorSignature> signature,
                                bool is_special_data_property,
                                bool replace_on_access) {
  auto info = Utils::OpenHandle(template_obj);
  auto isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::AccessorInfo> accessor_info =
      MakeAccessorInfo(isolate, name, getter, setter, data, settings, signature,
                       is_special_data_property, replace_on_access);
  accessor_info->set_initial_property_attributes(
      static_cast<i::PropertyAttributes>(attribute));
  i::ApiNatives::AddNativeDataProperty(isolate, info, accessor_info);
}


void Template::SetNativeDataProperty(v8::Local<String> name,
                                     AccessorGetterCallback getter,
                                     AccessorSetterCallback setter,
                                     v8::Local<Value> data,
                                     PropertyAttribute attribute,
                                     v8::Local<AccessorSignature> signature,
                                     AccessControl settings) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      signature, true, false);
}


void Template::SetNativeDataProperty(v8::Local<Name> name,
                                     AccessorNameGetterCallback getter,
                                     AccessorNameSetterCallback setter,
                                     v8::Local<Value> data,
                                     PropertyAttribute attribute,
                                     v8::Local<AccessorSignature> signature,
                                     AccessControl settings) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      signature, true, false);
}

void Template::SetLazyDataProperty(v8::Local<Name> name,
                                   AccessorNameGetterCallback getter,
                                   v8::Local<Value> data,
                                   PropertyAttribute attribute) {
  TemplateSetAccessor(
      this, name, getter, static_cast<AccessorNameSetterCallback>(nullptr),
      data, DEFAULT, attribute, Local<AccessorSignature>(), true, true);
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
                                 v8::Local<AccessorSignature> signature) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      signature, i::FLAG_disable_old_api_accessors, false);
}


void ObjectTemplate::SetAccessor(v8::Local<Name> name,
                                 AccessorNameGetterCallback getter,
                                 AccessorNameSetterCallback setter,
                                 v8::Local<Value> data, AccessControl settings,
                                 PropertyAttribute attribute,
                                 v8::Local<AccessorSignature> signature) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      signature, i::FLAG_disable_old_api_accessors, false);
}

template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
static i::Handle<i::InterceptorInfo> CreateInterceptorInfo(
    i::Isolate* isolate, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  DCHECK(query == nullptr ||
         descriptor == nullptr);  // Either intercept attributes or descriptor.
  DCHECK(query == nullptr ||
         definer ==
             nullptr);  // Only use descriptor callback with definer callback.
  auto obj = i::Handle<i::InterceptorInfo>::cast(
      isolate->factory()->NewStruct(i::INTERCEPTOR_INFO_TYPE, i::TENURED));
  obj->set_flags(0);

  if (getter != 0) SET_FIELD_WRAPPED(obj, set_getter, getter);
  if (setter != 0) SET_FIELD_WRAPPED(obj, set_setter, setter);
  if (query != 0) SET_FIELD_WRAPPED(obj, set_query, query);
  if (descriptor != 0) SET_FIELD_WRAPPED(obj, set_descriptor, descriptor);
  if (remover != 0) SET_FIELD_WRAPPED(obj, set_deleter, remover);
  if (enumerator != 0) SET_FIELD_WRAPPED(obj, set_enumerator, enumerator);
  if (definer != 0) SET_FIELD_WRAPPED(obj, set_definer, definer);
  obj->set_can_intercept_symbols(
      !(static_cast<int>(flags) &
        static_cast<int>(PropertyHandlerFlags::kOnlyInterceptStrings)));
  obj->set_all_can_read(static_cast<int>(flags) &
                        static_cast<int>(PropertyHandlerFlags::kAllCanRead));
  obj->set_non_masking(static_cast<int>(flags) &
                       static_cast<int>(PropertyHandlerFlags::kNonMasking));

  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  return obj;
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
  auto obj = CreateInterceptorInfo(isolate, getter, setter, query, descriptor,
                                   remover, enumerator, definer, data, flags);
  cons->set_named_property_handler(*obj);
}

void ObjectTemplate::SetNamedPropertyHandler(
    NamedPropertyGetterCallback getter, NamedPropertySetterCallback setter,
    NamedPropertyQueryCallback query, NamedPropertyDeleterCallback remover,
    NamedPropertyEnumeratorCallback enumerator, Local<Value> data) {
  ObjectTemplateSetNamedPropertyHandler(
      this, getter, setter, query, nullptr, remover, enumerator, nullptr, data,
      PropertyHandlerFlags::kOnlyInterceptStrings);
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

  i::Handle<i::Struct> struct_info =
      isolate->factory()->NewStruct(i::ACCESS_CHECK_INFO_TYPE, i::TENURED);
  i::Handle<i::AccessCheckInfo> info =
      i::Handle<i::AccessCheckInfo>::cast(struct_info);

  SET_FIELD_WRAPPED(info, set_callback, callback);
  info->set_named_interceptor(nullptr);
  info->set_indexed_interceptor(nullptr);

  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  info->set_data(*Utils::OpenHandle(*data));

  cons->set_access_check_info(*info);
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

  i::Handle<i::Struct> struct_info =
      isolate->factory()->NewStruct(i::ACCESS_CHECK_INFO_TYPE, i::TENURED);
  i::Handle<i::AccessCheckInfo> info =
      i::Handle<i::AccessCheckInfo>::cast(struct_info);

  SET_FIELD_WRAPPED(info, set_callback, callback);
  auto named_interceptor = CreateInterceptorInfo(
      isolate, named_handler.getter, named_handler.setter, named_handler.query,
      named_handler.descriptor, named_handler.deleter, named_handler.enumerator,
      named_handler.definer, named_handler.data, named_handler.flags);
  info->set_named_interceptor(*named_interceptor);
  auto indexed_interceptor = CreateInterceptorInfo(
      isolate, indexed_handler.getter, indexed_handler.setter,
      indexed_handler.query, indexed_handler.descriptor,
      indexed_handler.deleter, indexed_handler.enumerator,
      indexed_handler.definer, indexed_handler.data, indexed_handler.flags);
  info->set_indexed_interceptor(*indexed_interceptor);

  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  info->set_data(*Utils::OpenHandle(*data));

  cons->set_access_check_info(*info);
  cons->set_needs_access_check(true);
}

void ObjectTemplate::SetHandler(
    const IndexedPropertyHandlerConfiguration& config) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto cons = EnsureConstructor(isolate, this);
  EnsureNotInstantiated(cons, "v8::ObjectTemplate::SetHandler");
  auto obj = CreateInterceptorInfo(isolate, config.getter, config.setter,
                                   config.query, config.descriptor,
                                   config.deleter, config.enumerator,
                                   config.definer, config.data, config.flags);
  cons->set_indexed_property_handler(*obj);
}


void ObjectTemplate::SetCallAsFunctionHandler(FunctionCallback callback,
                                              Local<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto cons = EnsureConstructor(isolate, this);
  EnsureNotInstantiated(cons, "v8::ObjectTemplate::SetCallAsFunctionHandler");
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::TUPLE3_TYPE, i::TENURED);
  i::Handle<i::CallHandlerInfo> obj =
      i::Handle<i::CallHandlerInfo>::cast(struct_obj);
  SET_FIELD_WRAPPED(obj, set_callback, callback);
  SET_FIELD_WRAPPED(obj, set_js_callback, obj->redirected_callback());
  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  cons->set_instance_call_handler(*obj);
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
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  Utils::OpenHandle(this)->set_immutable_proto(true);
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
    : impl_(new i::StreamedSource(stream, encoding)) {}


ScriptCompiler::StreamedSource::~StreamedSource() { delete impl_; }


const ScriptCompiler::CachedData*
ScriptCompiler::StreamedSource::GetCachedData() const {
  return impl_->cached_data.get();
}


Local<Script> UnboundScript::BindToCurrentContext() {
  i::Handle<i::HeapObject> obj =
      i::Handle<i::HeapObject>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = obj->GetIsolate();
  i::Handle<i::SharedFunctionInfo> function_info(
      i::SharedFunctionInfo::cast(*obj), isolate);
  i::Handle<i::JSFunction> function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          function_info, isolate->native_context());
  return ToApiHandle<Script>(function);
}


int UnboundScript::GetId() {
  i::Handle<i::HeapObject> obj =
      i::Handle<i::HeapObject>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, UnboundScript, GetId);
  i::HandleScope scope(isolate);
  i::Handle<i::SharedFunctionInfo> function_info(
      i::SharedFunctionInfo::cast(*obj));
  i::Handle<i::Script> script(i::Script::cast(function_info->script()));
  return script->id();
}


int UnboundScript::GetLineNumber(int code_pos) {
  i::Handle<i::SharedFunctionInfo> obj =
      i::Handle<i::SharedFunctionInfo>::cast(Utils::OpenHandle(this));
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, UnboundScript, GetLineNumber);
  if (obj->script()->IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(obj->script()));
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
  if (obj->script()->IsScript()) {
    i::Object* name = i::Script::cast(obj->script())->name();
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
  if (obj->script()->IsScript()) {
    i::Object* url = i::Script::cast(obj->script())->source_url();
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
  if (obj->script()->IsScript()) {
    i::Object* url = i::Script::cast(obj->script())->source_mapping_url();
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


Local<Value> Script::Run() {
  auto self = Utils::OpenHandle(this, true);
  // If execution is terminating, Compile(..)->Run() requires this
  // check.
  if (self.is_null()) return Local<Value>();
  auto context = ContextFromHeapObject(self);
  RETURN_TO_LOCAL_UNCHECKED(Run(context), Value);
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
  return ToApiHandle<UnboundScript>(
      i::Handle<i::SharedFunctionInfo>(i::JSFunction::cast(*obj)->shared()));
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
  i::Isolate* isolate = array->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  return array->length();
}

void PrimitiveArray::Set(int index, Local<Primitive> item) {
  i::Handle<i::FixedArray> array = Utils::OpenHandle(this);
  i::Isolate* isolate = array->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  Utils::ApiCheck(index >= 0 && index < array->length(),
                  "v8::PrimitiveArray::Set",
                  "index must be greater than or equal to 0 and less than the "
                  "array length");
  i::Handle<i::Object> i_item = Utils::OpenHandle(*item);
  array->set(index, *i_item);
}

Local<Primitive> PrimitiveArray::Get(int index) {
  i::Handle<i::FixedArray> array = Utils::OpenHandle(this);
  i::Isolate* isolate = array->GetIsolate();
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
  return self->info()->module_requests()->length();
}

Local<String> Module::GetModuleRequest(int i) const {
  CHECK_GE(i, 0);
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  i::Handle<i::FixedArray> module_requests(self->info()->module_requests(),
                                           isolate);
  CHECK_LT(i, module_requests->length());
  return ToApiHandle<String>(i::handle(module_requests->get(i), isolate));
}

Location Module::GetModuleRequestLocation(int i) const {
  CHECK_GE(i, 0);
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  i::HandleScope scope(isolate);
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  i::Handle<i::FixedArray> module_request_positions(
      self->info()->module_request_positions(), isolate);
  CHECK_LT(i, module_request_positions->length());
  int position = i::Smi::ToInt(module_request_positions->get(i));
  i::Handle<i::Script> script(self->script(), isolate);
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
      i::Module::GetModuleNamespace(self);
  return ToApiHandle<Value>(module_namespace);
}

int Module::GetIdentityHash() const { return Utils::OpenHandle(this)->hash(); }

bool Module::Instantiate(Local<Context> context,
                         Module::ResolveCallback callback) {
  return InstantiateModule(context, callback).FromMaybe(false);
}

Maybe<bool> Module::InstantiateModule(Local<Context> context,
                                      Module::ResolveCallback callback) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Module, InstantiateModule, Nothing<bool>(),
           i::HandleScope);
  has_pending_exception =
      !i::Module::Instantiate(Utils::OpenHandle(this), context, callback);
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
  has_pending_exception = !ToLocal(i::Module::Evaluate(self), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<UnboundScript> ScriptCompiler::CompileUnboundInternal(
    Isolate* v8_isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  auto isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.ScriptCompiler");
  ENTER_V8_NO_SCRIPT(isolate, v8_isolate->GetCurrentContext(), ScriptCompiler,
                     CompileUnbound, MaybeLocal<UnboundScript>(),
                     InternalEscapableScope);
  bool produce_cache = options == kProduceParserCache ||
                       options == kProduceCodeCache ||
                       options == kProduceFullCodeCache;

  // Don't try to produce any kind of cache when the debugger is loaded.
  if (isolate->debug()->is_loaded() && produce_cache) {
    options = kNoCompileOptions;
  }

  i::ScriptData* script_data = nullptr;
  if (options == kConsumeParserCache || options == kConsumeCodeCache) {
    DCHECK(source->cached_data);
    // ScriptData takes care of pointer-aligning the data.
    script_data = new i::ScriptData(source->cached_data->data,
                                    source->cached_data->length);
  }

  i::Handle<i::String> str = Utils::OpenHandle(*(source->source_string));
  i::Handle<i::SharedFunctionInfo> result;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileScript");
  i::MaybeHandle<i::Object> name_obj;
  i::MaybeHandle<i::Object> source_map_url;
  i::MaybeHandle<i::FixedArray> host_defined_options =
      isolate->factory()->empty_fixed_array();
  int line_offset = 0;
  int column_offset = 0;
  if (!source->resource_name.IsEmpty()) {
    name_obj = Utils::OpenHandle(*(source->resource_name));
  }
  if (!source->host_defined_options.IsEmpty()) {
    host_defined_options = Utils::OpenHandle(*(source->host_defined_options));
  }
  if (!source->resource_line_offset.IsEmpty()) {
    line_offset = static_cast<int>(source->resource_line_offset->Value());
  }
  if (!source->resource_column_offset.IsEmpty()) {
    column_offset = static_cast<int>(source->resource_column_offset->Value());
  }
  if (!source->source_map_url.IsEmpty()) {
    source_map_url = Utils::OpenHandle(*(source->source_map_url));
  }
  i::MaybeHandle<i::SharedFunctionInfo> maybe_function_info =
      i::Compiler::GetSharedFunctionInfoForScript(
          str, name_obj, line_offset, column_offset, source->resource_options,
          source_map_url, isolate->native_context(), nullptr, &script_data,
          options, no_cache_reason, i::NOT_NATIVES_CODE, host_defined_options);
  has_pending_exception = !maybe_function_info.ToHandle(&result);
  if (has_pending_exception && script_data != nullptr) {
    // This case won't happen during normal operation; we have compiled
    // successfully and produced cached data, and but the second compilation
    // of the same source code fails.
    delete script_data;
    script_data = nullptr;
  }
  RETURN_ON_FAILED_EXECUTION(UnboundScript);

  if (produce_cache && script_data != nullptr) {
    // script_data now contains the data that was generated. source will
    // take the ownership.
    source->cached_data = new CachedData(
        script_data->data(), script_data->length(), CachedData::BufferOwned);
    script_data->ReleaseDataOwnership();
  } else if (options == kConsumeParserCache || options == kConsumeCodeCache) {
    source->cached_data->rejected = script_data->rejected();
  }
  delete script_data;
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

Local<UnboundScript> ScriptCompiler::CompileUnbound(
    Isolate* v8_isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  Utils::ApiCheck(
      !source->GetResourceOptions().IsModule(),
      "v8::ScriptCompiler::CompileUnbound",
      "v8::ScriptCompiler::CompileModule must be used to compile modules");
  RETURN_TO_LOCAL_UNCHECKED(
      CompileUnboundInternal(v8_isolate, source, options, no_cache_reason),
      UnboundScript);
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

Local<Script> ScriptCompiler::Compile(Isolate* v8_isolate, Source* source,
                                      CompileOptions options,
                                      NoCacheReason no_cache_reason) {
  auto context = v8_isolate->GetCurrentContext();
  RETURN_TO_LOCAL_UNCHECKED(Compile(context, source, options), Script);
}

MaybeLocal<Module> ScriptCompiler::CompileModule(Isolate* isolate,
                                                 Source* source) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  Utils::ApiCheck(source->GetResourceOptions().IsModule(),
                  "v8::ScriptCompiler::CompileModule",
                  "Invalid ScriptOrigin: is_module must be true");
  auto maybe = CompileUnboundInternal(isolate, source, kNoCompileOptions,
                                      kNoCacheBecauseModule);
  Local<UnboundScript> unbound;
  if (!maybe.ToLocal(&unbound)) return MaybeLocal<Module>();

  i::Handle<i::SharedFunctionInfo> shared = Utils::OpenHandle(*unbound);
  return ToApiHandle<Module>(i_isolate->factory()->NewModule(shared));
}


class IsIdentifierHelper {
 public:
  IsIdentifierHelper() : is_identifier_(false), first_char_(true) {}

  bool Check(i::String* string) {
    i::ConsString* cons_string = i::String::VisitFlat(this, string, 0);
    if (cons_string == nullptr) return is_identifier_;
    // We don't support cons strings here.
    return false;
  }
  void VisitOneByteString(const uint8_t* chars, int length) {
    for (int i = 0; i < length; ++i) {
      if (first_char_) {
        first_char_ = false;
        is_identifier_ = unicode_cache_.IsIdentifierStart(chars[0]);
      } else {
        is_identifier_ &= unicode_cache_.IsIdentifierPart(chars[i]);
      }
    }
  }
  void VisitTwoByteString(const uint16_t* chars, int length) {
    for (int i = 0; i < length; ++i) {
      if (first_char_) {
        first_char_ = false;
        is_identifier_ = unicode_cache_.IsIdentifierStart(chars[0]);
      } else {
        is_identifier_ &= unicode_cache_.IsIdentifierPart(chars[i]);
      }
    }
  }

 private:
  bool is_identifier_;
  bool first_char_;
  i::UnicodeCache unicode_cache_;
  DISALLOW_COPY_AND_ASSIGN(IsIdentifierHelper);
};


MaybeLocal<Function> ScriptCompiler::CompileFunctionInContext(
    Local<Context> v8_context, Source* source, size_t arguments_count,
    Local<String> arguments[], size_t context_extension_count,
    Local<Object> context_extensions[]) {
  PREPARE_FOR_EXECUTION(v8_context, ScriptCompiler, CompileFunctionInContext,
                        Function);
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.ScriptCompiler");
  i::Handle<i::String> source_string;
  auto factory = isolate->factory();
  if (arguments_count) {
    source_string = factory->NewStringFromStaticChars("(function(");
    for (size_t i = 0; i < arguments_count; ++i) {
      IsIdentifierHelper helper;
      if (!helper.Check(*Utils::OpenHandle(*arguments[i]))) {
        return Local<Function>();
      }
      has_pending_exception =
          !factory->NewConsString(source_string,
                                  Utils::OpenHandle(*arguments[i]))
               .ToHandle(&source_string);
      RETURN_ON_FAILED_EXECUTION(Function);
      if (i + 1 == arguments_count) continue;
      has_pending_exception =
          !factory->NewConsString(source_string,
                                  factory->LookupSingleCharacterStringFromCode(
                                      ',')).ToHandle(&source_string);
      RETURN_ON_FAILED_EXECUTION(Function);
    }
    i::Handle<i::String> brackets;
    brackets = factory->NewStringFromStaticChars("){");
    has_pending_exception = !factory->NewConsString(source_string, brackets)
                                 .ToHandle(&source_string);
    RETURN_ON_FAILED_EXECUTION(Function);
  } else {
    source_string = factory->NewStringFromStaticChars("(function(){");
  }

  int scope_position = source_string->length();
  has_pending_exception =
      !factory->NewConsString(source_string,
                              Utils::OpenHandle(*source->source_string))
           .ToHandle(&source_string);
  RETURN_ON_FAILED_EXECUTION(Function);
  // Include \n in case the source contains a line end comment.
  auto brackets = factory->NewStringFromStaticChars("\n})");
  has_pending_exception =
      !factory->NewConsString(source_string, brackets).ToHandle(&source_string);
  RETURN_ON_FAILED_EXECUTION(Function);

  i::Handle<i::Context> context = Utils::OpenHandle(*v8_context);
  i::Handle<i::SharedFunctionInfo> outer_info(context->closure()->shared(),
                                              isolate);
  for (size_t i = 0; i < context_extension_count; ++i) {
    i::Handle<i::JSReceiver> extension =
        Utils::OpenHandle(*context_extensions[i]);
    if (!extension->IsJSObject()) return Local<Function>();
    i::Handle<i::JSFunction> closure(context->closure(), isolate);
    context = factory->NewWithContext(
        closure, context,
        i::ScopeInfo::CreateForWithScope(
            isolate, context->IsNativeContext()
                         ? i::Handle<i::ScopeInfo>::null()
                         : i::Handle<i::ScopeInfo>(context->scope_info())),
        extension);
  }

  i::Handle<i::Object> name_obj;
  int eval_scope_position = 0;
  int eval_position = i::kNoSourcePosition;
  int line_offset = 0;
  int column_offset = 0;
  if (!source->resource_name.IsEmpty()) {
    name_obj = Utils::OpenHandle(*(source->resource_name));
  }
  if (!source->resource_line_offset.IsEmpty()) {
    line_offset = static_cast<int>(source->resource_line_offset->Value());
  }
  if (!source->resource_column_offset.IsEmpty()) {
    column_offset = static_cast<int>(source->resource_column_offset->Value());
  }
  i::Handle<i::JSFunction> fun;
  has_pending_exception =
      !i::Compiler::GetFunctionFromEval(
           source_string, outer_info, context, i::LanguageMode::kSloppy,
           i::ONLY_SINGLE_FUNCTION_LITERAL, i::kNoSourcePosition,
           eval_scope_position, eval_position, line_offset,
           column_offset - scope_position, name_obj, source->resource_options)
           .ToHandle(&fun);
  if (has_pending_exception) {
    isolate->ReportPendingMessages();
  }
  RETURN_ON_FAILED_EXECUTION(Function);

  i::Handle<i::Object> result;
  has_pending_exception =
      !i::Execution::Call(isolate, fun,
                          Utils::OpenHandle(*v8_context->Global()), 0,
                          nullptr).ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Function);
  RETURN_ESCAPED(
      Utils::CallableToLocal(i::Handle<i::JSFunction>::cast(result)));
}


Local<Function> ScriptCompiler::CompileFunctionInContext(
    Isolate* v8_isolate, Source* source, Local<Context> v8_context,
    size_t arguments_count, Local<String> arguments[],
    size_t context_extension_count, Local<Object> context_extensions[]) {
  RETURN_TO_LOCAL_UNCHECKED(
      CompileFunctionInContext(v8_context, source, arguments_count, arguments,
                               context_extension_count, context_extensions),
      Function);
}


ScriptCompiler::ScriptStreamingTask* ScriptCompiler::StartStreamingScript(
    Isolate* v8_isolate, StreamedSource* source, CompileOptions options) {
  if (!i::FLAG_script_streaming) {
    return nullptr;
  }
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  return new i::BackgroundParsingTask(source->impl(), options,
                                      i::FLAG_stack_size, isolate);
}


MaybeLocal<Script> ScriptCompiler::Compile(Local<Context> context,
                                           StreamedSource* v8_source,
                                           Local<String> full_source_string,
                                           const ScriptOrigin& origin) {
  PREPARE_FOR_EXECUTION(context, ScriptCompiler, Compile, Script);
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.ScriptCompiler");
  i::StreamedSource* source = v8_source->impl();
  i::Handle<i::String> str = Utils::OpenHandle(*(full_source_string));
  i::Handle<i::Script> script = isolate->factory()->NewScript(str);
  if (!origin.ResourceName().IsEmpty()) {
    script->set_name(*Utils::OpenHandle(*(origin.ResourceName())));
  }
  if (!origin.HostDefinedOptions().IsEmpty()) {
    script->set_host_defined_options(
        *Utils::OpenHandle(*(origin.HostDefinedOptions())));
  }
  if (!origin.ResourceLineOffset().IsEmpty()) {
    script->set_line_offset(
        static_cast<int>(origin.ResourceLineOffset()->Value()));
  }
  if (!origin.ResourceColumnOffset().IsEmpty()) {
    script->set_column_offset(
        static_cast<int>(origin.ResourceColumnOffset()->Value()));
  }
  script->set_origin_options(origin.Options());
  if (!origin.SourceMapUrl().IsEmpty()) {
    script->set_source_mapping_url(
        *Utils::OpenHandle(*(origin.SourceMapUrl())));
  }

  source->info->set_script(script);
  source->parser->UpdateStatistics(isolate, script);
  source->info->UpdateBackgroundParseStatisticsOnMainThread(isolate);
  source->parser->HandleSourceURLComments(isolate, script);

  i::Handle<i::SharedFunctionInfo> result;
  if (source->info->literal() == nullptr) {
    // Parsing has failed - report error messages.
    source->info->pending_error_handler()->ReportErrors(
        isolate, script, source->info->ast_value_factory());
  } else {
    // Parsing has succeeded - finalize compile.
    if (i::FLAG_background_compile) {
      result = i::Compiler::GetSharedFunctionInfoForBackgroundCompile(
          script, source->info.get(), str->length(),
          source->outer_function_job.get(), &source->inner_function_jobs);
    } else {
      result = i::Compiler::GetSharedFunctionInfoForStreamedScript(
          script, source->info.get(), str->length());
    }
  }
  has_pending_exception = result.is_null();
  if (has_pending_exception) isolate->ReportPendingMessages();

  source->Release();

  RETURN_ON_FAILED_EXECUTION(Script);

  Local<UnboundScript> generic = ToApiHandle<UnboundScript>(result);
  if (generic.IsEmpty()) return Local<Script>();
  Local<Script> bound = generic->BindToCurrentContext();
  if (bound.IsEmpty()) return Local<Script>();
  RETURN_ESCAPED(bound);
}


Local<Script> ScriptCompiler::Compile(Isolate* v8_isolate,
                                      StreamedSource* v8_source,
                                      Local<String> full_source_string,
                                      const ScriptOrigin& origin) {
  auto context = v8_isolate->GetCurrentContext();
  RETURN_TO_LOCAL_UNCHECKED(
      Compile(context, v8_source, full_source_string, origin), Script);
}


uint32_t ScriptCompiler::CachedDataVersionTag() {
  return static_cast<uint32_t>(base::hash_combine(
      internal::Version::Hash(), internal::FlagList::Hash(),
      static_cast<uint32_t>(internal::CpuFeatures::SupportedFeatures())));
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


Local<Script> Script::Compile(v8::Local<String> source,
                              v8::ScriptOrigin* origin) {
  auto str = Utils::OpenHandle(*source);
  auto context = ContextFromHeapObject(str);
  RETURN_TO_LOCAL_UNCHECKED(Compile(context, source, origin), Script);
}


Local<Script> Script::Compile(v8::Local<String> source,
                              v8::Local<String> file_name) {
  auto str = Utils::OpenHandle(*source);
  auto context = ContextFromHeapObject(str);
  ScriptOrigin origin(file_name);
  return Compile(context, source, &origin).FromMaybe(Local<Script>());
}


// --- E x c e p t i o n s ---


v8::TryCatch::TryCatch()
    : isolate_(i::Isolate::Current()),
      next_(isolate_->try_catch_handler()),
      is_verbose_(false),
      can_continue_(true),
      capture_message_(true),
      rethrow_(false),
      has_terminated_(false) {
  ResetInternal();
  // Special handling for simulators which have a separate JS stack.
  js_stack_comparable_address_ =
      reinterpret_cast<void*>(i::SimulatorStack::RegisterCTryCatch(
          isolate_, i::GetCurrentStackPosition()));
  isolate_->RegisterTryCatchHandler(this);
}


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
  js_stack_comparable_address_ =
      reinterpret_cast<void*>(i::SimulatorStack::RegisterCTryCatch(
          isolate_, i::GetCurrentStackPosition()));
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
    i::SimulatorStack::UnregisterCTryCatch(isolate_);
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
    i::SimulatorStack::UnregisterCTryCatch(isolate_);
  }
}

void* v8::TryCatch::operator new(size_t) { base::OS::Abort(); }
void* v8::TryCatch::operator new[](size_t) { base::OS::Abort(); }
void v8::TryCatch::operator delete(void*, size_t) { base::OS::Abort(); }
void v8::TryCatch::operator delete[](void*, size_t) { base::OS::Abort(); }

bool v8::TryCatch::HasCaught() const {
  return !reinterpret_cast<i::Object*>(exception_)->IsTheHole(isolate_);
}


bool v8::TryCatch::CanContinue() const {
  return can_continue_;
}


bool v8::TryCatch::HasTerminated() const {
  return has_terminated_;
}


v8::Local<v8::Value> v8::TryCatch::ReThrow() {
  if (!HasCaught()) return v8::Local<v8::Value>();
  rethrow_ = true;
  return v8::Undefined(reinterpret_cast<v8::Isolate*>(isolate_));
}


v8::Local<Value> v8::TryCatch::Exception() const {
  if (HasCaught()) {
    // Check for out of memory exception.
    i::Object* exception = reinterpret_cast<i::Object*>(exception_);
    return v8::Utils::ToLocal(i::Handle<i::Object>(exception, isolate_));
  } else {
    return v8::Local<Value>();
  }
}


MaybeLocal<Value> v8::TryCatch::StackTrace(Local<Context> context) const {
  if (!HasCaught()) return v8::Local<Value>();
  i::Object* raw_obj = reinterpret_cast<i::Object*>(exception_);
  if (!raw_obj->IsJSObject()) return v8::Local<Value>();
  PREPARE_FOR_EXECUTION(context, TryCatch, StackTrace, Value);
  i::Handle<i::JSObject> obj(i::JSObject::cast(raw_obj), isolate_);
  i::Handle<i::String> name = isolate->factory()->stack_string();
  Maybe<bool> maybe = i::JSReceiver::HasProperty(obj, name);
  has_pending_exception = maybe.IsNothing();
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!maybe.FromJust()) return v8::Local<Value>();
  Local<Value> result;
  has_pending_exception =
      !ToLocal<Value>(i::JSReceiver::GetProperty(obj, name), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}


v8::Local<Value> v8::TryCatch::StackTrace() const {
  auto context = reinterpret_cast<v8::Isolate*>(isolate_)->GetCurrentContext();
  RETURN_TO_LOCAL_UNCHECKED(StackTrace(context), Value);
}


v8::Local<v8::Message> v8::TryCatch::Message() const {
  i::Object* message = reinterpret_cast<i::Object*>(message_obj_);
  DCHECK(message->IsJSMessageObject() || message->IsTheHole(isolate_));
  if (HasCaught() && !message->IsTheHole(isolate_)) {
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
  i::Object* the_hole = isolate_->heap()->the_hole_value();
  exception_ = the_hole;
  message_obj_ = the_hole;
}


void v8::TryCatch::SetVerbose(bool value) {
  is_verbose_ = value;
}

bool v8::TryCatch::IsVerbose() const { return is_verbose_; }

void v8::TryCatch::SetCaptureMessage(bool value) {
  capture_message_ = value;
}


// --- M e s s a g e ---


Local<String> Message::Get() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::String> raw_result = i::MessageHandler::GetMessage(isolate, obj);
  Local<String> result = Utils::ToLocal(raw_result);
  return scope.Escape(result);
}


ScriptOrigin Message::GetScriptOrigin() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  auto message = i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  auto script_wraper = i::Handle<i::Object>(message->script(), isolate);
  auto script_value = i::Handle<i::JSValue>::cast(script_wraper);
  i::Handle<i::Script> script(i::Script::cast(script_value->value()));
  return GetScriptOriginForScript(isolate, script);
}


v8::Local<Value> Message::GetScriptResourceName() const {
  return GetScriptOrigin().ResourceName();
}


v8::Local<v8::StackTrace> Message::GetStackTrace() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  auto message = i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  i::Handle<i::Object> stackFramesObj(message->stack_frames(), isolate);
  if (!stackFramesObj->IsFixedArray()) return v8::Local<v8::StackTrace>();
  auto stackTrace = i::Handle<i::FixedArray>::cast(stackFramesObj);
  return scope.Escape(Utils::StackTraceToLocal(stackTrace));
}


Maybe<int> Message::GetLineNumber(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  auto msg = i::Handle<i::JSMessageObject>::cast(self);
  return Just(msg->GetLineNumber());
}


int Message::GetLineNumber() const {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return GetLineNumber(context).FromMaybe(0);
}


int Message::GetStartPosition() const {
  auto self = Utils::OpenHandle(this);
  return self->start_position();
}


int Message::GetEndPosition() const {
  auto self = Utils::OpenHandle(this);
  return self->end_position();
}

int Message::ErrorLevel() const {
  auto self = Utils::OpenHandle(this);
  return self->error_level();
}

Maybe<int> Message::GetStartColumn(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  auto msg = i::Handle<i::JSMessageObject>::cast(self);
  return Just(msg->GetColumnNumber());
}


int Message::GetStartColumn() const {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  const int default_value = kNoColumnInfo;
  return GetStartColumn(context).FromMaybe(default_value);
}


Maybe<int> Message::GetEndColumn(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  auto msg = i::Handle<i::JSMessageObject>::cast(self);
  const int column_number = msg->GetColumnNumber();
  if (column_number == -1) return Just(-1);
  const int start = self->start_position();
  const int end = self->end_position();
  return Just(column_number + (end - start));
}


int Message::GetEndColumn() const {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  const int default_value = kNoColumnInfo;
  return GetEndColumn(context).FromMaybe(default_value);
}


bool Message::IsSharedCrossOrigin() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  auto self = Utils::OpenHandle(this);
  auto script = i::Handle<i::JSValue>::cast(
      i::Handle<i::Object>(self->script(), isolate));
  return i::Script::cast(script->value())
      ->origin_options()
      .IsSharedCrossOrigin();
}

bool Message::IsOpaque() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  auto self = Utils::OpenHandle(this);
  auto script = i::Handle<i::JSValue>::cast(
      i::Handle<i::Object>(self->script(), isolate));
  return i::Script::cast(script->value())->origin_options().IsOpaque();
}


MaybeLocal<String> Message::GetSourceLine(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(isolate));
  auto msg = i::Handle<i::JSMessageObject>::cast(self);
  RETURN_ESCAPED(Utils::ToLocal(msg->GetSourceLine()));
}


Local<String> Message::GetSourceLine() const {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(GetSourceLine(context), String)
}


void Message::PrintCurrentStackTrace(Isolate* isolate, FILE* out) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i_isolate->PrintCurrentStackTrace(out);
}


// --- S t a c k T r a c e ---

Local<StackFrame> StackTrace::GetFrame(uint32_t index) const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  auto obj = handle(Utils::OpenHandle(this)->get(index), isolate);
  auto info = i::Handle<i::StackFrameInfo>::cast(obj);
  return scope.Escape(Utils::StackFrameToLocal(info));
}


int StackTrace::GetFrameCount() const {
  return Utils::OpenHandle(this)->length();
}

namespace {
i::Handle<i::JSObject> NewFrameObject(i::Isolate* isolate,
                                      i::Handle<i::StackFrameInfo> frame) {
  i::Handle<i::JSObject> frame_obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  i::JSObject::AddProperty(
      frame_obj, handle(isolate->heap()->line_string()),
      handle(i::Smi::FromInt(frame->line_number() + 1), isolate), i::NONE);
  i::JSObject::AddProperty(
      frame_obj, handle(isolate->heap()->column_string()),
      handle(i::Smi::FromInt(frame->column_number() + 1), isolate), i::NONE);
  i::JSObject::AddProperty(frame_obj,
                           isolate->factory()->InternalizeOneByteString(
                               STATIC_CHAR_VECTOR("scriptId")),
                           handle(i::Smi::FromInt(frame->script_id()), isolate),
                           i::NONE);
  i::JSObject::AddProperty(frame_obj,
                           isolate->factory()->InternalizeOneByteString(
                               STATIC_CHAR_VECTOR("scriptName")),
                           handle(frame->script_name(), isolate), i::NONE);
  i::JSObject::AddProperty(frame_obj,
                           isolate->factory()->InternalizeOneByteString(
                               STATIC_CHAR_VECTOR("scriptNameOrSourceURL")),
                           handle(frame->script_name_or_source_url(), isolate),
                           i::NONE);
  i::JSObject::AddProperty(frame_obj,
                           isolate->factory()->InternalizeOneByteString(
                               STATIC_CHAR_VECTOR("functionName")),
                           handle(frame->function_name(), isolate), i::NONE);
  i::JSObject::AddProperty(frame_obj,
                           isolate->factory()->InternalizeOneByteString(
                               STATIC_CHAR_VECTOR("isEval")),
                           isolate->factory()->ToBoolean(frame->is_eval()),
                           i::NONE);
  i::JSObject::AddProperty(
      frame_obj,
      isolate->factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("isConstructor")),
      isolate->factory()->ToBoolean(frame->is_constructor()), i::NONE);
  return frame_obj;
}
}  // namespace

Local<Array> StackTrace::AsArray() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  i::Handle<i::FixedArray> self = Utils::OpenHandle(this);
  int frame_count = self->length();
  i::Handle<i::FixedArray> frames =
      isolate->factory()->NewFixedArray(frame_count);
  for (int i = 0; i < frame_count; ++i) {
    auto obj = handle(self->get(i), isolate);
    auto frame = i::Handle<i::StackFrameInfo>::cast(obj);
    i::Handle<i::JSObject> frame_obj = NewFrameObject(isolate, frame);
    frames->set(i, *frame_obj);
  }
  return Utils::ToLocal(isolate->factory()->NewJSArrayWithElements(
      frames, i::PACKED_ELEMENTS, frame_count));
}


Local<StackTrace> StackTrace::CurrentStackTrace(
    Isolate* isolate,
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
  int v = Utils::OpenHandle(this)->line_number();
  return v ? v : Message::kNoLineNumberInfo;
}


int StackFrame::GetColumn() const {
  int v = Utils::OpenHandle(this)->column_number();
  return v ? v : Message::kNoLineNumberInfo;
}


int StackFrame::GetScriptId() const {
  int v = Utils::OpenHandle(this)->script_id();
  return v ? v : Message::kNoScriptIdInfo;
}

Local<String> StackFrame::GetScriptName() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Handle<i::Object> obj(self->script_name(), isolate);
  return obj->IsString()
             ? scope.Escape(Local<String>::Cast(Utils::ToLocal(obj)))
             : Local<String>();
}


Local<String> StackFrame::GetScriptNameOrSourceURL() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Handle<i::Object> obj(self->script_name_or_source_url(), isolate);
  return obj->IsString()
             ? scope.Escape(Local<String>::Cast(Utils::ToLocal(obj)))
             : Local<String>();
}


Local<String> StackFrame::GetFunctionName() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Handle<i::Object> obj(self->function_name(), isolate);
  return obj->IsString()
             ? scope.Escape(Local<String>::Cast(Utils::ToLocal(obj)))
             : Local<String>();
}

bool StackFrame::IsEval() const { return Utils::OpenHandle(this)->is_eval(); }

bool StackFrame::IsConstructor() const {
  return Utils::OpenHandle(this)->is_constructor();
}

bool StackFrame::IsWasm() const { return Utils::OpenHandle(this)->is_wasm(); }


// --- J S O N ---

MaybeLocal<Value> JSON::Parse(Isolate* v8_isolate, Local<String> json_string) {
  PREPARE_FOR_EXECUTION(v8_isolate->GetCurrentContext(), JSON, Parse, Value);
  i::Handle<i::String> string = Utils::OpenHandle(*json_string);
  i::Handle<i::String> source = i::String::Flatten(string);
  i::Handle<i::Object> undefined = isolate->factory()->undefined_value();
  auto maybe = source->IsSeqOneByteString()
                   ? i::JsonParser<true>::Parse(isolate, source, undefined)
                   : i::JsonParser<false>::Parse(isolate, source, undefined);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(maybe, &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<Value> JSON::Parse(Local<Context> context,
                              Local<String> json_string) {
  PREPARE_FOR_EXECUTION(context, JSON, Parse, Value);
  i::Handle<i::String> string = Utils::OpenHandle(*json_string);
  i::Handle<i::String> source = i::String::Flatten(string);
  i::Handle<i::Object> undefined = isolate->factory()->undefined_value();
  auto maybe = source->IsSeqOneByteString()
                   ? i::JsonParser<true>::Parse(isolate, source, undefined)
                   : i::JsonParser<false>::Parse(isolate, source, undefined);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(maybe, &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

Local<Value> JSON::Parse(Local<String> json_string) {
  RETURN_TO_LOCAL_UNCHECKED(Parse(Local<Context>(), json_string), Value);
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
  has_pending_exception = !i::JsonStringifier(isolate)
                               .Stringify(object, replacer, gap_string)
                               .ToHandle(&maybe);
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
    Isolate* v8_isolate, Local<WasmCompiledModule> module) {
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

std::vector<uint8_t> ValueSerializer::ReleaseBuffer() {
  return private_->serializer.ReleaseBuffer();
}

std::pair<uint8_t*, size_t> ValueSerializer::Release() {
  return private_->serializer.Release();
}

void ValueSerializer::TransferArrayBuffer(uint32_t transfer_id,
                                          Local<ArrayBuffer> array_buffer) {
  private_->serializer.TransferArrayBuffer(transfer_id,
                                           Utils::OpenHandle(*array_buffer));
}

void ValueSerializer::TransferSharedArrayBuffer(
    uint32_t transfer_id, Local<SharedArrayBuffer> shared_array_buffer) {
  private_->serializer.TransferArrayBuffer(
      transfer_id, Utils::OpenHandle(*shared_array_buffer));
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

MaybeLocal<WasmCompiledModule> ValueDeserializer::Delegate::GetWasmModuleFromId(
    Isolate* v8_isolate, uint32_t id) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->ScheduleThrow(*isolate->factory()->NewError(
      isolate->error_function(),
      i::MessageTemplate::kDataCloneDeserializationError));
  return MaybeLocal<WasmCompiledModule>();
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

void ValueDeserializer::SetExpectInlineWasm(bool expect_inline_wasm) {
  private_->deserializer.set_expect_inline_wasm(expect_inline_wasm);
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
  bool result = false;
  if (!object->IsSmi()) {
    result = object->IsUndefined(i::HeapObject::cast(*object)->GetIsolate());
  }
  DCHECK_EQ(result, QuickIsUndefined());
  return result;
}


bool Value::FullIsNull() const {
  i::Handle<i::Object> object = Utils::OpenHandle(this);
  bool result = false;
  if (!object->IsSmi()) {
    result = object->IsNull(i::HeapObject::cast(*object)->GetIsolate());
  }
  DCHECK_EQ(result, QuickIsNull());
  return result;
}


bool Value::IsTrue() const {
  i::Handle<i::Object> object = Utils::OpenHandle(this);
  if (object->IsSmi()) return false;
  return object->IsTrue(i::HeapObject::cast(*object)->GetIsolate());
}


bool Value::IsFalse() const {
  i::Handle<i::Object> object = Utils::OpenHandle(this);
  if (object->IsSmi()) return false;
  return object->IsFalse(i::HeapObject::cast(*object)->GetIsolate());
}


bool Value::IsFunction() const { return Utils::OpenHandle(this)->IsCallable(); }


bool Value::IsName() const {
  return Utils::OpenHandle(this)->IsName();
}


bool Value::FullIsString() const {
  bool result = Utils::OpenHandle(this)->IsString();
  DCHECK_EQ(result, QuickIsString());
  return result;
}


bool Value::IsSymbol() const {
  return Utils::OpenHandle(this)->IsSymbol();
}


bool Value::IsArray() const {
  return Utils::OpenHandle(this)->IsJSArray();
}


bool Value::IsArrayBuffer() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsJSArrayBuffer() && !i::JSArrayBuffer::cast(*obj)->is_shared();
}


bool Value::IsArrayBufferView() const {
  return Utils::OpenHandle(this)->IsJSArrayBufferView();
}


bool Value::IsTypedArray() const {
  return Utils::OpenHandle(this)->IsJSTypedArray();
}


#define VALUE_IS_TYPED_ARRAY(Type, typeName, TYPE, ctype, size)              \
  bool Value::Is##Type##Array() const {                                      \
    i::Handle<i::Object> obj = Utils::OpenHandle(this);                      \
    return obj->IsJSTypedArray() &&                                          \
           i::JSTypedArray::cast(*obj)->type() == i::kExternal##Type##Array; \
  }


TYPED_ARRAYS(VALUE_IS_TYPED_ARRAY)

#undef VALUE_IS_TYPED_ARRAY


bool Value::IsDataView() const {
  return Utils::OpenHandle(this)->IsJSDataView();
}


bool Value::IsSharedArrayBuffer() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsJSArrayBuffer() && i::JSArrayBuffer::cast(*obj)->is_shared();
}


bool Value::IsObject() const { return Utils::OpenHandle(this)->IsJSReceiver(); }


bool Value::IsNumber() const {
  return Utils::OpenHandle(this)->IsNumber();
}


bool Value::IsProxy() const { return Utils::OpenHandle(this)->IsJSProxy(); }

bool Value::IsWebAssemblyCompiledModule() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (!obj->IsJSObject()) return false;
  i::Handle<i::JSObject> js_obj = i::Handle<i::JSObject>::cast(obj);
  return js_obj->GetIsolate()->native_context()->wasm_module_constructor() ==
         js_obj->map()->GetConstructor();
}

#define VALUE_IS_SPECIFIC_TYPE(Type, Class)                            \
  bool Value::Is##Type() const {                                       \
    i::Handle<i::Object> obj = Utils::OpenHandle(this);                \
    if (!obj->IsHeapObject()) return false;                            \
    i::Isolate* isolate = i::HeapObject::cast(*obj)->GetIsolate();     \
    return obj->HasSpecificClassOf(isolate->heap()->Class##_string()); \
  }

VALUE_IS_SPECIFIC_TYPE(ArgumentsObject, Arguments)
VALUE_IS_SPECIFIC_TYPE(BooleanObject, Boolean)
VALUE_IS_SPECIFIC_TYPE(NumberObject, Number)
VALUE_IS_SPECIFIC_TYPE(StringObject, String)
VALUE_IS_SPECIFIC_TYPE(SymbolObject, Symbol)
VALUE_IS_SPECIFIC_TYPE(Date, Date)
VALUE_IS_SPECIFIC_TYPE(Map, Map)
VALUE_IS_SPECIFIC_TYPE(Set, Set)
VALUE_IS_SPECIFIC_TYPE(WeakMap, WeakMap)
VALUE_IS_SPECIFIC_TYPE(WeakSet, WeakSet)

#undef VALUE_IS_SPECIFIC_TYPE


bool Value::IsBoolean() const {
  return Utils::OpenHandle(this)->IsBoolean();
}


bool Value::IsExternal() const {
  return Utils::OpenHandle(this)->IsExternal();
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
    return !i::IsMinusZero(value) &&
        value >= 0 &&
        value <= i::kMaxUInt32 &&
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
  return i::IsAsyncFunction(func->shared()->kind());
}

bool Value::IsGeneratorFunction() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (!obj->IsJSFunction()) return false;
  i::Handle<i::JSFunction> func = i::Handle<i::JSFunction>::cast(obj);
  return i::IsGeneratorFunction(func->shared()->kind());
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


Local<String> Value::ToString(Isolate* isolate) const {
  RETURN_TO_LOCAL_UNCHECKED(ToString(isolate->GetCurrentContext()), String);
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


Local<String> Value::ToDetailString(Isolate* isolate) const {
  RETURN_TO_LOCAL_UNCHECKED(ToDetailString(isolate->GetCurrentContext()),
                            String);
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


Local<v8::Object> Value::ToObject(Isolate* isolate) const {
  RETURN_TO_LOCAL_UNCHECKED(ToObject(isolate->GetCurrentContext()), Object);
}


MaybeLocal<Boolean> Value::ToBoolean(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsBoolean()) return ToApiHandle<Boolean>(obj);
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  auto val = isolate->factory()->ToBoolean(obj->BooleanValue());
  return ToApiHandle<Boolean>(val);
}


Local<Boolean> Value::ToBoolean(Isolate* v8_isolate) const {
  return ToBoolean(v8_isolate->GetCurrentContext()).ToLocalChecked();
}


MaybeLocal<Number> Value::ToNumber(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return ToApiHandle<Number>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToNumber, Number);
  Local<Number> result;
  has_pending_exception = !ToLocal<Number>(i::Object::ToNumber(obj), &result);
  RETURN_ON_FAILED_EXECUTION(Number);
  RETURN_ESCAPED(result);
}


Local<Number> Value::ToNumber(Isolate* isolate) const {
  RETURN_TO_LOCAL_UNCHECKED(ToNumber(isolate->GetCurrentContext()), Number);
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


Local<Integer> Value::ToInteger(Isolate* isolate) const {
  RETURN_TO_LOCAL_UNCHECKED(ToInteger(isolate->GetCurrentContext()), Integer);
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


Local<Int32> Value::ToInt32(Isolate* isolate) const {
  RETURN_TO_LOCAL_UNCHECKED(ToInt32(isolate->GetCurrentContext()), Int32);
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


Local<Uint32> Value::ToUint32(Isolate* isolate) const {
  RETURN_TO_LOCAL_UNCHECKED(ToUint32(isolate->GetCurrentContext()), Uint32);
}


void i::Internals::CheckInitializedImpl(v8::Isolate* external_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  Utils::ApiCheck(isolate != nullptr && !isolate->IsDead(),
                  "v8::internal::Internals::CheckInitialized",
                  "Isolate is not initialized or V8 has died");
}


void External::CheckCast(v8::Value* that) {
  Utils::ApiCheck(Utils::OpenHandle(that)->IsExternal(), "v8::External::Cast",
                  "Could not convert to external");
}


void v8::Object::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSReceiver(), "v8::Object::Cast",
                  "Could not convert to object");
}


void v8::Function::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsCallable(), "v8::Function::Cast",
                  "Could not convert to function");
}


void v8::Boolean::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsBoolean(), "v8::Boolean::Cast",
                  "Could not convert to boolean");
}


void v8::Name::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsName(), "v8::Name::Cast", "Could not convert to name");
}


void v8::String::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsString(), "v8::String::Cast",
                  "Could not convert to string");
}


void v8::Symbol::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsSymbol(), "v8::Symbol::Cast",
                  "Could not convert to symbol");
}


void v8::Number::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsNumber(),
                  "v8::Number::Cast()",
                  "Could not convert to number");
}


void v8::Integer::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsNumber(), "v8::Integer::Cast",
                  "Could not convert to number");
}


void v8::Int32::CheckCast(v8::Value* that) {
  Utils::ApiCheck(that->IsInt32(), "v8::Int32::Cast",
                  "Could not convert to 32-bit signed integer");
}


void v8::Uint32::CheckCast(v8::Value* that) {
  Utils::ApiCheck(that->IsUint32(), "v8::Uint32::Cast",
                  "Could not convert to 32-bit unsigned integer");
}


void v8::Array::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSArray(), "v8::Array::Cast",
                  "Could not convert to array");
}


void v8::Map::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSMap(), "v8::Map::Cast", "Could not convert to Map");
}


void v8::Set::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSSet(), "v8_Set_Cast", "Could not convert to Set");
}


void v8::Promise::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsPromise(), "v8::Promise::Cast",
                  "Could not convert to promise");
}


void v8::Promise::Resolver::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsPromise(), "v8::Promise::Resolver::Cast",
                  "Could not convert to promise resolver");
}


void v8::Proxy::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsProxy(), "v8::Proxy::Cast",
                  "Could not convert to proxy");
}

void v8::WasmCompiledModule::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsWebAssemblyCompiledModule(),
                  "v8::WasmCompiledModule::Cast",
                  "Could not convert to wasm compiled module");
}

void v8::ArrayBuffer::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(
      obj->IsJSArrayBuffer() && !i::JSArrayBuffer::cast(*obj)->is_shared(),
      "v8::ArrayBuffer::Cast()", "Could not convert to ArrayBuffer");
}


void v8::ArrayBufferView::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSArrayBufferView(),
                  "v8::ArrayBufferView::Cast()",
                  "Could not convert to ArrayBufferView");
}


void v8::TypedArray::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSTypedArray(),
                  "v8::TypedArray::Cast()",
                  "Could not convert to TypedArray");
}


#define CHECK_TYPED_ARRAY_CAST(Type, typeName, TYPE, ctype, size)             \
  void v8::Type##Array::CheckCast(Value* that) {                              \
    i::Handle<i::Object> obj = Utils::OpenHandle(that);                       \
    Utils::ApiCheck(                                                          \
        obj->IsJSTypedArray() &&                                              \
            i::JSTypedArray::cast(*obj)->type() == i::kExternal##Type##Array, \
        "v8::" #Type "Array::Cast()", "Could not convert to " #Type "Array"); \
  }


TYPED_ARRAYS(CHECK_TYPED_ARRAY_CAST)

#undef CHECK_TYPED_ARRAY_CAST


void v8::DataView::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSDataView(),
                  "v8::DataView::Cast()",
                  "Could not convert to DataView");
}


void v8::SharedArrayBuffer::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(
      obj->IsJSArrayBuffer() && i::JSArrayBuffer::cast(*obj)->is_shared(),
      "v8::SharedArrayBuffer::Cast()",
      "Could not convert to SharedArrayBuffer");
}


void v8::Date::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  i::Isolate* isolate = nullptr;
  if (obj->IsHeapObject()) isolate = i::HeapObject::cast(*obj)->GetIsolate();
  Utils::ApiCheck(isolate != nullptr &&
                      obj->HasSpecificClassOf(isolate->heap()->Date_string()),
                  "v8::Date::Cast()", "Could not convert to date");
}


void v8::StringObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  i::Isolate* isolate = nullptr;
  if (obj->IsHeapObject()) isolate = i::HeapObject::cast(*obj)->GetIsolate();
  Utils::ApiCheck(isolate != nullptr &&
                      obj->HasSpecificClassOf(isolate->heap()->String_string()),
                  "v8::StringObject::Cast()",
                  "Could not convert to StringObject");
}


void v8::SymbolObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  i::Isolate* isolate = nullptr;
  if (obj->IsHeapObject()) isolate = i::HeapObject::cast(*obj)->GetIsolate();
  Utils::ApiCheck(isolate != nullptr &&
                      obj->HasSpecificClassOf(isolate->heap()->Symbol_string()),
                  "v8::SymbolObject::Cast()",
                  "Could not convert to SymbolObject");
}


void v8::NumberObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  i::Isolate* isolate = nullptr;
  if (obj->IsHeapObject()) isolate = i::HeapObject::cast(*obj)->GetIsolate();
  Utils::ApiCheck(isolate != nullptr &&
                      obj->HasSpecificClassOf(isolate->heap()->Number_string()),
                  "v8::NumberObject::Cast()",
                  "Could not convert to NumberObject");
}


void v8::BooleanObject::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  i::Isolate* isolate = nullptr;
  if (obj->IsHeapObject()) isolate = i::HeapObject::cast(*obj)->GetIsolate();
  Utils::ApiCheck(
      isolate != nullptr &&
          obj->HasSpecificClassOf(isolate->heap()->Boolean_string()),
      "v8::BooleanObject::Cast()", "Could not convert to BooleanObject");
}


void v8::RegExp::CheckCast(v8::Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsJSRegExp(),
                  "v8::RegExp::Cast()",
                  "Could not convert to regular expression");
}


Maybe<bool> Value::BooleanValue(Local<Context> context) const {
  return Just(Utils::OpenHandle(this)->BooleanValue());
}


bool Value::BooleanValue() const {
  return Utils::OpenHandle(this)->BooleanValue();
}


Maybe<double> Value::NumberValue(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return Just(obj->Number());
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Value, NumberValue, Nothing<double>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToNumber(obj).ToHandle(&num);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(double);
  return Just(num->Number());
}


double Value::NumberValue() const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return obj->Number();
  return NumberValue(ContextFromHeapObject(obj))
      .FromMaybe(std::numeric_limits<double>::quiet_NaN());
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


int64_t Value::IntegerValue() const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) {
    if (obj->IsSmi()) {
      return i::Smi::ToInt(*obj);
    } else {
      return static_cast<int64_t>(obj->Number());
    }
  }
  return IntegerValue(ContextFromHeapObject(obj)).FromMaybe(0);
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


int32_t Value::Int32Value() const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return NumberToInt32(*obj);
  return Int32Value(ContextFromHeapObject(obj)).FromMaybe(0);
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


uint32_t Value::Uint32Value() const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return NumberToUint32(*obj);
  return Uint32Value(ContextFromHeapObject(obj)).FromMaybe(0);
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


Local<Uint32> Value::ToArrayIndex() const {
  auto self = Utils::OpenHandle(this);
  if (self->IsSmi()) {
    if (i::Smi::ToInt(*self) >= 0) return Utils::Uint32ToLocal(self);
    return Local<Uint32>();
  }
  auto context = ContextFromHeapObject(self);
  RETURN_TO_LOCAL_UNCHECKED(ToArrayIndex(context), Uint32);
}


Maybe<bool> Value::Equals(Local<Context> context, Local<Value> that) const {
  auto self = Utils::OpenHandle(this);
  auto other = Utils::OpenHandle(*that);
  return i::Object::Equals(self, other);
}


bool Value::Equals(Local<Value> that) const {
  auto self = Utils::OpenHandle(this);
  auto other = Utils::OpenHandle(*that);
  if (self->IsSmi() && other->IsSmi()) {
    return self->Number() == other->Number();
  }
  if (self->IsJSObject() && other->IsJSObject()) {
    return *self == *other;
  }
  auto heap_object = self->IsSmi() ? other : self;
  auto context = ContextFromHeapObject(heap_object);
  return Equals(context, that).FromMaybe(false);
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
                                    i::LanguageMode::kSloppy)
          .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}


bool v8::Object::Set(v8::Local<Value> key, v8::Local<Value> value) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return Set(context, key, value).FromMaybe(false);
}


Maybe<bool> v8::Object::Set(v8::Local<v8::Context> context, uint32_t index,
                            v8::Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Object, Set, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto value_obj = Utils::OpenHandle(*value);
  has_pending_exception = i::Object::SetElement(isolate, self, index, value_obj,
                                                i::LanguageMode::kSloppy)
                              .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}


bool v8::Object::Set(uint32_t index, v8::Local<Value> value) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return Set(context, index, value).FromMaybe(false);
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

  i::LookupIterator it = i::LookupIterator::PropertyOrElement(
      isolate, self, key_obj, self, i::LookupIterator::OWN);
  Maybe<bool> result =
      i::JSReceiver::CreateDataProperty(&it, value_obj, i::kDontThrow);
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
      i::JSReceiver::CreateDataProperty(&it, value_obj, i::kDontThrow);
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
        isolate, self, key_obj, &desc, i::kDontThrow);
    // Even though we said kDontThrow, there might be accessors that do throw.
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return success;
  } else {
    // If it's not a JSProxy, i::JSReceiver::DefineOwnProperty should never run
    // a script.
    ENTER_V8_NO_SCRIPT(isolate, context, Object, DefineOwnProperty,
                       Nothing<bool>(), i::HandleScope);
    Maybe<bool> success = i::JSReceiver::DefineOwnProperty(
        isolate, self, key_obj, &desc, i::kDontThrow);
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
      isolate, self, key_obj, &descriptor.get_private()->desc, i::kDontThrow);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return success;
}

MUST_USE_RESULT
static i::MaybeHandle<i::Object> DefineObjectProperty(
    i::Handle<i::JSObject> js_object, i::Handle<i::Object> key,
    i::Handle<i::Object> value, i::PropertyAttributes attrs) {
  i::Isolate* isolate = js_object->GetIsolate();
  bool success = false;
  i::LookupIterator it = i::LookupIterator::PropertyOrElement(
      isolate, js_object, key, &success, i::LookupIterator::OWN);
  if (!success) return i::MaybeHandle<i::Object>();

  return i::JSObject::DefineOwnPropertyIgnoreAttributes(
      &it, value, attrs, i::JSObject::FORCE_FIELD);
}


Maybe<bool> v8::Object::ForceSet(v8::Local<v8::Context> context,
                                 v8::Local<Value> key, v8::Local<Value> value,
                                 v8::PropertyAttribute attribs) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, ForceSet, Nothing<bool>(),
                     i::HandleScope);
  auto self = i::Handle<i::JSObject>::cast(Utils::OpenHandle(this));
  auto key_obj = Utils::OpenHandle(*key);
  auto value_obj = Utils::OpenHandle(*value);
  has_pending_exception =
      DefineObjectProperty(self, key_obj, value_obj,
                           static_cast<i::PropertyAttributes>(attribs))
          .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
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
    return i::JSProxy::SetPrivateProperty(
        isolate, i::Handle<i::JSProxy>::cast(self),
        i::Handle<i::Symbol>::cast(key_obj), &desc, i::kDontThrow);
  }
  auto js_object = i::Handle<i::JSObject>::cast(self);
  i::LookupIterator it(js_object, key_obj, js_object);
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


Local<Value> v8::Object::Get(v8::Local<Value> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(Get(context, key), Value);
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


Local<Value> v8::Object::Get(uint32_t index) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(Get(context, index), Value);
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


PropertyAttribute v8::Object::GetPropertyAttributes(v8::Local<Value> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return GetPropertyAttributes(context, key)
      .FromMaybe(static_cast<PropertyAttribute>(i::NONE));
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

Local<Value> v8::Object::GetOwnPropertyDescriptor(Local<Name> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(GetOwnPropertyDescriptor(context, key), Value);
}


Local<Value> v8::Object::GetPrototype() {
  auto isolate = Utils::OpenHandle(this)->GetIsolate();
  auto self = Utils::OpenHandle(this);
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


bool v8::Object::SetPrototype(Local<Value> value) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return SetPrototype(context, value).FromMaybe(false);
}

Local<Object> v8::Object::FindInstanceInPrototypeChain(
    v8::Local<FunctionTemplate> tmpl) {
  auto self = Utils::OpenHandle(this);
  auto isolate = self->GetIsolate();
  i::PrototypeIterator iter(isolate, *self, i::kStartAtReceiver);
  auto tmpl_info = *Utils::OpenHandle(*tmpl);
  while (!tmpl_info->IsTemplateFor(iter.GetCurrent<i::JSObject>())) {
    iter.Advance();
    if (iter.IsAtEnd()) return Local<Object>();
    if (!iter.GetCurrent()->IsJSObject()) return Local<Object>();
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

MaybeLocal<Array> v8::Object::GetPropertyNames(Local<Context> context,
                                               KeyCollectionMode mode,
                                               PropertyFilter property_filter,
                                               IndexFilter index_filter) {
  PREPARE_FOR_EXECUTION(context, Object, GetPropertyNames, Array);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::FixedArray> value;
  i::KeyAccumulator accumulator(
      isolate, static_cast<i::KeyCollectionMode>(mode),
      static_cast<i::PropertyFilter>(property_filter));
  accumulator.set_skip_indices(index_filter == IndexFilter::kSkipIndices);
  has_pending_exception = accumulator.CollectKeys(self, self).IsNothing();
  RETURN_ON_FAILED_EXECUTION(Array);
  value = accumulator.GetKeys(i::GetKeysConversion::kKeepNumbers);
  DCHECK(self->map()->EnumLength() == i::kInvalidEnumCacheSentinel ||
         self->map()->EnumLength() == 0 ||
         self->map()->instance_descriptors()->GetEnumCache()->keys() != *value);
  auto result = isolate->factory()->NewJSArrayWithElements(value);
  RETURN_ESCAPED(Utils::ToLocal(result));
}


Local<Array> v8::Object::GetPropertyNames() {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(GetPropertyNames(context), Array);
}

MaybeLocal<Array> v8::Object::GetOwnPropertyNames(Local<Context> context) {
  return GetOwnPropertyNames(
      context, static_cast<v8::PropertyFilter>(ONLY_ENUMERABLE | SKIP_SYMBOLS));
}

Local<Array> v8::Object::GetOwnPropertyNames() {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(GetOwnPropertyNames(context), Array);
}

MaybeLocal<Array> v8::Object::GetOwnPropertyNames(Local<Context> context,
                                                  PropertyFilter filter) {
  return GetPropertyNames(context, KeyCollectionMode::kOwnOnly, filter,
                          v8::IndexFilter::kIncludeIndices);
}

MaybeLocal<String> v8::Object::ObjectProtoToString(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, Object, ObjectProtoToString, String);
  auto self = Utils::OpenHandle(this);
  Local<Value> result;
  has_pending_exception =
      !ToLocal<Value>(i::Execution::Call(isolate, isolate->object_to_string(),
                                         self, 0, nullptr),
                      &result);
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(Local<String>::Cast(result));
}


Local<String> v8::Object::ObjectProtoToString() {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(ObjectProtoToString(context), String);
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

bool v8::Object::Delete(v8::Local<Value> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return Delete(context, key).FromMaybe(false);
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


bool v8::Object::Has(v8::Local<Value> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return Has(context, key).FromMaybe(false);
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


bool v8::Object::Delete(uint32_t index) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return Delete(context, index).FromMaybe(false);
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


bool v8::Object::Has(uint32_t index) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return Has(context, index).FromMaybe(false);
}

template <typename Getter, typename Setter, typename Data>
static Maybe<bool> ObjectSetAccessor(Local<Context> context, Object* self,
                                     Local<Name> name, Getter getter,
                                     Setter setter, Data data,
                                     AccessControl settings,
                                     PropertyAttribute attributes,
                                     bool is_special_data_property) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, SetAccessor, Nothing<bool>(),
                     i::HandleScope);
  if (!Utils::OpenHandle(self)->IsJSObject()) return Just(false);
  i::Handle<i::JSObject> obj =
      i::Handle<i::JSObject>::cast(Utils::OpenHandle(self));
  v8::Local<AccessorSignature> signature;
  i::Handle<i::AccessorInfo> info =
      MakeAccessorInfo(isolate, name, getter, setter, data, settings, signature,
                       is_special_data_property, false);
  if (info.is_null()) return Nothing<bool>();
  bool fast = obj->HasFastProperties();
  i::Handle<i::Object> result;

  i::Handle<i::Name> accessor_name(info->name(), isolate);
  i::PropertyAttributes attrs = static_cast<i::PropertyAttributes>(attributes);
  has_pending_exception =
      !i::JSObject::SetAccessor(obj, accessor_name, info, attrs)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  if (result->IsUndefined(obj->GetIsolate())) return Just(false);
  if (fast) {
    i::JSObject::MigrateSlowToFast(obj, 0, "APISetAccessor");
  }
  return Just(true);
}


Maybe<bool> Object::SetAccessor(Local<Context> context, Local<Name> name,
                                AccessorNameGetterCallback getter,
                                AccessorNameSetterCallback setter,
                                MaybeLocal<Value> data, AccessControl settings,
                                PropertyAttribute attribute) {
  return ObjectSetAccessor(context, this, name, getter, setter,
                           data.FromMaybe(Local<Value>()), settings, attribute,
                           i::FLAG_disable_old_api_accessors);
}


bool Object::SetAccessor(Local<String> name, AccessorGetterCallback getter,
                         AccessorSetterCallback setter, v8::Local<Value> data,
                         AccessControl settings, PropertyAttribute attributes) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return ObjectSetAccessor(context, this, name, getter, setter, data, settings,
                           attributes, i::FLAG_disable_old_api_accessors)
      .FromMaybe(false);
}


bool Object::SetAccessor(Local<Name> name, AccessorNameGetterCallback getter,
                         AccessorNameSetterCallback setter,
                         v8::Local<Value> data, AccessControl settings,
                         PropertyAttribute attributes) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return ObjectSetAccessor(context, this, name, getter, setter, data, settings,
                           attributes, i::FLAG_disable_old_api_accessors)
      .FromMaybe(false);
}


void Object::SetAccessorProperty(Local<Name> name, Local<Function> getter,
                                 Local<Function> setter,
                                 PropertyAttribute attribute,
                                 AccessControl settings) {
  // TODO(verwaest): Remove |settings|.
  DCHECK_EQ(v8::DEFAULT, settings);
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope scope(isolate);
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return;
  i::Handle<i::Object> getter_i = v8::Utils::OpenHandle(*getter);
  i::Handle<i::Object> setter_i = v8::Utils::OpenHandle(*setter, true);
  if (setter_i.is_null()) setter_i = isolate->factory()->null_value();
  i::JSObject::DefineAccessor(i::Handle<i::JSObject>::cast(self),
                              v8::Utils::OpenHandle(*name), getter_i, setter_i,
                              static_cast<i::PropertyAttributes>(attribute));
}

Maybe<bool> Object::SetNativeDataProperty(v8::Local<v8::Context> context,
                                          v8::Local<Name> name,
                                          AccessorNameGetterCallback getter,
                                          AccessorNameSetterCallback setter,
                                          v8::Local<Value> data,
                                          PropertyAttribute attributes) {
  return ObjectSetAccessor(context, this, name, getter, setter, data, DEFAULT,
                           attributes, true);
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

bool v8::Object::HasOwnProperty(Local<String> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return HasOwnProperty(context, key).FromMaybe(false);
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


bool v8::Object::HasRealNamedProperty(Local<String> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return HasRealNamedProperty(context, key).FromMaybe(false);
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


bool v8::Object::HasRealIndexedProperty(uint32_t index) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return HasRealIndexedProperty(context, index).FromMaybe(false);
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


bool v8::Object::HasRealNamedCallbackProperty(Local<String> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return HasRealNamedCallbackProperty(context, key).FromMaybe(false);
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
  i::LookupIterator it = i::LookupIterator::PropertyOrElement(
      isolate, self, key_obj, proto,
      i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(i::Object::GetProperty(&it), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!it.IsFound()) return MaybeLocal<Value>();
  RETURN_ESCAPED(result);
}


Local<Value> v8::Object::GetRealNamedPropertyInPrototypeChain(
    Local<String> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(GetRealNamedPropertyInPrototypeChain(context, key),
                            Value);
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
  i::LookupIterator it = i::LookupIterator::PropertyOrElement(
      isolate, self, key_obj, proto,
      i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Maybe<i::PropertyAttributes> result =
      i::JSReceiver::GetPropertyAttributes(&it);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(PropertyAttribute);
  if (!it.IsFound()) return Nothing<PropertyAttribute>();
  if (result.FromJust() == i::ABSENT) return Just(None);
  return Just(static_cast<PropertyAttribute>(result.FromJust()));
}


Maybe<PropertyAttribute>
v8::Object::GetRealNamedPropertyAttributesInPrototypeChain(Local<String> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return GetRealNamedPropertyAttributesInPrototypeChain(context, key);
}


MaybeLocal<Value> v8::Object::GetRealNamedProperty(Local<Context> context,
                                                   Local<Name> key) {
  PREPARE_FOR_EXECUTION(context, Object, GetRealNamedProperty, Value);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  i::LookupIterator it = i::LookupIterator::PropertyOrElement(
      isolate, self, key_obj, self,
      i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(i::Object::GetProperty(&it), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!it.IsFound()) return MaybeLocal<Value>();
  RETURN_ESCAPED(result);
}


Local<Value> v8::Object::GetRealNamedProperty(Local<String> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(GetRealNamedProperty(context, key), Value);
}


Maybe<PropertyAttribute> v8::Object::GetRealNamedPropertyAttributes(
    Local<Context> context, Local<Name> key) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(isolate, context, Object, GetRealNamedPropertyAttributes,
                     Nothing<PropertyAttribute>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  i::LookupIterator it = i::LookupIterator::PropertyOrElement(
      isolate, self, key_obj, self,
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


Maybe<PropertyAttribute> v8::Object::GetRealNamedPropertyAttributes(
    Local<String> key) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  return GetRealNamedPropertyAttributes(context, key);
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
  return Utils::ToLocal(self->GetCreationContext());
}


int v8::Object::GetIdentityHash() {
  i::DisallowHeapAllocation no_gc;
  auto isolate = Utils::OpenHandle(this)->GetIsolate();
  i::HandleScope scope(isolate);
  auto self = Utils::OpenHandle(this);
  return self->GetOrCreateIdentityHash(isolate)->value();
}


bool v8::Object::IsCallable() {
  auto self = Utils::OpenHandle(this);
  return self->IsCallable();
}

bool v8::Object::IsConstructor() {
  auto self = Utils::OpenHandle(this);
  return self->IsConstructor();
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
  STATIC_ASSERT(sizeof(v8::Local<v8::Value>) == sizeof(i::Object**));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::Call(isolate, self, recv_obj, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}


Local<v8::Value> Object::CallAsFunction(v8::Local<v8::Value> recv, int argc,
                                        v8::Local<v8::Value> argv[]) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  Local<Value>* argv_cast = reinterpret_cast<Local<Value>*>(argv);
  RETURN_TO_LOCAL_UNCHECKED(CallAsFunction(context, recv, argc, argv_cast),
                            Value);
}


MaybeLocal<Value> Object::CallAsConstructor(Local<Context> context, int argc,
                                            Local<Value> argv[]) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  ENTER_V8(isolate, context, Object, CallAsConstructor, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(isolate);
  auto self = Utils::OpenHandle(this);
  STATIC_ASSERT(sizeof(v8::Local<v8::Value>) == sizeof(i::Object**));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::New(isolate, self, self, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}


Local<v8::Value> Object::CallAsConstructor(int argc,
                                           v8::Local<v8::Value> argv[]) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  Local<Value>* argv_cast = reinterpret_cast<Local<Value>*>(argv);
  RETURN_TO_LOCAL_UNCHECKED(CallAsConstructor(context, argc, argv_cast), Value);
}

MaybeLocal<Function> Function::New(Local<Context> context,
                                   FunctionCallback callback, Local<Value> data,
                                   int length, ConstructorBehavior behavior) {
  i::Isolate* isolate = Utils::OpenHandle(*context)->GetIsolate();
  LOG_API(isolate, Function, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  auto templ = FunctionTemplateNew(isolate, callback, data, Local<Signature>(),
                                   length, true);
  if (behavior == ConstructorBehavior::kThrow) templ->RemovePrototype();
  return templ->GetFunction(context);
}


Local<Function> Function::New(Isolate* v8_isolate, FunctionCallback callback,
                              Local<Value> data, int length) {
  return Function::New(v8_isolate->GetCurrentContext(), callback, data, length,
                       ConstructorBehavior::kAllow)
      .FromMaybe(Local<Function>());
}


Local<v8::Object> Function::NewInstance() const {
  return NewInstance(Isolate::GetCurrent()->GetCurrentContext(), 0, nullptr)
      .FromMaybe(Local<Object>());
}


MaybeLocal<Object> Function::NewInstance(Local<Context> context, int argc,
                                         v8::Local<v8::Value> argv[]) const {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.Execute");
  ENTER_V8(isolate, context, Function, NewInstance, MaybeLocal<Object>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(isolate);
  auto self = Utils::OpenHandle(this);
  STATIC_ASSERT(sizeof(v8::Local<v8::Value>) == sizeof(i::Object**));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Object> result;
  has_pending_exception = !ToLocal<Object>(
      i::Execution::New(isolate, self, self, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}


Local<v8::Object> Function::NewInstance(int argc,
                                        v8::Local<v8::Value> argv[]) const {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(NewInstance(context, argc, argv), Object);
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
  i::Handle<i::Object> recv_obj = Utils::OpenHandle(*recv);
  STATIC_ASSERT(sizeof(v8::Local<v8::Value>) == sizeof(i::Object**));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::Call(isolate, self, recv_obj, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}


Local<v8::Value> Function::Call(v8::Local<v8::Value> recv, int argc,
                                v8::Local<v8::Value> argv[]) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(Call(context, recv, argc, argv), Value);
}


void Function::SetName(v8::Local<v8::String> name) {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) return;
  auto func = i::Handle<i::JSFunction>::cast(self);
  func->shared()->set_name(*Utils::OpenHandle(*name));
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
    return Utils::ToLocal(handle(func->shared()->name(), isolate));
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
  return Utils::ToLocal(i::Handle<i::Object>(func->shared()->inferred_name(),
                                             func->GetIsolate()));
}


Local<Value> Function::GetDebugName() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return ToApiHandle<Primitive>(
        self->GetIsolate()->factory()->undefined_value());
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  i::Handle<i::String> name = i::JSFunction::GetDebugName(func);
  return Utils::ToLocal(i::Handle<i::Object>(*name, name->GetIsolate()));
}


Local<Value> Function::GetDisplayName() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return ToApiHandle<Primitive>(isolate->factory()->undefined_value());
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  i::Handle<i::String> property_name =
      isolate->factory()->NewStringFromStaticChars("displayName");
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
  if (func->shared()->script()->IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
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
  if (func->shared()->script()->IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
    return i::Script::GetLineNumber(script, func->shared()->start_position());
  }
  return kLineOffsetNotFound;
}


int Function::GetScriptColumnNumber() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return kLineOffsetNotFound;
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  if (func->shared()->script()->IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
    return i::Script::GetColumnNumber(script, func->shared()->start_position());
  }
  return kLineOffsetNotFound;
}


bool Function::IsBuiltin() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return false;
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  return !func->shared()->IsUserJavaScript();
}


int Function::ScriptId() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) {
    return v8::UnboundScript::kNoScriptId;
  }
  auto func = i::Handle<i::JSFunction>::cast(self);
  if (!func->shared()->script()->IsScript()) {
    return v8::UnboundScript::kNoScriptId;
  }
  i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
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
  return str->HasOnlyOneByteChars();
}


// Helpers for ContainsOnlyOneByteHelper
template<size_t size> struct OneByteMask;
template<> struct OneByteMask<4> {
  static const uint32_t value = 0xFF00FF00;
};
template<> struct OneByteMask<8> {
  static const uint64_t value = V8_2PART_UINT64_C(0xFF00FF00, FF00FF00);
};
static const uintptr_t kOneByteMask = OneByteMask<sizeof(uintptr_t)>::value;
static const uintptr_t kAlignmentMask = sizeof(uintptr_t) - 1;
static inline bool Unaligned(const uint16_t* chars) {
  return reinterpret_cast<const uintptr_t>(chars) & kAlignmentMask;
}


static inline const uint16_t* Align(const uint16_t* chars) {
  return reinterpret_cast<uint16_t*>(
      reinterpret_cast<uintptr_t>(chars) & ~kAlignmentMask);
}

class ContainsOnlyOneByteHelper {
 public:
  ContainsOnlyOneByteHelper() : is_one_byte_(true) {}
  bool Check(i::String* string) {
    i::ConsString* cons_string = i::String::VisitFlat(this, string, 0);
    if (cons_string == nullptr) return is_one_byte_;
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
    const int increment = sizeof(uintptr_t)/sizeof(uint16_t);
    const int inner_loops = 16;
    while (chars + inner_loops*increment < aligned_end) {
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
  bool CheckCons(i::ConsString* cons_string) {
    while (true) {
      // Check left side if flat.
      i::String* left = cons_string->first();
      i::ConsString* left_as_cons =
          i::String::VisitFlat(this, left, 0);
      if (!is_one_byte_) return false;
      // Check right side if flat.
      i::String* right = cons_string->second();
      i::ConsString* right_as_cons =
          i::String::VisitFlat(this, right, 0);
      if (!is_one_byte_) return false;
      // Standard recurse/iterate trick.
      if (left_as_cons != nullptr && right_as_cons != nullptr) {
        if (left->length() < right->length()) {
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
      if (left_as_cons != nullptr) {
        cons_string = left_as_cons;
        continue;
      }
      // Descend right in place.
      if (right_as_cons != nullptr) {
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
  if (str->HasOnlyOneByteChars()) return true;
  ContainsOnlyOneByteHelper helper;
  return helper.Check(*str);
}


class Utf8LengthHelper : public i::AllStatic {
 public:
  enum State {
    kEndsWithLeadingSurrogate = 1 << 0,
    kStartsWithTrailingSurrogate = 1 << 1,
    kLeftmostEdgeIsCalculated = 1 << 2,
    kRightmostEdgeIsCalculated = 1 << 3,
    kLeftmostEdgeIsSurrogate = 1 << 4,
    kRightmostEdgeIsSurrogate = 1 << 5
  };

  static const uint8_t kInitialState = 0;

  static inline bool EndsWithSurrogate(uint8_t state) {
    return state & kEndsWithLeadingSurrogate;
  }

  static inline bool StartsWithSurrogate(uint8_t state) {
    return state & kStartsWithTrailingSurrogate;
  }

  class Visitor {
   public:
    Visitor() : utf8_length_(0), state_(kInitialState) {}

    void VisitOneByteString(const uint8_t* chars, int length) {
      int utf8_length = 0;
      // Add in length 1 for each non-Latin1 character.
      for (int i = 0; i < length; i++) {
        utf8_length += *chars++ >> 7;
      }
      // Add in length 1 for each character.
      utf8_length_ = utf8_length + length;
      state_ = kInitialState;
    }

    void VisitTwoByteString(const uint16_t* chars, int length) {
      int utf8_length = 0;
      int last_character = unibrow::Utf16::kNoPreviousCharacter;
      for (int i = 0; i < length; i++) {
        uint16_t c = chars[i];
        utf8_length += unibrow::Utf8::Length(c, last_character);
        last_character = c;
      }
      utf8_length_ = utf8_length;
      uint8_t state = 0;
      if (unibrow::Utf16::IsTrailSurrogate(chars[0])) {
        state |= kStartsWithTrailingSurrogate;
      }
      if (unibrow::Utf16::IsLeadSurrogate(chars[length-1])) {
        state |= kEndsWithLeadingSurrogate;
      }
      state_ = state;
    }

    static i::ConsString* VisitFlat(i::String* string,
                                    int* length,
                                    uint8_t* state) {
      Visitor visitor;
      i::ConsString* cons_string = i::String::VisitFlat(&visitor, string);
      *length = visitor.utf8_length_;
      *state = visitor.state_;
      return cons_string;
    }

   private:
    int utf8_length_;
    uint8_t state_;
    DISALLOW_COPY_AND_ASSIGN(Visitor);
  };

  static inline void MergeLeafLeft(int* length,
                                   uint8_t* state,
                                   uint8_t leaf_state) {
    bool edge_surrogate = StartsWithSurrogate(leaf_state);
    if (!(*state & kLeftmostEdgeIsCalculated)) {
      DCHECK(!(*state & kLeftmostEdgeIsSurrogate));
      *state |= kLeftmostEdgeIsCalculated
          | (edge_surrogate ? kLeftmostEdgeIsSurrogate : 0);
    } else if (EndsWithSurrogate(*state) && edge_surrogate) {
      *length -= unibrow::Utf8::kBytesSavedByCombiningSurrogates;
    }
    if (EndsWithSurrogate(leaf_state)) {
      *state |= kEndsWithLeadingSurrogate;
    } else {
      *state &= ~kEndsWithLeadingSurrogate;
    }
  }

  static inline void MergeLeafRight(int* length,
                                    uint8_t* state,
                                    uint8_t leaf_state) {
    bool edge_surrogate = EndsWithSurrogate(leaf_state);
    if (!(*state & kRightmostEdgeIsCalculated)) {
      DCHECK(!(*state & kRightmostEdgeIsSurrogate));
      *state |= (kRightmostEdgeIsCalculated
                 | (edge_surrogate ? kRightmostEdgeIsSurrogate : 0));
    } else if (edge_surrogate && StartsWithSurrogate(*state)) {
      *length -= unibrow::Utf8::kBytesSavedByCombiningSurrogates;
    }
    if (StartsWithSurrogate(leaf_state)) {
      *state |= kStartsWithTrailingSurrogate;
    } else {
      *state &= ~kStartsWithTrailingSurrogate;
    }
  }

  static inline void MergeTerminal(int* length,
                                   uint8_t state,
                                   uint8_t* state_out) {
    DCHECK((state & kLeftmostEdgeIsCalculated) &&
           (state & kRightmostEdgeIsCalculated));
    if (EndsWithSurrogate(state) && StartsWithSurrogate(state)) {
      *length -= unibrow::Utf8::kBytesSavedByCombiningSurrogates;
    }
    *state_out = kInitialState |
        (state & kLeftmostEdgeIsSurrogate ? kStartsWithTrailingSurrogate : 0) |
        (state & kRightmostEdgeIsSurrogate ? kEndsWithLeadingSurrogate : 0);
  }

  static int Calculate(i::ConsString* current, uint8_t* state_out) {
    using internal::ConsString;
    int total_length = 0;
    uint8_t state = kInitialState;
    while (true) {
      i::String* left = current->first();
      i::String* right = current->second();
      uint8_t right_leaf_state;
      uint8_t left_leaf_state;
      int leaf_length;
      ConsString* left_as_cons =
          Visitor::VisitFlat(left, &leaf_length, &left_leaf_state);
      if (left_as_cons == nullptr) {
        total_length += leaf_length;
        MergeLeafLeft(&total_length, &state, left_leaf_state);
      }
      ConsString* right_as_cons =
          Visitor::VisitFlat(right, &leaf_length, &right_leaf_state);
      if (right_as_cons == nullptr) {
        total_length += leaf_length;
        MergeLeafRight(&total_length, &state, right_leaf_state);
        if (left_as_cons != nullptr) {
          // 1 Leaf node. Descend in place.
          current = left_as_cons;
          continue;
        } else {
          // Terminal node.
          MergeTerminal(&total_length, state, state_out);
          return total_length;
        }
      } else if (left_as_cons == nullptr) {
        // 1 Leaf node. Descend in place.
        current = right_as_cons;
        continue;
      }
      // Both strings are ConsStrings.
      // Recurse on smallest.
      if (left->length() < right->length()) {
        total_length += Calculate(left_as_cons, &left_leaf_state);
        MergeLeafLeft(&total_length, &state, left_leaf_state);
        current = right_as_cons;
      } else {
        total_length += Calculate(right_as_cons, &right_leaf_state);
        MergeLeafRight(&total_length, &state, right_leaf_state);
        current = left_as_cons;
      }
    }
    UNREACHABLE();
  }

  static inline int Calculate(i::ConsString* current) {
    uint8_t state = kInitialState;
    return Calculate(current, &state);
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Utf8LengthHelper);
};


static int Utf8Length(i::String* str, i::Isolate* isolate) {
  int length = str->length();
  if (length == 0) return 0;
  uint8_t state;
  i::ConsString* cons_string =
      Utf8LengthHelper::Visitor::VisitFlat(str, &length, &state);
  if (cons_string == nullptr) return length;
  return Utf8LengthHelper::Calculate(cons_string);
}


int String::Utf8Length() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  i::Isolate* isolate = str->GetIsolate();
  return v8::Utf8Length(*str, isolate);
}


class Utf8WriterVisitor {
 public:
  Utf8WriterVisitor(
      char* buffer,
      int capacity,
      bool skip_capacity_check,
      bool replace_invalid_utf8)
    : early_termination_(false),
      last_character_(unibrow::Utf16::kNoPreviousCharacter),
      buffer_(buffer),
      start_(buffer),
      capacity_(capacity),
      skip_capacity_check_(capacity == -1 || skip_capacity_check),
      replace_invalid_utf8_(replace_invalid_utf8),
      utf16_chars_read_(0) {
  }

  static int WriteEndCharacter(uint16_t character,
                               int last_character,
                               int remaining,
                               char* const buffer,
                               bool replace_invalid_utf8) {
    DCHECK_GT(remaining, 0);
    // We can't use a local buffer here because Encode needs to modify
    // previous characters in the stream.  We know, however, that
    // exactly one character will be advanced.
    if (unibrow::Utf16::IsSurrogatePair(last_character, character)) {
      int written = unibrow::Utf8::Encode(buffer, character, last_character,
                                          replace_invalid_utf8);
      DCHECK_EQ(written, 1);
      return written;
    }
    // Use a scratch buffer to check the required characters.
    char temp_buffer[unibrow::Utf8::kMaxEncodedSize];
    // Can't encode using last_character as gcc has array bounds issues.
    int written = unibrow::Utf8::Encode(temp_buffer, character,
                                        unibrow::Utf16::kNoPreviousCharacter,
                                        replace_invalid_utf8);
    // Won't fit.
    if (written > remaining) return 0;
    // Copy over the character from temp_buffer.
    for (int j = 0; j < written; j++) {
      buffer[j] = temp_buffer[j];
    }
    return written;
  }

  // Visit writes out a group of code units (chars) of a v8::String to the
  // internal buffer_. This is done in two phases. The first phase calculates a
  // pesimistic estimate (writable_length) on how many code units can be safely
  // written without exceeding the buffer capacity and without writing the last
  // code unit (it could be a lead surrogate). The estimated number of code
  // units is then written out in one go, and the reported byte usage is used
  // to correct the estimate. This is repeated until the estimate becomes <= 0
  // or all code units have been written out. The second phase writes out code
  // units until the buffer capacity is reached, would be exceeded by the next
  // unit, or all units have been written out.
  template<typename Char>
  void Visit(const Char* chars, const int length) {
    DCHECK(!early_termination_);
    if (length == 0) return;
    // Copy state to stack.
    char* buffer = buffer_;
    int last_character = sizeof(Char) == 1
                             ? unibrow::Utf16::kNoPreviousCharacter
                             : last_character_;
    int i = 0;
    // Do a fast loop where there is no exit capacity check.
    while (true) {
      int fast_length;
      if (skip_capacity_check_) {
        fast_length = length;
      } else {
        int remaining_capacity = capacity_ - static_cast<int>(buffer - start_);
        // Need enough space to write everything but one character.
        STATIC_ASSERT(unibrow::Utf16::kMaxExtraUtf8BytesForOneUtf16CodeUnit ==
                      3);
        int max_size_per_char =  sizeof(Char) == 1 ? 2 : 3;
        int writable_length =
            (remaining_capacity - max_size_per_char)/max_size_per_char;
        // Need to drop into slow loop.
        if (writable_length <= 0) break;
        fast_length = i + writable_length;
        if (fast_length > length) fast_length = length;
      }
      // Write the characters to the stream.
      if (sizeof(Char) == 1) {
        for (; i < fast_length; i++) {
          buffer += unibrow::Utf8::EncodeOneByte(
              buffer, static_cast<uint8_t>(*chars++));
          DCHECK(capacity_ == -1 || (buffer - start_) <= capacity_);
        }
      } else {
        for (; i < fast_length; i++) {
          uint16_t character = *chars++;
          buffer += unibrow::Utf8::Encode(buffer, character, last_character,
                                          replace_invalid_utf8_);
          last_character = character;
          DCHECK(capacity_ == -1 || (buffer - start_) <= capacity_);
        }
      }
      // Array is fully written. Exit.
      if (fast_length == length) {
        // Write state back out to object.
        last_character_ = last_character;
        buffer_ = buffer;
        utf16_chars_read_ += length;
        return;
      }
    }
    DCHECK(!skip_capacity_check_);
    // Slow loop. Must check capacity on each iteration.
    int remaining_capacity = capacity_ - static_cast<int>(buffer - start_);
    DCHECK_GE(remaining_capacity, 0);
    for (; i < length && remaining_capacity > 0; i++) {
      uint16_t character = *chars++;
      // remaining_capacity is <= 3 bytes at this point, so we do not write out
      // an umatched lead surrogate.
      if (replace_invalid_utf8_ && unibrow::Utf16::IsLeadSurrogate(character)) {
        early_termination_ = true;
        break;
      }
      int written = WriteEndCharacter(character,
                                      last_character,
                                      remaining_capacity,
                                      buffer,
                                      replace_invalid_utf8_);
      if (written == 0) {
        early_termination_ = true;
        break;
      }
      buffer += written;
      remaining_capacity -= written;
      last_character = character;
    }
    // Write state back out to object.
    last_character_ = last_character;
    buffer_ = buffer;
    utf16_chars_read_ += i;
  }

  inline bool IsDone() {
    return early_termination_;
  }

  inline void VisitOneByteString(const uint8_t* chars, int length) {
    Visit(chars, length);
  }

  inline void VisitTwoByteString(const uint16_t* chars, int length) {
    Visit(chars, length);
  }

  int CompleteWrite(bool write_null, int* utf16_chars_read_out) {
    // Write out number of utf16 characters written to the stream.
    if (utf16_chars_read_out != nullptr) {
      *utf16_chars_read_out = utf16_chars_read_;
    }
    // Only null terminate if all of the string was written and there's space.
    if (write_null &&
        !early_termination_ &&
        (capacity_ == -1 || (buffer_ - start_) < capacity_)) {
      *buffer_++ = '\0';
    }
    return static_cast<int>(buffer_ - start_);
  }

 private:
  bool early_termination_;
  int last_character_;
  char* buffer_;
  char* const start_;
  int capacity_;
  bool const skip_capacity_check_;
  bool const replace_invalid_utf8_;
  int utf16_chars_read_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(Utf8WriterVisitor);
};


static bool RecursivelySerializeToUtf8(i::String* current,
                                       Utf8WriterVisitor* writer,
                                       int recursion_budget) {
  while (!writer->IsDone()) {
    i::ConsString* cons_string = i::String::VisitFlat(writer, current);
    if (cons_string == nullptr) return true;  // Leaf node.
    if (recursion_budget <= 0) return false;
    // Must write the left branch first.
    i::String* first = cons_string->first();
    bool success = RecursivelySerializeToUtf8(first,
                                              writer,
                                              recursion_budget - 1);
    if (!success) return false;
    // Inline tail recurse for right branch.
    current = cons_string->second();
  }
  return true;
}


int String::WriteUtf8(char* buffer,
                      int capacity,
                      int* nchars_ref,
                      int options) const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  i::Isolate* isolate = str->GetIsolate();
  LOG_API(isolate, String, WriteUtf8);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (options & HINT_MANY_WRITES_EXPECTED) {
    str = i::String::Flatten(str);  // Flatten the string for efficiency.
  }
  const int string_length = str->length();
  bool write_null = !(options & NO_NULL_TERMINATION);
  bool replace_invalid_utf8 = (options & REPLACE_INVALID_UTF8);
  int max16BitCodeUnitSize = unibrow::Utf8::kMax16BitCodeUnitSize;
  // First check if we can just write the string without checking capacity.
  if (capacity == -1 || capacity / max16BitCodeUnitSize >= string_length) {
    Utf8WriterVisitor writer(buffer, capacity, true, replace_invalid_utf8);
    const int kMaxRecursion = 100;
    bool success = RecursivelySerializeToUtf8(*str, &writer, kMaxRecursion);
    if (success) return writer.CompleteWrite(write_null, nchars_ref);
  } else if (capacity >= string_length) {
    // First check that the buffer is large enough.
    int utf8_bytes = v8::Utf8Length(*str, isolate);
    if (utf8_bytes <= capacity) {
      // one-byte fast path.
      if (utf8_bytes == string_length) {
        WriteOneByte(reinterpret_cast<uint8_t*>(buffer), 0, capacity, options);
        if (nchars_ref != nullptr) *nchars_ref = string_length;
        if (write_null && (utf8_bytes+1 <= capacity)) {
          return string_length + 1;
        }
        return string_length;
      }
      if (write_null && (utf8_bytes+1 > capacity)) {
        options |= NO_NULL_TERMINATION;
      }
      // Recurse once without a capacity limit.
      // This will get into the first branch above.
      // TODO(dcarney) Check max left rec. in Utf8Length and fall through.
      return WriteUtf8(buffer, -1, nchars_ref, options);
    }
  }
  // Recursive slow path can potentially be unreasonable slow. Flatten.
  str = i::String::Flatten(str);
  Utf8WriterVisitor writer(buffer, capacity, false, replace_invalid_utf8);
  i::String::VisitFlat(&writer, *str);
  return writer.CompleteWrite(write_null, nchars_ref);
}


template<typename CharType>
static inline int WriteHelper(const String* string,
                              CharType* buffer,
                              int start,
                              int length,
                              int options) {
  i::Isolate* isolate = Utils::OpenHandle(string)->GetIsolate();
  LOG_API(isolate, String, Write);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  DCHECK(start >= 0 && length >= -1);
  i::Handle<i::String> str = Utils::OpenHandle(string);
  if (options & String::HINT_MANY_WRITES_EXPECTED) {
    // Flatten the string for efficiency.  This applies whether we are
    // using StringCharacterStream or Get(i) to access the characters.
    str = i::String::Flatten(str);
  }
  int end = start + length;
  if ((length == -1) || (length > str->length() - start) )
    end = str->length();
  if (end < 0) return 0;
  i::String::WriteToFlat(*str, buffer, start, end);
  if (!(options & String::NO_NULL_TERMINATION) &&
      (length == -1 || end - start < length)) {
    buffer[end - start] = '\0';
  }
  return end - start;
}


int String::WriteOneByte(uint8_t* buffer,
                         int start,
                         int length,
                         int options) const {
  return WriteHelper(this, buffer, start, length, options);
}


int String::Write(uint16_t* buffer,
                  int start,
                  int length,
                  int options) const {
  return WriteHelper(this, buffer, start, length, options);
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
  i::Handle<i::String> str = Utils::OpenHandle(this);
  const v8::String::ExternalStringResource* expected;
  if (i::StringShape(*str).IsExternalTwoByte()) {
    const void* resource =
        i::Handle<i::ExternalTwoByteString>::cast(str)->resource();
    expected = reinterpret_cast<const ExternalStringResource*>(resource);
  } else {
    expected = nullptr;
  }
  CHECK_EQ(expected, value);
}

void v8::String::VerifyExternalStringResourceBase(
    v8::String::ExternalStringResourceBase* value, Encoding encoding) const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  const v8::String::ExternalStringResourceBase* expected;
  Encoding expectedEncoding;
  if (i::StringShape(*str).IsExternalOneByte()) {
    const void* resource =
        i::Handle<i::ExternalOneByteString>::cast(str)->resource();
    expected = reinterpret_cast<const ExternalStringResourceBase*>(resource);
    expectedEncoding = ONE_BYTE_ENCODING;
  } else if (i::StringShape(*str).IsExternalTwoByte()) {
    const void* resource =
        i::Handle<i::ExternalTwoByteString>::cast(str)->resource();
    expected = reinterpret_cast<const ExternalStringResourceBase*>(resource);
    expectedEncoding = TWO_BYTE_ENCODING;
  } else {
    expected = nullptr;
    expectedEncoding =
        str->IsOneByteRepresentation() ? ONE_BYTE_ENCODING : TWO_BYTE_ENCODING;
  }
  CHECK_EQ(expected, value);
  CHECK_EQ(expectedEncoding, encoding);
}

const v8::String::ExternalOneByteStringResource*
v8::String::GetExternalOneByteStringResource() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (i::StringShape(*str).IsExternalOneByte()) {
    const void* resource =
        i::Handle<i::ExternalOneByteString>::cast(str)->resource();
    return reinterpret_cast<const ExternalOneByteStringResource*>(resource);
  } else {
    return nullptr;
  }
}


Local<Value> Symbol::Name() const {
  i::Handle<i::Symbol> sym = Utils::OpenHandle(this);
  i::Handle<i::Object> name(sym->name(), sym->GetIsolate());
  return Utils::ToLocal(name);
}


Local<Value> Private::Name() const {
  return reinterpret_cast<const Symbol*>(this)->Name();
}


double Number::Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->Number();
}


bool Boolean::Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsTrue(i::HeapObject::cast(*obj)->GetIsolate());
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
  i::Handle<i::Object> value(
      i::Handle<i::JSObject>::cast(obj)->GetEmbedderField(index),
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
  return DecodeSmiToAligned(
      i::Handle<i::JSObject>::cast(obj)->GetEmbedderField(index), location);
}

void v8::Object::SetAlignedPointerInInternalField(int index, void* value) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::SetAlignedPointerInInternalField()";
  if (!InternalFieldOK(obj, index, location)) return;
  i::Handle<i::JSObject>::cast(obj)->SetEmbedderField(
      index, EncodeAlignedAsSmi(value, location));
  DCHECK_EQ(value, GetAlignedPointerFromInternalField(index));
}

void v8::Object::SetAlignedPointerInInternalFields(int argc, int indices[],
                                                   void* values[]) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::SetAlignedPointerInInternalFields()";
  i::DisallowHeapAllocation no_gc;
  i::JSObject* object = i::JSObject::cast(*obj);
  int nof_embedder_fields = object->GetEmbedderFieldCount();
  for (int i = 0; i < argc; i++) {
    int index = indices[i];
    if (!Utils::ApiCheck(index < nof_embedder_fields, location,
                         "Internal field out of bounds")) {
      return;
    }
    void* value = values[i];
    object->SetEmbedderField(index, EncodeAlignedAsSmi(value, location));
    DCHECK_EQ(value, GetAlignedPointerFromInternalField(index));
  }
}

static void* ExternalValue(i::Object* obj) {
  // Obscure semantics for undefined, but somehow checked in our unit tests...
  if (!obj->IsSmi() &&
      obj->IsUndefined(i::HeapObject::cast(obj)->GetIsolate())) {
    return nullptr;
  }
  i::Object* foreign = i::JSObject::cast(obj)->GetEmbedderField(0);
  return i::Foreign::cast(foreign)->foreign_address();
}


// --- E n v i r o n m e n t ---


void v8::V8::InitializePlatform(Platform* platform) {
  i::V8::InitializePlatform(platform);
}


void v8::V8::ShutdownPlatform() {
  i::V8::ShutdownPlatform();
}


bool v8::V8::Initialize() {
  i::V8::Initialize();
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  i::ReadNatives();
#endif
  return true;
}

#if V8_OS_POSIX
bool V8::TryHandleSignal(int signum, void* info, void* context) {
#if V8_OS_LINUX && V8_TARGET_ARCH_X64 && !V8_OS_ANDROID
  return v8::internal::trap_handler::TryHandleSignal(
      signum, static_cast<siginfo_t*>(info), static_cast<ucontext_t*>(context));
#else  // V8_OS_LINUX && V8_TARGET_ARCH_X64 && !V8_OS_ANDROID
  return false;
#endif
}
#endif

bool V8::RegisterDefaultSignalHandler() {
  return v8::internal::trap_handler::RegisterDefaultSignalHandler();
}

void v8::V8::SetEntropySource(EntropySource entropy_source) {
  base::RandomNumberGenerator::SetEntropySource(entropy_source);
}


void v8::V8::SetReturnAddressLocationResolver(
    ReturnAddressLocationResolver return_address_resolver) {
  i::StackFrame::SetReturnAddressLocationResolver(return_address_resolver);
}


bool v8::V8::Dispose() {
  i::V8::TearDown();
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  i::DisposeNatives();
#endif
  return true;
}

HeapStatistics::HeapStatistics()
    : total_heap_size_(0),
      total_heap_size_executable_(0),
      total_physical_size_(0),
      total_available_size_(0),
      used_heap_size_(0),
      heap_size_limit_(0),
      malloced_memory_(0),
      peak_malloced_memory_(0),
      does_zap_garbage_(0) {}

HeapSpaceStatistics::HeapSpaceStatistics(): space_name_(0),
                                            space_size_(0),
                                            space_used_size_(0),
                                            space_available_size_(0),
                                            physical_space_size_(0) { }


HeapObjectStatistics::HeapObjectStatistics()
    : object_type_(nullptr),
      object_sub_type_(nullptr),
      object_count_(0),
      object_size_(0) {}

HeapCodeStatistics::HeapCodeStatistics()
    : code_and_metadata_size_(0), bytecode_and_metadata_size_(0) {}

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


void v8::V8::InitializeExternalStartupData(const char* natives_blob,
                                           const char* snapshot_blob) {
  i::InitializeExternalStartupData(natives_blob, snapshot_blob);
}


const char* v8::V8::GetVersion() {
  return i::Version::GetVersion();
}

template <typename ObjectType>
struct InvokeBootstrapper;

template <>
struct InvokeBootstrapper<i::Context> {
  i::Handle<i::Context> Invoke(
      i::Isolate* isolate, i::MaybeHandle<i::JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
      v8::DeserializeInternalFieldsCallback embedder_fields_deserializer) {
    return isolate->bootstrapper()->CreateEnvironment(
        maybe_global_proxy, global_proxy_template, extensions,
        context_snapshot_index, embedder_fields_deserializer);
  }
};

template <>
struct InvokeBootstrapper<i::JSGlobalProxy> {
  i::Handle<i::JSGlobalProxy> Invoke(
      i::Isolate* isolate, i::MaybeHandle<i::JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
      v8::DeserializeInternalFieldsCallback embedder_fields_deserializer) {
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
    v8::DeserializeInternalFieldsCallback embedder_fields_deserializer) {
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
      proxy_template = ObjectTemplate::New(
          reinterpret_cast<v8::Isolate*>(isolate));
      proxy_constructor = EnsureConstructor(isolate, *proxy_template);

      // Set the global template to be the prototype template of
      // global proxy template.
      proxy_constructor->set_prototype_template(
          *Utils::OpenHandle(*global_template));

      proxy_template->SetInternalFieldCount(
          global_template->InternalFieldCount());

      // Migrate security handlers from global_template to
      // proxy_template.  Temporarily removing access check
      // information from the global template.
      if (!global_constructor->access_check_info()->IsUndefined(isolate)) {
        proxy_constructor->set_access_check_info(
            global_constructor->access_check_info());
        proxy_constructor->set_needs_access_check(
            global_constructor->needs_access_check());
        global_constructor->set_needs_access_check(false);
        global_constructor->set_access_check_info(
            isolate->heap()->undefined_value());
      }

      // Same for other interceptors. If the global constructor has
      // interceptors, we need to replace them temporarily with noop
      // interceptors, so the map is correctly marked as having interceptors,
      // but we don't invoke any.
      if (!global_constructor->named_property_handler()->IsUndefined(isolate)) {
        named_interceptor =
            handle(global_constructor->named_property_handler(), isolate);
        global_constructor->set_named_property_handler(
            isolate->heap()->noop_interceptor_info());
      }
      if (!global_constructor->indexed_property_handler()->IsUndefined(
              isolate)) {
        indexed_interceptor =
            handle(global_constructor->indexed_property_handler(), isolate);
        global_constructor->set_indexed_property_handler(
            isolate->heap()->noop_interceptor_info());
      }
    }

    i::MaybeHandle<i::JSGlobalProxy> maybe_proxy;
    if (!maybe_global_proxy.IsEmpty()) {
      maybe_proxy = i::Handle<i::JSGlobalProxy>::cast(
          Utils::OpenHandle(*maybe_global_proxy.ToLocalChecked()));
    }
    // Create the environment.
    InvokeBootstrapper<ObjectType> invoke;
    result =
        invoke.Invoke(isolate, maybe_proxy, proxy_template, extensions,
                      context_snapshot_index, embedder_fields_deserializer);

    // Restore the access check info and interceptors on the global template.
    if (!maybe_global_template.IsEmpty()) {
      DCHECK(!global_constructor.is_null());
      DCHECK(!proxy_constructor.is_null());
      global_constructor->set_access_check_info(
          proxy_constructor->access_check_info());
      global_constructor->set_needs_access_check(
          proxy_constructor->needs_access_check());
      global_constructor->set_named_property_handler(*named_interceptor);
      global_constructor->set_indexed_property_handler(*indexed_interceptor);
    }
  }
  // Leave V8.

  return result;
}

Local<Context> NewContext(
    v8::Isolate* external_isolate, v8::ExtensionConfiguration* extensions,
    v8::MaybeLocal<ObjectTemplate> global_template,
    v8::MaybeLocal<Value> global_object, size_t context_snapshot_index,
    v8::DeserializeInternalFieldsCallback embedder_fields_deserializer) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  // TODO(jkummerow): This is for crbug.com/713699. Remove it if it doesn't
  // fail.
  // Sanity-check that the isolate is initialized and usable.
  CHECK(isolate->builtins()->builtin(i::Builtins::kIllegal)->IsCode());

  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8", "V8.NewContext");
  LOG_API(isolate, Context, New);
  i::HandleScope scope(isolate);
  ExtensionConfiguration no_extensions;
  if (extensions == nullptr) extensions = &no_extensions;
  i::Handle<i::Context> env = CreateEnvironment<i::Context>(
      isolate, extensions, global_template, global_object,
      context_snapshot_index, embedder_fields_deserializer);
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
    DeserializeInternalFieldsCallback internal_fields_deserializer) {
  return NewContext(external_isolate, extensions, global_template,
                    global_object, 0, internal_fields_deserializer);
}

MaybeLocal<Context> v8::Context::FromSnapshot(
    v8::Isolate* external_isolate, size_t context_snapshot_index,
    v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
    v8::ExtensionConfiguration* extensions, MaybeLocal<Value> global_object) {
  size_t index_including_default_context = context_snapshot_index + 1;
  if (!i::Snapshot::HasContextSnapshot(
          reinterpret_cast<i::Isolate*>(external_isolate),
          index_including_default_context)) {
    return MaybeLocal<Context>();
  }
  return NewContext(external_isolate, extensions, MaybeLocal<ObjectTemplate>(),
                    global_object, index_including_default_context,
                    embedder_fields_deserializer);
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
      i::AccessCheckInfo::cast(global_constructor->access_check_info()),
      isolate);
  Utils::ApiCheck(access_check_info->named_interceptor() != nullptr,
                  "v8::Context::NewRemoteContext",
                  "Global template needs to have access check handlers.");
  i::Handle<i::JSGlobalProxy> global_proxy =
      CreateEnvironment<i::JSGlobalProxy>(isolate, nullptr, global_template,
                                          global_object, 0,
                                          DeserializeInternalFieldsCallback());
  if (global_proxy.is_null()) {
    if (isolate->has_pending_exception()) isolate->clear_pending_exception();
    return MaybeLocal<Object>();
  }
  return Utils::ToLocal(
      scope.CloseAndEscape(i::Handle<i::JSObject>::cast(global_proxy)));
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
  i::Object* security_token = env->security_token();
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
  if (i::Handle<i::JSGlobalProxy>::cast(
          global)->IsDetachedFrom(context->global_object())) {
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
      allow ? isolate->heap()->true_value() : isolate->heap()->false_value());
}


bool Context::IsCodeGenerationFromStringsAllowed() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  return !context->allow_code_gen_from_strings()->IsFalse(
      context->GetIsolate());
}


void Context::SetErrorMessageForCodeGenerationFromStrings(Local<String> error) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Handle<i::String> error_handle = Utils::OpenHandle(*error);
  context->set_error_message_for_code_gen_from_strings(*error_handle);
}

size_t Context::EstimatedSize() { return 0; }

MaybeLocal<v8::Object> ObjectTemplate::NewInstance(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, ObjectTemplate, NewInstance, Object);
  auto self = Utils::OpenHandle(this);
  Local<Object> result;
  has_pending_exception =
      !ToLocal<Object>(i::ApiNatives::InstantiateObject(self), &result);
  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}


Local<v8::Object> ObjectTemplate::NewInstance() {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(NewInstance(context), Object);
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


Local<v8::Function> FunctionTemplate::GetFunction() {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(GetFunction(context), Function);
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
      i::AccessCheckInfo::cast(constructor->access_check_info()), isolate);
  Utils::ApiCheck(access_check_info->named_interceptor() != nullptr,
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
    i::PrototypeIterator iter(i::JSObject::cast(*obj)->map());
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
  return i::StrLength(string);
}


inline int StringLength(const uint8_t* string) {
  return i::StrLength(reinterpret_cast<const char*>(string));
}


inline int StringLength(const uint16_t* string) {
  int length = 0;
  while (string[length] != '\0')
    length++;
  return length;
}


MUST_USE_RESULT
inline i::MaybeHandle<i::String> NewString(i::Factory* factory,
                                           v8::NewStringType type,
                                           i::Vector<const char> string) {
  if (type == v8::NewStringType::kInternalized) {
    return factory->InternalizeUtf8String(string);
  }
  return factory->NewStringFromUtf8(string);
}


MUST_USE_RESULT
inline i::MaybeHandle<i::String> NewString(i::Factory* factory,
                                           v8::NewStringType type,
                                           i::Vector<const uint8_t> string) {
  if (type == v8::NewStringType::kInternalized) {
    return factory->InternalizeOneByteString(string);
  }
  return factory->NewStringFromOneByte(string);
}


MUST_USE_RESULT
inline i::MaybeHandle<i::String> NewString(i::Factory* factory,
                                           v8::NewStringType type,
                                           i::Vector<const uint16_t> string) {
  if (type == v8::NewStringType::kInternalized) {
    return factory->InternalizeTwoByteString(string);
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

Local<String> String::NewFromUtf8(Isolate* isolate,
                                  const char* data,
                                  NewStringType type,
                                  int length) {
  NEW_STRING(isolate, String, NewFromUtf8, char, data,
             static_cast<v8::NewStringType>(type), length);
  RETURN_TO_LOCAL_UNCHECKED(result, String);
}


MaybeLocal<String> String::NewFromUtf8(Isolate* isolate, const char* data,
                                       v8::NewStringType type, int length) {
  NEW_STRING(isolate, String, NewFromUtf8, char, data, type, length);
  return result;
}


Local<String> String::NewFromOneByte(Isolate* isolate,
                                     const uint8_t* data,
                                     NewStringType type,
                                     int length) {
  NEW_STRING(isolate, String, NewFromOneByte, uint8_t, data,
             static_cast<v8::NewStringType>(type), length);
  RETURN_TO_LOCAL_UNCHECKED(result, String);
}


MaybeLocal<String> String::NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                          v8::NewStringType type, int length) {
  NEW_STRING(isolate, String, NewFromOneByte, uint8_t, data, type, length);
  return result;
}


Local<String> String::NewFromTwoByte(Isolate* isolate,
                                     const uint16_t* data,
                                     NewStringType type,
                                     int length) {
  NEW_STRING(isolate, String, NewFromTwoByte, uint16_t, data,
             static_cast<v8::NewStringType>(type), length);
  RETURN_TO_LOCAL_UNCHECKED(result, String);
}


MaybeLocal<String> String::NewFromTwoByte(Isolate* isolate,
                                          const uint16_t* data,
                                          v8::NewStringType type, int length) {
  NEW_STRING(isolate, String, NewFromTwoByte, uint16_t, data, type, length);
  return result;
}


Local<String> v8::String::Concat(Local<String> left, Local<String> right) {
  i::Handle<i::String> left_string = Utils::OpenHandle(*left);
  i::Isolate* isolate = left_string->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  LOG_API(isolate, String, Concat);
  i::Handle<i::String> right_string = Utils::OpenHandle(*right);
  // If we are steering towards a range error, do not wait for the error to be
  // thrown, and return the null handle instead.
  if (left_string->length() + right_string->length() > i::String::kMaxLength) {
    return Local<String>();
  }
  i::Handle<i::String> result = isolate->factory()->NewConsString(
      left_string, right_string).ToHandleChecked();
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
    i_isolate->heap()->RegisterExternalString(*string);
    return Utils::ToLocal(string);
  } else {
    // The resource isn't going to be used, free it immediately.
    resource->Dispose();
    return Utils::ToLocal(i_isolate->factory()->empty_string());
  }
}


Local<String> v8::String::NewExternal(
    Isolate* isolate, v8::String::ExternalStringResource* resource) {
  RETURN_TO_LOCAL_UNCHECKED(NewExternalTwoByte(isolate, resource), String);
}


MaybeLocal<String> v8::String::NewExternalOneByte(
    Isolate* isolate, v8::String::ExternalOneByteStringResource* resource) {
  CHECK(resource && resource->data());
  // TODO(dcarney): throw a context free exception.
  if (resource->length() > static_cast<size_t>(i::String::kMaxLength)) {
    return MaybeLocal<String>();
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  LOG_API(i_isolate, String, NewExternalOneByte);
  if (resource->length() > 0) {
    i::Handle<i::String> string = i_isolate->factory()
                                      ->NewExternalStringFromOneByte(resource)
                                      .ToHandleChecked();
    i_isolate->heap()->RegisterExternalString(*string);
    return Utils::ToLocal(string);
  } else {
    // The resource isn't going to be used, free it immediately.
    resource->Dispose();
    return Utils::ToLocal(i_isolate->factory()->empty_string());
  }
}


Local<String> v8::String::NewExternal(
    Isolate* isolate, v8::String::ExternalOneByteStringResource* resource) {
  RETURN_TO_LOCAL_UNCHECKED(NewExternalOneByte(isolate, resource), String);
}


bool v8::String::MakeExternal(v8::String::ExternalStringResource* resource) {
  i::Handle<i::String> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  if (i::StringShape(*obj).IsExternal()) {
    return false;  // Already an external string.
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (isolate->heap()->IsInGCPostProcessing()) {
    return false;
  }
  CHECK(resource && resource->data());

  bool result = obj->MakeExternal(resource);
  // Assert that if CanMakeExternal(), then externalizing actually succeeds.
  DCHECK(!CanMakeExternal() || result);
  if (result) {
    DCHECK(obj->IsExternalString());
    isolate->heap()->RegisterExternalString(*obj);
  }
  return result;
}


bool v8::String::MakeExternal(
    v8::String::ExternalOneByteStringResource* resource) {
  i::Handle<i::String> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  if (i::StringShape(*obj).IsExternal()) {
    return false;  // Already an external string.
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (isolate->heap()->IsInGCPostProcessing()) {
    return false;
  }
  CHECK(resource && resource->data());

  bool result = obj->MakeExternal(resource);
  // Assert that if CanMakeExternal(), then externalizing actually succeeds.
  DCHECK(!CanMakeExternal() || result);
  if (result) {
    DCHECK(obj->IsExternalString());
    isolate->heap()->RegisterExternalString(*obj);
  }
  return result;
}


bool v8::String::CanMakeExternal() {
  i::Handle<i::String> obj = Utils::OpenHandle(this);
  if (obj->IsExternalString()) return false;

  // Old space strings should be externalized.
  i::Isolate* isolate = obj->GetIsolate();
  return !isolate->heap()->new_space()->Contains(*obj);
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
  i::Handle<i::JSValue> jsvalue = i::Handle<i::JSValue>::cast(obj);
  i::Isolate* isolate = jsvalue->GetIsolate();
  LOG_API(isolate, NumberObject, NumberValue);
  return jsvalue->value()->Number();
}


Local<v8::Value> v8::BooleanObject::New(Isolate* isolate, bool value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, BooleanObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> boolean(value ? i_isolate->heap()->true_value()
                                     : i_isolate->heap()->false_value(),
                               i_isolate);
  i::Handle<i::Object> obj =
      i::Object::ToObject(i_isolate, boolean).ToHandleChecked();
  return Utils::ToLocal(obj);
}


Local<v8::Value> v8::BooleanObject::New(bool value) {
  return New(Isolate::GetCurrent(), value);
}


bool v8::BooleanObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSValue> jsvalue = i::Handle<i::JSValue>::cast(obj);
  i::Isolate* isolate = jsvalue->GetIsolate();
  LOG_API(isolate, BooleanObject, BooleanValue);
  return jsvalue->value()->IsTrue(isolate);
}


Local<v8::Value> v8::StringObject::New(Local<String> value) {
  i::Handle<i::String> string = Utils::OpenHandle(*value);
  i::Isolate* isolate = string->GetIsolate();
  LOG_API(isolate, StringObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Object> obj =
      i::Object::ToObject(isolate, string).ToHandleChecked();
  return Utils::ToLocal(obj);
}


Local<v8::String> v8::StringObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSValue> jsvalue = i::Handle<i::JSValue>::cast(obj);
  i::Isolate* isolate = jsvalue->GetIsolate();
  LOG_API(isolate, StringObject, StringValue);
  return Utils::ToLocal(
      i::Handle<i::String>(i::String::cast(jsvalue->value())));
}


Local<v8::Value> v8::SymbolObject::New(Isolate* isolate, Local<Symbol> value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, SymbolObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> obj = i::Object::ToObject(
      i_isolate, Utils::OpenHandle(*value)).ToHandleChecked();
  return Utils::ToLocal(obj);
}


Local<v8::Symbol> v8::SymbolObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSValue> jsvalue = i::Handle<i::JSValue>::cast(obj);
  i::Isolate* isolate = jsvalue->GetIsolate();
  LOG_API(isolate, SymbolObject, SymbolValue);
  return Utils::ToLocal(
      i::Handle<i::Symbol>(i::Symbol::cast(jsvalue->value())));
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


Local<v8::Value> v8::Date::New(Isolate* isolate, double time) {
  auto context = isolate->GetCurrentContext();
  RETURN_TO_LOCAL_UNCHECKED(New(context, time), Value);
}


double v8::Date::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSDate> jsdate = i::Handle<i::JSDate>::cast(obj);
  i::Isolate* isolate = jsdate->GetIsolate();
  LOG_API(isolate, Date, NumberValue);
  return jsdate->value()->Number();
}


void v8::Date::DateTimeConfigurationChangeNotification(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Date, DateTimeConfigurationChangeNotification);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i_isolate->date_cache()->ResetDateCache();
  if (!i_isolate->eternal_handles()->Exists(
          i::EternalHandles::DATE_CACHE_VERSION)) {
    return;
  }
  i::Handle<i::FixedArray> date_cache_version =
      i::Handle<i::FixedArray>::cast(i_isolate->eternal_handles()->GetSingleton(
          i::EternalHandles::DATE_CACHE_VERSION));
  DCHECK_EQ(1, date_cache_version->length());
  CHECK(date_cache_version->get(0)->IsSmi());
  date_cache_version->set(
      0, i::Smi::FromInt(i::Smi::ToInt(date_cache_version->get(0)) + 1));
}


MaybeLocal<v8::RegExp> v8::RegExp::New(Local<Context> context,
                                       Local<String> pattern, Flags flags) {
  PREPARE_FOR_EXECUTION(context, RegExp, New, RegExp);
  Local<v8::RegExp> result;
  has_pending_exception =
      !ToLocal<RegExp>(i::JSRegExp::New(Utils::OpenHandle(*pattern),
                                        static_cast<i::JSRegExp::Flags>(flags)),
                       &result);
  RETURN_ON_FAILED_EXECUTION(RegExp);
  RETURN_ESCAPED(result);
}


Local<v8::RegExp> v8::RegExp::New(Local<String> pattern, Flags flags) {
  auto isolate =
      reinterpret_cast<Isolate*>(Utils::OpenHandle(*pattern)->GetIsolate());
  auto context = isolate->GetCurrentContext();
  RETURN_TO_LOCAL_UNCHECKED(New(context, pattern, flags), RegExp);
}


Local<v8::String> v8::RegExp::GetSource() const {
  i::Handle<i::JSRegExp> obj = Utils::OpenHandle(this);
  return Utils::ToLocal(i::Handle<i::String>(obj->Pattern()));
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


uint32_t v8::Array::Length() const {
  i::Handle<i::JSArray> obj = Utils::OpenHandle(this);
  i::Object* length = obj->length();
  if (length->IsSmi()) {
    return i::Smi::ToInt(length);
  } else {
    return static_cast<uint32_t>(length->Number());
  }
}


MaybeLocal<Object> Array::CloneElementAt(Local<Context> context,
                                         uint32_t index) {
  PREPARE_FOR_EXECUTION(context, Array, CloneElementAt, Object);
  auto self = Utils::OpenHandle(this);
  if (!self->HasObjectElements()) return Local<Object>();
  i::FixedArray* elms = i::FixedArray::cast(self->elements());
  i::Object* paragon = elms->get(index);
  if (!paragon->IsJSObject()) return Local<Object>();
  i::Handle<i::JSObject> paragon_handle(i::JSObject::cast(paragon));
  Local<Object> result;
  has_pending_exception =
      !ToLocal<Object>(isolate->factory()->CopyJSObject(paragon_handle),
                       &result);
  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}


Local<Object> Array::CloneElementAt(uint32_t index) { return Local<Object>(); }


Local<v8::Map> v8::Map::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Map, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSMap> obj = i_isolate->factory()->NewJSMap();
  return Utils::ToLocal(obj);
}


size_t v8::Map::Size() const {
  i::Handle<i::JSMap> obj = Utils::OpenHandle(this);
  return i::OrderedHashMap::cast(obj->table())->NumberOfElements();
}


void Map::Clear() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  LOG_API(isolate, Map, Clear);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::JSMap::Clear(self);
}


MaybeLocal<Value> Map::Get(Local<Context> context, Local<Value> key) {
  PREPARE_FOR_EXECUTION(context, Map, Get, Value);
  auto self = Utils::OpenHandle(this);
  Local<Value> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !ToLocal<Value>(i::Execution::Call(isolate, isolate->map_get(), self,
                                         arraysize(argv), argv),
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
  has_pending_exception = !i::Execution::Call(isolate, isolate->map_set(), self,
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
  has_pending_exception = !i::Execution::Call(isolate, isolate->map_has(), self,
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
  has_pending_exception = !i::Execution::Call(isolate, isolate->map_delete(),
                                              self, arraysize(argv), argv)
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

i::Handle<i::JSArray> MapAsArray(i::Isolate* isolate, i::Object* table_obj,
                                 int offset, MapAsArrayKind kind) {
  i::Factory* factory = isolate->factory();
  i::Handle<i::OrderedHashMap> table(i::OrderedHashMap::cast(table_obj));
  if (offset >= table->NumberOfElements()) return factory->NewJSArray(0);
  int length = (table->NumberOfElements() - offset) *
               (kind == MapAsArrayKind::kEntries ? 2 : 1);
  i::Handle<i::FixedArray> result = factory->NewFixedArray(length);
  int result_index = 0;
  {
    i::DisallowHeapAllocation no_gc;
    int capacity = table->UsedCapacity();
    i::Oddball* the_hole = isolate->heap()->the_hole_value();
    for (int i = 0; i < capacity; ++i) {
      i::Object* key = table->KeyAt(i);
      if (key == the_hole) continue;
      if (offset-- > 0) continue;
      if (kind == MapAsArrayKind::kEntries || kind == MapAsArrayKind::kKeys) {
        result->set(result_index++, key);
      }
      if (kind == MapAsArrayKind::kEntries || kind == MapAsArrayKind::kValues) {
        result->set(result_index++, table->ValueAt(i));
      }
    }
  }
  DCHECK_EQ(result_index, result->length());
  DCHECK_EQ(result_index, length);
  return factory->NewJSArrayWithElements(result, i::PACKED_ELEMENTS, length);
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
  return i::OrderedHashSet::cast(obj->table())->NumberOfElements();
}


void Set::Clear() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  LOG_API(isolate, Set, Clear);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::JSSet::Clear(self);
}


MaybeLocal<Set> Set::Add(Local<Context> context, Local<Value> key) {
  PREPARE_FOR_EXECUTION(context, Set, Add, Set);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception = !i::Execution::Call(isolate, isolate->set_add(), self,
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
  has_pending_exception = !i::Execution::Call(isolate, isolate->set_has(), self,
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
  has_pending_exception = !i::Execution::Call(isolate, isolate->set_delete(),
                                              self, arraysize(argv), argv)
                               .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(isolate));
}

namespace {
i::Handle<i::JSArray> SetAsArray(i::Isolate* isolate, i::Object* table_obj,
                                 int offset) {
  i::Factory* factory = isolate->factory();
  i::Handle<i::OrderedHashSet> table(i::OrderedHashSet::cast(table_obj));
  int length = table->NumberOfElements() - offset;
  if (length <= 0) return factory->NewJSArray(0);
  i::Handle<i::FixedArray> result = factory->NewFixedArray(length);
  int result_index = 0;
  {
    i::DisallowHeapAllocation no_gc;
    int capacity = table->UsedCapacity();
    i::Oddball* the_hole = isolate->heap()->the_hole_value();
    for (int i = 0; i < capacity; ++i) {
      i::Object* key = table->KeyAt(i);
      if (key == the_hole) continue;
      if (offset-- > 0) continue;
      result->set(result_index++, key);
    }
  }
  DCHECK_EQ(result_index, result->length());
  DCHECK_EQ(result_index, length);
  return factory->NewJSArrayWithElements(result, i::PACKED_ELEMENTS, length);
}
}  // namespace

Local<Array> Set::AsArray() const {
  i::Handle<i::JSSet> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  LOG_API(isolate, Set, AsArray);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  return Utils::ToLocal(SetAsArray(isolate, obj->table(), 0));
}


MaybeLocal<Promise::Resolver> Promise::Resolver::New(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, Promise_Resolver, New, Resolver);
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::Execution::Call(isolate, isolate->promise_internal_constructor(),
                          isolate->factory()->undefined_value(), 0, nullptr)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Promise::Resolver);
  RETURN_ESCAPED(Local<Promise::Resolver>::Cast(Utils::ToLocal(result)));
}


Local<Promise::Resolver> Promise::Resolver::New(Isolate* isolate) {
  RETURN_TO_LOCAL_UNCHECKED(New(isolate->GetCurrentContext()),
                            Promise::Resolver);
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
  i::Handle<i::Object> argv[] = {self, Utils::OpenHandle(*value)};
  has_pending_exception =
      i::Execution::Call(isolate, isolate->promise_resolve(),
                         isolate->factory()->undefined_value(), arraysize(argv),
                         argv)
          .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}


void Promise::Resolver::Resolve(Local<Value> value) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  USE(Resolve(context, value));
}


Maybe<bool> Promise::Resolver::Reject(Local<Context> context,
                                      Local<Value> value) {
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(isolate, context, Promise_Resolver, Reject, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);

  // We pass true to trigger the debugger's on exception handler.
  i::Handle<i::Object> argv[] = {self, Utils::OpenHandle(*value),
                                 isolate->factory()->ToBoolean(true)};
  has_pending_exception =
      i::Execution::Call(isolate, isolate->promise_internal_reject(),
                         isolate->factory()->undefined_value(), arraysize(argv),
                         argv)
          .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}


void Promise::Resolver::Reject(Local<Value> value) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  USE(Reject(context, value));
}


MaybeLocal<Promise> Promise::Catch(Local<Context> context,
                                   Local<Function> handler) {
  PREPARE_FOR_EXECUTION(context, Promise, Catch, Promise);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> argv[] = { Utils::OpenHandle(*handler) };
  i::Handle<i::Object> result;
  has_pending_exception = !i::Execution::Call(isolate, isolate->promise_catch(),
                                              self, arraysize(argv), argv)
                               .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Promise);
  RETURN_ESCAPED(Local<Promise>::Cast(Utils::ToLocal(result)));
}


Local<Promise> Promise::Catch(Local<Function> handler) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(Catch(context, handler), Promise);
}


MaybeLocal<Promise> Promise::Then(Local<Context> context,
                                  Local<Function> handler) {
  PREPARE_FOR_EXECUTION(context, Promise, Then, Promise);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> argv[] = { Utils::OpenHandle(*handler) };
  i::Handle<i::Object> result;
  has_pending_exception = !i::Execution::Call(isolate, isolate->promise_then(),
                                              self, arraysize(argv), argv)
                               .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Promise);
  RETURN_ESCAPED(Local<Promise>::Cast(Utils::ToLocal(result)));
}


Local<Promise> Promise::Then(Local<Function> handler) {
  auto context = ContextFromHeapObject(Utils::OpenHandle(this));
  RETURN_TO_LOCAL_UNCHECKED(Then(context, handler), Promise);
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

Local<Object> Proxy::GetTarget() {
  i::Handle<i::JSProxy> self = Utils::OpenHandle(this);
  i::Handle<i::JSReceiver> target(self->target());
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

Local<String> WasmCompiledModule::GetWasmWireBytes() {
  i::Handle<i::WasmModuleObject> obj =
      i::Handle<i::WasmModuleObject>::cast(Utils::OpenHandle(this));
  i::Handle<i::WasmCompiledModule> compiled_part =
      i::handle(i::WasmCompiledModule::cast(obj->compiled_module()));
  i::Handle<i::String> wire_bytes(compiled_part->module_bytes());
  return Local<String>::Cast(Utils::ToLocal(wire_bytes));
}

// Currently, wasm modules are bound, both to Isolate and to
// the Context they were created in. The currently-supported means to
// decontextualize and then re-contextualize a module is via
// serialization/deserialization.
WasmCompiledModule::TransferrableModule
WasmCompiledModule::GetTransferrableModule() {
  i::DisallowHeapAllocation no_gc;
  WasmCompiledModule::SerializedModule compiled_part = Serialize();

  Local<String> wire_bytes = GetWasmWireBytes();
  size_t wire_size = static_cast<size_t>(wire_bytes->Length());
  uint8_t* bytes = new uint8_t[wire_size];
  wire_bytes->WriteOneByte(bytes, 0, wire_bytes->Length());

  return TransferrableModule(
      std::move(compiled_part),
      std::make_pair(
          std::unique_ptr<const uint8_t[]>(const_cast<const uint8_t*>(bytes)),
          wire_size));
}

MaybeLocal<WasmCompiledModule> WasmCompiledModule::FromTransferrableModule(
    Isolate* isolate,
    const WasmCompiledModule::TransferrableModule& transferrable_module) {
  MaybeLocal<WasmCompiledModule> ret =
      Deserialize(isolate, AsCallerOwned(transferrable_module.compiled_code),
                  AsCallerOwned(transferrable_module.wire_bytes));
  return ret;
}

WasmCompiledModule::SerializedModule WasmCompiledModule::Serialize() {
  i::Handle<i::WasmModuleObject> obj =
      i::Handle<i::WasmModuleObject>::cast(Utils::OpenHandle(this));
  i::Handle<i::WasmCompiledModule> compiled_part =
      i::handle(i::WasmCompiledModule::cast(obj->compiled_module()));
  if (i::FLAG_wasm_jit_to_native) {
    i::Isolate* isolate = obj->GetIsolate();

    return i::wasm::NativeModuleSerializer::SerializeWholeModule(isolate,
                                                                 compiled_part);
  } else {
    std::unique_ptr<i::ScriptData> script_data =
        i::WasmCompiledModuleSerializer::SerializeWasmModule(obj->GetIsolate(),
                                                             compiled_part);
    script_data->ReleaseDataOwnership();

    size_t size = static_cast<size_t>(script_data->length());
    return {std::unique_ptr<const uint8_t[]>(script_data->data()), size};
  }
}

MaybeLocal<WasmCompiledModule> WasmCompiledModule::Deserialize(
    Isolate* isolate,
    const WasmCompiledModule::CallerOwnedBuffer& serialized_module,
    const WasmCompiledModule::CallerOwnedBuffer& wire_bytes) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::MaybeHandle<i::FixedArray> maybe_compiled_part;
  if (i::FLAG_wasm_jit_to_native) {
    maybe_compiled_part =
        i::wasm::NativeModuleDeserializer::DeserializeFullBuffer(
            i_isolate, {serialized_module.first, serialized_module.second},
            {wire_bytes.first, wire_bytes.second});
  } else {
    int size = static_cast<int>(serialized_module.second);
    i::ScriptData sc(serialized_module.first, size);
    maybe_compiled_part =
        i::WasmCompiledModuleSerializer::DeserializeWasmModule(
            i_isolate, &sc, {wire_bytes.first, wire_bytes.second});
  }
  i::Handle<i::FixedArray> compiled_part;
  if (!maybe_compiled_part.ToHandle(&compiled_part)) {
    return MaybeLocal<WasmCompiledModule>();
  }
  i::Handle<i::WasmCompiledModule> compiled_module =
      handle(i::WasmCompiledModule::cast(*compiled_part));
  return Local<WasmCompiledModule>::Cast(
      Utils::ToLocal(i::Handle<i::JSObject>::cast(
          i::WasmModuleObject::New(i_isolate, compiled_module))));
}

MaybeLocal<WasmCompiledModule> WasmCompiledModule::DeserializeOrCompile(
    Isolate* isolate,
    const WasmCompiledModule::CallerOwnedBuffer& serialized_module,
    const WasmCompiledModule::CallerOwnedBuffer& wire_bytes) {
  MaybeLocal<WasmCompiledModule> ret =
      Deserialize(isolate, serialized_module, wire_bytes);
  if (!ret.IsEmpty()) {
    return ret;
  }
  return Compile(isolate, wire_bytes.first, wire_bytes.second);
}

MaybeLocal<WasmCompiledModule> WasmCompiledModule::Compile(Isolate* isolate,
                                                           const uint8_t* start,
                                                           size_t length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::wasm::ErrorThrower thrower(i_isolate, "WasmCompiledModule::Compile()");
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    return MaybeLocal<WasmCompiledModule>();
  }
  i::MaybeHandle<i::JSObject> maybe_compiled = i::wasm::SyncCompile(
      i_isolate, &thrower, i::wasm::ModuleWireBytes(start, start + length));
  if (maybe_compiled.is_null()) return MaybeLocal<WasmCompiledModule>();
  return Local<WasmCompiledModule>::Cast(
      Utils::ToLocal(maybe_compiled.ToHandleChecked()));
}

WasmModuleObjectBuilderStreaming::WasmModuleObjectBuilderStreaming(
    Isolate* isolate)
    : isolate_(isolate) {
  MaybeLocal<Promise::Resolver> maybe_resolver =
      Promise::Resolver::New(isolate->GetCurrentContext());
  Local<Promise::Resolver> resolver = maybe_resolver.ToLocalChecked();
  promise_.Reset(isolate, resolver->GetPromise());

  if (i::FLAG_wasm_stream_compilation) {
    i::Handle<i::JSPromise> promise = Utils::OpenHandle(*GetPromise());
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    streaming_decoder_ =
        i_isolate->wasm_compilation_manager()->StartStreamingCompilation(
            i_isolate, handle(i_isolate->context()), promise);
  }
}

Local<Promise> WasmModuleObjectBuilderStreaming::GetPromise() {
  return promise_.Get(isolate_);
}

void WasmModuleObjectBuilderStreaming::OnBytesReceived(const uint8_t* bytes,
                                                       size_t size) {
  if (i::FLAG_wasm_stream_compilation) {
    streaming_decoder_->OnBytesReceived(i::Vector<const uint8_t>(bytes, size));
    return;
  }
  std::unique_ptr<uint8_t[]> cloned_bytes(new uint8_t[size]);
  memcpy(cloned_bytes.get(), bytes, size);
  received_buffers_.push_back(
      Buffer(std::unique_ptr<const uint8_t[]>(
                 const_cast<const uint8_t*>(cloned_bytes.release())),
             size));
  total_size_ += size;
}

void WasmModuleObjectBuilderStreaming::Finish() {
  if (i::FLAG_wasm_stream_compilation) {
    streaming_decoder_->Finish();
    return;
  }
  std::unique_ptr<uint8_t[]> wire_bytes(new uint8_t[total_size_]);
  uint8_t* insert_at = wire_bytes.get();

  for (size_t i = 0; i < received_buffers_.size(); ++i) {
    const Buffer& buff = received_buffers_[i];
    memcpy(insert_at, buff.first.get(), buff.second);
    insert_at += buff.second;
  }
  // AsyncCompile makes its own copy of the wire bytes. This inefficiency
  // will be resolved when we move to true streaming compilation.
  i::wasm::AsyncCompile(reinterpret_cast<i::Isolate*>(isolate_),
                        Utils::OpenHandle(*promise_.Get(isolate_)),
                        {wire_bytes.get(), wire_bytes.get() + total_size_});
}

void WasmModuleObjectBuilderStreaming::Abort(Local<Value> exception) {
  Local<Promise> promise = GetPromise();
  // The promise has already been resolved, e.g. because of a compilation
  // error.
  if (promise->State() != v8::Promise::kPending) return;
  if (i::FLAG_wasm_stream_compilation) streaming_decoder_->Abort();

  Local<Promise::Resolver> resolver = promise.As<Promise::Resolver>();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate_);
  i::HandleScope scope(i_isolate);
  Local<Context> context = Utils::ToLocal(handle(i_isolate->context()));
  auto maybe = resolver->Reject(context, exception);
  CHECK_IMPLIES(!maybe.FromMaybe(false), i_isolate->has_scheduled_exception());
}

WasmModuleObjectBuilderStreaming::~WasmModuleObjectBuilderStreaming() {
  promise_.Reset();
}

void WasmModuleObjectBuilder::OnBytesReceived(const uint8_t* bytes,
                                              size_t size) {
  std::unique_ptr<uint8_t[]> cloned_bytes(new uint8_t[size]);
  memcpy(cloned_bytes.get(), bytes, size);
  received_buffers_.push_back(
      Buffer(std::unique_ptr<const uint8_t[]>(
                 const_cast<const uint8_t*>(cloned_bytes.release())),
             size));
  total_size_ += size;
}

MaybeLocal<WasmCompiledModule> WasmModuleObjectBuilder::Finish() {
  std::unique_ptr<uint8_t[]> wire_bytes(new uint8_t[total_size_]);
  uint8_t* insert_at = wire_bytes.get();

  for (size_t i = 0; i < received_buffers_.size(); ++i) {
    const Buffer& buff = received_buffers_[i];
    memcpy(insert_at, buff.first.get(), buff.second);
    insert_at += buff.second;
  }
  return WasmCompiledModule::Compile(isolate_, wire_bytes.get(), total_size_);
}

// static
v8::ArrayBuffer::Allocator* v8::ArrayBuffer::Allocator::NewDefaultAllocator() {
  return new ArrayBufferAllocator();
}

bool v8::ArrayBuffer::IsExternal() const {
  return Utils::OpenHandle(this)->is_external();
}


bool v8::ArrayBuffer::IsNeuterable() const {
  return Utils::OpenHandle(this)->is_neuterable();
}


v8::ArrayBuffer::Contents v8::ArrayBuffer::Externalize() {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  Utils::ApiCheck(!self->is_external(), "v8_ArrayBuffer_Externalize",
                  "ArrayBuffer already externalized");
  self->set_is_external(true);
  isolate->heap()->UnregisterArrayBuffer(*self);

  return GetContents();
}


v8::ArrayBuffer::Contents v8::ArrayBuffer::GetContents() {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  size_t byte_length = static_cast<size_t>(self->byte_length()->Number());
  Contents contents;
  contents.allocation_base_ = self->allocation_base();
  contents.allocation_length_ = self->allocation_length();
  contents.allocation_mode_ = self->has_guard_region()
                                  ? Allocator::AllocationMode::kReservation
                                  : Allocator::AllocationMode::kNormal;
  contents.data_ = self->backing_store();
  contents.byte_length_ = byte_length;
  return contents;
}


void v8::ArrayBuffer::Neuter() {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  Utils::ApiCheck(obj->is_external(),
                  "v8::ArrayBuffer::Neuter",
                  "Only externalized ArrayBuffers can be neutered");
  Utils::ApiCheck(obj->is_neuterable(), "v8::ArrayBuffer::Neuter",
                  "Only neuterable ArrayBuffers can be neutered");
  LOG_API(isolate, ArrayBuffer, Neuter);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  obj->Neuter();
}


size_t v8::ArrayBuffer::ByteLength() const {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->byte_length()->Number());
}


Local<ArrayBuffer> v8::ArrayBuffer::New(Isolate* isolate, size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, ArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSArrayBuffer(i::SharedFlag::kNotShared);
  // TODO(jbroman): It may be useful in the future to provide a MaybeLocal
  // version that throws an exception or otherwise does not crash.
  if (!i::JSArrayBuffer::SetupAllocatingData(obj, i_isolate, byte_length)) {
    i::FatalProcessOutOfMemory("v8::ArrayBuffer::New");
  }
  return Utils::ToLocal(obj);
}


Local<ArrayBuffer> v8::ArrayBuffer::New(Isolate* isolate, void* data,
                                        size_t byte_length,
                                        ArrayBufferCreationMode mode) {
  // Embedders must guarantee that the external backing store is valid.
  CHECK(byte_length == 0 || data != nullptr);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, ArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSArrayBuffer(i::SharedFlag::kNotShared);
  i::JSArrayBuffer::Setup(obj, i_isolate,
                          mode == ArrayBufferCreationMode::kExternalized, data,
                          byte_length);
  return Utils::ToLocal(obj);
}


Local<ArrayBuffer> v8::ArrayBufferView::Buffer() {
  i::Handle<i::JSArrayBufferView> obj = Utils::OpenHandle(this);
  i::Handle<i::JSArrayBuffer> buffer;
  if (obj->IsJSDataView()) {
    i::Handle<i::JSDataView> data_view(i::JSDataView::cast(*obj));
    DCHECK(data_view->buffer()->IsJSArrayBuffer());
    buffer = i::handle(i::JSArrayBuffer::cast(data_view->buffer()));
  } else {
    DCHECK(obj->IsJSTypedArray());
    buffer = i::JSTypedArray::cast(*obj)->GetBuffer();
  }
  return Utils::ToLocal(buffer);
}


size_t v8::ArrayBufferView::CopyContents(void* dest, size_t byte_length) {
  i::Handle<i::JSArrayBufferView> self = Utils::OpenHandle(this);
  size_t byte_offset = i::NumberToSize(self->byte_offset());
  size_t bytes_to_copy =
      i::Min(byte_length, i::NumberToSize(self->byte_length()));
  if (bytes_to_copy) {
    i::DisallowHeapAllocation no_gc;
    i::Handle<i::JSArrayBuffer> buffer(i::JSArrayBuffer::cast(self->buffer()));
    const char* source = reinterpret_cast<char*>(buffer->backing_store());
    if (source == nullptr) {
      DCHECK(self->IsJSTypedArray());
      i::Handle<i::JSTypedArray> typed_array(i::JSTypedArray::cast(*self));
      i::Handle<i::FixedTypedArrayBase> fixed_array(
          i::FixedTypedArrayBase::cast(typed_array->elements()));
      source = reinterpret_cast<char*>(fixed_array->DataPtr());
    }
    memcpy(dest, source + byte_offset, bytes_to_copy);
  }
  return bytes_to_copy;
}


bool v8::ArrayBufferView::HasBuffer() const {
  i::Handle<i::JSArrayBufferView> self = Utils::OpenHandle(this);
  i::Handle<i::JSArrayBuffer> buffer(i::JSArrayBuffer::cast(self->buffer()));
  return buffer->backing_store() != nullptr;
}


size_t v8::ArrayBufferView::ByteOffset() {
  i::Handle<i::JSArrayBufferView> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->byte_offset()->Number());
}


size_t v8::ArrayBufferView::ByteLength() {
  i::Handle<i::JSArrayBufferView> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->byte_length()->Number());
}


size_t v8::TypedArray::Length() {
  i::Handle<i::JSTypedArray> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->length_value());
}

static_assert(v8::TypedArray::kMaxLength == i::Smi::kMaxValue,
              "v8::TypedArray::kMaxLength must match i::Smi::kMaxValue");

#define TYPED_ARRAY_NEW(Type, type, TYPE, ctype, size)                     \
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


bool v8::SharedArrayBuffer::IsExternal() const {
  return Utils::OpenHandle(this)->is_external();
}


v8::SharedArrayBuffer::Contents v8::SharedArrayBuffer::Externalize() {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  i::Isolate* isolate = self->GetIsolate();
  Utils::ApiCheck(!self->is_external(), "v8_SharedArrayBuffer_Externalize",
                  "SharedArrayBuffer already externalized");
  self->set_is_external(true);
  isolate->heap()->UnregisterArrayBuffer(*self);
  return GetContents();
}


v8::SharedArrayBuffer::Contents v8::SharedArrayBuffer::GetContents() {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  size_t byte_length = static_cast<size_t>(self->byte_length()->Number());
  Contents contents;
  contents.data_ = self->backing_store();
  contents.byte_length_ = byte_length;
  // SharedArrayBuffers never have guard regions, so their allocation and data
  // are equivalent.
  contents.allocation_base_ = self->backing_store();
  contents.allocation_length_ = byte_length;
  contents.allocation_mode_ =
      ArrayBufferAllocator::Allocator::AllocationMode::kNormal;
  return contents;
}


size_t v8::SharedArrayBuffer::ByteLength() const {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->byte_length()->Number());
}


Local<SharedArrayBuffer> v8::SharedArrayBuffer::New(Isolate* isolate,
                                                    size_t byte_length) {
  CHECK(i::FLAG_harmony_sharedarraybuffer);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, SharedArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSArrayBuffer(i::SharedFlag::kShared);
  // TODO(jbroman): It may be useful in the future to provide a MaybeLocal
  // version that throws an exception or otherwise does not crash.
  if (!i::JSArrayBuffer::SetupAllocatingData(obj, i_isolate, byte_length, true,
                                             i::SharedFlag::kShared)) {
    i::FatalProcessOutOfMemory("v8::SharedArrayBuffer::New");
  }
  return Utils::ToLocalShared(obj);
}


Local<SharedArrayBuffer> v8::SharedArrayBuffer::New(
    Isolate* isolate, void* data, size_t byte_length,
    ArrayBufferCreationMode mode) {
  CHECK(i::FLAG_harmony_sharedarraybuffer);
  // Embedders must guarantee that the external backing store is valid.
  CHECK(byte_length == 0 || data != nullptr);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, SharedArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSArrayBuffer(i::SharedFlag::kShared);
  i::JSArrayBuffer::Setup(obj, i_isolate,
                          mode == ArrayBufferCreationMode::kExternalized, data,
                          byte_length, i::SharedFlag::kShared);
  return Utils::ToLocalShared(obj);
}


Local<Symbol> v8::Symbol::New(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, Symbol, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Symbol> result = i_isolate->factory()->NewSymbol();
  if (!name.IsEmpty()) result->set_name(*Utils::OpenHandle(*name));
  return Utils::ToLocal(result);
}


Local<Symbol> v8::Symbol::For(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::String> i_name = Utils::OpenHandle(*name);
  return Utils::ToLocal(i_isolate->SymbolFor(
      i::Heap::kPublicSymbolTableRootIndex, i_name, false));
}


Local<Symbol> v8::Symbol::ForApi(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::String> i_name = Utils::OpenHandle(*name);
  return Utils::ToLocal(
      i_isolate->SymbolFor(i::Heap::kApiSymbolTableRootIndex, i_name, false));
}

#define WELL_KNOWN_SYMBOLS(V)                 \
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
  if (!name.IsEmpty()) symbol->set_name(*Utils::OpenHandle(*name));
  Local<Symbol> result = Utils::ToLocal(symbol);
  return v8::Local<Private>(reinterpret_cast<Private*>(*result));
}


Local<Private> v8::Private::ForApi(Isolate* isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::String> i_name = Utils::OpenHandle(*name);
  Local<Symbol> result = Utils::ToLocal(i_isolate->SymbolFor(
      i::Heap::kApiPrivateSymbolTableRootIndex, i_name, true));
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
    return Utils::IntegerToLocal(i::Handle<i::Object>(i::Smi::FromInt(value),
                                                      internal_isolate));
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


CpuProfiler* Isolate::GetCpuProfiler() {
  i::CpuProfiler* cpu_profiler =
      reinterpret_cast<i::Isolate*>(this)->cpu_profiler();
  return reinterpret_cast<CpuProfiler*>(cpu_profiler);
}


bool Isolate::InContext() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->context() != nullptr;
}


v8::Local<v8::Context> Isolate::GetCurrentContext() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Context* context = isolate->context();
  if (context == nullptr) return Local<Context>();
  i::Context* native_context = context->native_context();
  if (native_context == nullptr) return Local<Context>();
  return Utils::ToLocal(i::Handle<i::Context>(native_context));
}


v8::Local<v8::Context> Isolate::GetCallingContext() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::Object> calling = isolate->GetCallingNativeContext();
  if (calling.is_null()) return Local<Context>();
  return Utils::ToLocal(i::Handle<i::Context>::cast(calling));
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
  i::Handle<i::Object> last;
  if (isolate->handle_scope_implementer()
          ->MicrotaskContextIsLastEnteredContext()) {
    last = isolate->handle_scope_implementer()->MicrotaskContext();
  } else {
    last = isolate->handle_scope_implementer()->LastEnteredContext();
  }
  if (last.is_null()) return Local<Context>();
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
    isolate->ScheduleThrow(isolate->heap()->undefined_value());
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
    reinterpret_cast<i::Isolate*>(this)->heap()->CollectAllGarbage(
        i::Heap::kAbortIncrementalMarkingMask,
        i::GarbageCollectionReason::kTesting, kGCCallbackFlagForced);
  }
}


Isolate* Isolate::GetCurrent() {
  i::Isolate* isolate = i::Isolate::Current();
  return reinterpret_cast<Isolate*>(isolate);
}


Isolate* Isolate::New(const Isolate::CreateParams& params) {
  i::Isolate* isolate = new i::Isolate(false);
  return IsolateNewImpl(isolate, params);
}

// This is separate so that tests can provide a different |isolate|.
Isolate* IsolateNewImpl(internal::Isolate* isolate,
                        const v8::Isolate::CreateParams& params) {
  Isolate* v8_isolate = reinterpret_cast<Isolate*>(isolate);
  CHECK_NOT_NULL(params.array_buffer_allocator);
  isolate->set_array_buffer_allocator(params.array_buffer_allocator);
  if (params.snapshot_blob != nullptr) {
    isolate->set_snapshot_blob(params.snapshot_blob);
  } else {
    isolate->set_snapshot_blob(i::Snapshot::DefaultSnapshotBlob());
  }
  if (params.entry_hook) {
#ifdef V8_USE_SNAPSHOT
    // Setting a FunctionEntryHook is only supported in no-snapshot builds.
    Utils::ApiCheck(
        false, "v8::Isolate::New",
        "Setting a FunctionEntryHook is only supported in no-snapshot builds.");
#endif
    isolate->set_function_entry_hook(params.entry_hook);
  }
  auto code_event_handler = params.code_event_handler;
#ifdef ENABLE_GDB_JIT_INTERFACE
  if (code_event_handler == nullptr && i::FLAG_gdbjit) {
    code_event_handler = i::GDBJITInterface::EventHandler;
  }
#endif  // ENABLE_GDB_JIT_INTERFACE
  if (code_event_handler) {
    isolate->InitializeLoggingAndCounters();
    isolate->logger()->SetCodeEventHandler(kJitCodeEventDefault,
                                           code_event_handler);
  }
  if (params.counter_lookup_callback) {
    v8_isolate->SetCounterFunction(params.counter_lookup_callback);
  }

  if (params.create_histogram_callback) {
    v8_isolate->SetCreateHistogramFunction(params.create_histogram_callback);
  }

  if (params.add_histogram_sample_callback) {
    v8_isolate->SetAddHistogramSampleFunction(
        params.add_histogram_sample_callback);
  }

  isolate->set_api_external_references(params.external_references);
  isolate->set_allow_atomics_wait(params.allow_atomics_wait);

  SetResourceConstraints(isolate, params.constraints);
  // TODO(jochen): Once we got rid of Isolate::Current(), we can remove this.
  Isolate::Scope isolate_scope(v8_isolate);
  if (params.entry_hook || !i::Snapshot::Initialize(isolate)) {
    // If snapshot data was provided and we failed to deserialize it must
    // have been corrupted.
    CHECK_NULL(isolate->snapshot_blob());
    base::ElapsedTimer timer;
    if (i::FLAG_profile_deserialization) timer.Start();
    isolate->Init(nullptr);
    if (i::FLAG_profile_deserialization) {
      double ms = timer.Elapsed().InMillisecondsF();
      i::PrintF("[Initializing isolate from scratch took %0.3f ms]\n", ms);
    }
  }
  return v8_isolate;
}


void Isolate::Dispose() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  if (!Utils::ApiCheck(!isolate->IsInUse(),
                       "v8::Isolate::Dispose()",
                       "Disposing the isolate that is entered by a thread.")) {
    return;
  }
  isolate->TearDown();
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

Isolate::DisallowJavascriptExecutionScope::DisallowJavascriptExecutionScope(
    Isolate* isolate,
    Isolate::DisallowJavascriptExecutionScope::OnFailure on_failure)
    : on_failure_(on_failure) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (on_failure_ == CRASH_ON_FAILURE) {
    internal_ = reinterpret_cast<void*>(
        new i::DisallowJavascriptExecution(i_isolate));
  } else {
    DCHECK_EQ(THROW_ON_FAILURE, on_failure);
    internal_ = reinterpret_cast<void*>(
        new i::ThrowOnJavascriptExecution(i_isolate));
  }
}


Isolate::DisallowJavascriptExecutionScope::~DisallowJavascriptExecutionScope() {
  if (on_failure_ == CRASH_ON_FAILURE) {
    delete reinterpret_cast<i::DisallowJavascriptExecution*>(internal_);
  } else {
    delete reinterpret_cast<i::ThrowOnJavascriptExecution*>(internal_);
  }
}


Isolate::AllowJavascriptExecutionScope::AllowJavascriptExecutionScope(
    Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal_assert_ = reinterpret_cast<void*>(
      new i::AllowJavascriptExecution(i_isolate));
  internal_throws_ = reinterpret_cast<void*>(
      new i::NoThrowOnJavascriptExecution(i_isolate));
}


Isolate::AllowJavascriptExecutionScope::~AllowJavascriptExecutionScope() {
  delete reinterpret_cast<i::AllowJavascriptExecution*>(internal_assert_);
  delete reinterpret_cast<i::NoThrowOnJavascriptExecution*>(internal_throws_);
}


Isolate::SuppressMicrotaskExecutionScope::SuppressMicrotaskExecutionScope(
    Isolate* isolate)
    : isolate_(reinterpret_cast<i::Isolate*>(isolate)) {
  isolate_->handle_scope_implementer()->IncrementCallDepth();
  isolate_->handle_scope_implementer()->IncrementMicrotasksSuppressions();
}


Isolate::SuppressMicrotaskExecutionScope::~SuppressMicrotaskExecutionScope() {
  isolate_->handle_scope_implementer()->DecrementMicrotasksSuppressions();
  isolate_->handle_scope_implementer()->DecrementCallDepth();
}


void Isolate::GetHeapStatistics(HeapStatistics* heap_statistics) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = isolate->heap();
  heap_statistics->total_heap_size_ = heap->CommittedMemory();
  heap_statistics->total_heap_size_executable_ =
      heap->CommittedMemoryExecutable();
  heap_statistics->total_physical_size_ = heap->CommittedPhysicalMemory();
  heap_statistics->total_available_size_ = heap->Available();
  heap_statistics->used_heap_size_ = heap->SizeOfObjects();
  heap_statistics->heap_size_limit_ = heap->MaxReserved();
  heap_statistics->malloced_memory_ =
      isolate->allocator()->GetCurrentMemoryUsage();
  heap_statistics->peak_malloced_memory_ =
      isolate->allocator()->GetMaxMemoryUsage();
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

  space_statistics->space_name_ = heap->GetSpaceName(static_cast<int>(index));
  space_statistics->space_size_ = space->CommittedMemory();
  space_statistics->space_used_size_ = space->SizeOfObjects();
  space_statistics->space_available_size_ = space->Available();
  space_statistics->physical_space_size_ = space->CommittedPhysicalMemory();
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
  if (V8_LIKELY(!i::FLAG_gc_stats)) return false;

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
  return true;
}

void Isolate::GetStackSample(const RegisterState& state, void** frames,
                             size_t frames_limit, SampleInfo* sample_info) {
  RegisterState regs = state;
  if (TickSample::GetStackSample(this, &regs, TickSample::kSkipCEntryFrame,
                                 frames, frames_limit, sample_info)) {
    return;
  }
  sample_info->frames_count = 0;
  sample_info->vm_state = OTHER;
  sample_info->external_callback_entry = nullptr;
}

size_t Isolate::NumberOfPhantomHandleResetsSinceLastCall() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  size_t result = isolate->global_handles()->NumberOfPhantomHandleResets();
  isolate->global_handles()->ResetNumberOfPhantomHandleResets();
  return result;
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


void Isolate::AddCallCompletedCallback(
    DeprecatedCallCompletedCallback callback) {
  AddCallCompletedCallback(reinterpret_cast<CallCompletedCallback>(callback));
}


void Isolate::RemoveCallCompletedCallback(
    DeprecatedCallCompletedCallback callback) {
  RemoveCallCompletedCallback(
      reinterpret_cast<CallCompletedCallback>(callback));
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


void Isolate::RunMicrotasks() {
  DCHECK_NE(MicrotasksPolicy::kScoped, GetMicrotasksPolicy());
  reinterpret_cast<i::Isolate*>(this)->RunMicrotasks();
}


void Isolate::EnqueueMicrotask(Local<Function> microtask) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->EnqueueMicrotask(Utils::OpenHandle(*microtask));
}


void Isolate::EnqueueMicrotask(MicrotaskCallback microtask, void* data) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::HandleScope scope(isolate);
  i::Handle<i::CallHandlerInfo> callback_info =
      i::Handle<i::CallHandlerInfo>::cast(
          isolate->factory()->NewStruct(i::TUPLE3_TYPE, i::NOT_TENURED));
  SET_FIELD_WRAPPED(callback_info, set_callback, microtask);
  SET_FIELD_WRAPPED(callback_info, set_js_callback,
                    callback_info->redirected_callback());
  SET_FIELD_WRAPPED(callback_info, set_data, data);
  isolate->EnqueueMicrotask(callback_info);
}


void Isolate::SetAutorunMicrotasks(bool autorun) {
  SetMicrotasksPolicy(
      autorun ? MicrotasksPolicy::kAuto : MicrotasksPolicy::kExplicit);
}


bool Isolate::WillAutorunMicrotasks() const {
  return GetMicrotasksPolicy() == MicrotasksPolicy::kAuto;
}


void Isolate::SetMicrotasksPolicy(MicrotasksPolicy policy) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->handle_scope_implementer()->set_microtasks_policy(policy);
}


MicrotasksPolicy Isolate::GetMicrotasksPolicy() const {
  i::Isolate* isolate =
      reinterpret_cast<i::Isolate*>(const_cast<Isolate*>(this));
  return isolate->handle_scope_implementer()->microtasks_policy();
}


void Isolate::AddMicrotasksCompletedCallback(
    MicrotasksCompletedCallback callback) {
  DCHECK(callback);
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->AddMicrotasksCompletedCallback(callback);
}


void Isolate::RemoveMicrotasksCompletedCallback(
    MicrotasksCompletedCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->RemoveMicrotasksCompletedCallback(callback);
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


bool Isolate::IdleNotification(int idle_time_in_ms) {
  // Returning true tells the caller that it need not
  // continue to call IdleNotification.
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  if (!i::FLAG_use_idle_notification) return true;
  return isolate->heap()->IdleNotification(idle_time_in_ms);
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
  {
    i::HeapIterator iterator(isolate->heap());
    i::HeapObject* obj;
    while ((obj = iterator.next()) != nullptr) {
      if (obj->IsAbstractCode()) {
        i::AbstractCode::cast(obj)->DropStackFrameCache();
      }
    }
  }
}


int Isolate::ContextDisposedNotification(bool dependant_context) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
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
          : i::ThreadId::Current().Equals(isolate->thread_id());
  isolate->heap()->MemoryPressureNotification(level, on_isolate_thread);
  isolate->allocator()->MemoryPressureNotification(level);
  isolate->compiler_dispatcher()->MemoryPressureNotification(level,
                                                             on_isolate_thread);
}

void Isolate::SetRAILMode(RAILMode rail_mode) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->SetRAILMode(rail_mode);
}

void Isolate::IncreaseHeapLimitForDebugging() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->IncreaseHeapLimitForDebugging();
}

void Isolate::RestoreOriginalHeapLimit() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->heap()->RestoreOriginalHeapLimit();
}

bool Isolate::IsHeapLimitIncreasedForDebugging() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  return isolate->heap()->IsHeapLimitIncreasedForDebugging();
}

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
  if (isolate->heap()->memory_allocator()->code_range()->valid()) {
    *start = isolate->heap()->memory_allocator()->code_range()->start();
    *length_in_bytes =
        isolate->heap()->memory_allocator()->code_range()->size();
  } else {
    *start = nullptr;
    *length_in_bytes = 0;
  }
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
CALLBACK_SETTER(AllowWasmCodeGenerationCallback,
                AllowWasmCodeGenerationCallback, allow_wasm_code_gen_callback)

CALLBACK_SETTER(WasmModuleCallback, ExtensionCallback, wasm_module_callback)
CALLBACK_SETTER(WasmInstanceCallback, ExtensionCallback, wasm_instance_callback)

CALLBACK_SETTER(WasmCompileStreamingCallback, ApiImplementationCallback,
                wasm_compile_streaming_callback)

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
  listener->set(1, data.IsEmpty() ? isolate->heap()->undefined_value()
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
  i::TemplateList* listeners = isolate->heap()->message_listeners();
  for (int i = 0; i < listeners->length(); i++) {
    if (listeners->get(i)->IsUndefined(isolate)) continue;  // skip deleted ones
    i::FixedArray* listener = i::FixedArray::cast(listeners->get(i));
    i::Foreign* callback_obj = i::Foreign::cast(listener->get(0));
    if (callback_obj->foreign_address() == FUNCTION_ADDR(that)) {
      listeners->set(i, isolate->heap()->undefined_value());
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


void Isolate::VisitHandlesForPartialDependence(
    PersistentHandleVisitor* visitor) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::DisallowHeapAllocation no_allocation;
  isolate->global_handles()->IterateAllRootsInNewSpaceWithClassIds(visitor);
}


void Isolate::VisitWeakHandles(PersistentHandleVisitor* visitor) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  i::DisallowHeapAllocation no_allocation;
  isolate->global_handles()->IterateWeakRootsInNewSpaceWithClassIds(visitor);
}

void Isolate::SetAllowAtomicsWait(bool allow) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->set_allow_atomics_wait(allow);
}

MicrotasksScope::MicrotasksScope(Isolate* isolate, MicrotasksScope::Type type)
    : isolate_(reinterpret_cast<i::Isolate*>(isolate)),
      run_(type == MicrotasksScope::kRunMicrotasks) {
  auto handle_scope_implementer = isolate_->handle_scope_implementer();
  if (run_) handle_scope_implementer->IncrementMicrotasksScopeDepth();
#ifdef DEBUG
  if (!run_) handle_scope_implementer->IncrementDebugMicrotasksScopeDepth();
#endif
}


MicrotasksScope::~MicrotasksScope() {
  auto handle_scope_implementer = isolate_->handle_scope_implementer();
  if (run_) {
    handle_scope_implementer->DecrementMicrotasksScopeDepth();
    if (MicrotasksPolicy::kScoped ==
        handle_scope_implementer->microtasks_policy()) {
      PerformCheckpoint(reinterpret_cast<Isolate*>(isolate_));
    }
  }
#ifdef DEBUG
  if (!run_) handle_scope_implementer->DecrementDebugMicrotasksScopeDepth();
#endif
}


void MicrotasksScope::PerformCheckpoint(Isolate* v8Isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8Isolate);
  if (IsExecutionTerminatingCheck(isolate)) return;
  auto handle_scope_implementer = isolate->handle_scope_implementer();
  if (!handle_scope_implementer->GetMicrotasksScopeDepth() &&
      !handle_scope_implementer->HasMicrotasksSuppressions()) {
    isolate->RunMicrotasks();
  }
}


int MicrotasksScope::GetCurrentDepth(Isolate* v8Isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8Isolate);
  return isolate->handle_scope_implementer()->GetMicrotasksScopeDepth();
}

bool MicrotasksScope::IsRunningMicrotasks(Isolate* v8Isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8Isolate);
  return isolate->IsRunningMicrotasks();
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
  i::Handle<i::String> i_str = Utils::OpenHandle(*str);
  length_ = v8::Utf8Length(*i_str, i_isolate);
  str_ = i::NewArray<char>(length_ + 1);
  str->WriteUtf8(str_);
}

String::Utf8Value::Utf8Value(v8::Local<v8::Value> obj)
    : String::Utf8Value::Utf8Value(Isolate::GetCurrent(), obj) {}

String::Utf8Value::~Utf8Value() {
  i::DeleteArray(str_);
}

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
  str->Write(str_);
}

String::Value::Value(v8::Local<v8::Value> obj)
    : String::Value::Value(v8::Isolate::GetCurrent(), obj) {}

String::Value::~Value() {
  i::DeleteArray(str_);
}

#define DEFINE_ERROR(NAME, name)                                         \
  Local<Value> Exception::NAME(v8::Local<v8::String> raw_message) {      \
    i::Isolate* isolate = i::Isolate::Current();                         \
    LOG_API(isolate, NAME, New);                                         \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);                            \
    i::Object* error;                                                    \
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


Local<Message> Exception::CreateMessage(Local<Value> exception) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*exception);
  if (!obj->IsHeapObject()) return Local<Message>();
  i::Isolate* isolate = i::HeapObject::cast(*obj)->GetIsolate();
  return CreateMessage(reinterpret_cast<Isolate*>(isolate), exception);
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

bool Debug::SetDebugEventListener(Isolate* isolate, EventCallback that,
                                  Local<Value> data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (that == nullptr) {
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    i::HandleScope scope(i_isolate);
    i_isolate->debug()->SetDebugDelegate(nullptr, false);
  } else {
    // Might create the Debug context.
    ENTER_V8_FOR_NEW_CONTEXT(i_isolate);
    i::HandleScope scope(i_isolate);
    i::Handle<i::Object> i_data = i_isolate->factory()->undefined_value();
    if (!data.IsEmpty()) i_data = Utils::OpenHandle(*data);
    i::NativeDebugDelegate* delegate =
        new i::NativeDebugDelegate(i_isolate, that, i_data);
    i_isolate->debug()->SetDebugDelegate(delegate, true);
  }
  return true;
}

void Debug::DebugBreak(Isolate* isolate) { debug::DebugBreak(isolate); }

void Debug::CancelDebugBreak(Isolate* isolate) {
  debug::CancelDebugBreak(isolate);
}

bool Debug::CheckDebugBreak(Isolate* isolate) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  return internal_isolate->stack_guard()->CheckDebugBreak();
}

void Debug::SetMessageHandler(Isolate* isolate,
                              v8::Debug::MessageHandler handler) {}

void Debug::SendCommand(Isolate* isolate, const uint16_t* command, int length,
                        ClientData* client_data) {}

MaybeLocal<Value> Debug::Call(Local<Context> context,
                              v8::Local<v8::Function> fun,
                              v8::Local<v8::Value> data) {
  return debug::Call(context, fun, data);
}

void Debug::ProcessDebugMessages(Isolate* isolate) {}

Local<Context> Debug::GetDebugContext(Isolate* isolate) {
  return debug::GetDebugContext(isolate);
}

MaybeLocal<Context> Debug::GetDebuggedContext(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  if (!i_isolate->debug()->in_debug_scope()) return MaybeLocal<Context>();
  i::Handle<i::Object> calling = i_isolate->GetCallingNativeContext();
  if (calling.is_null()) return MaybeLocal<Context>();
  return Utils::ToLocal(i::Handle<i::Context>::cast(calling));
}

void Debug::SetLiveEditEnabled(Isolate* isolate, bool enable) {
  debug::SetLiveEditEnabled(isolate, enable);
}

bool Debug::IsTailCallEliminationEnabled(Isolate* isolate) { return false; }

void Debug::SetTailCallEliminationEnabled(Isolate* isolate, bool enabled) {
}

MaybeLocal<Array> Debug::GetInternalProperties(Isolate* v8_isolate,
                                               Local<Value> value) {
  return debug::GetInternalProperties(v8_isolate, value);
}

void debug::SetContextId(Local<Context> context, int id) {
  Utils::OpenHandle(*context)->set_debug_context_id(i::Smi::FromInt(id));
}

int debug::GetContextId(Local<Context> context) {
  i::Object* value = Utils::OpenHandle(*context)->debug_context_id();
  return (value->IsSmi()) ? i::Smi::ToInt(value) : 0;
}

Local<Context> debug::GetDebugContext(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return Utils::ToLocal(i_isolate->debug()->GetDebugContext());
}

MaybeLocal<Value> debug::Call(Local<Context> context,
                              v8::Local<v8::Function> fun,
                              v8::Local<v8::Value> data) {
  PREPARE_FOR_EXECUTION(context, Debug, Call, Value);
  i::Handle<i::Object> data_obj;
  if (data.IsEmpty()) {
    data_obj = isolate->factory()->undefined_value();
  } else {
    data_obj = Utils::OpenHandle(*data);
  }
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      isolate->debug()->Call(Utils::OpenHandle(*fun), data_obj), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

void debug::SetLiveEditEnabled(Isolate* isolate, bool enable) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal_isolate->debug()->set_live_edit_enabled(enable);
}

void debug::DebugBreak(Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)->stack_guard()->RequestDebugBreak();
}

void debug::CancelDebugBreak(Isolate* isolate) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal_isolate->stack_guard()->ClearDebugBreak();
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

void debug::SetOutOfMemoryCallback(Isolate* isolate,
                                   OutOfMemoryCallback callback, void* data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i_isolate->heap()->SetOutOfMemoryCallback(callback, data);
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
  return script->context_data() == script->GetHeap()->uninitialized_symbol();
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
  CHECK(script->line_ends()->IsFixedArray());
  i::Handle<i::FixedArray> line_ends(i::FixedArray::cast(script->line_ends()));
  std::vector<int> result(line_ends->length());
  for (int i = 0; i < line_ends->length(); ++i) {
    i::Smi* line_end = i::Smi::cast(line_ends->get(i));
    result[i] = line_end->value();
  }
  return result;
}

MaybeLocal<String> debug::Script::Name() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Handle<i::Object> value(script->name(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

MaybeLocal<String> debug::Script::SourceURL() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Handle<i::Object> value(script->source_url(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

MaybeLocal<String> debug::Script::SourceMappingURL() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Handle<i::Object> value(script->source_mapping_url(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

Maybe<int> debug::Script::ContextId() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Object* value = script->context_data();
  if (value->IsSmi()) return Just(i::Smi::ToInt(value));
  return Nothing<int>();
}

MaybeLocal<String> debug::Script::Source() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Script> script = Utils::OpenHandle(this);
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
    i::Handle<i::WasmCompiledModule> compiled_module(
        i::WasmCompiledModule::cast(script->wasm_compiled_module()));
    return compiled_module->GetPossibleBreakpoints(start, end, locations);
  }

  i::Script::InitLineEnds(script);
  CHECK(script->line_ends()->IsFixedArray());
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
    return i::WasmCompiledModule::cast(script->wasm_compiled_module())
               ->GetFunctionOffset(location.GetLineNumber()) +
           location.GetColumnNumber();
  }

  int line = std::max(location.GetLineNumber() - script->line_offset(), 0);
  int column = location.GetColumnNumber();
  if (line == 0) {
    column = std::max(0, column - script->column_offset());
  }

  i::Script::InitLineEnds(script);
  CHECK(script->line_ends()->IsFixedArray());
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
                                    bool preview, bool* stack_changed) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  return isolate->debug()->SetScriptSource(
      script, Utils::OpenHandle(*newSource), preview, stack_changed);
}

bool debug::Script::SetBreakpoint(v8::Local<v8::String> condition,
                                  debug::Location* location,
                                  debug::BreakpointId* id) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  int offset = GetSourceOffset(*location);
  if (!isolate->debug()->SetBreakpoint(script, Utils::OpenHandle(*condition),
                                       &offset, id)) {
    return false;
  }
  *location = GetSourceLocation(offset);
  return true;
}

void debug::RemoveBreakpoint(Isolate* v8_isolate, BreakpointId id) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::HandleScope handle_scope(isolate);
  isolate->debug()->RemoveBreakpoint(id);
}

debug::WasmScript* debug::WasmScript::Cast(debug::Script* script) {
  CHECK(script->IsWasm());
  return static_cast<WasmScript*>(script);
}

int debug::WasmScript::NumFunctions() const {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::WasmCompiledModule* compiled_module =
      i::WasmCompiledModule::cast(script->wasm_compiled_module());
  DCHECK_GE(i::kMaxInt, compiled_module->module()->functions.size());
  return static_cast<int>(compiled_module->module()->functions.size());
}

int debug::WasmScript::NumImportedFunctions() const {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::WasmCompiledModule* compiled_module =
      i::WasmCompiledModule::cast(script->wasm_compiled_module());
  DCHECK_GE(i::kMaxInt, compiled_module->module()->num_imported_functions);
  return static_cast<int>(compiled_module->module()->num_imported_functions);
}

std::pair<int, int> debug::WasmScript::GetFunctionRange(
    int function_index) const {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::WasmCompiledModule* compiled_module =
      i::WasmCompiledModule::cast(script->wasm_compiled_module());
  DCHECK_LE(0, function_index);
  DCHECK_GT(compiled_module->module()->functions.size(), function_index);
  i::wasm::WasmFunction& func =
      compiled_module->module()->functions[function_index];
  DCHECK_GE(i::kMaxInt, func.code.offset());
  DCHECK_GE(i::kMaxInt, func.code.end_offset());
  return std::make_pair(static_cast<int>(func.code.offset()),
                        static_cast<int>(func.code.end_offset()));
}

debug::WasmDisassembly debug::WasmScript::DisassembleFunction(
    int function_index) const {
  i::DisallowHeapAllocation no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::WasmCompiledModule* compiled_module =
      i::WasmCompiledModule::cast(script->wasm_compiled_module());
  return compiled_module->DisassembleFunction(function_index);
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
  // TODO(kozyatinskiy): remove this GC once tests are dealt with.
  isolate->heap()->CollectAllGarbage(i::Heap::kMakeHeapIterableMask,
                                     i::GarbageCollectionReason::kDebugger);
  {
    i::DisallowHeapAllocation no_gc;
    i::Script::Iterator iterator(isolate);
    i::Script* script;
    while ((script = iterator.Next()) != nullptr) {
      if (!script->IsUserJavaScript()) continue;
      if (script->HasValidSource()) {
        i::HandleScope handle_scope(isolate);
        i::Handle<i::Script> script_handle(script, isolate);
        scripts.Append(ToApiHandle<Script>(script_handle));
      }
    }
  }
}

MaybeLocal<UnboundScript> debug::CompileInspectorScript(Isolate* v8_isolate,
                                                        Local<String> source) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE(isolate, UnboundScript);
  i::ScriptData* script_data = nullptr;
  i::Handle<i::String> str = Utils::OpenHandle(*source);
  i::Handle<i::SharedFunctionInfo> result;
  {
    ScriptOriginOptions origin_options;
    i::MaybeHandle<i::SharedFunctionInfo> maybe_function_info =
        i::Compiler::GetSharedFunctionInfoForScript(
            str, i::MaybeHandle<i::Object>(), 0, 0, origin_options,
            i::MaybeHandle<i::Object>(), isolate->native_context(), nullptr,
            &script_data, ScriptCompiler::kNoCompileOptions,
            ScriptCompiler::kNoCacheBecauseInspector,
            i::FLAG_expose_inspector_scripts ? i::NOT_NATIVES_CODE
                                             : i::INSPECTOR_CODE,
            i::MaybeHandle<i::FixedArray>());
    has_pending_exception = !maybe_function_info.ToHandle(&result);
    RETURN_ON_FAILED_EXECUTION(UnboundScript);
  }
  RETURN_ESCAPED(ToApiHandle<UnboundScript>(result));
}

void debug::SetDebugDelegate(Isolate* v8_isolate,
                             debug::DebugDelegate* delegate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  // Might create the Debug context.
  ENTER_V8_FOR_NEW_CONTEXT(isolate);
  isolate->debug()->SetDebugDelegate(delegate, false);
}

void debug::ResetBlackboxedStateCache(Isolate* v8_isolate,
                                      v8::Local<debug::Script> script) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::DisallowHeapAllocation no_gc;
  i::SharedFunctionInfo::ScriptIterator iter(Utils::OpenHandle(*script));
  while (i::SharedFunctionInfo* info = iter.Next()) {
    info->set_computed_debug_is_blackboxed(false);
  }
}

int debug::EstimatedValueSize(Isolate* v8_isolate, v8::Local<v8::Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Object> object = Utils::OpenHandle(*value);
  if (object->IsSmi()) return i::kPointerSize;
  CHECK(object->IsHeapObject());
  return i::Handle<i::HeapObject>::cast(object)->Size();
}

v8::MaybeLocal<v8::Array> debug::EntriesPreview(Isolate* v8_isolate,
                                                v8::Local<v8::Value> value,
                                                bool* is_key_value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  if (value->IsMap()) {
    *is_key_value = true;
    return value.As<Map>()->AsArray();
  }
  if (value->IsSet()) {
    *is_key_value = false;
    return value.As<Set>()->AsArray();
  }

  i::Handle<i::Object> object = Utils::OpenHandle(*value);
  if (object->IsJSWeakCollection()) {
    *is_key_value = object->IsJSWeakMap();
    return Utils::ToLocal(i::JSWeakCollection::GetEntries(
        i::Handle<i::JSWeakCollection>::cast(object), 0));
  }
  if (object->IsJSMapIterator()) {
    i::Handle<i::JSMapIterator> iterator =
        i::Handle<i::JSMapIterator>::cast(object);
    MapAsArrayKind const kind =
        static_cast<MapAsArrayKind>(iterator->map()->instance_type());
    *is_key_value = kind == MapAsArrayKind::kEntries;
    if (!iterator->HasMore()) return v8::Array::New(v8_isolate);
    return Utils::ToLocal(MapAsArray(isolate, iterator->table(),
                                     i::Smi::ToInt(iterator->index()), kind));
  }
  if (object->IsJSSetIterator()) {
    i::Handle<i::JSSetIterator> it = i::Handle<i::JSSetIterator>::cast(object);
    *is_key_value = false;
    if (!it->HasMore()) return v8::Array::New(v8_isolate);
    return Utils::ToLocal(
        SetAsArray(isolate, it->table(), i::Smi::ToInt(it->index())));
  }
  return v8::MaybeLocal<v8::Array>();
}

Local<Function> debug::GetBuiltin(Isolate* v8_isolate, Builtin builtin) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope handle_scope(isolate);
  i::Builtins::Name builtin_id;
  switch (builtin) {
    case kObjectKeys:
      builtin_id = i::Builtins::kObjectKeys;
      break;
    case kObjectGetPrototypeOf:
      builtin_id = i::Builtins::kObjectGetPrototypeOf;
      break;
    case kObjectGetOwnPropertyDescriptor:
      builtin_id = i::Builtins::kObjectGetOwnPropertyDescriptor;
      break;
    case kObjectGetOwnPropertyNames:
      builtin_id = i::Builtins::kObjectGetOwnPropertyNames;
      break;
    case kObjectGetOwnPropertySymbols:
      builtin_id = i::Builtins::kObjectGetOwnPropertySymbols;
      break;
    default:
      UNREACHABLE();
  }

  i::Handle<i::String> name = isolate->factory()->empty_string();
  i::Handle<i::Code> code(isolate->builtins()->builtin(builtin_id));
  i::NewFunctionArgs args = i::NewFunctionArgs::ForBuiltinWithoutPrototype(
      name, code, builtin_id, i::LanguageMode::kSloppy);
  i::Handle<i::JSFunction> fun = isolate->factory()->NewFunction(args);

  fun->shared()->DontAdaptArguments();
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
    internal::BuiltinArguments& args)
    : v8::FunctionCallbackInfo<v8::Value>(nullptr, &args[0] - 1,
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
  i::Object* maybe_script = obj->function()->shared()->script();
  if (!maybe_script->IsScript()) return MaybeLocal<debug::Script>();
  i::Handle<i::Script> script(i::Script::cast(maybe_script), obj->GetIsolate());
  return ToApiHandle<debug::Script>(script);
}

Local<Function> debug::GeneratorObject::Function() {
  i::Handle<i::JSGeneratorObject> obj = Utils::OpenHandle(this);
  return Utils::ToLocal(handle(obj->function()));
}

debug::Location debug::GeneratorObject::SuspendedLocation() {
  i::Handle<i::JSGeneratorObject> obj = Utils::OpenHandle(this);
  CHECK(obj->is_suspended());
  i::Object* maybe_script = obj->function()->shared()->script();
  if (!maybe_script->IsScript()) return debug::Location();
  i::Handle<i::Script> script(i::Script::cast(maybe_script), obj->GetIsolate());
  i::Script::PositionInfo info;
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
  i::Handle<i::ScriptContextTable> table(
      context->global_object()->native_context()->script_context_table());
  for (int i = 0; i < table->used(); i++) {
    i::Handle<i::Context> context = i::ScriptContextTable::GetContext(table, i);
    DCHECK(context->IsScriptContext());
    i::Handle<i::ScopeInfo> scope_info(context->scope_info());
    int local_count = scope_info->ContextLocalCount();
    for (int j = 0; j < local_count; ++j) {
      i::String* name = scope_info->ContextLocalName(j);
      if (i::ScopeInfo::VariableIsSynthetic(name)) continue;
      names->Append(Utils::ToLocal(handle(name)));
    }
  }
}

void debug::SetReturnValue(v8::Isolate* v8_isolate,
                           v8::Local<v8::Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  if (!isolate->debug()->break_id()) return;
  isolate->debug()->set_return_value(*Utils::OpenHandle(*value));
}

int debug::GetNativeAccessorDescriptor(v8::Local<v8::Context> context,
                                       v8::Local<v8::Object> v8_object,
                                       v8::Local<v8::Name> v8_name) {
  i::Handle<i::JSReceiver> object = Utils::OpenHandle(*v8_object);
  i::Handle<i::Name> name = Utils::OpenHandle(*v8_name);
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    return static_cast<int>(debug::NativeAccessorType::None);
  }
  i::LookupIterator it =
      i::LookupIterator(object, name, i::LookupIterator::OWN);
  if (!it.IsFound()) return static_cast<int>(debug::NativeAccessorType::None);
  if (it.state() != i::LookupIterator::ACCESSOR) {
    return static_cast<int>(debug::NativeAccessorType::None);
  }
  i::Handle<i::Object> structure = it.GetAccessors();
  if (!structure->IsAccessorInfo()) {
    return static_cast<int>(debug::NativeAccessorType::None);
  }
  auto isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  int result = 0;
#define IS_BUILTIN_ACESSOR(name, _)                         \
  if (*structure == *isolate->factory()->name##_accessor()) \
    result |= static_cast<int>(debug::NativeAccessorType::IsBuiltin);
  ACCESSOR_INFO_LIST(IS_BUILTIN_ACESSOR)
#undef IS_BUILTIN_ACESSOR
  i::Handle<i::AccessorInfo> accessor_info =
      i::Handle<i::AccessorInfo>::cast(structure);
  if (accessor_info->getter())
    result |= static_cast<int>(debug::NativeAccessorType::HasGetter);
  if (accessor_info->setter())
    result |= static_cast<int>(debug::NativeAccessorType::HasSetter);
  return result;
}

int64_t debug::GetNextRandomInt64(v8::Isolate* v8_isolate) {
  return reinterpret_cast<i::Isolate*>(v8_isolate)
      ->random_number_generator()
      ->NextInt64();
}

Local<String> CpuProfileNode::GetFunctionName() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  i::Isolate* isolate = node->isolate();
  const i::CodeEntry* entry = node->entry();
  i::Handle<i::String> name =
      isolate->factory()->InternalizeUtf8String(entry->name());
  if (!entry->has_name_prefix()) {
    return ToApiHandle<String>(name);
  } else {
    // We do not expect this to fail. Change this if it does.
    i::Handle<i::String> cons = isolate->factory()->NewConsString(
        isolate->factory()->InternalizeUtf8String(entry->name_prefix()),
        name).ToHandleChecked();
    return ToApiHandle<String>(cons);
  }
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

void debug::Coverage::SelectMode(Isolate* isolate, debug::Coverage::Mode mode) {
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
                                    debug::TypeProfile::Mode mode) {
  i::TypeProfile::SelectMode(reinterpret_cast<i::Isolate*>(isolate), mode);
}

size_t debug::TypeProfile::ScriptCount() const { return type_profile_->size(); }

debug::TypeProfile::ScriptData debug::TypeProfile::GetScriptData(
    size_t i) const {
  return ScriptData(i, type_profile_);
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

int CpuProfileNode::GetLineNumber() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->entry()->line_number();
}


int CpuProfileNode::GetColumnNumber() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->
      entry()->column_number();
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


unsigned CpuProfileNode::GetCallUid() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->function_id();
}


unsigned CpuProfileNode::GetNodeId() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->id();
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
  return ToApiHandle<String>(isolate->factory()->InternalizeUtf8String(
      profile->title()));
}


const CpuProfileNode* CpuProfile::GetTopDownRoot() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->top_down()->root());
}


const CpuProfileNode* CpuProfile::GetSample(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->sample(index));
}


int64_t CpuProfile::GetSampleTimestamp(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return (profile->sample_timestamp(index) - base::TimeTicks())
      .InMicroseconds();
}


int64_t CpuProfile::GetStartTime() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return (profile->start_time() - base::TimeTicks()).InMicroseconds();
}


int64_t CpuProfile::GetEndTime() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return (profile->end_time() - base::TimeTicks()).InMicroseconds();
}


int CpuProfile::GetSamplesCount() const {
  return reinterpret_cast<const i::CpuProfile*>(this)->samples_count();
}

CpuProfiler* CpuProfiler::New(Isolate* isolate) {
  return reinterpret_cast<CpuProfiler*>(
      new i::CpuProfiler(reinterpret_cast<i::Isolate*>(isolate)));
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

void CpuProfiler::CollectSample() {
  reinterpret_cast<i::CpuProfiler*>(this)->CollectSample();
}

void CpuProfiler::StartProfiling(Local<String> title, bool record_samples) {
  reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      *Utils::OpenHandle(*title), record_samples);
}


CpuProfile* CpuProfiler::StopProfiling(Local<String> title) {
  return reinterpret_cast<CpuProfile*>(
      reinterpret_cast<i::CpuProfiler*>(this)->StopProfiling(
          *Utils::OpenHandle(*title)));
}


void CpuProfiler::SetIdle(bool is_idle) {
  i::CpuProfiler* profiler = reinterpret_cast<i::CpuProfiler*>(this);
  i::Isolate* isolate = profiler->isolate();
  if (!isolate->is_profiling()) return;
  v8::StateTag state = isolate->current_vm_state();
  DCHECK(state == v8::EXTERNAL || state == v8::IDLE);
  if (isolate->js_entry_sp() != nullptr) return;
  if (is_idle) {
    isolate->set_current_vm_state(v8::IDLE);
  } else if (state == v8::IDLE) {
    isolate->set_current_vm_state(v8::EXTERNAL);
  }
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
    default: UNREACHABLE();
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


SnapshotObjectId HeapGraphNode::GetId() const {
  return ToInternal(this)->id();
}


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
  Utils::ApiCheck(format == kJSON,
                  "v8::HeapSnapshot::Serialize",
                  "Unknown serialization format");
  Utils::ApiCheck(stream->GetChunkSize() > 0,
                  "v8::HeapSnapshot::Serialize",
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
    ActivityControl* control, ObjectNameResolver* resolver) {
  return reinterpret_cast<const HeapSnapshot*>(
      reinterpret_cast<i::HeapProfiler*>(this)
          ->TakeSnapshot(control, resolver));
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


void HeapProfiler::SetWrapperClassInfoProvider(uint16_t class_id,
                                               WrapperInfoCallback callback) {
  reinterpret_cast<i::HeapProfiler*>(this)->DefineWrapperClass(class_id,
                                                               callback);
}

void HeapProfiler::SetGetRetainerInfosCallback(
    GetRetainerInfosCallback callback) {
  reinterpret_cast<i::HeapProfiler*>(this)->SetGetRetainerInfosCallback(
      callback);
}

v8::Testing::StressType internal::Testing::stress_type_ =
    v8::Testing::kStressTypeOpt;


void Testing::SetStressRunType(Testing::StressType type) {
  internal::Testing::set_stress_type(type);
}


int Testing::GetStressRuns() {
  if (internal::FLAG_stress_runs != 0) return internal::FLAG_stress_runs;
#ifdef DEBUG
  // In debug mode the code runs much slower so stressing will only make two
  // runs.
  return 2;
#else
  return 5;
#endif
}


static void SetFlagsFromString(const char* flags) {
  V8::SetFlagsFromString(flags, i::StrLength(flags));
}


void Testing::PrepareStressRun(int run) {
  static const char* kLazyOptimizations =
      "--prepare-always-opt "
      "--max-inlined-bytecode-size=999999 "
      "--max-inlined-bytecode-size-cumulative=999999 "
      "--noalways-opt";
  static const char* kForcedOptimizations = "--always-opt";

  // If deoptimization stressed turn on frequent deoptimization. If no value
  // is spefified through --deopt-every-n-times use a default default value.
  static const char* kDeoptEvery13Times = "--deopt-every-n-times=13";
  if (internal::Testing::stress_type() == Testing::kStressTypeDeopt &&
      internal::FLAG_deopt_every_n_times == 0) {
    SetFlagsFromString(kDeoptEvery13Times);
  }

#ifdef DEBUG
  // As stressing in debug mode only make two runs skip the deopt stressing
  // here.
  if (run == GetStressRuns() - 1) {
    SetFlagsFromString(kForcedOptimizations);
  } else {
    SetFlagsFromString(kLazyOptimizations);
  }
#else
  if (run == GetStressRuns() - 1) {
    SetFlagsFromString(kForcedOptimizations);
  } else if (run != GetStressRuns() - 2) {
    SetFlagsFromString(kLazyOptimizations);
  }
#endif
}


void Testing::DeoptimizeAll(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::HandleScope scope(i_isolate);
  internal::Deoptimizer::DeoptimizeAll(i_isolate);
}


namespace internal {

void HandleScopeImplementer::FreeThreadResources() {
  Free();
}


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
    Object** block = blocks()->at(i);
    if (last_handle_before_deferred_block_ != nullptr &&
        (last_handle_before_deferred_block_ <= &block[kHandleBlockSize]) &&
        (last_handle_before_deferred_block_ >= block)) {
      v->VisitRootPointers(Root::kHandleScope, block,
                           last_handle_before_deferred_block_);
      DCHECK(!found_block_before_deferred);
#ifdef DEBUG
      found_block_before_deferred = true;
#endif
    } else {
      v->VisitRootPointers(Root::kHandleScope, block, &block[kHandleBlockSize]);
    }
  }

  DCHECK(last_handle_before_deferred_block_ == nullptr ||
         found_block_before_deferred);

  // Iterate over live handles in the last block (if any).
  if (!blocks()->empty()) {
    v->VisitRootPointers(Root::kHandleScope, blocks()->back(),
                         handle_scope_data_.next);
  }

  DetachableVector<Context*>* context_lists[2] = {&saved_contexts_,
                                                  &entered_contexts_};
  for (unsigned i = 0; i < arraysize(context_lists); i++) {
    if (context_lists[i]->empty()) continue;
    Object** start = reinterpret_cast<Object**>(&context_lists[i]->front());
    v->VisitRootPointers(Root::kHandleScope, start,
                         start + context_lists[i]->size());
  }
  if (microtask_context_) {
    v->VisitRootPointer(Root::kHandleScope,
                        reinterpret_cast<Object**>(&microtask_context_));
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


DeferredHandles* HandleScopeImplementer::Detach(Object** prev_limit) {
  DeferredHandles* deferred =
      new DeferredHandles(isolate()->handle_scope_data()->next, isolate());

  while (!blocks_.empty()) {
    Object** block_start = blocks_.back();
    Object** block_limit = &block_start[kHandleBlockSize];
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

  DCHECK((first_block_limit_ >= blocks_.front()) &&
         (first_block_limit_ <= &(blocks_.front())[kHandleBlockSize]));

  v->VisitRootPointers(Root::kHandleScope, blocks_.front(), first_block_limit_);

  for (size_t i = 1; i < blocks_.size(); i++) {
    v->VisitRootPointers(Root::kHandleScope, blocks_[i],
                         &blocks_[i][kHandleBlockSize]);
  }
}


void InvokeAccessorGetterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    v8::AccessorNameGetterCallback getter) {
  // Leaving JavaScript.
  Isolate* isolate = reinterpret_cast<Isolate*>(info.GetIsolate());
  RuntimeCallTimerScope timer(isolate,
                              &RuntimeCallStats::AccessorGetterCallback);
  Address getter_address = reinterpret_cast<Address>(reinterpret_cast<intptr_t>(
      getter));
  VMState<EXTERNAL> state(isolate);
  ExternalCallbackScope call_scope(isolate, getter_address);
  getter(property, info);
}


void InvokeFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                            v8::FunctionCallback callback) {
  Isolate* isolate = reinterpret_cast<Isolate*>(info.GetIsolate());
  RuntimeCallTimerScope timer(isolate,
                              &RuntimeCallStats::InvokeFunctionCallback);
  Address callback_address =
      reinterpret_cast<Address>(reinterpret_cast<intptr_t>(callback));
  VMState<EXTERNAL> state(isolate);
  ExternalCallbackScope call_scope(isolate, callback_address);
  callback(info);
}


}  // namespace internal
}  // namespace v8
