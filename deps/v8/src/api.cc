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

#include "api.h"

#include <math.h>  // For isnan.
#include <string.h>  // For memcpy, strlen.
#include "../include/v8-debug.h"
#include "../include/v8-profiler.h"
#include "../include/v8-testing.h"
#include "bootstrapper.h"
#include "compiler.h"
#include "conversions-inl.h"
#include "counters.h"
#include "debug.h"
#include "deoptimizer.h"
#include "execution.h"
#include "global-handles.h"
#include "heap-profiler.h"
#include "messages.h"
#ifdef COMPRESS_STARTUP_DATA_BZ2
#include "natives.h"
#endif
#include "parser.h"
#include "platform.h"
#include "profile-generator-inl.h"
#include "property-details.h"
#include "property.h"
#include "runtime-profiler.h"
#include "scanner-character-streams.h"
#include "snapshot.h"
#include "unicode-inl.h"
#include "v8threads.h"
#include "version.h"
#include "vm-state-inl.h"


#define LOG_API(isolate, expr) LOG(isolate, ApiEntryCall(expr))

#define ENTER_V8(isolate)                                        \
  ASSERT((isolate)->IsInitialized());                           \
  i::VMState __state__((isolate), i::OTHER)
#define LEAVE_V8(isolate) \
  i::VMState __state__((isolate), i::EXTERNAL)

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
  i::VMState __state__(i::Isolate::Current(), i::OTHER);
  API_Fatal(location, message);
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
  // BUG(1718):
  // Don't use the take_snapshot since we don't support HeapIterator here
  // without doing a special GC.
  isolate->heap()->RecordStats(&heap_stats, false);
  i::V8::SetFatalError();
  FatalErrorCallback callback = GetFatalErrorHandler();
  {
    LEAVE_V8(isolate);
    callback(location, "Allocation failed - process out of memory");
  }
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


#ifdef DEBUG
void ImplementationUtilities::ZapHandleRange(i::Object** begin,
                                             i::Object** end) {
  i::HandleScope::ZapRange(begin, end);
}
#endif


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
      auto_enable_(false) { }


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


i::Object** V8::GlobalizeReference(i::Object** obj) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "V8::Persistent::New")) return NULL;
  LOG_API(isolate, "Persistent::New");
  i::Handle<i::Object> result =
      isolate->global_handles()->Create(*obj);
  return result.location();
}


void V8::MakeWeak(i::Object** object, void* parameters,
                  WeakReferenceCallback callback) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "MakeWeak");
  isolate->global_handles()->MakeWeak(object, parameters,
                                                    callback);
}


void V8::ClearWeak(i::Object** obj) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "ClearWeak");
  isolate->global_handles()->ClearWeakness(obj);
}


void V8::MarkIndependent(i::Object** object) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "MakeIndependent");
  isolate->global_handles()->MarkIndependent(object);
}


bool V8::IsGlobalNearDeath(i::Object** obj) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "IsGlobalNearDeath");
  if (!isolate->IsInitialized()) return false;
  return i::GlobalHandles::IsNearDeath(obj);
}


bool V8::IsGlobalWeak(i::Object** obj) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "IsGlobalWeak");
  if (!isolate->IsInitialized()) return false;
  return i::GlobalHandles::IsWeak(obj);
}


void V8::DisposeGlobal(i::Object** obj) {
  i::Isolate* isolate = i::Isolate::Current();
  LOG_API(isolate, "DisposeGlobal");
  if (!isolate->IsInitialized()) return;
  isolate->global_handles()->Destroy(obj);
}

// --- H a n d l e s ---


HandleScope::HandleScope() {
  i::Isolate* isolate = i::Isolate::Current();
  API_ENTRY_CHECK(isolate, "HandleScope::HandleScope");
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate->handle_scope_data();
  isolate_ = isolate;
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
  ASSERT(isolate_ == i::Isolate::Current());
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate_->handle_scope_data();
  current->level--;
  ASSERT(current->level >= 0);
  current->next = prev_next_;
  if (current->limit != prev_limit_) {
    current->limit = prev_limit_;
    i::HandleScope::DeleteExtensions(isolate_);
  }

#ifdef DEBUG
  i::HandleScope::ZapRange(prev_next_, prev_limit_);
#endif
}


int HandleScope::NumberOfHandles() {
  EnsureInitializedForIsolate(
      i::Isolate::Current(), "HandleScope::NumberOfHandles");
  return i::HandleScope::NumberOfHandles();
}


i::Object** HandleScope::CreateHandle(i::Object* value) {
  return i::HandleScope::CreateHandle(value, i::Isolate::Current());
}


i::Object** HandleScope::CreateHandle(i::HeapObject* value) {
  ASSERT(value->IsHeapObject());
  return reinterpret_cast<i::Object**>(
      i::HandleScope::CreateHandle(value, value->GetIsolate()));
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


void Context::SetData(v8::Handle<String> data) {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Context::SetData()")) return;
  i::Handle<i::Object> raw_data = Utils::OpenHandle(*data);
  ASSERT(env->IsGlobalContext());
  if (env->IsGlobalContext()) {
    env->set_data(*raw_data);
  }
}


v8::Local<v8::Value> Context::GetData() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* isolate = env->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Context::GetData()")) {
    return v8::Local<Value>();
  }
  i::Object* raw_result = NULL;
  ASSERT(env->IsGlobalContext());
  if (env->IsGlobalContext()) {
    raw_result = env->data();
  } else {
    return Local<Value>();
  }
  i::Handle<i::Object> result(raw_result, isolate);
  return Utils::ToLocal(result);
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
  i::Handle<i::Object> handle(result);
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
  i::Handle<i::Object> list(Utils::OpenHandle(this)->property_list());
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
  i::Handle<i::Object> result(Utils::OpenHandle(this)->prototype_template());
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
    v8::Handle<Value> data, v8::Handle<Signature> signature) {
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


static i::Handle<i::AccessorInfo> MakeAccessorInfo(
      v8::Handle<String> name,
      AccessorGetter getter,
      AccessorSetter setter,
      v8::Handle<Value> data,
      v8::AccessControl settings,
      v8::PropertyAttribute attributes) {
  i::Handle<i::AccessorInfo> obj = FACTORY->NewAccessorInfo();
  ASSERT(getter != NULL);
  SET_FIELD_WRAPPED(obj, set_getter, getter);
  SET_FIELD_WRAPPED(obj, set_setter, setter);
  if (data.IsEmpty()) data = v8::Undefined();
  obj->set_data(*Utils::OpenHandle(*data));
  obj->set_name(*Utils::OpenHandle(*name));
  if (settings & ALL_CAN_READ) obj->set_all_can_read(true);
  if (settings & ALL_CAN_WRITE) obj->set_all_can_write(true);
  if (settings & PROHIBITS_OVERWRITING) obj->set_prohibits_overwriting(true);
  obj->set_property_attributes(static_cast<PropertyAttributes>(attributes));
  return obj;
}


void FunctionTemplate::AddInstancePropertyAccessor(
      v8::Handle<String> name,
      AccessorGetter getter,
      AccessorSetter setter,
      v8::Handle<Value> data,
      v8::AccessControl settings,
      v8::PropertyAttribute attributes) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate,
                  "v8::FunctionTemplate::AddInstancePropertyAccessor()")) {
    return;
  }
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);

  i::Handle<i::AccessorInfo> obj = MakeAccessorInfo(name,
                                                    getter, setter, data,
                                                    settings, attributes);
  i::Handle<i::Object> list(Utils::OpenHandle(this)->property_accessors());
  if (list->IsUndefined()) {
    list = NeanderArray().value();
    Utils::OpenHandle(this)->set_property_accessors(*list);
  }
  NeanderArray array(list);
  array.add(obj);
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
  if (IsDeadCheck(isolate, "v8::FunctionTemplate::SetPrototypeAttributes()")) {
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


void ObjectTemplate::SetAccessor(v8::Handle<String> name,
                                 AccessorGetter getter,
                                 AccessorSetter setter,
                                 v8::Handle<Value> data,
                                 AccessControl settings,
                                 PropertyAttribute attribute) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::ObjectTemplate::SetAccessor()")) return;
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  EnsureConstructor(this);
  i::FunctionTemplateInfo* constructor =
      i::FunctionTemplateInfo::cast(Utils::OpenHandle(this)->constructor());
  i::Handle<i::FunctionTemplateInfo> cons(constructor);
  Utils::ToLocal(cons)->AddInstancePropertyAccessor(name,
                                                    getter,
                                                    setter,
                                                    data,
                                                    settings,
                                                    attribute);
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
  return i::ParserApi::PreParse(&stream, NULL, i::FLAG_harmony_scoping);
}


