// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "src/third_party/vtune/v8-vtune.h"
#endif

#include "include/libplatform/libplatform.h"
#include "include/libplatform/v8-tracing.h"
#include "include/v8-inspector.h"
#include "include/v8-profiler.h"
#include "src/api/api-inl.h"
#include "src/base/cpu.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/platform/wrappers.h"
#include "src/base/sys-info.h"
#include "src/d8/d8-console.h"
#include "src/d8/d8-platforms.h"
#include "src/d8/d8.h"
#include "src/debug/debug-interface.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/basic-block-profiler.h"
#include "src/execution/vm-state-inl.h"
#include "src/flags/flags.h"
#include "src/handles/maybe-handles.h"
#include "src/init/v8.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/counters.h"
#include "src/logging/log-utils.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/profiler/profile-generator.h"
#include "src/sanitizer/msan.h"
#include "src/snapshot/snapshot.h"
#include "src/tasks/cancelable-task.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/wasm/wasm-engine.h"

#ifdef V8_FUZZILLI
#include "src/d8/cov.h"
#endif  // V8_FUZZILLI

#ifdef V8_USE_PERFETTO
#include "perfetto/tracing.h"
#endif  // V8_USE_PERFETTO

#ifdef V8_INTL_SUPPORT
#include "unicode/locid.h"
#endif  // V8_INTL_SUPPORT

#ifdef V8_OS_LINUX
#include <sys/mman.h>  // For MultiMappedAllocator.
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#else
#include <windows.h>  // NOLINT
#endif                // !defined(_WIN32) && !defined(_WIN64)

#ifndef DCHECK
#define DCHECK(condition) assert(condition)
#endif

#ifndef CHECK
#define CHECK(condition) assert(condition)
#endif

