// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// TODO(dcarney): remove
#define V8_ALLOW_ACCESS_TO_PERSISTENT_IMPLICIT

#include "api.h"

#include <string.h>  // For memcpy, strlen.
#include <cmath>  // For isnan.
#include "../include/v8-debug.h"
#include "../include/v8-profiler.h"
#include "../include/v8-testing.h"
#include "bootstrapper.h"
#include "code-stubs.h"
#include "compiler.h"
#include "conversions-inl.h"
#include "counters.h"
#include "debug.h"
#include "deoptimizer.h"
#include "execution.h"
#include "global-handles.h"
#include "heap-profiler.h"
#include "heap-snapshot-generator-inl.h"
#include "messages.h"
#ifdef COMPRESS_STARTUP_DATA_BZ2
#include "natives.h"
#endif
#include "parser.h"
#include "platform.h"
#include "profile-generator-inl.h"
#include "property-details.h"
#include "property.h"
#include "runtime.h"
#include "runtime-profiler.h"
#include "scanner-character-streams.h"
#include "snapshot.h"
#include "unicode-inl.h"
#include "v8threads.h"
#include "version.h"
#include "vm-state-inl.h"


#define LOG_API(isolate, expr) LOG(isolate, ApiEntryCall(expr))

#define ENTER_V8(isolate)                                          \
  ASSERT((isolate)->IsInitialized());                              \
  i::VMState<i::OTHER> __state__((isolate))

namespace v8 {

#define ON_BAILOUT(isolate, location, code)                        \
  if (IsDeadCheck(isolate, location) ||                            \
      IsExecutionTerminatingCheck(isolate)) {                      \
    code;                                                          \
    UNREACHABLE();                                                 \
  }


#define EXCEPTION_PREAMBLE(isolate)                                         \
  (isolate)->handle_scope_implementer()->IncrementCallDepth();              \
  ASSERT(!(isolate)->external_caught_exception());                          \
  bool has_pending_exception = false


#define EXCEPTION_BAILOUT_CHECK_GENERIC(isolate, value, do_callback)           \
  do {                                                                         \
    i::HandleScopeImplementer* handle_scope_implementer =                      \
        (isolate)->handle_scope_implementer();                                 \
    handle_scope_implementer->DecrementCallDepth();                            \
    if (has_pending_exception) {                                               \
      if (handle_scope_implementer->CallDepthIsZero() &&                       \
          (isolate)->is_out_of_memory()) {                                     \
        if (!(isolate)->ignore_out_of_memory())                                \
          i::V8::FatalProcessOutOfMemory(NULL);                                \
      }                                                                        \
      bool call_depth_is_zero = handle_scope_implementer->CallDepthIsZero();   \
      (isolate)->OptionalRescheduleException(call_depth_is_zero);              \
      do_callback                                                              \
      return value;                                                            \
    }                                                                          \
    do_callback                                                                \
  } while (false)


#define EXCEPTION_BAILOUT_CHECK_DO_CALLBACK(isolate, value)                    \
  EXCEPTION_BAILOUT_CHECK_GENERIC(                                             \
      isolate, value, i::V8::FireCallCompletedCallback(isolate);)


#define EXCEPTION_BAILOUT_CHECK(isolate, value)                                \
  EXCEPTION_BAILOUT_CHECK_GENERIC(isolate, value, ;)


#define API_ENTRY_CHECK(isolate, msg)                                          \
  do {                                                                         \
    if (v8::Locker::IsActive()) {                                              \
      ApiCheck(isolate->thread_manager()->IsLockedByCurrentThread(),           \
               msg,                                                            \
               "Entering the V8 API without proper locking in place");         \
    }                                                                          \
  } while (false)


// --- E x c e p t i o n   B e h a v i o r ---


static void DefaultFatalErrorHandler(const char* location,
                                     const char* message) {
  i::Isolate* isolate = i::Isolate::Current();
  if (isolate->IsInitialized()) {
    i::VMState<i::OTHER> state(isolate);
    API_Fatal(location, message);
  } else {
    API_Fatal(location, message);
  }
}


static FatalErrorCallback GetFatalErrorHandler() {
  i::Isolate* isolate = i::Isolate::Current();
  if (isolate->exception_behavior() == NULL) {
    isolate->set_exception_behavior(DefaultFatalErrorHandler);
  }
  return isolate->exception_behavior();
}


void i::FatalProcessOutOfMemory(const char* location) {
  i::V8::FatalProcessOutOfMemory(location, false);
}


// When V8 cannot allocated memory FatalProcessOutOfMemory is called.
// The default fatal error handler is called and execution is stopped.
void i::V8::FatalProcessOutOfMemory(const char* location, bool take_snapshot) {
  i::HeapStats heap_stats;
  int start_marker;
  heap_stats.start_marker = &start_marker;
  int new_space_size;
  heap_stats.new_space_size = &new_space_size;
  int new_space_capacity;
  heap_stats.new_space_capacity = &new_space_capacity;
  intptr_t old_pointer_space_size;
  heap_stats.old_pointer_space_size = &old_pointer_space_size;
  intptr_t old_pointer_space_capacity;
  heap_stats.old_pointer_space_capacity = &old_pointer_space_capacity;
  intptr_t old_data_space_size;
  heap_stats.old_data_space_size = &old_data_space_size;
  intptr_t old_data_space_capacity;
  heap_stats.old_data_space_capacity = &old_data_space_capacity;
  intptr_t code_space_size;
  heap_stats.code_space_size = &code_space_size;
  intptr_t code_space_capacity;
  heap_stats.code_space_capacity = &code_space_capacity;
  intptr_t map_space_size;
  heap_stats.map_space_size = &map_space_size;
  intptr_t map_space_capacity;
  heap_stats.map_space_capacity = &map_space_capacity;
  intptr_t cell_space_size;
  heap_stats.cell_space_size = &cell_space_size;
  intptr_t cell_space_capacity;
  heap_stats.cell_space_capacity = &cell_space_capacity;
  intptr_t lo_space_size;
  heap_stats.lo_space_size = &lo_space_size;
  int global_handle_count;
  heap_stats.global_handle_count = &global_handle_count;
  int weak_global_handle_count;
  heap_stats.weak_global_handle_count = &weak_global_handle_count;
  int pending_global_handle_count;
  heap_stats.pending_global_handle_count = &pending_global_handle_count;
  int near_death_global_handle_count;
  heap_stats.near_death_global_handle_count = &near_death_global_handle_count;
  int free_global_handle_count;
  heap_stats.free_global_handle_count = &free_global_handle_count;
  intptr_t memory_allocator_size;
  heap_stats.memory_allocator_size = &memory_allocator_size;
  intptr_t memory_allocator_capacity;
  heap_stats.memory_allocator_capacity = &memory_allocator_capacity;
  int objects_per_type[LAST_TYPE + 1] = {0};
  heap_stats.objects_per_type = objects_per_type;
  int size_per_type[LAST_TYPE + 1] = {0};
  heap_stats.size_per_type = size_per_type;
  int os_error;
  heap_stats.os_error = &os_error;
  int end_marker;
  heap_stats.end_marker = &end_marker;
  i::Isolate* isolate = i::Isolate::Current();
  if (isolate->heap()->HasBeenSetUp()) {
    // BUG(1718): Don't use the take_snapshot since we don't support
    // HeapIterator here without doing a special GC.
    isolate->heap()->RecordStats(&heap_stats, false);
  }
  i::V8::SetFatalError();
  FatalErrorCallback callback = GetFatalErrorHandler();
  const char* message = "Allocation failed - process out of memory";
  callback(location, message);
  // If the callback returns, we stop execution.
  UNREACHABLE();
}


bool Utils::ReportApiFailure(const char* location, const char* message) {
  FatalErrorCallback callback = GetFatalErrorHandler();
  callback(location, message);
  i::V8::SetFatalError();
  return false;
}


bool V8::IsDead() {
  return i::V8::IsDead();
}


static inline bool ApiCheck(bool condition,
                            const char* location,
                            const char* message) {
  return condition ? true : Utils::ReportApiFailure(location, message);
}


static bool ReportV8Dead(const char* location) {
  FatalErrorCallback callback = GetFatalErrorHandler();
  callback(location, "V8 is no longer usable");
  return true;
}


static bool ReportEmptyHandle(const char* location) {
  FatalErrorCallback callback = GetFatalErrorHandler();
  callback(location, "Reading from empty handle");
  return true;
}


/**
 * IsDeadCheck checks that the vm is usable.  If, for instance, the vm has been
 * out of memory at some point this check will fail.  It should be called on
 * entry to all methods that touch anything in the heap, except destructors
 * which you sometimes can't avoid calling after the vm has crashed.  Functions
 * that call EnsureInitialized or ON_BAILOUT don't have to also call
 * IsDeadCheck.  ON_BAILOUT has the advantage over EnsureInitialized that you
 * can arrange to return if the VM is dead.  This is needed to ensure that no VM
 * heap allocations are attempted on a dead VM.  EnsureInitialized has the
 * advantage over ON_BAILOUT that it actually initializes the VM if this has not
 * yet been done.
 */
static inline bool IsDeadCheck(i::Isolate* isolate, const char* location) {
  return !isolate->IsInitialized()
      && i::V8::IsDead() ? ReportV8Dead(location) : false;
}


static inline bool IsExecutionTerminatingCheck(i::Isolate* isolate) {
  if (!isolate->IsInitialized()) return false;
  if (isolate->has_scheduled_exception()) {
    return isolate->scheduled_exception() ==
        isolate->heap()->termination_exception();
  }
  return false;
}


static inline bool EmptyCheck(const char* location, v8::Handle<v8::Data> obj) {
  return obj.IsEmpty() ? ReportEmptyHandle(location) : false;
}


static inline bool EmptyCheck(const char* location, const v8::Data* obj) {
  return (obj == 0) ? ReportEmptyHandle(location) : false;
}

// --- S t a t i c s ---


static bool InitializeHelper() {
  if (i::Snapshot::Initialize()) return true;
  return i::V8::Initialize(NULL);
}


static inline bool EnsureInitializedForIsolate(i::Isolate* isolate,
                                               const char* location) {
  if (IsDeadCheck(isolate, location)) return false;
  if (isolate != NULL) {
    if (isolate->IsInitialized()) return true;
  }
  ASSERT(isolate == i::Isolate::Current());
  return ApiCheck(InitializeHelper(), location, "Error initializing V8");
}

// Some initializing API functions are called early and may be
// called on a thread different from static initializer thread.
// If Isolate API is used, Isolate::Enter() will initialize TLS so
// Isolate::Current() works. If it's a legacy case, then the thread
// may not have TLS initialized yet. However, in initializing APIs it
// may be too early to call EnsureInitialized() - some pre-init
// parameters still have to be configured.
static inline i::Isolate* EnterIsolateIfNeeded() {
  i::Isolate* isolate = i::Isolate::UncheckedCurrent();
  if (isolate != NULL)
    return isolate;

  i::Isolate::EnterDefaultIsolate();
  isolate = i::Isolate::Current();
  return isolate;
}


StartupDataDecompressor::StartupDataDecompressor()
    : raw_data(i::NewArray<char*>(V8::GetCompressedStartupDataCount())) {
  for (int i = 0; i < V8::GetCompressedStartupDataCount(); ++i) {
    raw_data[i] = NULL;
  }
}


StartupDataDecompressor::~StartupDataDecompressor() {
  for (int i = 0; i < V8::GetCompressedStartupDataCount(); ++i) {
    i::DeleteArray(raw_data[i]);
  }
  i::DeleteArray(raw_data);
}


int StartupDataDecompressor::Decompress() {
  int compressed_data_count = V8::GetCompressedStartupDataCount();
  StartupData* compressed_data =
      i::NewArray<StartupData>(compressed_data_count);
  V8::GetCompressedStartupData(compressed_data);
  for (int i = 0; i < compressed_data_count; ++i) {
    char* decompressed = raw_data[i] =
        i::NewArray<char>(compressed_data[i].raw_size);
    if (compressed_data[i].compressed_size != 0) {
      int result = DecompressData(decompressed,
                                  &compressed_data[i].raw_size,
                                  compressed_data[i].data,
                                  compressed_data[i].compressed_size);
      if (result != 0) return result;
    } else {
      ASSERT_EQ(0, compressed_data[i].raw_size);
    }
    compressed_data[i].data = decompressed;
  }
  V8::SetDecompressedStartupData(compressed_data);
  i::DeleteArray(compressed_data);
  return 0;
}


StartupData::CompressionAlgorithm V8::GetCompressedStartupDataAlgorithm() {
#ifdef COMPRESS_STARTUP_DATA_BZ2
  return StartupData::kBZip2;
#else
  return StartupData::kUncompressed;
#endif
}


enum CompressedStartupDataItems {
  kSnapshot = 0,
  kSnapshotContext,
  kLibraries,
  kExperimentalLibraries,
  kCompressedStartupDataCount
};

int V8::GetCompressedStartupDataCount() {
#ifdef COMPRESS_STARTUP_DATA_BZ2
  return kCompressedStartupDataCount;
#else
  return 0;
#endif
}


void V8::GetCompressedStartupData(StartupData* compressed_data) {
#ifdef COMPRESS_STARTUP_DATA_BZ2
  compressed_data[kSnapshot].data =
      reinterpret_cast<const char*>(i::Snapshot::data());
  compressed_data[kSnapshot].compressed_size = i::Snapshot::size();
  compressed_data[kSnapshot].raw_size = i::Snapshot::raw_size();

  compressed_data[kSnapshotContext].data =
      reinterpret_cast<const char*>(i::Snapshot::context_data());
  compressed_data[kSnapshotContext].compressed_size =
      i::Snapshot::context_size();
  compressed_data[kSnapshotContext].raw_size = i::Snapshot::context_raw_size();

  i::Vector<const i::byte> libraries_source = i::Natives::GetScriptsSource();
  compressed_data[kLibraries].data =
      reinterpret_cast<const char*>(libraries_source.start());
  compressed_data[kLibraries].compressed_size = libraries_source.length();
  compressed_data[kLibraries].raw_size = i::Natives::GetRawScriptsSize();

  i::Vector<const i::byte> exp_libraries_source =
      i::ExperimentalNatives::GetScriptsSource();
  compressed_data[kExperimentalLibraries].data =
      reinterpret_cast<const char*>(exp_libraries_source.start());
  compressed_data[kExperimentalLibraries].compressed_size =
      exp_libraries_source.length();
  compressed_data[kExperimentalLibraries].raw_size =
      i::ExperimentalNatives::GetRawScriptsSize();
#endif
}


void V8::SetDecompressedStartupData(StartupData* decompressed_data) {
#ifdef COMPRESS_STARTUP_DATA_BZ2
  ASSERT_EQ(i::Snapshot::raw_size(), decompressed_data[kSnapshot].raw_size);
  i::Snapshot::set_raw_data(
      reinterpret_cast<const i::byte*>(decompressed_data[kSnapshot].data));

  ASSERT_EQ(i::Snapshot::context_raw_size(),
            decompressed_data[kSnapshotContext].raw_size);
  i::Snapshot::set_context_raw_data(
      reinterpret_cast<const i::byte*>(
          decompressed_data[kSnapshotContext].data));

  ASSERT_EQ(i::Natives::GetRawScriptsSize(),
            decompressed_data[kLibraries].raw_size);
  i::Vector<const char> libraries_source(
      decompressed_data[kLibraries].data,
      decompressed_data[kLibraries].raw_size);
  i::Natives::SetRawScriptsSource(libraries_source);

  ASSERT_EQ(i::ExperimentalNatives::GetRawScriptsSize(),
            decompressed_data[kExperimentalLibraries].raw_size);
  i::Vector<const char> exp_libraries_source(
      decompressed_data[kExperimentalLibraries].data,
      decompressed_data[kExperimentalLibraries].raw_size);
  i::ExperimentalNatives::SetRawScriptsSource(exp_libraries_source);
#endif
}


void V8::SetFatalErrorHandler(FatalErrorCallback that) {
  i::Isolate* isolate = EnterIsolateIfNeeded();
  isolate->set_exception_behavior(that);
}


void V8::SetAllowCodeGenerationFromStringsCallback(
    AllowCodeGenerationFromStringsCallback callback) {
  i::Isolate* isolate = EnterIsolateIfNeeded();
  isolate->set_allow_code_gen_callback(callback);
}


void V8::SetFlagsFromString(const char* str, int length) {
  i::FlagList::SetFlagsFromString(str, length);
}


void V8::SetFlagsFromCommandLine(int* argc, char** argv, bool remove_flags) {
  i::FlagList::SetFlagsFromCommandLine(argc, argv, remove_flags);
}


v8::Handle<Value> ThrowException(v8::Handle<v8::Value> value) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::ThrowException()")) {
    return v8::Handle<Value>();
  }
  ENTER_V8(isolate);
  // If we're passed an empty handle, we throw an undefined exception
  // to deal more gracefully with out of memory situations.
  if (value.IsEmpty()) {
    isolate->ScheduleThrow(isolate->heap()->undefined_value());
  } else {
    isolate->ScheduleThrow(*Utils::OpenHandle(*value));
  }
  return v8::Undefined();
}


RegisteredExtension* RegisteredExtension::first_extension_ = NULL;


RegisteredExtension::RegisteredExtension(Extension* extension)
    : extension_(extension) { }


void RegisteredExtension::Register(RegisteredExtension* that) {
  that->next_ = first_extension_;
  first_extension_ = that;
}


void RegisteredExtension::UnregisterAll() {
  RegisteredExtension* re = first_extension_;
  while (re != NULL) {
    RegisteredExtension* next = re->next();
    delete re;
    re = next;
  }
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
  CHECK(source != NULL || source_length_ == 0);
}


v8::Handle<Primitive> Undefined() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!EnsureInitializedForIsolate(isolate, "v8::Undefined()")) {
    return v8::Handle<v8::Primitive>();
  }
  return v8::Handle<Primitive>(ToApi<Primitive>(
      isolate->factory()->undefined_value()));
}


v8::Handle<Primitive> Null() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!EnsureInitializedForIsolate(isolate, "v8::Null()")) {
    return v8::Handle<v8::Primitive>();
  }
  return v8::Handle<Primitive>(
      ToApi<Primitive>(isolate->factory()->null_value()));
}


v8::Handle<Boolean> True() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!EnsureInitializedForIsolate(isolate, "v8::True()")) {
    return v8::Handle<Boolean>();
  }
  return v8::Handle<Boolean>(
      ToApi<Boolean>(isolate->factory()->true_value()));
}


v8::Handle<Boolean> False() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!EnsureInitializedForIsolate(isolate, "v8::False()")) {
    return v8::Handle<Boolean>();
  }
  return v8::Handle<Boolean>(
      ToApi<Boolean>(isolate->factory()->false_value()));
}


ResourceConstraints::ResourceConstraints()
  : max_young_space_size_(0),
    max_old_space_size_(0),
    max_executable_size_(0),
    stack_limit_(NULL) { }


bool SetResourceConstraints(ResourceConstraints* constraints) {
  i::Isolate* isolate = EnterIsolateIfNeeded();

  int young_space_size = constraints->max_young_space_size();
  int old_gen_size = constraints->max_old_space_size();
  int max_executable_size = constraints->max_executable_size();
  if (young_space_size != 0 || old_gen_size != 0 || max_executable_size != 0) {
    // After initialization it's too late to change Heap constraints.
    ASSERT(!isolate->IsInitialized());
    bool result = isolate->heap()->ConfigureHeap(young_space_size / 2,
                                                 old_gen_size,
                                                 max_executable_size);
    if (!result) return false;
  }
  if (constraints->stack_limit() != NULL) {
    uintptr_t limit = reinterpret_cast<uintptr_t>(constraints->stack_limit());
    isolate->stack_guard()->SetStackLimit(limit);
  }
  return true;
}


i::Object** V8::GlobalizeReference(i::Isolate* isolate, i::Object** obj) {
  if (IsDeadCheck(isolate, "V8::Persistent::New")) return NULL;
  LOG_API(isolate, "Persistent::New");
  i::Handle<i::Object> result = isolate->global_handles()->Create(*obj);
#ifdef DEBUG
  (*obj)->Verify();
#endif  // DEBUG
  return result.location();
}


void V8::MakeWeak(i::Isolate* isolate,
                  i::Object** object,
                  void* parameters,
                  RevivableCallback weak_reference_callback,
                  NearDeathCallback near_death_callback) {
  ASSERT(isolate == i::Isolate::Current());
  LOG_API(isolate, "MakeWeak");
  isolate->global_handles()->MakeWeak(object,
                                      parameters,
                                      weak_reference_callback,
                                      near_death_callback);
}


void V8::ClearWeak(i::Isolate* isolate, i::Object** obj) {
  LOG_API(isolate, "ClearWeak");
  isolate->global_handles()->ClearWeakness(obj);
}


void V8::DisposeGlobal(i::Isolate* isolate, i::Object** obj) {
  ASSERT(isolate == i::Isolate::Current());
  LOG_API(isolate, "DisposeGlobal");
  if (!isolate->IsInitialized()) return;
  isolate->global_handles()->Destroy(obj);
}

// --- H a n d l e s ---


HandleScope::HandleScope() {
  Initialize(reinterpret_cast<Isolate*>(i::Isolate::Current()));
}


HandleScope::HandleScope(Isolate* isolate) {
  Initialize(isolate);
}


void HandleScope::Initialize(Isolate* isolate) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  API_ENTRY_CHECK(internal_isolate, "HandleScope::HandleScope");
  v8::ImplementationUtilities::HandleScopeData* current =
      internal_isolate->handle_scope_data();
  isolate_ = internal_isolate;
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  is_closed_ = false;
  current->level++;
}


HandleScope::~HandleScope() {
  if (!is_closed_) {
    Leave();
  }
}


void HandleScope::Leave() {
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate_->handle_scope_data();
  current->level--;
  ASSERT(current->level >= 0);
  current->next = prev_next_;
  if (current->limit != prev_limit_) {
    current->limit = prev_limit_;
    i::HandleScope::DeleteExtensions(isolate_);
  }

#ifdef ENABLE_EXTRA_CHECKS
  i::HandleScope::ZapRange(prev_next_, prev_limit_);
#endif
}


int HandleScope::NumberOfHandles() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!EnsureInitializedForIsolate(isolate, "HandleScope::NumberOfHandles")) {
    return 0;
  }
  return i::HandleScope::NumberOfHandles(isolate);
}


i::Object** HandleScope::CreateHandle(i::Object* value) {
  return i::HandleScope::CreateHandle(i::Isolate::Current(), value);
}


i::Object** HandleScope::CreateHandle(i::Isolate* isolate, i::Object* value) {
  ASSERT(isolate == i::Isolate::Current());
  return i::HandleScope::CreateHandle(isolate, value);
}


i::Object** HandleScope::CreateHandle(i::HeapObject* value) {
  ASSERT(value->IsHeapObject());
  return reinterpret_cast<i::Object**>(
      i::HandleScope::CreateHandle(value->GetIsolate(), value));
}


void Context::Enter() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Context::Enter()")) return;
  ENTER_V8(isolate);

  isolate->handle_scope_implementer()->EnterContext(env);

  isolate->handle_scope_implementer()->SaveContext(isolate->context());
  isolate->set_context(*env);
}


void Context::Exit() {
  // Exit is essentially a static function and doesn't use the
  // receiver, so we have to get the current isolate from the thread
  // local.
  i::Isolate* isolate = i::Isolate::Current();
  if (!isolate->IsInitialized()) return;

  if (!ApiCheck(isolate->handle_scope_implementer()->LeaveLastContext(),
                "v8::Context::Exit()",
                "Cannot exit non-entered context")) {
    return;
  }

  // Content of 'last_context' could be NULL.
  i::Context* last_context =
      isolate->handle_scope_implementer()->RestoreContext();
  isolate->set_context(last_context);
  isolate->set_context_exit_happened(true);
}


static void* DecodeSmiToAligned(i::Object* value, const char* location) {
  ApiCheck(value->IsSmi(), location, "Not a Smi");
  return reinterpret_cast<void*>(value);
}


static i::Smi* EncodeAlignedAsSmi(void* value, const char* location) {
  i::Smi* smi = reinterpret_cast<i::Smi*>(value);
  ApiCheck(smi->IsSmi(), location, "Pointer is not aligned");
  return smi;
}


static i::Handle<i::FixedArray> EmbedderDataFor(Context* context,
                                                int index,
                                                bool can_grow,
                                                const char* location) {
  i::Handle<i::Context> env = Utils::OpenHandle(context);
  bool ok = !IsDeadCheck(env->GetIsolate(), location) &&
      ApiCheck(env->IsNativeContext(), location, "Not a native context") &&
      ApiCheck(index >= 0, location, "Negative index");
  if (!ok) return i::Handle<i::FixedArray>();
  i::Handle<i::FixedArray> data(env->embedder_data());
  if (index < data->length()) return data;
  if (!can_grow) {
    Utils::ReportApiFailure(location, "Index too large");
    return i::Handle<i::FixedArray>();
  }
  int new_size = i::Max(index, data->length() << 1) + 1;
  data = env->GetIsolate()->factory()->CopySizeFixedArray(data, new_size);
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


void Context::SetEmbedderData(int index, v8::Handle<Value> value) {
  const char* location = "v8::Context::SetEmbedderData()";
  i::Handle<i::FixedArray> data = EmbedderDataFor(this, index, true, location);
  if (data.is_null()) return;
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  data->set(index, *val);
  ASSERT_EQ(*Utils::OpenHandle(*value),
            *Utils::OpenHandle(*GetEmbedderData(index)));
}


void* Context::SlowGetAlignedPointerFromEmbedderData(int index) {
  const char* location = "v8::Context::GetAlignedPointerFromEmbedderData()";
  i::Handle<i::FixedArray> data = EmbedderDataFor(this, index, false, location);
  if (data.is_null()) return NULL;
  return DecodeSmiToAligned(data->get(index), location);
}


void Context::SetAlignedPointerInEmbedderData(int index, void* value) {
  const char* location = "v8::Context::SetAlignedPointerInEmbedderData()";
  i::Handle<i::FixedArray> data = EmbedderDataFor(this, index, true, location);
  data->set(index, EncodeAlignedAsSmi(value, location));
  ASSERT_EQ(value, GetAlignedPointerFromEmbedderData(index));
}


i::Object** v8::HandleScope::RawClose(i::Object** value) {
  if (!ApiCheck(!is_closed_,
                "v8::HandleScope::Close()",
                "Local scope has already been closed")) {
    return 0;
  }
  LOG_API(isolate_, "CloseHandleScope");

  // Read the result before popping the handle block.
  i::Object* result = NULL;
  if (value != NULL) {
    result = *value;
  }
  is_closed_ = true;
  Leave();

  if (value == NULL) {
    return NULL;
  }

  // Allocate a new handle on the previous handle block.
  i::Handle<i::Object> handle(result, isolate_);
  return handle.location();
}


// --- N e a n d e r ---


// A constructor cannot easily return an error value, therefore it is necessary
// to check for a dead VM with ON_BAILOUT before constructing any Neander
// objects.  To remind you about this there is no HandleScope in the
// NeanderObject constructor.  When you add one to the site calling the
// constructor you should check that you ensured the VM was not dead first.
NeanderObject::NeanderObject(int size) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Nowhere");
  ENTER_V8(isolate);
  value_ = isolate->factory()->NewNeanderObject();
  i::Handle<i::FixedArray> elements = isolate->factory()->NewFixedArray(size);
  value_->set_elements(*elements);
}


int NeanderObject::size() {
  return i::FixedArray::cast(value_->elements())->length();
}


NeanderArray::NeanderArray() : obj_(2) {
  obj_.set(0, i::Smi::FromInt(0));
}