ScriptData* ScriptData::PreCompile(v8::Handle<String> source) {
  i::Handle<i::String> str = Utils::OpenHandle(*source);
  if (str->IsExternalTwoByteString()) {
    i::ExternalTwoByteStringUtf16CharacterStream stream(
      i::Handle<i::ExternalTwoByteString>::cast(str), 0, str->length());
    return i::ParserApi::PreParse(&stream, NULL, i::FLAG_harmony_scoping);
  } else {
    i::GenericStringUtf16CharacterStream stream(str, 0, str->length());
    return i::ParserApi::PreParse(&stream, NULL, i::FLAG_harmony_scoping);
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
  i::OS::MemCopy(deserialized_data, data, length);

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
                           NULL,
                           pre_data_impl,
                           Utils::OpenHandle(*script_data),
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
    i::Handle<i::Object> id(script->id());
    raw_id = *id;
  }
  i::Handle<i::Object> id(raw_id);
  return Utils::ToLocal(id);
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
      rethrow_(false) {
  isolate_->RegisterTryCatchHandler(this);
}


v8::TryCatch::~TryCatch() {
  ASSERT(isolate_ == i::Isolate::Current());
  if (rethrow_) {
    v8::HandleScope scope;
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
    i::Handle<i::String> name = isolate_->factory()->LookupAsciiSymbol("stack");
    if (!obj->HasProperty(*name)) return v8::Local<Value>();
    i::Handle<i::Object> value = i::GetProperty(obj, name);
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
  HandleScope scope;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::String> raw_result = i::MessageHandler::GetMessage(obj);
  Local<String> result = Utils::ToLocal(raw_result);
  return scope.Close(result);
}


v8::Handle<Value> Message::GetScriptResourceName() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetScriptResourceName()")) {
    return Local<String>();
  }
  ENTER_V8(isolate);
  HandleScope scope;
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  // Return this.script.name.
  i::Handle<i::JSValue> script =
      i::Handle<i::JSValue>::cast(i::Handle<i::Object>(message->script()));
  i::Handle<i::Object> resource_name(i::Script::cast(script->value())->name());
  return scope.Close(Utils::ToLocal(resource_name));
}


v8::Handle<Value> Message::GetScriptData() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetScriptResourceData()")) {
    return Local<Value>();
  }
  ENTER_V8(isolate);
  HandleScope scope;
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  // Return this.script.data.
  i::Handle<i::JSValue> script =
      i::Handle<i::JSValue>::cast(i::Handle<i::Object>(message->script()));
  i::Handle<i::Object> data(i::Script::cast(script->value())->data());
  return scope.Close(Utils::ToLocal(data));
}


v8::Handle<v8::StackTrace> Message::GetStackTrace() const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Message::GetStackTrace()")) {
    return Local<v8::StackTrace>();
  }
  ENTER_V8(isolate);
  HandleScope scope;
  i::Handle<i::JSMessageObject> message =
      i::Handle<i::JSMessageObject>::cast(Utils::OpenHandle(this));
  i::Handle<i::Object> stackFramesObj(message->stack_frames());
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
  i::Handle<i::String> fmt_str = isolate->factory()->LookupAsciiSymbol(name);
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
  HandleScope scope;
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
  HandleScope scope;
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
  HandleScope scope;
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
  HandleScope scope;
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
  HandleScope scope;
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


bool Value::IsArray() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Value::IsArray()")) return false;
  return Utils::OpenHandle(this)->IsJSArray();
}


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
  return Utils::OpenHandle(this)->IsForeign();
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
  return obj->HasSpecificClassOf(isolate->heap()->Date_symbol());
}


bool Value::IsStringObject() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::IsStringObject()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->HasSpecificClassOf(isolate->heap()->String_symbol());
}


bool Value::IsNumberObject() const {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Value::IsNumberObject()")) return false;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->HasSpecificClassOf(isolate->heap()->Number_symbol());
}


static i::Object* LookupBuiltin(i::Isolate* isolate,
                                const char* builtin_name) {
  i::Handle<i::String> symbol =
      isolate->factory()->LookupAsciiSymbol(builtin_name);
  i::Handle<i::JSBuiltinsObject> builtins = isolate->js_builtins_object();
  return builtins->GetPropertyNoExceptionThrown(*symbol);
}


static bool CheckConstructor(i::Isolate* isolate,
                             i::Handle<i::JSObject> obj,
                             const char* class_name) {
  return obj->map()->constructor() == LookupBuiltin(isolate, class_name);
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
  return obj->HasSpecificClassOf(isolate->heap()->Boolean_symbol());
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
    i::Handle<i::Object> val = i::Execution::ToBoolean(obj);
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
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->IsForeign(),
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


void v8::Date::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Date::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->Date_symbol()),
           "v8::Date::Cast()",
           "Could not convert to date");
}


void v8::StringObject::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::StringObject::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->String_symbol()),
           "v8::StringObject::Cast()",
           "Could not convert to StringObject");
}


void v8::NumberObject::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::NumberObject::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->Number_symbol()),
           "v8::NumberObject::Cast()",
           "Could not convert to NumberObject");
}