#define TRACE_BS(...)                                     \
  do {                                                    \
    if (i::FLAG_trace_backing_store) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {

namespace {

const int kMB = 1024 * 1024;

#ifdef V8_FUZZILLI
// REPRL = read-eval-print-reset-loop
// These file descriptors are being opened when Fuzzilli uses fork & execve to
// run V8.
#define REPRL_CRFD 100  // Control read file decriptor
#define REPRL_CWFD 101  // Control write file decriptor
#define REPRL_DRFD 102  // Data read file decriptor
#define REPRL_DWFD 103  // Data write file decriptor
bool fuzzilli_reprl = true;
#else
bool fuzzilli_reprl = false;
#endif  // V8_FUZZILLI

const int kMaxSerializerMemoryUsage =
    1 * kMB;  // Arbitrary maximum for testing.

// Base class for shell ArrayBuffer allocators. It forwards all opertions to
// the default v8 allocator.
class ArrayBufferAllocatorBase : public v8::ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override {
    return allocator_->Allocate(length);
  }

  void* AllocateUninitialized(size_t length) override {
    return allocator_->AllocateUninitialized(length);
  }

  void Free(void* data, size_t length) override {
    allocator_->Free(data, length);
  }

 private:
  std::unique_ptr<Allocator> allocator_ =
      std::unique_ptr<Allocator>(NewDefaultAllocator());
};

// ArrayBuffer allocator that can use virtual memory to improve performance.
class ShellArrayBufferAllocator : public ArrayBufferAllocatorBase {
 public:
  void* Allocate(size_t length) override {
    if (length >= kVMThreshold) return AllocateVM(length);
    return ArrayBufferAllocatorBase::Allocate(length);
  }

  void* AllocateUninitialized(size_t length) override {
    if (length >= kVMThreshold) return AllocateVM(length);
    return ArrayBufferAllocatorBase::AllocateUninitialized(length);
  }

  void Free(void* data, size_t length) override {
    if (length >= kVMThreshold) {
      FreeVM(data, length);
    } else {
      ArrayBufferAllocatorBase::Free(data, length);
    }
  }

 private:
  static constexpr size_t kVMThreshold = 65536;

  void* AllocateVM(size_t length) {
    DCHECK_LE(kVMThreshold, length);
    v8::PageAllocator* page_allocator = i::GetPlatformPageAllocator();
    size_t page_size = page_allocator->AllocatePageSize();
    size_t allocated = RoundUp(length, page_size);
    return i::AllocatePages(page_allocator, nullptr, allocated, page_size,
                            PageAllocator::kReadWrite);
  }

  void FreeVM(void* data, size_t length) {
    v8::PageAllocator* page_allocator = i::GetPlatformPageAllocator();
    size_t page_size = page_allocator->AllocatePageSize();
    size_t allocated = RoundUp(length, page_size);
    CHECK(i::FreePages(page_allocator, data, allocated));
  }
};

// ArrayBuffer allocator that never allocates over 10MB.
class MockArrayBufferAllocator : public ArrayBufferAllocatorBase {
 protected:
  void* Allocate(size_t length) override {
    return ArrayBufferAllocatorBase::Allocate(Adjust(length));
  }

  void* AllocateUninitialized(size_t length) override {
    return ArrayBufferAllocatorBase::AllocateUninitialized(Adjust(length));
  }

  void Free(void* data, size_t length) override {
    return ArrayBufferAllocatorBase::Free(data, Adjust(length));
  }

 private:
  size_t Adjust(size_t length) {
    const size_t kAllocationLimit = 10 * kMB;
    return length > kAllocationLimit ? i::AllocatePageSize() : length;
  }
};

// ArrayBuffer allocator that can be equipped with a limit to simulate system
// OOM.
class MockArrayBufferAllocatiorWithLimit : public MockArrayBufferAllocator {
 public:
  explicit MockArrayBufferAllocatiorWithLimit(size_t allocation_limit)
      : space_left_(allocation_limit) {}

 protected:
  void* Allocate(size_t length) override {
    if (length > space_left_) {
      return nullptr;
    }
    space_left_ -= length;
    return MockArrayBufferAllocator::Allocate(length);
  }

  void* AllocateUninitialized(size_t length) override {
    if (length > space_left_) {
      return nullptr;
    }
    space_left_ -= length;
    return MockArrayBufferAllocator::AllocateUninitialized(length);
  }

  void Free(void* data, size_t length) override {
    space_left_ += length;
    return MockArrayBufferAllocator::Free(data, length);
  }

 private:
  std::atomic<size_t> space_left_;
};

#ifdef V8_OS_LINUX

// This is a mock allocator variant that provides a huge virtual allocation
// backed by a small real allocation that is repeatedly mapped. If you create an
// array on memory allocated by this allocator, you will observe that elements
// will alias each other as if their indices were modulo-divided by the real
// allocation length.
// The purpose is to allow stability-testing of huge (typed) arrays without
// actually consuming huge amounts of physical memory.
// This is currently only available on Linux because it relies on {mremap}.
class MultiMappedAllocator : public ArrayBufferAllocatorBase {
 protected:
  void* Allocate(size_t length) override {
    if (length < kChunkSize) {
      return ArrayBufferAllocatorBase::Allocate(length);
    }
    // We use mmap, which initializes pages to zero anyway.
    return AllocateUninitialized(length);
  }

  void* AllocateUninitialized(size_t length) override {
    if (length < kChunkSize) {
      return ArrayBufferAllocatorBase::AllocateUninitialized(length);
    }
    size_t rounded_length = RoundUp(length, kChunkSize);
    int prot = PROT_READ | PROT_WRITE;
    // We have to specify MAP_SHARED to make {mremap} below do what we want.
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    void* real_alloc = mmap(nullptr, kChunkSize, prot, flags, -1, 0);
    if (reinterpret_cast<intptr_t>(real_alloc) == -1) {
      // If we ran into some limit (physical or virtual memory, or number
      // of mappings, etc), return {nullptr}, which callers can handle.
      if (errno == ENOMEM) {
        return nullptr;
      }
      // Other errors may be bugs which we want to learn about.
      FATAL("mmap (real) failed with error %d: %s", errno, strerror(errno));
    }
    void* virtual_alloc =
        mmap(nullptr, rounded_length, prot, flags | MAP_NORESERVE, -1, 0);
    if (reinterpret_cast<intptr_t>(virtual_alloc) == -1) {
      if (errno == ENOMEM) {
        // Undo earlier, successful mappings.
        munmap(real_alloc, kChunkSize);
        return nullptr;
      }
      FATAL("mmap (virtual) failed with error %d: %s", errno, strerror(errno));
    }
    i::Address virtual_base = reinterpret_cast<i::Address>(virtual_alloc);
    i::Address virtual_end = virtual_base + rounded_length;
    for (i::Address to_map = virtual_base; to_map < virtual_end;
         to_map += kChunkSize) {
      // Specifying 0 as the "old size" causes the existing map entry to not
      // get deleted, which is important so that we can remap it again in the
      // next iteration of this loop.
      void* result =
          mremap(real_alloc, 0, kChunkSize, MREMAP_MAYMOVE | MREMAP_FIXED,
                 reinterpret_cast<void*>(to_map));
      if (reinterpret_cast<intptr_t>(result) == -1) {
        if (errno == ENOMEM) {
          // Undo earlier, successful mappings.
          munmap(real_alloc, kChunkSize);
          munmap(virtual_alloc, (to_map - virtual_base));
          return nullptr;
        }
        FATAL("mremap failed with error %d: %s", errno, strerror(errno));
      }
    }
    base::MutexGuard lock_guard(&regions_mutex_);
    regions_[virtual_alloc] = real_alloc;
    return virtual_alloc;
  }

  void Free(void* data, size_t length) override {
    if (length < kChunkSize) {
      return ArrayBufferAllocatorBase::Free(data, length);
    }
    base::MutexGuard lock_guard(&regions_mutex_);
    void* real_alloc = regions_[data];
    munmap(real_alloc, kChunkSize);
    size_t rounded_length = RoundUp(length, kChunkSize);
    munmap(data, rounded_length);
    regions_.erase(data);
  }

 private:
  // Aiming for a "Huge Page" (2M on Linux x64) to go easy on the TLB.
  static constexpr size_t kChunkSize = 2 * 1024 * 1024;

  std::unordered_map<void*, void*> regions_;
  base::Mutex regions_mutex_;
};

#endif  // V8_OS_LINUX

v8::Platform* g_default_platform;
std::unique_ptr<v8::Platform> g_platform;

static Local<Value> Throw(Isolate* isolate, const char* message) {
  return isolate->ThrowException(v8::Exception::Error(
      String::NewFromUtf8(isolate, message).ToLocalChecked()));
}

static MaybeLocal<Value> TryGetValue(v8::Isolate* isolate,
                                     Local<Context> context,
                                     Local<v8::Object> object,
                                     const char* property) {
  MaybeLocal<String> v8_str = String::NewFromUtf8(isolate, property);
  if (v8_str.IsEmpty()) return {};
  return object->Get(context, v8_str.ToLocalChecked());
}

static Local<Value> GetValue(v8::Isolate* isolate, Local<Context> context,
                             Local<v8::Object> object, const char* property) {
  return TryGetValue(isolate, context, object, property).ToLocalChecked();
}

std::shared_ptr<Worker> GetWorkerFromInternalField(Isolate* isolate,
                                                   Local<Object> object) {
  if (object->InternalFieldCount() != 1) {
    Throw(isolate, "this is not a Worker");
    return nullptr;
  }

  i::Handle<i::Object> handle = Utils::OpenHandle(*object->GetInternalField(0));
  if (handle->IsSmi()) {
    Throw(isolate, "Worker is defunct because main thread is terminating");
    return nullptr;
  }
  auto managed = i::Handle<i::Managed<Worker>>::cast(handle);
  return managed->get();
}

base::Thread::Options GetThreadOptions(const char* name) {
  // On some systems (OSX 10.6) the stack size default is 0.5Mb or less
  // which is not enough to parse the big literal expressions used in tests.
  // The stack size should be at least StackGuard::kLimitSize + some
  // OS-specific padding for thread startup code.  2Mbytes seems to be enough.
  return base::Thread::Options(name, 2 * kMB);
}

}  // namespace

namespace tracing {

namespace {

static constexpr char kIncludedCategoriesParam[] = "included_categories";

class TraceConfigParser {
 public:
  static void FillTraceConfig(v8::Isolate* isolate,
                              platform::tracing::TraceConfig* trace_config,
                              const char* json_str) {
    HandleScope outer_scope(isolate);
    Local<Context> context = Context::New(isolate);
    Context::Scope context_scope(context);
    HandleScope inner_scope(isolate);

    Local<String> source =
        String::NewFromUtf8(isolate, json_str).ToLocalChecked();
    Local<Value> result = JSON::Parse(context, source).ToLocalChecked();
    Local<v8::Object> trace_config_object = result.As<v8::Object>();

    UpdateIncludedCategoriesList(isolate, context, trace_config_object,
                                 trace_config);
  }

 private:
  static int UpdateIncludedCategoriesList(
      v8::Isolate* isolate, Local<Context> context, Local<v8::Object> object,
      platform::tracing::TraceConfig* trace_config) {
    Local<Value> value =
        GetValue(isolate, context, object, kIncludedCategoriesParam);
    if (value->IsArray()) {
      Local<Array> v8_array = value.As<Array>();
      for (int i = 0, length = v8_array->Length(); i < length; ++i) {
        Local<Value> v = v8_array->Get(context, i)
                             .ToLocalChecked()
                             ->ToString(context)
                             .ToLocalChecked();
        String::Utf8Value str(isolate, v->ToString(context).ToLocalChecked());
        trace_config->AddIncludedCategory(*str);
      }
      return v8_array->Length();
    }
    return 0;
  }
};

}  // namespace

static platform::tracing::TraceConfig* CreateTraceConfigFromJSON(
    v8::Isolate* isolate, const char* json_str) {
  platform::tracing::TraceConfig* trace_config =
      new platform::tracing::TraceConfig();
  TraceConfigParser::FillTraceConfig(isolate, trace_config, json_str);
  return trace_config;
}

}  // namespace tracing

class ExternalOwningOneByteStringResource
    : public String::ExternalOneByteStringResource {
 public:
  ExternalOwningOneByteStringResource() = default;
  ExternalOwningOneByteStringResource(
      std::unique_ptr<base::OS::MemoryMappedFile> file)
      : file_(std::move(file)) {}
  const char* data() const override {
    return static_cast<char*>(file_->memory());
  }
  size_t length() const override { return file_->size(); }

 private:
  std::unique_ptr<base::OS::MemoryMappedFile> file_;
};

CounterMap* Shell::counter_map_;
base::OS::MemoryMappedFile* Shell::counters_file_ = nullptr;
CounterCollection Shell::local_counters_;
CounterCollection* Shell::counters_ = &local_counters_;
base::LazyMutex Shell::context_mutex_;
const base::TimeTicks Shell::kInitialTicks =
    base::TimeTicks::HighResolutionNow();
Global<Function> Shell::stringify_function_;
base::LazyMutex Shell::workers_mutex_;
bool Shell::allow_new_workers_ = true;
std::unordered_set<std::shared_ptr<Worker>> Shell::running_workers_;
std::atomic<bool> Shell::script_executed_{false};
std::atomic<bool> Shell::valid_fuzz_script_{false};
base::LazyMutex Shell::isolate_status_lock_;
std::map<v8::Isolate*, bool> Shell::isolate_status_;
std::map<v8::Isolate*, int> Shell::isolate_running_streaming_tasks_;
base::LazyMutex Shell::cached_code_mutex_;
std::map<std::string, std::unique_ptr<ScriptCompiler::CachedData>>
    Shell::cached_code_map_;
std::atomic<int> Shell::unhandled_promise_rejections_{0};

Global<Context> Shell::evaluation_context_;
ArrayBuffer::Allocator* Shell::array_buffer_allocator;
bool check_d8_flag_contradictions = true;
ShellOptions Shell::options;
base::OnceType Shell::quit_once_ = V8_ONCE_INIT;

ScriptCompiler::CachedData* Shell::LookupCodeCache(Isolate* isolate,
                                                   Local<Value> source) {
  base::MutexGuard lock_guard(cached_code_mutex_.Pointer());
  CHECK(source->IsString());
  v8::String::Utf8Value key(isolate, source);
  DCHECK(*key);
  auto entry = cached_code_map_.find(*key);
  if (entry != cached_code_map_.end() && entry->second) {
    int length = entry->second->length;
    uint8_t* cache = new uint8_t[length];
    base::Memcpy(cache, entry->second->data, length);
    ScriptCompiler::CachedData* cached_data = new ScriptCompiler::CachedData(
        cache, length, ScriptCompiler::CachedData::BufferOwned);
    return cached_data;
  }
  return nullptr;
}

void Shell::StoreInCodeCache(Isolate* isolate, Local<Value> source,
                             const ScriptCompiler::CachedData* cache_data) {
  base::MutexGuard lock_guard(cached_code_mutex_.Pointer());
  CHECK(source->IsString());
  if (cache_data == nullptr) return;
  v8::String::Utf8Value key(isolate, source);
  DCHECK(*key);
  int length = cache_data->length;
  uint8_t* cache = new uint8_t[length];
  base::Memcpy(cache, cache_data->data, length);
  cached_code_map_[*key] = std::unique_ptr<ScriptCompiler::CachedData>(
      new ScriptCompiler::CachedData(cache, length,
                                     ScriptCompiler::CachedData::BufferOwned));
}

// Dummy external source stream which returns the whole source in one go.
// TODO(leszeks): Also test chunking the data.
class DummySourceStream : public v8::ScriptCompiler::ExternalSourceStream {
 public:
  explicit DummySourceStream(Local<String> source) : done_(false) {
    source_buffer_ = Utils::OpenHandle(*source)->ToCString(
        i::ALLOW_NULLS, i::FAST_STRING_TRAVERSAL, &source_length_);
  }

  size_t GetMoreData(const uint8_t** src) override {
    if (done_) {
      return 0;
    }
    *src = reinterpret_cast<uint8_t*>(source_buffer_.release());
    done_ = true;

    return source_length_;
  }

 private:
  int source_length_;
  std::unique_ptr<char[]> source_buffer_;
  bool done_;
};

class StreamingCompileTask final : public v8::Task {
 public:
  StreamingCompileTask(Isolate* isolate,
                       v8::ScriptCompiler::StreamedSource* streamed_source,
                       v8::ScriptType type)
      : isolate_(isolate),
        script_streaming_task_(v8::ScriptCompiler::StartStreaming(
            isolate, streamed_source, type)) {
    Shell::NotifyStartStreamingTask(isolate_);
  }

  void Run() override {
    script_streaming_task_->Run();
    // Signal that the task has finished using the task runner to wake the
    // message loop.
    Shell::PostForegroundTask(isolate_, std::make_unique<FinishTask>(isolate_));
  }

 private:
  class FinishTask final : public v8::Task {
   public:
    explicit FinishTask(Isolate* isolate) : isolate_(isolate) {}
    void Run() final { Shell::NotifyFinishStreamingTask(isolate_); }
    Isolate* isolate_;
  };

  Isolate* isolate_;
  std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>
      script_streaming_task_;
};

namespace {
template <class T>
MaybeLocal<T> CompileStreamed(Local<Context> context,
                              ScriptCompiler::StreamedSource* v8_source,
                              Local<String> full_source_string,
                              const ScriptOrigin& origin) {}

template <>
MaybeLocal<Script> CompileStreamed(Local<Context> context,
                                   ScriptCompiler::StreamedSource* v8_source,
                                   Local<String> full_source_string,
                                   const ScriptOrigin& origin) {
  return ScriptCompiler::Compile(context, v8_source, full_source_string,
                                 origin);
}

template <>
MaybeLocal<Module> CompileStreamed(Local<Context> context,
                                   ScriptCompiler::StreamedSource* v8_source,
                                   Local<String> full_source_string,
                                   const ScriptOrigin& origin) {
  return ScriptCompiler::CompileModule(context, v8_source, full_source_string,
                                       origin);
}

template <class T>
MaybeLocal<T> Compile(Local<Context> context, ScriptCompiler::Source* source,
                      ScriptCompiler::CompileOptions options) {}
template <>
MaybeLocal<Script> Compile(Local<Context> context,
                           ScriptCompiler::Source* source,
                           ScriptCompiler::CompileOptions options) {
  return ScriptCompiler::Compile(context, source, options);
}

template <>
MaybeLocal<Module> Compile(Local<Context> context,
                           ScriptCompiler::Source* source,
                           ScriptCompiler::CompileOptions options) {
  return ScriptCompiler::CompileModule(context->GetIsolate(), source, options);
}

}  // namespace

template <class T>
MaybeLocal<T> Shell::CompileString(Isolate* isolate, Local<Context> context,
                                   Local<String> source,
                                   const ScriptOrigin& origin) {
  if (options.streaming_compile) {
    v8::ScriptCompiler::StreamedSource streamed_source(
        std::make_unique<DummySourceStream>(source),
        v8::ScriptCompiler::StreamedSource::UTF8);
    PostBlockingBackgroundTask(std::make_unique<StreamingCompileTask>(
        isolate, &streamed_source,
        std::is_same<T, Module>::value ? v8::ScriptType::kModule
                                       : v8::ScriptType::kClassic));
    // Pump the loop until the streaming task completes.
    Shell::CompleteMessageLoop(isolate);
    return CompileStreamed<T>(context, &streamed_source, source, origin);
  }

  ScriptCompiler::CachedData* cached_code = nullptr;
  if (options.compile_options == ScriptCompiler::kConsumeCodeCache) {
    cached_code = LookupCodeCache(isolate, source);
  }
  ScriptCompiler::Source script_source(source, origin, cached_code);
  MaybeLocal<T> result =
      Compile<T>(context, &script_source,
                 cached_code ? ScriptCompiler::kConsumeCodeCache
                             : ScriptCompiler::kNoCompileOptions);
  if (cached_code) CHECK(!cached_code->rejected);
  return result;
}

// Executes a string within the current v8 context.
bool Shell::ExecuteString(Isolate* isolate, Local<String> source,
                          Local<Value> name, PrintResult print_result,
                          ReportExceptions report_exceptions,
                          ProcessMessageQueue process_message_queue) {
  if (i::FLAG_parse_only) {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    i::VMState<PARSER> state(i_isolate);
    i::Handle<i::String> str = Utils::OpenHandle(*(source));

    // Set up ParseInfo.
    i::UnoptimizedCompileState compile_state(i_isolate);

    i::UnoptimizedCompileFlags flags =
        i::UnoptimizedCompileFlags::ForToplevelCompile(
            i_isolate, true, i::construct_language_mode(i::FLAG_use_strict),
            i::REPLMode::kNo);

    if (options.compile_options == v8::ScriptCompiler::kEagerCompile) {
      flags.set_is_eager(true);
    }

    i::ParseInfo parse_info(i_isolate, flags, &compile_state);

    i::Handle<i::Script> script = parse_info.CreateScript(
        i_isolate, str, i::kNullMaybeHandle, ScriptOriginOptions());
    if (!i::parsing::ParseProgram(&parse_info, script, i_isolate,
                                  i::parsing::ReportStatisticsMode::kYes)) {
      parse_info.pending_error_handler()->PrepareErrors(
          i_isolate, parse_info.ast_value_factory());
      parse_info.pending_error_handler()->ReportErrors(i_isolate, script);

      fprintf(stderr, "Failed parsing\n");
      return false;
    }
    return true;
  }

  HandleScope handle_scope(isolate);
  TryCatch try_catch(isolate);
  try_catch.SetVerbose(report_exceptions == kReportExceptions);

  MaybeLocal<Value> maybe_result;
  bool success = true;
  {
    PerIsolateData* data = PerIsolateData::Get(isolate);
    Local<Context> realm =
        Local<Context>::New(isolate, data->realms_[data->realm_current_]);
    Context::Scope context_scope(realm);
    MaybeLocal<Script> maybe_script;
    Local<Context> context(isolate->GetCurrentContext());
    ScriptOrigin origin(isolate, name);

    Local<Script> script;
    if (!CompileString<Script>(isolate, context, source, origin)
             .ToLocal(&script)) {
      return false;
    }

    if (options.code_cache_options ==
        ShellOptions::CodeCacheOptions::kProduceCache) {
      // Serialize and store it in memory for the next execution.
      ScriptCompiler::CachedData* cached_data =
          ScriptCompiler::CreateCodeCache(script->GetUnboundScript());
      StoreInCodeCache(isolate, source, cached_data);
      delete cached_data;
    }
    maybe_result = script->Run(realm);
    if (options.code_cache_options ==
        ShellOptions::CodeCacheOptions::kProduceCacheAfterExecute) {
      // Serialize and store it in memory for the next execution.
      ScriptCompiler::CachedData* cached_data =
          ScriptCompiler::CreateCodeCache(script->GetUnboundScript());
      StoreInCodeCache(isolate, source, cached_data);
      delete cached_data;
    }
    if (process_message_queue) {
      if (!EmptyMessageQueues(isolate)) success = false;
      if (!HandleUnhandledPromiseRejections(isolate)) success = false;
    }
    data->realm_current_ = data->realm_switch_;
  }
  Local<Value> result;
  if (!maybe_result.ToLocal(&result)) {
    DCHECK(try_catch.HasCaught());
    return false;
  }
  // It's possible that a FinalizationRegistry cleanup task threw an error.
  if (try_catch.HasCaught()) success = false;
  if (print_result) {
    if (options.test_shell) {
      if (!result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        v8::String::Utf8Value str(isolate, result);
        fwrite(*str, sizeof(**str), str.length(), stdout);
        printf("\n");
      }
    } else {
      v8::String::Utf8Value str(isolate, Stringify(isolate, result));
      fwrite(*str, sizeof(**str), str.length(), stdout);
      printf("\n");
    }
  }
  return success;
}

namespace {

std::string ToSTLString(Isolate* isolate, Local<String> v8_str) {
  String::Utf8Value utf8(isolate, v8_str);
  // Should not be able to fail since the input is a String.
  CHECK(*utf8);
  return *utf8;
}

bool IsAbsolutePath(const std::string& path) {
#if defined(_WIN32) || defined(_WIN64)
  // This is an incorrect approximation, but should
  // work for all our test-running cases.
  return path.find(':') != std::string::npos;
#else
  return path[0] == '/';
#endif
}

std::string GetWorkingDirectory() {
#if defined(_WIN32) || defined(_WIN64)
  char system_buffer[MAX_PATH];
  // Unicode paths are unsupported, which is fine as long as
  // the test directory doesn't include any such paths.
  DWORD len = GetCurrentDirectoryA(MAX_PATH, system_buffer);
  CHECK_GT(len, 0);
  return system_buffer;
#else
  char curdir[PATH_MAX];
  CHECK_NOT_NULL(getcwd(curdir, PATH_MAX));
  return curdir;
#endif
}

// Returns the directory part of path, without the trailing '/'.
std::string DirName(const std::string& path) {
  DCHECK(IsAbsolutePath(path));
  size_t last_slash = path.find_last_of('/');
  DCHECK(last_slash != std::string::npos);
  return path.substr(0, last_slash);
}

// Resolves path to an absolute path if necessary, and does some
// normalization (eliding references to the current directory
// and replacing backslashes with slashes).
std::string NormalizePath(const std::string& path,
                          const std::string& dir_name) {
  std::string absolute_path;
  if (IsAbsolutePath(path)) {
    absolute_path = path;
  } else {
    absolute_path = dir_name + '/' + path;
  }
  std::replace(absolute_path.begin(), absolute_path.end(), '\\', '/');
  std::vector<std::string> segments;
  std::istringstream segment_stream(absolute_path);
  std::string segment;
  while (std::getline(segment_stream, segment, '/')) {
    if (segment == "..") {
      segments.pop_back();
    } else if (segment != ".") {
      segments.push_back(segment);
    }
  }
  // Join path segments.
  std::ostringstream os;
  std::copy(segments.begin(), segments.end() - 1,
            std::ostream_iterator<std::string>(os, "/"));
  os << *segments.rbegin();
  return os.str();
}

// Per-context Module data, allowing sharing of module maps
// across top-level module loads.
class ModuleEmbedderData {
 private:
  class ModuleGlobalHash {
   public:
    explicit ModuleGlobalHash(Isolate* isolate) : isolate_(isolate) {}
    size_t operator()(const Global<Module>& module) const {
      return module.Get(isolate_)->GetIdentityHash();
    }

   private:
    Isolate* isolate_;
  };

 public:
  explicit ModuleEmbedderData(Isolate* isolate)
      : module_to_specifier_map(10, ModuleGlobalHash(isolate)),
        json_module_to_parsed_json_map(10, ModuleGlobalHash(isolate)) {}

  static ModuleType ModuleTypeFromImportAssertions(
      Local<Context> context, Local<FixedArray> import_assertions,
      bool hasPositions) {
    Isolate* isolate = context->GetIsolate();
    const int kV8AssertionEntrySize = hasPositions ? 3 : 2;
    for (int i = 0; i < import_assertions->Length();
         i += kV8AssertionEntrySize) {
      Local<String> v8_assertion_key =
          import_assertions->Get(context, i).As<v8::String>();
      std::string assertion_key = ToSTLString(isolate, v8_assertion_key);

      if (assertion_key == "type") {
        Local<String> v8_assertion_value =
            import_assertions->Get(context, i + 1).As<String>();
        std::string assertion_value = ToSTLString(isolate, v8_assertion_value);
        if (assertion_value == "json") {
          return ModuleType::kJSON;
        } else {
          // JSON is currently the only supported non-JS type
          return ModuleType::kInvalid;
        }
      }
    }

    // If no type is asserted, default to JS.
    return ModuleType::kJavaScript;
  }

  // Map from (normalized module specifier, module type) pair to Module.
  std::map<std::pair<std::string, ModuleType>, Global<Module>> module_map;
  // Map from Module to its URL as defined in the ScriptOrigin
  std::unordered_map<Global<Module>, std::string, ModuleGlobalHash>
      module_to_specifier_map;
  // Map from JSON Module to its parsed content, for use in module
  // JSONModuleEvaluationSteps
  std::unordered_map<Global<Module>, Global<Value>, ModuleGlobalHash>
      json_module_to_parsed_json_map;
};

enum { kModuleEmbedderDataIndex, kInspectorClientIndex };

void InitializeModuleEmbedderData(Local<Context> context) {
  context->SetAlignedPointerInEmbedderData(
      kModuleEmbedderDataIndex, new ModuleEmbedderData(context->GetIsolate()));
}

ModuleEmbedderData* GetModuleDataFromContext(Local<Context> context) {
  return static_cast<ModuleEmbedderData*>(
      context->GetAlignedPointerFromEmbedderData(kModuleEmbedderDataIndex));
}

void DisposeModuleEmbedderData(Local<Context> context) {
  delete GetModuleDataFromContext(context);
  context->SetAlignedPointerInEmbedderData(kModuleEmbedderDataIndex, nullptr);
}

MaybeLocal<Module> ResolveModuleCallback(Local<Context> context,
                                         Local<String> specifier,
                                         Local<FixedArray> import_assertions,
                                         Local<Module> referrer) {
  Isolate* isolate = context->GetIsolate();
  ModuleEmbedderData* d = GetModuleDataFromContext(context);
  auto specifier_it =
      d->module_to_specifier_map.find(Global<Module>(isolate, referrer));
  CHECK(specifier_it != d->module_to_specifier_map.end());
  std::string absolute_path = NormalizePath(ToSTLString(isolate, specifier),
                                            DirName(specifier_it->second));
  ModuleType module_type = ModuleEmbedderData::ModuleTypeFromImportAssertions(
      context, import_assertions, true);
  auto module_it =
      d->module_map.find(std::make_pair(absolute_path, module_type));
  CHECK(module_it != d->module_map.end());
  return module_it->second.Get(isolate);
}

}  // anonymous namespace

MaybeLocal<Module> Shell::FetchModuleTree(Local<Module> referrer,
                                          Local<Context> context,
                                          const std::string& file_name,
                                          ModuleType module_type) {
  DCHECK(IsAbsolutePath(file_name));
  Isolate* isolate = context->GetIsolate();
  Local<String> source_text = ReadFile(isolate, file_name.c_str());
  if (source_text.IsEmpty() && options.fuzzy_module_file_extensions) {
    std::string fallback_file_name = file_name + ".js";
    source_text = ReadFile(isolate, fallback_file_name.c_str());
    if (source_text.IsEmpty()) {
      fallback_file_name = file_name + ".mjs";
      source_text = ReadFile(isolate, fallback_file_name.c_str());
    }
  }

  ModuleEmbedderData* d = GetModuleDataFromContext(context);
  if (source_text.IsEmpty()) {
    std::string msg = "d8: Error reading  module from " + file_name;
    if (!referrer.IsEmpty()) {
      auto specifier_it =
          d->module_to_specifier_map.find(Global<Module>(isolate, referrer));
      CHECK(specifier_it != d->module_to_specifier_map.end());
      msg += "\n    imported by " + specifier_it->second;
    }
    Throw(isolate, msg.c_str());
    return MaybeLocal<Module>();
  }
  ScriptOrigin origin(
      isolate, String::NewFromUtf8(isolate, file_name.c_str()).ToLocalChecked(),
      0, 0, false, -1, Local<Value>(), false, false, true);

  Local<Module> module;
  if (module_type == ModuleType::kJavaScript) {
    ScriptCompiler::Source source(source_text, origin);
    if (!CompileString<Module>(isolate, context, source_text, origin)
             .ToLocal(&module)) {
      return MaybeLocal<Module>();
    }
  } else if (module_type == ModuleType::kJSON) {
    Local<Value> parsed_json;
    if (!v8::JSON::Parse(context, source_text).ToLocal(&parsed_json)) {
      return MaybeLocal<Module>();
    }

    std::vector<Local<String>> export_names{
        String::NewFromUtf8(isolate, "default").ToLocalChecked()};

    module = v8::Module::CreateSyntheticModule(
        isolate,
        String::NewFromUtf8(isolate, file_name.c_str()).ToLocalChecked(),
        export_names, Shell::JSONModuleEvaluationSteps);

    CHECK(d->json_module_to_parsed_json_map
              .insert(std::make_pair(Global<Module>(isolate, module),
                                     Global<Value>(isolate, parsed_json)))
              .second);
  } else {
    UNREACHABLE();
  }

  CHECK(d->module_map
            .insert(std::make_pair(std::make_pair(file_name, module_type),
                                   Global<Module>(isolate, module)))
            .second);
  CHECK(d->module_to_specifier_map
            .insert(std::make_pair(Global<Module>(isolate, module), file_name))
            .second);

  std::string dir_name = DirName(file_name);

  Local<FixedArray> module_requests = module->GetModuleRequests();
  for (int i = 0, length = module_requests->Length(); i < length; ++i) {
    Local<ModuleRequest> module_request =
        module_requests->Get(context, i).As<ModuleRequest>();
    Local<String> name = module_request->GetSpecifier();
    std::string absolute_path =
        NormalizePath(ToSTLString(isolate, name), dir_name);
    Local<FixedArray> import_assertions = module_request->GetImportAssertions();
    ModuleType request_module_type =
        ModuleEmbedderData::ModuleTypeFromImportAssertions(
            context, import_assertions, true);

    if (request_module_type == ModuleType::kInvalid) {
      Throw(isolate, "Invalid module type was asserted");
      return MaybeLocal<Module>();
    }

    if (d->module_map.count(
            std::make_pair(absolute_path, request_module_type))) {
      continue;
    }

    if (FetchModuleTree(module, context, absolute_path, request_module_type)
            .IsEmpty()) {
      return MaybeLocal<Module>();
    }
  }

  return module;
}

MaybeLocal<Value> Shell::JSONModuleEvaluationSteps(Local<Context> context,
                                                   Local<Module> module) {
  Isolate* isolate = context->GetIsolate();

  ModuleEmbedderData* d = GetModuleDataFromContext(context);
  auto json_value_it =
      d->json_module_to_parsed_json_map.find(Global<Module>(isolate, module));
  CHECK(json_value_it != d->json_module_to_parsed_json_map.end());
  Local<Value> json_value = json_value_it->second.Get(isolate);

  TryCatch try_catch(isolate);
  Maybe<bool> result = module->SetSyntheticModuleExport(
      isolate,
      String::NewFromUtf8Literal(isolate, "default",
                                 NewStringType::kInternalized),
      json_value);

  // Setting the default export should never fail.
  CHECK(!try_catch.HasCaught());
  CHECK(!result.IsNothing() && result.FromJust());

  if (i::FLAG_harmony_top_level_await) {
    Local<Promise::Resolver> resolver =
        Promise::Resolver::New(context).ToLocalChecked();
    resolver->Resolve(context, Undefined(isolate)).ToChecked();
    return resolver->GetPromise();
  }

  return Undefined(isolate);
}

struct DynamicImportData {
  DynamicImportData(Isolate* isolate_, Local<String> referrer_,
                    Local<String> specifier_,
                    Local<FixedArray> import_assertions_,
                    Local<Promise::Resolver> resolver_)
      : isolate(isolate_) {
    referrer.Reset(isolate, referrer_);
    specifier.Reset(isolate, specifier_);
    import_assertions.Reset(isolate, import_assertions_);
    resolver.Reset(isolate, resolver_);
  }

  Isolate* isolate;
  Global<String> referrer;
  Global<String> specifier;
  Global<FixedArray> import_assertions;
  Global<Promise::Resolver> resolver;
};

namespace {
struct ModuleResolutionData {
  ModuleResolutionData(Isolate* isolate_, Local<Value> module_namespace_,
                       Local<Promise::Resolver> resolver_)
      : isolate(isolate_) {
    module_namespace.Reset(isolate, module_namespace_);
    resolver.Reset(isolate, resolver_);
  }

  Isolate* isolate;
  Global<Value> module_namespace;
  Global<Promise::Resolver> resolver;
};

}  // namespace

void Shell::ModuleResolutionSuccessCallback(
    const FunctionCallbackInfo<Value>& info) {
  std::unique_ptr<ModuleResolutionData> module_resolution_data(
      static_cast<ModuleResolutionData*>(
          info.Data().As<v8::External>()->Value()));
  Isolate* isolate(module_resolution_data->isolate);
  HandleScope handle_scope(isolate);

  Local<Promise::Resolver> resolver(
      module_resolution_data->resolver.Get(isolate));
  Local<Value> module_namespace(
      module_resolution_data->module_namespace.Get(isolate));

  PerIsolateData* data = PerIsolateData::Get(isolate);
  Local<Context> realm = data->realms_[data->realm_current_].Get(isolate);
  Context::Scope context_scope(realm);

  resolver->Resolve(realm, module_namespace).ToChecked();
}

void Shell::ModuleResolutionFailureCallback(
    const FunctionCallbackInfo<Value>& info) {
  std::unique_ptr<ModuleResolutionData> module_resolution_data(
      static_cast<ModuleResolutionData*>(
          info.Data().As<v8::External>()->Value()));
  Isolate* isolate(module_resolution_data->isolate);
  HandleScope handle_scope(isolate);

  Local<Promise::Resolver> resolver(
      module_resolution_data->resolver.Get(isolate));

  PerIsolateData* data = PerIsolateData::Get(isolate);
  Local<Context> realm = data->realms_[data->realm_current_].Get(isolate);
  Context::Scope context_scope(realm);

  DCHECK_EQ(info.Length(), 1);
  resolver->Reject(realm, info[0]).ToChecked();
}

MaybeLocal<Promise> Shell::HostImportModuleDynamically(
    Local<Context> context, Local<ScriptOrModule> referrer,
    Local<String> specifier, Local<FixedArray> import_assertions) {
  Isolate* isolate = context->GetIsolate();

  MaybeLocal<Promise::Resolver> maybe_resolver =
      Promise::Resolver::New(context);
  Local<Promise::Resolver> resolver;
  if (maybe_resolver.ToLocal(&resolver)) {
    DynamicImportData* data =
        new DynamicImportData(isolate, referrer->GetResourceName().As<String>(),
                              specifier, import_assertions, resolver);
    PerIsolateData::Get(isolate)->AddDynamicImportData(data);
    isolate->EnqueueMicrotask(Shell::DoHostImportModuleDynamically, data);
    return resolver->GetPromise();
  }

  return MaybeLocal<Promise>();
}

void Shell::HostInitializeImportMetaObject(Local<Context> context,
                                           Local<Module> module,
                                           Local<Object> meta) {
  Isolate* isolate = context->GetIsolate();
  HandleScope handle_scope(isolate);

  ModuleEmbedderData* d = GetModuleDataFromContext(context);
  auto specifier_it =
      d->module_to_specifier_map.find(Global<Module>(isolate, module));
  CHECK(specifier_it != d->module_to_specifier_map.end());

  Local<String> url_key =
      String::NewFromUtf8Literal(isolate, "url", NewStringType::kInternalized);
  Local<String> url = String::NewFromUtf8(isolate, specifier_it->second.c_str())
                          .ToLocalChecked();
  meta->CreateDataProperty(context, url_key, url).ToChecked();
}

void Shell::DoHostImportModuleDynamically(void* import_data) {
  DynamicImportData* import_data_ =
      static_cast<DynamicImportData*>(import_data);

  Isolate* isolate(import_data_->isolate);
  HandleScope handle_scope(isolate);

  Local<String> referrer(import_data_->referrer.Get(isolate));
  Local<String> specifier(import_data_->specifier.Get(isolate));
  Local<FixedArray> import_assertions(
      import_data_->import_assertions.Get(isolate));
  Local<Promise::Resolver> resolver(import_data_->resolver.Get(isolate));

  PerIsolateData* data = PerIsolateData::Get(isolate);
  PerIsolateData::Get(isolate)->DeleteDynamicImportData(import_data_);

  Local<Context> realm = data->realms_[data->realm_current_].Get(isolate);
  Context::Scope context_scope(realm);

  ModuleType module_type = ModuleEmbedderData::ModuleTypeFromImportAssertions(
      realm, import_assertions, false);

  TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  if (module_type == ModuleType::kInvalid) {
    Throw(isolate, "Invalid module type was asserted");
    CHECK(try_catch.HasCaught());
    resolver->Reject(realm, try_catch.Exception()).ToChecked();
    return;
  }

  std::string source_url = ToSTLString(isolate, referrer);
  std::string dir_name =
      DirName(NormalizePath(source_url, GetWorkingDirectory()));
  std::string file_name = ToSTLString(isolate, specifier);
  std::string absolute_path = NormalizePath(file_name, dir_name);

  ModuleEmbedderData* d = GetModuleDataFromContext(realm);
  Local<Module> root_module;
  auto module_it =
      d->module_map.find(std::make_pair(absolute_path, module_type));
  if (module_it != d->module_map.end()) {
    root_module = module_it->second.Get(isolate);
  } else if (!FetchModuleTree(Local<Module>(), realm, absolute_path,
                              module_type)
                  .ToLocal(&root_module)) {
    CHECK(try_catch.HasCaught());
    resolver->Reject(realm, try_catch.Exception()).ToChecked();
    return;
  }

  MaybeLocal<Value> maybe_result;
  if (root_module->InstantiateModule(realm, ResolveModuleCallback)
          .FromMaybe(false)) {
    maybe_result = root_module->Evaluate(realm);
    CHECK_IMPLIES(i::FLAG_harmony_top_level_await, !maybe_result.IsEmpty());
    EmptyMessageQueues(isolate);
  }

  Local<Value> result;
  if (!maybe_result.ToLocal(&result)) {
    DCHECK(try_catch.HasCaught());
    resolver->Reject(realm, try_catch.Exception()).ToChecked();
    return;
  }

  Local<Value> module_namespace = root_module->GetModuleNamespace();
  if (i::FLAG_harmony_top_level_await) {
    Local<Promise> result_promise(result.As<Promise>());
    if (result_promise->State() == Promise::kRejected) {
      resolver->Reject(realm, result_promise->Result()).ToChecked();
      return;
    }

    // Setup callbacks, and then chain them to the result promise.
    // ModuleResolutionData will be deleted by the callbacks.
    auto module_resolution_data =
        new ModuleResolutionData(isolate, module_namespace, resolver);
    Local<v8::External> edata = External::New(isolate, module_resolution_data);
    Local<Function> callback_success;
    CHECK(Function::New(realm, ModuleResolutionSuccessCallback, edata)
              .ToLocal(&callback_success));
    Local<Function> callback_failure;
    CHECK(Function::New(realm, ModuleResolutionFailureCallback, edata)
              .ToLocal(&callback_failure));
    result_promise->Then(realm, callback_success, callback_failure)
        .ToLocalChecked();
  } else {
    // TODO(cbruni): Clean up exception handling after introducing new
    // API for evaluating async modules.
    DCHECK(!try_catch.HasCaught());
    resolver->Resolve(realm, module_namespace).ToChecked();
  }
}

bool Shell::ExecuteModule(Isolate* isolate, const char* file_name) {
  HandleScope handle_scope(isolate);

  PerIsolateData* data = PerIsolateData::Get(isolate);
  Local<Context> realm = data->realms_[data->realm_current_].Get(isolate);
  Context::Scope context_scope(realm);

  std::string absolute_path = NormalizePath(file_name, GetWorkingDirectory());

  // Use a non-verbose TryCatch and report exceptions manually using
  // Shell::ReportException, because some errors (such as file errors) are
  // thrown without entering JS and thus do not trigger
  // isolate->ReportPendingMessages().
  TryCatch try_catch(isolate);

  Local<Module> root_module;

  if (!FetchModuleTree(Local<Module>(), realm, absolute_path,
                       ModuleType::kJavaScript)
           .ToLocal(&root_module)) {
    CHECK(try_catch.HasCaught());
    ReportException(isolate, &try_catch);
    return false;
  }

  MaybeLocal<Value> maybe_result;
  if (root_module->InstantiateModule(realm, ResolveModuleCallback)
          .FromMaybe(false)) {
    maybe_result = root_module->Evaluate(realm);
    CHECK_IMPLIES(i::FLAG_harmony_top_level_await, !maybe_result.IsEmpty());
    EmptyMessageQueues(isolate);
  }
  Local<Value> result;
  if (!maybe_result.ToLocal(&result)) {
    DCHECK(try_catch.HasCaught());
    ReportException(isolate, &try_catch);
    return false;
  }
  if (i::FLAG_harmony_top_level_await) {
    // Loop until module execution finishes
    // TODO(cbruni): This is a bit wonky. "Real" engines would not be
    // able to just busy loop waiting for execution to finish.
    Local<Promise> result_promise(result.As<Promise>());
    while (result_promise->State() == Promise::kPending) {
      isolate->PerformMicrotaskCheckpoint();
    }

    if (result_promise->State() == Promise::kRejected) {
      // If the exception has been caught by the promise pipeline, we rethrow
      // here in order to ReportException.
      // TODO(cbruni): Clean this up after we create a new API for the case
      // where TLA is enabled.
      if (!try_catch.HasCaught()) {
        isolate->ThrowException(result_promise->Result());
      } else {
        DCHECK_EQ(try_catch.Exception(), result_promise->Result());
      }
      ReportException(isolate, &try_catch);
      return false;
    }
  }

  DCHECK(!try_catch.HasCaught());
  return true;
}

PerIsolateData::PerIsolateData(Isolate* isolate)
    : isolate_(isolate), realms_(nullptr) {
  isolate->SetData(0, this);
  if (i::FLAG_expose_async_hooks) {
    async_hooks_wrapper_ = new AsyncHooks(isolate);
  }
  ignore_unhandled_promises_ = false;
}

PerIsolateData::~PerIsolateData() {
  isolate_->SetData(0, nullptr);  // Not really needed, just to be sure...
  if (i::FLAG_expose_async_hooks) {
    delete async_hooks_wrapper_;  // This uses the isolate
  }
#if defined(LEAK_SANITIZER)
  for (DynamicImportData* data : import_data_) {
    delete data;
  }
#endif
}

void PerIsolateData::SetTimeout(Local<Function> callback,
                                Local<Context> context) {
  set_timeout_callbacks_.emplace(isolate_, callback);
  set_timeout_contexts_.emplace(isolate_, context);
}

MaybeLocal<Function> PerIsolateData::GetTimeoutCallback() {
  if (set_timeout_callbacks_.empty()) return MaybeLocal<Function>();
  Local<Function> result = set_timeout_callbacks_.front().Get(isolate_);
  set_timeout_callbacks_.pop();
  return result;
}

MaybeLocal<Context> PerIsolateData::GetTimeoutContext() {
  if (set_timeout_contexts_.empty()) return MaybeLocal<Context>();
  Local<Context> result = set_timeout_contexts_.front().Get(isolate_);
  set_timeout_contexts_.pop();
  return result;
}

void PerIsolateData::RemoveUnhandledPromise(Local<Promise> promise) {
  if (ignore_unhandled_promises_) return;
  // Remove handled promises from the list
  DCHECK_EQ(promise->GetIsolate(), isolate_);
  for (auto it = unhandled_promises_.begin(); it != unhandled_promises_.end();
       ++it) {
    v8::Local<v8::Promise> unhandled_promise = std::get<0>(*it).Get(isolate_);
    if (unhandled_promise == promise) {
      unhandled_promises_.erase(it--);
    }
  }
}

void PerIsolateData::AddUnhandledPromise(Local<Promise> promise,
                                         Local<Message> message,
                                         Local<Value> exception) {
  if (ignore_unhandled_promises_) return;
  DCHECK_EQ(promise->GetIsolate(), isolate_);
  unhandled_promises_.emplace_back(v8::Global<v8::Promise>(isolate_, promise),
                                   v8::Global<v8::Message>(isolate_, message),
                                   v8::Global<v8::Value>(isolate_, exception));
}

int PerIsolateData::HandleUnhandledPromiseRejections() {
  // Avoid recursive calls to HandleUnhandledPromiseRejections.
  if (ignore_unhandled_promises_) return 0;
  ignore_unhandled_promises_ = true;
  v8::HandleScope scope(isolate_);
  // Ignore promises that get added during error reporting.
  size_t i = 0;
  for (; i < unhandled_promises_.size(); i++) {
    const auto& tuple = unhandled_promises_[i];
    Local<v8::Message> message = std::get<1>(tuple).Get(isolate_);
    Local<v8::Value> value = std::get<2>(tuple).Get(isolate_);
    Shell::ReportException(isolate_, message, value);
  }
  unhandled_promises_.clear();
  ignore_unhandled_promises_ = false;
  return static_cast<int>(i);
}

void PerIsolateData::AddDynamicImportData(DynamicImportData* data) {
#if defined(LEAK_SANITIZER)
  import_data_.insert(data);
#endif
}
void PerIsolateData::DeleteDynamicImportData(DynamicImportData* data) {
#if defined(LEAK_SANITIZER)
  import_data_.erase(data);
#endif
  delete data;
}

PerIsolateData::RealmScope::RealmScope(PerIsolateData* data) : data_(data) {
  data_->realm_count_ = 1;
  data_->realm_current_ = 0;
  data_->realm_switch_ = 0;
  data_->realms_ = new Global<Context>[1];
  data_->realms_[0].Reset(data_->isolate_,
                          data_->isolate_->GetEnteredOrMicrotaskContext());
}

PerIsolateData::RealmScope::~RealmScope() {
  // Drop realms to avoid keeping them alive. We don't dispose the
  // module embedder data for the first realm here, but instead do
  // it in RunShell or in RunMain, if not running in interactive mode
  for (int i = 1; i < data_->realm_count_; ++i) {
    Global<Context>& realm = data_->realms_[i];
    if (realm.IsEmpty()) continue;
    DisposeModuleEmbedderData(realm.Get(data_->isolate_));
  }
  data_->realm_count_ = 0;
  delete[] data_->realms_;
}

int PerIsolateData::RealmFind(Local<Context> context) {
  for (int i = 0; i < realm_count_; ++i) {
    if (realms_[i] == context) return i;
  }
  return -1;
}

int PerIsolateData::RealmIndexOrThrow(
    const v8::FunctionCallbackInfo<v8::Value>& args, int arg_offset) {
  if (args.Length() < arg_offset || !args[arg_offset]->IsNumber()) {
    Throw(args.GetIsolate(), "Invalid argument");
    return -1;
  }
  int index = args[arg_offset]
                  ->Int32Value(args.GetIsolate()->GetCurrentContext())
                  .FromMaybe(-1);
  if (index < 0 || index >= realm_count_ || realms_[index].IsEmpty()) {
    Throw(args.GetIsolate(), "Invalid realm index");
    return -1;
  }
  return index;
}

// performance.now() returns a time stamp as double, measured in milliseconds.
// When FLAG_verify_predictable mode is enabled it returns result of
// v8::Platform::MonotonicallyIncreasingTime().
void Shell::PerformanceNow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (i::FLAG_verify_predictable) {
    args.GetReturnValue().Set(g_platform->MonotonicallyIncreasingTime());
  } else {
    base::TimeDelta delta =
        base::TimeTicks::HighResolutionNow() - kInitialTicks;
    args.GetReturnValue().Set(delta.InMillisecondsF());
  }
}