int NeanderArray::length() {
  return i::Smi::cast(obj_.get(0))->value();
}


i::Object* NeanderArray::get(int offset) {
  ASSERT(0 <= offset);
  ASSERT(offset < length());
  return obj_.get(offset + 1);
}


// This method cannot easily return an error value, therefore it is necessary
// to check for a dead VM with ON_BAILOUT before calling it.  To remind you
// about this there is no HandleScope in this method.  When you add one to the
// site calling this method you should check that you ensured the VM was not
// dead first.
void NeanderArray::add(i::Handle<i::Object> value) {
  int length = this->length();
  int size = obj_.size();
  if (length == size - 1) {
    i::Handle<i::FixedArray> new_elms = FACTORY->NewFixedArray(2 * size);
    for (int i = 0; i < length; i++)
      new_elms->set(i + 1, get(i));
    obj_.value()->set_elements(*new_elms);
  }
  obj_.set(length + 1, *value);
  obj_.set(0, i::Smi::FromInt(length + 1));
}


void NeanderArray::set(int index, i::Object* value) {
  if (index < 0 || index >= this->length()) return;
  obj_.set(index + 1, value);
}


// --- T e m p l a t e ---


static void InitializeTemplate(i::Handle<i::TemplateInfo> that, int type) {
  that->set_tag(i::Smi::FromInt(type));
}


void Template::Set(v8::Handle<String> name, v8::Handle<Data> value,
                   v8::PropertyAttribute attribute) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Template::Set()")) return;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::Object> list(Utils::OpenHandle(this)->property_list(), isolate);
  if (list->IsUndefined()) {
    list = NeanderArray().value();
    Utils::OpenHandle(this)->set_property_list(*list);
  }
  NeanderArray array(list);
  array.add(Utils::OpenHandle(*name));
  array.add(Utils::OpenHandle(*value));
  array.add(Utils::OpenHandle(*v8::Integer::New(attribute)));
}


// --- F u n c t i o n   T e m p l a t e ---
static void InitializeFunctionTemplate(
      i::Handle<i::FunctionTemplateInfo> info) {
  info->set_tag(i::Smi::FromInt(Consts::FUNCTION_TEMPLATE));
  info->set_flag(0);
}


Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::PrototypeTemplate()")) {
    return Local<ObjectTemplate>();
  }
  ENTER_V8(isolate);
  i::Handle<i::Object> result(Utils::OpenHandle(this)->prototype_template(),
                              isolate);
  if (result->IsUndefined()) {
    result = Utils::OpenHandle(*ObjectTemplate::New());
    Utils::OpenHandle(this)->set_prototype_template(*result);
  }
  return Local<ObjectTemplate>(ToApi<ObjectTemplate>(result));
}


void FunctionTemplate::Inherit(v8::Handle<FunctionTemplate> value) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::Inherit()")) return;
  ENTER_V8(isolate);
  Utils::OpenHandle(this)->set_parent_template(*Utils::OpenHandle(*value));
}


Local<FunctionTemplate> FunctionTemplate::New(InvocationCallback callback,
    v8::Handle<Value> data, v8::Handle<Signature> signature, int length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::FunctionTemplate::New()");
  LOG_API(isolate, "FunctionTemplate::New");
  ENTER_V8(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::FUNCTION_TEMPLATE_INFO_TYPE);
  i::Handle<i::FunctionTemplateInfo> obj =
      i::Handle<i::FunctionTemplateInfo>::cast(struct_obj);
  InitializeFunctionTemplate(obj);
  int next_serial_number = isolate->next_serial_number();
  isolate->set_next_serial_number(next_serial_number + 1);
  obj->set_serial_number(i::Smi::FromInt(next_serial_number));
  if (callback != 0) {
    if (data.IsEmpty()) data = v8::Undefined();
    Utils::ToLocal(obj)->SetCallHandler(callback, data);
  }
  obj->set_length(length);
  obj->set_undetectable(false);
  obj->set_needs_access_check(false);

  if (!signature.IsEmpty())
    obj->set_signature(*Utils::OpenHandle(*signature));
  return Utils::ToLocal(obj);
}


Local<Signature> Signature::New(Handle<FunctionTemplate> receiver,
      int argc, Handle<FunctionTemplate> argv[]) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Signature::New()");
  LOG_API(isolate, "Signature::New");
  ENTER_V8(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::SIGNATURE_INFO_TYPE);
  i::Handle<i::SignatureInfo> obj =
      i::Handle<i::SignatureInfo>::cast(struct_obj);
  if (!receiver.IsEmpty()) obj->set_receiver(*Utils::OpenHandle(*receiver));
  if (argc > 0) {
    i::Handle<i::FixedArray> args = isolate->factory()->NewFixedArray(argc);
    for (int i = 0; i < argc; i++) {
      if (!argv[i].IsEmpty())
        args->set(i, *Utils::OpenHandle(*argv[i]));
    }
    obj->set_args(*args);
  }
  return Utils::ToLocal(obj);
}


Local<AccessorSignature> AccessorSignature::New(
      Handle<FunctionTemplate> receiver) {
  return Utils::AccessorSignatureToLocal(Utils::OpenHandle(*receiver));
}


template<typename Operation>
static Local<Operation> NewDescriptor(
    Isolate* isolate,
    const i::DeclaredAccessorDescriptorData& data,
    Data* previous_descriptor
    ) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::DeclaredAccessorDescriptor> previous =
      i::Handle<i::DeclaredAccessorDescriptor>();
  if (previous_descriptor != NULL) {
    previous = Utils::OpenHandle(
      static_cast<DeclaredAccessorDescriptor*>(previous_descriptor));
  }
  i::Handle<i::DeclaredAccessorDescriptor> descriptor =
      i::DeclaredAccessorDescriptor::Create(internal_isolate, data, previous);
  return Local<Operation>(
      reinterpret_cast<Operation*>(*Utils::ToLocal(descriptor)));
}


Local<RawOperationDescriptor>
  ObjectOperationDescriptor::NewInternalFieldDereference(
    Isolate* isolate,
    int internal_field) {
  i::DeclaredAccessorDescriptorData data;
  data.type = i::kDescriptorObjectDereference;
  data.object_dereference_descriptor.internal_field = internal_field;
  return NewDescriptor<RawOperationDescriptor>(isolate, data, NULL);
}


Local<RawOperationDescriptor> RawOperationDescriptor::NewRawShift(
    Isolate* isolate,
    int16_t byte_offset) {
  i::DeclaredAccessorDescriptorData data;
  data.type = i::kDescriptorPointerShift;
  data.pointer_shift_descriptor.byte_offset = byte_offset;
  return NewDescriptor<RawOperationDescriptor>(isolate, data, this);
}


Local<DeclaredAccessorDescriptor> RawOperationDescriptor::NewHandleDereference(
    Isolate* isolate) {
  i::DeclaredAccessorDescriptorData data;
  data.type = i::kDescriptorReturnObject;
  return NewDescriptor<DeclaredAccessorDescriptor>(isolate, data, this);
}


Local<RawOperationDescriptor> RawOperationDescriptor::NewRawDereference(
    Isolate* isolate) {
  i::DeclaredAccessorDescriptorData data;
  data.type = i::kDescriptorPointerDereference;
  return NewDescriptor<RawOperationDescriptor>(isolate, data, this);
}


Local<DeclaredAccessorDescriptor> RawOperationDescriptor::NewPointerCompare(
    Isolate* isolate,
    void* compare_value) {
  i::DeclaredAccessorDescriptorData data;
  data.type = i::kDescriptorPointerCompare;
  data.pointer_compare_descriptor.compare_value = compare_value;
  return NewDescriptor<DeclaredAccessorDescriptor>(isolate, data, this);
}


Local<DeclaredAccessorDescriptor> RawOperationDescriptor::NewPrimitiveValue(
    Isolate* isolate,
    DeclaredAccessorDescriptorDataType data_type,
    uint8_t bool_offset) {
  i::DeclaredAccessorDescriptorData data;
  data.type = i::kDescriptorPrimitiveValue;
  data.primitive_value_descriptor.data_type = data_type;
  data.primitive_value_descriptor.bool_offset = bool_offset;
  return NewDescriptor<DeclaredAccessorDescriptor>(isolate, data, this);
}


template<typename T>
static Local<DeclaredAccessorDescriptor> NewBitmaskCompare(
    Isolate* isolate,
    T bitmask,
    T compare_value,
    RawOperationDescriptor* operation) {
  i::DeclaredAccessorDescriptorData data;
  data.type = i::kDescriptorBitmaskCompare;
  data.bitmask_compare_descriptor.bitmask = bitmask;
  data.bitmask_compare_descriptor.compare_value = compare_value;
  data.bitmask_compare_descriptor.size = sizeof(T);
  return NewDescriptor<DeclaredAccessorDescriptor>(isolate, data, operation);
}


Local<DeclaredAccessorDescriptor> RawOperationDescriptor::NewBitmaskCompare8(
    Isolate* isolate,
    uint8_t bitmask,
    uint8_t compare_value) {
  return NewBitmaskCompare(isolate, bitmask, compare_value, this);
}


Local<DeclaredAccessorDescriptor> RawOperationDescriptor::NewBitmaskCompare16(
    Isolate* isolate,
    uint16_t bitmask,
    uint16_t compare_value) {
  return NewBitmaskCompare(isolate, bitmask, compare_value, this);
}


Local<DeclaredAccessorDescriptor> RawOperationDescriptor::NewBitmaskCompare32(
    Isolate* isolate,
    uint32_t bitmask,
    uint32_t compare_value) {
  return NewBitmaskCompare(isolate, bitmask, compare_value, this);
}


Local<TypeSwitch> TypeSwitch::New(Handle<FunctionTemplate> type) {
  Handle<FunctionTemplate> types[1] = { type };
  return TypeSwitch::New(1, types);
}


Local<TypeSwitch> TypeSwitch::New(int argc, Handle<FunctionTemplate> types[]) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::TypeSwitch::New()");
  LOG_API(isolate, "TypeSwitch::New");
  ENTER_V8(isolate);
  i::Handle<i::FixedArray> vector = isolate->factory()->NewFixedArray(argc);
  for (int i = 0; i < argc; i++)
    vector->set(i, *Utils::OpenHandle(*types[i]));
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::TYPE_SWITCH_INFO_TYPE);
  i::Handle<i::TypeSwitchInfo> obj =
      i::Handle<i::TypeSwitchInfo>::cast(struct_obj);
  obj->set_types(*vector);
  return Utils::ToLocal(obj);
}


int TypeSwitch::match(v8::Handle<Value> value) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "TypeSwitch::match");
  USE(isolate);
  i::Handle<i::Object> obj = Utils::OpenHandle(*value);
  i::Handle<i::TypeSwitchInfo> info = Utils::OpenHandle(this);
  i::FixedArray* types = i::FixedArray::cast(info->types());
  for (int i = 0; i < types->length(); i++) {
    if (obj->IsInstanceOf(i::FunctionTemplateInfo::cast(types->get(i))))
      return i + 1;
  }
  return 0;
}


#define SET_FIELD_WRAPPED(obj, setter, cdata) do {    \
    i::Handle<i::Object> foreign = FromCData(cdata);  \
    (obj)->setter(*foreign);                          \
  } while (false)


void FunctionTemplate::SetCallHandler(InvocationCallback callback,
                                      v8::Handle<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::SetCallHandler()")) return;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::CALL_HANDLER_INFO_TYPE);
  i::Handle<i::CallHandlerInfo> obj =
      i::Handle<i::CallHandlerInfo>::cast(struct_obj);
  SET_FIELD_WRAPPED(obj, set_callback, callback);
  if (data.IsEmpty()) data = v8::Undefined();
  obj->set_data(*Utils::OpenHandle(*data));
  Utils::OpenHandle(this)->set_call_code(*obj);
}


static i::Handle<i::AccessorInfo> SetAccessorInfoProperties(
    i::Handle<i::AccessorInfo> obj,
    v8::Handle<String> name,
    v8::AccessControl settings,
    v8::PropertyAttribute attributes,
    v8::Handle<AccessorSignature> signature) {
  obj->set_name(*Utils::OpenHandle(*name));
  if (settings & ALL_CAN_READ) obj->set_all_can_read(true);
  if (settings & ALL_CAN_WRITE) obj->set_all_can_write(true);
  if (settings & PROHIBITS_OVERWRITING) obj->set_prohibits_overwriting(true);
  obj->set_property_attributes(static_cast<PropertyAttributes>(attributes));
  if (!signature.IsEmpty()) {
    obj->set_expected_receiver_type(*Utils::OpenHandle(*signature));
  }
  return obj;
}


static i::Handle<i::AccessorInfo> MakeAccessorInfo(
      v8::Handle<String> name,
      AccessorGetter getter,
      AccessorSetter setter,
      v8::Handle<Value> data,
      v8::AccessControl settings,
      v8::PropertyAttribute attributes,
      v8::Handle<AccessorSignature> signature) {
  i::Isolate* isolate = Utils::OpenHandle(*name)->GetIsolate();
  i::Handle<i::ExecutableAccessorInfo> obj =
      isolate->factory()->NewExecutableAccessorInfo();
  SET_FIELD_WRAPPED(obj, set_getter, getter);
  SET_FIELD_WRAPPED(obj, set_setter, setter);
  if (data.IsEmpty()) data = v8::Undefined();
  obj->set_data(*Utils::OpenHandle(*data));
  return SetAccessorInfoProperties(obj, name, settings, attributes, signature);
}


static i::Handle<i::AccessorInfo> MakeAccessorInfo(
      v8::Handle<String> name,
      v8::Handle<v8::DeclaredAccessorDescriptor> descriptor,
      v8::AccessControl settings,
      v8::PropertyAttribute attributes,
      v8::Handle<AccessorSignature> signature) {
  i::Isolate* isolate = Utils::OpenHandle(*name)->GetIsolate();
  if (descriptor.IsEmpty()) return i::Handle<i::DeclaredAccessorInfo>();
  i::Handle<i::DeclaredAccessorInfo> obj =
      isolate->factory()->NewDeclaredAccessorInfo();
  obj->set_descriptor(*Utils::OpenHandle(*descriptor));
  return SetAccessorInfoProperties(obj, name, settings, attributes, signature);
}


Local<ObjectTemplate> FunctionTemplate::InstanceTemplate() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::InstanceTemplate()")
      || EmptyCheck("v8::FunctionTemplate::InstanceTemplate()", this))
    return Local<ObjectTemplate>();
  ENTER_V8(isolate);
  if (Utils::OpenHandle(this)->instance_template()->IsUndefined()) {
    Local<ObjectTemplate> templ =
        ObjectTemplate::New(v8::Handle<FunctionTemplate>(this));
    Utils::OpenHandle(this)->set_instance_template(*Utils::OpenHandle(*templ));
  }
  i::Handle<i::ObjectTemplateInfo> result(i::ObjectTemplateInfo::cast(
        Utils::OpenHandle(this)->instance_template()));
  return Utils::ToLocal(result);
}


void FunctionTemplate::SetLength(int length) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::SetLength()")) return;
  ENTER_V8(isolate);
  Utils::OpenHandle(this)->set_length(length);
}


void FunctionTemplate::SetClassName(Handle<String> name) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::SetClassName()")) return;
  ENTER_V8(isolate);
  Utils::OpenHandle(this)->set_class_name(*Utils::OpenHandle(*name));
}


void FunctionTemplate::SetHiddenPrototype(bool value) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::SetHiddenPrototype()")) {
    return;
  }
  ENTER_V8(isolate);
  Utils::OpenHandle(this)->set_hidden_prototype(value);
}


void FunctionTemplate::ReadOnlyPrototype() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::ReadOnlyPrototype()")) {
    return;
  }
  ENTER_V8(isolate);
  Utils::OpenHandle(this)->set_read_only_prototype(true);
}


void FunctionTemplate::SetNamedInstancePropertyHandler(
      NamedPropertyGetter getter,
      NamedPropertySetter setter,
      NamedPropertyQuery query,
      NamedPropertyDeleter remover,
      NamedPropertyEnumerator enumerator,
      Handle<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate,
                  "v8::FunctionTemplate::SetNamedInstancePropertyHandler()")) {
    return;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::INTERCEPTOR_INFO_TYPE);
  i::Handle<i::InterceptorInfo> obj =
      i::Handle<i::InterceptorInfo>::cast(struct_obj);

  if (getter != 0) SET_FIELD_WRAPPED(obj, set_getter, getter);
  if (setter != 0) SET_FIELD_WRAPPED(obj, set_setter, setter);
  if (query != 0) SET_FIELD_WRAPPED(obj, set_query, query);
  if (remover != 0) SET_FIELD_WRAPPED(obj, set_deleter, remover);
  if (enumerator != 0) SET_FIELD_WRAPPED(obj, set_enumerator, enumerator);

  if (data.IsEmpty()) data = v8::Undefined();
  obj->set_data(*Utils::OpenHandle(*data));
  Utils::OpenHandle(this)->set_named_property_handler(*obj);
}


void FunctionTemplate::SetIndexedInstancePropertyHandler(
      IndexedPropertyGetter getter,
      IndexedPropertySetter setter,
      IndexedPropertyQuery query,
      IndexedPropertyDeleter remover,
      IndexedPropertyEnumerator enumerator,
      Handle<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate,
        "v8::FunctionTemplate::SetIndexedInstancePropertyHandler()")) {
    return;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::INTERCEPTOR_INFO_TYPE);
  i::Handle<i::InterceptorInfo> obj =
      i::Handle<i::InterceptorInfo>::cast(struct_obj);

  if (getter != 0) SET_FIELD_WRAPPED(obj, set_getter, getter);
  if (setter != 0) SET_FIELD_WRAPPED(obj, set_setter, setter);
  if (query != 0) SET_FIELD_WRAPPED(obj, set_query, query);
  if (remover != 0) SET_FIELD_WRAPPED(obj, set_deleter, remover);
  if (enumerator != 0) SET_FIELD_WRAPPED(obj, set_enumerator, enumerator);

  if (data.IsEmpty()) data = v8::Undefined();
  obj->set_data(*Utils::OpenHandle(*data));
  Utils::OpenHandle(this)->set_indexed_property_handler(*obj);
}


void FunctionTemplate::SetInstanceCallAsFunctionHandler(
      InvocationCallback callback,
      Handle<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate,
                  "v8::FunctionTemplate::SetInstanceCallAsFunctionHandler()")) {
    return;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::CALL_HANDLER_INFO_TYPE);
  i::Handle<i::CallHandlerInfo> obj =
      i::Handle<i::CallHandlerInfo>::cast(struct_obj);
  SET_FIELD_WRAPPED(obj, set_callback, callback);
  if (data.IsEmpty()) data = v8::Undefined();
  obj->set_data(*Utils::OpenHandle(*data));
  Utils::OpenHandle(this)->set_instance_call_handler(*obj);
}


// --- O b j e c t T e m p l a t e ---


Local<ObjectTemplate> ObjectTemplate::New() {
  return New(Local<FunctionTemplate>());
}


Local<ObjectTemplate> ObjectTemplate::New(
      v8::Handle<FunctionTemplate> constructor) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::New()")) {
    return Local<ObjectTemplate>();
  }
  EnsureInitializedForIsolate(isolate, "v8::ObjectTemplate::New()");
  LOG_API(isolate, "ObjectTemplate::New");
  ENTER_V8(isolate);
  i::Handle<i::Struct> struct_obj =
      isolate->factory()->NewStruct(i::OBJECT_TEMPLATE_INFO_TYPE);
  i::Handle<i::ObjectTemplateInfo> obj =
      i::Handle<i::ObjectTemplateInfo>::cast(struct_obj);
  InitializeTemplate(obj, Consts::OBJECT_TEMPLATE);
  if (!constructor.IsEmpty())
    obj->set_constructor(*Utils::OpenHandle(*constructor));
  obj->set_internal_field_count(i::Smi::FromInt(0));
  return Utils::ToLocal(obj);
}


// Ensure that the object template has a constructor.  If no
// constructor is available we create one.
static void EnsureConstructor(ObjectTemplate* object_template) {
  if (Utils::OpenHandle(object_template)->constructor()->IsUndefined()) {
    Local<FunctionTemplate> templ = FunctionTemplate::New();
    i::Handle<i::FunctionTemplateInfo> constructor = Utils::OpenHandle(*templ);
    constructor->set_instance_template(*Utils::OpenHandle(object_template));
    Utils::OpenHandle(object_template)->set_constructor(*constructor);
  }
}


static inline void AddPropertyToFunctionTemplate(
    i::Handle<i::FunctionTemplateInfo> cons,
    i::Handle<i::AccessorInfo> obj) {
  i::Handle<i::Object> list(cons->property_accessors(), cons->GetIsolate());
  if (list->IsUndefined()) {
    list = NeanderArray().value();
    cons->set_property_accessors(*list);
  }
  NeanderArray array(list);
  array.add(obj);
}


void ObjectTemplate::SetAccessor(v8::Handle<String> name,
                                 AccessorGetter getter,
                                 AccessorSetter setter,
                                 v8::Handle<Value> data,
                                 AccessControl settings,
                                 PropertyAttribute attribute,
                                 v8::Handle<AccessorSignature> signature) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::SetAccessor()")) return;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  EnsureConstructor(this);
  i::FunctionTemplateInfo* constructor =
      i::FunctionTemplateInfo::cast(Utils::OpenHandle(this)->constructor());
  i::Handle<i::FunctionTemplateInfo> cons(constructor);
  i::Handle<i::AccessorInfo> obj = MakeAccessorInfo(name, getter, setter, data,
                                                      settings, attribute,
                                                      signature);
  AddPropertyToFunctionTemplate(cons, obj);
}


bool ObjectTemplate::SetAccessor(Handle<String> name,
                                 Handle<DeclaredAccessorDescriptor> descriptor,
                                 AccessControl settings,
                                 PropertyAttribute attribute,
                                 Handle<AccessorSignature> signature) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::SetAccessor()")) return false;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  EnsureConstructor(this);
  i::FunctionTemplateInfo* constructor =
      i::FunctionTemplateInfo::cast(Utils::OpenHandle(this)->constructor());
  i::Handle<i::FunctionTemplateInfo> cons(constructor);
  i::Handle<i::AccessorInfo> obj = MakeAccessorInfo(
      name, descriptor, settings, attribute, signature);
  if (obj.is_null()) return false;
  AddPropertyToFunctionTemplate(cons, obj);
  return true;
}


void ObjectTemplate::SetNamedPropertyHandler(NamedPropertyGetter getter,
                                             NamedPropertySetter setter,
                                             NamedPropertyQuery query,
                                             NamedPropertyDeleter remover,
                                             NamedPropertyEnumerator enumerator,
                                             Handle<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::SetNamedPropertyHandler()")) {
    return;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  EnsureConstructor(this);
  i::FunctionTemplateInfo* constructor =
      i::FunctionTemplateInfo::cast(Utils::OpenHandle(this)->constructor());
  i::Handle<i::FunctionTemplateInfo> cons(constructor);
  Utils::ToLocal(cons)->SetNamedInstancePropertyHandler(getter,
                                                        setter,
                                                        query,
                                                        remover,
                                                        enumerator,
                                                        data);
}


void ObjectTemplate::MarkAsUndetectable() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::MarkAsUndetectable()")) return;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  EnsureConstructor(this);
  i::FunctionTemplateInfo* constructor =
      i::FunctionTemplateInfo::cast(Utils::OpenHandle(this)->constructor());
  i::Handle<i::FunctionTemplateInfo> cons(constructor);
  cons->set_undetectable(true);
}


void ObjectTemplate::SetAccessCheckCallbacks(
      NamedSecurityCallback named_callback,
      IndexedSecurityCallback indexed_callback,
      Handle<Value> data,
      bool turned_on_by_default) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::SetAccessCheckCallbacks()")) {
    return;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  EnsureConstructor(this);

  i::Handle<i::Struct> struct_info =
      isolate->factory()->NewStruct(i::ACCESS_CHECK_INFO_TYPE);
  i::Handle<i::AccessCheckInfo> info =
      i::Handle<i::AccessCheckInfo>::cast(struct_info);

  SET_FIELD_WRAPPED(info, set_named_callback, named_callback);
  SET_FIELD_WRAPPED(info, set_indexed_callback, indexed_callback);

  if (data.IsEmpty()) data = v8::Undefined();
  info->set_data(*Utils::OpenHandle(*data));

  i::FunctionTemplateInfo* constructor =
      i::FunctionTemplateInfo::cast(Utils::OpenHandle(this)->constructor());
  i::Handle<i::FunctionTemplateInfo> cons(constructor);
  cons->set_access_check_info(*info);
  cons->set_needs_access_check(turned_on_by_default);
}


void ObjectTemplate::SetIndexedPropertyHandler(
      IndexedPropertyGetter getter,
      IndexedPropertySetter setter,
      IndexedPropertyQuery query,
      IndexedPropertyDeleter remover,
      IndexedPropertyEnumerator enumerator,
      Handle<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::SetIndexedPropertyHandler()")) {
    return;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  EnsureConstructor(this);
  i::FunctionTemplateInfo* constructor =
      i::FunctionTemplateInfo::cast(Utils::OpenHandle(this)->constructor());
  i::Handle<i::FunctionTemplateInfo> cons(constructor);
  Utils::ToLocal(cons)->SetIndexedInstancePropertyHandler(getter,
                                                          setter,
                                                          query,
                                                          remover,
                                                          enumerator,
                                                          data);
}


void ObjectTemplate::SetCallAsFunctionHandler(InvocationCallback callback,
                                              Handle<Value> data) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate,
                  "v8::ObjectTemplate::SetCallAsFunctionHandler()")) {
    return;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  EnsureConstructor(this);
  i::FunctionTemplateInfo* constructor =
      i::FunctionTemplateInfo::cast(Utils::OpenHandle(this)->constructor());
  i::Handle<i::FunctionTemplateInfo> cons(constructor);
  Utils::ToLocal(cons)->SetInstanceCallAsFunctionHandler(callback, data);
}


int ObjectTemplate::InternalFieldCount() {
  if (IsDeadCheck(Utils::OpenHandle(this)->GetIsolate(),
                  "v8::ObjectTemplate::InternalFieldCount()")) {
    return 0;
  }
  return i::Smi::cast(Utils::OpenHandle(this)->internal_field_count())->value();
}


void ObjectTemplate::SetInternalFieldCount(int value) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::SetInternalFieldCount()")) {
    return;
  }
  if (!ApiCheck(i::Smi::IsValid(value),
                "v8::ObjectTemplate::SetInternalFieldCount()",
                "Invalid internal field count")) {
    return;
  }
  ENTER_V8(isolate);
  if (value > 0) {
    // The internal field count is set by the constructor function's
    // construct code, so we ensure that there is a constructor
    // function to do the setting.
    EnsureConstructor(this);
  }
  Utils::OpenHandle(this)->set_internal_field_count(i::Smi::FromInt(value));
}


// --- S c r i p t D a t a ---


ScriptData* ScriptData::PreCompile(const char* input, int length) {
  i::Utf8ToUtf16CharacterStream stream(
      reinterpret_cast<const unsigned char*>(input), length);
  return i::PreParserApi::PreParse(&stream);
}


ScriptData* ScriptData::PreCompile(v8::Handle<String> source) {
  i::Handle<i::String> str = Utils::OpenHandle(*source);
  if (str->IsExternalTwoByteString()) {
    i::ExternalTwoByteStringUtf16CharacterStream stream(
      i::Handle<i::ExternalTwoByteString>::cast(str), 0, str->length());
    return i::PreParserApi::PreParse(&stream);
  } else {
    i::GenericStringUtf16CharacterStream stream(str, 0, str->length());
    return i::PreParserApi::PreParse(&stream);
  }
}