void v8::BooleanObject::CheckCast(v8::Value* that) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::BooleanObject::Cast()")) return;
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  ApiCheck(obj->HasSpecificClassOf(isolate->heap()->Boolean_symbol()),
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
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsBoolean()) {
    return obj->IsTrue();
  } else {
    i::Isolate* isolate = i::Isolate::Current();
    if (IsDeadCheck(isolate, "v8::Value::BooleanValue()")) return false;
    LOG_API(isolate, "BooleanValue");
    ENTER_V8(isolate);
    i::Handle<i::Object> value = i::Execution::ToBoolean(obj);
    return value->IsTrue();
  }
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
      value = i::Handle<i::Object>(i::Smi::FromInt(index));
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
    return x == y && !isnan(x) && !isnan(y);
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
    i::Deoptimizer::DeoptimizeAll();
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
  i::Handle<i::Object> result = i::GetProperty(self, key_obj);
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
  if (!key_obj->IsString()) {
    EXCEPTION_PREAMBLE(isolate);
    key_obj = i::Execution::ToString(key_obj, &has_pending_exception);
    EXCEPTION_BAILOUT_CHECK(isolate, static_cast<PropertyAttribute>(NONE));
  }
  i::Handle<i::String> key_string = i::Handle<i::String>::cast(key_obj);
  PropertyAttributes result = self->GetPropertyAttribute(*key_string);
  if (result == ABSENT) return static_cast<PropertyAttribute>(NONE);
  return static_cast<PropertyAttribute>(result);
}


Local<Value> v8::Object::GetPrototype() {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetPrototype()",
             return Local<v8::Value>());
  ENTER_V8(isolate);
  i::Handle<i::Object> self = Utils::OpenHandle(this);
  i::Handle<i::Object> result(self->GetPrototype());
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

  i::Handle<i::Object> name(self->class_name());

  // Native implementation of Object.prototype.toString (v8natives.js):
  //   var c = %ClassOf(this);
  //   if (c === 'Arguments') c  = 'Object';
  //   return "[object " + c + "]";

  if (!name->IsString()) {
    return v8::String::New("[object ]");

  } else {
    i::Handle<i::String> class_name = i::Handle<i::String>::cast(name);
    if (class_name->IsEqualTo(i::CStrVector("Arguments"))) {
      return v8::String::New("[object Object]");

    } else {
      const char* prefix = "[object ";
      Local<String> str = Utils::ToLocal(class_name);
      const char* postfix = "]";

      int prefix_len = i::StrLength(prefix);
      int str_len = str->Length();
      int postfix_len = i::StrLength(postfix);

      int buf_len = prefix_len + str_len + postfix_len;
      i::ScopedVector<char> buf(buf_len);

      // Write prefix.
      char* ptr = buf.start();
      memcpy(ptr, prefix, prefix_len * v8::internal::kCharSize);
      ptr += prefix_len;

      // Write real content.
      str->WriteAscii(ptr, 0, str_len);
      ptr += str_len;

      // Write postfix.
      memcpy(ptr, postfix, postfix_len * v8::internal::kCharSize);

      // Copy the buffer into a heap-allocated string and return it.
      Local<String> result = v8::String::New(buf.start(), buf_len);
      return result;
    }
  }
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


bool v8::Object::Delete(v8::Handle<String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Delete()", return false);
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  return i::JSObject::DeleteProperty(self, key_obj)->IsTrue();
}


bool v8::Object::Has(v8::Handle<String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::Has()", return false);
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  return self->HasProperty(*key_obj);
}


bool v8::Object::Delete(uint32_t index) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::DeleteProperty()",
             return false);
  ENTER_V8(isolate);
  HandleScope scope;
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  return i::JSObject::DeleteElement(self, index)->IsTrue();
}


bool v8::Object::Has(uint32_t index) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::HasProperty()", return false);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  return self->HasElement(index);
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
  i::Handle<i::AccessorInfo> info = MakeAccessorInfo(name,
                                                     getter, setter, data,
                                                     settings, attributes);
  bool fast = Utils::OpenHandle(this)->HasFastProperties();
  i::Handle<i::Object> result = i::SetAccessor(Utils::OpenHandle(this), info);
  if (result.is_null() || result->IsUndefined()) return false;
  if (fast) i::JSObject::TransformToFastProperties(Utils::OpenHandle(this), 0);
  return true;
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
      *Utils::OpenHandle(*key));
}


bool v8::Object::HasRealIndexedProperty(uint32_t index) {
  ON_BAILOUT(Utils::OpenHandle(this)->GetIsolate(),
             "v8::Object::HasRealIndexedProperty()",
             return false);
  return Utils::OpenHandle(this)->HasRealElementProperty(index);
}


bool v8::Object::HasRealNamedCallbackProperty(Handle<String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate,
             "v8::Object::HasRealNamedCallbackProperty()",
             return false);
  ENTER_V8(isolate);
  return Utils::OpenHandle(this)->HasRealNamedCallbackProperty(
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
      isolate->factory()->CopyMapDropTransitions(i::Handle<i::Map>(obj->map()));
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
  return function->context()->global_context();
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
  ENTER_V8(isolate);
  i::HandleScope scope(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);
  i::Handle<i::Object> result =
      i::JSObject::SetHiddenProperty(self, key_obj, value_obj);
  return *result == *self;
}