// performance.measureMemory() implements JavaScript Memory API proposal.
// See https://github.com/ulan/javascript-agent-memory/blob/master/explainer.md.
void Shell::PerformanceMeasureMemory(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::MeasureMemoryMode mode = v8::MeasureMemoryMode::kSummary;
  v8::Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  if (args.Length() >= 1 && args[0]->IsObject()) {
    Local<Object> object = args[0].As<Object>();
    Local<Value> value = TryGetValue(isolate, context, object, "detailed")
                             .FromMaybe(Local<Value>());
    if (!value.IsEmpty() && value->IsBoolean() &&
        value->BooleanValue(isolate)) {
      mode = v8::MeasureMemoryMode::kDetailed;
    }
  }
  Local<v8::Promise::Resolver> promise_resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  args.GetIsolate()->MeasureMemory(
      v8::MeasureMemoryDelegate::Default(isolate, context, promise_resolver,
                                         mode),
      v8::MeasureMemoryExecution::kEager);
  args.GetReturnValue().Set(promise_resolver->GetPromise());
}

// Realm.current() returns the index of the currently active realm.
void Shell::RealmCurrent(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int index = data->RealmFind(isolate->GetEnteredOrMicrotaskContext());
  if (index == -1) return;
  args.GetReturnValue().Set(index);
}

// Realm.owner(o) returns the index of the realm that created o.
void Shell::RealmOwner(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  if (args.Length() < 1 || !args[0]->IsObject()) {
    Throw(args.GetIsolate(), "Invalid argument");
    return;
  }
  Local<Object> object =
      args[0]->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
  i::Handle<i::JSReceiver> i_object = Utils::OpenHandle(*object);
  if (i_object->IsJSGlobalProxy() &&
      i::Handle<i::JSGlobalProxy>::cast(i_object)->IsDetached()) {
    return;
  }
  Local<Context> creation_context;
  if (!object->GetCreationContext().ToLocal(&creation_context)) {
    Throw(args.GetIsolate(), "object doesn't have creation context");
    return;
  }
  int index = data->RealmFind(creation_context);
  if (index == -1) return;
  args.GetReturnValue().Set(index);
}

// Realm.global(i) returns the global object of realm i.
// (Note that properties of global objects cannot be read/written cross-realm.)
void Shell::RealmGlobal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  PerIsolateData* data = PerIsolateData::Get(args.GetIsolate());
  int index = data->RealmIndexOrThrow(args, 0);
  if (index == -1) return;
  args.GetReturnValue().Set(
      Local<Context>::New(args.GetIsolate(), data->realms_[index])->Global());
}

MaybeLocal<Context> Shell::CreateRealm(
    const v8::FunctionCallbackInfo<v8::Value>& args, int index,
    v8::MaybeLocal<Value> global_object) {
  Isolate* isolate = args.GetIsolate();
  TryCatch try_catch(isolate);
  PerIsolateData* data = PerIsolateData::Get(isolate);
  if (index < 0) {
    Global<Context>* old_realms = data->realms_;
    index = data->realm_count_;
    data->realms_ = new Global<Context>[++data->realm_count_];
    for (int i = 0; i < index; ++i) {
      data->realms_[i].Reset(isolate, old_realms[i]);
      old_realms[i].Reset();
    }
    delete[] old_realms;
  }
  Local<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  Local<Context> context =
      Context::New(isolate, nullptr, global_template, global_object);
  DCHECK(!try_catch.HasCaught());
  if (context.IsEmpty()) return MaybeLocal<Context>();
  InitializeModuleEmbedderData(context);
  data->realms_[index].Reset(isolate, context);
  args.GetReturnValue().Set(index);
  return context;
}

void Shell::DisposeRealm(const v8::FunctionCallbackInfo<v8::Value>& args,
                         int index) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  Local<Context> context = data->realms_[index].Get(isolate);
  DisposeModuleEmbedderData(context);
  data->realms_[index].Reset();
  // ContextDisposedNotification expects the disposed context to be entered.
  v8::Context::Scope scope(context);
  isolate->ContextDisposedNotification();
  isolate->IdleNotificationDeadline(g_platform->MonotonicallyIncreasingTime());
}

// Realm.create() creates a new realm with a distinct security token
// and returns its index.
void Shell::RealmCreate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CreateRealm(args, -1, v8::MaybeLocal<Value>());
}

// Realm.createAllowCrossRealmAccess() creates a new realm with the same
// security token as the current realm.
void Shell::RealmCreateAllowCrossRealmAccess(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Local<Context> context;
  if (CreateRealm(args, -1, v8::MaybeLocal<Value>()).ToLocal(&context)) {
    context->SetSecurityToken(
        args.GetIsolate()->GetEnteredOrMicrotaskContext()->GetSecurityToken());
  }
}

// Realm.navigate(i) creates a new realm with a distinct security token
// in place of realm i.
void Shell::RealmNavigate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int index = data->RealmIndexOrThrow(args, 0);
  if (index == -1) return;
  if (index == 0 || index == data->realm_current_ ||
      index == data->realm_switch_) {
    Throw(args.GetIsolate(), "Invalid realm index");
    return;
  }

  Local<Context> context = Local<Context>::New(isolate, data->realms_[index]);
  v8::MaybeLocal<Value> global_object = context->Global();

  // Context::Global doesn't return JSGlobalProxy if DetachGlobal is called in
  // advance.
  if (!global_object.IsEmpty()) {
    HandleScope scope(isolate);
    if (!Utils::OpenHandle(*global_object.ToLocalChecked())
             ->IsJSGlobalProxy()) {
      global_object = v8::MaybeLocal<Value>();
    }
  }

  DisposeRealm(args, index);
  CreateRealm(args, index, global_object);
}

// Realm.detachGlobal(i) detaches the global objects of realm i from realm i.
void Shell::RealmDetachGlobal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int index = data->RealmIndexOrThrow(args, 0);
  if (index == -1) return;
  if (index == 0 || index == data->realm_current_ ||
      index == data->realm_switch_) {
    Throw(args.GetIsolate(), "Invalid realm index");
    return;
  }

  HandleScope scope(isolate);
  Local<Context> realm = Local<Context>::New(isolate, data->realms_[index]);
  realm->DetachGlobal();
}

// Realm.dispose(i) disposes the reference to the realm i.
void Shell::RealmDispose(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int index = data->RealmIndexOrThrow(args, 0);
  if (index == -1) return;
  if (index == 0 || index == data->realm_current_ ||
      index == data->realm_switch_) {
    Throw(args.GetIsolate(), "Invalid realm index");
    return;
  }
  DisposeRealm(args, index);
}

// Realm.switch(i) switches to the realm i for consecutive interactive inputs.
void Shell::RealmSwitch(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int index = data->RealmIndexOrThrow(args, 0);
  if (index == -1) return;
  data->realm_switch_ = index;
}

// Realm.eval(i, s) evaluates s in realm i and returns the result.
void Shell::RealmEval(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int index = data->RealmIndexOrThrow(args, 0);
  if (index == -1) return;
  if (args.Length() < 2 || !args[1]->IsString()) {
    Throw(args.GetIsolate(), "Invalid argument");
    return;
  }
  ScriptOrigin origin(isolate,
                      String::NewFromUtf8Literal(isolate, "(d8)",
                                                 NewStringType::kInternalized));
  ScriptCompiler::Source script_source(
      args[1]->ToString(isolate->GetCurrentContext()).ToLocalChecked(), origin);
  Local<UnboundScript> script;
  if (!ScriptCompiler::CompileUnboundScript(isolate, &script_source)
           .ToLocal(&script)) {
    return;
  }
  Local<Context> realm = Local<Context>::New(isolate, data->realms_[index]);
  realm->Enter();
  int previous_index = data->realm_current_;
  data->realm_current_ = data->realm_switch_ = index;
  Local<Value> result;
  if (!script->BindToCurrentContext()->Run(realm).ToLocal(&result)) {
    realm->Exit();
    data->realm_current_ = data->realm_switch_ = previous_index;
    return;
  }
  realm->Exit();
  data->realm_current_ = data->realm_switch_ = previous_index;
  args.GetReturnValue().Set(result);
}

// Realm.shared is an accessor for a single shared value across realms.
void Shell::RealmSharedGet(Local<String> property,
                           const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  if (data->realm_shared_.IsEmpty()) return;
  info.GetReturnValue().Set(data->realm_shared_);
}

void Shell::RealmSharedSet(Local<String> property, Local<Value> value,
                           const PropertyCallbackInfo<void>& info) {
  Isolate* isolate = info.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  data->realm_shared_.Reset(isolate, value);
}

void Shell::LogGetAndStop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope handle_scope(isolate);

  std::string file_name = i_isolate->logger()->file_name();
  if (!i::Log::IsLoggingToTemporaryFile(file_name)) {
    Throw(isolate, "Only capturing from temporary files is supported.");
    return;
  }
  if (!i_isolate->logger()->is_logging()) {
    Throw(isolate, "Logging not enabled.");
    return;
  }

  std::string raw_log;
  FILE* log_file = i_isolate->logger()->TearDownAndGetLogFile();
  if (!log_file) {
    Throw(isolate, "Log file does not exist.");
    return;
  }

  bool exists = false;
  raw_log = i::ReadFile(log_file, &exists, true);
  base::Fclose(log_file);

  if (!exists) {
    Throw(isolate, "Unable to read log file.");
    return;
  }
  Local<String> result =
      String::NewFromUtf8(isolate, raw_log.c_str(), NewStringType::kNormal,
                          static_cast<int>(raw_log.size()))
          .ToLocalChecked();

  args.GetReturnValue().Set(result);
}

// async_hooks.createHook() registers functions to be called for different
// lifetime events of each async operation.
void Shell::AsyncHooksCreateHook(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Local<Object> wrap =
      PerIsolateData::Get(args.GetIsolate())->GetAsyncHooks()->CreateHook(args);
  args.GetReturnValue().Set(wrap);
}

// async_hooks.executionAsyncId() returns the asyncId of the current execution
// context.
void Shell::AsyncHooksExecutionAsyncId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  args.GetReturnValue().Set(v8::Number::New(
      isolate,
      PerIsolateData::Get(isolate)->GetAsyncHooks()->GetExecutionAsyncId()));
}

void Shell::AsyncHooksTriggerAsyncId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  args.GetReturnValue().Set(v8::Number::New(
      isolate,
      PerIsolateData::Get(isolate)->GetAsyncHooks()->GetTriggerAsyncId()));
}

void WriteToFile(FILE* file, const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(args.GetIsolate());
    if (i != 0) {
      fprintf(file, " ");
    }

    // Explicitly catch potential exceptions in toString().
    v8::TryCatch try_catch(args.GetIsolate());
    Local<Value> arg = args[i];
    Local<String> str_obj;

    if (arg->IsSymbol()) {
      arg = arg.As<Symbol>()->Description();
    }
    if (!arg->ToString(args.GetIsolate()->GetCurrentContext())
             .ToLocal(&str_obj)) {
      try_catch.ReThrow();
      return;
    }

    v8::String::Utf8Value str(args.GetIsolate(), str_obj);
    int n = static_cast<int>(fwrite(*str, sizeof(**str), str.length(), file));
    if (n != str.length()) {
      printf("Error in fwrite\n");
      base::OS::ExitProcess(1);
    }
  }
}