ScriptData* ScriptData::New(const char* data, int length) {
  // Return an empty ScriptData if the length is obviously invalid.
  if (length % sizeof(unsigned) != 0) {
    return new i::ScriptDataImpl();
  }

  // Copy the data to ensure it is properly aligned.
  int deserialized_data_length = length / sizeof(unsigned);
  // If aligned, don't create a copy of the data.
  if (reinterpret_cast<intptr_t>(data) % sizeof(unsigned) == 0) {
    return new i::ScriptDataImpl(data, length);
  }
  // Copy the data to align it.
  unsigned* deserialized_data = i::NewArray<unsigned>(deserialized_data_length);
  i::CopyBytes(reinterpret_cast<char*>(deserialized_data),
               data, static_cast<size_t>(length));

  return new i::ScriptDataImpl(
      i::Vector<unsigned>(deserialized_data, deserialized_data_length));
}


// --- S c r i p t ---


Local<Script> Script::New(v8::Handle<String> source,
                          v8::ScriptOrigin* origin,
                          v8::ScriptData* pre_data,
                          v8::Handle<String> script_data) {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Script::New()", return Local<Script>());
  LOG_API(isolate, "Script::New");
  ENTER_V8(isolate);
  i::SharedFunctionInfo* raw_result = NULL;
  { i::HandleScope scope(isolate);
    i::Handle<i::String> str = Utils::OpenHandle(*source);
    i::Handle<i::Object> name_obj;
    int line_offset = 0;
    int column_offset = 0;
    if (origin != NULL) {
      if (!origin->ResourceName().IsEmpty()) {
        name_obj = Utils::OpenHandle(*origin->ResourceName());
      }
      if (!origin->ResourceLineOffset().IsEmpty()) {
        line_offset = static_cast<int>(origin->ResourceLineOffset()->Value());
      }
      if (!origin->ResourceColumnOffset().IsEmpty()) {
        column_offset =
            static_cast<int>(origin->ResourceColumnOffset()->Value());
      }
    }
    EXCEPTION_PREAMBLE(isolate);
    i::ScriptDataImpl* pre_data_impl =
        static_cast<i::ScriptDataImpl*>(pre_data);
    // We assert that the pre-data is sane, even though we can actually
    // handle it if it turns out not to be in release mode.
    ASSERT(pre_data_impl == NULL || pre_data_impl->SanityCheck());
    // If the pre-data isn't sane we simply ignore it
    if (pre_data_impl != NULL && !pre_data_impl->SanityCheck()) {
      pre_data_impl = NULL;
    }
    i::Handle<i::SharedFunctionInfo> result =
      i::Compiler::Compile(str,
                           name_obj,
                           line_offset,
                           column_offset,
                           isolate->global_context(),
                           NULL,
                           pre_data_impl,
                           Utils::OpenHandle(*script_data, true),
                           i::NOT_NATIVES_CODE);
    has_pending_exception = result.is_null();
    EXCEPTION_BAILOUT_CHECK(isolate, Local<Script>());
    raw_result = *result;
  }
  i::Handle<i::SharedFunctionInfo> result(raw_result, isolate);
  return Local<Script>(ToApi<Script>(result));
}


Local<Script> Script::New(v8::Handle<String> source,
                          v8::Handle<Value> file_name) {
  ScriptOrigin origin(file_name);
  return New(source, &origin);
}


Local<Script> Script::Compile(v8::Handle<String> source,
                              v8::ScriptOrigin* origin,
                              v8::ScriptData* pre_data,
                              v8::Handle<String> script_data) {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Script::Compile()", return Local<Script>());
  LOG_API(isolate, "Script::Compile");
  ENTER_V8(isolate);
  Local<Script> generic = New(source, origin, pre_data, script_data);
  if (generic.IsEmpty())
    return generic;
  i::Handle<i::Object> obj = Utils::OpenHandle(*generic);
  i::Handle<i::SharedFunctionInfo> function =
      i::Handle<i::SharedFunctionInfo>(i::SharedFunctionInfo::cast(*obj));
  i::Handle<i::JSFunction> result =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          function,
          isolate->global_context());
  return Local<Script>(ToApi<Script>(result));
}


Local<Script> Script::Compile(v8::Handle<String> source,
                              v8::Handle<Value> file_name,
                              v8::Handle<String> script_data) {
  ScriptOrigin origin(file_name);
  return Compile(source, &origin, 0, script_data);
}


Local<Value> Script::Run() {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Script::Run()", return Local<Value>());
  LOG_API(isolate, "Script::Run");
  ENTER_V8(isolate);
  i::Logger::TimerEventScope timer_scope(
      isolate, i::Logger::TimerEventScope::v8_execute);
  i::Object* raw_result = NULL;
  {
    i::HandleScope scope(isolate);
    i::Handle<i::Object> obj = Utils::OpenHandle(this);
    i::Handle<i::JSFunction> fun;
    if (obj->IsSharedFunctionInfo()) {
      i::Handle<i::SharedFunctionInfo>
          function_info(i::SharedFunctionInfo::cast(*obj), isolate);
      fun = isolate->factory()->NewFunctionFromSharedFunctionInfo(
          function_info, isolate->global_context());
    } else {
      fun = i::Handle<i::JSFunction>(i::JSFunction::cast(*obj), isolate);
    }
    EXCEPTION_PREAMBLE(isolate);
    i::Handle<i::Object> receiver(
        isolate->context()->global_proxy(), isolate);
    i::Handle<i::Object> result =
        i::Execution::Call(fun, receiver, 0, NULL, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK_DO_CALLBACK(isolate, Local<Value>());
    raw_result = *result;
  }
  i::Handle<i::Object> result(raw_result, isolate);
  return Utils::ToLocal(result);
}


static i::Handle<i::SharedFunctionInfo> OpenScript(Script* script) {
  i::Handle<i::Object> obj = Utils::OpenHandle(script);
  i::Handle<i::SharedFunctionInfo> result;
  if (obj->IsSharedFunctionInfo()) {
    result =
        i::Handle<i::SharedFunctionInfo>(i::SharedFunctionInfo::cast(*obj));
  } else {
    result =
        i::Handle<i::SharedFunctionInfo>(i::JSFunction::cast(*obj)->shared());
  }
  return result;
}


Local<Value> Script::Id() {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Script::Id()", return Local<Value>());
  LOG_API(isolate, "Script::Id");
  i::Object* raw_id = NULL;
  {
    i::HandleScope scope(isolate);
    i::Handle<i::SharedFunctionInfo> function_info = OpenScript(this);
    i::Handle<i::Script> script(i::Script::cast(function_info->script()));
    i::Handle<i::Object> id(script->id(), isolate);
    raw_id = *id;
  }
  i::Handle<i::Object> id(raw_id, isolate);
  return Utils::ToLocal(id);
}


int Script::GetLineNumber(int code_pos) {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Script::GetLineNumber()", return -1);
  LOG_API(isolate, "Script::GetLineNumber");
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsScript()) {
    i::Handle<i::Script> script = i::Handle<i::Script>(i::Script::cast(*obj));
    return i::GetScriptLineNumber(script, code_pos);
  } else {
    return -1;
  }
}


Handle<Value> Script::GetScriptName() {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Script::GetName()", return Handle<String>());
  LOG_API(isolate, "Script::GetName");
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsScript()) {
    i::Object* name = i::Script::cast(*obj)->name();
    return Utils::ToLocal(i::Handle<i::Object>(name, isolate));
  } else {
    return Handle<String>();
  }
}


void Script::SetData(v8::Handle<String> data) {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Script::SetData()", return);
  LOG_API(isolate, "Script::SetData");
  {
    i::HandleScope scope(isolate);
    i::Handle<i::SharedFunctionInfo> function_info = OpenScript(this);
    i::Handle<i::Object> raw_data = Utils::OpenHandle(*data);
    i::Handle<i::Script> script(i::Script::cast(function_info->script()));
    script->set_data(*raw_data);
  }
}


// --- E x c e p t i o n s ---


v8::TryCatch::TryCatch()
    : isolate_(i::Isolate::Current()),
      next_(isolate_->try_catch_handler_address()),
      exception_(isolate_->heap()->the_hole_value()),
      message_(i::Smi::FromInt(0)),
      is_verbose_(false),
      can_continue_(true),
      capture_message_(true),
      rethrow_(false),
      has_terminated_(false) {
  isolate_->RegisterTryCatchHandler(this);
}


v8::TryCatch::~TryCatch() {
  ASSERT(isolate_ == i::Isolate::Current());
  if (rethrow_) {
    v8::HandleScope scope(reinterpret_cast<Isolate*>(isolate_));
    v8::Local<v8::Value> exc = v8::Local<v8::Value>::New(Exception());
    isolate_->UnregisterTryCatchHandler(this);
    v8::ThrowException(exc);
  } else {
    isolate_->UnregisterTryCatchHandler(this);
  }
}


bool v8::TryCatch::HasCaught() const {
  return !reinterpret_cast<i::Object*>(exception_)->IsTheHole();
}


bool v8::TryCatch::CanContinue() const {
  return can_continue_;
}


bool v8::TryCatch::HasTerminated() const {
  return has_terminated_;
}


v8::Handle<v8::Value> v8::TryCatch::ReThrow() {
  if (!HasCaught()) return v8::Local<v8::Value>();
  rethrow_ = true;
  return v8::Undefined();
}


v8::Local<Value> v8::TryCatch::Exception() const {
  ASSERT(isolate_ == i::Isolate::Current());
  if (HasCaught()) {
    // Check for out of memory exception.
    i::Object* exception = reinterpret_cast<i::Object*>(exception_);
    return v8::Utils::ToLocal(i::Handle<i::Object>(exception, isolate_));
  } else {
    return v8::Local<Value>();
  }
}


v8::Local<Value> v8::TryCatch::StackTrace() const {
  ASSERT(isolate_ == i::Isolate::Current());
  if (HasCaught()) {
    i::Object* raw_obj = reinterpret_cast<i::Object*>(exception_);
    if (!raw_obj->IsJSObject()) return v8::Local<Value>();
    i::HandleScope scope(isolate_);
    i::Handle<i::JSObject> obj(i::JSObject::cast(raw_obj), isolate_);
    i::Handle<i::String> name = isolate_->factory()->stack_string();
    if (!obj->HasProperty(*name)) return v8::Local<Value>();
    i::Handle<i::Object> value = i::GetProperty(isolate_, obj, name);
    if (value.is_null()) return v8::Local<Value>();
    return v8::Utils::ToLocal(scope.CloseAndEscape(value));
  } else {
    return v8::Local<Value>();
  }
}


v8::Local<v8::Message> v8::TryCatch::Message() const {
  ASSERT(isolate_ == i::Isolate::Current());
  if (HasCaught() && message_ != i::Smi::FromInt(0)) {
    i::Object* message = reinterpret_cast<i::Object*>(message_);
    return v8::Utils::MessageToLocal(i::Handle<i::Object>(message, isolate_));
  } else {
    return v8::Local<v8::Message>();
  }
}


void v8::TryCatch::Reset() {
  ASSERT(isolate_ == i::Isolate::Current());
  exception_ = isolate_->heap()->the_hole_value();
  message_ = i::Smi::FromInt(0);
}


void v8::TryCatch::SetVerbose(bool value) {
  is_verbose_ = value;
}


void v8::TryCatch::SetCaptureMessage(bool value) {
  capture_message_ = value;
}


// --- M e s s a g e ---


Local<String> Message::Get() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Message::Get()", return Local<String>());
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::String> raw_result = i::MessageHandler::GetMessage(isolate, obj);
  Local<String> result = Utils::ToLocal(raw_result);
  return scope.Close(result);
}


v8::Handle<Value> Message::GetScriptResourceName() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetScriptResourceName()")) {
    return Local<String>();
  }
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  // Return this.script.name.
  i::Handle<i::JSValue> script =
      i::Handle<i::JSValue>::cast(i::Handle<i::Object>(message->script(),
                                                       isolate));
  i::Handle<i::Object> resource_name(i::Script::cast(script->value())->name(),
                                     isolate);
  return scope.Close(Utils::ToLocal(resource_name));
}


v8::Handle<Value> Message::GetScriptData() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetScriptResourceData()")) {
    return Local<Value>();
  }
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  // Return this.script.data.
  i::Handle<i::JSValue> script =
      i::Handle<i::JSValue>::cast(i::Handle<i::Object>(message->script(),
                                                       isolate));
  i::Handle<i::Object> data(i::Script::cast(script->value())->data(), isolate);
  return scope.Close(Utils::ToLocal(data));
}


v8::Handle<v8::StackTrace> Message::GetStackTrace() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetStackTrace()")) {
    return Local<v8::StackTrace>();
  }
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  i::Handle<i::Object> stackFramesObj(message->stack_frames(), isolate);
  if (!stackFramesObj->IsJSArray()) return v8::Handle<v8::StackTrace>();
  i::Handle<i::JSArray> stackTrace =
      i::Handle<i::JSArray>::cast(stackFramesObj);
  return scope.Close(Utils::StackTraceToLocal(stackTrace));
}


static i::Handle<i::Object> CallV8HeapFunction(const char* name,
                                               i::Handle<i::Object> recv,
                                               int argc,
                                               i::Handle<i::Object> argv[],
                                               bool* has_pending_exception) {
  i::Isolate* isolate = i::Isolate::Current();
  i::Handle<i::String> fmt_str =
      isolate->factory()->InternalizeUtf8String(name);
  i::Object* object_fun =
      isolate->js_builtins_object()->GetPropertyNoExceptionThrown(*fmt_str);
  i::Handle<i::JSFunction> fun =
      i::Handle<i::JSFunction>(i::JSFunction::cast(object_fun));
  i::Handle<i::Object> value =
      i::Execution::Call(fun, recv, argc, argv, has_pending_exception);
  return value;
}


static i::Handle<i::Object> CallV8HeapFunction(const char* name,
                                               i::Handle<i::Object> data,
                                               bool* has_pending_exception) {
  i::Handle<i::Object> argv[] = { data };
  return CallV8HeapFunction(name,
                            i::Isolate::Current()->js_builtins_object(),
                            ARRAY_SIZE(argv),
                            argv,
                            has_pending_exception);
}


int Message::GetLineNumber() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Message::GetLineNumber()", return kNoLineNumberInfo);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);

  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> result = CallV8HeapFunction("GetLineNumber",
                                                   Utils::OpenHandle(this),
                                                   &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, 0);
  return static_cast<int>(result->Number());
}


int Message::GetStartPosition() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetStartPosition()")) return 0;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  return message->start_position();
}


int Message::GetEndPosition() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetEndPosition()")) return 0;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  return message->end_position();
}


int Message::GetStartColumn() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetStartColumn()")) {
    return kNoColumnInfo;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> data_obj = Utils::OpenHandle(this);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> start_col_obj = CallV8HeapFunction(
      "GetPositionInLine",
      data_obj,
      &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, 0);
  return static_cast<int>(start_col_obj->Number());
}


int Message::GetEndColumn() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetEndColumn()")) return kNoColumnInfo;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> data_obj = Utils::OpenHandle(this);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> start_col_obj = CallV8HeapFunction(
      "GetPositionInLine",
      data_obj,
      &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, 0);
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(data_obj);
  int start = message->start_position();
  int end = message->end_position();
  return static_cast<int>(start_col_obj->Number()) + (end - start);
}


Local<String> Message::GetSourceLine() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Message::GetSourceLine()", return Local<String>());
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> result = CallV8HeapFunction("GetSourceLine",
                                                   Utils::OpenHandle(this),
                                                   &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, Local<v8::String>());
  if (result->IsString()) {
    return scope.Close(Utils::ToLocal(i::Handle<i::String>::cast(result)));
  } else {
    return Local<String>();
  }
}


void Message::PrintCurrentStackTrace(FILE* out) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Message::PrintCurrentStackTrace()")) return;
  ENTER_V8(isolate);
  isolate->PrintCurrentStackTrace(out);
}


// --- S t a c k T r a c e ---

Local<StackFrame> StackTrace::GetFrame(uint32_t index) const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackTrace::GetFrame()")) {
    return Local<StackFrame>();
  }
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSArray> self = Utils::OpenHandle(this);
  i::Object* raw_object = self->GetElementNoExceptionThrown(index);
  i::Handle<i::JSObject> obj(i::JSObject::cast(raw_object));
  return scope.Close(Utils::StackFrameToLocal(obj));
}


int StackTrace::GetFrameCount() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackTrace::GetFrameCount()")) return -1;
  ENTER_V8(isolate);
  return i::Smi::cast(Utils::OpenHandle(this)->length())->value();
}


Local<Array> StackTrace::AsArray() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackTrace::AsArray()")) Local<Array>();
  ENTER_V8(isolate);
  return Utils::ToLocal(Utils::OpenHandle(this));
}


Local<StackTrace> StackTrace::CurrentStackTrace(int frame_limit,
    StackTraceOptions options) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::StackTrace::CurrentStackTrace()")) {
    Local<StackTrace>();
  }
  ENTER_V8(isolate);
  i::Handle<i::JSArray> stackTrace =
      isolate->CaptureCurrentStackTrace(frame_limit, options);
  return Utils::StackTraceToLocal(stackTrace);
}


// --- S t a c k F r a m e ---

int StackFrame::GetLineNumber() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackFrame::GetLineNumber()")) {
    return Message::kNoLineNumberInfo;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> line = GetProperty(self, "lineNumber");
  if (!line->IsSmi()) {
    return Message::kNoLineNumberInfo;
  }
  return i::Smi::cast(*line)->value();
}


int StackFrame::GetColumn() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackFrame::GetColumn()")) {
    return Message::kNoColumnInfo;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> column = GetProperty(self, "column");
  if (!column->IsSmi()) {
    return Message::kNoColumnInfo;
  }
  return i::Smi::cast(*column)->value();
}


Local<String> StackFrame::GetScriptName() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackFrame::GetScriptName()")) {
    return Local<String>();
  }
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> name = GetProperty(self, "scriptName");
  if (!name->IsString()) {
    return Local<String>();
  }
  return scope.Close(Local<String>::Cast(Utils::ToLocal(name)));
}


Local<String> StackFrame::GetScriptNameOrSourceURL() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackFrame::GetScriptNameOrSourceURL()")) {
    return Local<String>();
  }
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> name = GetProperty(self, "scriptNameOrSourceURL");
  if (!name->IsString()) {
    return Local<String>();
  }
  return scope.Close(Local<String>::Cast(Utils::ToLocal(name)));
}


Local<String> StackFrame::GetFunctionName() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackFrame::GetFunctionName()")) {
    return Local<String>();
  }
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> name = GetProperty(self, "functionName");
  if (!name->IsString()) {
    return Local<String>();
  }
  return scope.Close(Local<String>::Cast(Utils::ToLocal(name)));
}


bool StackFrame::IsEval() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackFrame::IsEval()")) return false;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> is_eval = GetProperty(self, "isEval");
  return is_eval->IsTrue();
}


bool StackFrame::IsConstructor() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::StackFrame::IsConstructor()")) return false;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> is_constructor = GetProperty(self, "isConstructor");
  return is_constructor->IsTrue();
}


// --- D a t a ---

bool Value::FullIsUndefined() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsUndefined()")) {
    return false;
  }
  bool result = Utils::OpenHandle(this)->IsUndefined();
  ASSERT_EQ(result, QuickIsUndefined());
  return result;
}


bool Value::FullIsNull() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsNull()")) return false;
  bool result = Utils::OpenHandle(this)->IsNull();
  ASSERT_EQ(result, QuickIsNull());
  return result;
}


bool Value::IsTrue() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsTrue()")) return false;
  return Utils::OpenHandle(this)->IsTrue();
}


bool Value::IsFalse() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsFalse()")) return false;
  return Utils::OpenHandle(this)->IsFalse();
}


bool Value::IsFunction() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsFunction()")) {
    return false;
  }
  return Utils::OpenHandle(this)->IsJSFunction();
}


bool Value::FullIsString() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsString()")) return false;
  bool result = Utils::OpenHandle(this)->IsString();
  ASSERT_EQ(result, QuickIsString());
  return result;
}


bool Value::IsSymbol() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsSymbol()")) return false;
  return Utils::OpenHandle(this)->IsSymbol();
}


bool Value::IsArray() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsArray()")) return false;
  return Utils::OpenHandle(this)->IsJSArray();
}


bool Value::IsArrayBuffer() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsArrayBuffer()"))
    return false;
  return Utils::OpenHandle(this)->IsJSArrayBuffer();
}


bool Value::IsTypedArray() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsArrayBuffer()"))
    return false;
  return Utils::OpenHandle(this)->IsJSTypedArray();
}


#define TYPED_ARRAY_LIST(F) \
F(Uint8Array, kExternalUnsignedByteArray) \
F(Int8Array, kExternalByteArray) \
F(Uint16Array, kExternalUnsignedShortArray) \
F(Int16Array, kExternalShortArray) \
F(Uint32Array, kExternalUnsignedIntArray) \
F(Int32Array, kExternalIntArray) \
F(Float32Array, kExternalFloatArray) \
F(Float64Array, kExternalDoubleArray) \
F(Uint8ClampedArray, kExternalPixelArray)


#define VALUE_IS_TYPED_ARRAY(TypedArray, type_const)                          \
  bool Value::Is##TypedArray() const {                                        \
    if (IsDeadCheck(i::Isolate::Current(), "v8::Value::Is" #TypedArray "()")) \
      return false;                                                           \
    i::Handle<i::Object> obj = Utils::OpenHandle(this);                       \
    if (!obj->IsJSTypedArray()) return false;                                 \
    return i::JSTypedArray::cast(*obj)->type() == type_const;                 \
  }

TYPED_ARRAY_LIST(VALUE_IS_TYPED_ARRAY)

#undef VALUE_IS_TYPED_ARRAY


bool Value::IsObject() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsObject()")) return false;
  return Utils::OpenHandle(this)->IsJSObject();
}


bool Value::IsNumber() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsNumber()")) return false;
  return Utils::OpenHandle(this)->IsNumber();
}


bool Value::IsBoolean() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsBoolean()")) {
    return false;
  }
  return Utils::OpenHandle(this)->IsBoolean();
}


bool Value::IsExternal() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsExternal()")) {
    return false;
  }
  return Utils::OpenHandle(this)->IsExternal();
}


bool Value::IsInt32() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsInt32()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return true;
  if (obj->IsNumber()) {
    double value = obj->Number();
    static const i::DoubleRepresentation minus_zero(-0.0);
    i::DoubleRepresentation rep(value);
    if (rep.bits == minus_zero.bits) {
      return false;
    }
    return i::FastI2D(i::FastD2I(value)) == value;
  }
  return false;
}


bool Value::IsUint32() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsUint32()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return i::Smi::cast(*obj)->value() >= 0;
  if (obj->IsNumber()) {
    double value = obj->Number();
    static const i::DoubleRepresentation minus_zero(-0.0);
    i::DoubleRepresentation rep(value);
    if (rep.bits == minus_zero.bits) {
      return false;
    }
    return i::FastUI2D(i::FastD2UI(value)) == value;
  }
  return false;
}


bool Value::IsDate() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::IsDate()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->HasSpecificClassOf(isolate->heap()->Date_string());
}


bool Value::IsStringObject() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::IsStringObject()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->HasSpecificClassOf(isolate->heap()->String_string());
}


bool Value::IsSymbolObject() const {
  // TODO(svenpanne): these and other test functions should be written such
  // that they do not use Isolate::Current().
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::IsSymbolObject()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->HasSpecificClassOf(isolate->heap()->Symbol_string());
}


bool Value::IsNumberObject() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::IsNumberObject()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->HasSpecificClassOf(isolate->heap()->Number_string());
}


static i::Object* LookupBuiltin(i::Isolate* isolate,
                                const char* builtin_name) {
  i::Handle<i::String> string =
      isolate->factory()->InternalizeUtf8String(builtin_name);
  i::Handle<i::JSBuiltinsObject> builtins = isolate->js_builtins_object();
  return builtins->GetPropertyNoExceptionThrown(*string);
}


static bool CheckConstructor(i::Isolate* isolate,
                             i::Handle<i::JSObject> obj,
                             const char* class_name) {
  i::Object* constr = obj->map()->constructor();
  if (!constr->IsJSFunction()) return false;
  i::JSFunction* func = i::JSFunction::cast(constr);
  return func->shared()->native() &&
         constr == LookupBuiltin(isolate, class_name);
}


bool Value::IsNativeError() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::IsNativeError()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsJSObject()) {
    i::Handle<i::JSObject> js_obj(i::JSObject::cast(*obj));
    return CheckConstructor(isolate, js_obj, "$Error") ||
        CheckConstructor(isolate, js_obj, "$EvalError") ||
        CheckConstructor(isolate, js_obj, "$RangeError") ||
        CheckConstructor(isolate, js_obj, "$ReferenceError") ||
        CheckConstructor(isolate, js_obj, "$SyntaxError") ||
        CheckConstructor(isolate, js_obj, "$TypeError") ||
        CheckConstructor(isolate, js_obj, "$URIError");
  } else {
    return false;
  }
}


bool Value::IsBooleanObject() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::IsBooleanObject()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->HasSpecificClassOf(isolate->heap()->Boolean_string());
}


bool Value::IsRegExp() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsRegExp()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsJSRegExp();
}


Local<String> Value::ToString() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> str;
  if (obj->IsString()) {
    str = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::ToString()")) {
      return Local<String>();
    }
    LOG_API(isolate, "ToString");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    str = i::Execution::ToString(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, Local<String>());
  }
  return Local<String>(ToApi<String>(str));
}


Local<String> Value::ToDetailString() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> str;
  if (obj->IsString()) {
    str = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::ToDetailString()")) {
      return Local<String>();
    }
    LOG_API(isolate, "ToDetailString");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    str = i::Execution::ToDetailString(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, Local<String>());
  }
  return Local<String>(ToApi<String>(str));
}


Local<v8::Object> Value::ToObject() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> val;
  if (obj->IsJSObject()) {
    val = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::ToObject()")) {
      return Local<v8::Object>();
    }
    LOG_API(isolate, "ToObject");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    val = i::Execution::ToObject(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, Local<v8::Object>());
  }
  return Local<v8::Object>(ToApi<Object>(val));
}


Local<Boolean> Value::ToBoolean() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsBoolean()) {
    return Local<Boolean>(ToApi<Boolean>(obj));
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::ToBoolean()")) {
      return Local<Boolean>();
    }
    LOG_API(isolate, "ToBoolean");
    ENTER_V8(isolate);
    i::Handle<i::Object> val =
        isolate->factory()->ToBoolean(obj->BooleanValue());
    return Local<Boolean>(ToApi<Boolean>(val));
  }
}


Local<Number> Value::ToNumber() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> num;
  if (obj->IsNumber()) {
    num = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::ToNumber()")) {
      return Local<Number>();
    }
    LOG_API(isolate, "ToNumber");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    num = i::Execution::ToNumber(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, Local<Number>());
  }
  return Local<Number>(ToApi<Number>(num));
}


Local<Integer> Value::ToInteger() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> num;
  if (obj->IsSmi()) {
    num = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::ToInteger()")) return Local<Integer>();
    LOG_API(isolate, "ToInteger");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    num = i::Execution::ToInteger(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, Local<Integer>());
  }
  return Local<Integer>(ToApi<Integer>(num));
}


void External::CheckCast(v8::Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::External::Cast()")) return;
  ApiCheck(Utils::OpenHandle(that)->IsExternal(),
           "v8::External::Cast()",
           "Could not convert to external");
}


void v8::Object::CheckCast(Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Object::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsJSObject(),
           "v8::Object::Cast()",
           "Could not convert to object");
}


void v8::Function::CheckCast(Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Function::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsJSFunction(),
           "v8::Function::Cast()",
           "Could not convert to function");
}