v8::Local<v8::Value> v8::Object::GetHiddenValue(v8::Handle<v8::String> key) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ON_BAILOUT(isolate, "v8::Object::GetHiddenValue()",
             return Local<v8::Value>());
  ENTER_V8(isolate);
  i::Handle<i::JSObject> self = Utils::OpenHandle(this);
  i::Handle<i::String> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> result(self->GetHiddenProperty(*key_obj));
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
  self->DeleteHiddenProperty(*key_obj);
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
  if (!ApiCheck(length <= i::ExternalPixelArray::kMaxLength,
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
  if (!ApiCheck(length <= i::ExternalArray::kMaxLength,
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
  HandleScope scope;
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
  i::Handle<i::Object> result(raw_result);
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
  return Utils::ToLocal(i::Handle<i::Object>(func->shared()->name()));
}


Handle<Value> Function::GetInferredName() const {
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  return Utils::ToLocal(i::Handle<i::Object>(func->shared()->inferred_name()));
}


ScriptOrigin Function::GetScriptOrigin() const {
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  if (func->shared()->script()->IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared()->script()));
    v8::ScriptOrigin origin(
      Utils::ToLocal(i::Handle<i::Object>(script->name())),
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
  return Utils::ToLocal(i::Handle<i::Object>(script->id()));
}

int String::Length() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (IsDeadCheck(str->GetIsolate(), "v8::String::Length()")) return 0;
  return str->length();
}


int String::Utf8Length() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (IsDeadCheck(str->GetIsolate(), "v8::String::Utf8Length()")) return 0;
  return i::Utf8Length(str);
}


// Will fail with a negative answer if the recursion depth is too high.
static int RecursivelySerializeToUtf8(i::String* string,
                                      char* buffer,
                                      int start,
                                      int end,
                                      int recursion_budget,
                                      int32_t previous_character,
                                      int32_t* last_character) {
  int utf8_bytes = 0;
  while (true) {
    if (string->IsAsciiRepresentation()) {
      i::String::WriteToFlat(string, buffer, start, end);
      *last_character = unibrow::Utf16::kNoPreviousCharacter;
      return utf8_bytes + end - start;
    }
    switch (i::StringShape(string).representation_tag()) {
      case i::kExternalStringTag: {
        const uint16_t* data = i::ExternalTwoByteString::cast(string)->
          ExternalTwoByteStringGetData(0);
        char* current = buffer;
        for (int i = start; i < end; i++) {
          uint16_t character = data[i];
          current +=
              unibrow::Utf8::Encode(current, character, previous_character);
          previous_character = character;
        }
        *last_character = previous_character;
        return static_cast<int>(utf8_bytes + current - buffer);
      }
      case i::kSeqStringTag: {
        const uint16_t* data =
            i::SeqTwoByteString::cast(string)->SeqTwoByteStringGetData(0);
        char* current = buffer;
        for (int i = start; i < end; i++) {
          uint16_t character = data[i];
          current +=
              unibrow::Utf8::Encode(current, character, previous_character);
          previous_character = character;
        }
        *last_character = previous_character;
        return static_cast<int>(utf8_bytes + current - buffer);
      }
      case i::kSlicedStringTag: {
        i::SlicedString* slice = i::SlicedString::cast(string);
        unsigned offset = slice->offset();
        string = slice->parent();
        start += offset;
        end += offset;
        continue;
      }
      case i::kConsStringTag: {
        i::ConsString* cons_string = i::ConsString::cast(string);
        i::String* first = cons_string->first();
        int boundary = first->length();
        if (start >= boundary) {
          // Only need RHS.
          string = cons_string->second();
          start -= boundary;
          end -= boundary;
          continue;
        } else if (end <= boundary) {
          // Only need LHS.
          string = first;
        } else {
          if (recursion_budget == 0) return -1;
          int extra_utf8_bytes =
              RecursivelySerializeToUtf8(first,
                                         buffer,
                                         start,
                                         boundary,
                                         recursion_budget - 1,
                                         previous_character,
                                         &previous_character);
          if (extra_utf8_bytes < 0) return extra_utf8_bytes;
          buffer += extra_utf8_bytes;
          utf8_bytes += extra_utf8_bytes;
          string = cons_string->second();
          start = 0;
          end -= boundary;
        }
      }
    }
  }
  UNREACHABLE();
  return 0;
}


bool String::MayContainNonAscii() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (IsDeadCheck(str->GetIsolate(), "v8::String::MayContainNonAscii()")) {
    return false;
  }
  return !str->HasOnlyAsciiChars();
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
  int string_length = str->length();
  if (str->IsAsciiRepresentation()) {
    int len;
    if (capacity == -1) {
      capacity = str->length() + 1;
      len = string_length;
    } else {
      len = i::Min(capacity, str->length());
    }
    i::String::WriteToFlat(*str, buffer, 0, len);
    if (nchars_ref != NULL) *nchars_ref = len;
    if (!(options & NO_NULL_TERMINATION) && capacity > len) {
      buffer[len] = '\0';
      return len + 1;
    }
    return len;
  }

  if (capacity == -1 || capacity / 3 >= string_length) {
    int32_t previous = unibrow::Utf16::kNoPreviousCharacter;
    const int kMaxRecursion = 100;
    int utf8_bytes =
        RecursivelySerializeToUtf8(*str,
                                   buffer,
                                   0,
                                   string_length,
                                   kMaxRecursion,
                                   previous,
                                   &previous);
    if (utf8_bytes >= 0) {
      // Success serializing with recursion.
      if ((options & NO_NULL_TERMINATION) == 0 &&
          (capacity > utf8_bytes || capacity == -1)) {
        buffer[utf8_bytes++] = '\0';
      }
      if (nchars_ref != NULL) *nchars_ref = string_length;
      return utf8_bytes;
    }
    FlattenString(str);
    // Recurse once.  This time around the string is flat and the serializing
    // with recursion will certainly succeed.
    return WriteUtf8(buffer, capacity, nchars_ref, options);
  } else if (capacity >= string_length) {
    // First check that the buffer is large enough.  If it is, then recurse
    // once without a capacity limit, which will get into the other branch of
    // this 'if'.
    int utf8_bytes = i::Utf8Length(str);
    if ((options & NO_NULL_TERMINATION) == 0) utf8_bytes++;
    if (utf8_bytes <= capacity) {
      return WriteUtf8(buffer, -1, nchars_ref, options);
    }
  }

  // Slow case.
  i::StringInputBuffer& write_input_buffer = *isolate->write_input_buffer();
  isolate->string_tracker()->RecordWrite(str);
  if (options & HINT_MANY_WRITES_EXPECTED) {
    // Flatten the string for efficiency.  This applies whether we are
    // using StringInputBuffer or Get(i) to access the characters.
    FlattenString(str);
  }
  write_input_buffer.Reset(0, *str);
  int len = str->length();
  // Encode the first K - 3 bytes directly into the buffer since we
  // know there's room for them.  If no capacity is given we copy all
  // of them here.
  int fast_end = capacity - (unibrow::Utf8::kMaxEncodedSize - 1);
  int i;
  int pos = 0;
  int nchars = 0;
  int previous = unibrow::Utf16::kNoPreviousCharacter;
  for (i = 0; i < len && (capacity == -1 || pos < fast_end); i++) {
    i::uc32 c = write_input_buffer.GetNext();
    int written = unibrow::Utf8::Encode(buffer + pos, c, previous);
    pos += written;
    nchars++;
    previous = c;
  }
  if (i < len) {
    // For the last characters we need to check the length for each one
    // because they may be longer than the remaining space in the
    // buffer.
    char intermediate[unibrow::Utf8::kMaxEncodedSize];
    for (; i < len && pos < capacity; i++) {
      i::uc32 c = write_input_buffer.GetNext();
      if (unibrow::Utf16::IsTrailSurrogate(c) &&
          unibrow::Utf16::IsLeadSurrogate(previous)) {
        // We can't use the intermediate buffer here because the encoding
        // of surrogate pairs is done under assumption that you can step
        // back and fix the UTF8 stream.  Luckily we only need space for one
        // more byte, so there is always space.
        ASSERT(pos < capacity);
        int written = unibrow::Utf8::Encode(buffer + pos, c, previous);
        ASSERT(written == 1);
        pos += written;
        nchars++;
      } else {
        int written =
            unibrow::Utf8::Encode(intermediate,
                                  c,
                                  unibrow::Utf16::kNoPreviousCharacter);
        if (pos + written <= capacity) {
          for (int j = 0; j < written; j++)
            buffer[pos + j] = intermediate[j];
          pos += written;
          nchars++;
        } else {
          // We've reached the end of the buffer
          break;
        }
      }
      previous = c;
    }
  }
  if (nchars_ref != NULL) *nchars_ref = nchars;
  if (!(options & NO_NULL_TERMINATION) &&
      (i == len && (capacity == -1 || pos < capacity)))
    buffer[pos++] = '\0';
  return pos;
}