void WriteAndFlush(FILE* file,
                   const v8::FunctionCallbackInfo<v8::Value>& args) {
  WriteToFile(file, args);
  fprintf(file, "\n");
  fflush(file);
}

void Shell::Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
  WriteAndFlush(stdout, args);
}

void Shell::PrintErr(const v8::FunctionCallbackInfo<v8::Value>& args) {
  WriteAndFlush(stderr, args);
}

void Shell::Write(const v8::FunctionCallbackInfo<v8::Value>& args) {
  WriteToFile(stdout, args);
}

void Shell::Read(const v8::FunctionCallbackInfo<v8::Value>& args) {
  String::Utf8Value file(args.GetIsolate(), args[0]);
  if (*file == nullptr) {
    Throw(args.GetIsolate(), "Error loading file");
    return;
  }
  if (args.Length() == 2) {
    String::Utf8Value format(args.GetIsolate(), args[1]);
    if (*format && std::strcmp(*format, "binary") == 0) {
      ReadBuffer(args);
      return;
    }
  }
  Local<String> source = ReadFile(args.GetIsolate(), *file);
  if (source.IsEmpty()) {
    Throw(args.GetIsolate(), "Error loading file");
    return;
  }
  args.GetReturnValue().Set(source);
}

Local<String> Shell::ReadFromStdin(Isolate* isolate) {
  static const int kBufferSize = 256;
  char buffer[kBufferSize];
  Local<String> accumulator = String::NewFromUtf8Literal(isolate, "");
  int length;
  while (true) {
    // Continue reading if the line ends with an escape '\\' or the line has
    // not been fully read into the buffer yet (does not end with '\n').
    // If fgets gets an error, just give up.
    char* input = nullptr;
    input = fgets(buffer, kBufferSize, stdin);
    if (input == nullptr) return Local<String>();
    length = static_cast<int>(strlen(buffer));
    if (length == 0) {
      return accumulator;
    } else if (buffer[length - 1] != '\n') {
      accumulator = String::Concat(
          isolate, accumulator,
          String::NewFromUtf8(isolate, buffer, NewStringType::kNormal, length)
              .ToLocalChecked());
    } else if (length > 1 && buffer[length - 2] == '\\') {
      buffer[length - 2] = '\n';
      accumulator =
          String::Concat(isolate, accumulator,
                         String::NewFromUtf8(isolate, buffer,
                                             NewStringType::kNormal, length - 1)
                             .ToLocalChecked());
    } else {
      return String::Concat(
          isolate, accumulator,
          String::NewFromUtf8(isolate, buffer, NewStringType::kNormal,
                              length - 1)
              .ToLocalChecked());
    }
  }
}

void Shell::Load(const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(args.GetIsolate());
    String::Utf8Value file(args.GetIsolate(), args[i]);
    if (*file == nullptr) {
      Throw(args.GetIsolate(), "Error loading file");
      return;
    }
    Local<String> source = ReadFile(args.GetIsolate(), *file);
    if (source.IsEmpty()) {
      Throw(args.GetIsolate(), "Error loading file");
      return;
    }
    if (!ExecuteString(
            args.GetIsolate(), source,
            String::NewFromUtf8(args.GetIsolate(), *file).ToLocalChecked(),
            kNoPrintResult,
            options.quiet_load ? kNoReportExceptions : kReportExceptions,
            kNoProcessMessageQueue)) {
      Throw(args.GetIsolate(), "Error executing file");
      return;
    }
  }
}

void Shell::SetTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  args.GetReturnValue().Set(v8::Number::New(isolate, 0));
  if (args.Length() == 0 || !args[0]->IsFunction()) return;
  Local<Function> callback = args[0].As<Function>();
  Local<Context> context = isolate->GetCurrentContext();
  PerIsolateData::Get(isolate)->SetTimeout(callback, context);
}

enum WorkerType { kClassic, kString, kFunction, kInvalid, kNone };

void ReadWorkerTypeAndArguments(const v8::FunctionCallbackInfo<v8::Value>& args,
                                WorkerType* worker_type,
                                Local<Value>* arguments = nullptr) {
  Isolate* isolate = args.GetIsolate();
  if (args.Length() > 1 && args[1]->IsObject()) {
    Local<Object> object = args[1].As<Object>();
    Local<Context> context = isolate->GetCurrentContext();
    Local<Value> value;
    if (!TryGetValue(isolate, context, object, "type").ToLocal(&value)) {
      *worker_type = WorkerType::kNone;
      return;
    }
    if (!value->IsString()) {
      *worker_type = WorkerType::kInvalid;
      return;
    }
    Local<String> worker_type_string =
        value->ToString(context).ToLocalChecked();
    String::Utf8Value str(isolate, worker_type_string);
    if (strcmp("string", *str) == 0) {
      *worker_type = WorkerType::kString;
    } else if (strcmp("classic", *str) == 0) {
      *worker_type = WorkerType::kClassic;
    } else if (strcmp("function", *str) == 0) {
      *worker_type = WorkerType::kFunction;
    } else {
      *worker_type = WorkerType::kInvalid;
    }
    if (arguments != nullptr) {
      bool got_arguments =
          TryGetValue(isolate, context, object, "arguments").ToLocal(arguments);
      USE(got_arguments);
    }
  } else {
    *worker_type = WorkerType::kNone;
  }
}

bool FunctionAndArgumentsToString(Local<Function> function,
                                  Local<Value> arguments, Local<String>* source,
                                  Isolate* isolate) {
  Local<Context> context = isolate->GetCurrentContext();
  MaybeLocal<String> maybe_function_string =
      function->FunctionProtoToString(context);
  Local<String> function_string;
  if (!maybe_function_string.ToLocal(&function_string)) {
    Throw(isolate, "Failed to convert function to string");
    return false;
  }
  *source = String::NewFromUtf8Literal(isolate, "(");
  *source = String::Concat(isolate, *source, function_string);
  Local<String> middle = String::NewFromUtf8Literal(isolate, ")(");
  *source = String::Concat(isolate, *source, middle);
  if (!arguments.IsEmpty() && !arguments->IsUndefined()) {
    if (!arguments->IsArray()) {
      Throw(isolate, "'arguments' must be an array");
      return false;
    }
    Local<String> comma = String::NewFromUtf8Literal(isolate, ",");
    Local<Array> array = arguments.As<Array>();
    for (uint32_t i = 0; i < array->Length(); ++i) {
      if (i > 0) {
        *source = String::Concat(isolate, *source, comma);
      }
      MaybeLocal<Value> maybe_argument = array->Get(context, i);
      Local<Value> argument;
      if (!maybe_argument.ToLocal(&argument)) {
        Throw(isolate, "Failed to get argument");
        return false;
      }
      Local<String> argument_string;
      if (!JSON::Stringify(context, argument).ToLocal(&argument_string)) {
        Throw(isolate, "Failed to convert argument to string");
        return false;
      }
      *source = String::Concat(isolate, *source, argument_string);
    }
  }
  Local<String> suffix = String::NewFromUtf8Literal(isolate, ")");
  *source = String::Concat(isolate, *source, suffix);
  return true;
}

void Shell::WorkerNew(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  if (args.Length() < 1 || (!args[0]->IsString() && !args[0]->IsFunction())) {
    Throw(isolate, "1st argument must be a string or a function");
    return;
  }

  Local<String> source;
  if (args[0]->IsFunction()) {
    // d8 supports `options={type: 'function', arguments:[...]}`, which means
    // the first argument is a function with the code to be ran. Restrictions
    // apply; in particular the function will be converted to a string and the
    // Worker constructed based on it.
    WorkerType worker_type;
    Local<Value> arguments;
    ReadWorkerTypeAndArguments(args, &worker_type, &arguments);
    if (worker_type != WorkerType::kFunction) {
      Throw(isolate, "Invalid or missing worker type");
      return;
    }

    // Source: ( function_to_string )( params )
    if (!FunctionAndArgumentsToString(args[0].As<Function>(), arguments,
                                      &source, isolate)) {
      return;
    }
  } else {
    // d8 honors `options={type: 'string'}`, which means the first argument is
    // not a filename but string of script to be run.
    bool load_from_file = true;
    WorkerType worker_type;
    ReadWorkerTypeAndArguments(args, &worker_type);
    if (worker_type == WorkerType::kString) {
      load_from_file = false;
    } else if (worker_type != WorkerType::kNone &&
               worker_type != WorkerType::kClassic) {
      Throw(isolate, "Invalid worker type");
      return;
    }

    if (load_from_file) {
      String::Utf8Value filename(isolate, args[0]);
      source = ReadFile(isolate, *filename);
      if (source.IsEmpty()) {
        Throw(args.GetIsolate(), "Error loading worker script");
        return;
      }
    } else {
      source = args[0].As<String>();
    }
  }

  if (!args.IsConstructCall()) {
    Throw(isolate, "Worker must be constructed with new");
    return;
  }

  // Initialize the embedder field to 0; if we return early without
  // creating a new Worker (because the main thread is terminating) we can
  // early-out from the instance calls.
  args.Holder()->SetInternalField(0, v8::Integer::New(isolate, 0));

  {
    // Don't allow workers to create more workers if the main thread
    // is waiting for existing running workers to terminate.
    base::MutexGuard lock_guard(workers_mutex_.Pointer());
    if (!allow_new_workers_) return;

    String::Utf8Value script(isolate, source);
    if (!*script) {
      Throw(isolate, "Can't get worker script");
      return;
    }

    // The C++ worker object's lifetime is shared between the Managed<Worker>
    // object on the heap, which the JavaScript object points to, and an
    // internal std::shared_ptr in the worker thread itself.
    auto worker = std::make_shared<Worker>(*script);
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    const size_t kWorkerSizeEstimate = 4 * 1024 * 1024;  // stack + heap.
    i::Handle<i::Object> managed = i::Managed<Worker>::FromSharedPtr(
        i_isolate, kWorkerSizeEstimate, worker);
    args.Holder()->SetInternalField(0, Utils::ToLocal(managed));
    if (!Worker::StartWorkerThread(std::move(worker))) {
      Throw(isolate, "Can't start thread");
      return;
    }
  }
}

void Shell::WorkerPostMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);

  if (args.Length() < 1) {
    Throw(isolate, "Invalid argument");
    return;
  }

  std::shared_ptr<Worker> worker =
      GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker.get()) {
    return;
  }

  Local<Value> message = args[0];
  Local<Value> transfer =
      args.Length() >= 2 ? args[1] : Undefined(isolate).As<Value>();
  std::unique_ptr<SerializationData> data =
      Shell::SerializeValue(isolate, message, transfer);
  if (data) {
    worker->PostMessage(std::move(data));
  }
}

void Shell::WorkerGetMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  std::shared_ptr<Worker> worker =
      GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker.get()) {
    return;
  }

  std::unique_ptr<SerializationData> data = worker->GetMessage();
  if (data) {
    Local<Value> value;
    if (Shell::DeserializeValue(isolate, std::move(data)).ToLocal(&value)) {
      args.GetReturnValue().Set(value);
    }
  }
}

void Shell::WorkerTerminate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  std::shared_ptr<Worker> worker =
      GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker.get()) {
    return;
  }

  worker->Terminate();
}

void Shell::WorkerTerminateAndWait(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  std::shared_ptr<Worker> worker =
      GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker.get()) {
    return;
  }

  worker->TerminateAndWaitForThread();
}

void Shell::QuitOnce(v8::FunctionCallbackInfo<v8::Value>* args) {
  int exit_code = (*args)[0]
                      ->Int32Value(args->GetIsolate()->GetCurrentContext())
                      .FromMaybe(0);
  WaitForRunningWorkers();
  args->GetIsolate()->Exit();
  OnExit(args->GetIsolate());
  base::OS::ExitProcess(exit_code);
}

void Shell::Quit(const v8::FunctionCallbackInfo<v8::Value>& args) {
  base::CallOnce(&quit_once_, &QuitOnce,
                 const_cast<v8::FunctionCallbackInfo<v8::Value>*>(&args));
}

void Shell::WaitUntilDone(const v8::FunctionCallbackInfo<v8::Value>& args) {
  SetWaitUntilDone(args.GetIsolate(), true);
}

void Shell::NotifyDone(const v8::FunctionCallbackInfo<v8::Value>& args) {
  SetWaitUntilDone(args.GetIsolate(), false);
}

void Shell::Version(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      String::NewFromUtf8(args.GetIsolate(), V8::GetVersion())
          .ToLocalChecked());
}

#ifdef V8_FUZZILLI

// We have to assume that the fuzzer will be able to call this function e.g. by
// enumerating the properties of the global object and eval'ing them. As such
// this function is implemented in a way that requires passing some magic value
// as first argument (with the idea being that the fuzzer won't be able to
// generate this value) which then also acts as a selector for the operation
// to perform.
void Shell::Fuzzilli(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope handle_scope(args.GetIsolate());

  String::Utf8Value operation(args.GetIsolate(), args[0]);
  if (*operation == nullptr) {
    return;
  }

  if (strcmp(*operation, "FUZZILLI_CRASH") == 0) {
    auto arg = args[1]
                   ->Int32Value(args.GetIsolate()->GetCurrentContext())
                   .FromMaybe(0);
    switch (arg) {
      case 0:
        V8_IMMEDIATE_CRASH();
        break;
      case 1:
        CHECK(0);
        break;
      default:
        DCHECK(false);
        break;
    }
  } else if (strcmp(*operation, "FUZZILLI_PRINT") == 0) {
    static FILE* fzliout = fdopen(REPRL_DWFD, "w");
    if (!fzliout) {
      fprintf(
          stderr,
          "Fuzzer output channel not available, printing to stdout instead\n");
      fzliout = stdout;
    }

    String::Utf8Value string(args.GetIsolate(), args[1]);
    if (*string == nullptr) {
      return;
    }
    fprintf(fzliout, "%s\n", *string);
    fflush(fzliout);
  }
}

#endif  // V8_FUZZILLI

void Shell::ReportException(Isolate* isolate, Local<v8::Message> message,
                            Local<v8::Value> exception_obj) {
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  bool enter_context = context.IsEmpty();
  if (enter_context) {
    context = Local<Context>::New(isolate, evaluation_context_);
    context->Enter();
  }
  // Converts a V8 value to a C string.
  auto ToCString = [](const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
  };

  v8::String::Utf8Value exception(isolate, exception_obj);
  const char* exception_string = ToCString(exception);
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    printf("%s\n", exception_string);
  } else if (message->GetScriptOrigin().Options().IsWasm()) {
    // Print wasm-function[(function index)]:(offset): (message).
    int function_index = message->GetWasmFunctionIndex();
    int offset = message->GetStartColumn(context).FromJust();
    printf("wasm-function[%d]:0x%x: %s\n", function_index, offset,
           exception_string);
  } else {
    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(isolate,
                                   message->GetScriptOrigin().ResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber(context).FromMaybe(-1);
    printf("%s:%i: %s\n", filename_string, linenum, exception_string);
    Local<String> sourceline;
    if (message->GetSourceLine(context).ToLocal(&sourceline)) {
      // Print line of source code.
      v8::String::Utf8Value sourcelinevalue(isolate, sourceline);
      const char* sourceline_string = ToCString(sourcelinevalue);
      printf("%s\n", sourceline_string);
      // Print wavy underline (GetUnderline is deprecated).
      int start = message->GetStartColumn(context).FromJust();
      for (int i = 0; i < start; i++) {
        printf(" ");
      }
      int end = message->GetEndColumn(context).FromJust();
      for (int i = start; i < end; i++) {
        printf("^");
      }
      printf("\n");
    }
  }
  Local<Value> stack_trace_string;
  if (v8::TryCatch::StackTrace(context, exception_obj)
          .ToLocal(&stack_trace_string) &&
      stack_trace_string->IsString()) {
    v8::String::Utf8Value stack_trace(isolate, stack_trace_string.As<String>());
    printf("%s\n", ToCString(stack_trace));
  }
  printf("\n");
  if (enter_context) context->Exit();
}

void Shell::ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {
  ReportException(isolate, try_catch->Message(), try_catch->Exception());
}

int32_t* Counter::Bind(const char* name, bool is_histogram) {
  int i;
  for (i = 0; i < kMaxNameSize - 1 && name[i]; i++)
    name_[i] = static_cast<char>(name[i]);
  name_[i] = '\0';
  is_histogram_ = is_histogram;
  return ptr();
}

void Counter::AddSample(int32_t sample) {
  count_++;
  sample_total_ += sample;
}

CounterCollection::CounterCollection() {
  magic_number_ = 0xDEADFACE;
  max_counters_ = kMaxCounters;
  max_name_size_ = Counter::kMaxNameSize;
  counters_in_use_ = 0;
}

Counter* CounterCollection::GetNextCounter() {
  if (counters_in_use_ == kMaxCounters) return nullptr;
  return &counters_[counters_in_use_++];
}

void Shell::MapCounters(v8::Isolate* isolate, const char* name) {
  counters_file_ = base::OS::MemoryMappedFile::create(
      name, sizeof(CounterCollection), &local_counters_);
  void* memory =
      (counters_file_ == nullptr) ? nullptr : counters_file_->memory();
  if (memory == nullptr) {
    printf("Could not map counters file %s\n", name);
    base::OS::ExitProcess(1);
  }
  counters_ = static_cast<CounterCollection*>(memory);
  isolate->SetCounterFunction(LookupCounter);
  isolate->SetCreateHistogramFunction(CreateHistogram);
  isolate->SetAddHistogramSampleFunction(AddHistogramSample);
}

Counter* Shell::GetCounter(const char* name, bool is_histogram) {
  auto map_entry = counter_map_->find(name);
  Counter* counter =
      map_entry != counter_map_->end() ? map_entry->second : nullptr;

  if (counter == nullptr) {
    counter = counters_->GetNextCounter();
    if (counter != nullptr) {
      (*counter_map_)[name] = counter;
      counter->Bind(name, is_histogram);
    }
  } else {
    DCHECK(counter->is_histogram() == is_histogram);
  }
  return counter;
}

int* Shell::LookupCounter(const char* name) {
  Counter* counter = GetCounter(name, false);

  if (counter != nullptr) {
    return counter->ptr();
  } else {
    return nullptr;
  }
}

void* Shell::CreateHistogram(const char* name, int min, int max,
                             size_t buckets) {
  return GetCounter(name, true);
}

void Shell::AddHistogramSample(void* histogram, int sample) {
  Counter* counter = reinterpret_cast<Counter*>(histogram);
  counter->AddSample(sample);
}

// Turn a value into a human-readable string.
Local<String> Shell::Stringify(Isolate* isolate, Local<Value> value) {
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, evaluation_context_);
  if (stringify_function_.IsEmpty()) {
    Local<String> source =
        String::NewFromUtf8(isolate, stringify_source_).ToLocalChecked();
    Local<String> name = String::NewFromUtf8Literal(isolate, "d8-stringify");
    ScriptOrigin origin(isolate, name);
    Local<Script> script =
        Script::Compile(context, source, &origin).ToLocalChecked();
    stringify_function_.Reset(
        isolate, script->Run(context).ToLocalChecked().As<Function>());
  }
  Local<Function> fun = Local<Function>::New(isolate, stringify_function_);
  Local<Value> argv[1] = {value};
  v8::TryCatch try_catch(isolate);
  MaybeLocal<Value> result = fun->Call(context, Undefined(isolate), 1, argv);
  if (result.IsEmpty()) return String::Empty(isolate);
  return result.ToLocalChecked().As<String>();
}

Local<ObjectTemplate> Shell::CreateGlobalTemplate(Isolate* isolate) {
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate);
  global_template->Set(Symbol::GetToStringTag(isolate),
                       String::NewFromUtf8Literal(isolate, "global"));
  global_template->Set(isolate, "version",
                       FunctionTemplate::New(isolate, Version));

  global_template->Set(isolate, "print", FunctionTemplate::New(isolate, Print));
  global_template->Set(isolate, "printErr",
                       FunctionTemplate::New(isolate, PrintErr));
  global_template->Set(isolate, "write", FunctionTemplate::New(isolate, Write));
  global_template->Set(isolate, "read", FunctionTemplate::New(isolate, Read));
  global_template->Set(isolate, "readbuffer",
                       FunctionTemplate::New(isolate, ReadBuffer));
  global_template->Set(isolate, "readline",
                       FunctionTemplate::New(isolate, ReadLine));
  global_template->Set(isolate, "load", FunctionTemplate::New(isolate, Load));
  global_template->Set(isolate, "setTimeout",
                       FunctionTemplate::New(isolate, SetTimeout));
  // Some Emscripten-generated code tries to call 'quit', which in turn would
  // call C's exit(). This would lead to memory leaks, because there is no way
  // we can terminate cleanly then, so we need a way to hide 'quit'.
  if (!options.omit_quit) {
    global_template->Set(isolate, "quit", FunctionTemplate::New(isolate, Quit));
  }
  global_template->Set(isolate, "testRunner",
                       Shell::CreateTestRunnerTemplate(isolate));
  global_template->Set(isolate, "Realm", Shell::CreateRealmTemplate(isolate));
  global_template->Set(isolate, "performance",
                       Shell::CreatePerformanceTemplate(isolate));
  global_template->Set(isolate, "Worker", Shell::CreateWorkerTemplate(isolate));
  // Prevent fuzzers from creating side effects.
  if (!i::FLAG_fuzzing) {
    global_template->Set(isolate, "os", Shell::CreateOSTemplate(isolate));
  }
  global_template->Set(isolate, "d8", Shell::CreateD8Template(isolate));