void v8::String::CheckCast(v8::Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::String::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsString(),
           "v8::String::Cast()",
           "Could not convert to string");
}


void v8::Symbol::CheckCast(v8::Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Symbol::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsSymbol(),
           "v8::Symbol::Cast()",
           "Could not convert to symbol");
}


void v8::Number::CheckCast(v8::Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Number::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsNumber(),
           "v8::Number::Cast()",
           "Could not convert to number");
}


void v8::Integer::CheckCast(v8::Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Integer::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsNumber(),
           "v8::Integer::Cast()",
           "Could not convert to number");
}


void v8::Array::CheckCast(Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Array::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsJSArray(),
           "v8::Array::Cast()",
           "Could not convert to array");
}


void v8::ArrayBuffer::CheckCast(Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::ArrayBuffer::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsJSArrayBuffer(),
           "v8::ArrayBuffer::Cast()",
           "Could not convert to ArrayBuffer");
}


void v8::TypedArray::CheckCast(Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::TypedArray::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsJSTypedArray(),
           "v8::TypedArray::Cast()",
           "Could not convert to TypedArray");
}


#define CHECK_TYPED_ARRAY_CAST(ApiClass, typeConst)                         \
  void v8::ApiClass::CheckCast(Value* that) {                               \
    if (IsDeadCheck(i::Isolate::Current(), "v8::" #ApiClass "::Cast()"))    \
      return;                                                               \
    i::Handle<i::Object> obj = Utils::OpenHandle(that);                     \
    ApiCheck(obj->IsJSTypedArray() &&                                       \
             i::JSTypedArray::cast(*obj)->type() == typeConst,              \
             "v8::" #ApiClass "::Cast()",                                   \
             "Could not convert to " #ApiClass);                            \
  }


TYPED_ARRAY_LIST(CHECK_TYPED_ARRAY_CAST)

#undef CHECK_TYPED_ARRAY_CAST


void v8::Date::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Date::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->Date_string()),
           "v8::Date::Cast()",
           "Could not convert to date");
}


void v8::StringObject::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::StringObject::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->String_string()),
           "v8::StringObject::Cast()",
           "Could not convert to StringObject");
}


void v8::SymbolObject::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::SymbolObject::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->Symbol_string()),
           "v8::SymbolObject::Cast()",
           "Could not convert to SymbolObject");
}


void v8::NumberObject::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::NumberObject::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->Number_string()),
           "v8::NumberObject::Cast()",
           "Could not convert to NumberObject");
}


void v8::BooleanObject::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::BooleanObject::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->Boolean_string()),
           "v8::BooleanObject::Cast()",
           "Could not convert to BooleanObject");
}


void v8::RegExp::CheckCast(v8::Value* that) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::RegExp::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsJSRegExp(),
           "v8::RegExp::Cast()",
           "Could not convert to regular expression");
}


bool Value::BooleanValue() const {
  return Utils::OpenHandle(this)->BooleanValue();
}


double Value::NumberValue() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> num;
  if (obj->IsNumber()) {
    num = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::NumberValue()")) {
      return i::OS::nan_value();
    }
    LOG_API(isolate, "NumberValue");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    num = i::Execution::ToNumber(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, i::OS::nan_value());
  }
  return num->Number();
}


int64_t Value::IntegerValue() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> num;
  if (obj->IsNumber()) {
    num = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::IntegerValue()")) return 0;
    LOG_API(isolate, "IntegerValue");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    num = i::Execution::ToInteger(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, 0);
  }
  if (num->IsSmi()) {
    return i::Smi::cast(*num)->value();
  } else {
    return static_cast<int64_t>(num->Number());
  }
}


Local<Int32> Value::ToInt32() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> num;
  if (obj->IsSmi()) {
    num = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::ToInt32()")) return Local<Int32>();
    LOG_API(isolate, "ToInt32");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    num = i::Execution::ToInt32(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, Local<Int32>());
  }
  return Local<Int32>(ToApi<Int32>(num));
}


Local<Uint32> Value::ToUint32() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> num;
  if (obj->IsSmi()) {
    num = obj;
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::ToUint32()")) return Local<Uint32>();
    LOG_API(isolate, "ToUInt32");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    num = i::Execution::ToUint32(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, Local<Uint32>());
  }
  return Local<Uint32>(ToApi<Uint32>(num));
}


Local<Uint32> Value::ToArrayIndex() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    if (i::Smi::cast(*obj)->value() >= 0) return Utils::Uint32ToLocal(obj);
    return Local<Uint32>();
  }
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::ToArrayIndex()")) return Local<Uint32>();
  LOG_API(isolate, "ToArrayIndex");
  ENTER_V8(isolate);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> string_obj =
      i::Execution::ToString(obj, &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, Local<Uint32>());
  i::Handle<i::String> str = i::Handle<i::String>::cast(string_obj);
  uint32_t index;
  if (str->AsArrayIndex(&index)) {
    i::Handle<i::Object> value;
    if (index <= static_cast<uint32_t>(i::Smi::kMaxValue)) {
      value = i::Handle<i::Object>(i::Smi::FromInt(index), isolate);
    } else {
      value = isolate->factory()->NewNumber(index);
    }
    return Utils::Uint32ToLocal(value);
  }
  return Local<Uint32>();
}


int32_t Value::Int32Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    return i::Smi::cast(*obj)->value();
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::Int32Value()")) return 0;
    LOG_API(isolate, "Int32Value (slow)");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    i::Handle<i::Object> num =
        i::Execution::ToInt32(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, 0);
    if (num->IsSmi()) {
      return i::Smi::cast(*num)->value();
    } else {
      return static_cast<int32_t>(num->Number());
    }
  }
}


bool Value::Equals(Handle<Value> that) const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::Equals()")
      || EmptyCheck("v8::Value::Equals()", this)
      || EmptyCheck("v8::Value::Equals()", that)) {
    return false;
  }
  LOG_API(isolate, "Equals");
  ENTER_V8(isolate);
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> other = Utils::OpenHandle(*that);
  // If both obj and other are JSObjects, we'd better compare by identity
  // immediately when going into JS builtin.  The reason is Invoke
  // would overwrite global object receiver with global proxy.
  if (obj->IsJSObject() && other->IsJSObject()) {
    return *obj == *other;
  }
  i::Handle<i::Object> args[] = { other };
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> result =
      CallV8HeapFunction("EQUALS", obj, ARRAY_SIZE(args), args,
                         &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, false);
  return *result == i::Smi::FromInt(i::EQUAL);
}


bool Value::StrictEquals(Handle<Value> that) const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::StrictEquals()")
      || EmptyCheck("v8::Value::StrictEquals()", this)
      || EmptyCheck("v8::Value::StrictEquals()", that)) {
    return false;
  }
  LOG_API(isolate, "StrictEquals");
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> other = Utils::OpenHandle(*that);
  // Must check HeapNumber first, since NaN !== NaN.
  if (obj->IsHeapNumber()) {
    if (!other->IsNumber()) return false;
    double x = obj->Number();
    double y = other->Number();
    // Must check explicitly for NaN:s on Windows, but -0 works fine.
    return x == y && !std::isnan(x) && !std::isnan(y);
  } else if (*obj == *other) {  // Also covers Booleans.
    return true;
  } else if (obj->IsSmi()) {
    return other->IsNumber() && obj->Number() == other->Number();
  } else if (obj->IsString()) {
    return other->IsString() &&
      i::String::cast(*obj)->Equals(i::String::cast(*other));
  } else if (obj->IsUndefined() || obj->IsUndetectableObject()) {
    return other->IsUndefined() || other->IsUndetectableObject();
  } else {
    return false;
  }
}


uint32_t Value::Uint32Value() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    return i::Smi::cast(*obj)->value();
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::Uint32Value()")) return 0;
    LOG_API(isolate, "Uint32Value");
    ENTER_V8(isolate);
    EXCEPTION_PREAMBLE(isolate);
    i::Handle<i::Object> num =
        i::Execution::ToUint32(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, 0);
    if (num->IsSmi()) {
      return i::Smi::cast(*num)->value();
    } else {
      return static_cast<uint32_t>(num->Number());
    }
  }
}


bool v8::Object::Set(v8::Handle<Value> key, v8::Handle<Value> value,
                     v8::PropertyAttribute attribs) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Set()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::Object> self = Utils::OpenHandle(this);
  i::Handle<i::Object> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj = i::SetProperty(
      isolate,
      self,
      key_obj,
      value_obj,
      static_cast<PropertyAttributes>(attribs),
      i::kNonStrictMode);
  has_pending_exception = obj.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, false);
  return true;
}


bool v8::Object::Set(uint32_t index, v8::Handle<Value> value) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Set()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj = i::JSObject::SetElement(
      self,
      index,
      value_obj,
      NONE,
      i::kNonStrictMode);
  has_pending_exception = obj.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, false);
  return true;
}


bool v8::Object::ForceSet(v8::Handle<Value> key,
                          v8::Handle<Value> value,
                          v8::PropertyAttribute attribs) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::ForceSet()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj = i::ForceSetProperty(
      self,
      key_obj,
      value_obj,
      static_cast<PropertyAttributes>(attribs));
  has_pending_exception = obj.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, false);
  return true;
}


bool v8::Object::ForceDelete(v8::Handle<Value> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::ForceDelete()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> key_obj = Utils::OpenHandle(*key);

  // When deleting a property on the global object using ForceDelete
  // deoptimize all functions as optimized code does not check for the hole
  // value with DontDelete properties.  We have to deoptimize all contexts
  // because of possible cross-context inlined functions.
  if (self->IsJSGlobalProxy() || self->IsGlobalObject()) {
    i::Deoptimizer::DeoptimizeAll(isolate);
  }

  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj = i::ForceDeleteProperty(self, key_obj);
  has_pending_exception = obj.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, false);
  return obj->IsTrue();
}


Local<Value> v8::Object::Get(v8::Handle<Value> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Get()", return Local<v8::Value>());
  ENTER_V8(isolate);
  i::Handle<i::Object> self = Utils::OpenHandle(this);
  i::Handle<i::Object> key_obj = Utils::OpenHandle(*key);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> result = i::GetProperty(isolate, self, key_obj);
  has_pending_exception = result.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, Local<Value>());
  return Utils::ToLocal(result);
}


Local<Value> v8::Object::Get(uint32_t index) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Get()", return Local<v8::Value>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> result = i::Object::GetElement(self, index);
  has_pending_exception = result.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, Local<Value>());
  return Utils::ToLocal(result);
}


PropertyAttribute v8::Object::GetPropertyAttributes(v8::Handle<Value> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetPropertyAttribute()",
             return static_cast<PropertyAttribute>(NONE));
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> key_obj = Utils::OpenHandle(*key);
  if (!key_obj->IsName()) {
    EXCEPTION_PREAMBLE(isolate);
    key_obj = i::Execution::ToString(key_obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, static_cast<PropertyAttribute>(NONE));
  }
  i::Handle<i::Name> key_name = i::Handle<i::Name>::cast(key_obj);
  PropertyAttributes result = self->GetPropertyAttribute(*key_name);
  if (result == ABSENT) return static_cast<PropertyAttribute>(NONE);
  return static_cast<PropertyAttribute>(result);
}


Local<Value> v8::Object::GetPrototype() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetPrototype()",
             return Local<v8::Value>());
  ENTER_V8(isolate);
  i::Handle<i::Object> self = Utils::OpenHandle(this);
  i::Handle<i::Object> result(self->GetPrototype(isolate), isolate);
  return Utils::ToLocal(result);
}


bool v8::Object::SetPrototype(Handle<Value> value) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::SetPrototype()", return false);
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);
  // We do not allow exceptions thrown while setting the prototype
  // to propagate outside.
  TryCatch try_catch;
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> result = i::SetPrototype(self, value_obj);
  has_pending_exception = result.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, false);
  return true;
}


Local<Object> v8::Object::FindInstanceInPrototypeChain(
    v8::Handle<FunctionTemplate> tmpl) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate,
             "v8::Object::FindInstanceInPrototypeChain()",
             return Local<v8::Object>());
  ENTER_V8(isolate);
  i::JSObject* object = *Utils::OpenHandle(this);
  i::FunctionTemplateInfo* tmpl_info = *Utils::OpenHandle(*tmpl);
  while (!object->IsInstanceOf(tmpl_info)) {
    i::Object* prototype = object->GetPrototype();
    if (!prototype->IsJSObject()) return Local<Object>();
    object = i::JSObject::cast(prototype);
  }
  return Utils::ToLocal(i::Handle<i::JSObject>(object));
}


Local<Array> v8::Object::GetPropertyNames() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetPropertyNames()",
             return Local<v8::Array>());
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  bool threw = false;
  i::Handle<i::FixedArray> value =
      i::GetKeysInFixedArrayFor(self, i::INCLUDE_PROTOS, &threw);
  if (threw) return Local<v8::Array>();
  // Because we use caching to speed up enumeration it is important
  // to never change the result of the basic enumeration function so
  // we clone the result.
  i::Handle<i::FixedArray> elms = isolate->factory()->CopyFixedArray(value);
  i::Handle<i::JSArray> result =
      isolate->factory()->NewJSArrayWithElements(elms);
  return Utils::ToLocal(scope.CloseAndEscape(result));
}


Local<Array> v8::Object::GetOwnPropertyNames() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetOwnPropertyNames()",
             return Local<v8::Array>());
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  bool threw = false;
  i::Handle<i::FixedArray> value =
      i::GetKeysInFixedArrayFor(self, i::LOCAL_ONLY, &threw);
  if (threw) return Local<v8::Array>();
  // Because we use caching to speed up enumeration it is important
  // to never change the result of the basic enumeration function so
  // we clone the result.
  i::Handle<i::FixedArray> elms = isolate->factory()->CopyFixedArray(value);
  i::Handle<i::JSArray> result =
      isolate->factory()->NewJSArrayWithElements(elms);
  return Utils::ToLocal(scope.CloseAndEscape(result));
}


Local<String> v8::Object::ObjectProtoToString() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::ObjectProtoToString()",
             return Local<v8::String>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);

  i::Handle<i::Object> name(self->class_name(), isolate);

  // Native implementation of Object.prototype.toString (v8natives.js):
  //   var c = %_ClassOf(this);
  //   if (c === 'Arguments') c  = 'Object';
  //   return "[object " + c + "]";

  if (!name->IsString()) {
    return v8::String::New("[object ]");

  } else {
    i::Handle<i::String> class_name = i::Handle<i::String>::cast(name);
    if (class_name->IsOneByteEqualTo(STATIC_ASCII_VECTOR("Arguments"))) {
      return v8::String::New("[object Object]");

    } else {
      const char* prefix = "[object ";
      Local<String> str = Utils::ToLocal(class_name);
      const char* postfix = "]";

      int prefix_len = i::StrLength(prefix);
      int str_len = str->Utf8Length();
      int postfix_len = i::StrLength(postfix);

      int buf_len = prefix_len + str_len + postfix_len;
      i::ScopedVector<char> buf(buf_len);

      // Write prefix.
      char* ptr = buf.start();
      i::OS::MemCopy(ptr, prefix, prefix_len * v8::internal::kCharSize);
      ptr += prefix_len;

      // Write real content.
      str->WriteUtf8(ptr, str_len);
      ptr += str_len;

      // Write postfix.
      i::OS::MemCopy(ptr, postfix, postfix_len * v8::internal::kCharSize);

      // Copy the buffer into a heap-allocated string and return it.
      Local<String> result = v8::String::New(buf.start(), buf_len);
      return result;
    }
  }
}


Local<Value> v8::Object::GetConstructor() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetConstructor()",
             return Local<v8::Function>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> constructor(self->GetConstructor(), isolate);
  return Utils::ToLocal(constructor);
}


Local<String> v8::Object::GetConstructorName() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetConstructorName()",
             return Local<v8::String>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::String> name(self->constructor_name());
  return Utils::ToLocal(name);
}


bool v8::Object::Delete(v8::Handle<Value> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Delete()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::Object> key_obj = Utils::OpenHandle(*key);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj = i::DeleteProperty(self, key_obj);
  has_pending_exception = obj.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, false);
  return obj->IsTrue();
}


bool v8::Object::Has(v8::Handle<Value> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Has()", return false);
  ENTER_V8(isolate);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Object> key_obj = Utils::OpenHandle(*key);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj = i::HasProperty(self, key_obj);
  has_pending_exception = obj.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, false);
  return obj->IsTrue();
}


bool v8::Object::Delete(uint32_t index) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::DeleteProperty()",
             return false);
  ENTER_V8(isolate);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  return i::JSObject::DeleteElement(self, index)->IsTrue();
}


bool v8::Object::Has(uint32_t index) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::HasProperty()", return false);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  return self->HasElement(index);
}


static inline bool SetAccessor(Object* obj, i::Handle<i::AccessorInfo> info) {
  if (info.is_null()) return false;
  bool fast = Utils::OpenHandle(obj)->HasFastProperties();
  i::Handle<i::Object> result = i::SetAccessor(Utils::OpenHandle(obj), info);
  if (result.is_null() || result->IsUndefined()) return false;
  if (fast) i::JSObject::TransformToFastProperties(Utils::OpenHandle(obj), 0);
  return true;
}


bool Object::SetAccessor(Handle<String> name,
                         AccessorGetter getter,
                         AccessorSetter setter,
                         v8::Handle<Value> data,
                         AccessControl settings,
                         PropertyAttribute attributes) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::SetAccessor()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  v8::Handle<AccessorSignature> signature;
  i::Handle<i::AccessorInfo> info = MakeAccessorInfo(name, getter, setter, data,
                                                     settings, attributes,
                                                     signature);
  return v8::SetAccessor(this, info);
}


bool Object::SetAccessor(Handle<String> name,
                         Handle<DeclaredAccessorDescriptor> descriptor,
                         AccessControl settings,
                         PropertyAttribute attributes) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::SetAccessor()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  v8::Handle<AccessorSignature> signature;
  i::Handle<i::AccessorInfo> info = MakeAccessorInfo(
      name, descriptor, settings, attributes, signature);
  return v8::SetAccessor(this, info);
}


bool v8::Object::HasOwnProperty(Handle<String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::HasOwnProperty()",
             return false);
  return Utils::OpenHandle(this)->HasLocalProperty(
      *Utils::OpenHandle(*key));
}


bool v8::Object::HasRealNamedProperty(Handle<String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::HasRealNamedProperty()",
             return false);
  return Utils::OpenHandle(this)->HasRealNamedProperty(
      isolate,
      *Utils::OpenHandle(*key));
}


bool v8::Object::HasRealIndexedProperty(uint32_t index) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::HasRealIndexedProperty()",
             return false);
  return Utils::OpenHandle(this)->HasRealElementProperty(isolate, index);
}


bool v8::Object::HasRealNamedCallbackProperty(Handle<String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate,
             "v8::Object::HasRealNamedCallbackProperty()",
             return false);
  ENTER_V8(isolate);
  return Utils::OpenHandle(this)->HasRealNamedCallbackProperty(
      isolate,
      *Utils::OpenHandle(*key));
}


bool v8::Object::HasNamedLookupInterceptor() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::HasNamedLookupInterceptor()",
             return false);
  return Utils::OpenHandle(this)->HasNamedInterceptor();
}


bool v8::Object::HasIndexedLookupInterceptor() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::HasIndexedLookupInterceptor()",
             return false);
  return Utils::OpenHandle(this)->HasIndexedInterceptor();
}


static Local<Value> GetPropertyByLookup(i::Isolate* isolate,
                                        i::Handle<i::JSObject> receiver,
                                        i::Handle<i::String> name,
                                        i::LookupResult* lookup) {
  if (!lookup->IsProperty()) {
    // No real property was found.
    return Local<Value>();
  }

  // If the property being looked up is a callback, it can throw
  // an exception.
  EXCEPTION_PREAMBLE(isolate);
  PropertyAttributes ignored;
  i::Handle<i::Object> result =
      i::Object::GetProperty(receiver, receiver, lookup, name,
                             &ignored);
  has_pending_exception = result.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, Local<Value>());

  return Utils::ToLocal(result);
}


Local<Value> v8::Object::GetRealNamedPropertyInPrototypeChain(
      Handle<String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate,
             "v8::Object::GetRealNamedPropertyInPrototypeChain()",
             return Local<Value>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self_obj = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  i::LookupResult lookup(isolate);
  self_obj->LookupRealNamedPropertyInPrototypes(*key_obj, &lookup);
  return GetPropertyByLookup(isolate, self_obj, key_obj, &lookup);
}


Local<Value> v8::Object::GetRealNamedProperty(Handle<String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetRealNamedProperty()",
             return Local<Value>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self_obj = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  i::LookupResult lookup(isolate);
  self_obj->LookupRealNamedProperty(*key_obj, &lookup);
  return GetPropertyByLookup(isolate, self_obj, key_obj, &lookup);
}


// Turns on access checks by copying the map and setting the check flag.
// Because the object gets a new map, existing inline cache caching
// the old map of this object will fail.
void v8::Object::TurnOnAccessCheck() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::TurnOnAccessCheck()", return);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);

  // When turning on access checks for a global object deoptimize all functions
  // as optimized code does not always handle access checks.
  i::Deoptimizer::DeoptimizeGlobalObject(*obj);

  i::Handle<i::Map> new_map =
      isolate->factory()->CopyMap(i::Handle<i::Map>(obj->map()));
  new_map->set_is_access_check_needed(true);
  obj->set_map(*new_map);
}


bool v8::Object::IsDirty() {
  return Utils::OpenHandle(this)->IsDirty();
}


Local<v8::Object> v8::Object::Clone() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Clone()", return Local<Object>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::JSObject> result = i::Copy(self);
  has_pending_exception = result.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, Local<Object>());
  return Utils::ToLocal(result);
}


static i::Context* GetCreationContext(i::JSObject* object) {
  i::Object* constructor = object->map()->constructor();
  i::JSFunction* function;
  if (!constructor->IsJSFunction()) {
    // Functions have null as a constructor,
    // but any JSFunction knows its context immediately.
    ASSERT(object->IsJSFunction());
    function = i::JSFunction::cast(object);
  } else {
    function = i::JSFunction::cast(constructor);
  }
  return function->context()->native_context();
}


Local<v8::Context> v8::Object::CreationContext() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate,
             "v8::Object::CreationContext()", return Local<v8::Context>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Context* context = GetCreationContext(*self);
  return Utils::ToLocal(i::Handle<i::Context>(context));
}


int v8::Object::GetIdentityHash() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetIdentityHash()", return 0);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  return i::JSObject::GetIdentityHash(self);
}


bool v8::Object::SetHiddenValue(v8::Handle<v8::String> key,
                                v8::Handle<v8::Value> value) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::SetHiddenValue()", return false);
  if (value.IsEmpty()) return DeleteHiddenValue(key);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::String> key_string =
      isolate->factory()->InternalizeString(key_obj);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);
  i::Handle<i::Object> result =
      i::JSObject::SetHiddenProperty(self, key_string, value_obj);
  return *result == *self;
}


v8::Local<v8::Value> v8::Object::GetHiddenValue(v8::Handle<v8::String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetHiddenValue()",
             return Local<v8::Value>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::String> key_string = FACTORY->InternalizeString(key_obj);
  i::Handle<i::Object> result(self->GetHiddenProperty(*key_string), isolate);
  if (result->IsUndefined()) return v8::Local<v8::Value>();
  return Utils::ToLocal(result);
}


bool v8::Object::DeleteHiddenValue(v8::Handle<v8::String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::DeleteHiddenValue()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::String> key_string = FACTORY->InternalizeString(key_obj);
  self->DeleteHiddenProperty(*key_string);
  return true;
}


namespace {

static i::ElementsKind GetElementsKindFromExternalArrayType(
    ExternalArrayType array_type) {
  switch (array_type) {
    case kExternalByteArray:
      return i::EXTERNAL_BYTE_ELEMENTS;
      break;
    case kExternalUnsignedByteArray:
      return i::EXTERNAL_UNSIGNED_BYTE_ELEMENTS;
      break;
    case kExternalShortArray:
      return i::EXTERNAL_SHORT_ELEMENTS;
      break;
    case kExternalUnsignedShortArray:
      return i::EXTERNAL_UNSIGNED_SHORT_ELEMENTS;
      break;
    case kExternalIntArray:
      return i::EXTERNAL_INT_ELEMENTS;
      break;
    case kExternalUnsignedIntArray:
      return i::EXTERNAL_UNSIGNED_INT_ELEMENTS;
      break;
    case kExternalFloatArray:
      return i::EXTERNAL_FLOAT_ELEMENTS;
      break;
    case kExternalDoubleArray:
      return i::EXTERNAL_DOUBLE_ELEMENTS;
      break;
    case kExternalPixelArray:
      return i::EXTERNAL_PIXEL_ELEMENTS;
      break;
  }
  UNREACHABLE();
  return i::DICTIONARY_ELEMENTS;
}


void PrepareExternalArrayElements(i::Handle<i::JSObject> object,
                                  void* data,
                                  ExternalArrayType array_type,
                                  int length) {
  i::Isolate* isolate = object->GetIsolate();
  i::Handle<i::ExternalArray> array =
      isolate->factory()->NewExternalArray(length, array_type, data);

  i::Handle<i::Map> external_array_map =
      isolate->factory()->GetElementsTransitionMap(
          object,
          GetElementsKindFromExternalArrayType(array_type));

  object->set_map(*external_array_map);
  object->set_elements(*array);
}

}  // namespace


void v8::Object::SetIndexedPropertiesToPixelData(uint8_t* data, int length) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::SetElementsToPixelData()", return);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  if (!ApiCheck(length >= 0 && length <= i::ExternalPixelArray::kMaxLength,
                "v8::Object::SetIndexedPropertiesToPixelData()",
                "length exceeds max acceptable value")) {
    return;
  }
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  if (!ApiCheck(!self->IsJSArray(),
                "v8::Object::SetIndexedPropertiesToPixelData()",
                "JSArray is not supported")) {
    return;
  }
  PrepareExternalArrayElements(self, data, kExternalPixelArray, length);
}


bool v8::Object::HasIndexedPropertiesInPixelData() {
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  ON_BAILOUT(self->GetIsolate(), "v8::HasIndexedPropertiesInPixelData()",
             return false);
  return self->HasExternalPixelElements();
}


uint8_t* v8::Object::GetIndexedPropertiesPixelData() {
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  ON_BAILOUT(self->GetIsolate(), "v8::GetIndexedPropertiesPixelData()",
             return NULL);
  if (self->HasExternalPixelElements()) {
    return i::ExternalPixelArray::cast(self->elements())->
        external_pixel_pointer();
  } else {
    return NULL;
  }
}


int v8::Object::GetIndexedPropertiesPixelDataLength() {
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  ON_BAILOUT(self->GetIsolate(), "v8::GetIndexedPropertiesPixelDataLength()",
             return -1);
  if (self->HasExternalPixelElements()) {
    return i::ExternalPixelArray::cast(self->elements())->length();
  } else {
    return -1;
  }
}


void v8::Object::SetIndexedPropertiesToExternalArrayData(
    void* data,
    ExternalArrayType array_type,
    int length) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::SetIndexedPropertiesToExternalArrayData()", return);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  if (!ApiCheck(length >= 0 && length <= i::ExternalArray::kMaxLength,
                "v8::Object::SetIndexedPropertiesToExternalArrayData()",
                "length exceeds max acceptable value")) {
    return;
  }
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  if (!ApiCheck(!self->IsJSArray(),
                "v8::Object::SetIndexedPropertiesToExternalArrayData()",
                "JSArray is not supported")) {
    return;
  }
  PrepareExternalArrayElements(self, data, array_type, length);
}