int String::WriteAscii(char* buffer,
                       int start,
                       int length,
                       int options) const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::WriteAscii()")) return 0;
  LOG_API(isolate, "String::WriteAscii");
  ENTER_V8(isolate);
  i::StringInputBuffer& write_input_buffer = *isolate->write_input_buffer();
  ASSERT(start >= 0 && length >= -1);
  i::Handle<i::String> str = Utils::OpenHandle(this);
  isolate->string_tracker()->RecordWrite(str);
  if (options & HINT_MANY_WRITES_EXPECTED) {
    // Flatten the string for efficiency.  This applies whether we are
    // using StringInputBuffer or Get(i) to access the characters.
    str->TryFlatten();
  }
  int end = length;
  if ( (length == -1) || (length > str->length() - start) )
    end = str->length() - start;
  if (end < 0) return 0;
  write_input_buffer.Reset(start, *str);
  int i;
  for (i = 0; i < end; i++) {
    char c = static_cast<char>(write_input_buffer.GetNext());
    if (c == '\0') c = ' ';
    buffer[i] = c;
  }
  if (!(options & NO_NULL_TERMINATION) && (length == -1 || i < length))
    buffer[i] = '\0';
  return i;
}


int String::Write(uint16_t* buffer,
                  int start,
                  int length,
                  int options) const {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  if (IsDeadCheck(isolate, "v8::String::Write()")) return 0;
  LOG_API(isolate, "String::Write");
  ENTER_V8(isolate);
  ASSERT(start >= 0 && length >= -1);
  i::Handle<i::String> str = Utils::OpenHandle(this);
  isolate->string_tracker()->RecordWrite(str);
  if (options & HINT_MANY_WRITES_EXPECTED) {
    // Flatten the string for efficiency.  This applies whether we are
    // using StringInputBuffer or Get(i) to access the characters.
    str->TryFlatten();
  }
  int end = start + length;
  if ((length == -1) || (length > str->length() - start) )
    end = str->length();
  if (end < 0) return 0;
  i::String::WriteToFlat(*str, buffer, start, end);
  if (!(options & NO_NULL_TERMINATION) &&
      (length == -1 || end - start < length)) {
    buffer[end - start] = '\0';
  }
  return end - start;
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


Local<Value> v8::Object::CheckedGetInternalField(int index) {
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  if (IsDeadCheck(obj->GetIsolate(), "v8::Object::GetInternalField()")) {
    return Local<Value>();
  }
  if (!ApiCheck(index < obj->GetInternalFieldCount(),
                "v8::Object::GetInternalField()",
                "Reading internal field out of bounds")) {
    return Local<Value>();
  }
  i::Handle<i::Object> value(obj->GetInternalField(index));
  Local<Value> result = Utils::ToLocal(value);
#ifdef DEBUG
  Local<Value> unchecked = UncheckedGetInternalField(index);
  ASSERT(unchecked.IsEmpty() || (unchecked == result));
#endif
  return result;
}


void v8::Object::SetInternalField(int index, v8::Handle<Value> value) {
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  i::Isolate* isolate = obj->GetIsolate();
  if (IsDeadCheck(isolate, "v8::Object::SetInternalField()")) {
    return;
  }
  if (!ApiCheck(index < obj->GetInternalFieldCount(),
                "v8::Object::SetInternalField()",
                "Writing internal field out of bounds")) {
    return;
  }
  ENTER_V8(isolate);
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  obj->SetInternalField(index, *val);
}


static bool CanBeEncodedAsSmi(void* ptr) {
  const uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
  return ((address & i::kEncodablePointerMask) == 0);
}


static i::Smi* EncodeAsSmi(void* ptr) {
  ASSERT(CanBeEncodedAsSmi(ptr));
  const uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
  i::Smi* result = reinterpret_cast<i::Smi*>(address << i::kPointerToSmiShift);
  ASSERT(i::Internals::HasSmiTag(result));
  ASSERT_EQ(result, i::Smi::FromInt(result->value()));
  ASSERT_EQ(ptr, i::Internals::GetExternalPointerFromSmi(result));
  return result;
}


void v8::Object::SetPointerInInternalField(int index, void* value) {
  i::Isolate* isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8(isolate);
  if (CanBeEncodedAsSmi(value)) {
    Utils::OpenHandle(this)->SetInternalField(index, EncodeAsSmi(value));
  } else {
    HandleScope scope;
    i::Handle<i::Foreign> foreign =
        isolate->factory()->NewForeign(
            reinterpret_cast<i::Address>(value), i::TENURED);
    if (!foreign.is_null())
        Utils::OpenHandle(this)->SetInternalField(index, *foreign);
  }
  ASSERT_EQ(value, GetPointerFromInternalField(index));
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
                                  used_heap_size_(0),
                                  heap_size_limit_(0) { }


void v8::V8::GetHeapStatistics(HeapStatistics* heap_statistics) {
  if (!i::Isolate::Current()->IsInitialized()) {
    // Isolate is unitialized thus heap is not configured yet.
    heap_statistics->set_total_heap_size(0);
    heap_statistics->set_total_heap_size_executable(0);
    heap_statistics->set_used_heap_size(0);
    heap_statistics->set_heap_size_limit(0);
    return;
  }

  i::Heap* heap = i::Isolate::Current()->heap();
  heap_statistics->set_total_heap_size(heap->CommittedMemory());
  heap_statistics->set_total_heap_size_executable(
      heap->CommittedMemoryExecutable());
  heap_statistics->set_used_heap_size(heap->SizeOfObjects());
  heap_statistics->set_heap_size_limit(heap->MaxReserved());
}


void v8::V8::VisitExternalResources(ExternalResourceVisitor* visitor) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::V8::VisitExternalResources");
  isolate->heap()->VisitExternalResources(visitor);
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


Persistent<Context> v8::Context::New(
    v8::ExtensionConfiguration* extensions,
    v8::Handle<ObjectTemplate> global_template,
    v8::Handle<Value> global_object) {
  i::Isolate::EnsureDefaultIsolate();
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Context::New()");
  LOG_API(isolate, "Context::New");
  ON_BAILOUT(isolate, "v8::Context::New()", return Persistent<Context>());

  // Enter V8 via an ENTER_V8 scope.
  i::Handle<i::Context> env;
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
        isolate,
        Utils::OpenHandle(*global_object),
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

  if (env.is_null()) {
    return Persistent<Context>();
  }
  return Persistent<Context>(Utils::ToLocal(env));
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
  env->set_security_token(env->global());
}


Handle<Value> v8::Context::GetSecurityToken() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::GetSecurityToken()")) {
    return Handle<Value>();
  }
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Object* security_token = env->security_token();
  i::Handle<i::Object> token_handle(security_token);
  return Utils::ToLocal(token_handle);
}