#ifdef V8_FUZZILLI
  global_template->Set(
      String::NewFromUtf8(isolate, "fuzzilli", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, Fuzzilli), PropertyAttribute::DontEnum);
#endif  // V8_FUZZILLI

  if (i::FLAG_expose_async_hooks) {
    global_template->Set(isolate, "async_hooks",
                         Shell::CreateAsyncHookTemplate(isolate));
  }

  return global_template;
}

Local<ObjectTemplate> Shell::CreateOSTemplate(Isolate* isolate) {
  Local<ObjectTemplate> os_template = ObjectTemplate::New(isolate);
  AddOSMethods(isolate, os_template);
  return os_template;
}

Local<FunctionTemplate> Shell::CreateWorkerTemplate(Isolate* isolate) {
  Local<FunctionTemplate> worker_fun_template =
      FunctionTemplate::New(isolate, WorkerNew);
  Local<Signature> worker_signature =
      Signature::New(isolate, worker_fun_template);
  worker_fun_template->SetClassName(
      String::NewFromUtf8Literal(isolate, "Worker"));
  worker_fun_template->ReadOnlyPrototype();
  worker_fun_template->PrototypeTemplate()->Set(
      isolate, "terminate",
      FunctionTemplate::New(isolate, WorkerTerminate, Local<Value>(),
                            worker_signature));
  worker_fun_template->PrototypeTemplate()->Set(
      isolate, "terminateAndWait",
      FunctionTemplate::New(isolate, WorkerTerminateAndWait, Local<Value>(),
                            worker_signature));
  worker_fun_template->PrototypeTemplate()->Set(
      isolate, "postMessage",
      FunctionTemplate::New(isolate, WorkerPostMessage, Local<Value>(),
                            worker_signature));
  worker_fun_template->PrototypeTemplate()->Set(
      isolate, "getMessage",
      FunctionTemplate::New(isolate, WorkerGetMessage, Local<Value>(),
                            worker_signature));
  worker_fun_template->InstanceTemplate()->SetInternalFieldCount(1);
  return worker_fun_template;
}

Local<ObjectTemplate> Shell::CreateAsyncHookTemplate(Isolate* isolate) {
  Local<ObjectTemplate> async_hooks_templ = ObjectTemplate::New(isolate);
  async_hooks_templ->Set(isolate, "createHook",
                         FunctionTemplate::New(isolate, AsyncHooksCreateHook));
  async_hooks_templ->Set(
      isolate, "executionAsyncId",
      FunctionTemplate::New(isolate, AsyncHooksExecutionAsyncId));
  async_hooks_templ->Set(
      isolate, "triggerAsyncId",
      FunctionTemplate::New(isolate, AsyncHooksTriggerAsyncId));
  return async_hooks_templ;
}

Local<ObjectTemplate> Shell::CreateTestRunnerTemplate(Isolate* isolate) {
  Local<ObjectTemplate> test_template = ObjectTemplate::New(isolate);
  test_template->Set(isolate, "notifyDone",
                     FunctionTemplate::New(isolate, NotifyDone));
  test_template->Set(isolate, "waitUntilDone",
                     FunctionTemplate::New(isolate, WaitUntilDone));
  // Reliable access to quit functionality. The "quit" method function
  // installed on the global object can be hidden with the --omit-quit flag
  // (e.g. on asan bots).
  test_template->Set(isolate, "quit", FunctionTemplate::New(isolate, Quit));
  return test_template;
}

Local<ObjectTemplate> Shell::CreatePerformanceTemplate(Isolate* isolate) {
  Local<ObjectTemplate> performance_template = ObjectTemplate::New(isolate);
  performance_template->Set(isolate, "now",
                            FunctionTemplate::New(isolate, PerformanceNow));
  performance_template->Set(
      isolate, "measureMemory",
      FunctionTemplate::New(isolate, PerformanceMeasureMemory));
  return performance_template;
}

Local<ObjectTemplate> Shell::CreateRealmTemplate(Isolate* isolate) {
  Local<ObjectTemplate> realm_template = ObjectTemplate::New(isolate);
  realm_template->Set(isolate, "current",
                      FunctionTemplate::New(isolate, RealmCurrent));
  realm_template->Set(isolate, "owner",
                      FunctionTemplate::New(isolate, RealmOwner));
  realm_template->Set(isolate, "global",
                      FunctionTemplate::New(isolate, RealmGlobal));
  realm_template->Set(isolate, "create",
                      FunctionTemplate::New(isolate, RealmCreate));
  realm_template->Set(
      isolate, "createAllowCrossRealmAccess",
      FunctionTemplate::New(isolate, RealmCreateAllowCrossRealmAccess));
  realm_template->Set(isolate, "navigate",
                      FunctionTemplate::New(isolate, RealmNavigate));
  realm_template->Set(isolate, "detachGlobal",
                      FunctionTemplate::New(isolate, RealmDetachGlobal));
  realm_template->Set(isolate, "dispose",
                      FunctionTemplate::New(isolate, RealmDispose));
  realm_template->Set(isolate, "switch",
                      FunctionTemplate::New(isolate, RealmSwitch));
  realm_template->Set(isolate, "eval",
                      FunctionTemplate::New(isolate, RealmEval));
  realm_template->SetAccessor(String::NewFromUtf8Literal(isolate, "shared"),
                              RealmSharedGet, RealmSharedSet);
  return realm_template;
}

Local<ObjectTemplate> Shell::CreateD8Template(Isolate* isolate) {
  Local<ObjectTemplate> d8_template = ObjectTemplate::New(isolate);
  {
    Local<ObjectTemplate> log_template = ObjectTemplate::New(isolate);
    log_template->Set(isolate, "getAndStop",
                      FunctionTemplate::New(isolate, LogGetAndStop));

    d8_template->Set(isolate, "log", log_template);
  }
  return d8_template;
}

static void PrintMessageCallback(Local<Message> message, Local<Value> error) {
  switch (message->ErrorLevel()) {
    case v8::Isolate::kMessageWarning:
    case v8::Isolate::kMessageLog:
    case v8::Isolate::kMessageInfo:
    case v8::Isolate::kMessageDebug: {
      break;
    }

    case v8::Isolate::kMessageError: {
      Shell::ReportException(message->GetIsolate(), message, error);
      return;
    }

    default: {
      UNREACHABLE();
    }
  }
  // Converts a V8 value to a C string.
  auto ToCString = [](const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
  };
  Isolate* isolate = message->GetIsolate();
  v8::String::Utf8Value msg(isolate, message->Get());
  const char* msg_string = ToCString(msg);
  // Print (filename):(line number): (message).
  v8::String::Utf8Value filename(isolate,
                                 message->GetScriptOrigin().ResourceName());
  const char* filename_string = ToCString(filename);
  Maybe<int> maybeline = message->GetLineNumber(isolate->GetCurrentContext());
  int linenum = maybeline.IsJust() ? maybeline.FromJust() : -1;
  printf("%s:%i: %s\n", filename_string, linenum, msg_string);
}

void Shell::PromiseRejectCallback(v8::PromiseRejectMessage data) {
  if (options.ignore_unhandled_promises) return;
  if (data.GetEvent() == v8::kPromiseRejectAfterResolved ||
      data.GetEvent() == v8::kPromiseResolveAfterResolved) {
    // Ignore reject/resolve after resolved.
    return;
  }
  v8::Local<v8::Promise> promise = data.GetPromise();
  v8::Isolate* isolate = promise->GetIsolate();
  PerIsolateData* isolate_data = PerIsolateData::Get(isolate);

  if (data.GetEvent() == v8::kPromiseHandlerAddedAfterReject) {
    isolate_data->RemoveUnhandledPromise(promise);
    return;
  }

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  bool capture_exceptions =
      i_isolate->get_capture_stack_trace_for_uncaught_exceptions();
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  v8::Local<Value> exception = data.GetValue();
  v8::Local<Message> message;
  // Assume that all objects are stack-traces.
  if (exception->IsObject()) {
    message = v8::Exception::CreateMessage(isolate, exception);
  }
  if (!exception->IsNativeError() &&
      (message.IsEmpty() || message->GetStackTrace().IsEmpty())) {
    // If there is no real Error object, manually create a stack trace.
    exception = v8::Exception::Error(
        v8::String::NewFromUtf8Literal(isolate, "Unhandled Promise."));
    message = Exception::CreateMessage(isolate, exception);
  }
  isolate->SetCaptureStackTraceForUncaughtExceptions(capture_exceptions);

  isolate_data->AddUnhandledPromise(promise, message, exception);
}

void Shell::Initialize(Isolate* isolate, D8Console* console,
                       bool isOnMainThread) {
  isolate->SetPromiseRejectCallback(PromiseRejectCallback);
  if (isOnMainThread) {
    // Set up counters
    if (i::FLAG_map_counters[0] != '\0') {
      MapCounters(isolate, i::FLAG_map_counters);
    }
    // Disable default message reporting.
    isolate->AddMessageListenerWithErrorLevel(
        PrintMessageCallback,
        v8::Isolate::kMessageError | v8::Isolate::kMessageWarning |
            v8::Isolate::kMessageInfo | v8::Isolate::kMessageDebug |
            v8::Isolate::kMessageLog);
  }

  isolate->SetHostImportModuleDynamicallyCallback(
      Shell::HostImportModuleDynamically);
  isolate->SetHostInitializeImportMetaObjectCallback(
      Shell::HostInitializeImportMetaObject);

#ifdef V8_FUZZILLI
  // Let the parent process (Fuzzilli) know we are ready.
  if (options.fuzzilli_enable_builtins_coverage) {
    cov_init_builtins_edges(static_cast<uint32_t>(
        i::BasicBlockProfiler::Get()
            ->GetCoverageBitmap(reinterpret_cast<i::Isolate*>(isolate))
            .size()));
  }
  char helo[] = "HELO";
  if (write(REPRL_CWFD, helo, 4) != 4 || read(REPRL_CRFD, helo, 4) != 4) {
    fuzzilli_reprl = false;
  }

  if (memcmp(helo, "HELO", 4) != 0) {
    fprintf(stderr, "Invalid response from parent\n");
    _exit(-1);
  }
#endif  // V8_FUZZILLI

  debug::SetConsoleDelegate(isolate, console);
}

Local<Context> Shell::CreateEvaluationContext(Isolate* isolate) {
  // This needs to be a critical section since this is not thread-safe
  base::MutexGuard lock_guard(context_mutex_.Pointer());
  // Initialize the global objects
  Local<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  EscapableHandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate, nullptr, global_template);
  DCHECK(!context.IsEmpty());
  if (i::FLAG_perf_prof_annotate_wasm || i::FLAG_vtune_prof_annotate_wasm) {
    isolate->SetWasmLoadSourceMapCallback(ReadFile);
  }
  InitializeModuleEmbedderData(context);
  if (options.include_arguments) {
    Context::Scope scope(context);
    const std::vector<const char*>& args = options.arguments;
    int size = static_cast<int>(args.size());
    Local<Array> array = Array::New(isolate, size);
    for (int i = 0; i < size; i++) {
      Local<String> arg =
          v8::String::NewFromUtf8(isolate, args[i]).ToLocalChecked();
      Local<Number> index = v8::Number::New(isolate, i);
      array->Set(context, index, arg).FromJust();
    }
    Local<String> name = String::NewFromUtf8Literal(
        isolate, "arguments", NewStringType::kInternalized);
    context->Global()->Set(context, name, array).FromJust();
  }
  return handle_scope.Escape(context);
}

void Shell::WriteIgnitionDispatchCountersFile(v8::Isolate* isolate) {
  HandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  Local<Object> dispatch_counters = reinterpret_cast<i::Isolate*>(isolate)
                                        ->interpreter()
                                        ->GetDispatchCountersObject();
  std::ofstream dispatch_counters_stream(
      i::FLAG_trace_ignition_dispatches_output_file);
  dispatch_counters_stream << *String::Utf8Value(
      isolate, JSON::Stringify(context, dispatch_counters).ToLocalChecked());
}

namespace {
int LineFromOffset(Local<debug::Script> script, int offset) {
  debug::Location location = script->GetSourceLocation(offset);
  return location.GetLineNumber();
}

void WriteLcovDataForRange(std::vector<uint32_t>* lines, int start_line,
                           int end_line, uint32_t count) {
  // Ensure space in the array.
  lines->resize(std::max(static_cast<size_t>(end_line + 1), lines->size()), 0);
  // Boundary lines could be shared between two functions with different
  // invocation counts. Take the maximum.
  (*lines)[start_line] = std::max((*lines)[start_line], count);
  (*lines)[end_line] = std::max((*lines)[end_line], count);
  // Invocation counts for non-boundary lines are overwritten.
  for (int k = start_line + 1; k < end_line; k++) (*lines)[k] = count;
}

void WriteLcovDataForNamedRange(std::ostream& sink,
                                std::vector<uint32_t>* lines,
                                const std::string& name, int start_line,
                                int end_line, uint32_t count) {
  WriteLcovDataForRange(lines, start_line, end_line, count);
  sink << "FN:" << start_line + 1 << "," << name << std::endl;
  sink << "FNDA:" << count << "," << name << std::endl;
}
}  // namespace

// Write coverage data in LCOV format. See man page for geninfo(1).
void Shell::WriteLcovData(v8::Isolate* isolate, const char* file) {
  if (!file) return;
  HandleScope handle_scope(isolate);
  debug::Coverage coverage = debug::Coverage::CollectPrecise(isolate);
  std::ofstream sink(file, std::ofstream::app);
  for (size_t i = 0; i < coverage.ScriptCount(); i++) {
    debug::Coverage::ScriptData script_data = coverage.GetScriptData(i);
    Local<debug::Script> script = script_data.GetScript();
    // Skip unnamed scripts.
    Local<String> name;
    if (!script->Name().ToLocal(&name)) continue;
    std::string file_name = ToSTLString(isolate, name);
    // Skip scripts not backed by a file.
    if (!std::ifstream(file_name).good()) continue;
    sink << "SF:";
    sink << NormalizePath(file_name, GetWorkingDirectory()) << std::endl;
    std::vector<uint32_t> lines;
    for (size_t j = 0; j < script_data.FunctionCount(); j++) {
      debug::Coverage::FunctionData function_data =
          script_data.GetFunctionData(j);

      // Write function stats.
      {
        debug::Location start =
            script->GetSourceLocation(function_data.StartOffset());
        debug::Location end =
            script->GetSourceLocation(function_data.EndOffset());
        int start_line = start.GetLineNumber();
        int end_line = end.GetLineNumber();
        uint32_t count = function_data.Count();

        Local<String> name;
        std::stringstream name_stream;
        if (function_data.Name().ToLocal(&name)) {
          name_stream << ToSTLString(isolate, name);
        } else {
          name_stream << "<" << start_line + 1 << "-";
          name_stream << start.GetColumnNumber() << ">";
        }

        WriteLcovDataForNamedRange(sink, &lines, name_stream.str(), start_line,
                                   end_line, count);
      }

      // Process inner blocks.
      for (size_t k = 0; k < function_data.BlockCount(); k++) {
        debug::Coverage::BlockData block_data = function_data.GetBlockData(k);
        int start_line = LineFromOffset(script, block_data.StartOffset());
        int end_line = LineFromOffset(script, block_data.EndOffset() - 1);
        uint32_t count = block_data.Count();
        WriteLcovDataForRange(&lines, start_line, end_line, count);
      }
    }
    // Write per-line coverage. LCOV uses 1-based line numbers.
    for (size_t i = 0; i < lines.size(); i++) {
      sink << "DA:" << (i + 1) << "," << lines[i] << std::endl;
    }
    sink << "end_of_record" << std::endl;
  }
}

void Shell::OnExit(v8::Isolate* isolate) {
  isolate->Dispose();

  if (i::FLAG_dump_counters || i::FLAG_dump_counters_nvp) {
    std::vector<std::pair<std::string, Counter*>> counters(
        counter_map_->begin(), counter_map_->end());
    std::sort(counters.begin(), counters.end());

    if (i::FLAG_dump_counters_nvp) {
      // Dump counters as name-value pairs.
      for (const auto& pair : counters) {
        std::string key = pair.first;
        Counter* counter = pair.second;
        if (counter->is_histogram()) {
          std::cout << "\"c:" << key << "\"=" << counter->count() << "\n";
          std::cout << "\"t:" << key << "\"=" << counter->sample_total()
                    << "\n";
        } else {
          std::cout << "\"" << key << "\"=" << counter->count() << "\n";
        }
      }
    } else {
      // Dump counters in formatted boxes.
      constexpr int kNameBoxSize = 64;
      constexpr int kValueBoxSize = 13;
      std::cout << "+" << std::string(kNameBoxSize, '-') << "+"
                << std::string(kValueBoxSize, '-') << "+\n";
      std::cout << "| Name" << std::string(kNameBoxSize - 5, ' ') << "| Value"
                << std::string(kValueBoxSize - 6, ' ') << "|\n";
      std::cout << "+" << std::string(kNameBoxSize, '-') << "+"
                << std::string(kValueBoxSize, '-') << "+\n";
      for (const auto& pair : counters) {
        std::string key = pair.first;
        Counter* counter = pair.second;
        if (counter->is_histogram()) {
          std::cout << "| c:" << std::setw(kNameBoxSize - 4) << std::left << key
                    << " | " << std::setw(kValueBoxSize - 2) << std::right
                    << counter->count() << " |\n";
          std::cout << "| t:" << std::setw(kNameBoxSize - 4) << std::left << key
                    << " | " << std::setw(kValueBoxSize - 2) << std::right
                    << counter->sample_total() << " |\n";
        } else {
          std::cout << "| " << std::setw(kNameBoxSize - 2) << std::left << key
                    << " | " << std::setw(kValueBoxSize - 2) << std::right
                    << counter->count() << " |\n";
        }
      }
      std::cout << "+" << std::string(kNameBoxSize, '-') << "+"
                << std::string(kValueBoxSize, '-') << "+\n";
    }
  }

  delete counters_file_;
  delete counter_map_;

  if (options.simulate_errors && is_valid_fuzz_script()) {
    // Simulate several errors detectable by fuzzers behind a flag if the
    // minimum file size for fuzzing was executed.
    FuzzerMonitor::SimulateErrors();
  }
}

void Dummy(char* arg) {}

V8_NOINLINE void FuzzerMonitor::SimulateErrors() {
  // Initialize a fresh RNG to not interfere with JS execution.
  std::unique_ptr<base::RandomNumberGenerator> rng;
  int64_t seed = internal::FLAG_random_seed;
  if (seed != 0) {
    rng = std::make_unique<base::RandomNumberGenerator>(seed);
  } else {
    rng = std::make_unique<base::RandomNumberGenerator>();
  }

  double p = rng->NextDouble();
  if (p < 0.1) {
    ControlFlowViolation();
  } else if (p < 0.2) {
    DCheck();
  } else if (p < 0.3) {
    Fatal();
  } else if (p < 0.4) {
    ObservableDifference();
  } else if (p < 0.5) {
    UndefinedBehavior();
  } else if (p < 0.6) {
    UseAfterFree();
  } else if (p < 0.7) {
    UseOfUninitializedValue();
  }
}

V8_NOINLINE void FuzzerMonitor::ControlFlowViolation() {
  // Control flow violation caught by CFI.
  void (*func)() = (void (*)()) & Dummy;
  func();
}

V8_NOINLINE void FuzzerMonitor::DCheck() {
  // Caught in debug builds.
  DCHECK(false);
}

V8_NOINLINE void FuzzerMonitor::Fatal() {
  // Caught in all build types.
  FATAL("Fake error.");
}

V8_NOINLINE void FuzzerMonitor::ObservableDifference() {
  // Observable difference caught by differential fuzzing.
  printf("___fake_difference___\n");
}

V8_NOINLINE void FuzzerMonitor::UndefinedBehavior() {
  // Caught by UBSAN.
  int32_t val = -1;
  USE(val << 8);
}

V8_NOINLINE void FuzzerMonitor::UseAfterFree() {
  // Use-after-free caught by ASAN.
  std::vector<bool>* storage = new std::vector<bool>(3);
  delete storage;
  USE(storage->at(1));
}

V8_NOINLINE void FuzzerMonitor::UseOfUninitializedValue() {
// Use-of-uninitialized-value caught by MSAN.
#if defined(__clang__)
  int uninitialized[1];
  if (uninitialized[0]) USE(uninitialized);
#endif
}

static FILE* FOpen(const char* path, const char* mode) {
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
  FILE* result;
  if (fopen_s(&result, path, mode) == 0) {
    return result;
  } else {
    return nullptr;
  }
#else
  FILE* file = base::Fopen(path, mode);
  if (file == nullptr) return nullptr;
  struct stat file_stat;
  if (fstat(fileno(file), &file_stat) != 0) return nullptr;
  bool is_regular_file = ((file_stat.st_mode & S_IFREG) != 0);
  if (is_regular_file) return file;
  base::Fclose(file);
  return nullptr;
#endif
}