bool v8::Object::HasIndexedPropertiesInExternalArrayData() {
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  ON_BAILOUT(self->GetIsolate(),
             "v8::HasIndexedPropertiesInExternalArrayData()",
             return false);
  return self->HasExternalArrayElements();
}


void* v8::Object::GetIndexedPropertiesExternalArrayData() {
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  ON_BAILOUT(self->GetIsolate(),
             "v8::GetIndexedPropertiesExternalArrayData()",
             return NULL);
  if (self->HasExternalArrayElements()) {
    return i::ExternalArray::cast(self->elements())->external_pointer();
  } else {
    return NULL;
  }
}


ExternalArrayType v8::Object::GetIndexedPropertiesExternalArrayDataType() {
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  ON_BAILOUT(self->GetIsolate(),
             "v8::GetIndexedPropertiesExternalArrayDataType()",
             return static_cast<ExternalArrayType>(-1));
  switch (self->elements()->map()->instance_type()) {
    case i::EXTERNAL_BYTE_ARRAY_TYPE:
      return kExternalByteArray;
    case i::EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE:
      return kExternalUnsignedByteArray;
    case i::EXTERNAL_SHORT_ARRAY_TYPE:
      return kExternalShortArray;
    case i::EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE:
      return kExternalUnsignedShortArray;
    case i::EXTERNAL_INT_ARRAY_TYPE:
      return kExternalIntArray;
    case i::EXTERNAL_UNSIGNED_INT_ARRAY_TYPE:
      return kExternalUnsignedIntArray;
    case i::EXTERNAL_FLOAT_ARRAY_TYPE:
      return kExternalFloatArray;
    case i::EXTERNAL_DOUBLE_ARRAY_TYPE:
      return kExternalDoubleArray;
    case i::EXTERNAL_PIXEL_ARRAY_TYPE:
      return kExternalPixelArray;
    default:
      return static_cast<ExternalArrayType>(-1);
  }
}


int v8::Object::GetIndexedPropertiesExternalArrayDataLength() {
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  ON_BAILOUT(self->GetIsolate(),
             "v8::GetIndexedPropertiesExternalArrayDataLength()",
             return 0);
  if (self->HasExternalArrayElements()) {
    return i::ExternalArray::cast(self->elements())->length();
  } else {
    return -1;
  }
}


bool v8::Object::IsCallable() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::IsCallable()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  if (obj->IsJSFunction()) return true;
  return i::Execution::GetFunctionDelegate(obj)->IsJSFunction();
}


Local<v8::Value> Object::CallAsFunction(v8::Handle<v8::Object> recv,
                                        int argc,
                                        v8::Handle<v8::Value> argv[]) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::CallAsFunction()",
             return Local<v8::Value>());
  LOG_API(isolate, "Object::CallAsFunction");
  ENTER_V8(isolate);
  i::Logger::TimerEventScope timer_scope(
      isolate, i::Logger::TimerEventScope::v8_execute);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> recv_obj = Utils::OpenHandle(*recv);
  STATIC_ASSERT(sizeof(v8::Handle<v8::Value>) == sizeof(i::Object**));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  i::Handle<i::JSFunction> fun = i::Handle<i::JSFunction>();
  if (obj->IsJSFunction()) {
    fun = i::Handle<i::JSFunction>::cast(obj);
  } else {
    EXCEPTION_PREAMBLE(isolate);
    i::Handle<i::Object> delegate =
        i::Execution::TryGetFunctionDelegate(obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, Local<Value>());
    fun = i::Handle<i::JSFunction>::cast(delegate);
    recv_obj = obj;
  }
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> returned =
      i::Execution::Call(fun, recv_obj, argc, args, &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK_DO_CALLBACK(isolate, Local<Value>());
  return Utils::ToLocal(scope.CloseAndEscape(returned));
}


Local<v8::Value> Object::CallAsConstructor(int argc,
                                           v8::Handle<v8::Value> argv[]) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::CallAsConstructor()",
             return Local<v8::Object>());
  LOG_API(isolate, "Object::CallAsConstructor");
  ENTER_V8(isolate);
  i::Logger::TimerEventScope timer_scope(
      isolate, i::Logger::TimerEventScope::v8_execute);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  STATIC_ASSERT(sizeof(v8::Handle<v8::Value>) == sizeof(i::Object**));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  if (obj->IsJSFunction()) {
    i::Handle<i::JSFunction> fun = i::Handle<i::JSFunction>::cast(obj);
    EXCEPTION_PREAMBLE(isolate);
    i::Handle<i::Object> returned =
        i::Execution::New(fun, argc, args, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK_DO_CALLBACK(isolate, Local<v8::Object>());
    return Utils::ToLocal(scope.CloseAndEscape(
        i::Handle<i::JSObject>::cast(returned)));
  }
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> delegate =
      i::Execution::TryGetConstructorDelegate(obj, &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, Local<v8::Object>());
  if (!delegate->IsUndefined()) {
    i::Handle<i::JSFunction> fun = i::Handle<i::JSFunction>::cast(delegate);
    EXCEPTION_PREAMBLE(isolate);
    i::Handle<i::Object> returned =
        i::Execution::Call(fun, obj, argc, args, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK_DO_CALLBACK(isolate, Local<v8::Object>());
    ASSERT(!delegate->IsUndefined());
    return Utils::ToLocal(scope.CloseAndEscape(returned));
  }
  return Local<v8::Object>();
}


Local<v8::Object> Function::NewInstance() const {
  return NewInstance(0, NULL);
}


Local<v8::Object> Function::NewInstance(int argc,
                                        v8::Handle<v8::Value> argv[]) const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Function::NewInstance()",
             return Local<v8::Object>());
  LOG_API(isolate, "Function::NewInstance");
  ENTER_V8(isolate);
  i::Logger::TimerEventScope timer_scope(
      isolate, i::Logger::TimerEventScope::v8_execute);
  HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Handle<i::JSFunction> function = Utils::OpenHandle(this);
  STATIC_ASSERT(sizeof(v8::Handle<v8::Value>) == sizeof(i::Object**));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> returned =
      i::Execution::New(function, argc, args, &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK_DO_CALLBACK(isolate, Local<v8::Object>());
  return scope.Close(Utils::ToLocal(i::Handle<i::JSObject>::cast(returned)));
}


Local<v8::Value> Function::Call(v8::Handle<v8::Object> recv, int argc,
                                v8::Handle<v8::Value> argv[]) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Function::Call()", return Local<v8::Value>());
  LOG_API(isolate, "Function::Call");
  ENTER_V8(isolate);
  i::Logger::TimerEventScope timer_scope(
      isolate, i::Logger::TimerEventScope::v8_execute);
  i::Object* raw_result = NULL;
  {
    i::HandleScope scope(isolate);
    i::Handle<i::JSFunction> fun = Utils::OpenHandle(this);
    i::Handle<i::Object> recv_obj = Utils::OpenHandle(*recv);
    STATIC_ASSERT(sizeof(v8::Handle<v8::Value>) == sizeof(i::Object**));
    i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
    EXCEPTION_PREAMBLE(isolate);
    i::Handle<i::Object> returned =
        i::Execution::Call(fun, recv_obj, argc, args, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK_DO_CALLBACK(isolate, Local<Object>());
    raw_result = *returned;
  }
  i::Handle<i::Object> result(raw_result, isolate);
  return Utils::ToLocal(result);
}


void Function::SetName(v8::Handle<v8::String> name) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8(isolate);
  USE(isolate);
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  func->shared()->set_name(*Utils::OpenHandle(*name));
}


Handle<Value> Function::GetName() const {
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  return Utils::ToLocal(i::Handle<i::Object>(func->shared()->name(),
                                             func->GetIsolate()));
}


Handle<Value> Function::GetInferredName() const {
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  return Utils::ToLocal(i::Handle<i::Object>(func->shared()->inferred_name(),
                                             func->GetIsolate()));
}


ScriptOrigin Function::GetScriptOrigin() const {
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  if (func->shared()->script()->IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
    i::Handle<i::Object> scriptName = GetScriptNameOrSourceURL(script);
    v8::ScriptOrigin origin(
      Utils::ToLocal(scriptName),
      v8::Integer::New(script->line_offset()->value()),
      v8::Integer::New(script->column_offset()->value()));
    return origin;
  }
  return v8::ScriptOrigin(Handle<Value>());
}


const int Function::kLineOffsetNotFound = -1;


int Function::GetScriptLineNumber() const {
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  if (func->shared()->script()->IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
    return i::GetScriptLineNumber(script, func->shared()->start_position());
  }
  return kLineOffsetNotFound;
}


int Function::GetScriptColumnNumber() const {
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  if (func->shared()->script()->IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
    return i::GetScriptColumnNumber(script, func->shared()->start_position());
  }
  return kLineOffsetNotFound;
}

Handle<Value> Function::GetScriptId() const {
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  if (!func->shared()->script()->IsScript())
    return v8::Undefined();
  i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
  return Utils::ToLocal(i::Handle<i::Object>(script->id(), func->GetIsolate()));
}

int String::Length() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (IsDeadCheck(str->GetIsolate(), "v8::String::Length()")) return 0;
  return str->length();
}


bool String::IsOneByte() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (IsDeadCheck(str->GetIsolate(), "v8::String::IsOneByte()")) {
    return false;
  }
  return str->HasOnlyOneByteChars();
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
    inline explicit Visitor()
      : utf8_length_(0),
        state_(kInitialState) {}

    void VisitOneByteString(const uint8_t* chars, int length) {
      int utf8_length = 0;
      // Add in length 1 for each non-ASCII character.
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
      ASSERT(!(*state & kLeftmostEdgeIsSurrogate));
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
      ASSERT(!(*state & kRightmostEdgeIsSurrogate));
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
    ASSERT((state & kLeftmostEdgeIsCalculated) &&
           (state & kRightmostEdgeIsCalculated));
    if (EndsWithSurrogate(state) && StartsWithSurrogate(state)) {
      *length -= unibrow::Utf8::kBytesSavedByCombiningSurrogates;
    }
    *state_out = kInitialState |
        (state & kLeftmostEdgeIsSurrogate ? kStartsWithTrailingSurrogate : 0) |
        (state & kRightmostEdgeIsSurrogate ? kEndsWithLeadingSurrogate : 0);
  }

  static int Calculate(i::ConsString* current, uint8_t* state_out) {
    using namespace internal;
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
      if (left_as_cons == NULL) {
        total_length += leaf_length;
        MergeLeafLeft(&total_length, &state, left_leaf_state);
      }
      ConsString* right_as_cons =
          Visitor::VisitFlat(right, &leaf_length, &right_leaf_state);
      if (right_as_cons == NULL) {
        total_length += leaf_length;
        MergeLeafRight(&total_length, &state, right_leaf_state);
        if (left_as_cons != NULL) {
          // 1 Leaf node. Descend in place.
          current = left_as_cons;
          continue;
        } else {
          // Terminal node.
          MergeTerminal(&total_length, state, state_out);
          return total_length;
        }
      } else if (left_as_cons == NULL) {
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
    return 0;
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
  if (cons_string == NULL) return length;
  return Utf8LengthHelper::Calculate(cons_string);
}


int String::Utf8Length() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  i::Isolate* isolate = str->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::Utf8Length()")) return 0;
  return v8::Utf8Length(*str, isolate);
}


class Utf8WriterVisitor {
 public:
  Utf8WriterVisitor(
      char* buffer, int capacity, bool skip_capacity_check)
    : early_termination_(false),
      last_character_(unibrow::Utf16::kNoPreviousCharacter),
      buffer_(buffer),
      start_(buffer),
      capacity_(capacity),
      skip_capacity_check_(capacity == -1 || skip_capacity_check),
      utf16_chars_read_(0) {
  }

  static int WriteEndCharacter(uint16_t character,
                               int last_character,
                               int remaining,
                               char* const buffer) {
    using namespace unibrow;
    ASSERT(remaining > 0);
    // We can't use a local buffer here because Encode needs to modify
    // previous characters in the stream.  We know, however, that
    // exactly one character will be advanced.
    if (Utf16::IsTrailSurrogate(character) &&
        Utf16::IsLeadSurrogate(last_character)) {
      int written = Utf8::Encode(buffer, character, last_character);
      ASSERT(written == 1);
      return written;
    }
    // Use a scratch buffer to check the required characters.
    char temp_buffer[Utf8::kMaxEncodedSize];
    // Can't encode using last_character as gcc has array bounds issues.
    int written = Utf8::Encode(temp_buffer,
                               character,
                               Utf16::kNoPreviousCharacter);
    // Won't fit.
    if (written > remaining) return 0;
    // Copy over the character from temp_buffer.
    for (int j = 0; j < written; j++) {
      buffer[j] = temp_buffer[j];
    }
    return written;
  }

  template<typename Char>
  void Visit(const Char* chars, const int length) {
    using namespace unibrow;
    ASSERT(!early_termination_);
    if (length == 0) return;
    // Copy state to stack.
    char* buffer = buffer_;
    int last_character =
        sizeof(Char) == 1 ? Utf16::kNoPreviousCharacter : last_character_;
    int i = 0;
    // Do a fast loop where there is no exit capacity check.
    while (true) {
      int fast_length;
      if (skip_capacity_check_) {
        fast_length = length;
      } else {
        int remaining_capacity = capacity_ - static_cast<int>(buffer - start_);
        // Need enough space to write everything but one character.
        STATIC_ASSERT(Utf16::kMaxExtraUtf8BytesForOneUtf16CodeUnit == 3);
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
          buffer +=
              Utf8::EncodeOneByte(buffer, static_cast<uint8_t>(*chars++));
          ASSERT(capacity_ == -1 || (buffer - start_) <= capacity_);
        }
      } else {
        for (; i < fast_length; i++) {
          uint16_t character = *chars++;
          buffer += Utf8::Encode(buffer, character, last_character);
          last_character = character;
          ASSERT(capacity_ == -1 || (buffer - start_) <= capacity_);
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
    ASSERT(!skip_capacity_check_);
    // Slow loop. Must check capacity on each iteration.
    int remaining_capacity = capacity_ - static_cast<int>(buffer - start_);
    ASSERT(remaining_capacity >= 0);
    for (; i < length && remaining_capacity > 0; i++) {
      uint16_t character = *chars++;
      int written = WriteEndCharacter(character,
                                      last_character,
                                      remaining_capacity,
                                      buffer);
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
    if (utf16_chars_read_out != NULL) {
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
  int utf16_chars_read_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(Utf8WriterVisitor);
};


static bool RecursivelySerializeToUtf8(i::String* current,
                                      Utf8WriterVisitor* writer,
                                      int recursion_budget) {
  while (!writer->IsDone()) {
    i::ConsString* cons_string = i::String::VisitFlat(writer, current);
    if (cons_string == NULL) return true;  // Leaf node.
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
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::WriteUtf8()")) return 0;
  LOG_API(isolate, "String::WriteUtf8");
  ENTER_V8(isolate);
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (options & HINT_MANY_WRITES_EXPECTED) {
    FlattenString(str);  // Flatten the string for efficiency.
  }
  const int string_length = str->length();
  bool write_null = !(options & NO_NULL_TERMINATION);
  // First check if we can just write the string without checking capacity.
  if (capacity == -1 || capacity / 3 >= string_length) {
    Utf8WriterVisitor writer(buffer, capacity, true);
    const int kMaxRecursion = 100;
    bool success = RecursivelySerializeToUtf8(*str, &writer, kMaxRecursion);
    if (success) return writer.CompleteWrite(write_null, nchars_ref);
  } else if (capacity >= string_length) {
    // First check that the buffer is large enough.
    int utf8_bytes = v8::Utf8Length(*str, str->GetIsolate());
    if (utf8_bytes <= capacity) {
      // ASCII fast path.
      if (utf8_bytes == string_length) {
        WriteOneByte(reinterpret_cast<uint8_t*>(buffer), 0, capacity, options);
        if (nchars_ref != NULL) *nchars_ref = string_length;
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
  str = FlattenGetString(str);
  Utf8WriterVisitor writer(buffer, capacity, false);
  i::String::VisitFlat(&writer, *str);
  return writer.CompleteWrite(write_null, nchars_ref);
}


int String::WriteAscii(char* buffer,
                       int start,
                       int length,
                       int options) const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::WriteAscii()")) return 0;
  LOG_API(isolate, "String::WriteAscii");
  ENTER_V8(isolate);
  ASSERT(start >= 0 && length >= -1);
  i::Handle<i::String> str = Utils::OpenHandle(this);
  isolate->string_tracker()->RecordWrite(str);
  if (options & HINT_MANY_WRITES_EXPECTED) {
    FlattenString(str);  // Flatten the string for efficiency.
  }

  int end = length;
  if ((length == -1) || (length > str->length() - start)) {
    end = str->length() - start;
  }
  if (end < 0) return 0;
  i::StringCharacterStream write_stream(*str, isolate->write_iterator(), start);
  int i;
  for (i = 0; i < end; i++) {
    char c = static_cast<char>(write_stream.GetNext());
    if (c == '\0' && !(options & PRESERVE_ASCII_NULL)) c = ' ';
    buffer[i] = c;
  }
  if (!(options & NO_NULL_TERMINATION) && (length == -1 || i < length)) {
    buffer[i] = '\0';
  }
  return i;
}


template<typename CharType>
static inline int WriteHelper(const String* string,
                              CharType* buffer,
                              int start,
                              int length,
                              int options) {
  i::Isolate* isolate = Utils::OpenHandle(string)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::Write()")) return 0;
  LOG_API(isolate, "String::Write");
  ENTER_V8(isolate);
  ASSERT(start >= 0 && length >= -1);
  i::Handle<i::String> str = Utils::OpenHandle(string);
  isolate->string_tracker()->RecordWrite(str);
  if (options & String::HINT_MANY_WRITES_EXPECTED) {
    // Flatten the string for efficiency.  This applies whether we are
    // using StringCharacterStream or Get(i) to access the characters.
    FlattenString(str);
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
  if (IsDeadCheck(str->GetIsolate(), "v8::String::IsExternal()")) {
    return false;
  }
  EnsureInitializedForIsolate(str->GetIsolate(), "v8::String::IsExternal()");
  return i::StringShape(*str).IsExternalTwoByte();
}


bool v8::String::IsExternalAscii() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (IsDeadCheck(str->GetIsolate(), "v8::String::IsExternalAscii()")) {
    return false;
  }
  return i::StringShape(*str).IsExternalAscii();
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
    expected = NULL;
  }
  CHECK_EQ(expected, value);
}

void v8::String::VerifyExternalStringResourceBase(
    v8::String::ExternalStringResourceBase* value, Encoding encoding) const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  const v8::String::ExternalStringResourceBase* expected;
  Encoding expectedEncoding;
  if (i::StringShape(*str).IsExternalAscii()) {
    const void* resource =
        i::Handle<i::ExternalAsciiString>::cast(str)->resource();
    expected = reinterpret_cast<const ExternalStringResourceBase*>(resource);
    expectedEncoding = ASCII_ENCODING;
  } else if (i::StringShape(*str).IsExternalTwoByte()) {
    const void* resource =
        i::Handle<i::ExternalTwoByteString>::cast(str)->resource();
    expected = reinterpret_cast<const ExternalStringResourceBase*>(resource);
    expectedEncoding = TWO_BYTE_ENCODING;
  } else {
    expected = NULL;
    expectedEncoding = str->IsOneByteRepresentation() ? ASCII_ENCODING
                                                    : TWO_BYTE_ENCODING;
  }
  CHECK_EQ(expected, value);
  CHECK_EQ(expectedEncoding, encoding);
}

const v8::String::ExternalAsciiStringResource*
      v8::String::GetExternalAsciiStringResource() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (IsDeadCheck(str->GetIsolate(),
                  "v8::String::GetExternalAsciiStringResource()")) {
    return NULL;
  }
  if (i::StringShape(*str).IsExternalAscii()) {
    const void* resource =
        i::Handle<i::ExternalAsciiString>::cast(str)->resource();
    return reinterpret_cast<const ExternalAsciiStringResource*>(resource);
  } else {
    return NULL;
  }
}


Local<Value> Symbol::Name() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Symbol::Name()"))
    return Local<Value>();
  i::Handle<i::Symbol> sym = Utils::OpenHandle(this);
  i::Handle<i::Object> name(sym->name(), sym->GetIsolate());
  return Utils::ToLocal(name);
}


double Number::Value() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Number::Value()")) return 0;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->Number();
}


bool Boolean::Value() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Boolean::Value()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsTrue();
}


int64_t Integer::Value() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Integer::Value()")) return 0;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    return i::Smi::cast(*obj)->value();
  } else {
    return static_cast<int64_t>(obj->Number());
  }
}


int32_t Int32::Value() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Int32::Value()")) return 0;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    return i::Smi::cast(*obj)->value();
  } else {
    return static_cast<int32_t>(obj->Number());
  }
}


uint32_t Uint32::Value() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Uint32::Value()")) return 0;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) {
    return i::Smi::cast(*obj)->value();
  } else {
    return static_cast<uint32_t>(obj->Number());
  }
}


int v8::Object::InternalFieldCount() {
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  if (IsDeadCheck(obj->GetIsolate(), "v8::Object::InternalFieldCount()")) {
    return 0;
  }
  return obj->GetInternalFieldCount();
}


static bool InternalFieldOK(i::Handle<i::JSObject> obj,
                            int index,
                            const char* location) {
  return !IsDeadCheck(obj->GetIsolate(), location) &&
      ApiCheck(index < obj->GetInternalFieldCount(),
               location,
               "Internal field out of bounds");
}


Local<Value> v8::Object::SlowGetInternalField(int index) {
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::GetInternalField()";
  if (!InternalFieldOK(obj, index, location)) return Local<Value>();
  i::Handle<i::Object> value(obj->GetInternalField(index), obj->GetIsolate());
  return Utils::ToLocal(value);
}


void v8::Object::SetInternalField(int index, v8::Handle<Value> value) {
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::SetInternalField()";
  if (!InternalFieldOK(obj, index, location)) return;
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  obj->SetInternalField(index, *val);
  ASSERT_EQ(value, GetInternalField(index));
}


void* v8::Object::SlowGetAlignedPointerFromInternalField(int index) {
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::GetAlignedPointerFromInternalField()";
  if (!InternalFieldOK(obj, index, location)) return NULL;
  return DecodeSmiToAligned(obj->GetInternalField(index), location);
}


void v8::Object::SetAlignedPointerInInternalField(int index, void* value) {
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::SetAlignedPointerInInternalField()";
  if (!InternalFieldOK(obj, index, location)) return;
  obj->SetInternalField(index, EncodeAlignedAsSmi(value, location));
  ASSERT_EQ(value, GetAlignedPointerFromInternalField(index));
}


static void* ExternalValue(i::Object* obj) {
  // Obscure semantics for undefined, but somehow checked in our unit tests...
  if (obj->IsUndefined()) return NULL;
  i::Object* foreign = i::JSObject::cast(obj)->GetInternalField(0);
  return i::Foreign::cast(foreign)->foreign_address();
}


// --- E n v i r o n m e n t ---


bool v8::V8::Initialize() {
  i::Isolate* isolate = i::Isolate::UncheckedCurrent();
  if (isolate != NULL && isolate->IsInitialized()) {
    return true;
  }
  return InitializeHelper();
}


void v8::V8::SetEntropySource(EntropySource source) {
  i::V8::SetEntropySource(source);
}


void v8::V8::SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver return_address_resolver) {
  i::V8::SetReturnAddressLocationResolver(return_address_resolver);
}


bool v8::V8::SetFunctionEntryHook(FunctionEntryHook entry_hook) {
  return i::ProfileEntryHookStub::SetFunctionEntryHook(entry_hook);
}


void v8::V8::SetJitCodeEventHandler(
    JitCodeEventOptions options, JitCodeEventHandler event_handler) {
  i::Isolate* isolate = i::Isolate::Current();
  // Ensure that logging is initialized for our isolate.
  isolate->InitializeLoggingAndCounters();
  isolate->logger()->SetCodeEventHandler(options, event_handler);
}


bool v8::V8::Dispose() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!ApiCheck(isolate != NULL && isolate->IsDefaultIsolate(),
                "v8::V8::Dispose()",
                "Use v8::Isolate::Dispose() for a non-default isolate.")) {
    return false;
  }
  i::V8::TearDown();
  return true;
}


HeapStatistics::HeapStatistics(): total_heap_size_(0),
                                  total_heap_size_executable_(0),
                                  total_physical_size_(0),
                                  used_heap_size_(0),
                                  heap_size_limit_(0) { }


void v8::V8::GetHeapStatistics(HeapStatistics* heap_statistics) {
  i::Isolate* isolate = i::Isolate::UncheckedCurrent();
  if (isolate == NULL || !isolate->IsInitialized()) {
    // Isolate is unitialized thus heap is not configured yet.
    heap_statistics->total_heap_size_ = 0;
    heap_statistics->total_heap_size_executable_ = 0;
    heap_statistics->total_physical_size_ = 0;
    heap_statistics->used_heap_size_ = 0;
    heap_statistics->heap_size_limit_ = 0;
    return;
  }
  Isolate* ext_isolate = reinterpret_cast<Isolate*>(isolate);
  return ext_isolate->GetHeapStatistics(heap_statistics);
}


void v8::V8::VisitExternalResources(ExternalResourceVisitor* visitor) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::V8::VisitExternalResources");
  isolate->heap()->VisitExternalResources(visitor);
}


class VisitorAdapter : public i::ObjectVisitor {
 public:
  explicit VisitorAdapter(PersistentHandleVisitor* visitor)
      : visitor_(visitor) {}
  virtual void VisitPointers(i::Object** start, i::Object** end) {
    UNREACHABLE();
  }
  virtual void VisitEmbedderReference(i::Object** p, uint16_t class_id) {
    visitor_->VisitPersistentHandle(ToApi<Value>(i::Handle<i::Object>(p)),
                                    class_id);
  }
 private:
  PersistentHandleVisitor* visitor_;
};


void v8::V8::VisitHandlesWithClassIds(PersistentHandleVisitor* visitor) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::V8::VisitHandlesWithClassId");

  i::AssertNoAllocation no_allocation;

  VisitorAdapter visitor_adapter(visitor);
  isolate->global_handles()->IterateAllRootsWithClassIds(&visitor_adapter);
}


void v8::V8::VisitHandlesForPartialDependence(
    Isolate* exported_isolate, PersistentHandleVisitor* visitor) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(exported_isolate);
  ASSERT(isolate == i::Isolate::Current());
  IsDeadCheck(isolate, "v8::V8::VisitHandlesForPartialDependence");

  i::AssertNoAllocation no_allocation;

  VisitorAdapter visitor_adapter(visitor);
  isolate->global_handles()->IterateAllRootsInNewSpaceWithClassIds(
      &visitor_adapter);
}


bool v8::V8::IdleNotification(int hint) {
  // Returning true tells the caller that it need not
  // continue to call IdleNotification.
  i::Isolate* isolate = i::Isolate::Current();
  if (isolate == NULL || !isolate->IsInitialized()) return true;
  return i::V8::IdleNotification(hint);
}


void v8::V8::LowMemoryNotification() {
  i::Isolate* isolate = i::Isolate::Current();
  if (isolate == NULL || !isolate->IsInitialized()) return;
  isolate->heap()->CollectAllAvailableGarbage("low memory notification");
}


int v8::V8::ContextDisposedNotification() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!isolate->IsInitialized()) return 0;
  return isolate->heap()->NotifyContextDisposed();
}


const char* v8::V8::GetVersion() {
  return i::Version::GetVersion();
}