bool Context::HasOutOfMemoryException() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  return env->has_out_of_memory();
}


bool Context::InContext() {
  return i::Isolate::Current()->context() != NULL;
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
  i::Handle<i::Object> current = isolate->global_context();
  if (current.is_null()) return Local<Context>();
  i::Handle<i::Context> context = i::Handle<i::Context>::cast(current);
  return Utils::ToLocal(context);
}


v8::Local<v8::Context> Context::GetCalling() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::Context::GetCalling()")) {
    return Local<Context>();
  }
  i::Handle<i::Object> calling =
      isolate->GetCallingGlobalContext();
  if (calling.is_null()) return Local<Context>();
  i::Handle<i::Context> context = i::Handle<i::Context>::cast(calling);
  return Utils::ToLocal(context);
}


v8::Local<v8::Object> Context::Global() {
  if (IsDeadCheck(i::Isolate::Current(), "v8::Context::Global()")) {
    return Local<v8::Object>();
  }
  i::Object** ctx = reinterpret_cast<i::Object**>(this);
  i::Handle<i::Context> context =
      i::Handle<i::Context>::cast(i::Handle<i::Object>(ctx));
  i::Handle<i::Object> global(context->global_proxy());
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
  isolate->bootstrapper()->ReattachGlobal(
      context,
      Utils::OpenHandle(*global_object));
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


void V8::SetWrapperClassId(i::Object** global_handle, uint16_t class_id) {
  i::GlobalHandles::SetWrapperClassId(global_handle, class_id);
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


static Local<External> ExternalNewImpl(void* data) {
  return Utils::ToLocal(FACTORY->NewForeign(static_cast<i::Address>(data)));
}

static void* ExternalValueImpl(i::Handle<i::Object> obj) {
  return reinterpret_cast<void*>(i::Foreign::cast(*obj)->foreign_address());
}


Local<Value> v8::External::Wrap(void* data) {
  i::Isolate* isolate = i::Isolate::Current();
  STATIC_ASSERT(sizeof(data) == sizeof(i::Address));
  EnsureInitializedForIsolate(isolate, "v8::External::Wrap()");
  LOG_API(isolate, "External::Wrap");
  ENTER_V8(isolate);

  v8::Local<v8::Value> result = CanBeEncodedAsSmi(data)
      ? Utils::ToLocal(i::Handle<i::Object>(EncodeAsSmi(data)))
      : v8::Local<v8::Value>(ExternalNewImpl(data));

  ASSERT_EQ(data, Unwrap(result));
  return result;
}


void* v8::Object::SlowGetPointerFromInternalField(int index) {
  i::Handle<i::JSObject> obj = Utils::OpenHandle(this);
  i::Object* value = obj->GetInternalField(index);
  if (value->IsSmi()) {
    return i::Internals::GetExternalPointerFromSmi(value);
  } else if (value->IsForeign()) {
    return reinterpret_cast<void*>(i::Foreign::cast(value)->foreign_address());
  } else {
    return NULL;
  }
}


void* v8::External::FullUnwrap(v8::Handle<v8::Value> wrapper) {
  if (IsDeadCheck(i::Isolate::Current(), "v8::External::Unwrap()")) return 0;
  i::Handle<i::Object> obj = Utils::OpenHandle(*wrapper);
  void* result;
  if (obj->IsSmi()) {
    result = i::Internals::GetExternalPointerFromSmi(*obj);
  } else if (obj->IsForeign()) {
    result = ExternalValueImpl(obj);
  } else {
    result = NULL;
  }
  ASSERT_EQ(result, QuickUnwrap(wrapper));
  return result;
}


Local<External> v8::External::New(void* data) {
  STATIC_ASSERT(sizeof(data) == sizeof(i::Address));
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::External::New()");
  LOG_API(isolate, "External::New");
  ENTER_V8(isolate);
  return ExternalNewImpl(data);
}


void* External::Value() const {
  if (IsDeadCheck(i::Isolate::Current(), "v8::External::Value()")) return 0;
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return ExternalValueImpl(obj);
}


Local<String> v8::String::Empty() {
  i::Isolate* isolate = i::Isolate::Current();
  if (!EnsureInitializedForIsolate(isolate, "v8::String::Empty()")) {
    return v8::Local<String>();
  }
  LOG_API(isolate, "String::Empty()");
  return Utils::ToLocal(isolate->factory()->empty_symbol());
}


Local<String> v8::String::New(const char* data, int length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::String::New()");
  LOG_API(isolate, "String::New(char)");
  if (length == 0) return Empty();
  ENTER_V8(isolate);
  if (length == -1) length = i::StrLength(data);
  i::Handle<i::String> result =
      isolate->factory()->NewStringFromUtf8(
          i::Vector<const char>(data, length));
  return Utils::ToLocal(result);
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


Local<String> v8::String::NewUndetectable(const char* data, int length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::String::NewUndetectable()");
  LOG_API(isolate, "String::NewUndetectable(char)");
  ENTER_V8(isolate);
  if (length == -1) length = i::StrLength(data);
  i::Handle<i::String> result =
      isolate->factory()->NewStringFromUtf8(
          i::Vector<const char>(data, length));
  result->MarkAsUndetectable();
  return Utils::ToLocal(result);
}


static int TwoByteStringLength(const uint16_t* data) {
  int length = 0;
  while (data[length] != '\0') length++;
  return length;
}


Local<String> v8::String::New(const uint16_t* data, int length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::String::New()");
  LOG_API(isolate, "String::New(uint16_)");
  if (length == 0) return Empty();
  ENTER_V8(isolate);
  if (length == -1) length = TwoByteStringLength(data);
  i::Handle<i::String> result =
      isolate->factory()->NewStringFromTwoByte(
          i::Vector<const uint16_t>(data, length));
  return Utils::ToLocal(result);
}


Local<String> v8::String::NewUndetectable(const uint16_t* data, int length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::String::NewUndetectable()");
  LOG_API(isolate, "String::NewUndetectable(uint16_)");
  ENTER_V8(isolate);
  if (length == -1) length = TwoByteStringLength(data);
  i::Handle<i::String> result =
      isolate->factory()->NewStringFromTwoByte(
          i::Vector<const uint16_t>(data, length));
  result->MarkAsUndetectable();
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
  bool result = obj->MakeExternal(resource);
  if (result && !obj->IsSymbol()) {
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
  bool result = obj->MakeExternal(resource);
  if (result && !obj->IsSymbol()) {
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
  i::Handle<i::Object> boolean(value ? isolate->heap()->true_value()
                                     : isolate->heap()->false_value());
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


Local<v8::Value> v8::Date::New(double time) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Date::New()");
  LOG_API(isolate, "Date::New");
  if (isnan(time)) {
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
      isolate->factory()->LookupAsciiSymbol("ResetDateCache");
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
  char flags_buf[3];
  int num_flags = 0;
  if ((flags & RegExp::kGlobal) != 0) flags_buf[num_flags++] = 'g';
  if ((flags & RegExp::kMultiline) != 0) flags_buf[num_flags++] = 'm';
  if ((flags & RegExp::kIgnoreCase) != 0) flags_buf[num_flags++] = 'i';
  ASSERT(num_flags <= static_cast<int>(ARRAY_SIZE(flags_buf)));
  return FACTORY->LookupSymbol(
      i::Vector<const char>(flags_buf, num_flags));
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
  if (!self->HasFastElements()) {
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


Local<String> v8::String::NewSymbol(const char* data, int length) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::String::NewSymbol()");
  LOG_API(isolate, "String::NewSymbol(char)");
  ENTER_V8(isolate);
  if (length == -1) length = i::StrLength(data);
  i::Handle<i::String> result =
      isolate->factory()->LookupSymbol(i::Vector<const char>(data, length));
  return Utils::ToLocal(result);
}


Local<Number> v8::Number::New(double value) {
  i::Isolate* isolate = i::Isolate::Current();
  EnsureInitializedForIsolate(isolate, "v8::Number::New()");
  if (isnan(value)) {
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
  if (i::Smi::IsValid(value)) {
    return Utils::IntegerToLocal(i::Handle<i::Object>(i::Smi::FromInt(value),
                                                      isolate));
  }
  ENTER_V8(isolate);
  i::Handle<i::Object> result = isolate->factory()->NewNumber(value);
  return Utils::IntegerToLocal(result);
}


Local<Integer> Integer::NewFromUnsigned(uint32_t value) {
  bool fits_into_int32_t = (value & (1 << 31)) == 0;
  if (fits_into_int32_t) {
    return Integer::New(static_cast<int32_t>(value));
  }
  i::Isolate* isolate = i::Isolate::Current();
  ENTER_V8(isolate);
  i::Handle<i::Object> result = isolate->factory()->NewNumber(value);
  return Utils::IntegerToLocal(result);
}


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
  obj.set(1, data.IsEmpty() ?
             isolate->heap()->undefined_value() :
             *Utils::OpenHandle(*data));
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
}

void V8::SetAddHistogramSampleFunction(AddHistogramSampleCallback callback) {
  i::Isolate* isolate = EnterIsolateIfNeeded();
  if (IsDeadCheck(isolate, "v8::V8::SetAddHistogramSampleFunction()")) return;
  isolate->stats_table()->
      SetAddHistogramSampleFunction(callback);
}

void V8::EnableSlidingStateWindow() {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::EnableSlidingStateWindow()")) return;
  isolate->logger()->EnableSlidingStateWindow();
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


intptr_t V8::AdjustAmountOfExternalAllocatedMemory(intptr_t change_in_bytes) {
  i::Isolate* isolate = i::Isolate::Current();
  if (IsDeadCheck(isolate, "v8::V8::AdjustAmountOfExternalAllocatedMemory()")) {
    return 0;
  }
  return isolate->heap()->AdjustAmountOfExternalAllocatedMemory(
      change_in_bytes);
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
  length_ = i::Utf8Length(i_str);
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
  length_ = str->Length();
  str_ = i::NewArray<char>(length_ + 1);
  str->WriteAscii(str_);
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
  i::Handle<i::Object> result(error);
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
  i::Handle<i::Object> result(error);
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
  i::Handle<i::Object> result(error);
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
  i::Handle<i::Object> result(error);
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
  i::Handle<i::Object> result(error);
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
  isolate->debugger()->SetEventListener(foreign, Utils::OpenHandle(*data));
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
  isolate->debugger()->SetEventListener(foreign, Utils::OpenHandle(*data));
  return true;
}


bool Debug::SetDebugEventListener(v8::Handle<v8::Object> that,
                                  Handle<Value> data) {
  i::Isolate* isolate = i::Isolate::Current();
  ON_BAILOUT(isolate, "v8::Debug::SetDebugEventListener()", return false);
  ENTER_V8(isolate);
  isolate->debugger()->SetEventListener(Utils::OpenHandle(*that),
                                                      Utils::OpenHandle(*data));
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
  v8::HandleScope scope;
  i::Debug* isolate_debug = isolate->debug();
  isolate_debug->Load();
  i::Handle<i::JSObject> debug(isolate_debug->debug_context()->global());
  i::Handle<i::String> name =
      isolate->factory()->LookupAsciiSymbol("MakeMirror");
  i::Handle<i::Object> fun_obj = i::GetProperty(debug, name);
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

#endif  // ENABLE_DEBUGGER_SUPPORT


Handle<String> CpuProfileNode::GetFunctionName() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetFunctionName");
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  const i::CodeEntry* entry = node->entry();
  if (!entry->has_name_prefix()) {
    return Handle<String>(ToApi<String>(
        isolate->factory()->LookupAsciiSymbol(entry->name())));
  } else {
    return Handle<String>(ToApi<String>(isolate->factory()->NewConsString(
        isolate->factory()->LookupAsciiSymbol(entry->name_prefix()),
        isolate->factory()->LookupAsciiSymbol(entry->name()))));
  }
}


Handle<String> CpuProfileNode::GetScriptResourceName() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfileNode::GetScriptResourceName");
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  return Handle<String>(ToApi<String>(isolate->factory()->LookupAsciiSymbol(
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
  i::CpuProfiler::DeleteProfile(reinterpret_cast<i::CpuProfile*>(this));
  if (i::CpuProfiler::GetProfilesCount() == 0 &&
      !i::CpuProfiler::HasDetachedProfiles()) {
    // If this was the last profile, clean up all accessory data as well.
    i::CpuProfiler::DeleteAllProfiles();
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
  return Handle<String>(ToApi<String>(isolate->factory()->LookupAsciiSymbol(
      profile->title())));
}


const CpuProfileNode* CpuProfile::GetBottomUpRoot() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfile::GetBottomUpRoot");
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->bottom_up()->root());
}


const CpuProfileNode* CpuProfile::GetTopDownRoot() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfile::GetTopDownRoot");
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->top_down()->root());
}


int CpuProfiler::GetProfilesCount() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::GetProfilesCount");
  return i::CpuProfiler::GetProfilesCount();
}


const CpuProfile* CpuProfiler::GetProfile(int index,
                                          Handle<Value> security_token) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::GetProfile");
  return reinterpret_cast<const CpuProfile*>(
      i::CpuProfiler::GetProfile(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          index));
}


const CpuProfile* CpuProfiler::FindProfile(unsigned uid,
                                           Handle<Value> security_token) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::FindProfile");
  return reinterpret_cast<const CpuProfile*>(
      i::CpuProfiler::FindProfile(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          uid));
}


void CpuProfiler::StartProfiling(Handle<String> title) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::StartProfiling");
  i::CpuProfiler::StartProfiling(*Utils::OpenHandle(*title));
}


const CpuProfile* CpuProfiler::StopProfiling(Handle<String> title,
                                             Handle<Value> security_token) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::StopProfiling");
  return reinterpret_cast<const CpuProfile*>(
      i::CpuProfiler::StopProfiling(
          security_token.IsEmpty() ? NULL : *Utils::OpenHandle(*security_token),
          *Utils::OpenHandle(*title)));
}


void CpuProfiler::DeleteAllProfiles() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::CpuProfiler::DeleteAllProfiles");
  i::CpuProfiler::DeleteAllProfiles();
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
      return Handle<String>(ToApi<String>(isolate->factory()->LookupAsciiSymbol(
          edge->name())));
    case i::HeapGraphEdge::kElement:
    case i::HeapGraphEdge::kHidden:
      return Handle<Number>(ToApi<Number>(isolate->factory()->NewNumberFromInt(
          edge->index())));
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
  return Handle<String>(ToApi<String>(isolate->factory()->LookupAsciiSymbol(
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


int HeapGraphNode::GetRetainedSize() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetRetainedSize");
  return ToInternal(this)->retained_size();
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
      &ToInternal(this)->children()[index]);
}


int HeapGraphNode::GetRetainersCount() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetRetainersCount");
  return ToInternal(this)->retainers().length();
}


const HeapGraphEdge* HeapGraphNode::GetRetainer(int index) const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetRetainer");
  return reinterpret_cast<const HeapGraphEdge*>(
      ToInternal(this)->retainers()[index]);
}


const HeapGraphNode* HeapGraphNode::GetDominatorNode() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetDominatorNode");
  return reinterpret_cast<const HeapGraphNode*>(ToInternal(this)->dominator());
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
  if (i::HeapProfiler::GetSnapshotsCount() > 1) {
    ToInternal(this)->Delete();
  } else {
    // If this is the last snapshot, clean up all accessory data as well.
    i::HeapProfiler::DeleteAllSnapshots();
  }
}