static char* ReadChars(const char* name, int* size_out) {
  if (Shell::options.read_from_tcp_port >= 0) {
    return Shell::ReadCharsFromTcpPort(name, size_out);
  }

  FILE* file = FOpen(name, "rb");
  if (file == nullptr) return nullptr;

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (size_t i = 0; i < size;) {
    i += fread(&chars[i], 1, size - i, file);
    if (ferror(file)) {
      base::Fclose(file);
      delete[] chars;
      return nullptr;
    }
  }
  base::Fclose(file);
  *size_out = static_cast<int>(size);
  return chars;
}

void Shell::ReadBuffer(const v8::FunctionCallbackInfo<v8::Value>& args) {
  static_assert(sizeof(char) == sizeof(uint8_t),
                "char and uint8_t should both have 1 byte");
  Isolate* isolate = args.GetIsolate();
  String::Utf8Value filename(isolate, args[0]);
  int length;
  if (*filename == nullptr) {
    Throw(isolate, "Error loading file");
    return;
  }

  uint8_t* data = reinterpret_cast<uint8_t*>(ReadChars(*filename, &length));
  if (data == nullptr) {
    Throw(isolate, "Error reading file");
    return;
  }
  std::unique_ptr<v8::BackingStore> backing_store =
      ArrayBuffer::NewBackingStore(
          data, length,
          [](void* data, size_t length, void*) {
            delete[] reinterpret_cast<uint8_t*>(data);
          },
          nullptr);
  Local<v8::ArrayBuffer> buffer =
      ArrayBuffer::New(isolate, std::move(backing_store));

  args.GetReturnValue().Set(buffer);
}

// Reads a file into a v8 string.
Local<String> Shell::ReadFile(Isolate* isolate, const char* name) {
  std::unique_ptr<base::OS::MemoryMappedFile> file(
      base::OS::MemoryMappedFile::open(
          name, base::OS::MemoryMappedFile::FileMode::kReadOnly));
  if (!file) return Local<String>();

  int size = static_cast<int>(file->size());
  char* chars = static_cast<char*>(file->memory());
  Local<String> result;
  if (i::FLAG_use_external_strings && i::String::IsAscii(chars, size)) {
    String::ExternalOneByteStringResource* resource =
        new ExternalOwningOneByteStringResource(std::move(file));
    result = String::NewExternalOneByte(isolate, resource).ToLocalChecked();
  } else {
    result = String::NewFromUtf8(isolate, chars, NewStringType::kNormal, size)
                 .ToLocalChecked();
  }
  return result;
}

void Shell::RunShell(Isolate* isolate) {
  HandleScope outer_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, evaluation_context_);
  v8::Context::Scope context_scope(context);
  PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
  Local<String> name = String::NewFromUtf8Literal(isolate, "(d8)");
  printf("V8 version %s\n", V8::GetVersion());
  while (true) {
    HandleScope inner_scope(isolate);
    printf("d8> ");
    Local<String> input = Shell::ReadFromStdin(isolate);
    if (input.IsEmpty()) break;
    ExecuteString(isolate, input, name, kPrintResult, kReportExceptions,
                  kProcessMessageQueue);
  }
  printf("\n");
  // We need to explicitly clean up the module embedder data for
  // the interative shell context.
  DisposeModuleEmbedderData(context);
}

class InspectorFrontend final : public v8_inspector::V8Inspector::Channel {
 public:
  explicit InspectorFrontend(Local<Context> context) {
    isolate_ = context->GetIsolate();
    context_.Reset(isolate_, context);
  }
  ~InspectorFrontend() override = default;

 private:
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    Send(message->string());
  }
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    Send(message->string());
  }
  void flushProtocolNotifications() override {}

  void Send(const v8_inspector::StringView& string) {
    v8::Isolate::AllowJavascriptExecutionScope allow_script(isolate_);
    v8::HandleScope handle_scope(isolate_);
    int length = static_cast<int>(string.length());
    DCHECK_LT(length, v8::String::kMaxLength);
    Local<String> message =
        (string.is8Bit()
             ? v8::String::NewFromOneByte(
                   isolate_,
                   reinterpret_cast<const uint8_t*>(string.characters8()),
                   v8::NewStringType::kNormal, length)
             : v8::String::NewFromTwoByte(
                   isolate_,
                   reinterpret_cast<const uint16_t*>(string.characters16()),
                   v8::NewStringType::kNormal, length))
            .ToLocalChecked();
    Local<String> callback_name = v8::String::NewFromUtf8Literal(
        isolate_, "receive", NewStringType::kInternalized);
    Local<Context> context = context_.Get(isolate_);
    Local<Value> callback =
        context->Global()->Get(context, callback_name).ToLocalChecked();
    if (callback->IsFunction()) {
      v8::TryCatch try_catch(isolate_);
      Local<Value> args[] = {message};
      USE(callback.As<Function>()->Call(context, Undefined(isolate_), 1, args));
#ifdef DEBUG
      if (try_catch.HasCaught()) {
        Local<Object> exception = try_catch.Exception().As<Object>();
        Local<String> key = v8::String::NewFromUtf8Literal(
            isolate_, "message", NewStringType::kInternalized);
        Local<String> expected = v8::String::NewFromUtf8Literal(
            isolate_, "Maximum call stack size exceeded");
        Local<Value> value = exception->Get(context, key).ToLocalChecked();
        DCHECK(value->StrictEquals(expected));
      }
#endif
    }
  }

  Isolate* isolate_;
  Global<Context> context_;
};

class InspectorClient : public v8_inspector::V8InspectorClient {
 public:
  InspectorClient(Local<Context> context, bool connect) {
    if (!connect) return;
    isolate_ = context->GetIsolate();
    channel_.reset(new InspectorFrontend(context));
    inspector_ = v8_inspector::V8Inspector::create(isolate_, this);
    session_ =
        inspector_->connect(1, channel_.get(), v8_inspector::StringView());
    context->SetAlignedPointerInEmbedderData(kInspectorClientIndex, this);
    inspector_->contextCreated(v8_inspector::V8ContextInfo(
        context, kContextGroupId, v8_inspector::StringView()));

    Local<Value> function =
        FunctionTemplate::New(isolate_, SendInspectorMessage)
            ->GetFunction(context)
            .ToLocalChecked();
    Local<String> function_name = String::NewFromUtf8Literal(
        isolate_, "send", NewStringType::kInternalized);
    CHECK(context->Global()->Set(context, function_name, function).FromJust());

    context_.Reset(isolate_, context);
  }

  void runMessageLoopOnPause(int contextGroupId) override {
    v8::Isolate::AllowJavascriptExecutionScope allow_script(isolate_);
    v8::HandleScope handle_scope(isolate_);
    Local<String> callback_name = v8::String::NewFromUtf8Literal(
        isolate_, "handleInspectorMessage", NewStringType::kInternalized);
    Local<Context> context = context_.Get(isolate_);
    Local<Value> callback =
        context->Global()->Get(context, callback_name).ToLocalChecked();
    if (!callback->IsFunction()) return;

    v8::TryCatch try_catch(isolate_);
    try_catch.SetVerbose(true);
    is_paused = true;

    while (is_paused) {
      USE(callback.As<Function>()->Call(context, Undefined(isolate_), 0, {}));
      if (try_catch.HasCaught()) {
        is_paused = false;
      }
    }
  }

  void quitMessageLoopOnPause() override { is_paused = false; }

 private:
  static v8_inspector::V8InspectorSession* GetSession(Local<Context> context) {
    InspectorClient* inspector_client = static_cast<InspectorClient*>(
        context->GetAlignedPointerFromEmbedderData(kInspectorClientIndex));
    return inspector_client->session_.get();
  }

  Local<Context> ensureDefaultContextInGroup(int group_id) override {
    DCHECK(isolate_);
    DCHECK_EQ(kContextGroupId, group_id);
    return context_.Get(isolate_);
  }

  static void SendInspectorMessage(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = args.GetIsolate();
    v8::HandleScope handle_scope(isolate);
    Local<Context> context = isolate->GetCurrentContext();
    args.GetReturnValue().Set(Undefined(isolate));
    Local<String> message = args[0]->ToString(context).ToLocalChecked();
    v8_inspector::V8InspectorSession* session =
        InspectorClient::GetSession(context);
    int length = message->Length();
    std::unique_ptr<uint16_t[]> buffer(new uint16_t[length]);
    message->Write(isolate, buffer.get(), 0, length);
    v8_inspector::StringView message_view(buffer.get(), length);
    {
      v8::SealHandleScope seal_handle_scope(isolate);
      session->dispatchProtocolMessage(message_view);
    }
    args.GetReturnValue().Set(True(isolate));
  }

  static const int kContextGroupId = 1;

  std::unique_ptr<v8_inspector::V8Inspector> inspector_;
  std::unique_ptr<v8_inspector::V8InspectorSession> session_;
  std::unique_ptr<v8_inspector::V8Inspector::Channel> channel_;
  bool is_paused = false;
  Global<Context> context_;
  Isolate* isolate_;
};

SourceGroup::~SourceGroup() {
  delete thread_;
  thread_ = nullptr;
}

bool ends_with(const char* input, const char* suffix) {
  size_t input_length = strlen(input);
  size_t suffix_length = strlen(suffix);
  if (suffix_length <= input_length) {
    return strcmp(input + input_length - suffix_length, suffix) == 0;
  }
  return false;
}

bool SourceGroup::Execute(Isolate* isolate) {
  bool success = true;
#ifdef V8_FUZZILLI
  if (fuzzilli_reprl) {
    HandleScope handle_scope(isolate);
    Local<String> file_name =
        String::NewFromUtf8(isolate, "fuzzcode.js", NewStringType::kNormal)
            .ToLocalChecked();

    size_t script_size;
    CHECK_EQ(read(REPRL_CRFD, &script_size, 8), 8);
    char* buffer = new char[script_size + 1];
    char* ptr = buffer;
    size_t remaining = script_size;
    while (remaining > 0) {
      ssize_t rv = read(REPRL_DRFD, ptr, remaining);
      CHECK_GE(rv, 0);
      remaining -= rv;
      ptr += rv;
    }
    buffer[script_size] = 0;

    Local<String> source =
        String::NewFromUtf8(isolate, buffer, NewStringType::kNormal)
            .ToLocalChecked();
    delete[] buffer;
    Shell::set_script_executed();
    if (!Shell::ExecuteString(isolate, source, file_name, Shell::kNoPrintResult,
                              Shell::kReportExceptions,
                              Shell::kNoProcessMessageQueue)) {
      return false;
    }
  }
#endif  // V8_FUZZILLI
  for (int i = begin_offset_; i < end_offset_; ++i) {
    const char* arg = argv_[i];
    if (strcmp(arg, "-e") == 0 && i + 1 < end_offset_) {
      // Execute argument given to -e option directly.
      HandleScope handle_scope(isolate);
      Local<String> file_name = String::NewFromUtf8Literal(isolate, "unnamed");
      Local<String> source =
          String::NewFromUtf8(isolate, argv_[i + 1]).ToLocalChecked();
      Shell::set_script_executed();
      if (!Shell::ExecuteString(isolate, source, file_name,
                                Shell::kNoPrintResult, Shell::kReportExceptions,
                                Shell::kNoProcessMessageQueue)) {
        success = false;
        break;
      }
      ++i;
      continue;
    } else if (ends_with(arg, ".mjs")) {
      Shell::set_script_executed();
      if (!Shell::ExecuteModule(isolate, arg)) {
        success = false;
        break;
      }
      continue;
    } else if (strcmp(arg, "--module") == 0 && i + 1 < end_offset_) {
      // Treat the next file as a module.
      arg = argv_[++i];
      Shell::set_script_executed();
      if (!Shell::ExecuteModule(isolate, arg)) {
        success = false;
        break;
      }
      continue;
    } else if (arg[0] == '-') {
      // Ignore other options. They have been parsed already.
      continue;
    }

    // Use all other arguments as names of files to load and run.
    HandleScope handle_scope(isolate);
    Local<String> file_name =
        String::NewFromUtf8(isolate, arg).ToLocalChecked();
    Local<String> source = ReadFile(isolate, arg);
    if (source.IsEmpty()) {
      printf("Error reading '%s'\n", arg);
      base::OS::ExitProcess(1);
    }
    Shell::set_script_executed();
    Shell::update_script_size(source->Length());
    if (!Shell::ExecuteString(isolate, source, file_name, Shell::kNoPrintResult,
                              Shell::kReportExceptions,
                              Shell::kProcessMessageQueue)) {
      success = false;
      break;
    }
  }
  return success;
}

Local<String> SourceGroup::ReadFile(Isolate* isolate, const char* name) {
  return Shell::ReadFile(isolate, name);
}

SourceGroup::IsolateThread::IsolateThread(SourceGroup* group)
    : base::Thread(GetThreadOptions("IsolateThread")), group_(group) {}

void SourceGroup::ExecuteInThread() {
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = Shell::array_buffer_allocator;
  Isolate* isolate = Isolate::New(create_params);
  Shell::SetWaitUntilDone(isolate, false);
  D8Console console(isolate);
  Shell::Initialize(isolate, &console, false);

  for (int i = 0; i < Shell::options.stress_runs; ++i) {
    next_semaphore_.Wait();
    {
      Isolate::Scope iscope(isolate);
      PerIsolateData data(isolate);
      {
        HandleScope scope(isolate);
        Local<Context> context = Shell::CreateEvaluationContext(isolate);
        {
          Context::Scope cscope(context);
          InspectorClient inspector_client(context,
                                           Shell::options.enable_inspector);
          PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
          Execute(isolate);
          Shell::CompleteMessageLoop(isolate);
        }
        DisposeModuleEmbedderData(context);
      }
      Shell::CollectGarbage(isolate);
    }
    done_semaphore_.Signal();
  }

  isolate->Dispose();
}

void SourceGroup::StartExecuteInThread() {
  if (thread_ == nullptr) {
    thread_ = new IsolateThread(this);
    CHECK(thread_->Start());
  }
  next_semaphore_.Signal();
}

void SourceGroup::WaitForThread() {
  if (thread_ == nullptr) return;
  done_semaphore_.Wait();
}

void SourceGroup::JoinThread() {
  if (thread_ == nullptr) return;
  thread_->Join();
}

void SerializationDataQueue::Enqueue(std::unique_ptr<SerializationData> data) {
  base::MutexGuard lock_guard(&mutex_);
  data_.push_back(std::move(data));
}

bool SerializationDataQueue::Dequeue(
    std::unique_ptr<SerializationData>* out_data) {
  out_data->reset();
  base::MutexGuard lock_guard(&mutex_);
  if (data_.empty()) return false;
  *out_data = std::move(data_[0]);
  data_.erase(data_.begin());
  return true;
}

bool SerializationDataQueue::IsEmpty() {
  base::MutexGuard lock_guard(&mutex_);
  return data_.empty();
}

void SerializationDataQueue::Clear() {
  base::MutexGuard lock_guard(&mutex_);
  data_.clear();
}

Worker::Worker(const char* script) : script_(i::StrDup(script)) {
  running_.store(false);
}

Worker::~Worker() {
  DCHECK_NULL(isolate_);

  delete thread_;
  thread_ = nullptr;
  delete[] script_;
  script_ = nullptr;
}

bool Worker::StartWorkerThread(std::shared_ptr<Worker> worker) {
  worker->running_.store(true);
  auto thread = new WorkerThread(worker);
  worker->thread_ = thread;
  if (thread->Start()) {
    // Wait until the worker is ready to receive messages.
    worker->started_semaphore_.Wait();
    Shell::AddRunningWorker(std::move(worker));
    return true;
  }
  return false;
}

void Worker::WorkerThread::Run() {
  // Prevent a lifetime cycle from Worker -> WorkerThread -> Worker.
  // We must clear the worker_ field of the thread, but we keep the
  // worker alive via a stack root until the thread finishes execution
  // and removes itself from the running set. Thereafter the only
  // remaining reference can be from a JavaScript object via a Managed.
  auto worker = std::move(worker_);
  worker_ = nullptr;
  worker->ExecuteInThread();
  Shell::RemoveRunningWorker(worker);
}

class ProcessMessageTask : public i::CancelableTask {
 public:
  ProcessMessageTask(i::CancelableTaskManager* task_manager,
                     std::shared_ptr<Worker> worker,
                     std::unique_ptr<SerializationData> data)
      : i::CancelableTask(task_manager),
        worker_(worker),
        data_(std::move(data)) {}

  void RunInternal() override { worker_->ProcessMessage(std::move(data_)); }

 private:
  std::shared_ptr<Worker> worker_;
  std::unique_ptr<SerializationData> data_;
};

void Worker::PostMessage(std::unique_ptr<SerializationData> data) {
  // Hold the worker_mutex_ so that the worker thread can't delete task_runner_
  // after we've checked running_.
  base::MutexGuard lock_guard(&worker_mutex_);
  if (!running_.load()) {
    return;
  }
  std::unique_ptr<v8::Task> task(new ProcessMessageTask(
      task_manager_, shared_from_this(), std::move(data)));
  task_runner_->PostNonNestableTask(std::move(task));
}

class TerminateTask : public i::CancelableTask {
 public:
  TerminateTask(i::CancelableTaskManager* task_manager,
                std::shared_ptr<Worker> worker)
      : i::CancelableTask(task_manager), worker_(worker) {}

  void RunInternal() override {
    // Make sure the worker doesn't enter the task loop after processing this
    // task.
    worker_->running_.store(false);
  }

 private:
  std::shared_ptr<Worker> worker_;
};

std::unique_ptr<SerializationData> Worker::GetMessage() {
  std::unique_ptr<SerializationData> result;
  while (!out_queue_.Dequeue(&result)) {
    // If the worker is no longer running, and there are no messages in the
    // queue, don't expect any more messages from it.
    if (!running_.load()) {
      break;
    }
    out_semaphore_.Wait();
  }
  return result;
}

void Worker::Terminate() {
  // Hold the worker_mutex_ so that the worker thread can't delete task_runner_
  // after we've checked running_.
  base::MutexGuard lock_guard(&worker_mutex_);
  if (!running_.load()) {
    return;
  }
  // Post a task to wake up the worker thread.
  std::unique_ptr<v8::Task> task(
      new TerminateTask(task_manager_, shared_from_this()));
  task_runner_->PostTask(std::move(task));
}

void Worker::TerminateAndWaitForThread() {
  Terminate();
  thread_->Join();
}

void Worker::ProcessMessage(std::unique_ptr<SerializationData> data) {
  if (!running_.load()) {
    return;
  }

  DCHECK_NOT_NULL(isolate_);
  HandleScope scope(isolate_);
  Local<Context> context = context_.Get(isolate_);
  Context::Scope cscope(context);
  Local<Object> global = context->Global();

  // Get the message handler.
  MaybeLocal<Value> maybe_onmessage = global->Get(
      context, String::NewFromUtf8Literal(isolate_, "onmessage",
                                          NewStringType::kInternalized));
  Local<Value> onmessage;
  if (!maybe_onmessage.ToLocal(&onmessage) || !onmessage->IsFunction()) {
    return;
  }
  Local<Function> onmessage_fun = onmessage.As<Function>();

  v8::TryCatch try_catch(isolate_);
  try_catch.SetVerbose(true);
  Local<Value> value;
  if (Shell::DeserializeValue(isolate_, std::move(data)).ToLocal(&value)) {
    Local<Value> argv[] = {value};
    MaybeLocal<Value> result = onmessage_fun->Call(context, global, 1, argv);
    USE(result);
  }
}

void Worker::ProcessMessages() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate_);
  i::SaveAndSwitchContext saved_context(i_isolate, i::Context());
  SealHandleScope shs(isolate_);
  while (running_.load() && v8::platform::PumpMessageLoop(
                                g_default_platform, isolate_,
                                platform::MessageLoopBehavior::kWaitForWork)) {
    if (running_.load()) {
      MicrotasksScope::PerformCheckpoint(isolate_);
    }
  }
}

void Worker::ExecuteInThread() {
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = Shell::array_buffer_allocator;
  isolate_ = Isolate::New(create_params);
  {
    base::MutexGuard lock_guard(&worker_mutex_);
    task_runner_ = g_default_platform->GetForegroundTaskRunner(isolate_);
    task_manager_ =
        reinterpret_cast<i::Isolate*>(isolate_)->cancelable_task_manager();
  }
  // The Worker is now ready to receive messages.
  started_semaphore_.Signal();

  D8Console console(isolate_);
  Shell::Initialize(isolate_, &console, false);
  {
    Isolate::Scope iscope(isolate_);
    {
      HandleScope scope(isolate_);
      PerIsolateData data(isolate_);
      Local<Context> context = Shell::CreateEvaluationContext(isolate_);
      context_.Reset(isolate_, context);
      {
        Context::Scope cscope(context);
        PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate_));

        Local<Object> global = context->Global();
        Local<Value> this_value = External::New(isolate_, this);
        Local<FunctionTemplate> postmessage_fun_template =
            FunctionTemplate::New(isolate_, PostMessageOut, this_value);

        Local<Function> postmessage_fun;
        if (postmessage_fun_template->GetFunction(context).ToLocal(
                &postmessage_fun)) {
          global
              ->Set(context,
                    v8::String::NewFromUtf8Literal(
                        isolate_, "postMessage", NewStringType::kInternalized),
                    postmessage_fun)
              .FromJust();
        }

        // First run the script
        Local<String> file_name =
            String::NewFromUtf8Literal(isolate_, "unnamed");
        Local<String> source =
            String::NewFromUtf8(isolate_, script_).ToLocalChecked();
        if (Shell::ExecuteString(
                isolate_, source, file_name, Shell::kNoPrintResult,
                Shell::kReportExceptions, Shell::kProcessMessageQueue)) {
          // Check that there's a message handler
          MaybeLocal<Value> maybe_onmessage = global->Get(
              context,
              String::NewFromUtf8Literal(isolate_, "onmessage",
                                         NewStringType::kInternalized));
          Local<Value> onmessage;
          if (maybe_onmessage.ToLocal(&onmessage) && onmessage->IsFunction()) {
            // Now wait for messages
            ProcessMessages();
          }
        }
      }
      DisposeModuleEmbedderData(context);
    }
    Shell::CollectGarbage(isolate_);
  }
  // TODO(cbruni): Check for unhandled promises here.
  {
    // Hold the mutex to ensure running_ and task_runner_ change state
    // atomically (see Worker::PostMessage which reads them).
    base::MutexGuard lock_guard(&worker_mutex_);
    running_.store(false);
    task_runner_.reset();
    task_manager_ = nullptr;
  }
  context_.Reset();
  platform::NotifyIsolateShutdown(g_default_platform, isolate_);
  isolate_->Dispose();
  isolate_ = nullptr;

  // Post nullptr to wake the thread waiting on GetMessage() if there is one.
  out_queue_.Enqueue(nullptr);
  out_semaphore_.Signal();
}