static i::Handle<i::FunctionTemplateInfo>
    EnsureConstructor(i::Handle<i::ObjectTemplateInfo> templ) {
  if (templ->constructor()->IsUndefined()) {
    Local<FunctionTemplate> constructor = FunctionTemplate::New();
    Utils::OpenHandle(*constructor)->set_instance_template(*templ);
    templ->set_constructor(*Utils::OpenHandle(*constructor));
  }
  return i::Handle<i::FunctionTemplateInfo>(
    i::FunctionTemplateInfo::cast(templ->constructor()));
}


static i::Handle<i::Context> CreateEnvironment(
    i::Isolate* isolate,
    v8::ExtensionConfiguration* extensions,
    v8::Handle<ObjectTemplate> global_template,
    v8::Handle<Value> global_object) {
  i::Handle<i::Context> env;

  // Enter V8 via an ENTER_V8 scope.
  {
    ENTER_V8(isolate);
    v8::Handle<ObjectTemplate> proxy_template = global_template;
    i::Handle<i::FunctionTemplateInfo> proxy_constructor;
    i::Handle<i::FunctionTemplateInfo> global_constructor;

    if (!global_template.IsEmpty()) {
      // Make sure that the global_template has a constructor.
      global_constructor =
          EnsureConstructor(Utils::OpenHandle(*global_template));

      // Create a fresh template for the global proxy object.
      proxy_template = ObjectTemplate::New();
      proxy_constructor =
          EnsureConstructor(Utils::OpenHandle(*proxy_template));

      // Set the global template to be the prototype template of
      // global proxy template.
      proxy_constructor->set_prototype_template(
          *Utils::OpenHandle(*global_template));

      // Migrate security handlers from global_template to
      // proxy_template.  Temporarily removing access check
      // information from the global template.
      if (!global_constructor->access_check_info()->IsUndefined()) {
        proxy_constructor->set_access_check_info(
            global_constructor->access_check_info());
        proxy_constructor->set_needs_access_check(
            global_constructor->needs_access_check());
        global_constructor->set_needs_access_check(false);
        global_constructor->set_access_check_info(
            isolate->heap()->undefined_value());
      }
    }

    // Create the environment.
    env = isolate->bootstrapper()->CreateEnvironment(
        Utils::OpenHandle(*global_object, true),
        proxy_template,
        extensions);

    // Restore the access check info on the global template.
    if (!global_template.IsEmpty()) {
      ASSERT(!global_constructor.is_null());
      ASSERT(!proxy_constructor.is_null());
      global_constructor->set_access_check_info(
          proxy_constructor->access_check_info());
      global_constructor->set_needs_access_check(
          proxy_constructor->needs_access_check());
    }
    isolate->runtime_profiler()->Reset();
  }
  // Leave V8.

  return env;
}


Persistent<Context> v8::Context::New(
    v8::ExtensionConfiguration* extensions,
    v8::Handle<ObjectTemplate> global_template,
    v8::Handle<Value> global_object) {
  i::Isolate::EnsureDefaultIsolate();
  i::Isolate* isolate = i::Isolate::Current();
  Isolate* external_isolate = reinterpret_cast<Isolate*>(isolate);
  EnsureInitializedForIsolate(isolate, "v8::Context::New()");
  LOG_API(isolate, "Context::New");
  ON_BAILOUT(isolate, "v8::Context::New()", return Persistent<Context>());
  i::HandleScope scope(isolate);
  i::Handle<i::Context> env =
      CreateEnvironment(isolate, extensions, global_template, global_object);
  if (env.is_null()) return Persistent<Context>();
  return Persistent<Context>::New(external_isolate, Utils::ToLocal(env));
}


Local<Context> v8::Context::New(
    v8::Isolate* external_isolate,
    v8::ExtensionConfiguration* extensions,
    v8::Handle<ObjectTemplate> global_template,
    v8::Handle<Value> global_object) {
  i::Isolate::EnsureDefaultIsolate();
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  EnsureInitializedForIsolate(isolate, "v8::Context::New()");
  LOG_API(isolate, "Context::New");
  ON_BAILOUT(isolate, "v8::Context::New()", return Local<Context>());
  i::HandleScope scope(isolate);
  i::Handle<i::Context> env =
      CreateEnvironment(isolate, extensions, global_template, global_object);
  if (env.is_null()) return Local<Context>();
  return Utils::ToLocal(scope.CloseAndEscape(env));
}


void v8::Context::SetSecurityToken(Handle<Value> token) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::SetSecurityToken()")) {
    return;
  }
  ENTER_V8(isolate);
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Handle<i::Object> token_handle = Utils::OpenHandle(*token);
  env->set_security_token(*token_handle);
}


void v8::Context::UseDefaultSecurityToken() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate,
                  "v8::Context::UseDefaultSecurityToken()")) {
    return;
  }
  ENTER_V8(isolate);
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  env->set_security_token(env->global_object());
}


Handle<Value> v8::Context::GetSecurityToken() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::GetSecurityToken()")) {
    return Handle<Value>();
  }
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Object* security_token = env->security_token();
  i::Handle<i::Object> token_handle(security_token, isolate);
  return Utils::ToLocal(token_handle);
}


bool Context::HasOutOfMemoryException() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  return env->has_out_of_memory();
}


bool Context::InContext() {
  return i::Isolate::Current()->context() != NULL;
}


v8::Isolate* Context::GetIsolate() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  return reinterpret_cast<Isolate*>(env->GetIsolate());
}


v8::Local<v8::Context> Context::GetEntered() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!EnsureInitializedForIsolate(isolate, "v8::Context::GetEntered()")) {
    return Local<Context>();
  }
  i::Handle<i::Object> last =
      isolate->handle_scope_implementer()->LastEnteredContext();
  if (last.is_null()) return Local<Context>();
  i::Handle<i::Context> context = i::Handle<i::Context>::cast(last);
  return Utils::ToLocal(context);
}


v8::Local<v8::Context> Context::GetCurrent() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::GetCurrent()")) {
    return Local<Context>();
  }
  return reinterpret_cast<Isolate*>(isolate)->GetCurrentContext();
}


v8::Local<v8::Context> Context::GetCalling() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::GetCalling()")) {
    return Local<Context>();
  }
  i::Handle<i::Object> calling =
      isolate->GetCallingNativeContext();
  if (calling.is_null()) return Local<Context>();
  i::Handle<i::Context> context = i::Handle<i::Context>::cast(calling);
  return Utils::ToLocal(context);
}


v8::Local<v8::Object> Context::Global() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::Global()")) {
    return Local<v8::Object>();
  }
  i::Object** ctx = reinterpret_cast<i::Object**>(this);
  i::Handle<i::Context> context =
      i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
  i::Handle<i::Object> global(context->global_proxy(), isolate);
  return Utils::ToLocal(i::Handle<i::JSObject>::cast(global));
}


void Context::DetachGlobal() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::DetachGlobal()")) return;
  ENTER_V8(isolate);
  i::Object** ctx = reinterpret_cast<i::Object**>(this);
  i::Handle<i::Context> context =
      i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
  isolate->bootstrapper()->DetachGlobal(context);
}


void Context::ReattachGlobal(Handle<Object> global_object) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::ReattachGlobal()")) return;
  ENTER_V8(isolate);
  i::Object** ctx = reinterpret_cast<i::Object**>(this);
  i::Handle<i::Context> context =
      i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
  i::Handle<i::JSGlobalProxy> global_proxy =
      i::Handle<i::JSGlobalProxy>::cast(Utils::OpenHandle(*global_object));
  isolate->bootstrapper()->ReattachGlobal(context, global_proxy);
}


void Context::AllowCodeGenerationFromStrings(bool allow) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::AllowCodeGenerationFromStrings()")) {
    return;
  }
  ENTER_V8(isolate);
  i::Object** ctx = reinterpret_cast<i::Object**>(this);
  i::Handle<i::Context> context =
      i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
  context->set_allow_code_gen_from_strings(
      allow ? isolate->heap()->true_value() : isolate->heap()->false_value());
}


bool Context::IsCodeGenerationFromStringsAllowed() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate,
                  "v8::Context::IsCodeGenerationFromStringsAllowed()")) {
    return false;
  }
  ENTER_V8(isolate);
  i::Object** ctx = reinterpret_cast<i::Object**>(this);
  i::Handle<i::Context> context =
      i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
  return !context->allow_code_gen_from_strings()->IsFalse();
}


void Context::SetErrorMessageForCodeGenerationFromStrings(
    Handle<String> error) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate,
      "v8::Context::SetErrorMessageForCodeGenerationFromStrings()")) {
    return;
  }
  ENTER_V8(isolate);
  i::Object** ctx = reinterpret_cast<i::Object**>(this);
  i::Handle<i::Context> context =
      i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
  i::Handle<i::String> error_handle = Utils::OpenHandle(*error);
  context->set_error_message_for_code_gen_from_strings(*error_handle);
}


Local<v8::Object> ObjectTemplate::NewInstance() {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::ObjectTemplate::NewInstance()",
             return Local<v8::Object>());
  LOG_API(isolate, "ObjectTemplate::NewInstance");
  ENTER_V8(isolate);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj =
      i::Execution::InstantiateObject(Utils::OpenHandle(this),
                                      &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, Local<v8::Object>());
  return Utils::ToLocal(i::Handle<i::JSObject>::cast(obj));
}


Local<v8::Function> FunctionTemplate::GetFunction() {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::FunctionTemplate::GetFunction()",
             return Local<v8::Function>());
  LOG_API(isolate, "FunctionTemplate::GetFunction");
  ENTER_V8(isolate);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj =
      i::Execution::InstantiateFunction(Utils::OpenHandle(this),
                                        &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, Local<v8::Function>());
  return Utils::ToLocal(i::Handle<i::JSFunction>::cast(obj));
}


bool FunctionTemplate::HasInstance(v8::Handle<v8::Value> value) {
  ON_BAILOUT(i::Isolate::Current(), "v8::FunctionTemplate::HasInstanceOf()",
             return false);
  i::Object* obj = *Utils::OpenHandle(*value);
  return obj->IsInstanceOf(*Utils::OpenHandle(this));
}


Local<External> v8::External::New(void* value) {
  STATIC_ASSERT(sizeof(value) == sizeof(i::Address));
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::External::New()");
  LOG_API(isolate, "External::New");
  ENTER_V8(isolate);
  i::Handle<i::JSObject> external = isolate->factory()->NewExternal(value);
  return Utils::ExternalToLocal(external);
}


void* External::Value() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::External::Value()")) return NULL;
  return ExternalValue(*Utils::OpenHandle(this));
}


Local<String> v8::String::Empty() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!EnsureInitializedForIsolate(isolate, "v8::String::Empty()")) {
    return v8::Local<String>();
  }
  LOG_API(isolate, "String::Empty()");
  return Utils::ToLocal(isolate->factory()->empty_string());
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


inline i::Handle<i::String> NewString(i::Factory* factory,
                                      String::NewStringType type,
                                      i::Vector<const char> string) {
  if (type ==String::kInternalizedString) {
    return factory->InternalizeUtf8String(string);
  }
  return factory->NewStringFromUtf8(string);
}


inline i::Handle<i::String> NewString(i::Factory* factory,
                                      String::NewStringType type,
                                      i::Vector<const uint8_t> string) {
  if (type == String::kInternalizedString) {
    return factory->InternalizeOneByteString(string);
  }
  return factory->NewStringFromOneByte(string);
}


inline i::Handle<i::String> NewString(i::Factory* factory,
                                      String::NewStringType type,
                                      i::Vector<const uint16_t> string) {
  if (type == String::kInternalizedString) {
    return factory->InternalizeTwoByteString(string);
  }
  return factory->NewStringFromTwoByte(string);
}


template<typename Char>
inline Local<String> NewString(Isolate* v8_isolate,
                               const char* location,
                               const char* env,
                               const Char* data,
                               String::NewStringType type,
                               int length) {
  i::Isolate* isolate = reinterpret_cast<internal::Isolate*>(v8_isolate);
  EnsureInitializedForIsolate(isolate, location);
  LOG_API(isolate, env);
  if (length == 0 && type != String::kUndetectableString) {
    return String::Empty();
  }
  ENTER_V8(isolate);
  if (length == -1) length = StringLength(data);
  i::Handle<i::String> result = NewString(
      isolate->factory(), type, i::Vector<const Char>(data, length));
  if (type == String::kUndetectableString) {
    result->MarkAsUndetectable();
  }
  return Utils::ToLocal(result);
}

}  // anonymous namespace


Local<String> String::NewFromUtf8(Isolate* isolate,
                                  const char* data,
                                  NewStringType type,
                                  int length) {
  return NewString(isolate,
                   "v8::String::NewFromUtf8()",
                   "String::NewFromUtf8",
                   data,
                   type,
                   length);
}


Local<String> String::NewFromOneByte(Isolate* isolate,
                                     const uint8_t* data,
                                     NewStringType type,
                                     int length) {
  return NewString(isolate,
                   "v8::String::NewFromOneByte()",
                   "String::NewFromOneByte",
                   data,
                   type,
                   length);
}


Local<String> String::NewFromTwoByte(Isolate* isolate,
                                     const uint16_t* data,
                                     NewStringType type,
                                     int length) {
  return NewString(isolate,
                   "v8::String::NewFromTwoByte()",
                   "String::NewFromTwoByte",
                   data,
                   type,
                   length);
}


Local<String> v8::String::Concat(Handle<String> left, Handle<String> right) {
  i::Handle<i::String> left_string = Utils::OpenHandle(*left);
  i::Isolate* isolate = left_string->GetIsolate();
  EnsureInitializedForIsolate(isolate, "v8::String::New()");
  LOG_API(isolate, "String::New(char)");
  ENTER_V8(isolate);
  i::Handle<i::String> right_string = Utils::OpenHandle(*right);
  i::Handle<i::String> result = isolate->factory()->NewConsString(left_string,
                                                                  right_string);
  return Utils::ToLocal(result);
}


i::Handle<i::String> NewExternalStringHandle(i::Isolate* isolate,
      v8::String::ExternalStringResource* resource) {
  i::Handle<i::String> result =
      isolate->factory()->NewExternalStringFromTwoByte(resource);
  return result;
}


i::Handle<i::String> NewExternalAsciiStringHandle(i::Isolate* isolate,
      v8::String::ExternalAsciiStringResource* resource) {
  i::Handle<i::String> result =
      isolate->factory()->NewExternalStringFromAscii(resource);
  return result;
}


Local<String> v8::String::NewExternal(
      v8::String::ExternalStringResource* resource) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::String::NewExternal()");
  LOG_API(isolate, "String::NewExternal");
  ENTER_V8(isolate);
  CHECK(resource && resource->data());
  i::Handle<i::String> result = NewExternalStringHandle(isolate, resource);
  isolate->heap()->external_string_table()->AddString(*result);
  return Utils::ToLocal(result);
}


bool v8::String::MakeExternal(v8::String::ExternalStringResource* resource) {
  i::Handle<i::String> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::MakeExternal()")) return false;
  if (i::StringShape(*obj).IsExternalTwoByte()) {
    return false;  // Already an external string.
  }
  ENTER_V8(isolate);
  if (isolate->string_tracker()->IsFreshUnusedString(obj)) {
    return false;
  }
  if (isolate->heap()->IsInGCPostProcessing()) {
    return false;
  }
  CHECK(resource && resource->data());
  bool result = obj->MakeExternal(resource);
  if (result && !obj->IsInternalizedString()) {
    isolate->heap()->external_string_table()->AddString(*obj);
  }
  return result;
}


Local<String> v8::String::NewExternal(
      v8::String::ExternalAsciiStringResource* resource) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::String::NewExternal()");
  LOG_API(isolate, "String::NewExternal");
  ENTER_V8(isolate);
  CHECK(resource && resource->data());
  i::Handle<i::String> result = NewExternalAsciiStringHandle(isolate, resource);
  isolate->heap()->external_string_table()->AddString(*result);
  return Utils::ToLocal(result);
}


bool v8::String::MakeExternal(
    v8::String::ExternalAsciiStringResource* resource) {
  i::Handle<i::String> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::MakeExternal()")) return false;
  if (i::StringShape(*obj).IsExternalTwoByte()) {
    return false;  // Already an external string.
  }
  ENTER_V8(isolate);
  if (isolate->string_tracker()->IsFreshUnusedString(obj)) {
    return false;
  }
  if (isolate->heap()->IsInGCPostProcessing()) {
    return false;
  }
  CHECK(resource && resource->data());
  bool result = obj->MakeExternal(resource);
  if (result && !obj->IsInternalizedString()) {
    isolate->heap()->external_string_table()->AddString(*obj);
  }
  return result;
}


bool v8::String::CanMakeExternal() {
  if (!internal::FLAG_clever_optimizations) return false;
  i::Handle<i::String> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::CanMakeExternal()")) return false;
  if (isolate->string_tracker()->IsFreshUnusedString(obj)) return false;
  int size = obj->Size();  // Byte size of the original string.
  if (size < i::ExternalString::kShortSize) return false;
  i::StringShape shape(*obj);
  return !shape.IsExternal();
}


Local<v8::Object> v8::Object::New() {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Object::New()");
  LOG_API(isolate, "Object::New");
  ENTER_V8(isolate);
  i::Handle<i::JSObject> obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  return Utils::ToLocal(obj);
}


Local<v8::Value> v8::NumberObject::New(double value) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::NumberObject::New()");
  LOG_API(isolate, "NumberObject::New");
  ENTER_V8(isolate);
  i::Handle<i::Object> number = isolate->factory()->NewNumber(value);
  i::Handle<i::Object> obj = isolate->factory()->ToObject(number);
  return Utils::ToLocal(obj);
}


double v8::NumberObject::NumberValue() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::NumberObject::NumberValue()")) return 0;
  LOG_API(isolate, "NumberObject::NumberValue");
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSValue> jsvalue = i::Handle<i::JSValue>::cast(obj);
  return jsvalue->value()->Number();
}


Local<v8::Value> v8::BooleanObject::New(bool value) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::BooleanObject::New()");
  LOG_API(isolate, "BooleanObject::New");
  ENTER_V8(isolate);
  i::Handle<i::Object> boolean(value
                               ? isolate->heap()->true_value()
                               : isolate->heap()->false_value(),
                               isolate);
  i::Handle<i::Object> obj = isolate->factory()->ToObject(boolean);
  return Utils::ToLocal(obj);
}


bool v8::BooleanObject::BooleanValue() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::BooleanObject::BooleanValue()")) return 0;
  LOG_API(isolate, "BooleanObject::BooleanValue");
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSValue> jsvalue = i::Handle<i::JSValue>::cast(obj);
  return jsvalue->value()->IsTrue();
}


Local<v8::Value> v8::StringObject::New(Handle<String> value) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::StringObject::New()");
  LOG_API(isolate, "StringObject::New");
  ENTER_V8(isolate);
  i::Handle<i::Object> obj =
      isolate->factory()->ToObject(Utils::OpenHandle(*value));
  return Utils::ToLocal(obj);
}


Local<v8::String> v8::StringObject::StringValue() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::StringObject::StringValue()")) {
    return Local<v8::String>();
  }
  LOG_API(isolate, "StringObject::StringValue");
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSValue> jsvalue = i::Handle<i::JSValue>::cast(obj);
  return Utils::ToLocal(
      i::Handle<i::String>(i::String::cast(jsvalue->value())));
}


Local<v8::Value> v8::SymbolObject::New(Isolate* isolate, Handle<Symbol> value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  EnsureInitializedForIsolate(i_isolate, "v8::SymbolObject::New()");
  LOG_API(i_isolate, "SymbolObject::New");
  ENTER_V8(i_isolate);
  i::Handle<i::Object> obj =
      i_isolate->factory()->ToObject(Utils::OpenHandle(*value));
  return Utils::ToLocal(obj);
}


Local<v8::Symbol> v8::SymbolObject::SymbolValue() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::SymbolObject::SymbolValue()"))
    return Local<v8::Symbol>();
  LOG_API(isolate, "SymbolObject::SymbolValue");
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSValue> jsvalue = i::Handle<i::JSValue>::cast(obj);
  return Utils::ToLocal(
      i::Handle<i::Symbol>(i::Symbol::cast(jsvalue->value())));
}


Local<v8::Value> v8::Date::New(double time) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Date::New()");
  LOG_API(isolate, "Date::New");
  if (std::isnan(time)) {
    // Introduce only canonical NaN value into the VM, to avoid signaling NaNs.
    time = i::OS::nan_value();
  }
  ENTER_V8(isolate);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::Object> obj =
      i::Execution::NewDate(time, &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, Local<v8::Value>());
  return Utils::ToLocal(obj);
}


double v8::Date::NumberValue() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Date::NumberValue()")) return 0;
  LOG_API(isolate, "Date::NumberValue");
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSDate> jsdate = i::Handle<i::JSDate>::cast(obj);
  return jsdate->value()->Number();
}


void v8::Date::DateTimeConfigurationChangeNotification() {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Date::DateTimeConfigurationChangeNotification()",
             return);
  LOG_API(isolate, "Date::DateTimeConfigurationChangeNotification");
  ENTER_V8(isolate);

  isolate->date_cache()->ResetDateCache();

  i::HandleScope scope(isolate);
  // Get the function ResetDateCache (defined in date.js).
  i::Handle<i::String> func_name_str =
      isolate->factory()->InternalizeOneByteString(
          STATIC_ASCII_VECTOR("ResetDateCache"));
  i::MaybeObject* result =
      isolate->js_builtins_object()->GetProperty(*func_name_str);
  i::Object* object_func;
  if (!result->ToObject(&object_func)) {
    return;
  }

  if (object_func->IsJSFunction()) {
    i::Handle<i::JSFunction> func =
        i::Handle<i::JSFunction>(i::JSFunction::cast(object_func));

    // Call ResetDateCache(0 but expect no exceptions:
    bool caught_exception = false;
    i::Execution::TryCall(func,
                          isolate->js_builtins_object(),
                          0,
                          NULL,
                          &caught_exception);
  }
}


static i::Handle<i::String> RegExpFlagsToString(RegExp::Flags flags) {
  uint8_t flags_buf[3];
  int num_flags = 0;
  if ((flags & RegExp::kGlobal) != 0) flags_buf[num_flags++] = 'g';
  if ((flags & RegExp::kMultiline) != 0) flags_buf[num_flags++] = 'm';
  if ((flags & RegExp::kIgnoreCase) != 0) flags_buf[num_flags++] = 'i';
  ASSERT(num_flags <= static_cast<int>(ARRAY_SIZE(flags_buf)));
  return FACTORY->InternalizeOneByteString(
      i::Vector<const uint8_t>(flags_buf, num_flags));
}


Local<v8::RegExp> v8::RegExp::New(Handle<String> pattern,
                                  Flags flags) {
  i::Isolate* isolate = Utils::OpenHandle(*pattern)->GetIsolate();
  EnsureInitializedForIsolate(isolate, "v8::RegExp::New()");
  LOG_API(isolate, "RegExp::New");
  ENTER_V8(isolate);
  EXCEPTION_PREAMBLE(isolate);
  i::Handle<i::JSRegExp> obj = i::Execution::NewJSRegExp(
      Utils::OpenHandle(*pattern),
      RegExpFlagsToString(flags),
      &has_pending_exception);
  EXCEPTION_BAILOUT_CHECK(isolate, Local<v8::RegExp>());
  return Utils::ToLocal(i::Handle<i::JSRegExp>::cast(obj));
}


Local<v8::String> v8::RegExp::GetSource() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::RegExp::GetSource()")) {
    return Local<v8::String>();
  }
  i::Handle<i::JSRegExp> obj = Utils::OpenHandle(this);
  return Utils::ToLocal(i::Handle<i::String>(obj->Pattern()));
}


// Assert that the static flags cast in GetFlags is valid.
#define REGEXP_FLAG_ASSERT_EQ(api_flag, internal_flag)        \
  STATIC_ASSERT(static_cast<int>(v8::RegExp::api_flag) ==     \
                static_cast<int>(i::JSRegExp::internal_flag))
REGEXP_FLAG_ASSERT_EQ(kNone, NONE);
REGEXP_FLAG_ASSERT_EQ(kGlobal, GLOBAL);
REGEXP_FLAG_ASSERT_EQ(kIgnoreCase, IGNORE_CASE);
REGEXP_FLAG_ASSERT_EQ(kMultiline, MULTILINE);
#undef REGEXP_FLAG_ASSERT_EQ

v8::RegExp::Flags v8::RegExp::GetFlags() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::RegExp::GetFlags()")) {
    return v8::RegExp::kNone;
  }
  i::Handle<i::JSRegExp> obj = Utils::OpenHandle(this);
  return static_cast<RegExp::Flags>(obj->GetFlags().value());
}


Local<v8::Array> v8::Array::New(int length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Array::New()");
  LOG_API(isolate, "Array::New");
  ENTER_V8(isolate);
  int real_length = length > 0 ? length : 0;
  i::Handle<i::JSArray> obj = isolate->factory()->NewJSArray(real_length);
  i::Handle<i::Object> length_obj =
      isolate->factory()->NewNumberFromInt(real_length);
  obj->set_length(*length_obj);
  return Utils::ToLocal(obj);
}


uint32_t v8::Array::Length() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Array::Length()")) return 0;
  i::Handle<i::JSArray> obj = Utils::OpenHandle(this);
  i::Object* length = obj->length();
  if (length->IsSmi()) {
    return i::Smi::cast(length)->value();
  } else {
    return static_cast<uint32_t>(length->Number());
  }
}


Local<Object> Array::CloneElementAt(uint32_t index) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Array::CloneElementAt()", return Local<Object>());
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  if (!self->HasFastObjectElements()) {
    return Local<Object>();
  }
  i::FixedArray* elms = i::FixedArray::cast(self->elements());
  i::Object* paragon = elms->get(index);
  if (!paragon->IsJSObject()) {
    return Local<Object>();
  }
  i::Handle<i::JSObject> paragon_handle(i::JSObject::cast(paragon));
  EXCEPTION_PREAMBLE(isolate);
  ENTER_V8(isolate);
  i::Handle<i::JSObject> result = i::Copy(paragon_handle);
  has_pending_exception = result.is_null();
  EXCEPTION_BAILOUT_CHECK(isolate, Local<Object>());
  return Utils::ToLocal(result);
}


size_t v8::ArrayBuffer::ByteLength() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ArrayBuffer::ByteLength()")) return 0;
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->byte_length()->Number());
}


void* v8::ArrayBuffer::Data() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ArrayBuffer::Data()")) return 0;
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return obj->backing_store();
}


Local<ArrayBuffer> v8::ArrayBuffer::New(size_t byte_length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::ArrayBuffer::New(size_t)");
  LOG_API(isolate, "v8::ArrayBuffer::New(size_t)");
  ENTER_V8(isolate);
  i::Handle<i::JSArrayBuffer> obj =
      isolate->factory()->NewJSArrayBuffer();
  i::Runtime::SetupArrayBufferAllocatingData(isolate, obj, byte_length);
  return Utils::ToLocal(obj);
}


Local<ArrayBuffer> v8::ArrayBuffer::New(void* data, size_t byte_length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::ArrayBuffer::New(void*, size_t)");
  LOG_API(isolate, "v8::ArrayBuffer::New(void*, size_t)");
  ENTER_V8(isolate);
  i::Handle<i::JSArrayBuffer> obj =
      isolate->factory()->NewJSArrayBuffer();
  i::Runtime::SetupArrayBuffer(isolate, obj, data, byte_length);
  return Utils::ToLocal(obj);
}


Local<ArrayBuffer> v8::TypedArray::Buffer() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::TypedArray::Buffer()"))
    return Local<ArrayBuffer>();
  i::Handle<i::JSTypedArray> obj = Utils::OpenHandle(this);
  ASSERT(obj->buffer()->IsJSArrayBuffer());
  i::Handle<i::JSArrayBuffer> buffer(i::JSArrayBuffer::cast(obj->buffer()));
  return Utils::ToLocal(buffer);
}