HeapSnapshot::Type HeapSnapshot::GetType() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetType");
  return static_cast<HeapSnapshot::Type>(ToInternal(this)->type());
}


unsigned HeapSnapshot::GetUid() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetUid");
  return ToInternal(this)->uid();
}


Handle<String> HeapSnapshot::GetTitle() const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetTitle");
  return Handle<String>(ToApi<String>(isolate->factory()->LookupAsciiSymbol(
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
  return ToInternal(this)->entries()->length();
}


const HeapGraphNode* HeapSnapshot::GetNode(int index) const {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapSnapshot::GetNode");
  return reinterpret_cast<const HeapGraphNode*>(
      ToInternal(this)->entries()->at(index));
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
  return i::HeapProfiler::GetSnapshotsCount();
}


const HeapSnapshot* HeapProfiler::GetSnapshot(int index) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::GetSnapshot");
  return reinterpret_cast<const HeapSnapshot*>(
      i::HeapProfiler::GetSnapshot(index));
}


const HeapSnapshot* HeapProfiler::FindSnapshot(unsigned uid) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::FindSnapshot");
  return reinterpret_cast<const HeapSnapshot*>(
      i::HeapProfiler::FindSnapshot(uid));
}


SnapshotObjectId HeapProfiler::GetSnapshotObjectId(Handle<Value> value) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::GetSnapshotObjectId");
  i::Handle<i::Object> obj = Utils::OpenHandle(*value);
  return i::HeapProfiler::GetSnapshotObjectId(obj);
}