void Worker::PostMessageOut(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);

  if (args.Length() < 1) {
    Throw(isolate, "Invalid argument");
    return;
  }

  Local<Value> message = args[0];
  Local<Value> transfer = Undefined(isolate);
  std::unique_ptr<SerializationData> data =
      Shell::SerializeValue(isolate, message, transfer);
  if (data) {
    DCHECK(args.Data()->IsExternal());
    Local<External> this_value = args.Data().As<External>();
    Worker* worker = static_cast<Worker*>(this_value->Value());
    worker->out_queue_.Enqueue(std::move(data));
    worker->out_semaphore_.Signal();
  }
}

bool Shell::SetOptions(int argc, char* argv[]) {
  bool logfile_per_isolate = false;
  bool no_always_opt = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--") == 0) {
      argv[i] = nullptr;
      for (int j = i + 1; j < argc; j++) {
        options.arguments.push_back(argv[j]);
        argv[j] = nullptr;
      }
      break;
    } else if (strcmp(argv[i], "--no-arguments") == 0) {
      options.include_arguments = false;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--simulate-errors") == 0) {
      options.simulate_errors = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--stress-opt") == 0) {
      options.stress_opt = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--nostress-opt") == 0 ||
               strcmp(argv[i], "--no-stress-opt") == 0) {
      options.stress_opt = false;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--stress-snapshot") == 0) {
      options.stress_snapshot = true;
      // Incremental marking is incompatible with the stress_snapshot mode;
      // specifically, serialization may clear bytecode arrays from shared
      // function infos which the MarkCompactCollector (running concurrently)
      // may still need. See also https://crbug.com/v8/10882.
      //
      // We thus force the implication
      //
      //   --stress-snapshot ~~> --no-incremental-marking
      //
      // Note: This is not an issue in production because we don't clear SFI's
      // there (that only happens in mksnapshot and in --stress-snapshot mode).
      i::FLAG_incremental_marking = false;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--nostress-snapshot") == 0 ||
               strcmp(argv[i], "--no-stress-snapshot") == 0) {
      options.stress_snapshot = false;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--noalways-opt") == 0 ||
               strcmp(argv[i], "--no-always-opt") == 0) {
      no_always_opt = true;
    } else if (strcmp(argv[i], "--fuzzing") == 0 ||
               strcmp(argv[i], "--no-abort-on-contradictory-flags") == 0 ||
               strcmp(argv[i], "--noabort-on-contradictory-flags") == 0) {
      check_d8_flag_contradictions = false;
    } else if (strcmp(argv[i], "--abort-on-contradictory-flags") == 0) {
      check_d8_flag_contradictions = true;
    } else if (strcmp(argv[i], "--logfile-per-isolate") == 0) {
      logfile_per_isolate = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--shell") == 0) {
      options.interactive_shell = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--test") == 0) {
      options.test_shell = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--notest") == 0 ||
               strcmp(argv[i], "--no-test") == 0) {
      options.test_shell = false;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--send-idle-notification") == 0) {
      options.send_idle_notification = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--invoke-weak-callbacks") == 0) {
      options.invoke_weak_callbacks = true;
      // TODO(jochen) See issue 3351
      options.send_idle_notification = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--omit-quit") == 0) {
      options.omit_quit = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--no-wait-for-background-tasks") == 0) {
      // TODO(herhut) Remove this flag once wasm compilation is fully
      // isolate-independent.
      options.wait_for_background_tasks = false;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "-f") == 0) {
      // Ignore any -f flags for compatibility with other stand-alone
      // JavaScript engines.
      continue;
    } else if (strcmp(argv[i], "--ignore-unhandled-promises") == 0) {
      options.ignore_unhandled_promises = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--isolate") == 0) {
      options.num_isolates++;
    } else if (strcmp(argv[i], "--throws") == 0) {
      options.expected_to_throw = true;
      argv[i] = nullptr;
    } else if (strncmp(argv[i], "--icu-data-file=", 16) == 0) {
      options.icu_data_file = argv[i] + 16;
      argv[i] = nullptr;
    } else if (strncmp(argv[i], "--icu-locale=", 13) == 0) {
      options.icu_locale = argv[i] + 13;
      argv[i] = nullptr;
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
    } else if (strncmp(argv[i], "--snapshot_blob=", 16) == 0) {
      options.snapshot_blob = argv[i] + 16;
      argv[i] = nullptr;
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
    } else if (strcmp(argv[i], "--cache") == 0 ||
               strncmp(argv[i], "--cache=", 8) == 0) {
      const char* value = argv[i] + 7;
      if (!*value || strncmp(value, "=code", 6) == 0) {
        options.compile_options = v8::ScriptCompiler::kNoCompileOptions;
        options.code_cache_options =
            ShellOptions::CodeCacheOptions::kProduceCache;
      } else if (strncmp(value, "=none", 6) == 0) {
        options.compile_options = v8::ScriptCompiler::kNoCompileOptions;
        options.code_cache_options =
            ShellOptions::CodeCacheOptions::kNoProduceCache;
      } else if (strncmp(value, "=after-execute", 15) == 0) {
        options.compile_options = v8::ScriptCompiler::kNoCompileOptions;
        options.code_cache_options =
            ShellOptions::CodeCacheOptions::kProduceCacheAfterExecute;
      } else if (strncmp(value, "=full-code-cache", 17) == 0) {
        options.compile_options = v8::ScriptCompiler::kEagerCompile;
        options.code_cache_options =
            ShellOptions::CodeCacheOptions::kProduceCache;
      } else {
        printf("Unknown option to --cache.\n");
        return false;
      }
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--streaming-compile") == 0) {
      options.streaming_compile = true;
      argv[i] = nullptr;
    } else if ((strcmp(argv[i], "--no-streaming-compile") == 0) ||
               (strcmp(argv[i], "--nostreaming-compile") == 0)) {
      options.streaming_compile = false;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--enable-tracing") == 0) {
      options.trace_enabled = true;
      argv[i] = nullptr;
    } else if (strncmp(argv[i], "--trace-path=", 13) == 0) {
      options.trace_path = argv[i] + 13;
      argv[i] = nullptr;
    } else if (strncmp(argv[i], "--trace-config=", 15) == 0) {
      options.trace_config = argv[i] + 15;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--enable-inspector") == 0) {
      options.enable_inspector = true;
      argv[i] = nullptr;
    } else if (strncmp(argv[i], "--lcov=", 7) == 0) {
      options.lcov_file = argv[i] + 7;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--disable-in-process-stack-traces") == 0) {
      options.disable_in_process_stack_traces = true;
      argv[i] = nullptr;
#ifdef V8_OS_POSIX
    } else if (strncmp(argv[i], "--read-from-tcp-port=", 21) == 0) {
      options.read_from_tcp_port = atoi(argv[i] + 21);
      argv[i] = nullptr;
#endif  // V8_OS_POSIX
    } else if (strcmp(argv[i], "--enable-os-system") == 0) {
      options.enable_os_system = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--quiet-load") == 0) {
      options.quiet_load = true;
      argv[i] = nullptr;
    } else if (strncmp(argv[i], "--thread-pool-size=", 19) == 0) {
      options.thread_pool_size = atoi(argv[i] + 19);
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--stress-delay-tasks") == 0) {
      // Delay execution of tasks by 0-100ms randomly (based on --random-seed).
      options.stress_delay_tasks = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--cpu-profiler") == 0) {
      options.cpu_profiler = true;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--cpu-profiler-print") == 0) {
      options.cpu_profiler = true;
      options.cpu_profiler_print = true;
      argv[i] = nullptr;
#ifdef V8_FUZZILLI
    } else if (strcmp(argv[i], "--no-fuzzilli-enable-builtins-coverage") == 0) {
      options.fuzzilli_enable_builtins_coverage = false;
      argv[i] = nullptr;
    } else if (strcmp(argv[i], "--fuzzilli-coverage-statistics") == 0) {
      options.fuzzilli_coverage_statistics = true;
      argv[i] = nullptr;
#endif
    } else if (strcmp(argv[i], "--fuzzy-module-file-extensions") == 0) {
      options.fuzzy_module_file_extensions = true;
      argv[i] = nullptr;
#ifdef V8_ENABLE_SYSTEM_INSTRUMENTATION
    } else if (strcmp(argv[i], "--enable-system-instrumentation") == 0) {
      options.enable_system_instrumentation = true;
      options.trace_enabled = true;
      argv[i] = nullptr;
#endif
    }
  }

  if (options.stress_opt && no_always_opt && check_d8_flag_contradictions) {
    FATAL("Flag --no-always-opt is incompatible with --stress-opt.");
  }

  const char* usage =
      "Synopsis:\n"
      "  shell [options] [--shell] [<file>...]\n"
      "  d8 [options] [-e <string>] [--shell] [[--module] <file>...]\n\n"
      "  -e        execute a string in V8\n"
      "  --shell   run an interactive JavaScript shell\n"
      "  --module  execute a file as a JavaScript module\n\n";
  using HelpOptions = i::FlagList::HelpOptions;
  i::FLAG_abort_on_contradictory_flags = true;
  i::FlagList::SetFlagsFromCommandLine(&argc, argv, true,
                                       HelpOptions(HelpOptions::kExit, usage));
  options.mock_arraybuffer_allocator = i::FLAG_mock_arraybuffer_allocator;
  options.mock_arraybuffer_allocator_limit =
      i::FLAG_mock_arraybuffer_allocator_limit;
#if V8_OS_LINUX
  options.multi_mapped_mock_allocator = i::FLAG_multi_mapped_mock_allocator;
#endif

  // Set up isolated source groups.
  options.isolate_sources = new SourceGroup[options.num_isolates];
  SourceGroup* current = options.isolate_sources;
  current->Begin(argv, 1);
  for (int i = 1; i < argc; i++) {
    const char* str = argv[i];
    if (strcmp(str, "--isolate") == 0) {
      current->End(i);
      current++;
      current->Begin(argv, i + 1);
    } else if (strcmp(str, "--module") == 0) {
      // Pass on to SourceGroup, which understands this option.
    } else if (strncmp(str, "--", 2) == 0) {
      if (!i::FLAG_correctness_fuzzer_suppressions) {
        printf("Warning: unknown flag %s.\nTry --help for options\n", str);
      }
    } else if (strcmp(str, "-e") == 0 && i + 1 < argc) {
      set_script_executed();
    } else if (strncmp(str, "-", 1) != 0) {
      // Not a flag, so it must be a script to execute.
      set_script_executed();
    }
  }
  current->End(argc);

  if (!logfile_per_isolate && options.num_isolates) {
    V8::SetFlagsFromString("--no-logfile-per-isolate");
  }

  return true;
}

int Shell::RunMain(Isolate* isolate, bool last_run) {
  for (int i = 1; i < options.num_isolates; ++i) {
    options.isolate_sources[i].StartExecuteInThread();
  }
  bool success = true;
  {
    SetWaitUntilDone(isolate, false);
    if (options.lcov_file) {
      debug::Coverage::SelectMode(isolate, debug::CoverageMode::kBlockCount);
    }
    HandleScope scope(isolate);
    Local<Context> context = CreateEvaluationContext(isolate);
    bool use_existing_context = last_run && use_interactive_shell();
    if (use_existing_context) {
      // Keep using the same context in the interactive shell.
      evaluation_context_.Reset(isolate, context);
    }
    {
      Context::Scope cscope(context);
      InspectorClient inspector_client(context, options.enable_inspector);
      PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
      if (!options.isolate_sources[0].Execute(isolate)) success = false;
      if (!CompleteMessageLoop(isolate)) success = false;
    }
    if (!use_existing_context) {
      DisposeModuleEmbedderData(context);
    }
    WriteLcovData(isolate, options.lcov_file);
    if (last_run && options.stress_snapshot) {
      static constexpr bool kClearRecompilableData = true;
      i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
      i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
      // TODO(jgruber,v8:10500): Don't deoptimize once we support serialization
      // of optimized code.
      i::Deoptimizer::DeoptimizeAll(i_isolate);
      i::Snapshot::ClearReconstructableDataForSerialization(
          i_isolate, kClearRecompilableData);
      i::Snapshot::SerializeDeserializeAndVerifyForTesting(i_isolate,
                                                           i_context);
    }
  }
  CollectGarbage(isolate);
  for (int i = 1; i < options.num_isolates; ++i) {
    if (last_run) {
      options.isolate_sources[i].JoinThread();
    } else {
      options.isolate_sources[i].WaitForThread();
    }
  }
  WaitForRunningWorkers();
  if (Shell::unhandled_promise_rejections_.load() > 0) {
    printf("%i pending unhandled Promise rejection(s) detected.\n",
           Shell::unhandled_promise_rejections_.load());
    success = false;
    // RunMain may be executed multiple times, e.g. in REPRL mode, so we have to
    // reset this counter.
    Shell::unhandled_promise_rejections_.store(0);
  }
  // In order to finish successfully, success must be != expected_to_throw.
  return success == Shell::options.expected_to_throw ? 1 : 0;
}

void Shell::CollectGarbage(Isolate* isolate) {
  if (options.send_idle_notification) {
    const double kLongIdlePauseInSeconds = 1.0;
    isolate->ContextDisposedNotification();
    isolate->IdleNotificationDeadline(
        g_platform->MonotonicallyIncreasingTime() + kLongIdlePauseInSeconds);
  }
  if (options.invoke_weak_callbacks) {
    // By sending a low memory notifications, we will try hard to collect all
    // garbage and will therefore also invoke all weak callbacks of actually
    // unreachable persistent handles.
    isolate->LowMemoryNotification();
  }
}

void Shell::SetWaitUntilDone(Isolate* isolate, bool value) {
  base::MutexGuard guard(isolate_status_lock_.Pointer());
  isolate_status_[isolate] = value;
}

void Shell::NotifyStartStreamingTask(Isolate* isolate) {
  DCHECK(options.streaming_compile);
  base::MutexGuard guard(isolate_status_lock_.Pointer());
  ++isolate_running_streaming_tasks_[isolate];
}

void Shell::NotifyFinishStreamingTask(Isolate* isolate) {
  DCHECK(options.streaming_compile);
  base::MutexGuard guard(isolate_status_lock_.Pointer());
  --isolate_running_streaming_tasks_[isolate];
  DCHECK_GE(isolate_running_streaming_tasks_[isolate], 0);
}

namespace {
bool RunSetTimeoutCallback(Isolate* isolate, bool* did_run) {
  PerIsolateData* data = PerIsolateData::Get(isolate);
  HandleScope handle_scope(isolate);
  Local<Function> callback;
  if (!data->GetTimeoutCallback().ToLocal(&callback)) return true;
  Local<Context> context;
  if (!data->GetTimeoutContext().ToLocal(&context)) return true;
  TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  Context::Scope context_scope(context);
  if (callback->Call(context, Undefined(isolate), 0, nullptr).IsEmpty()) {
    return false;
  }
  *did_run = true;
  return true;
}

bool ProcessMessages(
    Isolate* isolate,
    const std::function<platform::MessageLoopBehavior()>& behavior) {
  while (true) {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    i::SaveAndSwitchContext saved_context(i_isolate, i::Context());
    SealHandleScope shs(isolate);
    while (v8::platform::PumpMessageLoop(g_default_platform, isolate,
                                         behavior())) {
      MicrotasksScope::PerformCheckpoint(isolate);

      if (i::FLAG_verify_predictable) {
        // In predictable mode we push all background tasks into the foreground
        // task queue of the {kProcessGlobalPredictablePlatformWorkerTaskQueue}
        // isolate. We execute the tasks after one foreground task has been
        // executed.
        while (v8::platform::PumpMessageLoop(
            g_default_platform,
            kProcessGlobalPredictablePlatformWorkerTaskQueue, behavior())) {
        }
      }
    }
    if (g_default_platform->IdleTasksEnabled(isolate)) {
      v8::platform::RunIdleTasks(g_default_platform, isolate,
                                 50.0 / base::Time::kMillisecondsPerSecond);
    }
    bool ran_set_timeout = false;
    if (!RunSetTimeoutCallback(isolate, &ran_set_timeout)) {
      return false;
    }

    if (!ran_set_timeout) return true;
  }
  return true;
}
}  // anonymous namespace

bool Shell::CompleteMessageLoop(Isolate* isolate) {
  auto get_waiting_behaviour = [isolate]() {
    base::MutexGuard guard(isolate_status_lock_.Pointer());
    DCHECK_GT(isolate_status_.count(isolate), 0);
    bool should_wait = (options.wait_for_background_tasks &&
                        isolate->HasPendingBackgroundTasks()) ||
                       isolate_status_[isolate] ||
                       isolate_running_streaming_tasks_[isolate] > 0;
    return should_wait ? platform::MessageLoopBehavior::kWaitForWork
                       : platform::MessageLoopBehavior::kDoNotWait;
  };
  return ProcessMessages(isolate, get_waiting_behaviour);
}

bool Shell::EmptyMessageQueues(Isolate* isolate) {
  return ProcessMessages(
      isolate, []() { return platform::MessageLoopBehavior::kDoNotWait; });
}

void Shell::PostForegroundTask(Isolate* isolate, std::unique_ptr<Task> task) {
  g_default_platform->GetForegroundTaskRunner(isolate)->PostTask(
      std::move(task));
}

void Shell::PostBlockingBackgroundTask(std::unique_ptr<Task> task) {
  g_default_platform->CallBlockingTaskOnWorkerThread(std::move(task));
}

bool Shell::HandleUnhandledPromiseRejections(Isolate* isolate) {
  if (options.ignore_unhandled_promises) return true;
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int count = data->HandleUnhandledPromiseRejections();
  Shell::unhandled_promise_rejections_.store(
      Shell::unhandled_promise_rejections_.load() + count);
  return count == 0;
}

class Serializer : public ValueSerializer::Delegate {
 public:
  explicit Serializer(Isolate* isolate)
      : isolate_(isolate),
        serializer_(isolate, this),
        current_memory_usage_(0) {}

  Serializer(const Serializer&) = delete;
  Serializer& operator=(const Serializer&) = delete;

  Maybe<bool> WriteValue(Local<Context> context, Local<Value> value,
                         Local<Value> transfer) {
    bool ok;
    DCHECK(!data_);
    data_.reset(new SerializationData);
    if (!PrepareTransfer(context, transfer).To(&ok)) {
      return Nothing<bool>();
    }
    serializer_.WriteHeader();

    if (!serializer_.WriteValue(context, value).To(&ok)) {
      data_.reset();
      return Nothing<bool>();
    }

    if (!FinalizeTransfer().To(&ok)) {
      return Nothing<bool>();
    }

    std::pair<uint8_t*, size_t> pair = serializer_.Release();
    data_->data_.reset(pair.first);
    data_->size_ = pair.second;
    return Just(true);
  }

  std::unique_ptr<SerializationData> Release() { return std::move(data_); }

  void AppendBackingStoresTo(std::vector<std::shared_ptr<BackingStore>>* to) {
    to->insert(to->end(), std::make_move_iterator(backing_stores_.begin()),
               std::make_move_iterator(backing_stores_.end()));
    backing_stores_.clear();
  }

 protected:
  // Implements ValueSerializer::Delegate.
  void ThrowDataCloneError(Local<String> message) override {
    isolate_->ThrowException(Exception::Error(message));
  }

  Maybe<uint32_t> GetSharedArrayBufferId(
      Isolate* isolate, Local<SharedArrayBuffer> shared_array_buffer) override {
    DCHECK_NOT_NULL(data_);
    for (size_t index = 0; index < shared_array_buffers_.size(); ++index) {
      if (shared_array_buffers_[index] == shared_array_buffer) {
        return Just<uint32_t>(static_cast<uint32_t>(index));
      }
    }

    size_t index = shared_array_buffers_.size();
    shared_array_buffers_.emplace_back(isolate_, shared_array_buffer);
    data_->sab_backing_stores_.push_back(
        shared_array_buffer->GetBackingStore());
    return Just<uint32_t>(static_cast<uint32_t>(index));
  }