size_t v8::TypedArray::ByteOffset() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::TypedArray::ByteOffset()")) return 0;
  i::Handle<i::JSTypedArray> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->byte_offset()->Number());
}


size_t v8::TypedArray::ByteLength() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::TypedArray::ByteLength()")) return 0;
  i::Handle<i::JSTypedArray> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->byte_length()->Number());
}


size_t v8::TypedArray::Length() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::TypedArray::Length()")) return 0;
  i::Handle<i::JSTypedArray> obj = Utils::OpenHandle(this);
  return static_cast<size_t>(obj->length()->Number());
}


void* v8::TypedArray::BaseAddress() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::TypedArray::BaseAddress()")) return NULL;
  i::Handle<i::JSTypedArray> obj = Utils::OpenHandle(this);
  i::Handle<i::JSArrayBuffer> buffer(i::JSArrayBuffer::cast(obj->buffer()));
  void* buffer_data = buffer->backing_store();
  size_t byte_offset = static_cast<size_t>(obj->byte_offset()->Number());
  return static_cast<uint8_t*>(buffer_data) + byte_offset;
}


template<typename ElementType,
         ExternalArrayType array_type,
         i::ElementsKind elements_kind>
i::Handle<i::JSTypedArray> NewTypedArray(
    i::Isolate* isolate,
    Handle<ArrayBuffer> array_buffer, size_t byte_offset, size_t length) {
  i::Handle<i::JSTypedArray> obj =
      isolate->factory()->NewJSTypedArray(array_type);
  i::Handle<i::JSArrayBuffer> buffer = Utils::OpenHandle(*array_buffer);

  ASSERT(byte_offset % sizeof(ElementType) == 0);
  ASSERT(byte_offset + length * sizeof(ElementType) <=
      static_cast<size_t>(buffer->byte_length()->Number()));

  obj->set_buffer(*buffer);

  i::Handle<i::Object> byte_offset_object = isolate->factory()->NewNumber(
        static_cast<double>(byte_offset));
  obj->set_byte_offset(*byte_offset_object);

  i::Handle<i::Object> byte_length_object = isolate->factory()->NewNumber(
        static_cast<double>(length * sizeof(ElementType)));
  obj->set_byte_length(*byte_length_object);

  i::Handle<i::Object> length_object = isolate->factory()->NewNumber(
        static_cast<double>(length));
  obj->set_length(*length_object);

  i::Handle<i::ExternalArray> elements =
      isolate->factory()->NewExternalArray(
          static_cast<int>(length), array_type,
          static_cast<uint8_t*>(buffer->backing_store()) + byte_offset);
  i::Handle<i::Map> map =
      isolate->factory()->GetElementsTransitionMap(
          obj, elements_kind);
  obj->set_map(*map);
  obj->set_elements(*elements);
  return obj;
}


#define TYPED_ARRAY_NEW(TypedArray, element_type, array_type, elements_kind) \
  Local<TypedArray> TypedArray::New(Handle<ArrayBuffer> array_buffer,        \
                                    size_t byte_offset, size_t length) {     \
    i::Isolate* isolate = i::Isolate::Current();                             \
    EnsureInitializedForIsolate(isolate,                                     \
        "v8::" #TypedArray "::New(Handle<ArrayBuffer>, size_t, size_t)");    \
    LOG_API(isolate,                                                         \
        "v8::" #TypedArray "::New(Handle<ArrayBuffer>, size_t, size_t)");    \
    ENTER_V8(isolate);                                                       \
    i::Handle<i::JSTypedArray> obj =                                         \
        NewTypedArray<element_type, array_type, elements_kind>(              \
            isolate, array_buffer, byte_offset, length);                     \
    return Utils::ToLocal##TypedArray(obj);                                  \
  }


TYPED_ARRAY_NEW(Uint8Array, uint8_t, kExternalUnsignedByteArray,
                i::EXTERNAL_UNSIGNED_BYTE_ELEMENTS)
TYPED_ARRAY_NEW(Uint8ClampedArray, uint8_t, kExternalPixelArray,
                i::EXTERNAL_PIXEL_ELEMENTS)
TYPED_ARRAY_NEW(Int8Array, int8_t, kExternalByteArray,
                i::EXTERNAL_BYTE_ELEMENTS)
TYPED_ARRAY_NEW(Uint16Array, uint16_t, kExternalUnsignedShortArray,
                i::EXTERNAL_UNSIGNED_SHORT_ELEMENTS)
TYPED_ARRAY_NEW(Int16Array, int16_t, kExternalShortArray,
                i::EXTERNAL_SHORT_ELEMENTS)
TYPED_ARRAY_NEW(Uint32Array, uint32_t, kExternalUnsignedIntArray,
                i::EXTERNAL_UNSIGNED_INT_ELEMENTS)
TYPED_ARRAY_NEW(Int32Array, int32_t, kExternalIntArray,
                i::EXTERNAL_INT_ELEMENTS)
TYPED_ARRAY_NEW(Float32Array, float, kExternalFloatArray,
                i::EXTERNAL_FLOAT_ELEMENTS)
TYPED_ARRAY_NEW(Float64Array, double, kExternalDoubleArray,
                i::EXTERNAL_DOUBLE_ELEMENTS)

#undef TYPED_ARRAY_NEW


Local<Symbol> v8::Symbol::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  EnsureInitializedForIsolate(i_isolate, "v8::Symbol::New()");
  LOG_API(i_isolate, "Symbol::New()");
  ENTER_V8(i_isolate);
  i::Handle<i::Symbol> result = i_isolate->factory()->NewSymbol();
  return Utils::ToLocal(result);
}


Local<Symbol> v8::Symbol::New(Isolate* isolate, const char* data, int length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  EnsureInitializedForIsolate(i_isolate, "v8::Symbol::New()");
  LOG_API(i_isolate, "Symbol::New(char)");
  ENTER_V8(i_isolate);
  if (length == -1) length = i::StrLength(data);
  i::Handle<i::String> name = i_isolate->factory()->NewStringFromUtf8(
      i::Vector<const char>(data, length));
  i::Handle<i::Symbol> result = i_isolate->factory()->NewSymbol();
  result->set_name(*name);
  return Utils::ToLocal(result);
}


Local<Number> v8::Number::New(double value) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Number::New()");
  if (std::isnan(value)) {
    // Introduce only canonical NaN value into the VM, to avoid signaling NaNs.
    value = i::OS::nan_value();
  }
  ENTER_V8(isolate);
  i::Handle<i::Object> result = isolate->factory()->NewNumber(value);
  return Utils::NumberToLocal(result);
}


Local<Integer> v8::Integer::New(int32_t value) {
  i::Isolate* isolate = i::Isolate::UncheckedCurrent();
  EnsureInitializedForIsolate(isolate, "v8::Integer::New()");
  return v8::Integer::New(value, reinterpret_cast<Isolate*>(isolate));
}


Local<Integer> Integer::NewFromUnsigned(uint32_t value) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Integer::NewFromUnsigned()");
  return Integer::NewFromUnsigned(value, reinterpret_cast<Isolate*>(isolate));
}


Local<Integer> v8::Integer::New(int32_t value, Isolate* isolate) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ASSERT(internal_isolate->IsInitialized());
  if (i::Smi::IsValid(value)) {
    return Utils::IntegerToLocal(i::Handle<i::Object>(i::Smi::FromInt(value),
                                                      internal_isolate));
  }
  ENTER_V8(internal_isolate);
  i::Handle<i::Object> result = internal_isolate->factory()->NewNumber(value);
  return Utils::IntegerToLocal(result);
}


Local<Integer> v8::Integer::NewFromUnsigned(uint32_t value, Isolate* isolate) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ASSERT(internal_isolate->IsInitialized());
  bool fits_into_int32_t = (value & (1 << 31)) == 0;
  if (fits_into_int32_t) {
    return Integer::New(static_cast<int32_t>(value), isolate);
  }
  ENTER_V8(internal_isolate);
  i::Handle<i::Object> result = internal_isolate->factory()->NewNumber(value);
  return Utils::IntegerToLocal(result);
}


#ifdef DEBUG
v8::AssertNoGCScope::AssertNoGCScope(v8::Isolate* isolate)
  : isolate_(isolate),
    last_state_(i::EnterAllocationScope(
        reinterpret_cast<i::Isolate*>(isolate), false)) {
}

v8::AssertNoGCScope::~AssertNoGCScope() {
  i::ExitAllocationScope(reinterpret_cast<i::Isolate*>(isolate_), last_state_);
}
#endif


void V8::IgnoreOutOfMemoryException() {
  EnterIsolateIfNeeded()->set_ignore_out_of_memory(true);
}


bool V8::AddMessageListener(MessageCallback that, Handle<Value> data) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::V8::AddMessageListener()");
  ON_BAILOUT(isolate, "v8::V8::AddMessageListener()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  NeanderArray listeners(isolate->factory()->message_listeners());
  NeanderObject obj(2);
  obj.set(0, *isolate->factory()->NewForeign(FUNCTION_ADDR(that)));
  obj.set(1, data.IsEmpty() ? isolate->heap()->undefined_value()
                            : *Utils::OpenHandle(*data));
  listeners.add(obj.value());
  return true;
}


void V8::RemoveMessageListeners(MessageCallback that) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::V8::RemoveMessageListener()");
  ON_BAILOUT(isolate, "v8::V8::RemoveMessageListeners()", return);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  NeanderArray listeners(isolate->factory()->message_listeners());
  for (int i = 0; i < listeners.length(); i++) {
    if (listeners.get(i)->IsUndefined()) continue;  // skip deleted ones

    NeanderObject listener(i::JSObject::cast(listeners.get(i)));
    i::Handle<i::Foreign> callback_obj(i::Foreign::cast(listener.get(0)));
    if (callback_obj->foreign_address() == FUNCTION_ADDR(that)) {
      listeners.set(i, isolate->heap()->undefined_value());
    }
  }
}


void V8::SetCaptureStackTraceForUncaughtExceptions(
      bool capture,
      int frame_limit,
      StackTrace::StackTraceOptions options) {
  i::Isolate::Current()->SetCaptureStackTraceForUncaughtExceptions(
      capture,
      frame_limit,
      options);
}


void V8::SetCounterFunction(CounterLookupCallback callback) {
  i::Isolate* isolate = EnterIsolateIfNeeded();
  if (IsDeadCheck(isolate, "v8::V8::SetCounterFunction()")) return;
  isolate->stats_table()->SetCounterFunction(callback);
}

void V8::SetCreateHistogramFunction(CreateHistogramCallback callback) {
  i::Isolate* isolate = EnterIsolateIfNeeded();
  if (IsDeadCheck(isolate, "v8::V8::SetCreateHistogramFunction()")) return;
  isolate->stats_table()->SetCreateHistogramFunction(callback);
  isolate->InitializeLoggingAndCounters();
  isolate->counters()->ResetHistograms();
}

void V8::SetAddHistogramSampleFunction(AddHistogramSampleCallback callback) {
  i::Isolate* isolate = EnterIsolateIfNeeded();
  if (IsDeadCheck(isolate, "v8::V8::SetAddHistogramSampleFunction()")) return;
  isolate->stats_table()->
      SetAddHistogramSampleFunction(callback);
}

void V8::SetFailedAccessCheckCallbackFunction(
      FailedAccessCheckCallback callback) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::SetFailedAccessCheckCallbackFunction()")) {
    return;
  }
  isolate->SetFailedAccessCheckCallback(callback);
}


void V8::AddObjectGroup(Persistent<Value>* objects,
                        size_t length,
                        RetainedObjectInfo* info) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::AddObjectGroup()")) return;
  STATIC_ASSERT(sizeof(Persistent<Value>) == sizeof(i::Object**));
  isolate->global_handles()->AddObjectGroup(
      reinterpret_cast<i::Object***>(objects), length, info);
}


void V8::AddObjectGroup(Isolate* exported_isolate,
                        Persistent<Value>* objects,
                        size_t length,
                        RetainedObjectInfo* info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(exported_isolate);
  ASSERT(isolate == i::Isolate::Current());
  if (IsDeadCheck(isolate, "v8::V8::AddObjectGroup()")) return;
  STATIC_ASSERT(sizeof(Persistent<Value>) == sizeof(i::Object**));
  isolate->global_handles()->AddObjectGroup(
      reinterpret_cast<i::Object***>(objects), length, info);
}


void V8::AddImplicitReferences(Persistent<Object> parent,
                               Persistent<Value>* children,
                               size_t length) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::AddImplicitReferences()")) return;
  STATIC_ASSERT(sizeof(Persistent<Value>) == sizeof(i::Object**));
  isolate->global_handles()->AddImplicitReferences(
      i::Handle<i::HeapObject>::cast(Utils::OpenHandle(*parent)).location(),
      reinterpret_cast<i::Object***>(children), length);
}


intptr_t Isolate::AdjustAmountOfExternalAllocatedMemory(
    intptr_t change_in_bytes) {
  i::Heap* heap = reinterpret_cast<i::Isolate*>(this)->heap();
  return heap->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);
}


intptr_t V8::AdjustAmountOfExternalAllocatedMemory(intptr_t change_in_bytes) {
  i::Isolate* isolate = i::Isolate::UncheckedCurrent();
  if (isolate == NULL || !isolate->IsInitialized() ||
      IsDeadCheck(isolate, "v8::V8::AdjustAmountOfExternalAllocatedMemory()")) {
    return 0;
  }
  Isolate* isolate_ext = reinterpret_cast<Isolate*>(isolate);
  return isolate_ext->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);
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


v8::Local<v8::Context> Isolate::GetCurrentContext() {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::Object> current = internal_isolate->native_context();
  if (current.is_null()) return Local<Context>();
  i::Handle<i::Context> context = i::Handle<i::Context>::cast(current);
  return Utils::ToLocal(context);
}


void Isolate::SetObjectGroupId(const Persistent<Value>& object,
                               UniqueId id) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(this);
  internal_isolate->global_handles()->SetObjectGroupId(
      reinterpret_cast<i::Object**>(*object), id);
}


void Isolate::SetReferenceFromGroup(UniqueId id,
                                    const Persistent<Value>& object) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(this);
  internal_isolate->global_handles()
      ->SetReferenceFromGroup(id, reinterpret_cast<i::Object**>(*object));
}


void Isolate::SetReference(const Persistent<Object>& parent,
                           const Persistent<Value>& child) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(this);
  internal_isolate->global_handles()->SetReference(
      i::Handle<i::HeapObject>::cast(Utils::OpenHandle(*parent)).location(),
      reinterpret_cast<i::Object**>(*child));
}


void V8::SetGlobalGCPrologueCallback(GCCallback callback) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::SetGlobalGCPrologueCallback()")) return;
  isolate->heap()->SetGlobalGCPrologueCallback(callback);
}


void V8::SetGlobalGCEpilogueCallback(GCCallback callback) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::SetGlobalGCEpilogueCallback()")) return;
  isolate->heap()->SetGlobalGCEpilogueCallback(callback);
}


void V8::AddGCPrologueCallback(GCPrologueCallback callback, GCType gc_type) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::AddGCPrologueCallback()")) return;
  isolate->heap()->AddGCPrologueCallback(callback, gc_type);
}


void V8::RemoveGCPrologueCallback(GCPrologueCallback callback) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::RemoveGCPrologueCallback()")) return;
  isolate->heap()->RemoveGCPrologueCallback(callback);
}


void V8::AddGCEpilogueCallback(GCEpilogueCallback callback, GCType gc_type) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::AddGCEpilogueCallback()")) return;
  isolate->heap()->AddGCEpilogueCallback(callback, gc_type);
}


void V8::RemoveGCEpilogueCallback(GCEpilogueCallback callback) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::RemoveGCEpilogueCallback()")) return;
  isolate->heap()->RemoveGCEpilogueCallback(callback);
}


void V8::AddMemoryAllocationCallback(MemoryAllocationCallback callback,
                                     ObjectSpace space,
                                     AllocationAction action) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::AddMemoryAllocationCallback()")) return;
  isolate->memory_allocator()->AddMemoryAllocationCallback(
      callback, space, action);
}


void V8::RemoveMemoryAllocationCallback(MemoryAllocationCallback callback) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::RemoveMemoryAllocationCallback()")) return;
  isolate->memory_allocator()->RemoveMemoryAllocationCallback(
      callback);
}


void V8::AddCallCompletedCallback(CallCompletedCallback callback) {
  if (callback == NULL) return;
  i::Isolate::EnsureDefaultIsolate();
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::AddLeaveScriptCallback()")) return;
  i::V8::AddCallCompletedCallback(callback);
}


void V8::RemoveCallCompletedCallback(CallCompletedCallback callback) {
  i::Isolate::EnsureDefaultIsolate();
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::RemoveLeaveScriptCallback()")) return;
  i::V8::RemoveCallCompletedCallback(callback);
}


void V8::PauseProfiler() {
  i::Isolate* isolate = i::Isolate::Current();
  isolate->logger()->PauseProfiler();
}


void V8::ResumeProfiler() {
  i::Isolate* isolate = i::Isolate::Current();
  isolate->logger()->ResumeProfiler();
}


bool V8::IsProfilerPaused() {
  i::Isolate* isolate = i::Isolate::Current();
  return isolate->logger()->IsProfilerPaused();
}


int V8::GetCurrentThreadId() {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "V8::GetCurrentThreadId()");
  return isolate->thread_id().ToInteger();
}


void V8::TerminateExecution(int thread_id) {
  i::Isolate* isolate = i::Isolate::Current();
  if (!isolate->IsInitialized()) return;
  API_ENTRY_CHECK(isolate, "V8::TerminateExecution()");
  // If the thread_id identifies the current thread just terminate
  // execution right away.  Otherwise, ask the thread manager to
  // terminate the thread with the given id if any.
  i::ThreadId internal_tid = i::ThreadId::FromInteger(thread_id);
  if (isolate->thread_id().Equals(internal_tid)) {
    isolate->stack_guard()->TerminateExecution();
  } else {
    isolate->thread_manager()->TerminateExecution(internal_tid);
  }
}


void V8::TerminateExecution(Isolate* isolate) {
  // If no isolate is supplied, use the default isolate.
  if (isolate != NULL) {
    reinterpret_cast<i::Isolate*>(isolate)->stack_guard()->TerminateExecution();
  } else {
    i::Isolate::GetDefaultIsolateStackGuard()->TerminateExecution();
  }
}


bool V8::IsExecutionTerminating(Isolate* isolate) {
  i::Isolate* i_isolate = isolate != NULL ?
      reinterpret_cast<i::Isolate*>(isolate) : i::Isolate::Current();
  return IsExecutionTerminatingCheck(i_isolate);
}


void V8::CancelTerminateExecution(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->stack_guard()->CancelTerminateExecution();
}


Isolate* Isolate::GetCurrent() {
  i::Isolate* isolate = i::Isolate::UncheckedCurrent();
  return reinterpret_cast<Isolate*>(isolate);
}


Isolate* Isolate::New() {
  i::Isolate* isolate = new i::Isolate();
  return reinterpret_cast<Isolate*>(isolate);
}


void Isolate::Dispose() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  if (!ApiCheck(!isolate->IsInUse(),
                "v8::Isolate::Dispose()",
                "Disposing the isolate that is entered by a thread.")) {
    return;
  }
  isolate->TearDown();
}


void Isolate::Enter() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->Enter();
}


void Isolate::Exit() {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  isolate->Exit();
}


void Isolate::GetHeapStatistics(HeapStatistics* heap_statistics) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(this);
  if (!isolate->IsInitialized()) {
    heap_statistics->total_heap_size_ = 0;
    heap_statistics->total_heap_size_executable_ = 0;
    heap_statistics->total_physical_size_ = 0;
    heap_statistics->used_heap_size_ = 0;
    heap_statistics->heap_size_limit_ = 0;
    return;
  }
  i::Heap* heap = isolate->heap();
  heap_statistics->total_heap_size_ = heap->CommittedMemory();
  heap_statistics->total_heap_size_executable_ =
      heap->CommittedMemoryExecutable();
  heap_statistics->total_physical_size_ = heap->CommittedPhysicalMemory();
  heap_statistics->used_heap_size_ = heap->SizeOfObjects();
  heap_statistics->heap_size_limit_ = heap->MaxReserved();
}


String::Utf8Value::Utf8Value(v8::Handle<v8::Value> obj)
    : str_(NULL), length_(0) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::String::Utf8Value::Utf8Value()")) return;
  if (obj.IsEmpty()) return;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  TryCatch try_catch;
  Handle<String> str = obj->ToString();
  if (str.IsEmpty()) return;
  i::Handle<i::String> i_str = Utils::OpenHandle(*str);
  length_ = v8::Utf8Length(*i_str, isolate);
  str_ = i::NewArray<char>(length_ + 1);
  str->WriteUtf8(str_);
}


String::Utf8Value::~Utf8Value() {
  i::DeleteArray(str_);
}


String::AsciiValue::AsciiValue(v8::Handle<v8::Value> obj)
    : str_(NULL), length_(0) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::String::AsciiValue::AsciiValue()")) return;
  if (obj.IsEmpty()) return;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  TryCatch try_catch;
  Handle<String> str = obj->ToString();
  if (str.IsEmpty()) return;
  length_ = str->Utf8Length();
  str_ = i::NewArray<char>(length_ + 1);
  str->WriteUtf8(str_);
  ASSERT(i::String::NonAsciiStart(str_, length_) >= length_);
}


String::AsciiValue::~AsciiValue() {
  i::DeleteArray(str_);
}


String::Value::Value(v8::Handle<v8::Value> obj)
    : str_(NULL), length_(0) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::String::Value::Value()")) return;
  if (obj.IsEmpty()) return;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  TryCatch try_catch;
  Handle<String> str = obj->ToString();
  if (str.IsEmpty()) return;
  length_ = str->Length();
  str_ = i::NewArray<uint16_t>(length_ + 1);
  str->Write(str_);
}


String::Value::~Value() {
  i::DeleteArray(str_);
}

Local<Value> Exception::RangeError(v8::Handle<v8::String> raw_message) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "RangeError");
  ON_BAILOUT(isolate, "v8::Exception::RangeError()", return Local<Value>());
  ENTER_V8(isolate);
  i::Object* error;
  {
    i::HandleScope scope(isolate);
    i::Handle<i::String> message = Utils::OpenHandle(*raw_message);
    i::Handle<i::Object> result = isolate->factory()->NewRangeError(message);
    error = *result;
  }
  i::Handle<i::Object> result(error, isolate);
  return Utils::ToLocal(result);
}

Local<Value> Exception::ReferenceError(v8::Handle<v8::String> raw_message) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "ReferenceError");
  ON_BAILOUT(isolate, "v8::Exception::ReferenceError()", return Local<Value>());
  ENTER_V8(isolate);
  i::Object* error;
  {
    i::HandleScope scope(isolate);
    i::Handle<i::String> message = Utils::OpenHandle(*raw_message);
    i::Handle<i::Object> result =
        isolate->factory()->NewReferenceError(message);
    error = *result;
  }
  i::Handle<i::Object> result(error, isolate);
  return Utils::ToLocal(result);
}

Local<Value> Exception::SyntaxError(v8::Handle<v8::String> raw_message) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "SyntaxError");
  ON_BAILOUT(isolate, "v8::Exception::SyntaxError()", return Local<Value>());
  ENTER_V8(isolate);
  i::Object* error;
  {
    i::HandleScope scope(isolate);
    i::Handle<i::String> message = Utils::OpenHandle(*raw_message);
    i::Handle<i::Object> result = isolate->factory()->NewSyntaxError(message);
    error = *result;
  }
  i::Handle<i::Object> result(error, isolate);
  return Utils::ToLocal(result);
}

Local<Value> Exception::TypeError(v8::Handle<v8::String> raw_message) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "TypeError");
  ON_BAILOUT(isolate, "v8::Exception::TypeError()", return Local<Value>());
  ENTER_V8(isolate);
  i::Object* error;
  {
    i::HandleScope scope(isolate);
    i::Handle<i::String> message = Utils::OpenHandle(*raw_message);
    i::Handle<i::Object> result = isolate->factory()->NewTypeError(message);
    error = *result;
  }
  i::Handle<i::Object> result(error, isolate);
  return Utils::ToLocal(result);
}

Local<Value> Exception::Error(v8::Handle<v8::String> raw_message) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "Error");
  ON_BAILOUT(isolate, "v8::Exception::Error()", return Local<Value>());
  ENTER_V8(isolate);
  i::Object* error;
  {
    i::HandleScope scope(isolate);
    i::Handle<i::String> message = Utils::OpenHandle(*raw_message);
    i::Handle<i::Object> result = isolate->factory()->NewError(message);
    error = *result;
  }
  i::Handle<i::Object> result(error, isolate);
  return Utils::ToLocal(result);
}


// --- D e b u g   S u p p o r t ---

#ifdef ENABLE_DEBUGGER_SUPPORT

static void EventCallbackWrapper(const v8::Debug::EventDetails& event_details) {
  i::Isolate* isolate = i::Isolate::Current();
  if (isolate->debug_event_callback() != NULL) {
    isolate->debug_event_callback()(event_details.GetEvent(),
                                    event_details.GetExecutionState(),
                                    event_details.GetEventData(),
                                    event_details.GetCallbackData());
  }
}


bool Debug::SetDebugEventListener(EventCallback that, Handle<Value> data) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Debug::SetDebugEventListener()");
  ON_BAILOUT(isolate, "v8::Debug::SetDebugEventListener()", return false);
  ENTER_V8(isolate);

  isolate->set_debug_event_callback(that);

  i::HandleScope scope(isolate);
  i::Handle<i::Object> foreign = isolate->factory()->undefined_value();
  if (that != NULL) {
    foreign =
        isolate->factory()->NewForeign(FUNCTION_ADDR(EventCallbackWrapper));
  }
  isolate->debugger()->SetEventListener(foreign,
                                        Utils::OpenHandle(*data, true));
  return true;
}


bool Debug::SetDebugEventListener2(EventCallback2 that, Handle<Value> data) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Debug::SetDebugEventListener2()");
  ON_BAILOUT(isolate, "v8::Debug::SetDebugEventListener2()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::Object> foreign = isolate->factory()->undefined_value();
  if (that != NULL) {
    foreign = isolate->factory()->NewForeign(FUNCTION_ADDR(that));
  }
  isolate->debugger()->SetEventListener(foreign,
                                        Utils::OpenHandle(*data, true));
  return true;
}


bool Debug::SetDebugEventListener(v8::Handle<v8::Object> that,
                                  Handle<Value> data) {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Debug::SetDebugEventListener()", return false);
  ENTER_V8(isolate);
  isolate->debugger()->SetEventListener(Utils::OpenHandle(*that),
                                        Utils::OpenHandle(*data, true));
  return true;
}


void Debug::DebugBreak(Isolate* isolate) {
  // If no isolate is supplied, use the default isolate.
  if (isolate != NULL) {
    reinterpret_cast<i::Isolate*>(isolate)->stack_guard()->DebugBreak();
  } else {
    i::Isolate::GetDefaultIsolateStackGuard()->DebugBreak();
  }
}


void Debug::CancelDebugBreak(Isolate* isolate) {
  // If no isolate is supplied, use the default isolate.
  if (isolate != NULL) {
    i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
    internal_isolate->stack_guard()->Continue(i::DEBUGBREAK);
  } else {
    i::Isolate::GetDefaultIsolateStackGuard()->Continue(i::DEBUGBREAK);
  }
}


void Debug::DebugBreakForCommand(ClientData* data, Isolate* isolate) {
  // If no isolate is supplied, use the default isolate.
  if (isolate != NULL) {
    i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
    internal_isolate->debugger()->EnqueueDebugCommand(data);
  } else {
    i::Isolate::GetDefaultIsolateDebugger()->EnqueueDebugCommand(data);
  }
}