const HeapSnapshot* HeapProfiler::TakeSnapshot(Handle<String> title,
                                               HeapSnapshot::Type type,
                                               ActivityControl* control) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::TakeSnapshot");
  i::HeapSnapshot::Type internal_type = i::HeapSnapshot::kFull;
  switch (type) {
    case HeapSnapshot::kFull:
      internal_type = i::HeapSnapshot::kFull;
      break;
    default:
      UNREACHABLE();
  }
  return reinterpret_cast<const HeapSnapshot*>(
      i::HeapProfiler::TakeSnapshot(
          *Utils::OpenHandle(*title), internal_type, control));
}


void HeapProfiler::StartHeapObjectsTracking() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::StartHeapObjectsTracking");
  i::HeapProfiler::StartHeapObjectsTracking();
}


void HeapProfiler::StopHeapObjectsTracking() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::StopHeapObjectsTracking");
  i::HeapProfiler::StopHeapObjectsTracking();
}


void HeapProfiler::PushHeapObjectsStats(OutputStream* stream) {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::PushHeapObjectsStats");
  return i::HeapProfiler::PushHeapObjectsStats(stream);
}


void HeapProfiler::DeleteAllSnapshots() {
  i::Isolate* isolate = i::Isolate::Current();
  IsDeadCheck(isolate, "v8::HeapProfiler::DeleteAllSnapshots");
  i::HeapProfiler::DeleteAllSnapshots();
}


void HeapProfiler::DefineWrapperClass(uint16_t class_id,
                                      WrapperInfoCallback callback) {
  i::Isolate::Current()->heap_profiler()->DefineWrapperClass(class_id,
                                                             callback);
}


int HeapProfiler::GetPersistentHandleCount() {
  i::Isolate* isolate = i::Isolate::Current();
  return isolate->global_handles()->NumberOfGlobalHandles();
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


void Testing::DeoptimizeAll() {
  internal::Deoptimizer::DeoptimizeAll();
}


namespace internal {


void HandleScopeImplementer::FreeThreadResources() {
  Free();
}


char* HandleScopeImplementer::ArchiveThread(char* storage) {
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate_->handle_scope_data();
  handle_scope_data_ = *current;
  memcpy(storage, this, sizeof(*this));

  ResetAfterArchive();
  current->Initialize();

  return storage + ArchiveSpacePerThread();
}


int HandleScopeImplementer::ArchiveSpacePerThread() {
  return sizeof(HandleScopeImplementer);
}


char* HandleScopeImplementer::RestoreThread(char* storage) {
  memcpy(this, storage, sizeof(*this));
  *isolate_->handle_scope_data() = handle_scope_data_;
  return storage + ArchiveSpacePerThread();
}


void HandleScopeImplementer::IterateThis(ObjectVisitor* v) {
  // Iterate over all handles in the blocks except for the last.
  for (int i = blocks()->length() - 2; i >= 0; --i) {
    Object** block = blocks()->at(i);
    v->VisitPointers(block, &block[kHandleBlockSize]);
  }

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

} }  // namespace v8::internal