  Maybe<uint32_t> GetWasmModuleTransferId(
      Isolate* isolate, Local<WasmModuleObject> module) override {
    DCHECK_NOT_NULL(data_);
    for (size_t index = 0; index < wasm_modules_.size(); ++index) {
      if (wasm_modules_[index] == module) {
        return Just<uint32_t>(static_cast<uint32_t>(index));
      }
    }

    size_t index = wasm_modules_.size();
    wasm_modules_.emplace_back(isolate_, module);
    data_->compiled_wasm_modules_.push_back(module->GetCompiledModule());
    return Just<uint32_t>(static_cast<uint32_t>(index));
  }

  void* ReallocateBufferMemory(void* old_buffer, size_t size,
                               size_t* actual_size) override {
    // Not accurate, because we don't take into account reallocated buffers,
    // but this is fine for testing.
    current_memory_usage_ += size;
    if (current_memory_usage_ > kMaxSerializerMemoryUsage) return nullptr;

    void* result = base::Realloc(old_buffer, size);
    *actual_size = result ? size : 0;
    return result;
  }

  void FreeBufferMemory(void* buffer) override { base::Free(buffer); }

 private:
  Maybe<bool> PrepareTransfer(Local<Context> context, Local<Value> transfer) {
    if (transfer->IsArray()) {
      Local<Array> transfer_array = transfer.As<Array>();
      uint32_t length = transfer_array->Length();
      for (uint32_t i = 0; i < length; ++i) {
        Local<Value> element;
        if (transfer_array->Get(context, i).ToLocal(&element)) {
          if (!element->IsArrayBuffer()) {
            Throw(isolate_, "Transfer array elements must be an ArrayBuffer");
            return Nothing<bool>();
          }

          Local<ArrayBuffer> array_buffer = element.As<ArrayBuffer>();

          if (std::find(array_buffers_.begin(), array_buffers_.end(),
                        array_buffer) != array_buffers_.end()) {
            Throw(isolate_,
                  "ArrayBuffer occurs in the transfer array more than once");
            return Nothing<bool>();
          }

          serializer_.TransferArrayBuffer(
              static_cast<uint32_t>(array_buffers_.size()), array_buffer);
          array_buffers_.emplace_back(isolate_, array_buffer);
        } else {
          return Nothing<bool>();
        }
      }
      return Just(true);
    } else if (transfer->IsUndefined()) {
      return Just(true);
    } else {
      Throw(isolate_, "Transfer list must be an Array or undefined");
      return Nothing<bool>();
    }
  }

  Maybe<bool> FinalizeTransfer() {
    for (const auto& global_array_buffer : array_buffers_) {
      Local<ArrayBuffer> array_buffer =
          Local<ArrayBuffer>::New(isolate_, global_array_buffer);
      if (!array_buffer->IsDetachable()) {
        Throw(isolate_, "ArrayBuffer could not be transferred");
        return Nothing<bool>();
      }

      auto backing_store = array_buffer->GetBackingStore();
      data_->backing_stores_.push_back(std::move(backing_store));
      array_buffer->Detach();
    }

    return Just(true);
  }

  Isolate* isolate_;
  ValueSerializer serializer_;
  std::unique_ptr<SerializationData> data_;
  std::vector<Global<ArrayBuffer>> array_buffers_;
  std::vector<Global<SharedArrayBuffer>> shared_array_buffers_;
  std::vector<Global<WasmModuleObject>> wasm_modules_;
  std::vector<std::shared_ptr<v8::BackingStore>> backing_stores_;
  size_t current_memory_usage_;
};

class Deserializer : public ValueDeserializer::Delegate {
 public:
  Deserializer(Isolate* isolate, std::unique_ptr<SerializationData> data)
      : isolate_(isolate),
        deserializer_(isolate, data->data(), data->size(), this),
        data_(std::move(data)) {
    deserializer_.SetSupportsLegacyWireFormat(true);
  }

  Deserializer(const Deserializer&) = delete;
  Deserializer& operator=(const Deserializer&) = delete;

  MaybeLocal<Value> ReadValue(Local<Context> context) {
    bool read_header;
    if (!deserializer_.ReadHeader(context).To(&read_header)) {
      return MaybeLocal<Value>();
    }

    uint32_t index = 0;
    for (const auto& backing_store : data_->backing_stores()) {
      Local<ArrayBuffer> array_buffer =
          ArrayBuffer::New(isolate_, std::move(backing_store));
      deserializer_.TransferArrayBuffer(index++, array_buffer);
    }

    return deserializer_.ReadValue(context);
  }

  MaybeLocal<SharedArrayBuffer> GetSharedArrayBufferFromId(
      Isolate* isolate, uint32_t clone_id) override {
    DCHECK_NOT_NULL(data_);
    if (clone_id < data_->sab_backing_stores().size()) {
      return SharedArrayBuffer::New(
          isolate_, std::move(data_->sab_backing_stores().at(clone_id)));
    }
    return MaybeLocal<SharedArrayBuffer>();
  }

  MaybeLocal<WasmModuleObject> GetWasmModuleFromId(
      Isolate* isolate, uint32_t transfer_id) override {
    DCHECK_NOT_NULL(data_);
    if (transfer_id >= data_->compiled_wasm_modules().size()) return {};
    return WasmModuleObject::FromCompiledModule(
        isolate_, data_->compiled_wasm_modules().at(transfer_id));
  }

 private:
  Isolate* isolate_;
  ValueDeserializer deserializer_;
  std::unique_ptr<SerializationData> data_;
};

class D8Testing {
 public:
  /**
   * Get the number of runs of a given test that is required to get the full
   * stress coverage.
   */
  static int GetStressRuns() {
    if (internal::FLAG_stress_runs != 0) return internal::FLAG_stress_runs;
#ifdef DEBUG
    // In debug mode the code runs much slower so stressing will only make two
    // runs.
    return 2;
#else
    return 5;
#endif
  }

  /**
   * Indicate the number of the run which is about to start. The value of run
   * should be between 0 and one less than the result from GetStressRuns()
   */
  static void PrepareStressRun(int run) {
    static const char* kLazyOptimizations =
        "--prepare-always-opt "
        "--max-inlined-bytecode-size=999999 "
        "--max-inlined-bytecode-size-cumulative=999999 "
        "--noalways-opt";

    if (run == 0) {
      V8::SetFlagsFromString(kLazyOptimizations);
    } else if (run == GetStressRuns() - 1) {
      i::FLAG_always_opt = true;
    }
  }

  /**
   * Force deoptimization of all functions.
   */
  static void DeoptimizeAll(Isolate* isolate) {
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    i::HandleScope scope(i_isolate);
    i::Deoptimizer::DeoptimizeAll(i_isolate);
  }
};

std::unique_ptr<SerializationData> Shell::SerializeValue(
    Isolate* isolate, Local<Value> value, Local<Value> transfer) {
  bool ok;
  Local<Context> context = isolate->GetCurrentContext();
  Serializer serializer(isolate);
  std::unique_ptr<SerializationData> data;
  if (serializer.WriteValue(context, value, transfer).To(&ok)) {
    data = serializer.Release();
  }
  return data;
}

MaybeLocal<Value> Shell::DeserializeValue(
    Isolate* isolate, std::unique_ptr<SerializationData> data) {
  Local<Value> value;
  Local<Context> context = isolate->GetCurrentContext();
  Deserializer deserializer(isolate, std::move(data));
  return deserializer.ReadValue(context);
}

void Shell::AddRunningWorker(std::shared_ptr<Worker> worker) {
  workers_mutex_.Pointer()->AssertHeld();  // caller should hold the mutex.
  running_workers_.insert(worker);
}

void Shell::RemoveRunningWorker(const std::shared_ptr<Worker>& worker) {
  base::MutexGuard lock_guard(workers_mutex_.Pointer());
  auto it = running_workers_.find(worker);
  if (it != running_workers_.end()) running_workers_.erase(it);
}

void Shell::WaitForRunningWorkers() {
  // Make a copy of running_workers_, because we don't want to call
  // Worker::Terminate while holding the workers_mutex_ lock. Otherwise, if a
  // worker is about to create a new Worker, it would deadlock.
  std::unordered_set<std::shared_ptr<Worker>> workers_copy;
  {
    base::MutexGuard lock_guard(workers_mutex_.Pointer());
    allow_new_workers_ = false;
    workers_copy.swap(running_workers_);
  }

  for (auto& worker : workers_copy) {
    worker->TerminateAndWaitForThread();
  }

  // Now that all workers are terminated, we can re-enable Worker creation.
  base::MutexGuard lock_guard(workers_mutex_.Pointer());
  DCHECK(running_workers_.empty());
  allow_new_workers_ = true;
}

int Shell::Main(int argc, char* argv[]) {
  v8::base::EnsureConsoleOutput();
  if (!SetOptions(argc, argv)) return 1;

  v8::V8::InitializeICUDefaultLocation(argv[0], options.icu_data_file);

#ifdef V8_INTL_SUPPORT
  if (options.icu_locale != nullptr) {
    icu::Locale locale(options.icu_locale);
    UErrorCode error_code = U_ZERO_ERROR;
    icu::Locale::setDefault(locale, error_code);
  }
#endif  // V8_INTL_SUPPORT

  v8::platform::InProcessStackDumping in_process_stack_dumping =
      options.disable_in_process_stack_traces
          ? v8::platform::InProcessStackDumping::kDisabled
          : v8::platform::InProcessStackDumping::kEnabled;

  std::ofstream trace_file;
  std::unique_ptr<platform::tracing::TracingController> tracing;
  if (options.trace_enabled && !i::FLAG_verify_predictable) {
    tracing = std::make_unique<platform::tracing::TracingController>();
    const char* trace_path =
        options.trace_path ? options.trace_path : "v8_trace.json";
    trace_file.open(trace_path);
    if (!trace_file.good()) {
      printf("Cannot open trace file '%s' for writing: %s.\n", trace_path,
             strerror(errno));
      return 1;
    }

#ifdef V8_USE_PERFETTO
    // Set up the in-process backend that the tracing controller will connect
    // to.
    perfetto::TracingInitArgs init_args;
    init_args.backends = perfetto::BackendType::kInProcessBackend;
    perfetto::Tracing::Initialize(init_args);

    tracing->InitializeForPerfetto(&trace_file);
#else
    platform::tracing::TraceBuffer* trace_buffer = nullptr;
#if defined(V8_ENABLE_SYSTEM_INSTRUMENTATION)
    if (options.enable_system_instrumentation) {
      trace_buffer =
          platform::tracing::TraceBuffer::CreateTraceBufferRingBuffer(
              platform::tracing::TraceBuffer::kRingBufferChunks,
              platform::tracing::TraceWriter::
                  CreateSystemInstrumentationTraceWriter());
    }
#endif  // V8_ENABLE_SYSTEM_INSTRUMENTATION
    if (!trace_buffer) {
      trace_buffer =
          platform::tracing::TraceBuffer::CreateTraceBufferRingBuffer(
              platform::tracing::TraceBuffer::kRingBufferChunks,
              platform::tracing::TraceWriter::CreateJSONTraceWriter(
                  trace_file));
    }
    tracing->Initialize(trace_buffer);
#endif  // V8_USE_PERFETTO
  }

  platform::tracing::TracingController* tracing_controller = tracing.get();
  g_platform = v8::platform::NewDefaultPlatform(
      options.thread_pool_size, v8::platform::IdleTaskSupport::kEnabled,
      in_process_stack_dumping, std::move(tracing));
  g_default_platform = g_platform.get();
  if (i::FLAG_verify_predictable) {
    g_platform = MakePredictablePlatform(std::move(g_platform));
  }
  if (options.stress_delay_tasks) {
    int64_t random_seed = i::FLAG_fuzzer_random_seed;
    if (!random_seed) random_seed = i::FLAG_random_seed;
    // If random_seed is still 0 here, the {DelayedTasksPlatform} will choose a
    // random seed.
    g_platform = MakeDelayedTasksPlatform(std::move(g_platform), random_seed);
  }

  if (i::FLAG_trace_turbo_cfg_file == nullptr) {
    V8::SetFlagsFromString("--trace-turbo-cfg-file=turbo.cfg");
  }
  if (i::FLAG_redirect_code_traces_to == nullptr) {
    V8::SetFlagsFromString("--redirect-code-traces-to=code.asm");
  }
  v8::V8::InitializePlatform(g_platform.get());
  v8::V8::Initialize();
  if (options.snapshot_blob) {
    v8::V8::InitializeExternalStartupDataFromFile(options.snapshot_blob);
  } else {
    v8::V8::InitializeExternalStartupData(argv[0]);
  }
  int result = 0;
  Isolate::CreateParams create_params;
  ShellArrayBufferAllocator shell_array_buffer_allocator;
  MockArrayBufferAllocator mock_arraybuffer_allocator;
  const size_t memory_limit =
      options.mock_arraybuffer_allocator_limit * options.num_isolates;
  MockArrayBufferAllocatiorWithLimit mock_arraybuffer_allocator_with_limit(
      memory_limit >= options.mock_arraybuffer_allocator_limit
          ? memory_limit
          : std::numeric_limits<size_t>::max());
#if V8_OS_LINUX
  MultiMappedAllocator multi_mapped_mock_allocator;
#endif  // V8_OS_LINUX
  if (options.mock_arraybuffer_allocator) {
    if (memory_limit) {
      Shell::array_buffer_allocator = &mock_arraybuffer_allocator_with_limit;
    } else {
      Shell::array_buffer_allocator = &mock_arraybuffer_allocator;
    }
#if V8_OS_LINUX
  } else if (options.multi_mapped_mock_allocator) {
    Shell::array_buffer_allocator = &multi_mapped_mock_allocator;
#endif  // V8_OS_LINUX
  } else {
    Shell::array_buffer_allocator = &shell_array_buffer_allocator;
  }
  create_params.array_buffer_allocator = Shell::array_buffer_allocator;
#ifdef ENABLE_VTUNE_JIT_INTERFACE
  create_params.code_event_handler = vTune::GetVtuneCodeEventHandler();
#endif
  create_params.constraints.ConfigureDefaults(
      base::SysInfo::AmountOfPhysicalMemory(),
      base::SysInfo::AmountOfVirtualMemory());

  Shell::counter_map_ = new CounterMap();
  if (i::FLAG_dump_counters || i::FLAG_dump_counters_nvp ||
      i::TracingFlags::is_gc_stats_enabled()) {
    create_params.counter_lookup_callback = LookupCounter;
    create_params.create_histogram_callback = CreateHistogram;
    create_params.add_histogram_sample_callback = AddHistogramSample;
  }

  if (V8_TRAP_HANDLER_SUPPORTED && i::FLAG_wasm_trap_handler) {
    constexpr bool use_default_trap_handler = true;
    if (!v8::V8::EnableWebAssemblyTrapHandler(use_default_trap_handler)) {
      FATAL("Could not register trap handler");
    }
  }

  Isolate* isolate = Isolate::New(create_params);

  {
    D8Console console(isolate);
    Isolate::Scope scope(isolate);
    Initialize(isolate, &console);
    PerIsolateData data(isolate);

    // Fuzzilli REPRL = read-eval-print-loop
    do {
#ifdef V8_FUZZILLI
      if (fuzzilli_reprl) {
        unsigned action = 0;
        ssize_t nread = read(REPRL_CRFD, &action, 4);
        if (nread != 4 || action != 'cexe') {
          fprintf(stderr, "Unknown action: %u\n", action);
          _exit(-1);
        }
      }
#endif  // V8_FUZZILLI

      result = 0;

      if (options.trace_enabled) {
        platform::tracing::TraceConfig* trace_config;
        if (options.trace_config) {
          int size = 0;
          char* trace_config_json_str = ReadChars(options.trace_config, &size);
          trace_config = tracing::CreateTraceConfigFromJSON(
              isolate, trace_config_json_str);
          delete[] trace_config_json_str;
        } else {
          trace_config =
              platform::tracing::TraceConfig::CreateDefaultTraceConfig();
          if (options.enable_system_instrumentation) {
            trace_config->AddIncludedCategory("disabled-by-default-v8.compile");
          }
        }
        tracing_controller->StartTracing(trace_config);
      }

      CpuProfiler* cpu_profiler;
      if (options.cpu_profiler) {
        cpu_profiler = CpuProfiler::New(isolate);
        CpuProfilingOptions profile_options;
        cpu_profiler->StartProfiling(String::Empty(isolate), profile_options);
      }

      if (options.stress_opt) {
        options.stress_runs = D8Testing::GetStressRuns();
        for (int i = 0; i < options.stress_runs && result == 0; i++) {
          printf("============ Stress %d/%d ============\n", i + 1,
                 options.stress_runs.get());
          D8Testing::PrepareStressRun(i);
          bool last_run = i == options.stress_runs - 1;
          result = RunMain(isolate, last_run);
        }
        printf("======== Full Deoptimization =======\n");
        D8Testing::DeoptimizeAll(isolate);
      } else if (i::FLAG_stress_runs > 0) {
        options.stress_runs = i::FLAG_stress_runs;
        for (int i = 0; i < options.stress_runs && result == 0; i++) {
          printf("============ Run %d/%d ============\n", i + 1,
                 options.stress_runs.get());
          bool last_run = i == options.stress_runs - 1;
          result = RunMain(isolate, last_run);
        }
      } else if (options.code_cache_options !=
                 ShellOptions::CodeCacheOptions::kNoProduceCache) {
        printf("============ Run: Produce code cache ============\n");
        // First run to produce the cache
        Isolate::CreateParams create_params;
        create_params.array_buffer_allocator = Shell::array_buffer_allocator;
        i::FLAG_hash_seed ^= 1337;  // Use a different hash seed.
        Isolate* isolate2 = Isolate::New(create_params);
        i::FLAG_hash_seed ^= 1337;  // Restore old hash seed.
        {
          D8Console console(isolate2);
          Initialize(isolate2, &console);
          PerIsolateData data(isolate2);
          Isolate::Scope isolate_scope(isolate2);

          result = RunMain(isolate2, false);
        }
        isolate2->Dispose();

        // Change the options to consume cache
        DCHECK(options.compile_options == v8::ScriptCompiler::kEagerCompile ||
               options.compile_options ==
                   v8::ScriptCompiler::kNoCompileOptions);
        options.compile_options.Overwrite(
            v8::ScriptCompiler::kConsumeCodeCache);
        options.code_cache_options.Overwrite(
            ShellOptions::CodeCacheOptions::kNoProduceCache);

        printf("============ Run: Consume code cache ============\n");
        // Second run to consume the cache in current isolate
        result = RunMain(isolate, true);
        options.compile_options.Overwrite(
            v8::ScriptCompiler::kNoCompileOptions);
      } else {
        bool last_run = true;
        result = RunMain(isolate, last_run);
      }

      // Run interactive shell if explicitly requested or if no script has been
      // executed, but never on --test
      if (use_interactive_shell()) {
        RunShell(isolate);
      }

      if (i::FLAG_trace_ignition_dispatches &&
          i::FLAG_trace_ignition_dispatches_output_file != nullptr) {
        WriteIgnitionDispatchCountersFile(isolate);
      }

      if (options.cpu_profiler) {
        CpuProfile* profile =
            cpu_profiler->StopProfiling(String::Empty(isolate));
        if (options.cpu_profiler_print) {
          const internal::ProfileNode* root =
              reinterpret_cast<const internal::ProfileNode*>(
                  profile->GetTopDownRoot());
          root->Print(0);
        }
        profile->Delete();
        cpu_profiler->Dispose();
      }

      // Shut down contexts and collect garbage.
      cached_code_map_.clear();
      evaluation_context_.Reset();
      stringify_function_.Reset();
      CollectGarbage(isolate);
#ifdef V8_FUZZILLI
      // Send result to parent (fuzzilli) and reset edge guards.
      if (fuzzilli_reprl) {
        int status = result << 8;
        std::vector<bool> bitmap;
        if (options.fuzzilli_enable_builtins_coverage) {
          bitmap = i::BasicBlockProfiler::Get()->GetCoverageBitmap(
              reinterpret_cast<i::Isolate*>(isolate));
          cov_update_builtins_basic_block_coverage(bitmap);
        }
        if (options.fuzzilli_coverage_statistics) {
          int tot = 0;
          for (bool b : bitmap) {
            if (b) tot++;
          }
          static int iteration_counter = 0;
          std::ofstream covlog("covlog.txt", std::ios::app);
          covlog << iteration_counter << "\t" << tot << "\t"
                 << sanitizer_cov_count_discovered_edges() << "\t"
                 << bitmap.size() << std::endl;
          iteration_counter++;
        }
        // In REPRL mode, stdout and stderr can be regular files, so they need
        // to be flushed after every execution
        fflush(stdout);
        fflush(stderr);
        CHECK_EQ(write(REPRL_CWFD, &status, 4), 4);
        sanitizer_cov_reset_edgeguards();
        if (options.fuzzilli_enable_builtins_coverage) {
          i::BasicBlockProfiler::Get()->ResetCounts(
              reinterpret_cast<i::Isolate*>(isolate));
        }
      }
#endif  // V8_FUZZILLI
    } while (fuzzilli_reprl);
  }
  OnExit(isolate);

  V8::Dispose();
  V8::ShutdownPlatform();

  // Delete the platform explicitly here to write the tracing output to the
  // tracing file.
  if (options.trace_enabled) {
    tracing_controller->StopTracing();
  }
  g_platform.reset();
  return result;
}

}  // namespace v8

#ifndef GOOGLE3
int main(int argc, char* argv[]) { return v8::Shell::Main(argc, argv); }
#endif

#undef CHECK
#undef DCHECK
#undef TRACE_BS