static void MessageHandlerWrapper(const v8::Debug::Message& message) {
  i::Isolate* isolate = i::Isolate::Current();
  if (isolate->message_handler()) {
    v8::String::Value json(message.GetJSON());
    (isolate->message_handler())(*json, json.length(), message.GetClientData());
  }
}


void Debug::SetMessageHandler(v8::Debug::MessageHandler handler,
                              bool message_handler_thread) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Debug::SetMessageHandler");
  ENTER_V8(isolate);

  // Message handler thread not supported any more. Parameter temporally left in
  // the API for client compatibility reasons.
  CHECK(!message_handler_thread);

  // TODO(sgjesse) support the old message handler API through a simple wrapper.
  isolate->set_message_handler(handler);
  if (handler != NULL) {
    isolate->debugger()->SetMessageHandler(MessageHandlerWrapper);
  } else {
    isolate->debugger()->SetMessageHandler(NULL);
  }
}


void Debug::SetMessageHandler2(v8::Debug::MessageHandler2 handler) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Debug::SetMessageHandler");
  ENTER_V8(isolate);
  isolate->debugger()->SetMessageHandler(handler);
}


void Debug::SendCommand(const uint16_t* command, int length,
                        ClientData* client_data,
                        Isolate* isolate) {
  // If no isolate is supplied, use the default isolate.
  if (isolate != NULL) {
    i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
    internal_isolate->debugger()->ProcessCommand(
        i::Vector<const uint16_t>(command, length), client_data);
  } else {
    i::Isolate::GetDefaultIsolateDebugger()->ProcessCommand(
        i::Vector<const uint16_t>(command, length), client_data);
  }
}


void Debug::SetHostDispatchHandler(HostDispatchHandler handler,
                                   int period) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Debug::SetHostDispatchHandler");
  ENTER_V8(isolate);
  isolate->debugger()->SetHostDispatchHandler(handler, period);
}


void Debug::SetDebugMessageDispatchHandler(
    DebugMessageDispatchHandler handler, bool provide_locker) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate,
                              "v8::Debug::SetDebugMessageDispatchHandler");
  ENTER_V8(isolate);
  isolate->debugger()->SetDebugMessageDispatchHandler(
      handler, provide_locker);
}


Local<Value> Debug::Call(v8::Handle<v8::Function> fun,
                         v8::Handle<v8::Value> data) {
  i::Isolate* isolate = i::Isolate::Current();
  if (!isolate->IsInitialized()) return Local<Value>();
  ON_BAILOUT(isolate, "v8::Debug::Call()", return Local<Value>());
  ENTER_V8(isolate);
  i::Handle<i::Object> result;
  EXCEPTION_PREAMBLE(isolate);
  if (data.IsEmpty()) {
    result = isolate->debugger()->Call(Utils::OpenHandle(*fun),
                                       isolate->factory()->undefined_value(),
                                       &has_pending_exception);
  } else {
    result = isolate->debugger()->Call(Utils::OpenHandle(*fun),
                                       Utils::OpenHandle(*data),
                                       &has_pending_exception);
  }
  EXCEPTION_BAILOUT_CHECK(isolate, Local<Value>());
  return Utils::ToLocal(result);
}


Local<Value> Debug::GetMirror(v8::Handle<v8::Value> obj) {
  i::Isolate* isolate = i::Isolate::Current();
  if (!isolate->IsInitialized()) return Local<Value>();
  ON_BAILOUT(isolate, "v8::Debug::GetMirror()", return Local<Value>());
  ENTER_V8(isolate);
  v8::HandleScope scope(reinterpret_cast<Isolate*>(isolate));
  i::Debug* isolate_debug = isolate->debug();
  isolate_debug->Load();
  i::Handle<i::JSObject> debug(isolate_debug->debug_context()->global_object());
  i::Handle<i::String> name = isolate->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("MakeMirror"));
  i::Handle<i::Object> fun_obj = i::GetProperty(isolate, debug, name);
  i::Handle<i::JSFunction> fun = i::Handle<i::JSFunction>::cast(fun_obj);
  v8::Handle<v8::Function> v8_fun = Utils::ToLocal(fun);
  const int kArgc = 1;
  v8::Handle<v8::Value> argv[kArgc] = { obj };
  EXCEPTION_PREAMBLE(isolate);
  v8::Handle<v8::Value> result = v8_fun->Call(Utils::ToLocal(debug),
                                              kArgc,
                                              argv);
  EXCEPTION_BAILOUT_CHECK(isolate, Local<Value>());
  return scope.Close(result);
}


bool Debug::EnableAgent(const char* name, int port, bool wait_for_connection) {
  return i::Isolate::Current()->debugger()->StartAgent(name, port,
                                                       wait_for_connection);
}


void Debug::DisableAgent() {
  return i::Isolate::Current()->debugger()->StopAgent();
}


void Debug::ProcessDebugMessages() {
  i::Execution::ProcessDebugMessages(true);
}


Local<Context> Debug::GetDebugContext() {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Debug::GetDebugContext()");
  ENTER_V8(isolate);
  return Utils::ToLocal(i::Isolate::Current()->debugger()->GetDebugContext());
}


void Debug::SetLiveEditEnabled(bool enable, Isolate* isolate) {
  // If no isolate is supplied, use the default isolate.
  i::Debugger* debugger;
  if (isolate != NULL) {
    i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
    debugger = internal_isolate->debugger();
  } else {
    debugger = i::Isolate::GetDefaultIsolateDebugger();
  }
  debugger->set_live_edit_enabled(enable);
}


#endif  // ENABLE_DEBUGGER_SUPPORT


Handle<String> CpuProfileNode::GetFunctionName() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetFunctionName");
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  const i::CodeEntry* entry = node->entry();
  if (!entry->has_name_prefix()) {
    return Handle<String>(ToApi<String>(
        isolate->factory()->InternalizeUtf8String(entry->name())));
  } else {
    return Handle<String>(ToApi<String>(isolate->factory()->NewConsString(
        isolate->factory()->InternalizeUtf8String(entry->name_prefix()),
        isolate->factory()->InternalizeUtf8String(entry->name()))));
  }
}


Handle<String> CpuProfileNode::GetScriptResourceName() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetScriptResourceName");
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return Handle<String>(ToApi<String>(isolate->factory()->InternalizeUtf8String(
      node->entry()->resource_name())));
}


int CpuProfileNode::GetLineNumber() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetLineNumber");
  return reinterpret_cast<const i::ProfileNode*>(this)->entry()->line_number();
}


double CpuProfileNode::GetTotalTime() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetTotalTime");
  return reinterpret_cast<const i::ProfileNode*>(this)->GetTotalMillis();
}


double CpuProfileNode::GetSelfTime() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetSelfTime");
  return reinterpret_cast<const i::ProfileNode*>(this)->GetSelfMillis();
}


double CpuProfileNode::GetTotalSamplesCount() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetTotalSamplesCount");
  return reinterpret_cast<const i::ProfileNode*>(this)->total_ticks();
}


double CpuProfileNode::GetSelfSamplesCount() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetSelfSamplesCount");
  return reinterpret_cast<const i::ProfileNode*>(this)->self_ticks();
}


unsigned CpuProfileNode::GetCallUid() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetCallUid");
  return reinterpret_cast<const i::ProfileNode*>(this)->entry()->GetCallUid();
}


unsigned CpuProfileNode::GetNodeId() const {
  return reinterpret_cast<const i::ProfileNode*>(this)->id();
}


int CpuProfileNode::GetChildrenCount() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetChildrenCount");
  return reinterpret_cast<const i::ProfileNode*>(this)->children()->length();
}


const CpuProfileNode* CpuProfileNode::GetChild(int index) const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetChild");
  const i::ProfileNode* child =
      reinterpret_cast<const i::ProfileNode*>(this)->children()->at(index);
  return reinterpret_cast<const CpuProfileNode*>(child);
}


void CpuProfile::Delete() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfile::Delete");
  i::CpuProfiler* profiler = isolate->cpu_profiler();
  ASSERT(profiler != NULL);
  profiler->DeleteProfile(reinterpret_cast<i::CpuProfile*>(this));
  if (profiler->GetProfilesCount() == 0 && !profiler->HasDetachedProfiles()) {
    // If this was the last profile, clean up all accessory data as well.
    profiler->DeleteAllProfiles();
  }
}


unsigned CpuProfile::GetUid() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfile::GetUid");
  return reinterpret_cast<const i::CpuProfile*>(this)->uid();
}


Handle<String> CpuProfile::GetTitle() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfile::GetTitle");
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return Handle<String>(ToApi<String>(isolate->factory()->InternalizeUtf8String(
      profile->title())));
}


const CpuProfileNode* CpuProfile::GetTopDownRoot() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfile::GetTopDownRoot");
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->top_down()->root());
}


const CpuProfileNode* CpuProfile::GetSample(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->sample(index));
}


int CpuProfile::GetSamplesCount() const {
  return reinterpret_cast<const i::CpuProfile*>(this)->samples_count();
}


int CpuProfiler::GetProfilesCount() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::GetProfilesCount");
  i::CpuProfiler* profiler = isolate->cpu_profiler();
  ASSERT(profiler != NULL);
  return profiler->GetProfilesCount();
}


int CpuProfiler::GetProfileCount() {
  return reinterpret_cast<i::CpuProfiler*>(this)->GetProfilesCount();
}


const CpuProfile* CpuProfiler::GetProfile(int index,
                                          Handle<Value> security_token) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::GetProfile");
  i::CpuProfiler* profiler = isolate->cpu_profiler();
  ASSERT(profiler != NULL);
  return reinterpret_cast<const CpuProfile*>(
      profiler->GetProfile(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          index));
}


const CpuProfile* CpuProfiler::GetCpuProfile(int index,
                                             Handle<Value> security_token) {
  return reinterpret_cast<const CpuProfile*>(
      reinterpret_cast<i::CpuProfiler*>(this)->GetProfile(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          index));
}


const CpuProfile* CpuProfiler::FindProfile(unsigned uid,
                                           Handle<Value> security_token) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::FindProfile");
  i::CpuProfiler* profiler = isolate->cpu_profiler();
  ASSERT(profiler != NULL);
  return reinterpret_cast<const CpuProfile*>(
      profiler->FindProfile(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          uid));
}


const CpuProfile* CpuProfiler::FindCpuProfile(unsigned uid,
                                              Handle<Value> security_token) {
  return reinterpret_cast<const CpuProfile*>(
      reinterpret_cast<i::CpuProfiler*>(this)->FindProfile(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          uid));
}


void CpuProfiler::StartProfiling(Handle<String> title, bool record_samples) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::StartProfiling");
  i::CpuProfiler* profiler = isolate->cpu_profiler();
  ASSERT(profiler != NULL);
  profiler->StartProfiling(*Utils::OpenHandle(*title), record_samples);
}


void CpuProfiler::StartCpuProfiling(Handle<String> title, bool record_samples) {
  reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      *Utils::OpenHandle(*title), record_samples);
}


const CpuProfile* CpuProfiler::StopProfiling(Handle<String> title,
                                             Handle<Value> security_token) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::StopProfiling");
  i::CpuProfiler* profiler = isolate->cpu_profiler();
  ASSERT(profiler != NULL);
  return reinterpret_cast<const CpuProfile*>(
      profiler->StopProfiling(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          *Utils::OpenHandle(*title)));
}


const CpuProfile* CpuProfiler::StopCpuProfiling(Handle<String> title,
                                                Handle<Value> security_token) {
  return reinterpret_cast<const CpuProfile*>(
      reinterpret_cast<i::CpuProfiler*>(this)->StopProfiling(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          *Utils::OpenHandle(*title)));
}


void CpuProfiler::DeleteAllProfiles() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::DeleteAllProfiles");
  i::CpuProfiler* profiler = isolate->cpu_profiler();
  ASSERT(profiler != NULL);
  profiler->DeleteAllProfiles();
}


void CpuProfiler::DeleteAllCpuProfiles() {
  reinterpret_cast<i::CpuProfiler*>(this)->DeleteAllProfiles();
}


static i::HeapGraphEdge* ToInternal(const HeapGraphEdge* edge) {
  return const_cast<i::HeapGraphEdge*>(
      reinterpret_cast<const i::HeapGraphEdge*>(edge));
}


HeapGraphEdge::Type HeapGraphEdge::GetType() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphEdge::GetType");
  return static_cast<HeapGraphEdge::Type>(ToInternal(this)->type());
}


Handle<Value> HeapGraphEdge::GetName() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphEdge::GetName");
  i::HeapGraphEdge* edge = ToInternal(this);
  switch (edge->type()) {
    case i::HeapGraphEdge::kContextVariable:
    case i::HeapGraphEdge::kInternal:
    case i::HeapGraphEdge::kProperty:
    case i::HeapGraphEdge::kShortcut:
      return Handle<String>(ToApi<String>(
          isolate->factory()->InternalizeUtf8String(edge->name())));
    case i::HeapGraphEdge::kElement:
    case i::HeapGraphEdge::kHidden:
      return Handle<Number>(ToApi<Number>(
          isolate->factory()->NewNumberFromInt(edge->index())));
    default: UNREACHABLE();
  }
  return v8::Undefined();
}


const HeapGraphNode* HeapGraphEdge::GetFromNode() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphEdge::GetFromNode");
  const i::HeapEntry* from = ToInternal(this)->from();
  return reinterpret_cast<const HeapGraphNode*>(from);
}


const HeapGraphNode* HeapGraphEdge::GetToNode() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphEdge::GetToNode");
  const i::HeapEntry* to = ToInternal(this)->to();
  return reinterpret_cast<const HeapGraphNode*>(to);
}


static i::HeapEntry* ToInternal(const HeapGraphNode* entry) {
  return const_cast<i::HeapEntry*>(
      reinterpret_cast<const i::HeapEntry*>(entry));
}


HeapGraphNode::Type HeapGraphNode::GetType() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphNode::GetType");
  return static_cast<HeapGraphNode::Type>(ToInternal(this)->type());
}


Handle<String> HeapGraphNode::GetName() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphNode::GetName");
  return Handle<String>(ToApi<String>(isolate->factory()->InternalizeUtf8String(
      ToInternal(this)->name())));
}


SnapshotObjectId HeapGraphNode::GetId() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphNode::GetId");
  return ToInternal(this)->id();
}


int HeapGraphNode::GetSelfSize() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphNode::GetSelfSize");
  return ToInternal(this)->self_size();
}


int HeapGraphNode::GetChildrenCount() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetChildrenCount");
  return ToInternal(this)->children().length();
}


const HeapGraphEdge* HeapGraphNode::GetChild(int index) const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetChild");
  return reinterpret_cast<const HeapGraphEdge*>(
      ToInternal(this)->children()[index]);
}


v8::Handle<v8::Value> HeapGraphNode::GetHeapValue() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapGraphNode::GetHeapValue");
  i::Handle<i::HeapObject> object = ToInternal(this)->GetHeapObject();
  return v8::Handle<Value>(!object.is_null() ?
                           ToApi<Value>(object) : ToApi<Value>(
                               isolate->factory()->undefined_value()));
}


static i::HeapSnapshot* ToInternal(const HeapSnapshot* snapshot) {
  return const_cast<i::HeapSnapshot*>(
      reinterpret_cast<const i::HeapSnapshot*>(snapshot));
}


void HeapSnapshot::Delete() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::Delete");
  if (isolate->heap_profiler()->GetSnapshotsCount() > 1) {
    ToInternal(this)->Delete();
  } else {
    // If this is the last snapshot, clean up all accessory data as well.
    isolate->heap_profiler()->DeleteAllSnapshots();
  }
}


HeapSnapshot::Type HeapSnapshot::GetType() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetType");
  return kFull;
}


unsigned HeapSnapshot::GetUid() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetUid");
  return ToInternal(this)->uid();
}


Handle<String> HeapSnapshot::GetTitle() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetTitle");
  return Handle<String>(ToApi<String>(isolate->factory()->InternalizeUtf8String(
      ToInternal(this)->title())));
}


const HeapGraphNode* HeapSnapshot::GetRoot() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetHead");
  return reinterpret_cast<const HeapGraphNode*>(ToInternal(this)->root());
}


const HeapGraphNode* HeapSnapshot::GetNodeById(SnapshotObjectId id) const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetNodeById");
  return reinterpret_cast<const HeapGraphNode*>(
      ToInternal(this)->GetEntryById(id));
}


int HeapSnapshot::GetNodesCount() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetNodesCount");
  return ToInternal(this)->entries().length();
}


const HeapGraphNode* HeapSnapshot::GetNode(int index) const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetNode");
  return reinterpret_cast<const HeapGraphNode*>(
      &ToInternal(this)->entries().at(index));
}


SnapshotObjectId HeapSnapshot::GetMaxSnapshotJSObjectId() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetMaxSnapshotJSObjectId");
  return ToInternal(this)->max_snapshot_js_object_id();
}


void HeapSnapshot::Serialize(OutputStream* stream,
                             HeapSnapshot::SerializationFormat format) const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::Serialize");
  ApiCheck(format == kJSON,
           "v8::HeapSnapshot::Serialize",
           "Unknown serialization format");
  ApiCheck(stream->GetOutputEncoding() == OutputStream::kAscii,
           "v8::HeapSnapshot::Serialize",
           "Unsupported output encoding");
  ApiCheck(stream->GetChunkSize() > 0,
           "v8::HeapSnapshot::Serialize",
           "Invalid stream chunk size");
  i::HeapSnapshotJSONSerializer serializer(ToInternal(this));
  serializer.Serialize(stream);
}


int HeapProfiler::GetSnapshotsCount() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::GetSnapshotsCount");
  return isolate->heap_profiler()->GetSnapshotsCount();
}


int HeapProfiler::GetSnapshotCount() {
  return reinterpret_cast<i::HeapProfiler*>(this)->GetSnapshotsCount();
}


const HeapSnapshot* HeapProfiler::GetSnapshot(int index) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::GetSnapshot");
  return reinterpret_cast<const HeapSnapshot*>(
      isolate->heap_profiler()->GetSnapshot(index));
}


const HeapSnapshot* HeapProfiler::GetHeapSnapshot(int index) {
  return reinterpret_cast<const HeapSnapshot*>(
      reinterpret_cast<i::HeapProfiler*>(this)->GetSnapshot(index));
}


const HeapSnapshot* HeapProfiler::FindSnapshot(unsigned uid) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::FindSnapshot");
  return reinterpret_cast<const HeapSnapshot*>(
      isolate->heap_profiler()->FindSnapshot(uid));
}


const HeapSnapshot* HeapProfiler::FindHeapSnapshot(unsigned uid) {
  return reinterpret_cast<const HeapSnapshot*>(
      reinterpret_cast<i::HeapProfiler*>(this)->FindSnapshot(uid));
}


SnapshotObjectId HeapProfiler::GetSnapshotObjectId(Handle<Value> value) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::GetSnapshotObjectId");
  i::Handle<i::Object> obj = Utils::OpenHandle(*value);
  return isolate->heap_profiler()->GetSnapshotObjectId(obj);
}


SnapshotObjectId HeapProfiler::GetObjectId(Handle<Value> value) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*value);
  return reinterpret_cast<i::HeapProfiler*>(this)->GetSnapshotObjectId(obj);
}


const HeapSnapshot* HeapProfiler::TakeSnapshot(Handle<String> title,
                                               HeapSnapshot::Type type,
                                               ActivityControl* control,
                                               ObjectNameResolver* resolver) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::TakeSnapshot");
  return reinterpret_cast<const HeapSnapshot*>(
      isolate->heap_profiler()->TakeSnapshot(
          *Utils::OpenHandle(*title), control, resolver));
}


const HeapSnapshot* HeapProfiler::TakeHeapSnapshot(
    Handle<String> title,
    ActivityControl* control,
    ObjectNameResolver* resolver) {
  return reinterpret_cast<const HeapSnapshot*>(
      reinterpret_cast<i::HeapProfiler*>(this)->TakeSnapshot(
          *Utils::OpenHandle(*title), control, resolver));
}


void HeapProfiler::StartHeapObjectsTracking() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::StartHeapObjectsTracking");
  isolate->heap_profiler()->StartHeapObjectsTracking();
}


void HeapProfiler::StartTrackingHeapObjects() {
  reinterpret_cast<i::HeapProfiler*>(this)->StartHeapObjectsTracking();
}


void HeapProfiler::StopHeapObjectsTracking() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::StopHeapObjectsTracking");
  isolate->heap_profiler()->StopHeapObjectsTracking();
}


void HeapProfiler::StopTrackingHeapObjects() {
  reinterpret_cast<i::HeapProfiler*>(this)->StopHeapObjectsTracking();
}


SnapshotObjectId HeapProfiler::PushHeapObjectsStats(OutputStream* stream) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::PushHeapObjectsStats");
  return isolate->heap_profiler()->PushHeapObjectsStats(stream);
}


SnapshotObjectId HeapProfiler::GetHeapStats(OutputStream* stream) {
  return reinterpret_cast<i::HeapProfiler*>(this)->PushHeapObjectsStats(stream);
}


void HeapProfiler::DeleteAllSnapshots() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::DeleteAllSnapshots");
  isolate->heap_profiler()->DeleteAllSnapshots();
}


void HeapProfiler::DeleteAllHeapSnapshots() {
  reinterpret_cast<i::HeapProfiler*>(this)->DeleteAllSnapshots();
}


void HeapProfiler::DefineWrapperClass(uint16_t class_id,
                                      WrapperInfoCallback callback) {
  i::Isolate::Current()->heap_profiler()->DefineWrapperClass(class_id,
                                                             callback);
}


void HeapProfiler::SetWrapperClassInfoProvider(uint16_t class_id,
                                               WrapperInfoCallback callback) {
  reinterpret_cast<i::HeapProfiler*>(this)->DefineWrapperClass(class_id,
                                                               callback);
}


int HeapProfiler::GetPersistentHandleCount() {
  i::Isolate* isolate = i::Isolate::Current();
  return isolate->global_handles()->NumberOfGlobalHandles();
}


size_t HeapProfiler::GetMemorySizeUsedByProfiler() {
  return i::Isolate::Current()->heap_profiler()->GetMemorySizeUsedByProfiler();
}


size_t HeapProfiler::GetProfilerMemorySize() {
  return reinterpret_cast<i::HeapProfiler*>(this)->
      GetMemorySizeUsedByProfiler();
}


void HeapProfiler::SetRetainedObjectInfo(UniqueId id,
                                         RetainedObjectInfo* info) {
  reinterpret_cast<i::HeapProfiler*>(this)->SetRetainedObjectInfo(id, info);
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
      "--max-inlined-source-size=999999 "
      "--max-inlined-nodes=999999 "
      "--max-inlined-nodes-cumulative=999999 "
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


// TODO(svenpanne) Deprecate this.
void Testing::DeoptimizeAll() {
  i::Isolate* isolate = i::Isolate::Current();
  i::HandleScope scope(isolate);
  internal::Deoptimizer::DeoptimizeAll(isolate);
}


namespace internal {


void HandleScopeImplementer::FreeThreadResources() {
  Free();
}


char* HandleScopeImplementer::ArchiveThread(char* storage) {
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate_->handle_scope_data();
  handle_scope_data_ = *current;
  OS::MemCopy(storage, this, sizeof(*this));

  ResetAfterArchive();
  current->Initialize();

  return storage + ArchiveSpacePerThread();
}


int HandleScopeImplementer::ArchiveSpacePerThread() {
  return sizeof(HandleScopeImplementer);
}


char* HandleScopeImplementer::RestoreThread(char* storage) {
  OS::MemCopy(this, storage, sizeof(*this));
  *isolate_->handle_scope_data() = handle_scope_data_;
  return storage + ArchiveSpacePerThread();
}


void HandleScopeImplementer::IterateThis(ObjectVisitor* v) {
#ifdef DEBUG
  bool found_block_before_deferred = false;
#endif
  // Iterate over all handles in the blocks except for the last.
  for (int i = blocks()->length() - 2; i >= 0; --i) {
    Object** block = blocks()->at(i);
    if (last_handle_before_deferred_block_ != NULL &&
        (last_handle_before_deferred_block_ <= &block[kHandleBlockSize]) &&
        (last_handle_before_deferred_block_ >= block)) {
      v->VisitPointers(block, last_handle_before_deferred_block_);
      ASSERT(!found_block_before_deferred);
#ifdef DEBUG
      found_block_before_deferred = true;
#endif
    } else {
      v->VisitPointers(block, &block[kHandleBlockSize]);
    }
  }

  ASSERT(last_handle_before_deferred_block_ == NULL ||
         found_block_before_deferred);

  // Iterate over live handles in the last block (if any).
  if (!blocks()->is_empty()) {
    v->VisitPointers(blocks()->last(), handle_scope_data_.next);
  }

  if (!saved_contexts_.is_empty()) {
    Object** start = reinterpret_cast<Object**>(&saved_contexts_.first());
    v->VisitPointers(start, start + saved_contexts_.length());
  }
}


void HandleScopeImplementer::Iterate(ObjectVisitor* v) {
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate_->handle_scope_data();
  handle_scope_data_ = *current;
  IterateThis(v);
}


char* HandleScopeImplementer::Iterate(ObjectVisitor* v, char* storage) {
  HandleScopeImplementer* scope_implementer =
      reinterpret_cast<HandleScopeImplementer*>(storage);
  scope_implementer->IterateThis(v);
  return storage + ArchiveSpacePerThread();
}


DeferredHandles* HandleScopeImplementer::Detach(Object** prev_limit) {
  DeferredHandles* deferred =
      new DeferredHandles(isolate()->handle_scope_data()->next, isolate());

  while (!blocks_.is_empty()) {
    Object** block_start = blocks_.last();
    Object** block_limit = &block_start[kHandleBlockSize];
    // We should not need to check for NoHandleAllocation here. Assert
    // this.
    ASSERT(prev_limit == block_limit ||
           !(block_start <= prev_limit && prev_limit <= block_limit));
    if (prev_limit == block_limit) break;
    deferred->blocks_.Add(blocks_.last());
    blocks_.RemoveLast();
  }

  // deferred->blocks_ now contains the blocks installed on the
  // HandleScope stack since BeginDeferredScope was called, but in
  // reverse order.

  ASSERT(prev_limit == NULL || !blocks_.is_empty());

  ASSERT(!blocks_.is_empty() && prev_limit != NULL);
  ASSERT(last_handle_before_deferred_block_ != NULL);
  last_handle_before_deferred_block_ = NULL;
  return deferred;
}


void HandleScopeImplementer::BeginDeferredScope() {
  ASSERT(last_handle_before_deferred_block_ == NULL);
  last_handle_before_deferred_block_ = isolate()->handle_scope_data()->next;
}


DeferredHandles::~DeferredHandles() {
  isolate_->UnlinkDeferredHandles(this);

  for (int i = 0; i < blocks_.length(); i++) {
#ifdef ENABLE_EXTRA_CHECKS
    HandleScope::ZapRange(blocks_[i], &blocks_[i][kHandleBlockSize]);
#endif
    isolate_->handle_scope_implementer()->ReturnBlock(blocks_[i]);
  }
}


void DeferredHandles::Iterate(ObjectVisitor* v) {
  ASSERT(!blocks_.is_empty());

  ASSERT((first_block_limit_ >= blocks_.first()) &&
         (first_block_limit_ <= &(blocks_.first())[kHandleBlockSize]));

  v->VisitPointers(blocks_.first(), first_block_limit_);

  for (int i = 1; i < blocks_.length(); i++) {
    v->VisitPointers(blocks_[i], &blocks_[i][kHandleBlockSize]);
  }
}


} }  // namespace v8::internal
