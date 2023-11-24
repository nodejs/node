// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api.h"

#include <algorithm>  // For min
#include <cmath>      // For isnan.
#include <limits>
#include <sstream>
#include <string>
#include <utility>  // For move
#include <vector>

#include "include/v8-callbacks.h"
#include "include/v8-cppgc.h"
#include "include/v8-date.h"
#include "include/v8-embedder-state-scope.h"
#include "include/v8-extension.h"
#include "include/v8-fast-api-calls.h"
#include "include/v8-function.h"
#include "include/v8-json.h"
#include "include/v8-locker.h"
#include "include/v8-primitive-object.h"
#include "include/v8-profiler.h"
#include "include/v8-unwinder-state.h"
#include "include/v8-util.h"
#include "include/v8-wasm.h"
#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/base/platform/memory.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/safe_conversions.h"
#include "src/base/utils/random-number-generator.h"
#include "src/baseline/baseline-batch-compiler.h"
#include "src/builtins/accessors.h"
#include "src/builtins/builtins-utils.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/script-details.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/compiler-dispatcher/lazy-compile-dispatcher.h"
#include "src/date/date.h"
#include "src/debug/liveedit.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/embedder-state.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/simulator.h"
#include "src/execution/v8threads.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/global-handles.h"
#include "src/handles/persistent-handles.h"
#include "src/handles/shared-object-conveyor-handles.h"
#include "src/handles/traced-handles.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/safepoint.h"
#include "src/init/bootstrapper.h"
#include "src/init/icu_util.h"
#include "src/init/startup-data-util.h"
#include "src/init/v8.h"
#include "src/json/json-parser.h"
#include "src/json/json-stringifier.h"
#include "src/logging/counters-scopes.h"
#include "src/logging/metrics.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/logging/tracing-flags.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/contexts.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/embedder-data-slot-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/ordered-hash-table-inl.h"
#include "src/objects/primitive-heap-object.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/property-details.h"
#include "src/objects/property.h"
#include "src/objects/prototype.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/synthetic-module-inl.h"
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
#include "src/roots/static-roots.h"
#include "src/runtime/runtime.h"
#include "src/sandbox/external-pointer.h"
#include "src/sandbox/sandbox.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/snapshot.h"
#include "src/snapshot/startup-serializer.h"  // For SerializedHandleChecker.
#include "src/strings/char-predicates-inl.h"
#include "src/strings/string-hasher.h"
#include "src/strings/unicode-inl.h"
#include "src/tracing/trace-event.h"
#include "src/utils/detachable-vector.h"
#include "src/utils/identity-map.h"
#include "src/utils/version.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/debug/debug-wasm-objects.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-serialization.h"
#endif  // V8_ENABLE_WEBASSEMBLY

#if V8_OS_LINUX || V8_OS_DARWIN || V8_OS_FREEBSD
#include <signal.h>
#include <unistd.h>

#if V8_ENABLE_WEBASSEMBLY
#include "include/v8-wasm-trap-handler-posix.h"
#include "src/trap-handler/handler-inside-posix.h"
#endif  // V8_ENABLE_WEBASSEMBLY

#endif  // V8_OS_LINUX || V8_OS_DARWIN || V8_OS_FREEBSD

#if V8_OS_WIN
#include <windows.h>

// This has to come after windows.h.
#include <versionhelpers.h>

#include "include/v8-wasm-trap-handler-win.h"
#include "src/trap-handler/handler-inside-win.h"
#if defined(V8_OS_WIN64)
#include "src/base/platform/wrappers.h"
#include "src/diagnostics/unwinding-info-win64.h"
#endif  // V8_OS_WIN64
#endif  // V8_OS_WIN

#if defined(V8_OS_WIN) && defined(V8_ENABLE_ETW_STACK_WALKING)
#include "src/diagnostics/etw-jit-win.h"
#endif

// Has to be the last include (doesn't have include guards):
#include "src/api/api-macros.h"

namespace v8 {

static OOMErrorCallback g_oom_error_callback = nullptr;

static ScriptOrigin GetScriptOriginForScript(i::Isolate* i_isolate,
                                             i::Handle<i::Script> script) {
  i::Handle<i::Object> scriptName(script->GetNameOrSourceURL(), i_isolate);
  i::Handle<i::Object> source_map_url(script->source_mapping_url(), i_isolate);
  i::Handle<i::Object> host_defined_options(script->host_defined_options(),
                                            i_isolate);
  ScriptOriginOptions options(script->origin_options());
  bool is_wasm = false;
#if V8_ENABLE_WEBASSEMBLY
  is_wasm = script->type() == i::Script::TYPE_WASM;
#endif  // V8_ENABLE_WEBASSEMBLY
  v8::ScriptOrigin origin(
      reinterpret_cast<v8::Isolate*>(i_isolate), Utils::ToLocal(scriptName),
      script->line_offset(), script->column_offset(),
      options.IsSharedCrossOrigin(), script->id(),
      Utils::ToLocal(source_map_url), options.IsOpaque(), is_wasm,
      options.IsModule(), Utils::ToLocal(host_defined_options));
  return origin;
}

// --- E x c e p t i o n   B e h a v i o r ---

// When V8 cannot allocate memory FatalProcessOutOfMemory is called. The default
// OOM error handler is called and execution is stopped.
void i::V8::FatalProcessOutOfMemory(i::Isolate* i_isolate, const char* location,
                                    const OOMDetails& details) {
  char last_few_messages[Heap::kTraceRingBufferSize + 1];
  char js_stacktrace[Heap::kStacktraceBufferSize + 1];
  i::HeapStats heap_stats;

  if (i_isolate == nullptr) {
    i_isolate = Isolate::TryGetCurrent();
  }

  if (i_isolate == nullptr) {
    // If the Isolate is not available for the current thread we cannot retrieve
    // memory information from the Isolate. Write easy-to-recognize values on
    // the stack.
    memset(last_few_messages, 0x0BADC0DE, Heap::kTraceRingBufferSize + 1);
    memset(js_stacktrace, 0x0BADC0DE, Heap::kStacktraceBufferSize + 1);
    memset(&heap_stats, 0xBADC0DE, sizeof(heap_stats));
    // Give the embedder a chance to handle the condition. If it doesn't,
    // just crash.
    if (g_oom_error_callback) g_oom_error_callback(location, details);
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
  if (i_isolate->heap()->HasBeenSetUp()) {
    // BUG(1718): Don't use the take_snapshot since we don't support
    // HeapObjectIterator here without doing a special GC.
    i_isolate->heap()->RecordStats(&heap_stats, false);
    if (!v8_flags.correctness_fuzzer_suppressions) {
      char* first_newline = strchr(last_few_messages, '\n');
      if (first_newline == nullptr || first_newline[1] == '\0')
        first_newline = last_few_messages;
      base::OS::PrintError("\n<--- Last few GCs --->\n%s\n", first_newline);
      base::OS::PrintError("\n<--- JS stacktrace --->\n%s\n", js_stacktrace);
    }
  }
  Utils::ReportOOMFailure(i_isolate, location, details);
  if (g_oom_error_callback) g_oom_error_callback(location, details);
  // If the fatal error handler returns, we stop execution.
  FATAL("API fatal error handler returned after process out of memory");
}

void i::V8::FatalProcessOutOfMemory(i::Isolate* i_isolate, const char* location,
                                    const char* detail) {
  OOMDetails details;
  details.detail = detail;
  FatalProcessOutOfMemory(i_isolate, location, details);
}

void Utils::ReportApiFailure(const char* location, const char* message) {
  i::Isolate* i_isolate = i::Isolate::TryGetCurrent();
  FatalErrorCallback callback = nullptr;
  if (i_isolate != nullptr) {
    callback = i_isolate->exception_behavior();
  }
  if (callback == nullptr) {
    base::OS::PrintError("\n#\n# Fatal error in %s\n# %s\n#\n\n", location,
                         message);
    base::OS::Abort();
  } else {
    callback(location, message);
  }
  i_isolate->SignalFatalError();
}

void Utils::ReportOOMFailure(i::Isolate* i_isolate, const char* location,
                             const OOMDetails& details) {
  if (auto oom_callback = i_isolate->oom_behavior()) {
    oom_callback(location, details);
  } else {
    // TODO(wfh): Remove this fallback once Blink is setting OOM handler. See
    // crbug.com/614440.
    FatalErrorCallback fatal_callback = i_isolate->exception_behavior();
    if (fatal_callback == nullptr) {
      base::OS::PrintError("\n#\n# Fatal %s OOM in %s\n#\n\n",
                           details.is_heap_oom ? "javascript" : "process",
                           location);
#ifdef V8_FUZZILLI
      // Ignore OOM crashes for fuzzing but exit with an error such that
      // samples are discarded by Fuzzilli.
      _exit(1);
#else
      base::OS::Abort();
#endif  // V8_FUZZILLI
    } else {
      fatal_callback(location,
                     details.is_heap_oom
                         ? "Allocation failed - JavaScript heap out of memory"
                         : "Allocation failed - process out of memory");
    }
  }
  i_isolate->SignalFatalError();
}

void V8::SetSnapshotDataBlob(StartupData* snapshot_blob) {
  i::V8::SetSnapshotBlob(snapshot_blob);
}

namespace {

#ifdef V8_ENABLE_SANDBOX
// ArrayBufferAllocator to use when the sandbox is enabled in which case all
// ArrayBuffer backing stores need to be allocated inside the sandbox.
class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override {
    return allocator_->Allocate(length);
  }

  void* AllocateUninitialized(size_t length) override {
    return Allocate(length);
  }

  void Free(void* data, size_t length) override {
    return allocator_->Free(data);
  }

 private:
  // Backend allocator shared by all ArrayBufferAllocator instances. This way,
  // there is a single region of virtual addres space reserved inside the
  // sandbox from which all ArrayBufferAllocators allocate their memory,
  // instead of each allocator creating their own region, which may cause
  // address space exhaustion inside the sandbox.
  // TODO(chromium:1340224): replace this with a more efficient allocator.
  class BackendAllocator {
   public:
    BackendAllocator() {
      CHECK(i::GetProcessWideSandbox()->is_initialized());
      VirtualAddressSpace* vas = i::GetProcessWideSandbox()->address_space();
      constexpr size_t max_backing_memory_size = 8ULL * i::GB;
      constexpr size_t min_backing_memory_size = 1ULL * i::GB;
      size_t backing_memory_size = max_backing_memory_size;
      i::Address backing_memory_base = 0;
      while (!backing_memory_base &&
             backing_memory_size >= min_backing_memory_size) {
        backing_memory_base = vas->AllocatePages(
            VirtualAddressSpace::kNoHint, backing_memory_size, kChunkSize,
            PagePermissions::kNoAccess);
        if (!backing_memory_base) {
          backing_memory_size /= 2;
        }
      }
      if (!backing_memory_base) {
        i::V8::FatalProcessOutOfMemory(
            nullptr,
            "Could not reserve backing memory for ArrayBufferAllocators");
      }
      DCHECK(IsAligned(backing_memory_base, kChunkSize));

      region_alloc_ = std::make_unique<base::RegionAllocator>(
          backing_memory_base, backing_memory_size, kAllocationGranularity);
      end_of_accessible_region_ = region_alloc_->begin();

      // Install a on-merge callback to discard or decommit unused pages.
      region_alloc_->set_on_merge_callback([this](i::Address start,
                                                  size_t size) {
        mutex_.AssertHeld();
        VirtualAddressSpace* vas = i::GetProcessWideSandbox()->address_space();
        i::Address end = start + size;
        if (end == region_alloc_->end() &&
            start <= end_of_accessible_region_ - kChunkSize) {
          // Can shrink the accessible region.
          i::Address new_end_of_accessible_region = RoundUp(start, kChunkSize);
          size_t size =
              end_of_accessible_region_ - new_end_of_accessible_region;
          CHECK(vas->DecommitPages(new_end_of_accessible_region, size));
          end_of_accessible_region_ = new_end_of_accessible_region;
        } else if (size >= 2 * kChunkSize) {
          // Can discard pages. The pages stay accessible, so the size of the
          // accessible region doesn't change.
          i::Address chunk_start = RoundUp(start, kChunkSize);
          i::Address chunk_end = RoundDown(start + size, kChunkSize);
          CHECK(vas->DiscardSystemPages(chunk_start, chunk_end - chunk_start));
        }
      });
    }

    ~BackendAllocator() {
      // The sandbox may already have been torn down, in which case there's no
      // need to free any memory.
      if (i::GetProcessWideSandbox()->is_initialized()) {
        VirtualAddressSpace* vas = i::GetProcessWideSandbox()->address_space();
        vas->FreePages(region_alloc_->begin(), region_alloc_->size());
      }
    }

    BackendAllocator(const BackendAllocator&) = delete;
    BackendAllocator& operator=(const BackendAllocator&) = delete;

    void* Allocate(size_t length) {
      base::MutexGuard guard(&mutex_);

      length = RoundUp(length, kAllocationGranularity);
      i::Address region = region_alloc_->AllocateRegion(length);
      if (region == base::RegionAllocator::kAllocationFailure) return nullptr;

      // Check if the memory is inside the accessible region. If not, grow it.
      i::Address end = region + length;
      size_t length_to_memset = length;
      if (end > end_of_accessible_region_) {
        VirtualAddressSpace* vas = i::GetProcessWideSandbox()->address_space();
        i::Address new_end_of_accessible_region = RoundUp(end, kChunkSize);
        size_t size = new_end_of_accessible_region - end_of_accessible_region_;
        if (!vas->SetPagePermissions(end_of_accessible_region_, size,
                                     PagePermissions::kReadWrite)) {
          CHECK(region_alloc_->FreeRegion(region));
          return nullptr;
        }

        // The pages that were inaccessible are guaranteed to be zeroed, so only
        // memset until the previous end of the accessible region.
        length_to_memset = end_of_accessible_region_ - region;
        end_of_accessible_region_ = new_end_of_accessible_region;
      }

      void* mem = reinterpret_cast<void*>(region);
      memset(mem, 0, length_to_memset);
      return mem;
    }

    void Free(void* data) {
      base::MutexGuard guard(&mutex_);
      region_alloc_->FreeRegion(reinterpret_cast<i::Address>(data));
    }

    static BackendAllocator* SharedInstance() {
      static base::LeakyObject<BackendAllocator> instance;
      return instance.get();
    }

   private:
    // Use a region allocator with a "page size" of 128 bytes as a reasonable
    // compromise between the number of regions it has to manage and the amount
    // of memory wasted due to rounding allocation sizes up to the page size.
    static constexpr size_t kAllocationGranularity = 128;
    // The backing memory's accessible region is grown in chunks of this size.
    static constexpr size_t kChunkSize = 1 * i::MB;

    std::unique_ptr<base::RegionAllocator> region_alloc_;
    size_t end_of_accessible_region_;
    base::Mutex mutex_;
  };

  BackendAllocator* allocator_ = BackendAllocator::SharedInstance();
};

#else

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override { return base::Calloc(length, 1); }

  void* AllocateUninitialized(size_t length) override {
    return base::Malloc(length);
  }

  void Free(void* data, size_t) override { base::Free(data); }

  void* Reallocate(void* data, size_t old_length, size_t new_length) override {
    void* new_data = base::Realloc(data, new_length);
    if (new_length > old_length) {
      memset(reinterpret_cast<uint8_t*>(new_data) + old_length, 0,
             new_length - old_length);
    }
    return new_data;
  }
};
#endif  // V8_ENABLE_SANDBOX

struct SnapshotCreatorData {
  explicit SnapshotCreatorData(Isolate* v8_isolate) : isolate_(v8_isolate) {}

  static SnapshotCreatorData* cast(void* data) {
    return reinterpret_cast<SnapshotCreatorData*>(data);
  }

  ArrayBufferAllocator allocator_;
  Isolate* isolate_;
  Persistent<Context> default_context_;
  SerializeInternalFieldsCallback default_embedder_fields_serializer_;
  std::vector<Global<Context>> contexts_;
  std::vector<SerializeInternalFieldsCallback> embedder_fields_serializers_;
  bool created_ = false;
};

}  // namespace

SnapshotCreator::SnapshotCreator(Isolate* v8_isolate,
                                 const intptr_t* external_references,
                                 const StartupData* existing_snapshot) {
  SnapshotCreatorData* data = new SnapshotCreatorData(v8_isolate);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i_isolate->set_array_buffer_allocator(&data->allocator_);
  i_isolate->set_api_external_references(external_references);
  i_isolate->enable_serializer();
  v8_isolate->Enter();
  const StartupData* blob = existing_snapshot
                                ? existing_snapshot
                                : i::Snapshot::DefaultSnapshotBlob();
  if (blob && blob->raw_size > 0) {
    i_isolate->set_snapshot_blob(blob);
    i::Snapshot::Initialize(i_isolate);
  } else {
    i_isolate->InitWithoutSnapshot();
  }
  data_ = data;
  // Disable batch compilation during snapshot creation.
  i_isolate->baseline_batch_compiler()->set_enabled(false);
}

SnapshotCreator::SnapshotCreator(const intptr_t* external_references,
                                 const StartupData* existing_snapshot)
    : SnapshotCreator(Isolate::Allocate(), external_references,
                      existing_snapshot) {}

SnapshotCreator::~SnapshotCreator() {
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  Isolate* v8_isolate = data->isolate_;
  v8_isolate->Exit();
  v8_isolate->Dispose();
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
  Isolate* v8_isolate = data->isolate_;
  CHECK_EQ(v8_isolate, context->GetIsolate());
  data->default_context_.Reset(v8_isolate, context);
  data->default_embedder_fields_serializer_ = callback;
}

size_t SnapshotCreator::AddContext(Local<Context> context,
                                   SerializeInternalFieldsCallback callback) {
  DCHECK(!context.IsEmpty());
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  DCHECK(!data->created_);
  Isolate* v8_isolate = data->isolate_;
  CHECK_EQ(v8_isolate, context->GetIsolate());
  size_t index = data->contexts_.size();
  data->contexts_.emplace_back(v8_isolate, context);
  data->embedder_fields_serializers_.push_back(callback);
  return index;
}

size_t SnapshotCreator::AddData(i::Address object) {
  DCHECK_NE(object, i::kNullAddress);
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  DCHECK(!data->created_);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(data->isolate_);
  i::HandleScope scope(i_isolate);
  i::Handle<i::Object> obj(i::Object(object), i_isolate);
  i::Handle<i::ArrayList> list;
  if (!i_isolate->heap()->serialized_objects().IsArrayList()) {
    list = i::ArrayList::New(i_isolate, 1);
  } else {
    list = i::Handle<i::ArrayList>(
        i::ArrayList::cast(i_isolate->heap()->serialized_objects()), i_isolate);
  }
  size_t index = static_cast<size_t>(list->Length());
  list = i::ArrayList::Add(i_isolate, list, obj);
  i_isolate->heap()->SetSerializedObjects(*list);
  return index;
}

size_t SnapshotCreator::AddData(Local<Context> context, i::Address object) {
  DCHECK_NE(object, i::kNullAddress);
  DCHECK(!SnapshotCreatorData::cast(data_)->created_);
  i::Handle<i::Context> ctx = Utils::OpenHandle(*context);
  i::Isolate* i_isolate = ctx->GetIsolate();
  i::HandleScope scope(i_isolate);
  i::Handle<i::Object> obj(i::Object(object), i_isolate);
  i::Handle<i::ArrayList> list;
  if (!ctx->serialized_objects().IsArrayList()) {
    list = i::ArrayList::New(i_isolate, 1);
  } else {
    list = i::Handle<i::ArrayList>(
        i::ArrayList::cast(ctx->serialized_objects()), i_isolate);
  }
  size_t index = static_cast<size_t>(list->Length());
  list = i::ArrayList::Add(i_isolate, list, obj);
  ctx->set_serialized_objects(*list);
  return index;
}

namespace {
void ConvertSerializedObjectsToFixedArray(Local<Context> context) {
  i::Handle<i::Context> ctx = Utils::OpenHandle(*context);
  i::Isolate* i_isolate = ctx->GetIsolate();
  if (!ctx->serialized_objects().IsArrayList()) {
    ctx->set_serialized_objects(
        i::ReadOnlyRoots(i_isolate).empty_fixed_array());
  } else {
    i::Handle<i::ArrayList> list(i::ArrayList::cast(ctx->serialized_objects()),
                                 i_isolate);
    i::Handle<i::FixedArray> elements = i::ArrayList::Elements(i_isolate, list);
    ctx->set_serialized_objects(*elements);
  }
}

void ConvertSerializedObjectsToFixedArray(i::Isolate* i_isolate) {
  if (!i_isolate->heap()->serialized_objects().IsArrayList()) {
    i_isolate->heap()->SetSerializedObjects(
        i::ReadOnlyRoots(i_isolate).empty_fixed_array());
  } else {
    i::Handle<i::ArrayList> list(
        i::ArrayList::cast(i_isolate->heap()->serialized_objects()), i_isolate);
    i::Handle<i::FixedArray> elements = i::ArrayList::Elements(i_isolate, list);
    i_isolate->heap()->SetSerializedObjects(*elements);
  }
}
}  // anonymous namespace

StartupData SnapshotCreator::CreateBlob(
    SnapshotCreator::FunctionCodeHandling function_code_handling) {
  SnapshotCreatorData* data = SnapshotCreatorData::cast(data_);
  Isolate* isolate = data->isolate_;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  Utils::ApiCheck(!data->created_, "v8::SnapshotCreator::CreateBlob",
                  "CreateBlob() cannot be called more than once on the same "
                  "SnapshotCreator");
  Utils::ApiCheck(
      !data->default_context_.IsEmpty(), "v8::SnapshotCreator::CreateBlob",
      "CreateBlob() cannot be called before the default context is set");
  const int num_additional_contexts = static_cast<int>(data->contexts_.size());
  const int num_contexts = num_additional_contexts + 1;  // The default context.

  // Create and store lists of embedder-provided data needed during
  // serialization.
  {
    i::HandleScope scope(i_isolate);
    // Convert list of context-independent data to FixedArray.
    ConvertSerializedObjectsToFixedArray(i_isolate);

    // Convert lists of context-dependent data to FixedArray.
    ConvertSerializedObjectsToFixedArray(
        data->default_context_.Get(data->isolate_));
    for (int i = 0; i < num_additional_contexts; i++) {
      ConvertSerializedObjectsToFixedArray(data->contexts_[i].Get(isolate));
    }

    // We need to store the global proxy size upfront in case we need the
    // bootstrapper to create a global proxy before we deserialize the context.
    i::Handle<i::FixedArray> global_proxy_sizes =
        i_isolate->factory()->NewFixedArray(num_additional_contexts,
                                            i::AllocationType::kOld);
    for (int i = 0; i < num_additional_contexts; i++) {
      i::Handle<i::Context> context =
          v8::Utils::OpenHandle(*data->contexts_[i].Get(isolate));
      global_proxy_sizes->set(i,
                              i::Smi::FromInt(context->global_proxy().Size()));
    }
    i_isolate->heap()->SetSerializedGlobalProxySizes(*global_proxy_sizes);
  }

  // We might rehash strings and re-sort descriptors. Clear the lookup cache.
  i_isolate->descriptor_lookup_cache()->Clear();

  // If we don't do this then we end up with a stray root pointing at the
  // context even after we have disposed of the context.
  i_isolate->heap()->CollectAllAvailableGarbage(
      i::GarbageCollectionReason::kSnapshotCreator);
  {
    i::HandleScope scope(i_isolate);
    i_isolate->heap()->CompactWeakArrayLists();
  }

  i::Snapshot::ClearReconstructableDataForSerialization(
      i_isolate, function_code_handling == FunctionCodeHandling::kClear);

  i::SafepointKind safepoint_kind = i_isolate->has_shared_space()
                                        ? i::SafepointKind::kGlobal
                                        : i::SafepointKind::kIsolate;
  i::SafepointScope safepoint_scope(i_isolate, safepoint_kind);
  i::DisallowGarbageCollection no_gc_from_here_on;

  // Create a vector with all contexts and clear associated Persistent fields.
  // Note these contexts may be dead after calling Clear(), but will not be
  // collected until serialization completes and the DisallowGarbageCollection
  // scope above goes out of scope.
  std::vector<i::Context> contexts;
  contexts.reserve(num_contexts);
  {
    i::HandleScope scope(i_isolate);
    contexts.push_back(
        *v8::Utils::OpenHandle(*data->default_context_.Get(data->isolate_)));
    data->default_context_.Reset();
    for (int i = 0; i < num_additional_contexts; i++) {
      i::Handle<i::Context> context =
          v8::Utils::OpenHandle(*data->contexts_[i].Get(isolate));
      contexts.push_back(*context);
    }
    data->contexts_.clear();
  }

  // Check that values referenced by global/eternal handles are accounted for.
  i::SerializedHandleChecker handle_checker(i_isolate, &contexts);
  CHECK(handle_checker.CheckGlobalAndEternalHandles());

  // Create a vector with all embedder fields serializers.
  std::vector<SerializeInternalFieldsCallback> embedder_fields_serializers;
  embedder_fields_serializers.reserve(num_contexts);
  embedder_fields_serializers.push_back(
      data->default_embedder_fields_serializer_);
  for (int i = 0; i < num_additional_contexts; i++) {
    embedder_fields_serializers.push_back(
        data->embedder_fields_serializers_[i]);
  }

  data->created_ = true;
  return i::Snapshot::Create(i_isolate, &contexts, embedder_fields_serializers,
                             safepoint_scope, no_gc_from_here_on);
}

bool StartupData::CanBeRehashed() const {
  DCHECK(i::Snapshot::VerifyChecksum(this));
  return i::Snapshot::ExtractRehashability(this);
}

bool StartupData::IsValid() const { return i::Snapshot::VersionIsValid(this); }

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
  using HelpOptions = i::FlagList::HelpOptions;
  i::FlagList::SetFlagsFromCommandLine(argc, argv, remove_flags,
                                       HelpOptions(HelpOptions::kDontExit));
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
      std::max(young_generation, i::Heap::MinYoungGenerationSize()));
  set_max_old_generation_size_in_bytes(
      std::max(old_generation, i::Heap::MinOldGenerationSize()));
  if (initial_heap_size_in_bytes > 0) {
    i::Heap::GenerationSizesFromHeapSize(initial_heap_size_in_bytes,
                                         &young_generation, &old_generation);
    // We do not set lower bounds for the initial sizes.
    set_initial_young_generation_size_in_bytes(young_generation);
    set_initial_old_generation_size_in_bytes(old_generation);
  }
  if (i::kPlatformRequiresCodeRange) {
    set_code_range_size_in_bytes(
        std::min(i::kMaximalCodeRangeSize, maximum_heap_size_in_bytes));
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

  if (virtual_memory_limit > 0 && i::kPlatformRequiresCodeRange) {
    set_code_range_size_in_bytes(
        std::min(i::kMaximalCodeRangeSize,
                 static_cast<size_t>(virtual_memory_limit / 8)));
  }
}

namespace internal {

i::Address* GlobalizeTracedReference(i::Isolate* i_isolate, i::Address* obj,
                                     internal::Address* slot,
                                     GlobalHandleStoreMode store_mode) {
  API_RCS_SCOPE(i_isolate, TracedGlobal, New);
#ifdef DEBUG
  Utils::ApiCheck((slot != nullptr), "v8::GlobalizeTracedReference",
                  "the address slot must be not null");
#endif
  auto obj_addr = internal::ValueHelper::ValueAsAddress(obj);
  auto result = i_isolate->traced_handles()->Create(obj_addr, slot, store_mode);
#ifdef VERIFY_HEAP
  if (i::v8_flags.verify_heap) {
    i::Object(obj_addr).ObjectVerify(i_isolate);
  }
#endif  // VERIFY_HEAP
  return result.location();
}

void MoveTracedReference(internal::Address** from, internal::Address** to) {
  TracedHandles::Move(from, to);
}

void CopyTracedReference(const internal::Address* const* from,
                         internal::Address** to) {
  TracedHandles::Copy(from, to);
}

void DisposeTracedReference(internal::Address* location) {
  TracedHandles::Destroy(location);
}

#if V8_STATIC_ROOTS_BOOL

// Initialize static root constants exposed in v8-internal.h.

namespace {
constexpr InstanceTypeChecker::RootIndexRange kStringMapRange =
    *InstanceTypeChecker::UniqueMapRangeOfInstanceTypeRange(FIRST_STRING_TYPE,
                                                            LAST_STRING_TYPE);
constexpr Tagged_t kFirstStringMapPtr =
    StaticReadOnlyRootsPointerTable[static_cast<size_t>(kStringMapRange.first)];
constexpr Tagged_t kLastStringMapPtr =
    StaticReadOnlyRootsPointerTable[static_cast<size_t>(
        kStringMapRange.second)];
}  // namespace

#define EXPORTED_STATIC_ROOTS_MAPPING(V)                    \
  V(UndefinedValue, i::StaticReadOnlyRoot::kUndefinedValue) \
  V(NullValue, i::StaticReadOnlyRoot::kNullValue)           \
  V(TrueValue, i::StaticReadOnlyRoot::kTrueValue)           \
  V(FalseValue, i::StaticReadOnlyRoot::kFalseValue)         \
  V(EmptyString, i::StaticReadOnlyRoot::kempty_string)      \
  V(TheHoleValue, i::StaticReadOnlyRoot::kTheHoleValue)     \
  V(FirstStringMap, kFirstStringMapPtr)                     \
  V(LastStringMap, kLastStringMapPtr)

static_assert(std::is_same<Internals::Tagged_t, Tagged_t>::value);
#define DEF_STATIC_ROOT(name, internal_value)                        \
  const Internals::Tagged_t Internals::StaticReadOnlyRoot::k##name = \
      internal_value;
EXPORTED_STATIC_ROOTS_MAPPING(DEF_STATIC_ROOT)
#undef DEF_STATIC_ROOT
#undef EXPORTED_STATIC_ROOTS_MAPPING

#endif  // V8_STATIC_ROOTS_BOOL

}  // namespace internal

namespace api_internal {

i::Address* GlobalizeReference(i::Isolate* i_isolate, i::Address* obj) {
  API_RCS_SCOPE(i_isolate, Persistent, New);
  i::Handle<i::Object> result = i_isolate->global_handles()->Create(*obj);
#ifdef VERIFY_HEAP
  if (i::v8_flags.verify_heap) {
    i::Object(*obj).ObjectVerify(i_isolate);
  }
#endif  // VERIFY_HEAP
  return result.location();
}

i::Address* CopyGlobalReference(i::Address* from) {
  i::Handle<i::Object> result = i::GlobalHandles::CopyGlobal(from);
  return result.location();
}

void MoveGlobalReference(internal::Address** from, internal::Address** to) {
  i::GlobalHandles::MoveGlobal(from, to);
}

void MakeWeak(i::Address* location, void* parameter,
              WeakCallbackInfo<void>::Callback weak_callback,
              WeakCallbackType type) {
  i::GlobalHandles::MakeWeak(location, parameter, weak_callback, type);
}

void MakeWeak(i::Address** location_addr) {
  i::GlobalHandles::MakeWeak(location_addr);
}

void* ClearWeak(i::Address* location) {
  return i::GlobalHandles::ClearWeakness(location);
}

void AnnotateStrongRetainer(i::Address* location, const char* label) {
  i::GlobalHandles::AnnotateStrongRetainer(location, label);
}

void DisposeGlobal(i::Address* location) {
  i::GlobalHandles::Destroy(location);
}

Value* Eternalize(Isolate* v8_isolate, Value* value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Object object = *Utils::OpenHandle(value);
  int index = -1;
  i_isolate->eternal_handles()->Create(i_isolate, object, &index);
  return reinterpret_cast<Value*>(
      i_isolate->eternal_handles()->Get(index).location());
}

void FromJustIsNothing() {
  Utils::ApiCheck(false, "v8::FromJust", "Maybe value is Nothing");
}

void ToLocalEmpty() {
  Utils::ApiCheck(false, "v8::ToLocalChecked", "Empty MaybeLocal");
}

void InternalFieldOutOfBounds(int index) {
  Utils::ApiCheck(0 <= index && index < kInternalFieldsInWeakCallback,
                  "WeakCallbackInfo::GetInternalField",
                  "Internal field out of bounds");
}

}  // namespace api_internal

// --- H a n d l e s ---

HandleScope::HandleScope(Isolate* v8_isolate) { Initialize(v8_isolate); }

void HandleScope::Initialize(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  // We do not want to check the correct usage of the Locker class all over the
  // place, so we do it only here: Without a HandleScope, an embedder can do
  // almost nothing, so it is enough to check in this central place.
  // We make an exception if the serializer is enabled, which means that the
  // Isolate is exclusively used to create a snapshot.
  Utils::ApiCheck(!i_isolate->was_locker_ever_used() ||
                      i_isolate->thread_manager()->IsLockedByCurrentThread() ||
                      i_isolate->serializer_enabled(),
                  "HandleScope::HandleScope",
                  "Entering the V8 API without proper locking in place");
  i::HandleScopeData* current = i_isolate->handle_scope_data();
  i_isolate_ = i_isolate;
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
}

HandleScope::~HandleScope() {
  i::HandleScope::CloseScope(i_isolate_, prev_next_, prev_limit_);
}

void* HandleScope::operator new(size_t) { base::OS::Abort(); }
void* HandleScope::operator new[](size_t) { base::OS::Abort(); }
void HandleScope::operator delete(void*, size_t) { base::OS::Abort(); }
void HandleScope::operator delete[](void*, size_t) { base::OS::Abort(); }

int HandleScope::NumberOfHandles(Isolate* v8_isolate) {
  return i::HandleScope::NumberOfHandles(
      reinterpret_cast<i::Isolate*>(v8_isolate));
}

i::Address* HandleScope::CreateHandle(i::Isolate* i_isolate, i::Address value) {
  return i::HandleScope::CreateHandle(i_isolate, value);
}

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING

i::Address* HandleScope::CreateHandleForCurrentIsolate(i::Address value) {
  i::Isolate* i_isolate = i::Isolate::Current();
  return i::HandleScope::CreateHandle(i_isolate, value);
}

#endif

EscapableHandleScope::EscapableHandleScope(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  escape_slot_ = CreateHandle(
      i_isolate, i::ReadOnlyRoots(i_isolate).the_hole_value().ptr());
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

SealHandleScope::SealHandleScope(Isolate* v8_isolate)
    : i_isolate_(reinterpret_cast<i::Isolate*>(v8_isolate)) {
  i::HandleScopeData* current = i_isolate_->handle_scope_data();
  prev_limit_ = current->limit;
  current->limit = current->next;
  prev_sealed_level_ = current->sealed_level;
  current->sealed_level = current->level;
}

SealHandleScope::~SealHandleScope() {
  i::HandleScopeData* current = i_isolate_->handle_scope_data();
  DCHECK_EQ(current->next, current->limit);
  current->limit = prev_limit_;
  DCHECK_EQ(current->level, current->sealed_level);
  current->sealed_level = prev_sealed_level_;
}

void* SealHandleScope::operator new(size_t) { base::OS::Abort(); }
void* SealHandleScope::operator new[](size_t) { base::OS::Abort(); }
void SealHandleScope::operator delete(void*, size_t) { base::OS::Abort(); }
void SealHandleScope::operator delete[](void*, size_t) { base::OS::Abort(); }

bool Data::IsModule() const { return Utils::OpenHandle(this)->IsModule(); }
bool Data::IsFixedArray() const {
  return Utils::OpenHandle(this)->IsFixedArray();
}

bool Data::IsValue() const {
  i::DisallowGarbageCollection no_gc;
  i::Object self = *Utils::OpenHandle(this);
  if (self.IsSmi()) return true;
  i::HeapObject heap_object = i::HeapObject::cast(self);
  DCHECK(!heap_object.IsTheHole());
  if (heap_object.IsSymbol()) {
    return !i::Symbol::cast(heap_object).is_private();
  }
  return heap_object.IsPrimitiveHeapObject() || heap_object.IsJSReceiver();
}

bool Data::IsPrivate() const {
  return Utils::OpenHandle(this)->IsPrivateSymbol();
}

bool Data::IsObjectTemplate() const {
  return Utils::OpenHandle(this)->IsObjectTemplateInfo();
}

bool Data::IsFunctionTemplate() const {
  return Utils::OpenHandle(this)->IsFunctionTemplateInfo();
}

bool Data::IsContext() const { return Utils::OpenHandle(this)->IsContext(); }

void Context::Enter() {
  i::DisallowGarbageCollection no_gc;
  i::Context env = *Utils::OpenHandle(this);
  i::Isolate* i_isolate = env.GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScopeImplementer* impl = i_isolate->handle_scope_implementer();
  impl->EnterContext(env);
  impl->SaveContext(i_isolate->context());
  i_isolate->set_context(env);
}

void Context::Exit() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* i_isolate = env->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScopeImplementer* impl = i_isolate->handle_scope_implementer();
  if (!Utils::ApiCheck(impl->LastEnteredContextWas(*env), "v8::Context::Exit()",
                       "Cannot exit non-entered context")) {
    return;
  }
  impl->LeaveContext();
  i_isolate->set_context(impl->RestoreContext());
}

Context::BackupIncumbentScope::BackupIncumbentScope(
    Local<Context> backup_incumbent_context)
    : backup_incumbent_context_(backup_incumbent_context) {
  DCHECK(!backup_incumbent_context_.IsEmpty());

  i::Handle<i::Context> env = Utils::OpenHandle(*backup_incumbent_context_);
  i::Isolate* i_isolate = env->GetIsolate();

  js_stack_comparable_address_ =
      i::SimulatorStack::RegisterJSStackComparableAddress(i_isolate);

  prev_ = i_isolate->top_backup_incumbent_scope();
  i_isolate->set_top_backup_incumbent_scope(this);
}

Context::BackupIncumbentScope::~BackupIncumbentScope() {
  i::Handle<i::Context> env = Utils::OpenHandle(*backup_incumbent_context_);
  i::Isolate* i_isolate = env->GetIsolate();

  i::SimulatorStack::UnregisterJSStackComparableAddress(i_isolate);

  i_isolate->set_top_backup_incumbent_scope(prev_);
}

static_assert(i::Internals::kEmbedderDataSlotSize == i::kEmbedderDataSlotSize);
static_assert(i::Internals::kEmbedderDataSlotExternalPointerOffset ==
              i::EmbedderDataSlot::kExternalPointerOffset);

static i::Handle<i::EmbedderDataArray> EmbedderDataFor(Context* context,
                                                       int index, bool can_grow,
                                                       const char* location) {
  i::Handle<i::Context> env = Utils::OpenHandle(context);
  i::Isolate* i_isolate = env->GetIsolate();
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  bool ok = Utils::ApiCheck(env->IsNativeContext(), location,
                            "Not a native context") &&
            Utils::ApiCheck(index >= 0, location, "Negative index");
  if (!ok) return i::Handle<i::EmbedderDataArray>();
  // TODO(ishell): remove cast once embedder_data slot has a proper type.
  i::Handle<i::EmbedderDataArray> data(
      i::EmbedderDataArray::cast(env->embedder_data()), i_isolate);
  if (index < data->length()) return data;
  if (!Utils::ApiCheck(can_grow && index < i::EmbedderDataArray::kMaxLength,
                       location, "Index too large")) {
    return i::Handle<i::EmbedderDataArray>();
  }
  data = i::EmbedderDataArray::EnsureCapacity(i_isolate, data, index);
  env->set_embedder_data(*data);
  return data;
}

uint32_t Context::GetNumberOfEmbedderDataFields() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(context->GetIsolate());
  Utils::ApiCheck(context->IsNativeContext(),
                  "Context::GetNumberOfEmbedderDataFields",
                  "Not a native context");
  // TODO(ishell): remove cast once embedder_data slot has a proper type.
  return static_cast<uint32_t>(
      i::EmbedderDataArray::cast(context->embedder_data()).length());
}

v8::Local<v8::Value> Context::SlowGetEmbedderData(int index) {
  const char* location = "v8::Context::GetEmbedderData()";
  i::Handle<i::EmbedderDataArray> data =
      EmbedderDataFor(this, index, false, location);
  if (data.is_null()) return Local<Value>();
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  i::Handle<i::Object> result(i::EmbedderDataSlot(*data, index).load_tagged(),
                              i_isolate);
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
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  i::HandleScope handle_scope(i_isolate);
  i::Handle<i::EmbedderDataArray> data =
      EmbedderDataFor(this, index, false, location);
  if (data.is_null()) return nullptr;
  void* result;
  Utils::ApiCheck(
      i::EmbedderDataSlot(*data, index).ToAlignedPointer(i_isolate, &result),
      location, "Pointer is not aligned");
  return result;
}

void Context::SetAlignedPointerInEmbedderData(int index, void* value) {
  const char* location = "v8::Context::SetAlignedPointerInEmbedderData()";
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  i::Handle<i::EmbedderDataArray> data =
      EmbedderDataFor(this, index, true, location);
  bool ok =
      i::EmbedderDataSlot(*data, index).store_aligned_pointer(i_isolate, value);
  Utils::ApiCheck(ok, location, "Pointer is not aligned");
  DCHECK_EQ(value, GetAlignedPointerFromEmbedderData(index));
}

// --- T e m p l a t e ---

static void InitializeTemplate(i::TemplateInfo that, int type,
                               bool do_not_cache) {
  that.set_number_of_properties(0);
  that.set_tag(type);
  int serial_number =
      do_not_cache ? i::TemplateInfo::kDoNotCache : i::TemplateInfo::kUncached;
  that.set_serial_number(serial_number);
}

void Template::Set(v8::Local<Name> name, v8::Local<Data> value,
                   v8::PropertyAttribute attribute) {
  auto templ = Utils::OpenHandle(this);
  i::Isolate* i_isolate = templ->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  auto value_obj = Utils::OpenHandle(*value);

  Utils::ApiCheck(!value_obj->IsJSReceiver() || value_obj->IsTemplateInfo(),
                  "v8::Template::Set",
                  "Invalid value, must be a primitive or a Template");

  // The template cache only performs shallow clones, if we set an
  // ObjectTemplate as a property value then we can not cache the receiver
  // template.
  if (value_obj->IsObjectTemplateInfo()) {
    templ->set_serial_number(i::TemplateInfo::kDoNotCache);
  }

  i::ApiNatives::AddDataProperty(i_isolate, templ, Utils::OpenHandle(*name),
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
  Utils::ApiCheck(
      getter.IsEmpty() ||
          !Utils::OpenHandle(*getter)->call_code(kAcquireLoad).IsUndefined(),
      "v8::Template::SetAccessorProperty", "Getter must have a call handler");
  Utils::ApiCheck(
      setter.IsEmpty() ||
          !Utils::OpenHandle(*setter)->call_code(kAcquireLoad).IsUndefined(),
      "v8::Template::SetAccessorProperty", "Setter must have a call handler");

  // TODO(verwaest): Remove |access_control|.
  DCHECK_EQ(v8::DEFAULT, access_control);
  auto templ = Utils::OpenHandle(this);
  auto i_isolate = templ->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  DCHECK(!name.IsEmpty());
  DCHECK(!getter.IsEmpty() || !setter.IsEmpty());
  i::HandleScope scope(i_isolate);
  i::ApiNatives::AddAccessorProperty(
      i_isolate, templ, Utils::OpenHandle(*name),
      Utils::OpenHandle(*getter, true), Utils::OpenHandle(*setter, true),
      static_cast<i::PropertyAttributes>(attribute));
}

// --- F u n c t i o n   T e m p l a t e ---
static void InitializeFunctionTemplate(i::FunctionTemplateInfo info,
                                       bool do_not_cache) {
  InitializeTemplate(info, Consts::FUNCTION_TEMPLATE, do_not_cache);
  info.set_flag(0);
}

namespace {
Local<ObjectTemplate> ObjectTemplateNew(i::Isolate* i_isolate,
                                        v8::Local<FunctionTemplate> constructor,
                                        bool do_not_cache) {
  API_RCS_SCOPE(i_isolate, ObjectTemplate, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Struct> struct_obj = i_isolate->factory()->NewStruct(
      i::OBJECT_TEMPLATE_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::ObjectTemplateInfo> obj =
      i::Handle<i::ObjectTemplateInfo>::cast(struct_obj);
  {
    // Disallow GC until all fields of obj have acceptable types.
    i::DisallowGarbageCollection no_gc;
    i::ObjectTemplateInfo raw = *obj;
    InitializeTemplate(raw, Consts::OBJECT_TEMPLATE, do_not_cache);
    raw.set_data(0);
    if (!constructor.IsEmpty()) {
      raw.set_constructor(*Utils::OpenHandle(*constructor));
    }
  }
  return Utils::ToLocal(obj);
}
}  // namespace

Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::HeapObject> result(self->GetPrototypeTemplate(), i_isolate);
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
  i::Handle<i::FunctionTemplateInfo> result =
      Utils::OpenHandle(*prototype_provider);
  Utils::ApiCheck(self->GetPrototypeTemplate().IsUndefined(i_isolate),
                  "v8::FunctionTemplate::SetPrototypeProviderTemplate",
                  "Protoype must be undefined");
  Utils::ApiCheck(self->GetParentTemplate().IsUndefined(i_isolate),
                  "v8::FunctionTemplate::SetPrototypeProviderTemplate",
                  "Prototype provider must be empty");
  i::FunctionTemplateInfo::SetPrototypeProviderTemplate(i_isolate, self,
                                                        result);
}

namespace {
static void EnsureNotPublished(i::Handle<i::FunctionTemplateInfo> info,
                               const char* func) {
  DCHECK_IMPLIES(info->instantiated(), info->published());
  Utils::ApiCheck(!info->published(), func,
                  "FunctionTemplate already instantiated");
}

Local<FunctionTemplate> FunctionTemplateNew(
    i::Isolate* i_isolate, FunctionCallback callback, v8::Local<Value> data,
    v8::Local<Signature> signature, int length, ConstructorBehavior behavior,
    bool do_not_cache,
    v8::Local<Private> cached_property_name = v8::Local<Private>(),
    SideEffectType side_effect_type = SideEffectType::kHasSideEffect,
    const MemorySpan<const CFunction>& c_function_overloads = {},
    uint8_t instance_type = 0,
    uint8_t allowed_receiver_instance_type_range_start = 0,
    uint8_t allowed_receiver_instance_type_range_end = 0) {
  i::Handle<i::Struct> struct_obj = i_isolate->factory()->NewStruct(
      i::FUNCTION_TEMPLATE_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::FunctionTemplateInfo> obj =
      i::Handle<i::FunctionTemplateInfo>::cast(struct_obj);
  {
    // Disallow GC until all fields of obj have acceptable types.
    i::DisallowGarbageCollection no_gc;
    i::FunctionTemplateInfo raw = *obj;
    InitializeFunctionTemplate(raw, do_not_cache);
    raw.set_length(length);
    raw.set_undetectable(false);
    raw.set_needs_access_check(false);
    raw.set_accept_any_receiver(true);
    if (!signature.IsEmpty()) {
      raw.set_signature(*Utils::OpenHandle(*signature));
    }
    raw.set_cached_property_name(
        cached_property_name.IsEmpty()
            ? i::ReadOnlyRoots(i_isolate).the_hole_value()
            : *Utils::OpenHandle(*cached_property_name));
    if (behavior == ConstructorBehavior::kThrow) raw.set_remove_prototype(true);
    raw.SetInstanceType(instance_type);
    raw.set_allowed_receiver_instance_type_range_start(
        allowed_receiver_instance_type_range_start);
    raw.set_allowed_receiver_instance_type_range_end(
        allowed_receiver_instance_type_range_end);
  }
  if (callback != nullptr) {
    Utils::ToLocal(obj)->SetCallHandler(callback, data, side_effect_type,
                                        c_function_overloads);
  }
  return Utils::ToLocal(obj);
}
}  // namespace

void FunctionTemplate::Inherit(v8::Local<FunctionTemplate> value) {
  auto info = Utils::OpenHandle(this);
  EnsureNotPublished(info, "v8::FunctionTemplate::Inherit");
  i::Isolate* i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  Utils::ApiCheck(info->GetPrototypeProviderTemplate().IsUndefined(i_isolate),
                  "v8::FunctionTemplate::Inherit",
                  "Protoype provider must be empty");
  i::FunctionTemplateInfo::SetParentTemplate(i_isolate, info,
                                             Utils::OpenHandle(*value));
}

Local<FunctionTemplate> FunctionTemplate::New(
    Isolate* v8_isolate, FunctionCallback callback, v8::Local<Value> data,
    v8::Local<Signature> signature, int length, ConstructorBehavior behavior,
    SideEffectType side_effect_type, const CFunction* c_function,
    uint16_t instance_type, uint16_t allowed_receiver_instance_type_range_start,
    uint16_t allowed_receiver_instance_type_range_end) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  // Changes to the environment cannot be captured in the snapshot. Expect no
  // function templates when the isolate is created for serialization.
  API_RCS_SCOPE(i_isolate, FunctionTemplate, New);

  if (!Utils::ApiCheck(
          !c_function || behavior == ConstructorBehavior::kThrow,
          "FunctionTemplate::New",
          "Fast API calls are not supported for constructor functions")) {
    return Local<FunctionTemplate>();
  }

  if (instance_type != 0) {
    if (!Utils::ApiCheck(
            instance_type >= i::Internals::kFirstJSApiObjectType &&
                instance_type <= i::Internals::kLastJSApiObjectType,
            "FunctionTemplate::New",
            "instance_type is outside the range of valid JSApiObject types")) {
      return Local<FunctionTemplate>();
    }
  }

  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return FunctionTemplateNew(
      i_isolate, callback, data, signature, length, behavior, false,
      Local<Private>(), side_effect_type,
      c_function ? MemorySpan<const CFunction>{c_function, 1}
                 : MemorySpan<const CFunction>{},
      instance_type, allowed_receiver_instance_type_range_start,
      allowed_receiver_instance_type_range_end);
}

Local<FunctionTemplate> FunctionTemplate::NewWithCFunctionOverloads(
    Isolate* v8_isolate, FunctionCallback callback, v8::Local<Value> data,
    v8::Local<Signature> signature, int length, ConstructorBehavior behavior,
    SideEffectType side_effect_type,
    const MemorySpan<const CFunction>& c_function_overloads) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, FunctionTemplate, New);

  if (!Utils::ApiCheck(
          c_function_overloads.size() == 0 ||
              behavior == ConstructorBehavior::kThrow,
          "FunctionTemplate::NewWithCFunctionOverloads",
          "Fast API calls are not supported for constructor functions")) {
    return Local<FunctionTemplate>();
  }

  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return FunctionTemplateNew(i_isolate, callback, data, signature, length,
                             behavior, false, Local<Private>(),
                             side_effect_type, c_function_overloads);
}

Local<FunctionTemplate> FunctionTemplate::NewWithCache(
    Isolate* v8_isolate, FunctionCallback callback,
    Local<Private> cache_property, Local<Value> data,
    Local<Signature> signature, int length, SideEffectType side_effect_type) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, FunctionTemplate, NewWithCache);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return FunctionTemplateNew(i_isolate, callback, data, signature, length,
                             ConstructorBehavior::kAllow, false, cache_property,
                             side_effect_type);
}

Local<Signature> Signature::New(Isolate* v8_isolate,
                                Local<FunctionTemplate> receiver) {
  return Utils::SignatureToLocal(Utils::OpenHandle(*receiver));
}

#define SET_FIELD_WRAPPED(i_isolate, obj, setter, cdata)        \
  do {                                                          \
    i::Handle<i::Object> foreign = FromCData(i_isolate, cdata); \
    (obj)->setter(*foreign);                                    \
  } while (false)

void FunctionTemplate::SetCallHandler(
    FunctionCallback callback, v8::Local<Value> data,
    SideEffectType side_effect_type,
    const MemorySpan<const CFunction>& c_function_overloads) {
  auto info = Utils::OpenHandle(this);
  EnsureNotPublished(info, "v8::FunctionTemplate::SetCallHandler");
  i::Isolate* i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  i::Handle<i::CallHandlerInfo> obj = i_isolate->factory()->NewCallHandlerInfo(
      side_effect_type == SideEffectType::kHasNoSideEffect);
  obj->set_callback(i_isolate, reinterpret_cast<i::Address>(callback));
  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  if (c_function_overloads.size() > 0) {
    // Stores the data for a sequence of CFunction overloads into a single
    // FixedArray, as [address_0, signature_0, ... address_n-1, signature_n-1].
    i::Handle<i::FixedArray> function_overloads =
        i_isolate->factory()->NewFixedArray(static_cast<int>(
            c_function_overloads.size() *
            i::FunctionTemplateInfo::kFunctionOverloadEntrySize));
    int function_count = static_cast<int>(c_function_overloads.size());
    for (int i = 0; i < function_count; i++) {
      const CFunction& c_function = c_function_overloads.data()[i];
      i::Handle<i::Object> address =
          FromCData(i_isolate, c_function.GetAddress());
      function_overloads->set(
          i::FunctionTemplateInfo::kFunctionOverloadEntrySize * i, *address);
      i::Handle<i::Object> signature =
          FromCData(i_isolate, c_function.GetTypeInfo());
      function_overloads->set(
          i::FunctionTemplateInfo::kFunctionOverloadEntrySize * i + 1,
          *signature);
    }
    i::FunctionTemplateInfo::SetCFunctionOverloads(i_isolate, info,
                                                   function_overloads);
  }
  info->set_call_code(*obj, kReleaseStore);
}

namespace {

template <typename Getter, typename Setter>
i::Handle<i::AccessorInfo> MakeAccessorInfo(
    i::Isolate* i_isolate, v8::Local<Name> name, Getter getter, Setter setter,
    v8::Local<Value> data, v8::AccessControl settings,
    bool is_special_data_property, bool replace_on_access) {
  i::Handle<i::AccessorInfo> obj = i_isolate->factory()->NewAccessorInfo();
  obj->set_getter(i_isolate, reinterpret_cast<i::Address>(getter));
  DCHECK_IMPLIES(replace_on_access,
                 is_special_data_property && setter == nullptr);
  if (is_special_data_property && setter == nullptr) {
    setter = reinterpret_cast<Setter>(&i::Accessors::ReconfigureToDataProperty);
  }
  obj->set_setter(i_isolate, reinterpret_cast<i::Address>(setter));

  i::Handle<i::Name> accessor_name = Utils::OpenHandle(*name);
  if (!accessor_name->IsUniqueName()) {
    accessor_name = i_isolate->factory()->InternalizeString(
        i::Handle<i::String>::cast(accessor_name));
  }
  i::DisallowGarbageCollection no_gc;
  i::AccessorInfo raw_obj = *obj;
  if (data.IsEmpty()) {
    raw_obj.set_data(i::ReadOnlyRoots(i_isolate).undefined_value());
  } else {
    raw_obj.set_data(*Utils::OpenHandle(*data));
  }
  raw_obj.set_name(*accessor_name);
  raw_obj.set_is_special_data_property(is_special_data_property);
  raw_obj.set_replace_on_access(replace_on_access);
  if (settings & ALL_CAN_READ) raw_obj.set_all_can_read(true);
  if (settings & ALL_CAN_WRITE) raw_obj.set_all_can_write(true);
  raw_obj.set_initial_property_attributes(i::NONE);
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
  i::Isolate* i_isolate = handle->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  if (handle->GetInstanceTemplate().IsUndefined(i_isolate)) {
    Local<ObjectTemplate> templ =
        ObjectTemplate::New(i_isolate, ToApiHandle<FunctionTemplate>(handle));
    i::FunctionTemplateInfo::SetInstanceTemplate(i_isolate, handle,
                                                 Utils::OpenHandle(*templ));
  }
  i::Handle<i::ObjectTemplateInfo> result(
      i::ObjectTemplateInfo::cast(handle->GetInstanceTemplate()), i_isolate);
  return Utils::ToLocal(result);
}

void FunctionTemplate::SetLength(int length) {
  auto info = Utils::OpenHandle(this);
  EnsureNotPublished(info, "v8::FunctionTemplate::SetLength");
  auto i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  info->set_length(length);
}

void FunctionTemplate::SetClassName(Local<String> name) {
  auto info = Utils::OpenHandle(this);
  EnsureNotPublished(info, "v8::FunctionTemplate::SetClassName");
  auto i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  info->set_class_name(*Utils::OpenHandle(*name));
}

void FunctionTemplate::SetAcceptAnyReceiver(bool value) {
  auto info = Utils::OpenHandle(this);
  EnsureNotPublished(info, "v8::FunctionTemplate::SetAcceptAnyReceiver");
  auto i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  info->set_accept_any_receiver(value);
}

void FunctionTemplate::ReadOnlyPrototype() {
  auto info = Utils::OpenHandle(this);
  EnsureNotPublished(info, "v8::FunctionTemplate::ReadOnlyPrototype");
  auto i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  info->set_read_only_prototype(true);
}

void FunctionTemplate::RemovePrototype() {
  auto info = Utils::OpenHandle(this);
  EnsureNotPublished(info, "v8::FunctionTemplate::RemovePrototype");
  auto i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  info->set_remove_prototype(true);
}

// --- O b j e c t T e m p l a t e ---

Local<ObjectTemplate> ObjectTemplate::New(
    Isolate* v8_isolate, v8::Local<FunctionTemplate> constructor) {
  return New(reinterpret_cast<i::Isolate*>(v8_isolate), constructor);
}

Local<ObjectTemplate> ObjectTemplate::New(
    i::Isolate* i_isolate, v8::Local<FunctionTemplate> constructor) {
  return ObjectTemplateNew(i_isolate, constructor, false);
}

namespace {
// Ensure that the object template has a constructor.  If no
// constructor is available we create one.
i::Handle<i::FunctionTemplateInfo> EnsureConstructor(
    i::Isolate* i_isolate, ObjectTemplate* object_template) {
  i::Object obj = Utils::OpenHandle(object_template)->constructor();
  if (!obj.IsUndefined(i_isolate)) {
    i::FunctionTemplateInfo info = i::FunctionTemplateInfo::cast(obj);
    return i::Handle<i::FunctionTemplateInfo>(info, i_isolate);
  }
  Local<FunctionTemplate> templ =
      FunctionTemplate::New(reinterpret_cast<Isolate*>(i_isolate));
  i::Handle<i::FunctionTemplateInfo> constructor = Utils::OpenHandle(*templ);
  i::FunctionTemplateInfo::SetInstanceTemplate(
      i_isolate, constructor, Utils::OpenHandle(object_template));
  Utils::OpenHandle(object_template)->set_constructor(*constructor);
  return constructor;
}

template <typename Getter, typename Setter, typename Data, typename Template>
void TemplateSetAccessor(Template* template_obj, v8::Local<Name> name,
                         Getter getter, Setter setter, Data data,
                         AccessControl settings, PropertyAttribute attribute,
                         bool is_special_data_property, bool replace_on_access,
                         SideEffectType getter_side_effect_type,
                         SideEffectType setter_side_effect_type) {
  auto info = Utils::OpenHandle(template_obj);
  auto i_isolate = info->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  i::Handle<i::AccessorInfo> accessor_info =
      MakeAccessorInfo(i_isolate, name, getter, setter, data, settings,
                       is_special_data_property, replace_on_access);
  {
    i::DisallowGarbageCollection no_gc;
    i::AccessorInfo raw = *accessor_info;
    raw.set_initial_property_attributes(
        static_cast<i::PropertyAttributes>(attribute));
    raw.set_getter_side_effect_type(getter_side_effect_type);
    raw.set_setter_side_effect_type(setter_side_effect_type);
  }
  i::ApiNatives::AddNativeDataProperty(i_isolate, info, accessor_info);
}
}  // namespace

void Template::SetNativeDataProperty(v8::Local<String> name,
                                     AccessorGetterCallback getter,
                                     AccessorSetterCallback setter,
                                     v8::Local<Value> data,
                                     PropertyAttribute attribute,
                                     AccessControl settings,
                                     SideEffectType getter_side_effect_type,
                                     SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      true, false, getter_side_effect_type,
                      setter_side_effect_type);
}

void Template::SetNativeDataProperty(v8::Local<Name> name,
                                     AccessorNameGetterCallback getter,
                                     AccessorNameSetterCallback setter,
                                     v8::Local<Value> data,
                                     PropertyAttribute attribute,
                                     AccessControl settings,
                                     SideEffectType getter_side_effect_type,
                                     SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      true, false, getter_side_effect_type,
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
                      DEFAULT, attribute, true, true, getter_side_effect_type,
                      setter_side_effect_type);
}

void Template::SetIntrinsicDataProperty(Local<Name> name, Intrinsic intrinsic,
                                        PropertyAttribute attribute) {
  auto templ = Utils::OpenHandle(this);
  i::Isolate* i_isolate = templ->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  i::ApiNatives::AddDataProperty(i_isolate, templ, Utils::OpenHandle(*name),
                                 intrinsic,
                                 static_cast<i::PropertyAttributes>(attribute));
}

void ObjectTemplate::SetAccessor(v8::Local<String> name,
                                 AccessorGetterCallback getter,
                                 AccessorSetterCallback setter,
                                 v8::Local<Value> data, AccessControl settings,
                                 PropertyAttribute attribute,
                                 SideEffectType getter_side_effect_type,
                                 SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      i::v8_flags.disable_old_api_accessors, false,
                      getter_side_effect_type, setter_side_effect_type);
}

void ObjectTemplate::SetAccessor(v8::Local<Name> name,
                                 AccessorNameGetterCallback getter,
                                 AccessorNameSetterCallback setter,
                                 v8::Local<Value> data, AccessControl settings,
                                 PropertyAttribute attribute,
                                 SideEffectType getter_side_effect_type,
                                 SideEffectType setter_side_effect_type) {
  TemplateSetAccessor(this, name, getter, setter, data, settings, attribute,
                      i::v8_flags.disable_old_api_accessors, false,
                      getter_side_effect_type, setter_side_effect_type);
}

namespace {
template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
i::Handle<i::InterceptorInfo> CreateInterceptorInfo(
    i::Isolate* i_isolate, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  auto obj =
      i::Handle<i::InterceptorInfo>::cast(i_isolate->factory()->NewStruct(
          i::INTERCEPTOR_INFO_TYPE, i::AllocationType::kOld));
  obj->set_flags(0);

  if (getter != nullptr) SET_FIELD_WRAPPED(i_isolate, obj, set_getter, getter);
  if (setter != nullptr) SET_FIELD_WRAPPED(i_isolate, obj, set_setter, setter);
  if (query != nullptr) SET_FIELD_WRAPPED(i_isolate, obj, set_query, query);
  if (descriptor != nullptr)
    SET_FIELD_WRAPPED(i_isolate, obj, set_descriptor, descriptor);
  if (remover != nullptr)
    SET_FIELD_WRAPPED(i_isolate, obj, set_deleter, remover);
  if (enumerator != nullptr)
    SET_FIELD_WRAPPED(i_isolate, obj, set_enumerator, enumerator);
  if (definer != nullptr)
    SET_FIELD_WRAPPED(i_isolate, obj, set_definer, definer);
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
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  return obj;
}

template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
i::Handle<i::InterceptorInfo> CreateNamedInterceptorInfo(
    i::Isolate* i_isolate, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  auto interceptor =
      CreateInterceptorInfo(i_isolate, getter, setter, query, descriptor,
                            remover, enumerator, definer, data, flags);
  interceptor->set_is_named(true);
  return interceptor;
}

template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
i::Handle<i::InterceptorInfo> CreateIndexedInterceptorInfo(
    i::Isolate* i_isolate, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  auto interceptor =
      CreateInterceptorInfo(i_isolate, getter, setter, query, descriptor,
                            remover, enumerator, definer, data, flags);
  interceptor->set_is_named(false);
  return interceptor;
}

template <typename Getter, typename Setter, typename Query, typename Descriptor,
          typename Deleter, typename Enumerator, typename Definer>
void ObjectTemplateSetNamedPropertyHandler(
    ObjectTemplate* templ, Getter getter, Setter setter, Query query,
    Descriptor descriptor, Deleter remover, Enumerator enumerator,
    Definer definer, Local<Value> data, PropertyHandlerFlags flags) {
  i::Isolate* i_isolate = Utils::OpenHandle(templ)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  auto cons = EnsureConstructor(i_isolate, templ);
  EnsureNotPublished(cons, "ObjectTemplateSetNamedPropertyHandler");
  auto obj =
      CreateNamedInterceptorInfo(i_isolate, getter, setter, query, descriptor,
                                 remover, enumerator, definer, data, flags);
  i::FunctionTemplateInfo::SetNamedPropertyHandler(i_isolate, cons, obj);
}
}  // namespace

void ObjectTemplate::SetHandler(
    const NamedPropertyHandlerConfiguration& config) {
  ObjectTemplateSetNamedPropertyHandler(
      this, config.getter, config.setter, config.query, config.descriptor,
      config.deleter, config.enumerator, config.definer, config.data,
      config.flags);
}

void ObjectTemplate::MarkAsUndetectable() {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  auto cons = EnsureConstructor(i_isolate, this);
  EnsureNotPublished(cons, "v8::ObjectTemplate::MarkAsUndetectable");
  cons->set_undetectable(true);
}

void ObjectTemplate::SetAccessCheckCallback(AccessCheckCallback callback,
                                            Local<Value> data) {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  auto cons = EnsureConstructor(i_isolate, this);
  EnsureNotPublished(cons, "v8::ObjectTemplate::SetAccessCheckCallback");

  i::Handle<i::Struct> struct_info = i_isolate->factory()->NewStruct(
      i::ACCESS_CHECK_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::AccessCheckInfo> info =
      i::Handle<i::AccessCheckInfo>::cast(struct_info);

  SET_FIELD_WRAPPED(i_isolate, info, set_callback, callback);
  info->set_named_interceptor(i::Object());
  info->set_indexed_interceptor(i::Object());

  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate));
  }
  info->set_data(*Utils::OpenHandle(*data));

  i::FunctionTemplateInfo::SetAccessCheckInfo(i_isolate, cons, info);
  cons->set_needs_access_check(true);
}

void ObjectTemplate::SetAccessCheckCallbackAndHandler(
    AccessCheckCallback callback,
    const NamedPropertyHandlerConfiguration& named_handler,
    const IndexedPropertyHandlerConfiguration& indexed_handler,
    Local<Value> data) {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  auto cons = EnsureConstructor(i_isolate, this);
  EnsureNotPublished(cons,
                     "v8::ObjectTemplate::SetAccessCheckCallbackWithHandler");

  i::Handle<i::Struct> struct_info = i_isolate->factory()->NewStruct(
      i::ACCESS_CHECK_INFO_TYPE, i::AllocationType::kOld);
  i::Handle<i::AccessCheckInfo> info =
      i::Handle<i::AccessCheckInfo>::cast(struct_info);

  SET_FIELD_WRAPPED(i_isolate, info, set_callback, callback);
  auto named_interceptor = CreateNamedInterceptorInfo(
      i_isolate, named_handler.getter, named_handler.setter,
      named_handler.query, named_handler.descriptor, named_handler.deleter,
      named_handler.enumerator, named_handler.definer, named_handler.data,
      named_handler.flags);
  info->set_named_interceptor(*named_interceptor);
  auto indexed_interceptor = CreateIndexedInterceptorInfo(
      i_isolate, indexed_handler.getter, indexed_handler.setter,
      indexed_handler.query, indexed_handler.descriptor,
      indexed_handler.deleter, indexed_handler.enumerator,
      indexed_handler.definer, indexed_handler.data, indexed_handler.flags);
  info->set_indexed_interceptor(*indexed_interceptor);

  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate));
  }
  info->set_data(*Utils::OpenHandle(*data));

  i::FunctionTemplateInfo::SetAccessCheckInfo(i_isolate, cons, info);
  cons->set_needs_access_check(true);
}

void ObjectTemplate::SetHandler(
    const IndexedPropertyHandlerConfiguration& config) {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  auto cons = EnsureConstructor(i_isolate, this);
  EnsureNotPublished(cons, "v8::ObjectTemplate::SetHandler");
  auto obj = CreateIndexedInterceptorInfo(
      i_isolate, config.getter, config.setter, config.query, config.descriptor,
      config.deleter, config.enumerator, config.definer, config.data,
      config.flags);
  i::FunctionTemplateInfo::SetIndexedPropertyHandler(i_isolate, cons, obj);
}

void ObjectTemplate::SetCallAsFunctionHandler(FunctionCallback callback,
                                              Local<Value> data) {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  auto cons = EnsureConstructor(i_isolate, this);
  EnsureNotPublished(cons, "v8::ObjectTemplate::SetCallAsFunctionHandler");
  i::Handle<i::CallHandlerInfo> obj =
      i_isolate->factory()->NewCallHandlerInfo();
  obj->set_callback(i_isolate, reinterpret_cast<i::Address>(callback));
  if (data.IsEmpty()) {
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate));
  }
  obj->set_data(*Utils::OpenHandle(*data));
  i::FunctionTemplateInfo::SetInstanceCallHandler(i_isolate, cons, obj);
}

int ObjectTemplate::InternalFieldCount() const {
  return Utils::OpenHandle(this)->embedder_field_count();
}

void ObjectTemplate::SetInternalFieldCount(int value) {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  if (!Utils::ApiCheck(i::Smi::IsValid(value),
                       "v8::ObjectTemplate::SetInternalFieldCount()",
                       "Invalid embedder field count")) {
    return;
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  if (value > 0) {
    // The embedder field count is set by the constructor function's
    // construct code, so we ensure that there is a constructor
    // function to do the setting.
    EnsureConstructor(i_isolate, this);
  }
  Utils::OpenHandle(this)->set_embedder_field_count(value);
}

bool ObjectTemplate::IsImmutableProto() const {
  return Utils::OpenHandle(this)->immutable_proto();
}

void ObjectTemplate::SetImmutableProto() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  self->set_immutable_proto(true);
}

bool ObjectTemplate::IsCodeLike() const {
  return Utils::OpenHandle(this)->code_like();
}

void ObjectTemplate::SetCodeLike() {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  self->set_code_like(true);
}

// --- S c r i p t s ---

// Internally, UnboundScript and UnboundModuleScript are SharedFunctionInfos,
// and Script is a JSFunction.

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

ScriptCompiler::StreamedSource::StreamedSource(
    std::unique_ptr<ExternalSourceStream> stream, Encoding encoding)
    : impl_(new i::ScriptStreamingData(std::move(stream), encoding)) {}

ScriptCompiler::StreamedSource::~StreamedSource() = default;

Local<Script> UnboundScript::BindToCurrentContext() {
  i::Handle<i::SharedFunctionInfo> function_info = Utils::OpenHandle(this);
  // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is gone.
  DCHECK(!function_info->InReadOnlySpace());
  i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*function_info);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSFunction> function =
      i::Factory::JSFunctionBuilder{i_isolate, function_info,
                                    i_isolate->native_context()}
          .Build();
  return ToApiHandle<Script>(function);
}

int UnboundScript::GetId() const {
  i::Handle<i::SharedFunctionInfo> function_info = Utils::OpenHandle(this);
  // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is gone.
  DCHECK(!function_info->InReadOnlySpace());
  API_RCS_SCOPE(i::GetIsolateFromWritableObject(*function_info), UnboundScript,
                GetId);
  return i::Script::cast(function_info->script()).id();
}

int UnboundScript::GetLineNumber(int code_pos) {
  i::Handle<i::SharedFunctionInfo> obj = Utils::OpenHandle(this);
  if (obj->script().IsScript()) {
    // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is
    // gone.
    DCHECK(!obj->InReadOnlySpace());
    i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    API_RCS_SCOPE(i_isolate, UnboundScript, GetLineNumber);
    i::Handle<i::Script> script(i::Script::cast(obj->script()), i_isolate);
    return i::Script::GetLineNumber(script, code_pos);
  } else {
    return -1;
  }
}

int UnboundScript::GetColumnNumber(int code_pos) {
  i::Handle<i::SharedFunctionInfo> obj = Utils::OpenHandle(this);
  if (obj->script().IsScript()) {
    // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is
    // gone.
    DCHECK(!obj->InReadOnlySpace());
    i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    API_RCS_SCOPE(i_isolate, UnboundScript, GetColumnNumber);
    i::Handle<i::Script> script(i::Script::cast(obj->script()), i_isolate);
    return i::Script::GetColumnNumber(script, code_pos);
  } else {
    return -1;
  }
}

Local<Value> UnboundScript::GetScriptName() {
  i::Handle<i::SharedFunctionInfo> obj = Utils::OpenHandle(this);
  if (obj->script().IsScript()) {
    // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is
    // gone.
    DCHECK(!obj->InReadOnlySpace());
    i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    API_RCS_SCOPE(i_isolate, UnboundScript, GetName);
    i::Object name = i::Script::cast(obj->script()).name();
    return Utils::ToLocal(i::Handle<i::Object>(name, i_isolate));
  } else {
    return Local<String>();
  }
}

Local<Value> UnboundScript::GetSourceURL() {
  i::Handle<i::SharedFunctionInfo> obj = Utils::OpenHandle(this);
  if (obj->script().IsScript()) {
    // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is
    // gone.
    DCHECK(!obj->InReadOnlySpace());
    i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
    API_RCS_SCOPE(i_isolate, UnboundScript, GetSourceURL);
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    i::Object url = i::Script::cast(obj->script()).source_url();
    return Utils::ToLocal(i::Handle<i::Object>(url, i_isolate));
  } else {
    return Local<String>();
  }
}

Local<Value> UnboundScript::GetSourceMappingURL() {
  i::Handle<i::SharedFunctionInfo> obj = Utils::OpenHandle(this);
  if (obj->script().IsScript()) {
    // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is
    // gone.
    DCHECK(!obj->InReadOnlySpace());
    i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
    API_RCS_SCOPE(i_isolate, UnboundScript, GetSourceMappingURL);
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    i::Object url = i::Script::cast(obj->script()).source_mapping_url();
    return Utils::ToLocal(i::Handle<i::Object>(url, i_isolate));
  } else {
    return Local<String>();
  }
}

Local<Value> UnboundModuleScript::GetSourceURL() {
  i::Handle<i::SharedFunctionInfo> obj = Utils::OpenHandle(this);
  if (obj->script().IsScript()) {
    // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is
    // gone.
    DCHECK(!obj->InReadOnlySpace());
    i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
    API_RCS_SCOPE(i_isolate, UnboundModuleScript, GetSourceURL);
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    i::Object url = i::Script::cast(obj->script()).source_url();
    return Utils::ToLocal(i::Handle<i::Object>(url, i_isolate));
  } else {
    return Local<String>();
  }
}

Local<Value> UnboundModuleScript::GetSourceMappingURL() {
  i::Handle<i::SharedFunctionInfo> obj = Utils::OpenHandle(this);
  if (obj->script().IsScript()) {
    // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is
    // gone.
    DCHECK(!obj->InReadOnlySpace());
    i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
    API_RCS_SCOPE(i_isolate, UnboundModuleScript, GetSourceMappingURL);
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    i::Object url = i::Script::cast(obj->script()).source_mapping_url();
    return Utils::ToLocal(i::Handle<i::Object>(url, i_isolate));
  } else {
    return Local<String>();
  }
}

MaybeLocal<Value> Script::Run(Local<Context> context) {
  return Run(context, Local<Data>());
}

MaybeLocal<Value> Script::Run(Local<Context> context,
                              Local<Data> host_defined_options) {
  auto v8_isolate = context->GetIsolate();
  auto i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.Execute");
  ENTER_V8(i_isolate, context, Script, Run, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(i_isolate);
  i::NestedTimedHistogramScope execute_timer(i_isolate->counters()->execute(),
                                             i_isolate);
  i::AggregatingHistogramTimerScope histogram_timer(
      i_isolate->counters()->compile_lazy());

#if defined(V8_OS_WIN) && defined(V8_ENABLE_ETW_STACK_WALKING)
  // In case ETW has been activated, tasks to log existing code are
  // created. But in case the task runner does not run those before
  // starting to execute code (as it happens in d8, that will run
  // first the code from prompt), then that code will not have
  // JIT instrumentation on time.
  //
  // To avoid this, on running scripts check first if JIT code log is
  // pending and generate immediately.
  if (i::v8_flags.enable_etw_stack_walking) {
    i::ETWJITInterface::MaybeSetHandlerNow(i_isolate);
  }
#endif
  auto fun = i::Handle<i::JSFunction>::cast(Utils::OpenHandle(this));
  i::Handle<i::Object> receiver = i_isolate->global_proxy();
  // TODO(cbruni, chromium:1244145): Remove once migrated to the context.
  i::Handle<i::Object> options(
      i::Script::cast(fun->shared().script()).host_defined_options(),
      i_isolate);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::CallScript(i_isolate, fun, receiver, options), &result);

  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

Local<Value> ScriptOrModule::GetResourceName() {
  i::Handle<i::ScriptOrModule> obj = Utils::OpenHandle(this);
  i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> val(obj->resource_name(), i_isolate);
  return ToApiHandle<Value>(val);
}

Local<Data> ScriptOrModule::HostDefinedOptions() {
  i::Handle<i::ScriptOrModule> obj = Utils::OpenHandle(this);
  i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*obj);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> val(obj->host_defined_options(), i_isolate);
  return ToApiHandle<Data>(val);
}

Local<UnboundScript> Script::GetUnboundScript() {
  i::DisallowGarbageCollection no_gc;
  i::Handle<i::JSFunction> obj = Utils::OpenHandle(this);
  i::Handle<i::SharedFunctionInfo> sfi =
      i::handle(obj->shared(), obj->GetIsolate());
  DCHECK(!sfi->InReadOnlySpace());
  return ToApiHandle<UnboundScript>(sfi);
}

Local<Value> Script::GetResourceName() {
  i::DisallowGarbageCollection no_gc;
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  i::SharedFunctionInfo sfi = func->shared();
  CHECK(sfi.script().IsScript());
  return ToApiHandle<Value>(
      i::handle(i::Script::cast(sfi.script()).name(), func->GetIsolate()));
}

std::vector<int> Script::GetProducedCompileHints() const {
  i::DisallowGarbageCollection no_gc;
  i::Handle<i::JSFunction> func = Utils::OpenHandle(this);
  i::Isolate* i_isolate = func->GetIsolate();
  i::SharedFunctionInfo sfi = func->shared();
  CHECK(sfi.script().IsScript());
  i::Script script = i::Script::cast(sfi.script());
  i::Object maybe_array_list = script.compiled_lazy_function_positions();
  std::vector<int> result;
  if (!maybe_array_list.IsUndefined(i_isolate)) {
    i::ArrayList array_list = i::ArrayList::cast(maybe_array_list);
    result.reserve(array_list.Length());
    for (int i = 0; i < array_list.Length(); ++i) {
      i::Object item = array_list.Get(i);
      CHECK(item.IsSmi());
      result.push_back(i::Smi::ToInt(item));
    }
    // Clear the data; the embedder can still request more data later, but it'll
    // have to keep track of the original data itself.
    script.set_compiled_lazy_function_positions(
        i::ReadOnlyRoots(i_isolate).undefined_value());
  }
  return result;
}

// static
Local<PrimitiveArray> PrimitiveArray::New(Isolate* v8_isolate, int length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  Utils::ApiCheck(length >= 0, "v8::PrimitiveArray::New",
                  "length must be equal or greater than zero");
  i::Handle<i::FixedArray> array = i_isolate->factory()->NewFixedArray(length);
  return ToApiHandle<PrimitiveArray>(array);
}

int PrimitiveArray::Length() const {
  i::Handle<i::FixedArray> array = Utils::OpenHandle(this);
  return array->length();
}

void PrimitiveArray::Set(Isolate* v8_isolate, int index,
                         Local<Primitive> item) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::FixedArray> array = Utils::OpenHandle(this);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  Utils::ApiCheck(index >= 0 && index < array->length(),
                  "v8::PrimitiveArray::Set",
                  "index must be greater than or equal to 0 and less than the "
                  "array length");
  i::Handle<i::Object> i_item = Utils::OpenHandle(*item);
  array->set(index, *i_item);
}

Local<Primitive> PrimitiveArray::Get(Isolate* v8_isolate, int index) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::FixedArray> array = Utils::OpenHandle(this);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  Utils::ApiCheck(index >= 0 && index < array->length(),
                  "v8::PrimitiveArray::Get",
                  "index must be greater than or equal to 0 and less than the "
                  "array length");
  i::Handle<i::Object> i_item(array->get(index), i_isolate);
  return ToApiHandle<Primitive>(i_item);
}

void v8::PrimitiveArray::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(
      obj->IsFixedArray(), "v8::PrimitiveArray::Cast",
      "Value is not a PrimitiveArray; this is a temporary issue, v8::Data and "
      "v8::PrimitiveArray will not be compatible in the future");
}

int FixedArray::Length() const {
  i::Handle<i::FixedArray> self = Utils::OpenHandle(this);
  return self->length();
}

Local<Data> FixedArray::Get(Local<Context> context, int i) const {
  i::Handle<i::FixedArray> self = Utils::OpenHandle(this);
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  CHECK_LT(i, self->length());
  i::Handle<i::Object> entry(self->get(i), i_isolate);
  return ToApiHandle<Data>(entry);
}

Local<String> ModuleRequest::GetSpecifier() const {
  i::Handle<i::ModuleRequest> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  return ToApiHandle<String>(i::handle(self->specifier(), i_isolate));
}

int ModuleRequest::GetSourceOffset() const {
  return Utils::OpenHandle(this)->position();
}

Local<FixedArray> ModuleRequest::GetImportAssertions() const {
  i::Handle<i::ModuleRequest> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  return ToApiHandle<FixedArray>(
      i::handle(self->import_assertions(), i_isolate));
}

Module::Status Module::GetStatus() const {
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  switch (self->status()) {
    case i::Module::kUnlinked:
    case i::Module::kPreLinking:
      return kUninstantiated;
    case i::Module::kLinking:
      return kInstantiating;
    case i::Module::kLinked:
      return kInstantiated;
    case i::Module::kEvaluating:
    case i::Module::kEvaluatingAsync:
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
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return ToApiHandle<Value>(i::handle(self->GetException(), i_isolate));
}

Local<FixedArray> Module::GetModuleRequests() const {
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  if (self->IsSyntheticModule()) {
    // Synthetic modules are leaf nodes in the module graph. They have no
    // ModuleRequests.
    return ToApiHandle<FixedArray>(
        self->GetReadOnlyRoots().empty_fixed_array_handle());
  } else {
    i::Handle<i::FixedArray> module_requests(
        i::Handle<i::SourceTextModule>::cast(self)->info().module_requests(),
        i_isolate);
    return ToApiHandle<FixedArray>(module_requests);
  }
}

Location Module::SourceOffsetToLocation(int offset) const {
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  Utils::ApiCheck(
      self->IsSourceTextModule(), "v8::Module::SourceOffsetToLocation",
      "v8::Module::SourceOffsetToLocation must be used on an SourceTextModule");
  i::Handle<i::Script> script(
      i::Handle<i::SourceTextModule>::cast(self)->GetScript(), i_isolate);
  i::Script::PositionInfo info;
  i::Script::GetPositionInfo(script, offset, &info, i::Script::WITH_OFFSET);
  return v8::Location(info.line, info.column);
}

Local<Value> Module::GetModuleNamespace() {
  Utils::ApiCheck(
      GetStatus() >= kInstantiated, "v8::Module::GetModuleNamespace",
      "v8::Module::GetModuleNamespace must be used on an instantiated module");
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  auto i_isolate = self->GetIsolate();
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSModuleNamespace> module_namespace =
      i::Module::GetModuleNamespace(i_isolate, self);
  return ToApiHandle<Value>(module_namespace);
}

Local<UnboundModuleScript> Module::GetUnboundModuleScript() {
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  Utils::ApiCheck(
      self->IsSourceTextModule(), "v8::Module::GetUnboundModuleScript",
      "v8::Module::GetUnboundModuleScript must be used on an SourceTextModule");
  auto i_isolate = self->GetIsolate();
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return ToApiHandle<UnboundModuleScript>(i::handle(
      i::Handle<i::SourceTextModule>::cast(self)->GetSharedFunctionInfo(),
      i_isolate));
}

int Module::ScriptId() const {
  i::Module self = *Utils::OpenHandle(this);
  Utils::ApiCheck(self.IsSourceTextModule(), "v8::Module::ScriptId",
                  "v8::Module::ScriptId must be used on an SourceTextModule");
  DCHECK_NO_SCRIPT_NO_EXCEPTION(self.GetIsolate());
  return i::SourceTextModule::cast(self).GetScript().id();
}

bool Module::IsGraphAsync() const {
  Utils::ApiCheck(
      GetStatus() >= kInstantiated, "v8::Module::IsGraphAsync",
      "v8::Module::IsGraphAsync must be used on an instantiated module");
  i::Module self = *Utils::OpenHandle(this);
  auto i_isolate = self.GetIsolate();
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return self.IsGraphAsync(i_isolate);
}

bool Module::IsSourceTextModule() const {
  auto self = Utils::OpenHandle(this);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(self->GetIsolate());
  return self->IsSourceTextModule();
}

bool Module::IsSyntheticModule() const {
  auto self = Utils::OpenHandle(this);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(self->GetIsolate());
  return self->IsSyntheticModule();
}

int Module::GetIdentityHash() const {
  auto self = Utils::OpenHandle(this);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(self->GetIsolate());
  return self->hash();
}

Maybe<bool> Module::InstantiateModule(Local<Context> context,
                                      Module::ResolveModuleCallback callback) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Module, InstantiateModule, Nothing<bool>(),
           i::HandleScope);
  has_pending_exception = !i::Module::Instantiate(
      i_isolate, Utils::OpenHandle(this), context, callback, nullptr);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

MaybeLocal<Value> Module::Evaluate(Local<Context> context) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.Execute");
  ENTER_V8(i_isolate, context, Module, Evaluate, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(i_isolate);
  i::NestedTimedHistogramScope execute_timer(i_isolate->counters()->execute(),
                                             i_isolate);
  i::AggregatingHistogramTimerScope timer(
      i_isolate->counters()->compile_lazy());

  i::Handle<i::Module> self = Utils::OpenHandle(this);
  Utils::ApiCheck(self->status() >= i::Module::kLinked, "Module::Evaluate",
                  "Expected instantiated module");

  Local<Value> result;
  has_pending_exception =
      !ToLocal(i::Module::Evaluate(i_isolate, self), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

Local<Module> Module::CreateSyntheticModule(
    Isolate* v8_isolate, Local<String> module_name,
    const std::vector<Local<v8::String>>& export_names,
    v8::Module::SyntheticModuleEvaluationSteps evaluation_steps) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
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

Maybe<bool> Module::SetSyntheticModuleExport(Isolate* v8_isolate,
                                             Local<String> export_name,
                                             Local<v8::Value> export_value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::String> i_export_name = Utils::OpenHandle(*export_name);
  i::Handle<i::Object> i_export_value = Utils::OpenHandle(*export_value);
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  Utils::ApiCheck(self->IsSyntheticModule(),
                  "v8::Module::SyntheticModuleSetExport",
                  "v8::Module::SyntheticModuleSetExport must only be called on "
                  "a SyntheticModule");
  ENTER_V8_NO_SCRIPT(i_isolate, v8_isolate->GetCurrentContext(), Module,
                     SetSyntheticModuleExport, Nothing<bool>(), i::HandleScope);
  has_pending_exception =
      i::SyntheticModule::SetExport(i_isolate,
                                    i::Handle<i::SyntheticModule>::cast(self),
                                    i_export_name, i_export_value)
          .IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

std::vector<std::tuple<Local<Module>, Local<Message>>>
Module::GetStalledTopLevelAwaitMessage(Isolate* isolate) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::Module> self = Utils::OpenHandle(this);
  Utils::ApiCheck(self->IsSourceTextModule(),
                  "v8::Module::GetStalledTopLevelAwaitMessage",
                  "v8::Module::GetStalledTopLevelAwaitMessage must only be "
                  "called on a SourceTextModule");
  std::vector<
      std::tuple<i::Handle<i::SourceTextModule>, i::Handle<i::JSMessageObject>>>
      stalled_awaits = i::Handle<i::SourceTextModule>::cast(self)
                           ->GetStalledTopLevelAwaitMessage(i_isolate);

  std::vector<std::tuple<Local<Module>, Local<Message>>> result;
  size_t stalled_awaits_count = stalled_awaits.size();
  if (stalled_awaits_count == 0) {
    return result;
  }
  result.reserve(stalled_awaits_count);
  for (size_t i = 0; i < stalled_awaits_count; ++i) {
    auto [module, message] = stalled_awaits[i];
    result.push_back(std::make_tuple(ToApiHandle<Module>(module),
                                     ToApiHandle<Message>(message)));
  }
  return result;
}

namespace {

i::ScriptDetails GetScriptDetails(
    i::Isolate* i_isolate, Local<Value> resource_name, int resource_line_offset,
    int resource_column_offset, Local<Value> source_map_url,
    Local<Data> host_defined_options, ScriptOriginOptions origin_options) {
  i::ScriptDetails script_details(Utils::OpenHandle(*(resource_name), true),
                                  origin_options);
  script_details.line_offset = resource_line_offset;
  script_details.column_offset = resource_column_offset;
  script_details.host_defined_options =
      host_defined_options.IsEmpty()
          ? i_isolate->factory()->empty_fixed_array()
          : Utils::OpenHandle(*(host_defined_options));
  if (!source_map_url.IsEmpty()) {
    script_details.source_map_url = Utils::OpenHandle(*(source_map_url));
  }
  return script_details;
}

}  // namespace

MaybeLocal<UnboundScript> ScriptCompiler::CompileUnboundInternal(
    Isolate* v8_isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.ScriptCompiler");
  ENTER_V8_NO_SCRIPT(i_isolate, v8_isolate->GetCurrentContext(), ScriptCompiler,
                     CompileUnbound, MaybeLocal<UnboundScript>(),
                     InternalEscapableScope);

  i::Handle<i::String> str = Utils::OpenHandle(*(source->source_string));

  i::Handle<i::SharedFunctionInfo> result;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileScript");
  i::ScriptDetails script_details = GetScriptDetails(
      i_isolate, source->resource_name, source->resource_line_offset,
      source->resource_column_offset, source->source_map_url,
      source->host_defined_options, source->resource_options);

  i::MaybeHandle<i::SharedFunctionInfo> maybe_function_info;
  if (options == kConsumeCodeCache) {
    if (source->consume_cache_task) {
      // Take ownership of the internal deserialization task and clear it off
      // the consume task on the source.
      DCHECK_NOT_NULL(source->consume_cache_task->impl_);
      std::unique_ptr<i::BackgroundDeserializeTask> deserialize_task =
          std::move(source->consume_cache_task->impl_);
      maybe_function_info =
          i::Compiler::GetSharedFunctionInfoForScriptWithDeserializeTask(
              i_isolate, str, script_details, deserialize_task.get(), options,
              no_cache_reason, i::NOT_NATIVES_CODE);
      source->cached_data->rejected = deserialize_task->rejected();
    } else {
      DCHECK(source->cached_data);
      // AlignedCachedData takes care of pointer-aligning the data.
      auto cached_data = std::make_unique<i::AlignedCachedData>(
          source->cached_data->data, source->cached_data->length);
      maybe_function_info =
          i::Compiler::GetSharedFunctionInfoForScriptWithCachedData(
              i_isolate, str, script_details, cached_data.get(), options,
              no_cache_reason, i::NOT_NATIVES_CODE);
      source->cached_data->rejected = cached_data->rejected();
    }
  } else if (options == kConsumeCompileHints) {
    maybe_function_info =
        i::Compiler::GetSharedFunctionInfoForScriptWithCompileHints(
            i_isolate, str, script_details, source->compile_hint_callback,
            source->compile_hint_callback_data, options, no_cache_reason,
            i::NOT_NATIVES_CODE);
  } else {
    // Compile without any cache.
    maybe_function_info = i::Compiler::GetSharedFunctionInfoForScript(
        i_isolate, str, script_details, options, no_cache_reason,
        i::NOT_NATIVES_CODE);
  }

  has_pending_exception = !maybe_function_info.ToHandle(&result);
  DCHECK_IMPLIES(!has_pending_exception, !result->InReadOnlySpace());
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
  auto i_isolate = context->GetIsolate();
  MaybeLocal<UnboundScript> maybe =
      CompileUnboundInternal(i_isolate, source, options, no_cache_reason);
  Local<UnboundScript> result;
  if (!maybe.ToLocal(&result)) return MaybeLocal<Script>();
  v8::Context::Scope scope(context);
  return result->BindToCurrentContext();
}

MaybeLocal<Module> ScriptCompiler::CompileModule(
    Isolate* v8_isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  Utils::ApiCheck(
      options == kNoCompileOptions || options == kConsumeCodeCache ||
          options == kProduceCompileHints,
      "v8::ScriptCompiler::CompileModule", "Invalid CompileOptions");
  Utils::ApiCheck(source->GetResourceOptions().IsModule(),
                  "v8::ScriptCompiler::CompileModule",
                  "Invalid ScriptOrigin: is_module must be true");
  MaybeLocal<UnboundScript> maybe =
      CompileUnboundInternal(v8_isolate, source, options, no_cache_reason);
  Local<UnboundScript> unbound;
  if (!maybe.ToLocal(&unbound)) return MaybeLocal<Module>();
  i::Handle<i::SharedFunctionInfo> shared = Utils::OpenHandle(*unbound);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  return ToApiHandle<Module>(i_isolate->factory()->NewSourceTextModule(shared));
}

// static
V8_WARN_UNUSED_RESULT MaybeLocal<Function> ScriptCompiler::CompileFunction(
    Local<Context> context, Source* source, size_t arguments_count,
    Local<String> arguments[], size_t context_extension_count,
    Local<Object> context_extensions[], CompileOptions options,
    NoCacheReason no_cache_reason) {
  return CompileFunctionInternal(context, source, arguments_count, arguments,
                                 context_extension_count, context_extensions,
                                 options, no_cache_reason, nullptr);
}

#ifdef V8_SCRIPTORMODULE_LEGACY_LIFETIME
// static
MaybeLocal<Function> ScriptCompiler::CompileFunctionInContext(
    Local<Context> context, Source* source, size_t arguments_count,
    Local<String> arguments[], size_t context_extension_count,
    Local<Object> context_extensions[], CompileOptions options,
    NoCacheReason no_cache_reason,
    Local<ScriptOrModule>* script_or_module_out) {
  return CompileFunctionInternal(
      context, source, arguments_count, arguments, context_extension_count,
      context_extensions, options, no_cache_reason, script_or_module_out);
}
#endif  // V8_SCRIPTORMODULE_LEGACY_LIFETIME

MaybeLocal<Function> ScriptCompiler::CompileFunctionInternal(
    Local<Context> v8_context, Source* source, size_t arguments_count,
    Local<String> arguments[], size_t context_extension_count,
    Local<Object> context_extensions[], CompileOptions options,
    NoCacheReason no_cache_reason,
    Local<ScriptOrModule>* script_or_module_out) {
  Local<Function> result;

  {
    PREPARE_FOR_EXECUTION(v8_context, ScriptCompiler, CompileFunction,
                          Function);
    TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.ScriptCompiler");

    DCHECK(options == CompileOptions::kConsumeCodeCache ||
           options == CompileOptions::kEagerCompile ||
           options == CompileOptions::kNoCompileOptions);

    i::Handle<i::Context> context = Utils::OpenHandle(*v8_context);

    DCHECK(context->IsNativeContext());

    i::Handle<i::FixedArray> arguments_list =
        i_isolate->factory()->NewFixedArray(static_cast<int>(arguments_count));
    for (int i = 0; i < static_cast<int>(arguments_count); i++) {
      i::Handle<i::String> argument = Utils::OpenHandle(*arguments[i]);
      if (!i::String::IsIdentifier(i_isolate, argument))
        return Local<Function>();
      arguments_list->set(i, *argument);
    }

    for (size_t i = 0; i < context_extension_count; ++i) {
      i::Handle<i::JSReceiver> extension =
          Utils::OpenHandle(*context_extensions[i]);
      if (!extension->IsJSObject()) return Local<Function>();
      context = i_isolate->factory()->NewWithContext(
          context,
          i::ScopeInfo::CreateForWithScope(
              i_isolate,
              context->IsNativeContext()
                  ? i::Handle<i::ScopeInfo>::null()
                  : i::Handle<i::ScopeInfo>(context->scope_info(), i_isolate)),
          extension);
    }

    i::ScriptDetails script_details = GetScriptDetails(
        i_isolate, source->resource_name, source->resource_line_offset,
        source->resource_column_offset, source->source_map_url,
        source->host_defined_options, source->resource_options);

    std::unique_ptr<i::AlignedCachedData> cached_data;
    if (options == kConsumeCodeCache) {
      DCHECK(source->cached_data);
      // ScriptData takes care of pointer-aligning the data.
      cached_data.reset(new i::AlignedCachedData(source->cached_data->data,
                                                 source->cached_data->length));
    }

    i::Handle<i::JSFunction> scoped_result;
    has_pending_exception =
        !i::Compiler::GetWrappedFunction(
             Utils::OpenHandle(*source->source_string), arguments_list, context,
             script_details, cached_data.get(), options, no_cache_reason)
             .ToHandle(&scoped_result);
    if (options == kConsumeCodeCache) {
      source->cached_data->rejected = cached_data->rejected();
    }
    RETURN_ON_FAILED_EXECUTION(Function);
    result = handle_scope.Escape(Utils::CallableToLocal(scoped_result));
  }
  // TODO(cbruni): remove script_or_module_out paramater
  if (script_or_module_out != nullptr) {
    i::Handle<i::JSFunction> function =
        i::Handle<i::JSFunction>::cast(Utils::OpenHandle(*result));
    i::Isolate* i_isolate = function->GetIsolate();
    i::Handle<i::SharedFunctionInfo> shared(function->shared(), i_isolate);
    i::Handle<i::Script> script(i::Script::cast(shared->script()), i_isolate);
    // TODO(cbruni, v8:12302): Avoid creating tempory ScriptOrModule objects.
    auto script_or_module = i::Handle<i::ScriptOrModule>::cast(
        i_isolate->factory()->NewStruct(i::SCRIPT_OR_MODULE_TYPE));
    script_or_module->set_resource_name(script->name());
    script_or_module->set_host_defined_options(script->host_defined_options());
#ifdef V8_SCRIPTORMODULE_LEGACY_LIFETIME
    i::Handle<i::ArrayList> list =
        i::handle(script->script_or_modules(), i_isolate);
    list = i::ArrayList::Add(i_isolate, list, script_or_module);
    script->set_script_or_modules(*list);
#endif  // V8_SCRIPTORMODULE_LEGACY_LIFETIME
    *script_or_module_out = v8::Utils::ToLocal(script_or_module);
  }
  return result;
}

void ScriptCompiler::ScriptStreamingTask::Run() { data_->task->Run(); }

ScriptCompiler::ScriptStreamingTask* ScriptCompiler::StartStreaming(
    Isolate* v8_isolate, StreamedSource* source, v8::ScriptType type,
    CompileOptions options) {
  Utils::ApiCheck(options == kNoCompileOptions || options == kEagerCompile ||
                      options == kProduceCompileHints,
                  "v8::ScriptCompiler::StartStreaming",
                  "Invalid CompileOptions");
  if (!i::v8_flags.script_streaming) return nullptr;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::ScriptStreamingData* data = source->impl();
  std::unique_ptr<i::BackgroundCompileTask> task =
      std::make_unique<i::BackgroundCompileTask>(data, i_isolate, type,
                                                 options);
  data->task = std::move(task);
  return new ScriptCompiler::ScriptStreamingTask(data);
}

ScriptCompiler::ConsumeCodeCacheTask::ConsumeCodeCacheTask(
    std::unique_ptr<i::BackgroundDeserializeTask> impl)
    : impl_(std::move(impl)) {}

ScriptCompiler::ConsumeCodeCacheTask::~ConsumeCodeCacheTask() = default;

void ScriptCompiler::ConsumeCodeCacheTask::Run() { impl_->Run(); }

void ScriptCompiler::ConsumeCodeCacheTask::SourceTextAvailable(
    Isolate* v8_isolate, Local<String> source_text,
    const ScriptOrigin& origin) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::String> str = Utils::OpenHandle(*(source_text));
  i::ScriptDetails script_details =
      GetScriptDetails(i_isolate, origin.ResourceName(), origin.LineOffset(),
                       origin.ColumnOffset(), origin.SourceMapUrl(),
                       origin.GetHostDefinedOptions(), origin.Options());
  impl_->SourceTextAvailable(i_isolate, str, script_details);
}

bool ScriptCompiler::ConsumeCodeCacheTask::ShouldMergeWithExistingScript()
    const {
  if (!i::v8_flags
           .merge_background_deserialized_script_with_compilation_cache) {
    return false;
  }
  return impl_->ShouldMergeWithExistingScript();
}

void ScriptCompiler::ConsumeCodeCacheTask::MergeWithExistingScript() {
  DCHECK(
      i::v8_flags.merge_background_deserialized_script_with_compilation_cache);
  impl_->MergeWithExistingScript();
}

ScriptCompiler::ConsumeCodeCacheTask* ScriptCompiler::StartConsumingCodeCache(
    Isolate* v8_isolate, std::unique_ptr<CachedData> cached_data) {
  if (!i::v8_flags.concurrent_cache_deserialization) return nullptr;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return new ScriptCompiler::ConsumeCodeCacheTask(
      std::make_unique<i::BackgroundDeserializeTask>(i_isolate,
                                                     std::move(cached_data)));
}

namespace {
i::MaybeHandle<i::SharedFunctionInfo> CompileStreamedSource(
    i::Isolate* i_isolate, ScriptCompiler::StreamedSource* v8_source,
    Local<String> full_source_string, const ScriptOrigin& origin) {
  i::Handle<i::String> str = Utils::OpenHandle(*(full_source_string));
  i::ScriptDetails script_details =
      GetScriptDetails(i_isolate, origin.ResourceName(), origin.LineOffset(),
                       origin.ColumnOffset(), origin.SourceMapUrl(),
                       origin.GetHostDefinedOptions(), origin.Options());
  i::ScriptStreamingData* data = v8_source->impl();
  return i::Compiler::GetSharedFunctionInfoForStreamedScript(
      i_isolate, str, script_details, data);
}

}  // namespace

MaybeLocal<Script> ScriptCompiler::Compile(Local<Context> context,
                                           StreamedSource* v8_source,
                                           Local<String> full_source_string,
                                           const ScriptOrigin& origin) {
  PREPARE_FOR_EXECUTION(context, ScriptCompiler, Compile, Script);
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.ScriptCompiler");
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileStreamedScript");
  i::Handle<i::SharedFunctionInfo> sfi;
  i::MaybeHandle<i::SharedFunctionInfo> maybe_sfi =
      CompileStreamedSource(i_isolate, v8_source, full_source_string, origin);
  has_pending_exception = !maybe_sfi.ToHandle(&sfi);
  if (has_pending_exception) i_isolate->ReportPendingMessages();
  RETURN_ON_FAILED_EXECUTION(Script);
  Local<UnboundScript> generic = ToApiHandle<UnboundScript>(sfi);
  if (generic.IsEmpty()) return Local<Script>();
  Local<Script> bound = generic->BindToCurrentContext();
  if (bound.IsEmpty()) return Local<Script>();
  RETURN_ESCAPED(bound);
}

MaybeLocal<Module> ScriptCompiler::CompileModule(
    Local<Context> context, StreamedSource* v8_source,
    Local<String> full_source_string, const ScriptOrigin& origin) {
  PREPARE_FOR_EXECUTION(context, ScriptCompiler, Compile, Module);
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.ScriptCompiler");
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileStreamedModule");
  i::Handle<i::SharedFunctionInfo> sfi;
  i::MaybeHandle<i::SharedFunctionInfo> maybe_sfi =
      CompileStreamedSource(i_isolate, v8_source, full_source_string, origin);
  has_pending_exception = !maybe_sfi.ToHandle(&sfi);
  if (has_pending_exception) i_isolate->ReportPendingMessages();
  RETURN_ON_FAILED_EXECUTION(Module);
  RETURN_ESCAPED(
      ToApiHandle<Module>(i_isolate->factory()->NewSourceTextModule(sfi)));
}

uint32_t ScriptCompiler::CachedDataVersionTag() {
  return static_cast<uint32_t>(base::hash_combine(
      internal::Version::Hash(), internal::FlagList::Hash(),
      static_cast<uint32_t>(internal::CpuFeatures::SupportedFeatures())));
}

ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCache(
    Local<UnboundScript> unbound_script) {
  i::Handle<i::SharedFunctionInfo> shared = Utils::OpenHandle(*unbound_script);
  // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is gone.
  DCHECK(!shared->InReadOnlySpace());
  i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*shared);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  DCHECK(shared->is_toplevel());
  return i::CodeSerializer::Serialize(i_isolate, shared);
}

// static
ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCache(
    Local<UnboundModuleScript> unbound_module_script) {
  i::Handle<i::SharedFunctionInfo> shared =
      Utils::OpenHandle(*unbound_module_script);
  // TODO(jgruber): Remove this DCHECK once Function::GetUnboundScript is gone.
  DCHECK(!shared->InReadOnlySpace());
  i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*shared);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  DCHECK(shared->is_toplevel());
  return i::CodeSerializer::Serialize(i_isolate, shared);
}

ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCacheForFunction(
    Local<Function> function) {
  i::Handle<i::JSFunction> js_function =
      i::Handle<i::JSFunction>::cast(Utils::OpenHandle(*function));
  i::Isolate* i_isolate = js_function->GetIsolate();
  i::Handle<i::SharedFunctionInfo> shared(js_function->shared(), i_isolate);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  Utils::ApiCheck(shared->is_wrapped(),
                  "v8::ScriptCompiler::CreateCodeCacheForFunction",
                  "Expected SharedFunctionInfo with wrapped source code");
  return i::CodeSerializer::Serialize(i_isolate, shared);
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

v8::TryCatch::TryCatch(v8::Isolate* v8_isolate)
    : i_isolate_(reinterpret_cast<i::Isolate*>(v8_isolate)),
      next_(i_isolate_->try_catch_handler()),
      is_verbose_(false),
      can_continue_(true),
      capture_message_(true),
      rethrow_(false),
      has_terminated_(false) {
  ResetInternal();
  // Special handling for simulators which have a separate JS stack.
  js_stack_comparable_address_ = static_cast<internal::Address>(
      i::SimulatorStack::RegisterJSStackComparableAddress(i_isolate_));
  i_isolate_->RegisterTryCatchHandler(this);
}

v8::TryCatch::~TryCatch() {
  if (rethrow_) {
    v8::Isolate* v8_isolate = reinterpret_cast<Isolate*>(i_isolate_);
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Value> exc =
        v8::Local<v8::Value>::New(v8_isolate, Exception());
    if (HasCaught() && capture_message_) {
      // If an exception was caught and rethrow_ is indicated, the saved
      // message, script, and location need to be restored to Isolate TLS
      // for reuse.  capture_message_ needs to be disabled so that Throw()
      // does not create a new message.
      i_isolate_->thread_local_top()->rethrowing_message_ = true;
      i_isolate_->RestorePendingMessageFromTryCatch(this);
    }
    i_isolate_->UnregisterTryCatchHandler(this);
    i::SimulatorStack::UnregisterJSStackComparableAddress(i_isolate_);
    reinterpret_cast<v8::Isolate*>(i_isolate_)->ThrowException(exc);
    DCHECK(!i_isolate_->thread_local_top()->rethrowing_message_);
  } else {
    if (HasCaught() && i_isolate_->has_scheduled_exception()) {
      // If an exception was caught but is still scheduled because no API call
      // promoted it, then it is canceled to prevent it from being propagated.
      // Note that this will not cancel termination exceptions.
      i_isolate_->CancelScheduledExceptionFromTryCatch(this);
    }
    i_isolate_->UnregisterTryCatchHandler(this);
    i::SimulatorStack::UnregisterJSStackComparableAddress(i_isolate_);
  }
}

void* v8::TryCatch::operator new(size_t) { base::OS::Abort(); }
void* v8::TryCatch::operator new[](size_t) { base::OS::Abort(); }
void v8::TryCatch::operator delete(void*, size_t) { base::OS::Abort(); }
void v8::TryCatch::operator delete[](void*, size_t) { base::OS::Abort(); }

bool v8::TryCatch::HasCaught() const {
  return !i::Object(reinterpret_cast<i::Address>(exception_))
              .IsTheHole(i_isolate_);
}

bool v8::TryCatch::CanContinue() const { return can_continue_; }

bool v8::TryCatch::HasTerminated() const { return has_terminated_; }

v8::Local<v8::Value> v8::TryCatch::ReThrow() {
  if (!HasCaught()) return v8::Local<v8::Value>();
  rethrow_ = true;
  return v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate_));
}

v8::Local<Value> v8::TryCatch::Exception() const {
  if (HasCaught()) {
    // Check for out of memory exception.
    i::Object exception(reinterpret_cast<i::Address>(exception_));
    return v8::Utils::ToLocal(i::Handle<i::Object>(exception, i_isolate_));
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
  i::Handle<i::String> name = i_isolate->factory()->stack_string();
  Maybe<bool> maybe = i::JSReceiver::HasProperty(i_isolate, obj, name);
  has_pending_exception = maybe.IsNothing();
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!maybe.FromJust()) return v8::Local<Value>();
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::JSReceiver::GetProperty(i_isolate, obj, name), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<Value> v8::TryCatch::StackTrace(Local<Context> context) const {
  if (!HasCaught()) return v8::Local<Value>();
  return StackTrace(context, Exception());
}

v8::Local<v8::Message> v8::TryCatch::Message() const {
  i::Object message(reinterpret_cast<i::Address>(message_obj_));
  DCHECK(message.IsJSMessageObject() || message.IsTheHole(i_isolate_));
  if (HasCaught() && !message.IsTheHole(i_isolate_)) {
    return v8::Utils::MessageToLocal(i::Handle<i::Object>(message, i_isolate_));
  } else {
    return v8::Local<v8::Message>();
  }
}

void v8::TryCatch::Reset() {
  if (!rethrow_ && HasCaught() && i_isolate_->has_scheduled_exception()) {
    // If an exception was caught but is still scheduled because no API call
    // promoted it, then it is canceled to prevent it from being propagated.
    // Note that this will not cancel termination exceptions.
    i_isolate_->CancelScheduledExceptionFromTryCatch(this);
  }
  ResetInternal();
}

void v8::TryCatch::ResetInternal() {
  i::Object the_hole = i::ReadOnlyRoots(i_isolate_).the_hole_value();
  exception_ = reinterpret_cast<void*>(the_hole.ptr());
  message_obj_ = reinterpret_cast<void*>(the_hole.ptr());
}

void v8::TryCatch::SetVerbose(bool value) { is_verbose_ = value; }

bool v8::TryCatch::IsVerbose() const { return is_verbose_; }

void v8::TryCatch::SetCaptureMessage(bool value) { capture_message_ = value; }

// --- M e s s a g e ---

Local<String> Message::Get() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(i_isolate));
  i::Handle<i::String> raw_result =
      i::MessageHandler::GetMessage(i_isolate, self);
  Local<String> result = Utils::ToLocal(raw_result);
  return scope.Escape(result);
}

v8::Isolate* Message::GetIsolate() const {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  return reinterpret_cast<Isolate*>(i_isolate);
}

ScriptOrigin Message::GetScriptOrigin() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Script> script(self->script(), i_isolate);
  return GetScriptOriginForScript(i_isolate, script);
}

void ScriptOrigin::VerifyHostDefinedOptions() const {
  // TODO(cbruni, chromium:1244145): Remove checks once we allow arbitrary
  // host-defined options.
  USE(v8_isolate_);
  if (host_defined_options_.IsEmpty()) return;
  Utils::ApiCheck(host_defined_options_->IsFixedArray(), "ScriptOrigin()",
                  "Host-defined options has to be a PrimitiveArray");
  i::Handle<i::FixedArray> options =
      Utils::OpenHandle(*host_defined_options_.As<FixedArray>());
  for (int i = 0; i < options->length(); i++) {
    Utils::ApiCheck(options->get(i).IsPrimitive(), "ScriptOrigin()",
                    "PrimitiveArray can only contain primtive values");
  }
}

v8::Local<Value> Message::GetScriptResourceName() const {
  DCHECK_NO_SCRIPT_NO_EXCEPTION(Utils::OpenHandle(this)->GetIsolate());
  return GetScriptOrigin().ResourceName();
}

v8::Local<v8::StackTrace> Message::GetStackTrace() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope scope(reinterpret_cast<Isolate*>(i_isolate));
  i::Handle<i::Object> stackFramesObj(self->stack_frames(), i_isolate);
  if (!stackFramesObj->IsFixedArray()) return v8::Local<v8::StackTrace>();
  auto stackTrace = i::Handle<i::FixedArray>::cast(stackFramesObj);
  return scope.Escape(Utils::StackTraceToLocal(stackTrace));
}

Maybe<int> Message::GetLineNumber(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(i_isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(i_isolate, self);
  return Just(self->GetLineNumber());
}

int Message::GetStartPosition() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(i_isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(i_isolate, self);
  return self->GetStartPosition();
}

int Message::GetEndPosition() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(i_isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(i_isolate, self);
  return self->GetEndPosition();
}

int Message::ErrorLevel() const {
  auto self = Utils::OpenHandle(this);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(self->GetIsolate());
  return self->error_level();
}

int Message::GetStartColumn() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(i_isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(i_isolate, self);
  return self->GetColumnNumber();
}

int Message::GetWasmFunctionIndex() const {
#if V8_ENABLE_WEBASSEMBLY
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(i_isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(i_isolate, self);
  int start_position = self->GetColumnNumber();
  if (start_position == -1) return Message::kNoWasmFunctionIndexInfo;

  i::Handle<i::Script> script(self->script(), i_isolate);

  if (script->type() != i::Script::TYPE_WASM) {
    return Message::kNoWasmFunctionIndexInfo;
  }

  auto debug_script = ToApiHandle<debug::Script>(script);
  return Local<debug::WasmScript>::Cast(debug_script)
      ->GetContainingFunction(start_position);
#else
  return Message::kNoWasmFunctionIndexInfo;
#endif  // V8_ENABLE_WEBASSEMBLY
}

Maybe<int> Message::GetStartColumn(Local<Context> context) const {
  return Just(GetStartColumn());
}

int Message::GetEndColumn() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(i_isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(i_isolate, self);
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
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return self->script().origin_options().IsSharedCrossOrigin();
}

bool Message::IsOpaque() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return self->script().origin_options().IsOpaque();
}

MaybeLocal<String> Message::GetSource(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(i_isolate));
  i::Handle<i::String> source(self->GetSource(), i_isolate);
  RETURN_ESCAPED(Utils::ToLocal(source));
}

MaybeLocal<String> Message::GetSourceLine(Local<Context> context) const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  EscapableHandleScope handle_scope(reinterpret_cast<Isolate*>(i_isolate));
  i::JSMessageObject::EnsureSourcePositionsAvailable(i_isolate, self);
  RETURN_ESCAPED(Utils::ToLocal(self->GetSourceLine()));
}

void Message::PrintCurrentStackTrace(Isolate* v8_isolate, std::ostream& out) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i_isolate->PrintCurrentStackTrace(out);
}

// --- S t a c k T r a c e ---

Local<StackFrame> StackTrace::GetFrame(Isolate* v8_isolate,
                                       uint32_t index) const {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::StackFrameInfo> info(
      i::StackFrameInfo::cast(Utils::OpenHandle(this)->get(index)), i_isolate);
  return Utils::StackFrameToLocal(info);
}

int StackTrace::GetFrameCount() const {
  return Utils::OpenHandle(this)->length();
}

Local<StackTrace> StackTrace::CurrentStackTrace(Isolate* v8_isolate,
                                                int frame_limit,
                                                StackTraceOptions options) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::FixedArray> stackTrace =
      i_isolate->CaptureDetailedStackTrace(frame_limit, options);
  return Utils::StackTraceToLocal(stackTrace);
}

Local<String> StackTrace::CurrentScriptNameOrSourceURL(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::String> name_or_source_url =
      i_isolate->CurrentScriptNameOrSourceURL();
  return Utils::ToLocal(name_or_source_url);
}

// --- S t a c k F r a m e ---

Location StackFrame::GetLocation() const {
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  i::Handle<i::Script> script(self->script(), i_isolate);
  i::Script::PositionInfo info;
  CHECK(i::Script::GetPositionInfo(script,
                                   i::StackFrameInfo::GetSourcePosition(self),
                                   &info, i::Script::WITH_OFFSET));
  if (script->HasSourceURLComment()) {
    info.line -= script->line_offset();
    if (info.line == 0) {
      info.column -= script->column_offset();
    }
  }
  return {info.line, info.column};
}

int StackFrame::GetScriptId() const {
  return Utils::OpenHandle(this)->script().id();
}

Local<String> StackFrame::GetScriptName() const {
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  i::Handle<i::Object> name(self->script().name(), i_isolate);
  if (!name->IsString()) return {};
  return Utils::ToLocal(i::Handle<i::String>::cast(name));
}

Local<String> StackFrame::GetScriptNameOrSourceURL() const {
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  i::Handle<i::Object> name_or_source_url(self->script().GetNameOrSourceURL(),
                                          i_isolate);
  if (!name_or_source_url->IsString()) return {};
  return Utils::ToLocal(i::Handle<i::String>::cast(name_or_source_url));
}

Local<String> StackFrame::GetScriptSource() const {
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  if (!self->script().HasValidSource()) return {};
  i::Handle<i::PrimitiveHeapObject> source(self->script().source(), i_isolate);
  if (!source->IsString()) return {};
  return Utils::ToLocal(i::Handle<i::String>::cast(source));
}

Local<String> StackFrame::GetScriptSourceMappingURL() const {
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  i::Handle<i::Object> source_mapping_url(self->script().source_mapping_url(),
                                          i_isolate);
  if (!source_mapping_url->IsString()) return {};
  return Utils::ToLocal(i::Handle<i::String>::cast(source_mapping_url));
}

Local<String> StackFrame::GetFunctionName() const {
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  i::Handle<i::String> name(self->function_name(), i_isolate);
  if (name->length() == 0) return {};
  return Utils::ToLocal(name);
}

bool StackFrame::IsEval() const {
  i::Handle<i::StackFrameInfo> self = Utils::OpenHandle(this);
  return self->script().compilation_type() == i::Script::COMPILATION_TYPE_EVAL;
}

bool StackFrame::IsConstructor() const {
  return Utils::OpenHandle(this)->is_constructor();
}

bool StackFrame::IsWasm() const { return !IsUserJavaScript(); }

bool StackFrame::IsUserJavaScript() const {
  return Utils::OpenHandle(this)->script().IsUserJavaScript();
}

// --- J S O N ---

MaybeLocal<Value> JSON::Parse(Local<Context> context,
                              Local<String> json_string) {
  PREPARE_FOR_EXECUTION(context, JSON, Parse, Value);
  i::Handle<i::String> string = Utils::OpenHandle(*json_string);
  i::Handle<i::String> source = i::String::Flatten(i_isolate, string);
  i::Handle<i::Object> undefined = i_isolate->factory()->undefined_value();
  auto maybe =
      source->IsOneByteRepresentation()
          ? i::JsonParser<uint8_t>::Parse(i_isolate, source, undefined)
          : i::JsonParser<uint16_t>::Parse(i_isolate, source, undefined);
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
  i::Handle<i::Object> replacer = i_isolate->factory()->undefined_value();
  i::Handle<i::String> gap_string = gap.IsEmpty()
                                        ? i_isolate->factory()->empty_string()
                                        : Utils::OpenHandle(*gap);
  i::Handle<i::Object> maybe;
  has_pending_exception =
      !i::JsonStringify(i_isolate, object, replacer, gap_string)
           .ToHandle(&maybe);
  RETURN_ON_FAILED_EXECUTION(String);
  Local<String> result;
  has_pending_exception =
      !ToLocal<String>(i::Object::ToString(i_isolate, maybe), &result);
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(result);
}

// --- V a l u e   S e r i a l i z a t i o n ---

SharedValueConveyor::SharedValueConveyor(SharedValueConveyor&& other) noexcept
    : private_(std::move(other.private_)) {}

SharedValueConveyor::~SharedValueConveyor() = default;

SharedValueConveyor& SharedValueConveyor::operator=(
    SharedValueConveyor&& other) noexcept {
  private_ = std::move(other.private_);
  return *this;
}

SharedValueConveyor::SharedValueConveyor(Isolate* v8_isolate)
    : private_(std::make_unique<i::SharedObjectConveyorHandles>(
          reinterpret_cast<i::Isolate*>(v8_isolate))) {}

Maybe<bool> ValueSerializer::Delegate::WriteHostObject(Isolate* v8_isolate,
                                                       Local<Object> object) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i_isolate->ScheduleThrow(*i_isolate->factory()->NewError(
      i_isolate->error_function(), i::MessageTemplate::kDataCloneError,
      Utils::OpenHandle(*object)));
  return Nothing<bool>();
}

Maybe<uint32_t> ValueSerializer::Delegate::GetSharedArrayBufferId(
    Isolate* v8_isolate, Local<SharedArrayBuffer> shared_array_buffer) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i_isolate->ScheduleThrow(*i_isolate->factory()->NewError(
      i_isolate->error_function(), i::MessageTemplate::kDataCloneError,
      Utils::OpenHandle(*shared_array_buffer)));
  return Nothing<uint32_t>();
}

Maybe<uint32_t> ValueSerializer::Delegate::GetWasmModuleTransferId(
    Isolate* v8_isolate, Local<WasmModuleObject> module) {
  return Nothing<uint32_t>();
}

bool ValueSerializer::Delegate::AdoptSharedValueConveyor(
    Isolate* v8_isolate, SharedValueConveyor&& conveyor) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i_isolate->ScheduleThrow(*i_isolate->factory()->NewError(
      i_isolate->error_function(), i::MessageTemplate::kDataCloneError,
      i_isolate->factory()->NewStringFromAsciiChecked("shared value")));
  return false;
}

void* ValueSerializer::Delegate::ReallocateBufferMemory(void* old_buffer,
                                                        size_t size,
                                                        size_t* actual_size) {
  *actual_size = size;
  return base::Realloc(old_buffer, size);
}

void ValueSerializer::Delegate::FreeBufferMemory(void* buffer) {
  return base::Free(buffer);
}

struct ValueSerializer::PrivateData {
  explicit PrivateData(i::Isolate* i, ValueSerializer::Delegate* delegate)
      : isolate(i), serializer(i, delegate) {}
  i::Isolate* isolate;
  i::ValueSerializer serializer;
};

ValueSerializer::ValueSerializer(Isolate* v8_isolate)
    : ValueSerializer(v8_isolate, nullptr) {}

ValueSerializer::ValueSerializer(Isolate* v8_isolate, Delegate* delegate)
    : private_(new PrivateData(reinterpret_cast<i::Isolate*>(v8_isolate),
                               delegate)) {}

ValueSerializer::~ValueSerializer() { delete private_; }

void ValueSerializer::WriteHeader() { private_->serializer.WriteHeader(); }

void ValueSerializer::SetTreatArrayBufferViewsAsHostObjects(bool mode) {
  private_->serializer.SetTreatArrayBufferViewsAsHostObjects(mode);
}

Maybe<bool> ValueSerializer::WriteValue(Local<Context> context,
                                        Local<Value> value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, ValueSerializer, WriteValue, Nothing<bool>(),
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i_isolate->ScheduleThrow(*i_isolate->factory()->NewError(
      i_isolate->error_function(),
      i::MessageTemplate::kDataCloneDeserializationError));
  return MaybeLocal<Object>();
}

MaybeLocal<WasmModuleObject> ValueDeserializer::Delegate::GetWasmModuleFromId(
    Isolate* v8_isolate, uint32_t id) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i_isolate->ScheduleThrow(*i_isolate->factory()->NewError(
      i_isolate->error_function(),
      i::MessageTemplate::kDataCloneDeserializationError));
  return MaybeLocal<WasmModuleObject>();
}

MaybeLocal<SharedArrayBuffer>
ValueDeserializer::Delegate::GetSharedArrayBufferFromId(Isolate* v8_isolate,
                                                        uint32_t id) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i_isolate->ScheduleThrow(*i_isolate->factory()->NewError(
      i_isolate->error_function(),
      i::MessageTemplate::kDataCloneDeserializationError));
  return MaybeLocal<SharedArrayBuffer>();
}

const SharedValueConveyor* ValueDeserializer::Delegate::GetSharedValueConveyor(
    Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i_isolate->ScheduleThrow(*i_isolate->factory()->NewError(
      i_isolate->error_function(),
      i::MessageTemplate::kDataCloneDeserializationError));
  return nullptr;
}

struct ValueDeserializer::PrivateData {
  PrivateData(i::Isolate* i_isolate, base::Vector<const uint8_t> data,
              Delegate* delegate)
      : isolate(i_isolate), deserializer(i_isolate, data, delegate) {}
  i::Isolate* isolate;
  i::ValueDeserializer deserializer;
  bool supports_legacy_wire_format = false;
};

ValueDeserializer::ValueDeserializer(Isolate* v8_isolate, const uint8_t* data,
                                     size_t size)
    : ValueDeserializer(v8_isolate, data, size, nullptr) {}

ValueDeserializer::ValueDeserializer(Isolate* v8_isolate, const uint8_t* data,
                                     size_t size, Delegate* delegate) {
  private_ = new PrivateData(reinterpret_cast<i::Isolate*>(v8_isolate),
                             base::Vector<const uint8_t>(data, size), delegate);
}

ValueDeserializer::~ValueDeserializer() { delete private_; }

Maybe<bool> ValueDeserializer::ReadHeader(Local<Context> context) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(i_isolate, context, ValueDeserializer, ReadHeader,
                     Nothing<bool>(), i::HandleScope);

  bool read_header = false;
  has_pending_exception = !private_->deserializer.ReadHeader().To(&read_header);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  DCHECK(read_header);

  static const uint32_t kMinimumNonLegacyVersion = 13;
  if (GetWireFormatVersion() < kMinimumNonLegacyVersion &&
      !private_->supports_legacy_wire_format) {
    i_isolate->Throw(*i_isolate->factory()->NewError(
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
  return private_->deserializer.GetWireFormatVersion();
}

MaybeLocal<Value> ValueDeserializer::ReadValue(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, ValueDeserializer, ReadValue, Value);
  i::MaybeHandle<i::Object> result;
  if (GetWireFormatVersion() > 0) {
    result = private_->deserializer.ReadObjectWrapper();
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
  private_->deserializer.TransferArrayBuffer(transfer_id,
                                             Utils::OpenHandle(*array_buffer));
}

void ValueDeserializer::TransferSharedArrayBuffer(
    uint32_t transfer_id, Local<SharedArrayBuffer> shared_array_buffer) {
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
  i::Object object = *Utils::OpenHandle(this);
  if (object.IsSmi()) return false;
  return object.IsTrue();
}

bool Value::IsFalse() const {
  i::Object object = *Utils::OpenHandle(this);
  if (object.IsSmi()) return false;
  return object.IsFalse();
}

bool Value::IsFunction() const { return Utils::OpenHandle(this)->IsCallable(); }

bool Value::IsName() const { return Utils::OpenHandle(this)->IsName(); }

bool Value::FullIsString() const {
  bool result = Utils::OpenHandle(this)->IsString();
  DCHECK_EQ(result, QuickIsString());
  return result;
}

bool Value::IsSymbol() const {
  return Utils::OpenHandle(this)->IsPublicSymbol();
}

bool Value::IsArray() const { return Utils::OpenHandle(this)->IsJSArray(); }

bool Value::IsArrayBuffer() const {
  i::Object obj = *Utils::OpenHandle(this);
  if (!obj.IsJSArrayBuffer()) return false;
  return !i::JSArrayBuffer::cast(obj).is_shared();
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
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  return obj->IsJSDataView() || obj->IsJSRabGsabDataView();
}

bool Value::IsSharedArrayBuffer() const {
  i::Object obj = *Utils::OpenHandle(this);
  if (!obj.IsJSArrayBuffer()) return false;
  return i::JSArrayBuffer::cast(obj).is_shared();
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
#if V8_ENABLE_WEBASSEMBLY
VALUE_IS_SPECIFIC_TYPE(WasmMemoryObject, WasmMemoryObject)
VALUE_IS_SPECIFIC_TYPE(WasmModuleObject, WasmModuleObject)
VALUE_IS_SPECIFIC_TYPE(WasmNull, WasmNull)
#else
bool Value::IsWasmMemoryObject() const { return false; }
bool Value::IsWasmModuleObject() const { return false; }
bool Value::IsWasmNull() const { return false; }
#endif  // V8_ENABLE_WEBASSEMBLY
VALUE_IS_SPECIFIC_TYPE(WeakMap, JSWeakMap)
VALUE_IS_SPECIFIC_TYPE(WeakSet, JSWeakSet)
VALUE_IS_SPECIFIC_TYPE(WeakRef, JSWeakRef)

#undef VALUE_IS_SPECIFIC_TYPE

bool Value::IsBoolean() const { return Utils::OpenHandle(this)->IsBoolean(); }

bool Value::IsExternal() const {
  i::Object obj = *Utils::OpenHandle(this);
  return obj.IsJSExternalObject();
}

bool Value::IsInt32() const {
  i::Object obj = *Utils::OpenHandle(this);
  if (obj.IsSmi()) return true;
  if (obj.IsNumber()) {
    return i::IsInt32Double(obj.Number());
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
  i::Object obj = *Utils::OpenHandle(this);
  if (!obj.IsJSFunction()) return false;
  i::JSFunction func = i::JSFunction::cast(obj);
  return i::IsAsyncFunction(func.shared().kind());
}

bool Value::IsGeneratorFunction() const {
  i::Object obj = *Utils::OpenHandle(this);
  if (!obj.IsJSFunction()) return false;
  i::JSFunction func = i::JSFunction::cast(obj);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(func.GetIsolate());
  return i::IsGeneratorFunction(func.shared().kind());
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
      !ToLocal<String>(i::Object::ToString(i_isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(result);
}

MaybeLocal<String> Value::ToDetailString(Local<Context> context) const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsString()) return ToApiHandle<String>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToDetailString, String);
  Local<String> result =
      Utils::ToLocal(i::Object::NoSideEffectsToString(i_isolate, obj));
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(result);
}

MaybeLocal<Object> Value::ToObject(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsJSReceiver()) return ToApiHandle<Object>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToObject, Object);
  Local<Object> result;
  has_pending_exception =
      !ToLocal<Object>(i::Object::ToObject(i_isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}

MaybeLocal<BigInt> Value::ToBigInt(Local<Context> context) const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  if (obj->IsBigInt()) return ToApiHandle<BigInt>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToBigInt, BigInt);
  Local<BigInt> result;
  has_pending_exception =
      !ToLocal<BigInt>(i::BigInt::FromObject(i_isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(BigInt);
  RETURN_ESCAPED(result);
}

bool Value::BooleanValue(Isolate* v8_isolate) const {
  return Utils::OpenHandle(this)->BooleanValue(
      reinterpret_cast<i::Isolate*>(v8_isolate));
}

Local<Boolean> Value::ToBoolean(Isolate* v8_isolate) const {
  auto i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return ToApiHandle<Boolean>(
      i_isolate->factory()->ToBoolean(BooleanValue(v8_isolate)));
}

MaybeLocal<Number> Value::ToNumber(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return ToApiHandle<Number>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToNumber, Number);
  Local<Number> result;
  has_pending_exception =
      !ToLocal<Number>(i::Object::ToNumber(i_isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Number);
  RETURN_ESCAPED(result);
}

MaybeLocal<Integer> Value::ToInteger(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return ToApiHandle<Integer>(obj);
  PREPARE_FOR_EXECUTION(context, Object, ToInteger, Integer);
  Local<Integer> result;
  has_pending_exception =
      !ToLocal<Integer>(i::Object::ToInteger(i_isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Integer);
  RETURN_ESCAPED(result);
}

MaybeLocal<Int32> Value::ToInt32(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return ToApiHandle<Int32>(obj);
  Local<Int32> result;
  PREPARE_FOR_EXECUTION(context, Object, ToInt32, Int32);
  has_pending_exception =
      !ToLocal<Int32>(i::Object::ToInt32(i_isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Int32);
  RETURN_ESCAPED(result);
}

MaybeLocal<Uint32> Value::ToUint32(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsSmi()) return ToApiHandle<Uint32>(obj);
  Local<Uint32> result;
  PREPARE_FOR_EXECUTION(context, Object, ToUint32, Uint32);
  has_pending_exception =
      !ToLocal<Uint32>(i::Object::ToUint32(i_isolate, obj), &result);
  RETURN_ON_FAILED_EXECUTION(Uint32);
  RETURN_ESCAPED(result);
}

i::Isolate* i::IsolateFromNeverReadOnlySpaceObject(i::Address obj) {
  return i::GetIsolateFromWritableObject(i::HeapObject::cast(i::Object(obj)));
}

bool i::ShouldThrowOnError(i::Isolate* i_isolate) {
  return i::GetShouldThrow(i_isolate, Nothing<i::ShouldThrow>()) ==
         i::ShouldThrow::kThrowOnError;
}

void i::Internals::CheckInitializedImpl(v8::Isolate* external_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  Utils::ApiCheck(i_isolate != nullptr && !i_isolate->IsDead(),
                  "v8::internal::Internals::CheckInitialized",
                  "Isolate is not initialized or V8 has died");
}

void v8::Value::CheckCast(Data* that) {
  Utils::ApiCheck(that->IsValue(), "v8::Value::Cast", "Data is not a Value");
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

void v8::Boolean::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsBoolean(), "v8::Boolean::Cast",
                  "Value is not a Boolean");
}

void v8::Name::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsName(), "v8::Name::Cast", "Value is not a Name");
}

void v8::String::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsString(), "v8::String::Cast", "Value is not a String");
}

void v8::Symbol::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsSymbol(), "v8::Symbol::Cast", "Value is not a Symbol");
}

void v8::Private::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(
      obj->IsSymbol() && i::Handle<i::Symbol>::cast(obj)->is_private(),
      "v8::Private::Cast", "Value is not a Private");
}

void v8::FixedArray::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsFixedArray(), "v8::FixedArray::Cast",
                  "Value is not a FixedArray");
}

void v8::ModuleRequest::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsModuleRequest(), "v8::ModuleRequest::Cast",
                  "Value is not a ModuleRequest");
}

void v8::Module::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsModule(), "v8::Module::Cast", "Value is not a Module");
}

void v8::Number::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsNumber(), "v8::Number::Cast()",
                  "Value is not a Number");
}

void v8::Integer::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsNumber(), "v8::Integer::Cast",
                  "Value is not an Integer");
}

void v8::Int32::CheckCast(v8::Data* that) {
  Utils::ApiCheck(Value::Cast(that)->IsInt32(), "v8::Int32::Cast",
                  "Value is not a 32-bit signed integer");
}

void v8::Uint32::CheckCast(v8::Data* that) {
  Utils::ApiCheck(Value::Cast(that)->IsUint32(), "v8::Uint32::Cast",
                  "Value is not a 32-bit unsigned integer");
}

void v8::BigInt::CheckCast(v8::Data* that) {
  Utils::ApiCheck(Value::Cast(that)->IsBigInt(), "v8::BigInt::Cast",
                  "Value is not a BigInt");
}

void v8::Context::CheckCast(v8::Data* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsContext(), "v8::Context::Cast",
                  "Value is not a Context");
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

void v8::WasmMemoryObject::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsWasmMemoryObject(), "v8::WasmMemoryObject::Cast",
                  "Value is not a WasmMemoryObject");
}

void v8::WasmModuleObject::CheckCast(Value* that) {
  Utils::ApiCheck(that->IsWasmModuleObject(), "v8::WasmModuleObject::Cast",
                  "Value is not a WasmModuleObject");
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

size_t v8::BackingStore::MaxByteLength() const {
  return reinterpret_cast<const i::BackingStore*>(this)->max_byte_length();
}

bool v8::BackingStore::IsShared() const {
  return reinterpret_cast<const i::BackingStore*>(this)->is_shared();
}

bool v8::BackingStore::IsResizableByUserJavaScript() const {
  return reinterpret_cast<const i::BackingStore*>(this)->is_resizable_by_js();
}

// static
std::unique_ptr<v8::BackingStore> v8::BackingStore::Reallocate(
    v8::Isolate* v8_isolate, std::unique_ptr<v8::BackingStore> backing_store,
    size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, ArrayBuffer, BackingStore_Reallocate);
  Utils::ApiCheck(byte_length <= i::JSArrayBuffer::kMaxByteLength,
                  "v8::BackingStore::Reallocate", "byte_lenght is too large");
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::BackingStore* i_backing_store =
      reinterpret_cast<i::BackingStore*>(backing_store.get());
  if (!i_backing_store->Reallocate(i_isolate, byte_length)) {
    i::V8::FatalProcessOutOfMemory(i_isolate, "v8::BackingStore::Reallocate");
  }
  return backing_store;
}

// static
void v8::BackingStore::EmptyDeleter(void* data, size_t length,
                                    void* deleter_data) {
  DCHECK_NULL(deleter_data);
}

std::shared_ptr<v8::BackingStore> v8::ArrayBuffer::GetBackingStore() {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  std::shared_ptr<i::BackingStore> backing_store = self->GetBackingStore();
  if (!backing_store) {
    backing_store =
        i::BackingStore::EmptyBackingStore(i::SharedFlag::kNotShared);
  }
  std::shared_ptr<i::BackingStoreBase> bs_base = backing_store;
  return std::static_pointer_cast<v8::BackingStore>(bs_base);
}

void* v8::ArrayBuffer::Data() const {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  return self->backing_store();
}

std::shared_ptr<v8::BackingStore> v8::SharedArrayBuffer::GetBackingStore() {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  std::shared_ptr<i::BackingStore> backing_store = self->GetBackingStore();
  if (!backing_store) {
    backing_store = i::BackingStore::EmptyBackingStore(i::SharedFlag::kShared);
  }
  std::shared_ptr<i::BackingStoreBase> bs_base = backing_store;
  return std::static_pointer_cast<v8::BackingStore>(bs_base);
}

void* v8::SharedArrayBuffer::Data() const {
  i::Handle<i::JSArrayBuffer> self = Utils::OpenHandle(this);
  return self->backing_store();
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
  Utils::ApiCheck(obj->IsJSDataView() || obj->IsJSRabGsabDataView(),
                  "v8::DataView::Cast()", "Value is not a DataView");
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Value, NumberValue, Nothing<double>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToNumber(i_isolate, obj).ToHandle(&num);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(double);
  return Just(num->Number());
}

Maybe<int64_t> Value::IntegerValue(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) {
    return Just(NumberToInt64(*obj));
  }
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Value, IntegerValue, Nothing<int64_t>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToInteger(i_isolate, obj).ToHandle(&num);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(int64_t);
  return Just(NumberToInt64(*num));
}

Maybe<int32_t> Value::Int32Value(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return Just(NumberToInt32(*obj));
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Value, Int32Value, Nothing<int32_t>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToInt32(i_isolate, obj).ToHandle(&num);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(int32_t);
  return Just(num->IsSmi() ? i::Smi::ToInt(*num)
                           : static_cast<int32_t>(num->Number()));
}

Maybe<uint32_t> Value::Uint32Value(Local<Context> context) const {
  auto obj = Utils::OpenHandle(this);
  if (obj->IsNumber()) return Just(NumberToUint32(*obj));
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Value, Uint32Value, Nothing<uint32_t>(),
           i::HandleScope);
  i::Handle<i::Object> num;
  has_pending_exception = !i::Object::ToUint32(i_isolate, obj).ToHandle(&num);
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
      !i::Object::ToString(i_isolate, self).ToHandle(&string_obj);
  RETURN_ON_FAILED_EXECUTION(Uint32);
  i::Handle<i::String> str = i::Handle<i::String>::cast(string_obj);
  uint32_t index;
  if (str->AsArrayIndex(&index)) {
    i::Handle<i::Object> value;
    if (index <= static_cast<uint32_t>(i::Smi::kMaxValue)) {
      value = i::Handle<i::Object>(i::Smi::FromInt(index), i_isolate);
    } else {
      value = i_isolate->factory()->NewNumber(index);
    }
    RETURN_ESCAPED(Utils::Uint32ToLocal(value));
  }
  return Local<Uint32>();
}

Maybe<bool> Value::Equals(Local<Context> context, Local<Value> that) const {
  i::Isolate* i_isolate = Utils::OpenHandle(*context)->GetIsolate();
  ENTER_V8(i_isolate, context, Value, Equals, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto other = Utils::OpenHandle(*that);
  Maybe<bool> result = i::Object::Equals(i_isolate, self, other);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  API_RCS_SCOPE(i_isolate, Value, TypeOf);
  return Utils::ToLocal(i::Object::TypeOf(i_isolate, Utils::OpenHandle(this)));
}

Maybe<bool> Value::InstanceOf(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> object) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Value, InstanceOf, Nothing<bool>(),
           i::HandleScope);
  auto left = Utils::OpenHandle(this);
  auto right = Utils::OpenHandle(*object);
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::Object::InstanceOf(i_isolate, left, right).ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(i_isolate));
}

Maybe<bool> v8::Object::Set(v8::Local<v8::Context> context,
                            v8::Local<Value> key, v8::Local<Value> value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, Set, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  auto value_obj = Utils::OpenHandle(*value);
  has_pending_exception =
      i::Runtime::SetObjectProperty(i_isolate, self, key_obj, value_obj,
                                    i::StoreOrigin::kMaybeKeyed,
                                    Just(i::ShouldThrow::kDontThrow))
          .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

Maybe<bool> v8::Object::Set(v8::Local<v8::Context> context, uint32_t index,
                            v8::Local<Value> value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, Set, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto value_obj = Utils::OpenHandle(*value);
  has_pending_exception =
      i::Object::SetElement(i_isolate, self, index, value_obj,
                            i::ShouldThrow::kDontThrow)
          .is_null();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

Maybe<bool> v8::Object::CreateDataProperty(v8::Local<v8::Context> context,
                                           v8::Local<Name> key,
                                           v8::Local<Value> value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);

  i::PropertyKey lookup_key(i_isolate, key_obj);
  i::LookupIterator it(i_isolate, self, lookup_key, i::LookupIterator::OWN);
  if (self->IsJSProxy()) {
    ENTER_V8(i_isolate, context, Object, CreateDataProperty, Nothing<bool>(),
             i::HandleScope);
    Maybe<bool> result =
        i::JSReceiver::CreateDataProperty(&it, value_obj, Just(i::kDontThrow));
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return result;
  } else {
    ENTER_V8_NO_SCRIPT(i_isolate, context, Object, CreateDataProperty,
                       Nothing<bool>(), i::HandleScope);
    Maybe<bool> result =
        i::JSObject::CreateDataProperty(&it, value_obj, Just(i::kDontThrow));
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return result;
  }
}

Maybe<bool> v8::Object::CreateDataProperty(v8::Local<v8::Context> context,
                                           uint32_t index,
                                           v8::Local<Value> value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);

  i::LookupIterator it(i_isolate, self, index, self, i::LookupIterator::OWN);
  if (self->IsJSProxy()) {
    ENTER_V8(i_isolate, context, Object, CreateDataProperty, Nothing<bool>(),
             i::HandleScope);
    Maybe<bool> result =
        i::JSReceiver::CreateDataProperty(&it, value_obj, Just(i::kDontThrow));
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return result;
  } else {
    ENTER_V8_NO_SCRIPT(i_isolate, context, Object, CreateDataProperty,
                       Nothing<bool>(), i::HandleScope);
    Maybe<bool> result =
        i::JSObject::CreateDataProperty(&it, value_obj, Just(i::kDontThrow));
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return result;
  }
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::Handle<i::Object> value_obj = Utils::OpenHandle(*value);

  i::PropertyDescriptor desc;
  desc.set_writable(!(attributes & v8::ReadOnly));
  desc.set_enumerable(!(attributes & v8::DontEnum));
  desc.set_configurable(!(attributes & v8::DontDelete));
  desc.set_value(value_obj);

  if (self->IsJSProxy()) {
    ENTER_V8(i_isolate, context, Object, DefineOwnProperty, Nothing<bool>(),
             i::HandleScope);
    Maybe<bool> success = i::JSReceiver::DefineOwnProperty(
        i_isolate, self, key_obj, &desc, Just(i::kDontThrow));
    // Even though we said kDontThrow, there might be accessors that do throw.
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return success;
  } else {
    // If it's not a JSProxy, i::JSReceiver::DefineOwnProperty should never run
    // a script.
    ENTER_V8_NO_SCRIPT(i_isolate, context, Object, DefineOwnProperty,
                       Nothing<bool>(), i::HandleScope);
    Maybe<bool> success = i::JSReceiver::DefineOwnProperty(
        i_isolate, self, key_obj, &desc, Just(i::kDontThrow));
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return success;
  }
}

Maybe<bool> v8::Object::DefineProperty(v8::Local<v8::Context> context,
                                       v8::Local<Name> key,
                                       PropertyDescriptor& descriptor) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, DefineOwnProperty, Nothing<bool>(),
           i::HandleScope);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);

  Maybe<bool> success = i::JSReceiver::DefineOwnProperty(
      i_isolate, self, key_obj, &descriptor.get_private()->desc,
      Just(i::kDontThrow));
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return success;
}

Maybe<bool> v8::Object::SetPrivate(Local<Context> context, Local<Private> key,
                                   Local<Value> value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(i_isolate, context, Object, SetPrivate, Nothing<bool>(),
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
        i_isolate, i::Handle<i::JSProxy>::cast(self),
        i::Handle<i::Symbol>::cast(key_obj), &desc, Just(i::kDontThrow));
  }
  auto js_object = i::Handle<i::JSObject>::cast(self);
  i::LookupIterator it(i_isolate, js_object, key_obj, js_object);
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
      !i::Runtime::GetObjectProperty(i_isolate, self, key_obj)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(Utils::ToLocal(result));
}

MaybeLocal<Value> v8::Object::Get(Local<Context> context, uint32_t index) {
  PREPARE_FOR_EXECUTION(context, Object, Get, Value);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  has_pending_exception =
      !i::JSReceiver::GetElement(i_isolate, self, index).ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(Utils::ToLocal(result));
}

MaybeLocal<Value> v8::Object::GetPrivate(Local<Context> context,
                                         Local<Private> key) {
  return Get(context, Local<Value>(reinterpret_cast<Value*>(*key)));
}

Maybe<PropertyAttribute> v8::Object::GetPropertyAttributes(
    Local<Context> context, Local<Value> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, GetPropertyAttributes,
           Nothing<PropertyAttribute>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  if (!key_obj->IsName()) {
    has_pending_exception =
        !i::Object::ToString(i_isolate, key_obj).ToHandle(&key_obj);
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
      i::JSReceiver::GetOwnPropertyDescriptor(i_isolate, obj, key_name, &desc);
  has_pending_exception = found.IsNothing();
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!found.FromJust()) {
    return v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate));
  }
  RETURN_ESCAPED(Utils::ToLocal(desc.ToObject(i_isolate)));
}

Local<Value> v8::Object::GetPrototype() {
  auto self = Utils::OpenHandle(this);
  auto i_isolate = self->GetIsolate();
  i::PrototypeIterator iter(i_isolate, self);
  return Utils::ToLocal(i::PrototypeIterator::GetCurrent(iter));
}

Maybe<bool> v8::Object::SetPrototype(Local<Context> context,
                                     Local<Value> value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  auto self = Utils::OpenHandle(this);
  auto value_obj = Utils::OpenHandle(*value);
  if (self->IsJSProxy()) {
    ENTER_V8(i_isolate, context, Object, SetPrototype, Nothing<bool>(),
             i::HandleScope);
    // We do not allow exceptions thrown while setting the prototype
    // to propagate outside.
    TryCatch try_catch(reinterpret_cast<v8::Isolate*>(i_isolate));
    auto result =
        i::JSProxy::SetPrototype(i_isolate, i::Handle<i::JSProxy>::cast(self),
                                 value_obj, false, i::kThrowOnError);
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  } else {
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
    auto result =
        i::JSObject::SetPrototype(i_isolate, i::Handle<i::JSObject>::cast(self),
                                  value_obj, false, i::kThrowOnError);
    if (result.IsNothing()) {
      i_isolate->clear_pending_exception();
      return Nothing<bool>();
    }
  }
  return Just(true);
}

Local<Object> v8::Object::FindInstanceInPrototypeChain(
    v8::Local<FunctionTemplate> tmpl) {
  auto self = Utils::OpenHandle(this);
  auto i_isolate = self->GetIsolate();
  i::PrototypeIterator iter(i_isolate, *self, i::kStartAtReceiver);
  auto tmpl_info = *Utils::OpenHandle(*tmpl);
  while (!tmpl_info.IsTemplateFor(iter.GetCurrent<i::JSObject>())) {
    iter.Advance();
    if (iter.IsAtEnd()) return Local<Object>();
    if (!iter.GetCurrent().IsJSObject()) return Local<Object>();
  }
  // IsTemplateFor() ensures that iter.GetCurrent() can't be a Proxy here.
  return Utils::ToLocal(i::handle(iter.GetCurrent<i::JSObject>(), i_isolate));
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
      i_isolate, static_cast<i::KeyCollectionMode>(mode),
      static_cast<i::PropertyFilter>(property_filter));
  accumulator.set_skip_indices(index_filter == IndexFilter::kSkipIndices);
  has_pending_exception = accumulator.CollectKeys(self, self).IsNothing();
  RETURN_ON_FAILED_EXECUTION(Array);
  value =
      accumulator.GetKeys(static_cast<i::GetKeysConversion>(key_conversion));
  DCHECK(self->map().EnumLength() == i::kInvalidEnumCacheSentinel ||
         self->map().EnumLength() == 0 ||
         self->map().instance_descriptors(i_isolate).enum_cache().keys() !=
             *value);
  auto result = i_isolate->factory()->NewJSArrayWithElements(value);
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
      i::Execution::CallBuiltin(i_isolate, i_isolate->object_to_string(), self,
                                0, nullptr),
      &result);
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(Local<String>::Cast(result));
}

Local<String> v8::Object::GetConstructorName() {
  // TODO(v8:12547): Consider adding GetConstructorName(Local<Context>).
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate;
  if (self->InWritableSharedSpace()) {
    i_isolate = i::Isolate::Current();
  } else {
    i_isolate = self->GetIsolate();
  }
  i::Handle<i::String> name =
      i::JSReceiver::GetConstructorName(i_isolate, self);
  return Utils::ToLocal(name);
}

Maybe<bool> v8::Object::SetIntegrityLevel(Local<Context> context,
                                          IntegrityLevel level) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, SetIntegrityLevel, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::JSReceiver::IntegrityLevel i_level =
      level == IntegrityLevel::kFrozen ? i::FROZEN : i::SEALED;
  Maybe<bool> result = i::JSReceiver::SetIntegrityLevel(
      i_isolate, self, i_level, i::kThrowOnError);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::Delete(Local<Context> context, Local<Value> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  if (self->IsJSProxy()) {
    ENTER_V8(i_isolate, context, Object, Delete, Nothing<bool>(),
             i::HandleScope);
    Maybe<bool> result = i::Runtime::DeleteObjectProperty(
        i_isolate, self, key_obj, i::LanguageMode::kSloppy);
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return result;
  } else {
    // If it's not a JSProxy, i::Runtime::DeleteObjectProperty should never run
    // a script.
    ENTER_V8_NO_SCRIPT(i_isolate, context, Object, Delete, Nothing<bool>(),
                       i::HandleScope);
    Maybe<bool> result = i::Runtime::DeleteObjectProperty(
        i_isolate, self, key_obj, i::LanguageMode::kSloppy);
    has_pending_exception = result.IsNothing();
    RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
    return result;
  }
}

Maybe<bool> v8::Object::DeletePrivate(Local<Context> context,
                                      Local<Private> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  // In case of private symbols, i::Runtime::DeleteObjectProperty does not run
  // any author script.
  ENTER_V8_NO_SCRIPT(i_isolate, context, Object, Delete, Nothing<bool>(),
                     i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  Maybe<bool> result = i::Runtime::DeleteObjectProperty(
      i_isolate, self, key_obj, i::LanguageMode::kSloppy);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::Has(Local<Context> context, Local<Value> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, Has, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_obj = Utils::OpenHandle(*key);
  Maybe<bool> maybe = Nothing<bool>();
  // Check if the given key is an array index.
  uint32_t index = 0;
  if (key_obj->ToArrayIndex(&index)) {
    maybe = i::JSReceiver::HasElement(i_isolate, self, index);
  } else {
    // Convert the key to a name - possibly by calling back into JavaScript.
    i::Handle<i::Name> name;
    if (i::Object::ToName(i_isolate, key_obj).ToHandle(&name)) {
      maybe = i::JSReceiver::HasProperty(i_isolate, self, name);
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, Delete, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  Maybe<bool> result = i::JSReceiver::DeleteElement(self, index);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::Has(Local<Context> context, uint32_t index) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, Has, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto maybe = i::JSReceiver::HasElement(i_isolate, self, index);
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(i_isolate, context, Object, SetAccessor, Nothing<bool>(),
                     i::HandleScope);
  if (!Utils::OpenHandle(self)->IsJSObject()) return Just(false);
  i::Handle<i::JSObject> obj =
      i::Handle<i::JSObject>::cast(Utils::OpenHandle(self));
  i::Handle<i::AccessorInfo> info =
      MakeAccessorInfo(i_isolate, name, getter, setter, data, settings,
                       is_special_data_property, replace_on_access);
  info->set_getter_side_effect_type(getter_side_effect_type);
  info->set_setter_side_effect_type(setter_side_effect_type);
  if (info.is_null()) return Nothing<bool>();
  bool fast = obj->HasFastProperties();
  i::Handle<i::Object> result;

  i::Handle<i::Name> accessor_name(info->name(), i_isolate);
  i::PropertyAttributes attrs = static_cast<i::PropertyAttributes>(attributes);
  has_pending_exception =
      !i::JSObject::SetAccessor(obj, accessor_name, info, attrs)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  if (result->IsUndefined(i_isolate)) return Just(false);
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
                           i::v8_flags.disable_old_api_accessors, false,
                           getter_side_effect_type, setter_side_effect_type);
}

void Object::SetAccessorProperty(Local<Name> name, Local<Function> getter,
                                 Local<Function> setter,
                                 PropertyAttribute attribute,
                                 AccessControl settings) {
  // TODO(verwaest): Remove |settings|.
  DCHECK_EQ(v8::DEFAULT, settings);
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  if (!self->IsJSObject()) return;
  i::Handle<i::Object> getter_i = v8::Utils::OpenHandle(*getter);
  i::Handle<i::Object> setter_i = v8::Utils::OpenHandle(*setter, true);
  if (setter_i.is_null()) setter_i = i_isolate->factory()->null_value();
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, HasOwnProperty, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto key_val = Utils::OpenHandle(*key);
  auto result = i::JSReceiver::HasOwnProperty(i_isolate, self, key_val);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::HasOwnProperty(Local<Context> context, uint32_t index) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, HasOwnProperty, Nothing<bool>(),
           i::HandleScope);
  auto self = Utils::OpenHandle(this);
  auto result = i::JSReceiver::HasOwnProperty(i_isolate, self, index);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::HasRealNamedProperty(Local<Context> context,
                                             Local<Name> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(i_isolate, context, Object, HasRealNamedProperty,
                     Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return Just(false);
  auto key_val = Utils::OpenHandle(*key);
  auto result = i::JSObject::HasRealNamedProperty(
      i_isolate, i::Handle<i::JSObject>::cast(self), key_val);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::HasRealIndexedProperty(Local<Context> context,
                                               uint32_t index) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(i_isolate, context, Object, HasRealIndexedProperty,
                     Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return Just(false);
  auto result = i::JSObject::HasRealElementProperty(
      i_isolate, i::Handle<i::JSObject>::cast(self), index);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

Maybe<bool> v8::Object::HasRealNamedCallbackProperty(Local<Context> context,
                                                     Local<Name> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(i_isolate, context, Object, HasRealNamedCallbackProperty,
                     Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return Just(false);
  auto key_val = Utils::OpenHandle(*key);
  auto result = i::JSObject::HasRealNamedCallbackProperty(
      i_isolate, i::Handle<i::JSObject>::cast(self), key_val);
  has_pending_exception = result.IsNothing();
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return result;
}

bool v8::Object::HasNamedLookupInterceptor() const {
  auto self = *Utils::OpenHandle(this);
  if (self.IsJSObject()) return false;
  return i::JSObject::cast(self).HasNamedInterceptor();
}

bool v8::Object::HasIndexedLookupInterceptor() const {
  auto self = *Utils::OpenHandle(this);
  if (self.IsJSObject()) return false;
  return i::JSObject::cast(self).HasIndexedInterceptor();
}

MaybeLocal<Value> v8::Object::GetRealNamedPropertyInPrototypeChain(
    Local<Context> context, Local<Name> key) {
  PREPARE_FOR_EXECUTION(context, Object, GetRealNamedPropertyInPrototypeChain,
                        Value);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return MaybeLocal<Value>();
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::PrototypeIterator iter(i_isolate, self);
  if (iter.IsAtEnd()) return MaybeLocal<Value>();
  i::Handle<i::JSReceiver> proto =
      i::PrototypeIterator::GetCurrent<i::JSReceiver>(iter);
  i::PropertyKey lookup_key(i_isolate, key_obj);
  i::LookupIterator it(i_isolate, self, lookup_key, proto,
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object,
           GetRealNamedPropertyAttributesInPrototypeChain,
           Nothing<PropertyAttribute>(), i::HandleScope);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  if (!self->IsJSObject()) return Nothing<PropertyAttribute>();
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::PrototypeIterator iter(i_isolate, self);
  if (iter.IsAtEnd()) return Nothing<PropertyAttribute>();
  i::Handle<i::JSReceiver> proto =
      i::PrototypeIterator::GetCurrent<i::JSReceiver>(iter);
  i::PropertyKey lookup_key(i_isolate, key_obj);
  i::LookupIterator it(i_isolate, self, lookup_key, proto,
                       i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Maybe<i::PropertyAttributes> result =
      i::JSReceiver::GetPropertyAttributes(&it);
  has_pending_exception = result.IsNothing();
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
  i::PropertyKey lookup_key(i_isolate, key_obj);
  i::LookupIterator it(i_isolate, self, lookup_key, self,
                       i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(i::Object::GetProperty(&it), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  if (!it.IsFound()) return MaybeLocal<Value>();
  RETURN_ESCAPED(result);
}

Maybe<PropertyAttribute> v8::Object::GetRealNamedPropertyAttributes(
    Local<Context> context, Local<Name> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Object, GetRealNamedPropertyAttributes,
           Nothing<PropertyAttribute>(), i::HandleScope);
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  i::Handle<i::Name> key_obj = Utils::OpenHandle(*key);
  i::PropertyKey lookup_key(i_isolate, key_obj);
  i::LookupIterator it(i_isolate, self, lookup_key, self,
                       i::LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  auto result = i::JSReceiver::GetPropertyAttributes(&it);
  has_pending_exception = result.IsNothing();
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
  auto i_isolate = self->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSObject> result = i_isolate->factory()->CopyJSObject(self);
  return Utils::ToLocal(result);
}

MaybeLocal<v8::Context> v8::Object::GetCreationContext() {
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Context> context;
  if (self->GetCreationContext().ToHandle(&context)) {
    return Utils::ToLocal(context);
  }
  return MaybeLocal<v8::Context>();
}

void* v8::Object::GetAlignedPointerFromEmbedderDataInCreationContext(
    int index) {
  const char* location =
      "v8::Object::GetAlignedPointerFromEmbedderDataInCreationContext()";
  auto self = Utils::OpenHandle(this);
  auto maybe_context = self->GetCreationContextRaw();
  if (!maybe_context.has_value()) return nullptr;

  // The code below mostly mimics Context::GetAlignedPointerFromEmbedderData()
  // but it doesn't try to expand the EmbedderDataArray instance.
  i::DisallowGarbageCollection no_gc;
  i::NativeContext native_context =
      i::NativeContext::cast(maybe_context.value());
  i::Isolate* i_isolate = native_context.GetIsolate();

  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  // TODO(ishell): remove cast once embedder_data slot has a proper type.
  i::EmbedderDataArray data =
      i::EmbedderDataArray::cast(native_context.embedder_data());
  if (V8_LIKELY(static_cast<unsigned>(index) <
                static_cast<unsigned>(data.length()))) {
    void* result;
    Utils::ApiCheck(
        i::EmbedderDataSlot(data, index).ToAlignedPointer(i_isolate, &result),
        location, "Pointer is not aligned");
    return result;
  }
  // Bad index, report an API error.
  Utils::ApiCheck(index >= 0, location, "Negative index");
  Utils::ApiCheck(index < i::EmbedderDataArray::kMaxLength, location,
                  "Index too large");
  return nullptr;
}

Local<v8::Context> v8::Object::GetCreationContextChecked() {
  Local<Context> context;
  Utils::ApiCheck(GetCreationContext().ToLocal(&context),
                  "v8::Object::GetCreationContextChecked",
                  "No creation context available");
  return context;
}

int v8::Object::GetIdentityHash() {
  i::DisallowGarbageCollection no_gc;
  auto self = Utils::OpenHandle(this);
  auto i_isolate = self->GetIsolate();
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  return self->GetOrCreateIdentityHash(i_isolate).value();
}

bool v8::Object::IsCallable() const {
  auto self = Utils::OpenHandle(this);
  return self->IsCallable();
}

bool v8::Object::IsConstructor() const {
  auto self = Utils::OpenHandle(this);
  return self->IsConstructor();
}

bool v8::Object::IsApiWrapper() const {
  auto self = i::Handle<i::JSObject>::cast(Utils::OpenHandle(this));
  // Objects with embedder fields can wrap API objects.
  return self->MayHaveEmbedderFields();
}

bool v8::Object::IsUndetectable() const {
  auto self = i::Handle<i::JSObject>::cast(Utils::OpenHandle(this));
  return self->IsUndetectable();
}

MaybeLocal<Value> Object::CallAsFunction(Local<Context> context,
                                         Local<Value> recv, int argc,
                                         Local<Value> argv[]) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.Execute");
  ENTER_V8(i_isolate, context, Object, CallAsFunction, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(i_isolate);
  i::NestedTimedHistogramScope execute_timer(i_isolate->counters()->execute(),
                                             i_isolate);
  auto self = Utils::OpenHandle(this);
  auto recv_obj = Utils::OpenHandle(*recv);
  static_assert(sizeof(v8::Local<v8::Value>) == sizeof(i::Handle<i::Object>));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::Call(i_isolate, self, recv_obj, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<Value> Object::CallAsConstructor(Local<Context> context, int argc,
                                            Local<Value> argv[]) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.Execute");
  ENTER_V8(i_isolate, context, Object, CallAsConstructor, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(i_isolate);
  i::NestedTimedHistogramScope execute_timer(i_isolate->counters()->execute(),
                                             i_isolate);
  auto self = Utils::OpenHandle(this);
  static_assert(sizeof(v8::Local<v8::Value>) == sizeof(i::Handle<i::Object>));
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::New(i_isolate, self, self, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

MaybeLocal<Function> Function::New(Local<Context> context,
                                   FunctionCallback callback, Local<Value> data,
                                   int length, ConstructorBehavior behavior,
                                   SideEffectType side_effect_type) {
  i::Isolate* i_isolate = Utils::OpenHandle(*context)->GetIsolate();
  API_RCS_SCOPE(i_isolate, Function, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  auto templ =
      FunctionTemplateNew(i_isolate, callback, data, Local<Signature>(), length,
                          behavior, true, Local<Private>(), side_effect_type);
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.Execute");
  ENTER_V8(i_isolate, context, Function, NewInstance, MaybeLocal<Object>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(i_isolate);
  i::NestedTimedHistogramScope execute_timer(i_isolate->counters()->execute(),
                                             i_isolate);
  auto self = Utils::OpenHandle(this);
  static_assert(sizeof(v8::Local<v8::Value>) == sizeof(i::Handle<i::Object>));
  bool should_set_has_no_side_effect =
      side_effect_type == SideEffectType::kHasNoSideEffect &&
      i_isolate->debug_execution_mode() == i::DebugInfo::kSideEffects;
  if (should_set_has_no_side_effect) {
    CHECK(self->IsJSFunction() &&
          i::JSFunction::cast(*self).shared().IsApiFunction());
    i::Object obj =
        i::JSFunction::cast(*self).shared().get_api_func_data().call_code(
            kAcquireLoad);
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
      i::Execution::New(i_isolate, self, self, argc, args), &result);
  if (should_set_has_no_side_effect) {
    i::Object obj =
        i::JSFunction::cast(*self).shared().get_api_func_data().call_code(
            kAcquireLoad);
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.Execute");
  ENTER_V8(i_isolate, context, Function, Call, MaybeLocal<Value>(),
           InternalEscapableScope);
  i::TimerEventScope<i::TimerEventExecute> timer_scope(i_isolate);
  i::NestedTimedHistogramScope execute_timer(i_isolate->counters()->execute(),
                                             i_isolate);
  auto self = Utils::OpenHandle(this);
  Utils::ApiCheck(!self.is_null(), "v8::Function::Call",
                  "Function to be called is a null pointer");
  i::Handle<i::Object> recv_obj = Utils::OpenHandle(*recv);
  static_assert(sizeof(v8::Local<v8::Value>) == sizeof(i::Handle<i::Object>));
#if V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  i::Handle<i::Object>* args = new i::Handle<i::Object>[argc];
  for (int i = 0; i < argc; ++i) {
    args[i] = Utils::OpenHandle(*argv[i]);
  }
#else
  i::Handle<i::Object>* args = reinterpret_cast<i::Handle<i::Object>*>(argv);
#endif
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::Call(i_isolate, self, recv_obj, argc, args), &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

void Function::SetName(v8::Local<v8::String> name) {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) return;
  auto func = i::Handle<i::JSFunction>::cast(self);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(func->GetIsolate());
  func->shared().SetName(*Utils::OpenHandle(*name));
}

Local<Value> Function::GetName() const {
  auto self = Utils::OpenHandle(this);
  i::Isolate* i_isolate = self->GetIsolate();
  if (self->IsJSBoundFunction()) {
    auto func = i::Handle<i::JSBoundFunction>::cast(self);
    i::Handle<i::Object> name;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        i_isolate, name, i::JSBoundFunction::GetName(i_isolate, func),
        Local<Value>());
    return Utils::ToLocal(name);
  }
  if (self->IsJSFunction()) {
    auto func = i::Handle<i::JSFunction>::cast(self);
    return Utils::ToLocal(handle(func->shared().Name(), i_isolate));
  }
  return ToApiHandle<Primitive>(i_isolate->factory()->undefined_value());
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

ScriptOrigin Function::GetScriptOrigin() const {
  auto self = Utils::OpenHandle(this);
  auto i_isolate = reinterpret_cast<v8::Isolate*>(self->GetIsolate());
  if (!self->IsJSFunction()) return v8::ScriptOrigin(i_isolate, Local<Value>());
  auto func = i::Handle<i::JSFunction>::cast(self);
  if (func->shared().script().IsScript()) {
    i::Handle<i::Script> script(i::Script::cast(func->shared().script()),
                                func->GetIsolate());
    return GetScriptOriginForScript(func->GetIsolate(), script);
  }
  return v8::ScriptOrigin(i_isolate, Local<Value>());
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

MaybeLocal<UnboundScript> Function::GetUnboundScript() const {
  i::Handle<i::JSReceiver> self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) return MaybeLocal<UnboundScript>();
  i::SharedFunctionInfo sfi = i::JSFunction::cast(*self).shared();
  i::Isolate* i_isolate = self->GetIsolate();
  return ToApiHandle<UnboundScript>(i::handle(sfi, i_isolate));
}

int Function::ScriptId() const {
  i::JSReceiver self = *Utils::OpenHandle(this);
  if (!self.IsJSFunction()) return v8::UnboundScript::kNoScriptId;
  auto func = i::JSFunction::cast(self);
  if (!func.shared().script().IsScript()) return v8::UnboundScript::kNoScriptId;
  return i::Script::cast(func.shared().script()).id();
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

bool Function::Experimental_IsNopFunction() const {
  auto self = Utils::OpenHandle(this);
  if (!self->IsJSFunction()) return false;
  i::SharedFunctionInfo sfi = i::JSFunction::cast(*self).shared();
  i::Isolate* i_isolate = self->GetIsolate();
  i::IsCompiledScope is_compiled_scope(sfi.is_compiled_scope(i_isolate));
  if (!is_compiled_scope.is_compiled() &&
      !i::Compiler::Compile(i_isolate, i::handle(sfi, i_isolate),
                            i::Compiler::CLEAR_EXCEPTION, &is_compiled_scope)) {
    return false;
  }
  DCHECK(is_compiled_scope.is_compiled());
  // Since |sfi| can be GC'ed, we get it again.
  sfi = i::JSFunction::cast(*self).shared();
  if (!sfi.HasBytecodeArray()) return false;
  i::Handle<i::BytecodeArray> bytecode_array(sfi.GetBytecodeArray(i_isolate),
                                             i_isolate);
  i::interpreter::BytecodeArrayIterator it(bytecode_array, 0);
  if (it.current_bytecode() != i::interpreter::Bytecode::kLdaUndefined) {
    return false;
  }
  it.Advance();
  DCHECK(!it.done());
  if (it.current_bytecode() != i::interpreter::Bytecode::kReturn) return false;
  it.Advance();
  DCHECK(it.done());
  return true;
}

MaybeLocal<String> v8::Function::FunctionProtoToString(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, Function, FunctionProtoToString, String);
  auto self = Utils::OpenHandle(this);
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::Execution::CallBuiltin(i_isolate, i_isolate->function_to_string(),
                                self, 0, nullptr),
      &result);
  RETURN_ON_FAILED_EXECUTION(String);
  RETURN_ESCAPED(Local<String>::Cast(result));
}

int Name::GetIdentityHash() {
  auto self = Utils::OpenHandle(this);
  return static_cast<int>(self->EnsureHash());
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
  static const uint64_t value = 0xFF00'FF00'FF00'FF00;
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
  ContainsOnlyOneByteHelper(const ContainsOnlyOneByteHelper&) = delete;
  ContainsOnlyOneByteHelper& operator=(const ContainsOnlyOneByteHelper&) =
      delete;
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
};

bool String::ContainsOnlyOneByte() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (str->IsOneByteRepresentation()) return true;
  ContainsOnlyOneByteHelper helper;
  return helper.Check(*str);
}

int String::Utf8Length(Isolate* v8_isolate) const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  str = i::String::Flatten(reinterpret_cast<i::Isolate*>(v8_isolate), str);
  int length = str->length();
  if (length == 0) return 0;
  i::DisallowGarbageCollection no_gc;
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
static int WriteUtf8Impl(base::Vector<const Char> string, char* write_start,
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
  static_assert(unibrow::Utf16::kMaxExtraUtf8BytesForOneUtf16CodeUnit == 3);
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, String, WriteUtf8);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  str = i::String::Flatten(i_isolate, str);
  i::DisallowGarbageCollection no_gc;
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
static inline int WriteHelper(i::Isolate* i_isolate, const String* string,
                              CharType* buffer, int start, int length,
                              int options) {
  API_RCS_SCOPE(i_isolate, String, Write);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  DCHECK(start >= 0 && length >= -1);
  i::Handle<i::String> str = Utils::OpenHandle(string);
  str = i::String::Flatten(i_isolate, str);
  int end = start + length;
  if ((length == -1) || (length > str->length() - start)) end = str->length();
  if (end < 0) return 0;
  int write_length = end - start;
  if (start < end) i::String::WriteToFlat(*str, buffer, start, write_length);
  if (!(options & String::NO_NULL_TERMINATION) &&
      (length == -1 || write_length < length)) {
    buffer[write_length] = '\0';
  }
  return write_length;
}

int String::WriteOneByte(Isolate* v8_isolate, uint8_t* buffer, int start,
                         int length, int options) const {
  return WriteHelper(reinterpret_cast<i::Isolate*>(v8_isolate), this, buffer,
                     start, length, options);
}

int String::Write(Isolate* v8_isolate, uint16_t* buffer, int start, int length,
                  int options) const {
  return WriteHelper(reinterpret_cast<i::Isolate*>(v8_isolate), this, buffer,
                     start, length, options);
}

namespace {

bool HasExternalStringResource(i::String string) {
  return i::StringShape(string).IsExternal() ||
         string.HasExternalForwardingIndex(kAcquireLoad);
}

v8::String::ExternalStringResourceBase* GetExternalResourceFromForwardingTable(
    i::String string, uint32_t raw_hash, bool* is_one_byte) {
  DCHECK(i::String::IsExternalForwardingIndex(raw_hash));
  const int index = i::String::ForwardingIndexValueBits::decode(raw_hash);
  i::Isolate* isolate = i::GetIsolateFromWritableObject(string);
  auto resource = isolate->string_forwarding_table()->GetExternalResource(
      index, is_one_byte);
  DCHECK_NOT_NULL(resource);
  return resource;
}

}  // namespace

bool v8::String::IsExternal() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  return HasExternalStringResource(*str);
}

bool v8::String::IsExternalTwoByte() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (i::StringShape(*str).IsExternalTwoByte()) return true;
  uint32_t raw_hash_field = str->raw_hash_field(kAcquireLoad);
  if (i::String::IsExternalForwardingIndex(raw_hash_field)) {
    bool is_one_byte;
    GetExternalResourceFromForwardingTable(*str, raw_hash_field, &is_one_byte);
    return !is_one_byte;
  }
  return false;
}

bool v8::String::IsExternalOneByte() const {
  i::Handle<i::String> str = Utils::OpenHandle(this);
  if (i::StringShape(*str).IsExternalOneByte()) return true;
  uint32_t raw_hash_field = str->raw_hash_field(kAcquireLoad);
  if (i::String::IsExternalForwardingIndex(raw_hash_field)) {
    bool is_one_byte;
    GetExternalResourceFromForwardingTable(*str, raw_hash_field, &is_one_byte);
    return is_one_byte;
  }
  return false;
}

void v8::String::VerifyExternalStringResource(
    v8::String::ExternalStringResource* value) const {
  i::DisallowGarbageCollection no_gc;
  i::String str = *Utils::OpenHandle(this);
  const v8::String::ExternalStringResource* expected;

  if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
  }

  if (i::StringShape(str).IsExternalTwoByte()) {
    const void* resource = i::ExternalTwoByteString::cast(str).resource();
    expected = reinterpret_cast<const ExternalStringResource*>(resource);
  } else {
    uint32_t raw_hash_field = str.raw_hash_field(kAcquireLoad);
    if (i::String::IsExternalForwardingIndex(raw_hash_field)) {
      bool is_one_byte;
      auto resource = GetExternalResourceFromForwardingTable(
          str, raw_hash_field, &is_one_byte);
      if (!is_one_byte) {
        expected = reinterpret_cast<const ExternalStringResource*>(resource);
      }
    } else {
      expected = nullptr;
    }
  }
  CHECK_EQ(expected, value);
}

void v8::String::VerifyExternalStringResourceBase(
    v8::String::ExternalStringResourceBase* value, Encoding encoding) const {
  i::DisallowGarbageCollection no_gc;
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
    uint32_t raw_hash_field = str.raw_hash_field(kAcquireLoad);
    if (i::String::IsExternalForwardingIndex(raw_hash_field)) {
      bool is_one_byte;
      expected = GetExternalResourceFromForwardingTable(str, raw_hash_field,
                                                        &is_one_byte);
      expectedEncoding = is_one_byte ? ONE_BYTE_ENCODING : TWO_BYTE_ENCODING;
    } else {
      expected = nullptr;
      expectedEncoding =
          str.IsOneByteRepresentation() ? ONE_BYTE_ENCODING : TWO_BYTE_ENCODING;
    }
  }
  CHECK_EQ(expected, value);
  CHECK_EQ(expectedEncoding, encoding);
}

String::ExternalStringResource* String::GetExternalStringResourceSlow() const {
  i::DisallowGarbageCollection no_gc;
  i::String str = *Utils::OpenHandle(this);

  if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
  }

  if (i::StringShape(str).IsExternalTwoByte()) {
    Isolate* isolate = i::Internals::GetIsolateForSandbox(str.ptr());
    i::Address value =
        i::Internals::ReadExternalPointerField<i::kExternalStringResourceTag>(
            isolate, str.ptr(), i::Internals::kStringResourceOffset);
    return reinterpret_cast<String::ExternalStringResource*>(value);
  } else {
    uint32_t raw_hash_field = str.raw_hash_field(kAcquireLoad);
    if (i::String::IsExternalForwardingIndex(raw_hash_field)) {
      bool is_one_byte;
      auto resource = GetExternalResourceFromForwardingTable(
          str, raw_hash_field, &is_one_byte);
      if (!is_one_byte) {
        return reinterpret_cast<ExternalStringResource*>(resource);
      }
    }
  }
  return nullptr;
}

void String::ExternalStringResource::UpdateDataCache() {
  DCHECK(IsCacheable());
  cached_data_ = data();
}

void String::ExternalStringResource::CheckCachedDataInvariants() const {
  DCHECK(IsCacheable() && cached_data_ != nullptr);
}

void String::ExternalOneByteStringResource::UpdateDataCache() {
  DCHECK(IsCacheable());
  cached_data_ = data();
}

void String::ExternalOneByteStringResource::CheckCachedDataInvariants() const {
  DCHECK(IsCacheable() && cached_data_ != nullptr);
}

String::ExternalStringResourceBase* String::GetExternalStringResourceBaseSlow(
    String::Encoding* encoding_out) const {
  i::DisallowGarbageCollection no_gc;
  ExternalStringResourceBase* resource = nullptr;
  i::String str = *Utils::OpenHandle(this);

  if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
  }

  internal::Address string = str.ptr();
  int type = i::Internals::GetInstanceType(string) &
             i::Internals::kStringRepresentationAndEncodingMask;
  *encoding_out =
      static_cast<Encoding>(type & i::Internals::kStringEncodingMask);
  if (i::StringShape(str).IsExternalOneByte() ||
      i::StringShape(str).IsExternalTwoByte()) {
    Isolate* isolate = i::Internals::GetIsolateForSandbox(string);
    i::Address value =
        i::Internals::ReadExternalPointerField<i::kExternalStringResourceTag>(
            isolate, string, i::Internals::kStringResourceOffset);
    resource = reinterpret_cast<ExternalStringResourceBase*>(value);
  } else {
    uint32_t raw_hash_field = str.raw_hash_field();
    if (i::String::IsExternalForwardingIndex(raw_hash_field)) {
      bool is_one_byte;
      resource = GetExternalResourceFromForwardingTable(str, raw_hash_field,
                                                        &is_one_byte);
      *encoding_out = is_one_byte ? Encoding::ONE_BYTE_ENCODING
                                  : Encoding::TWO_BYTE_ENCODING;
    }
  }
  return resource;
}

const v8::String::ExternalOneByteStringResource*
v8::String::GetExternalOneByteStringResource() const {
  i::DisallowGarbageCollection no_gc;
  i::String str = *Utils::OpenHandle(this);
  if (i::StringShape(str).IsExternalOneByte()) {
    return i::ExternalOneByteString::cast(str).resource();
  } else if (str.IsThinString()) {
    str = i::ThinString::cast(str).actual();
    if (i::StringShape(str).IsExternalOneByte()) {
      return i::ExternalOneByteString::cast(str).resource();
    }
  }
  uint32_t raw_hash_field = str.raw_hash_field(kAcquireLoad);
  if (i::String::IsExternalForwardingIndex(raw_hash_field)) {
    bool is_one_byte;
    auto resource = GetExternalResourceFromForwardingTable(str, raw_hash_field,
                                                           &is_one_byte);
    if (is_one_byte) {
      return reinterpret_cast<ExternalOneByteStringResource*>(resource);
    }
  }
  return nullptr;
}

Local<Value> Symbol::Description(Isolate* v8_isolate) const {
  i::Handle<i::Symbol> sym = Utils::OpenHandle(this);
  i::Handle<i::Object> description(sym->description(),
                                   reinterpret_cast<i::Isolate*>(v8_isolate));
  return Utils::ToLocal(description);
}

Local<Value> Private::Name() const {
  const Symbol* sym = reinterpret_cast<const Symbol*>(this);
  i::Handle<i::Symbol> i_sym = Utils::OpenHandle(sym);
  // v8::Private symbols are created by API and are therefore writable, so we
  // can always recover an Isolate.
  i::Isolate* i_isolate = i::GetIsolateFromWritableObject(*i_sym);
  return sym->Description(reinterpret_cast<Isolate*>(i_isolate));
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
  i::Object obj = *Utils::OpenHandle(this);
  if (obj.IsSmi()) {
    return i::Smi::ToInt(obj);
  } else {
    return static_cast<int64_t>(obj.Number());
  }
}

int32_t Int32::Value() const {
  i::Object obj = *Utils::OpenHandle(this);
  if (obj.IsSmi()) {
    return i::Smi::ToInt(obj);
  } else {
    return static_cast<int32_t>(obj.Number());
  }
}

uint32_t Uint32::Value() const {
  i::Object obj = *Utils::OpenHandle(this);
  if (obj.IsSmi()) {
    return i::Smi::ToInt(obj);
  } else {
    return static_cast<uint32_t>(obj.Number());
  }
}

int v8::Object::InternalFieldCount() const {
  i::JSReceiver self = *Utils::OpenHandle(this);
  if (!self.IsJSObject()) return 0;
  return i::JSObject::cast(self).GetEmbedderFieldCount();
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

template<typename T>
void SetInternalFieldImpl(v8::Object* receiver, int index, v8::Local<T> value) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(receiver);
  const char* location = "v8::Object::SetInternalField()";
  if (!InternalFieldOK(obj, index, location)) return;
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  i::Handle<i::JSObject>::cast(obj)->SetEmbedderField(index, *val);
}

void v8::Object::SetInternalField(int index, v8::Local<Value> value) {
  SetInternalFieldImpl(this, index, value);
}

/**
 * These are Node.js-specific extentions used to avoid breaking changes in
 * Node.js v20.x.
 */
void v8::Object::SetInternalFieldForNodeCore(int index,
                                             v8::Local<Module> value) {
  SetInternalFieldImpl(this, index, value);
}

void v8::Object::SetInternalFieldForNodeCore(int index,
                                             v8::Local<UnboundScript> value) {
  SetInternalFieldImpl(this, index, value);
}

void* v8::Object::SlowGetAlignedPointerFromInternalField(int index) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::GetAlignedPointerFromInternalField()";
  if (!InternalFieldOK(obj, index, location)) return nullptr;
  void* result;
  Utils::ApiCheck(i::EmbedderDataSlot(i::JSObject::cast(*obj), index)
                      .ToAlignedPointer(obj->GetIsolate(), &result),
                  location, "Unaligned pointer");
  return result;
}

void v8::Object::SetAlignedPointerInInternalField(int index, void* value) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);
  const char* location = "v8::Object::SetAlignedPointerInInternalField()";
  if (!InternalFieldOK(obj, index, location)) return;

  i::DisallowGarbageCollection no_gc;
  Utils::ApiCheck(i::EmbedderDataSlot(i::JSObject::cast(*obj), index)
                      .store_aligned_pointer(obj->GetIsolate(), value),
                  location, "Unaligned pointer");
  DCHECK_EQ(value, GetAlignedPointerFromInternalField(index));
  internal::WriteBarrier::CombinedBarrierFromInternalFields(
      i::JSObject::cast(*obj), value);
}

void v8::Object::SetAlignedPointerInInternalFields(int argc, int indices[],
                                                   void* values[]) {
  i::Handle<i::JSReceiver> obj = Utils::OpenHandle(this);

  i::DisallowGarbageCollection no_gc;
  const char* location = "v8::Object::SetAlignedPointerInInternalFields()";
  i::JSObject js_obj = i::JSObject::cast(*obj);
  int nof_embedder_fields = js_obj.GetEmbedderFieldCount();
  for (int i = 0; i < argc; i++) {
    int index = indices[i];
    if (!Utils::ApiCheck(index < nof_embedder_fields, location,
                         "Internal field out of bounds")) {
      return;
    }
    void* value = values[i];
    Utils::ApiCheck(i::EmbedderDataSlot(js_obj, index)
                        .store_aligned_pointer(obj->GetIsolate(), value),
                    location, "Unaligned pointer");
    DCHECK_EQ(value, GetAlignedPointerFromInternalField(index));
  }
  internal::WriteBarrier::CombinedBarrierFromInternalFields(js_obj, argc,
                                                            values);
}

// --- E n v i r o n m e n t ---

void v8::V8::InitializePlatform(Platform* platform) {
  i::V8::InitializePlatform(platform);
}

void v8::V8::DisposePlatform() { i::V8::DisposePlatform(); }

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

  const bool kEmbedderSandbox = (build_config & kSandbox) != 0;
  if (kEmbedderSandbox != V8_ENABLE_SANDBOX_BOOL) {
    FATAL(
        "Embedder-vs-V8 build configuration mismatch. On embedder side "
        "sandbox is %s while on V8 side it's %s.",
        kEmbedderSandbox ? "ENABLED" : "DISABLED",
        V8_ENABLE_SANDBOX_BOOL ? "ENABLED" : "DISABLED");
  }

  i::V8::Initialize();
  return true;
}

#if V8_OS_LINUX || V8_OS_DARWIN
bool TryHandleWebAssemblyTrapPosix(int sig_code, siginfo_t* info,
                                   void* context) {
#if V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED
  return i::trap_handler::TryHandleSignal(sig_code, info, context);
#else
  return false;
#endif
}
#endif

#if V8_OS_WIN
bool TryHandleWebAssemblyTrapWindows(EXCEPTION_POINTERS* exception) {
#if V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED
  return i::trap_handler::TryHandleWasmTrap(exception);
#else
  return false;
#endif
}
#endif

bool V8::EnableWebAssemblyTrapHandler(bool use_v8_signal_handler) {
#if V8_ENABLE_WEBASSEMBLY
  return v8::internal::trap_handler::EnableTrapHandler(use_v8_signal_handler);
#else
  return false;
#endif
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

void v8::V8::SetFatalMemoryErrorCallback(
    v8::OOMErrorCallback oom_error_callback) {
  g_oom_error_callback = oom_error_callback;
}

void v8::V8::SetEntropySource(EntropySource entropy_source) {
  base::RandomNumberGenerator::SetEntropySource(entropy_source);
}

void v8::V8::SetReturnAddressLocationResolver(
    ReturnAddressLocationResolver return_address_resolver) {
  i::StackFrame::SetReturnAddressLocationResolver(return_address_resolver);
}

bool v8::V8::Dispose() {
  i::V8::Dispose();
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
      number_of_detached_contexts_(0) {}

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
      external_script_source_size_(0),
      cpu_profiler_metadata_size_(0) {}

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

#ifdef V8_ENABLE_SANDBOX
VirtualAddressSpace* v8::V8::GetSandboxAddressSpace() {
  Utils::ApiCheck(i::GetProcessWideSandbox()->is_initialized(),
                  "v8::V8::GetSandboxAddressSpace",
                  "The sandbox must be initialized first");
  return i::GetProcessWideSandbox()->address_space();
}

size_t v8::V8::GetSandboxSizeInBytes() {
  Utils::ApiCheck(i::GetProcessWideSandbox()->is_initialized(),
                  "v8::V8::GetSandboxSizeInBytes",
                  "The sandbox must be initialized first.");
  return i::GetProcessWideSandbox()->size();
}

size_t v8::V8::GetSandboxReservationSizeInBytes() {
  Utils::ApiCheck(i::GetProcessWideSandbox()->is_initialized(),
                  "v8::V8::GetSandboxReservationSizeInBytes",
                  "The sandbox must be initialized first");
  return i::GetProcessWideSandbox()->reservation_size();
}

bool v8::V8::IsSandboxConfiguredSecurely() {
  Utils::ApiCheck(i::GetProcessWideSandbox()->is_initialized(),
                  "v8::V8::IsSandoxConfiguredSecurely",
                  "The sandbox must be initialized first");
  // The sandbox is (only) configured insecurely if it is a partially reserved
  // sandbox, since in that case unrelated memory mappings may end up inside
  // the sandbox address space where they could be corrupted by an attacker.
  return !i::GetProcessWideSandbox()->is_partially_reserved();
}
#endif  // V8_ENABLE_SANDBOX

void V8::GetSharedMemoryStatistics(SharedMemoryStatistics* statistics) {
  i::ReadOnlyHeap::PopulateReadOnlySpaceStatistics(statistics);
}

template <typename ObjectType>
struct InvokeBootstrapper;

template <>
struct InvokeBootstrapper<i::Context> {
  i::Handle<i::Context> Invoke(
      i::Isolate* i_isolate,
      i::MaybeHandle<i::JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
      v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
      v8::MicrotaskQueue* microtask_queue) {
    return i_isolate->bootstrapper()->CreateEnvironment(
        maybe_global_proxy, global_proxy_template, extensions,
        context_snapshot_index, embedder_fields_deserializer, microtask_queue);
  }
};

template <>
struct InvokeBootstrapper<i::JSGlobalProxy> {
  i::Handle<i::JSGlobalProxy> Invoke(
      i::Isolate* i_isolate,
      i::MaybeHandle<i::JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_proxy_template,
      v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
      v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
      v8::MicrotaskQueue* microtask_queue) {
    USE(extensions);
    USE(context_snapshot_index);
    return i_isolate->bootstrapper()->NewRemoteContext(maybe_global_proxy,
                                                       global_proxy_template);
  }
};

template <typename ObjectType>
static i::Handle<ObjectType> CreateEnvironment(
    i::Isolate* i_isolate, v8::ExtensionConfiguration* extensions,
    v8::MaybeLocal<ObjectTemplate> maybe_global_template,
    v8::MaybeLocal<Value> maybe_global_proxy, size_t context_snapshot_index,
    v8::DeserializeInternalFieldsCallback embedder_fields_deserializer,
    v8::MicrotaskQueue* microtask_queue) {
  i::Handle<ObjectType> result;

  {
    ENTER_V8_FOR_NEW_CONTEXT(i_isolate);
    v8::Local<ObjectTemplate> proxy_template;
    i::Handle<i::FunctionTemplateInfo> proxy_constructor;
    i::Handle<i::FunctionTemplateInfo> global_constructor;
    i::Handle<i::HeapObject> named_interceptor(
        i_isolate->factory()->undefined_value());
    i::Handle<i::HeapObject> indexed_interceptor(
        i_isolate->factory()->undefined_value());

    if (!maybe_global_template.IsEmpty()) {
      v8::Local<v8::ObjectTemplate> global_template =
          maybe_global_template.ToLocalChecked();
      // Make sure that the global_template has a constructor.
      global_constructor = EnsureConstructor(i_isolate, *global_template);

      // Create a fresh template for the global proxy object.
      proxy_template =
          ObjectTemplate::New(reinterpret_cast<v8::Isolate*>(i_isolate));
      proxy_constructor = EnsureConstructor(i_isolate, *proxy_template);

      // Set the global template to be the prototype template of
      // global proxy template.
      i::FunctionTemplateInfo::SetPrototypeTemplate(
          i_isolate, proxy_constructor, Utils::OpenHandle(*global_template));

      proxy_template->SetInternalFieldCount(
          global_template->InternalFieldCount());

      // Migrate security handlers from global_template to
      // proxy_template.  Temporarily removing access check
      // information from the global template.
      if (!global_constructor->GetAccessCheckInfo().IsUndefined(i_isolate)) {
        i::FunctionTemplateInfo::SetAccessCheckInfo(
            i_isolate, proxy_constructor,
            i::handle(global_constructor->GetAccessCheckInfo(), i_isolate));
        proxy_constructor->set_needs_access_check(
            global_constructor->needs_access_check());
        global_constructor->set_needs_access_check(false);
        i::FunctionTemplateInfo::SetAccessCheckInfo(
            i_isolate, global_constructor,
            i::ReadOnlyRoots(i_isolate).undefined_value_handle());
      }

      // Same for other interceptors. If the global constructor has
      // interceptors, we need to replace them temporarily with noop
      // interceptors, so the map is correctly marked as having interceptors,
      // but we don't invoke any.
      if (!global_constructor->GetNamedPropertyHandler().IsUndefined(
              i_isolate)) {
        named_interceptor =
            handle(global_constructor->GetNamedPropertyHandler(), i_isolate);
        i::FunctionTemplateInfo::SetNamedPropertyHandler(
            i_isolate, global_constructor,
            i::ReadOnlyRoots(i_isolate).noop_interceptor_info_handle());
      }
      if (!global_constructor->GetIndexedPropertyHandler().IsUndefined(
              i_isolate)) {
        indexed_interceptor =
            handle(global_constructor->GetIndexedPropertyHandler(), i_isolate);
        i::FunctionTemplateInfo::SetIndexedPropertyHandler(
            i_isolate, global_constructor,
            i::ReadOnlyRoots(i_isolate).noop_interceptor_info_handle());
      }
    }

    i::MaybeHandle<i::JSGlobalProxy> maybe_proxy;
    if (!maybe_global_proxy.IsEmpty()) {
      maybe_proxy = i::Handle<i::JSGlobalProxy>::cast(
          Utils::OpenHandle(*maybe_global_proxy.ToLocalChecked()));
    }
    // Create the environment.
    InvokeBootstrapper<ObjectType> invoke;
    result = invoke.Invoke(i_isolate, maybe_proxy, proxy_template, extensions,
                           context_snapshot_index, embedder_fields_deserializer,
                           microtask_queue);

    // Restore the access check info and interceptors on the global template.
    if (!maybe_global_template.IsEmpty()) {
      DCHECK(!global_constructor.is_null());
      DCHECK(!proxy_constructor.is_null());
      i::FunctionTemplateInfo::SetAccessCheckInfo(
          i_isolate, global_constructor,
          i::handle(proxy_constructor->GetAccessCheckInfo(), i_isolate));
      global_constructor->set_needs_access_check(
          proxy_constructor->needs_access_check());
      i::FunctionTemplateInfo::SetNamedPropertyHandler(
          i_isolate, global_constructor, named_interceptor);
      i::FunctionTemplateInfo::SetIndexedPropertyHandler(
          i_isolate, global_constructor, indexed_interceptor);
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  // TODO(jkummerow): This is for crbug.com/713699. Remove it if it doesn't
  // fail.
  // Sanity-check that the isolate is initialized and usable.
  CHECK(i_isolate->builtins()->code(i::Builtin::kIllegal).IsCode());

  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.NewContext");
  API_RCS_SCOPE(i_isolate, Context, New);
  i::HandleScope scope(i_isolate);
  ExtensionConfiguration no_extensions;
  if (extensions == nullptr) extensions = &no_extensions;
  i::Handle<i::Context> env = CreateEnvironment<i::Context>(
      i_isolate, extensions, global_template, global_object,
      context_snapshot_index, embedder_fields_deserializer, microtask_queue);
  if (env.is_null()) {
    if (i_isolate->has_pending_exception())
      i_isolate->clear_pending_exception();
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(external_isolate);
  API_RCS_SCOPE(i_isolate, Context, NewRemoteContext);
  i::HandleScope scope(i_isolate);
  i::Handle<i::FunctionTemplateInfo> global_constructor =
      EnsureConstructor(i_isolate, *global_template);
  Utils::ApiCheck(global_constructor->needs_access_check(),
                  "v8::Context::NewRemoteContext",
                  "Global template needs to have access checks enabled");
  i::Handle<i::AccessCheckInfo> access_check_info = i::handle(
      i::AccessCheckInfo::cast(global_constructor->GetAccessCheckInfo()),
      i_isolate);
  Utils::ApiCheck(access_check_info->named_interceptor() != i::Object(),
                  "v8::Context::NewRemoteContext",
                  "Global template needs to have access check handlers");
  i::Handle<i::JSObject> global_proxy = CreateEnvironment<i::JSGlobalProxy>(
      i_isolate, nullptr, global_template, global_object, 0,
      DeserializeInternalFieldsCallback(), nullptr);
  if (global_proxy.is_null()) {
    if (i_isolate->has_pending_exception())
      i_isolate->clear_pending_exception();
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
  i::Isolate* i_isolate = env->GetIsolate();
  i::Object security_token = env->security_token();
  i::Handle<i::Object> token_handle(security_token, i_isolate);
  return Utils::ToLocal(token_handle);
}

namespace {

bool MayContainObjectsToFreeze(i::InstanceType obj_type) {
  if (i::InstanceTypeChecker::IsString(obj_type)) return false;
  // SharedFunctionInfo is cross-context so it shouldn't be frozen.
  if (i::InstanceTypeChecker::IsSharedFunctionInfo(obj_type)) return false;
  return true;
}

bool RequiresEmbedderSupportToFreeze(i::InstanceType obj_type) {
  DCHECK(i::InstanceTypeChecker::IsJSReceiver(obj_type));

  return (i::InstanceTypeChecker::IsJSApiObject(obj_type) ||
          i::InstanceTypeChecker::IsJSExternalObject(obj_type) ||
          i::InstanceTypeChecker::IsJSObjectWithEmbedderSlots(obj_type));
}

bool IsJSReceiverSafeToFreeze(i::InstanceType obj_type) {
  DCHECK(i::InstanceTypeChecker::IsJSReceiver(obj_type));

  switch (obj_type) {
    case i::JS_OBJECT_TYPE:
    case i::JS_GLOBAL_OBJECT_TYPE:
    case i::JS_GLOBAL_PROXY_TYPE:
    case i::JS_PRIMITIVE_WRAPPER_TYPE:
    case i::JS_FUNCTION_TYPE:
    /* Function types */
    case i::BIGINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::BIGUINT64_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::FLOAT32_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::FLOAT64_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::INT16_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::INT32_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::INT8_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::UINT16_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::UINT32_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::UINT8_CLAMPED_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::UINT8_TYPED_ARRAY_CONSTRUCTOR_TYPE:
    case i::JS_ARRAY_CONSTRUCTOR_TYPE:
    case i::JS_PROMISE_CONSTRUCTOR_TYPE:
    case i::JS_REG_EXP_CONSTRUCTOR_TYPE:
    case i::JS_CLASS_CONSTRUCTOR_TYPE:
    /* Prototype Types */
    case i::JS_ARRAY_ITERATOR_PROTOTYPE_TYPE:
    case i::JS_ITERATOR_PROTOTYPE_TYPE:
    case i::JS_MAP_ITERATOR_PROTOTYPE_TYPE:
    case i::JS_OBJECT_PROTOTYPE_TYPE:
    case i::JS_PROMISE_PROTOTYPE_TYPE:
    case i::JS_REG_EXP_PROTOTYPE_TYPE:
    case i::JS_SET_ITERATOR_PROTOTYPE_TYPE:
    case i::JS_SET_PROTOTYPE_TYPE:
    case i::JS_STRING_ITERATOR_PROTOTYPE_TYPE:
    case i::JS_TYPED_ARRAY_PROTOTYPE_TYPE:
    /* */
    case i::JS_ARRAY_TYPE:
      return true;
#if V8_ENABLE_WEBASSEMBLY
    case i::WASM_ARRAY_TYPE:
    case i::WASM_STRUCT_TYPE:
#endif  // V8_ENABLE_WEBASSEMBLY
    case i::JS_PROXY_TYPE:
      return true;
    // These types are known not to freeze.
    case i::JS_MAP_KEY_ITERATOR_TYPE:
    case i::JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case i::JS_MAP_VALUE_ITERATOR_TYPE:
    case i::JS_SET_KEY_VALUE_ITERATOR_TYPE:
    case i::JS_SET_VALUE_ITERATOR_TYPE:
    case i::JS_GENERATOR_OBJECT_TYPE:
    case i::JS_ASYNC_FUNCTION_OBJECT_TYPE:
    case i::JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case i::JS_ARRAY_ITERATOR_TYPE: {
      return false;
    }
    default:
      // TODO(behamilton): Handle any types that fall through here.
      return false;
  }
}

class ObjectVisitorDeepFreezer : i::ObjectVisitor {
 public:
  explicit ObjectVisitorDeepFreezer(i::Isolate* isolate,
                                    Context::DeepFreezeDelegate* delegate)
      : isolate_(isolate), delegate_(delegate) {}

  bool DeepFreeze(i::Handle<i::Context> context) {
    bool success = VisitObject(i::HeapObject::cast(*context));
    DCHECK_EQ(success, !error_.has_value());
    if (!success) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate_, NewTypeError(error_->msg_id, error_->name), false);
    }
    for (const auto& obj : objects_to_freeze_) {
      MAYBE_RETURN_ON_EXCEPTION_VALUE(
          isolate_,
          i::JSReceiver::SetIntegrityLevel(isolate_, obj, i::FROZEN,
                                           i::kThrowOnError),
          false);
    }
    return true;
  }

  void VisitPointers(i::HeapObject host, i::ObjectSlot start,
                     i::ObjectSlot end) final {
    VisitPointersImpl(start, end);
  }
  void VisitPointers(i::HeapObject host, i::MaybeObjectSlot start,
                     i::MaybeObjectSlot end) final {
    VisitPointersImpl(start, end);
  }
  void VisitMapPointer(i::HeapObject host) final {
    VisitPointer(host, host.map_slot());
  }
  void VisitCodePointer(i::Code host, i::CodeObjectSlot slot) final {}
  void VisitCodeTarget(i::RelocInfo* rinfo) final {}
  void VisitEmbeddedPointer(i::RelocInfo* rinfo) final {}
  void VisitCustomWeakPointers(i::HeapObject host, i::ObjectSlot start,
                               i::ObjectSlot end) final {}

 private:
  struct ErrorInfo {
    i::MessageTemplate msg_id;
    i::Handle<i::String> name;
  };

  template <typename TSlot>
  void VisitPointersImpl(TSlot start, TSlot end) {
    for (TSlot current = start; current < end; ++current) {
      typename TSlot::TObject object = current.load(isolate_);
      i::HeapObject heap_object;
      if (object.GetHeapObjectIfStrong(&heap_object)) {
        if (!VisitObject(heap_object)) {
          return;
        }
      }
    }
  }

  bool FreezeEmbedderObjectAndVisitChildren(i::Handle<i::JSObject> obj) {
    DCHECK(delegate_);
    std::vector<Local<Object>> children;
    if (!delegate_->FreezeEmbedderObjectAndGetChildren(Utils::ToLocal(obj),
                                                       children)) {
      return false;
    }
    for (auto child : children) {
      if (!VisitObject(*Utils::OpenHandle<Object, i::JSReceiver>(child))) {
        return false;
      }
    }
    return true;
  }

  bool VisitObject(i::HeapObject obj) {
    DCHECK(!obj.is_null());
    if (error_.has_value()) {
      return false;
    }

    i::DisallowGarbageCollection no_gc;
    i::InstanceType obj_type = obj.map().instance_type();

    // Skip common types that can't contain items to freeze.
    if (!MayContainObjectsToFreeze(obj_type)) {
      return true;
    }

    if (!done_list_.insert(obj).second) {
      // If we couldn't insert (because it is already in the set) then we're
      // done.
      return true;
    }

    if (i::InstanceTypeChecker::IsAccessorPair(obj_type)) {
      // For AccessorPairs we need to ensure that the functions they point to
      // have been instantiated into actual JavaScript objects that can be
      // frozen. TODO(behamilton): If they haven't then we need to save them to
      // instantiate (and recurse) before freezing.
      i::AccessorPair accessor_pair = i::AccessorPair::cast(obj);
      if (accessor_pair.getter().IsFunctionTemplateInfo() ||
          accessor_pair.setter().IsFunctionTemplateInfo()) {
        // TODO(behamilton): Handle this more gracefully.
        error_ = ErrorInfo{i::MessageTemplate::kCannotDeepFreezeObject,
                           isolate_->factory()->empty_string()};
        return false;
      }
    } else if (i::InstanceTypeChecker::IsContext(obj_type)) {
      // For contexts we need to ensure that all accessible locals are const.
      // If not they could be replaced to bypass freezing.
      i::ScopeInfo scope_info = i::Context::cast(obj).scope_info();
      for (auto it : i::ScopeInfo::IterateLocalNames(&scope_info, no_gc)) {
        if (scope_info.ContextLocalMode(it->index()) !=
            i::VariableMode::kConst) {
          DCHECK(!error_.has_value());
          error_ = ErrorInfo{i::MessageTemplate::kCannotDeepFreezeValue,
                             i::handle(it->name(), isolate_)};
          return false;
        }
      }
    } else if (i::InstanceTypeChecker::IsJSReceiver(obj_type)) {
      i::Handle<i::JSReceiver> receiver =
          i::handle(i::JSReceiver::cast(obj), isolate_);
      if (RequiresEmbedderSupportToFreeze(obj_type)) {
        auto js_obj = i::Handle<i::JSObject>::cast(receiver);

        // External objects don't have slots but still need to be processed by
        // the embedder.
        if (i::InstanceTypeChecker::IsJSExternalObject(obj_type) ||
            js_obj->GetEmbedderFieldCount() > 0) {
          if (!delegate_) {
            DCHECK(!error_.has_value());
            error_ = ErrorInfo{i::MessageTemplate::kCannotDeepFreezeObject,
                               i::handle(receiver->class_name(), isolate_)};
            return false;
          }

          // Handle embedder specific types and any v8 children it wants to
          // freeze.
          if (!FreezeEmbedderObjectAndVisitChildren(js_obj)) {
            return false;
          }
        } else {
          DCHECK_EQ(js_obj->GetEmbedderFieldCount(), 0);
        }
      } else {
        DCHECK_IMPLIES(
            i::InstanceTypeChecker::IsJSObject(obj_type),
            i::JSObject::cast(*receiver).GetEmbedderFieldCount() == 0);
        if (!IsJSReceiverSafeToFreeze(obj_type)) {
          DCHECK(!error_.has_value());
          error_ = ErrorInfo{i::MessageTemplate::kCannotDeepFreezeObject,
                             i::handle(receiver->class_name(), isolate_)};
          return false;
        }
      }

      // Save this to freeze after we are done. Freezing triggers garbage
      // collection which doesn't work well with this visitor pattern, so we
      // delay it until after.
      objects_to_freeze_.push_back(receiver);

    } else {
      DCHECK(!i::InstanceTypeChecker::IsAccessorPair(obj_type));
      DCHECK(!i::InstanceTypeChecker::IsContext(obj_type));
      DCHECK(!i::InstanceTypeChecker::IsJSReceiver(obj_type));
    }

    DCHECK(!error_.has_value());
    obj.Iterate(isolate_, this);
    // Iterate sets error_ on failure. We should propagate errors.
    return !error_.has_value();
  }

  i::Isolate* isolate_;
  Context::DeepFreezeDelegate* delegate_;
  std::unordered_set<i::Object, i::Object::Hasher> done_list_;
  std::vector<i::Handle<i::JSReceiver>> objects_to_freeze_;
  base::Optional<ErrorInfo> error_;
};

}  // namespace

Maybe<void> Context::DeepFreeze(DeepFreezeDelegate* delegate) {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  i::Isolate* i_isolate = env->GetIsolate();

  // TODO(behamilton): Incorporate compatibility improvements similar to NodeJS:
  // https://github.com/nodejs/node/blob/main/lib/internal/freeze_intrinsics.js
  // These need to be done before freezing.

  Local<Context> context = Utils::ToLocal(env);
  ENTER_V8_NO_SCRIPT(i_isolate, context, Context, DeepFreeze, Nothing<void>(),
                     i::HandleScope);
  ObjectVisitorDeepFreezer vfreezer(i_isolate, delegate);
  has_pending_exception = !vfreezer.DeepFreeze(env);

  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(void);
  return JustVoid();
}

v8::Isolate* Context::GetIsolate() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  return reinterpret_cast<Isolate*>(env->GetIsolate());
}

v8::MicrotaskQueue* Context::GetMicrotaskQueue() {
  i::Handle<i::Context> env = Utils::OpenHandle(this);
  Utils::ApiCheck(env->IsNativeContext(), "v8::Context::GetMicrotaskQueue",
                  "Must be called on a native context");
  return i::Handle<i::NativeContext>::cast(env)->microtask_queue();
}

void Context::SetMicrotaskQueue(v8::MicrotaskQueue* queue) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  Utils::ApiCheck(context->IsNativeContext(), "v8::Context::SetMicrotaskQueue",
                  "Must be called on a native context");
  i::Handle<i::NativeContext> native_context =
      i::Handle<i::NativeContext>::cast(context);
  i::HandleScopeImplementer* impl = i_isolate->handle_scope_implementer();
  Utils::ApiCheck(!native_context->microtask_queue()->IsRunningMicrotasks(),
                  "v8::Context::SetMicrotaskQueue",
                  "Must not be running microtasks");
  Utils::ApiCheck(
      native_context->microtask_queue()->GetMicrotasksScopeDepth() == 0,
      "v8::Context::SetMicrotaskQueue", "Must not have microtask scope pushed");
  Utils::ApiCheck(impl->EnteredContextCount() == 0,
                  "v8::Context::SetMicrotaskQueue()",
                  "Cannot set Microtask Queue with an entered context");
  native_context->set_microtask_queue(
      i_isolate, static_cast<const i::MicrotaskQueue*>(queue));
}

v8::Local<v8::Object> Context::Global() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  i::Handle<i::Object> global(context->global_proxy(), i_isolate);
  // TODO(chromium:324812): This should always return the global proxy
  // but can't presently as calls to GetProtoype will return the wrong result.
  if (i::Handle<i::JSGlobalProxy>::cast(global)->IsDetachedFrom(
          context->global_object())) {
    global = i::Handle<i::Object>(context->global_object(), i_isolate);
  }
  return Utils::ToLocal(i::Handle<i::JSObject>::cast(global));
}

void Context::DetachGlobal() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i_isolate->DetachGlobal(context);
}

Local<v8::Object> Context::GetExtrasBindingObject() {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  i::Handle<i::JSObject> binding(context->extras_binding_object(), i_isolate);
  return Utils::ToLocal(binding);
}

void Context::AllowCodeGenerationFromStrings(bool allow) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  context->set_allow_code_gen_from_strings(
      allow ? i::ReadOnlyRoots(i_isolate).true_value()
            : i::ReadOnlyRoots(i_isolate).false_value());
}

bool Context::IsCodeGenerationFromStringsAllowed() const {
  i::Context context = *Utils::OpenHandle(this);
  return !context.allow_code_gen_from_strings().IsFalse(context.GetIsolate());
}

void Context::SetErrorMessageForCodeGenerationFromStrings(Local<String> error) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Handle<i::String> error_handle = Utils::OpenHandle(*error);
  context->set_error_message_for_code_gen_from_strings(*error_handle);
}

void Context::SetErrorMessageForWasmCodeGeneration(Local<String> error) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Handle<i::String> error_handle = Utils::OpenHandle(*error);
  context->set_error_message_for_wasm_code_gen(*error_handle);
}

void Context::SetAbortScriptExecution(
    Context::AbortScriptExecutionCallback callback) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  if (callback == nullptr) {
    context->set_script_execution_callback(
        i::ReadOnlyRoots(i_isolate).undefined_value());
  } else {
    SET_FIELD_WRAPPED(i_isolate, context, set_script_execution_callback,
                      callback);
  }
}

Local<Value> Context::GetContinuationPreservedEmbedderData() const {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  i::Handle<i::Object> data(
      context->native_context().continuation_preserved_embedder_data(),
      i_isolate);
  return ToApiHandle<Object>(data);
}

void Context::SetContinuationPreservedEmbedderData(Local<Value> data) {
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();
  if (data.IsEmpty())
    data = v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate));
  context->native_context().set_continuation_preserved_embedder_data(
      *i::Handle<i::HeapObject>::cast(Utils::OpenHandle(*data)));
}

void v8::Context::SetPromiseHooks(Local<Function> init_hook,
                                  Local<Function> before_hook,
                                  Local<Function> after_hook,
                                  Local<Function> resolve_hook) {
#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
  i::Handle<i::Context> context = Utils::OpenHandle(this);
  i::Isolate* i_isolate = context->GetIsolate();

  i::Handle<i::Object> init = i_isolate->factory()->undefined_value();
  i::Handle<i::Object> before = i_isolate->factory()->undefined_value();
  i::Handle<i::Object> after = i_isolate->factory()->undefined_value();
  i::Handle<i::Object> resolve = i_isolate->factory()->undefined_value();

  bool has_hook = false;

  if (!init_hook.IsEmpty()) {
    init = Utils::OpenHandle(*init_hook);
    has_hook = true;
  }
  if (!before_hook.IsEmpty()) {
    before = Utils::OpenHandle(*before_hook);
    has_hook = true;
  }
  if (!after_hook.IsEmpty()) {
    after = Utils::OpenHandle(*after_hook);
    has_hook = true;
  }
  if (!resolve_hook.IsEmpty()) {
    resolve = Utils::OpenHandle(*resolve_hook);
    has_hook = true;
  }

  i_isolate->SetHasContextPromiseHooks(has_hook);

  context->native_context().set_promise_hook_init_function(*init);
  context->native_context().set_promise_hook_before_function(*before);
  context->native_context().set_promise_hook_after_function(*after);
  context->native_context().set_promise_hook_resolve_function(*resolve);
#else   // V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
  Utils::ApiCheck(false, "v8::Context::SetPromiseHook",
                  "V8 was compiled without JavaScript Promise hooks");
#endif  // V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
}

bool Context::HasTemplateLiteralObject(Local<Value> object) {
  i::DisallowGarbageCollection no_gc;
  i::Object i_object = *Utils::OpenHandle(*object);
  if (!i_object.IsJSArray()) return false;
  return Utils::OpenHandle(this)->native_context().HasTemplateLiteralObject(
      i::JSArray::cast(i_object));
}

MaybeLocal<Context> metrics::Recorder::GetContext(
    Isolate* v8_isolate, metrics::Recorder::ContextId id) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  return i_isolate->GetContextFromRecorderContextId(id);
}

metrics::Recorder::ContextId metrics::Recorder::GetContextId(
    Local<Context> context) {
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  i::Isolate* i_isolate = i_context->GetIsolate();
  return i_isolate->GetOrRegisterRecorderContextId(
      handle(i_context->native_context(), i_isolate));
}

metrics::LongTaskStats metrics::LongTaskStats::Get(v8::Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  return *i_isolate->GetCurrentLongTaskStats();
}

namespace {
i::Address* GetSerializedDataFromFixedArray(i::Isolate* i_isolate,
                                            i::FixedArray list, size_t index) {
  if (index < static_cast<size_t>(list.length())) {
    int int_index = static_cast<int>(index);
    i::Object object = list.get(int_index);
    if (!object.IsTheHole(i_isolate)) {
      list.set_the_hole(i_isolate, int_index);
      // Shrink the list so that the last element is not the hole (unless it's
      // the first element, because we don't want to end up with a non-canonical
      // empty FixedArray).
      int last = list.length() - 1;
      while (last >= 0 && list.is_the_hole(i_isolate, last)) last--;
      if (last != -1) list.Shrink(i_isolate, last + 1);
      return i::Handle<i::Object>(object, i_isolate).location();
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
      i::ApiNatives::InstantiateObject(i_isolate, self), &result);
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
  i::Isolate* i_isolate = self->GetIsolate();
  API_RCS_SCOPE(i_isolate, FunctionTemplate, NewRemoteInstance);
  i::HandleScope scope(i_isolate);
  i::Handle<i::FunctionTemplateInfo> constructor =
      EnsureConstructor(i_isolate, *InstanceTemplate());
  Utils::ApiCheck(constructor->needs_access_check(),
                  "v8::FunctionTemplate::NewRemoteInstance",
                  "InstanceTemplate needs to have access checks enabled");
  i::Handle<i::AccessCheckInfo> access_check_info = i::handle(
      i::AccessCheckInfo::cast(constructor->GetAccessCheckInfo()), i_isolate);
  Utils::ApiCheck(access_check_info->named_interceptor() != i::Object(),
                  "v8::FunctionTemplate::NewRemoteInstance",
                  "InstanceTemplate needs to have access check handlers");
  i::Handle<i::JSObject> object;
  if (!i::ApiNatives::InstantiateRemoteObject(
           Utils::OpenHandle(*InstanceTemplate()))
           .ToHandle(&object)) {
    if (i_isolate->has_pending_exception()) {
      i_isolate->OptionalRescheduleException(true);
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

bool FunctionTemplate::IsLeafTemplateForApiObject(
    v8::Local<v8::Value> value) const {
  i::DisallowGarbageCollection no_gc;

  i::Object object = *Utils::OpenHandle(*value);

  auto self = Utils::OpenHandle(this);
  return self->IsLeafTemplateForApiObject(object);
}

Local<External> v8::External::New(Isolate* v8_isolate, void* value) {
  static_assert(sizeof(value) == sizeof(i::Address));
  // Nullptr is not allowed here because serialization/deserialization of
  // nullptr external api references is not possible as nullptr is used as an
  // external_references table terminator, see v8::SnapshotCreator()
  // constructors.
  DCHECK_NOT_NULL(value);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, External, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSObject> external = i_isolate->factory()->NewExternal(value);
  return Utils::ExternalToLocal(external);
}

void* External::Value() const {
  auto self = Utils::OpenHandle(this);
  return i::JSExternalObject::cast(*self).value();
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
                                           base::Vector<const char> string) {
  if (type == NewStringType::kInternalized) {
    return factory->InternalizeUtf8String(string);
  }
  return factory->NewStringFromUtf8(string);
}

V8_WARN_UNUSED_RESULT
inline i::MaybeHandle<i::String> NewString(i::Factory* factory,
                                           NewStringType type,
                                           base::Vector<const uint8_t> string) {
  if (type == NewStringType::kInternalized) {
    return factory->InternalizeString(string);
  }
  return factory->NewStringFromOneByte(string);
}

V8_WARN_UNUSED_RESULT
inline i::MaybeHandle<i::String> NewString(
    i::Factory* factory, NewStringType type,
    base::Vector<const uint16_t> string) {
  if (type == NewStringType::kInternalized) {
    return factory->InternalizeString(string);
  }
  return factory->NewStringFromTwoByte(string);
}

static_assert(v8::String::kMaxLength == i::String::kMaxLength);

}  // anonymous namespace

// TODO(dcarney): throw a context free exception.
#define NEW_STRING(v8_isolate, class_name, function_name, Char, data, type,   \
                   length)                                                    \
  MaybeLocal<String> result;                                                  \
  if (length == 0) {                                                          \
    result = String::Empty(v8_isolate);                                       \
  } else if (length > i::String::kMaxLength) {                                \
    result = MaybeLocal<String>();                                            \
  } else {                                                                    \
    i::Isolate* i_isolate = reinterpret_cast<internal::Isolate*>(v8_isolate); \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);                               \
    API_RCS_SCOPE(i_isolate, class_name, function_name);                      \
    if (length < 0) length = StringLength(data);                              \
    i::Handle<i::String> handle_result =                                      \
        NewString(i_isolate->factory(), type,                                 \
                  base::Vector<const Char>(data, length))                     \
            .ToHandleChecked();                                               \
    result = Utils::ToLocal(handle_result);                                   \
  }

Local<String> String::NewFromUtf8Literal(Isolate* v8_isolate,
                                         const char* literal,
                                         NewStringType type, int length) {
  DCHECK_LE(length, i::String::kMaxLength);
  i::Isolate* i_isolate = reinterpret_cast<internal::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  API_RCS_SCOPE(i_isolate, String, NewFromUtf8Literal);
  i::Handle<i::String> handle_result =
      NewString(i_isolate->factory(), type,
                base::Vector<const char>(literal, length))
          .ToHandleChecked();
  return Utils::ToLocal(handle_result);
}

MaybeLocal<String> String::NewFromUtf8(Isolate* v8_isolate, const char* data,
                                       NewStringType type, int length) {
  NEW_STRING(v8_isolate, String, NewFromUtf8, char, data, type, length);
  return result;
}

MaybeLocal<String> String::NewFromOneByte(Isolate* v8_isolate,
                                          const uint8_t* data,
                                          NewStringType type, int length) {
  NEW_STRING(v8_isolate, String, NewFromOneByte, uint8_t, data, type, length);
  return result;
}

MaybeLocal<String> String::NewFromTwoByte(Isolate* v8_isolate,
                                          const uint16_t* data,
                                          NewStringType type, int length) {
  NEW_STRING(v8_isolate, String, NewFromTwoByte, uint16_t, data, type, length);
  return result;
}

Local<String> v8::String::Concat(Isolate* v8_isolate, Local<String> left,
                                 Local<String> right) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::String> left_string = Utils::OpenHandle(*left);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  API_RCS_SCOPE(i_isolate, String, Concat);
  i::Handle<i::String> right_string = Utils::OpenHandle(*right);
  // If we are steering towards a range error, do not wait for the error to be
  // thrown, and return the null handle instead.
  if (left_string->length() + right_string->length() > i::String::kMaxLength) {
    return Local<String>();
  }
  i::Handle<i::String> result = i_isolate->factory()
                                    ->NewConsString(left_string, right_string)
                                    .ToHandleChecked();
  return Utils::ToLocal(result);
}

MaybeLocal<String> v8::String::NewExternalTwoByte(
    Isolate* v8_isolate, v8::String::ExternalStringResource* resource) {
  CHECK(resource && resource->data());
  // TODO(dcarney): throw a context free exception.
  if (resource->length() > static_cast<size_t>(i::String::kMaxLength)) {
    return MaybeLocal<String>();
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  API_RCS_SCOPE(i_isolate, String, NewExternalTwoByte);
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
    Isolate* v8_isolate, v8::String::ExternalOneByteStringResource* resource) {
  CHECK_NOT_NULL(resource);
  // TODO(dcarney): throw a context free exception.
  if (resource->length() > static_cast<size_t>(i::String::kMaxLength)) {
    return MaybeLocal<String>();
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  API_RCS_SCOPE(i_isolate, String, NewExternalOneByte);
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
  i::DisallowGarbageCollection no_gc;

  i::String obj = *Utils::OpenHandle(this);

  if (obj.IsThinString()) {
    obj = i::ThinString::cast(obj).actual();
  }

  if (!obj.SupportsExternalization()) {
    return false;
  }

  // TODO(v8:12007): Consider adding
  // MakeExternal(Isolate*, ExternalStringResource*).
  i::Isolate* i_isolate;
  if (obj.InWritableSharedSpace()) {
    i_isolate = i::Isolate::Current();
  } else {
    // It is safe to call GetIsolateFromWritableHeapObject because
    // SupportsExternalization already checked that the object is writable.
    i_isolate = i::GetIsolateFromWritableObject(obj);
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

  CHECK(resource && resource->data());

  bool result = obj.MakeExternal(resource);
  DCHECK_IMPLIES(result, HasExternalStringResource(obj));
  return result;
}

bool v8::String::MakeExternal(
    v8::String::ExternalOneByteStringResource* resource) {
  i::DisallowGarbageCollection no_gc;

  i::String obj = *Utils::OpenHandle(this);

  if (obj.IsThinString()) {
    obj = i::ThinString::cast(obj).actual();
  }

  if (!obj.SupportsExternalization()) {
    return false;
  }

  // TODO(v8:12007): Consider adding
  // MakeExternal(Isolate*, ExternalOneByteStringResource*).
  i::Isolate* i_isolate;
  if (obj.InWritableSharedSpace()) {
    i_isolate = i::Isolate::Current();
  } else {
    // It is safe to call GetIsolateFromWritableHeapObject because
    // SupportsExternalization already checked that the object is writable.
    i_isolate = i::GetIsolateFromWritableObject(obj);
  }
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

  CHECK(resource && resource->data());

  bool result = obj.MakeExternal(resource);
  DCHECK_IMPLIES(result, HasExternalStringResource(obj));
  return result;
}

bool v8::String::CanMakeExternal() const {
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

bool v8::String::CanMakeExternal(Encoding encoding) const {
  i::String obj = *Utils::OpenHandle(this);

  if (obj.IsThinString()) {
    obj = i::ThinString::cast(obj).actual();
  }

  if (!obj.SupportsExternalization(encoding)) {
    return false;
  }

  // Only old space strings should be externalized.
  return !i::Heap::InYoungGeneration(obj);
}

bool v8::String::StringEquals(Local<String> that) const {
  auto self = Utils::OpenHandle(this);
  auto other = Utils::OpenHandle(*that);
  return self->Equals(*other);
}

Isolate* v8::Object::GetIsolate() {
  i::Isolate* i_isolate = Utils::OpenHandle(this)->GetIsolate();
  return reinterpret_cast<Isolate*>(i_isolate);
}

Local<v8::Object> v8::Object::New(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, Object, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSObject> obj =
      i_isolate->factory()->NewJSObject(i_isolate->object_function());
  return Utils::ToLocal(obj);
}

namespace {

// TODO(v8:7569): This is a workaround for the Handle vs MaybeHandle difference
// in the return types of the different Add functions:
// OrderedNameDictionary::Add returns MaybeHandle, NameDictionary::Add returns
// Handle.
template <typename T>
i::Handle<T> ToHandle(i::Handle<T> h) {
  return h;
}
template <typename T>
i::Handle<T> ToHandle(i::MaybeHandle<T> h) {
  return h.ToHandleChecked();
}

template <typename Dictionary>
void AddPropertiesAndElementsToObject(i::Isolate* i_isolate,
                                      i::Handle<Dictionary>& properties,
                                      i::Handle<i::FixedArrayBase>& elements,
                                      Local<Name>* names, Local<Value>* values,
                                      size_t length) {
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
        properties = ToHandle(Dictionary::Add(
            i_isolate, properties, name, value, i::PropertyDetails::Empty()));
      } else {
        // Overwrite the {entry} with the {value}.
        properties->ValueAtPut(entry, *value);
      }
    }
  }
}

}  // namespace

Local<v8::Object> v8::Object::New(Isolate* v8_isolate,
                                  Local<Value> prototype_or_null,
                                  Local<Name>* names, Local<Value>* values,
                                  size_t length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::Object> proto = Utils::OpenHandle(*prototype_or_null);
  if (!Utils::ApiCheck(proto->IsNull() || proto->IsJSReceiver(),
                       "v8::Object::New", "prototype must be null or object")) {
    return Local<v8::Object>();
  }
  API_RCS_SCOPE(i_isolate, Object, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

  i::Handle<i::FixedArrayBase> elements =
      i_isolate->factory()->empty_fixed_array();

  // We assume that this API is mostly used to create objects with named
  // properties, and so we default to creating a properties backing store
  // large enough to hold all of them, while we start with no elements
  // (see http://bit.ly/v8-fast-object-create-cpp for the motivation).
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    i::Handle<i::SwissNameDictionary> properties =
        i_isolate->factory()->NewSwissNameDictionary(static_cast<int>(length));
    AddPropertiesAndElementsToObject(i_isolate, properties, elements, names,
                                     values, length);
    i::Handle<i::JSObject> obj =
        i_isolate->factory()->NewSlowJSObjectWithPropertiesAndElements(
            i::Handle<i::HeapObject>::cast(proto), properties, elements);
    return Utils::ToLocal(obj);
  } else {
    i::Handle<i::NameDictionary> properties =
        i::NameDictionary::New(i_isolate, static_cast<int>(length));
    AddPropertiesAndElementsToObject(i_isolate, properties, elements, names,
                                     values, length);
    i::Handle<i::JSObject> obj =
        i_isolate->factory()->NewSlowJSObjectWithPropertiesAndElements(
            i::Handle<i::HeapObject>::cast(proto), properties, elements);
    return Utils::ToLocal(obj);
  }
}

Local<v8::Value> v8::NumberObject::New(Isolate* v8_isolate, double value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, NumberObject, New);
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
  API_RCS_SCOPE(js_primitive_wrapper->GetIsolate(), NumberObject, NumberValue);
  return js_primitive_wrapper->value().Number();
}

Local<v8::Value> v8::BigIntObject::New(Isolate* v8_isolate, int64_t value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, BigIntObject, New);
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
  i::Isolate* i_isolate = js_primitive_wrapper->GetIsolate();
  API_RCS_SCOPE(i_isolate, BigIntObject, BigIntValue);
  return Utils::ToLocal(i::Handle<i::BigInt>(
      i::BigInt::cast(js_primitive_wrapper->value()), i_isolate));
}

Local<v8::Value> v8::BooleanObject::New(Isolate* v8_isolate, bool value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, BooleanObject, New);
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
  i::Object obj = *Utils::OpenHandle(this);
  i::JSPrimitiveWrapper js_primitive_wrapper = i::JSPrimitiveWrapper::cast(obj);
  i::Isolate* i_isolate = js_primitive_wrapper.GetIsolate();
  API_RCS_SCOPE(i_isolate, BooleanObject, BooleanValue);
  return js_primitive_wrapper.value().IsTrue(i_isolate);
}

Local<v8::Value> v8::StringObject::New(Isolate* v8_isolate,
                                       Local<String> value) {
  i::Handle<i::String> string = Utils::OpenHandle(*value);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, StringObject, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Object> obj =
      i::Object::ToObject(i_isolate, string).ToHandleChecked();
  return Utils::ToLocal(obj);
}

Local<v8::String> v8::StringObject::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSPrimitiveWrapper> js_primitive_wrapper =
      i::Handle<i::JSPrimitiveWrapper>::cast(obj);
  i::Isolate* i_isolate = js_primitive_wrapper->GetIsolate();
  API_RCS_SCOPE(i_isolate, StringObject, StringValue);
  return Utils::ToLocal(i::Handle<i::String>(
      i::String::cast(js_primitive_wrapper->value()), i_isolate));
}

Local<v8::Value> v8::SymbolObject::New(Isolate* v8_isolate,
                                       Local<Symbol> value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, SymbolObject, New);
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
  i::Isolate* i_isolate = js_primitive_wrapper->GetIsolate();
  API_RCS_SCOPE(i_isolate, SymbolObject, SymbolValue);
  return Utils::ToLocal(i::Handle<i::Symbol>(
      i::Symbol::cast(js_primitive_wrapper->value()), i_isolate));
}

MaybeLocal<v8::Value> v8::Date::New(Local<Context> context, double time) {
  if (std::isnan(time)) {
    // Introduce only canonical NaN value into the VM, to avoid signaling NaNs.
    time = std::numeric_limits<double>::quiet_NaN();
  }
  PREPARE_FOR_EXECUTION(context, Date, New, Value);
  Local<Value> result;
  has_pending_exception =
      !ToLocal<Value>(i::JSDate::New(i_isolate->date_function(),
                                     i_isolate->date_function(), time),
                      &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

double v8::Date::ValueOf() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSDate> jsdate = i::Handle<i::JSDate>::cast(obj);
  API_RCS_SCOPE(jsdate->GetIsolate(), Date, NumberValue);
  return jsdate->value().Number();
}

v8::Local<v8::String> v8::Date::ToISOString() const {
  i::Handle<i::Object> obj = Utils::OpenHandle(this);
  i::Handle<i::JSDate> jsdate = i::Handle<i::JSDate>::cast(obj);
  i::Isolate* i_isolate = jsdate->GetIsolate();
  API_RCS_SCOPE(i_isolate, Date, NumberValue);
  i::DateBuffer buffer =
      i::ToDateString(jsdate->value().Number(), i_isolate->date_cache(),
                      i::ToDateStringMode::kISODateAndTime);
  i::Handle<i::String> str = i_isolate->factory()
                                 ->NewStringFromUtf8(base::VectorOf(buffer))
                                 .ToHandleChecked();
  return Utils::ToLocal(str);
}

// Assert that the static TimeZoneDetection cast in
// DateTimeConfigurationChangeNotification is valid.
#define TIME_ZONE_DETECTION_ASSERT_EQ(value)                     \
  static_assert(                                                 \
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
      !ToLocal<RegExp>(i::JSRegExp::New(i_isolate, Utils::OpenHandle(*pattern),
                                        static_cast<i::JSRegExp::Flags>(flags)),
                       &result);
  RETURN_ON_FAILED_EXECUTION(RegExp);
  RETURN_ESCAPED(result);
}

MaybeLocal<v8::RegExp> v8::RegExp::NewWithBacktrackLimit(
    Local<Context> context, Local<String> pattern, Flags flags,
    uint32_t backtrack_limit) {
  Utils::ApiCheck(i::Smi::IsValid(backtrack_limit),
                  "v8::RegExp::NewWithBacktrackLimit",
                  "backtrack_limit is too large or too small");
  Utils::ApiCheck(backtrack_limit != i::JSRegExp::kNoBacktrackLimit,
                  "v8::RegExp::NewWithBacktrackLimit",
                  "Must set backtrack_limit");
  PREPARE_FOR_EXECUTION(context, RegExp, New, RegExp);
  Local<v8::RegExp> result;
  has_pending_exception = !ToLocal<RegExp>(
      i::JSRegExp::New(i_isolate, Utils::OpenHandle(*pattern),
                       static_cast<i::JSRegExp::Flags>(flags), backtrack_limit),
      &result);
  RETURN_ON_FAILED_EXECUTION(RegExp);
  RETURN_ESCAPED(result);
}

Local<v8::String> v8::RegExp::GetSource() const {
  i::Handle<i::JSRegExp> obj = Utils::OpenHandle(this);
  return Utils::ToLocal(
      i::Handle<i::String>(obj->EscapedPattern(), obj->GetIsolate()));
}

// Assert that the static flags cast in GetFlags is valid.
#define REGEXP_FLAG_ASSERT_EQ(flag)                   \
  static_assert(static_cast<int>(v8::RegExp::flag) == \
                static_cast<int>(i::JSRegExp::flag))
REGEXP_FLAG_ASSERT_EQ(kNone);
REGEXP_FLAG_ASSERT_EQ(kGlobal);
REGEXP_FLAG_ASSERT_EQ(kIgnoreCase);
REGEXP_FLAG_ASSERT_EQ(kMultiline);
REGEXP_FLAG_ASSERT_EQ(kSticky);
REGEXP_FLAG_ASSERT_EQ(kUnicode);
REGEXP_FLAG_ASSERT_EQ(kHasIndices);
REGEXP_FLAG_ASSERT_EQ(kLinear);
REGEXP_FLAG_ASSERT_EQ(kUnicodeSets);
#undef REGEXP_FLAG_ASSERT_EQ

v8::RegExp::Flags v8::RegExp::GetFlags() const {
  i::Handle<i::JSRegExp> obj = Utils::OpenHandle(this);
  return RegExp::Flags(static_cast<int>(obj->flags()));
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
      i::RegExpUtils::RegExpExec(i_isolate, regexp, subject_string,
                                 i_isolate->factory()->undefined_value()),
      &result);

  RETURN_ON_FAILED_EXECUTION(Object);
  RETURN_ESCAPED(result);
}

Local<v8::Array> v8::Array::New(Isolate* v8_isolate, int length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, Array, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  int real_length = length > 0 ? length : 0;
  i::Handle<i::JSArray> obj = i_isolate->factory()->NewJSArray(real_length);
  i::Handle<i::Object> length_obj =
      i_isolate->factory()->NewNumberFromInt(real_length);
  obj->set_length(*length_obj);
  return Utils::ToLocal(obj);
}

Local<v8::Array> v8::Array::New(Isolate* v8_isolate, Local<Value>* elements,
                                size_t length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Factory* factory = i_isolate->factory();
  API_RCS_SCOPE(i_isolate, Array, New);
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

Local<v8::Map> v8::Map::New(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, Map, New);
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
  i::Isolate* i_isolate = self->GetIsolate();
  API_RCS_SCOPE(i_isolate, Map, Clear);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::JSMap::Clear(i_isolate, self);
}

MaybeLocal<Value> Map::Get(Local<Context> context, Local<Value> key) {
  PREPARE_FOR_EXECUTION(context, Map, Get, Value);
  auto self = Utils::OpenHandle(this);
  Local<Value> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !ToLocal<Value>(i::Execution::CallBuiltin(i_isolate, i_isolate->map_get(),
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
      !i::Execution::CallBuiltin(i_isolate, i_isolate->map_set(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Map);
  RETURN_ESCAPED(Local<Map>::Cast(Utils::ToLocal(result)));
}

Maybe<bool> Map::Has(Local<Context> context, Local<Value> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Map, Has, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(i_isolate, i_isolate->map_has(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(i_isolate));
}

Maybe<bool> Map::Delete(Local<Context> context, Local<Value> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Map, Delete, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(i_isolate, i_isolate->map_delete(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(i_isolate));
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

i::Handle<i::JSArray> MapAsArray(i::Isolate* i_isolate, i::Object table_obj,
                                 int offset, MapAsArrayKind kind) {
  i::Factory* factory = i_isolate->factory();
  i::Handle<i::OrderedHashMap> table(i::OrderedHashMap::cast(table_obj),
                                     i_isolate);
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
    i::DisallowGarbageCollection no_gc;
    i::Oddball the_hole = i::ReadOnlyRoots(i_isolate).the_hole_value();
    for (int i = offset; i < capacity; ++i) {
      i::InternalIndex entry(i);
      i::Object key = table->KeyAt(entry);
      if (key == the_hole) continue;
      if (collect_keys) result->set(result_index++, key);
      if (collect_values) result->set(result_index++, table->ValueAt(entry));
    }
  }
  DCHECK_GE(max_length, result_index);
  if (result_index == 0) return factory->NewJSArray(0);
  result->Shrink(i_isolate, result_index);
  return factory->NewJSArrayWithElements(result, i::PACKED_ELEMENTS,
                                         result_index);
}

}  // namespace

Local<Array> Map::AsArray() const {
  i::Handle<i::JSMap> obj = Utils::OpenHandle(this);
  i::Isolate* i_isolate = obj->GetIsolate();
  API_RCS_SCOPE(i_isolate, Map, AsArray);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return Utils::ToLocal(
      MapAsArray(i_isolate, obj->table(), 0, MapAsArrayKind::kEntries));
}

Local<v8::Set> v8::Set::New(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, Set, New);
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
  i::Isolate* i_isolate = self->GetIsolate();
  API_RCS_SCOPE(i_isolate, Set, Clear);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::JSSet::Clear(i_isolate, self);
}

MaybeLocal<Set> Set::Add(Local<Context> context, Local<Value> key) {
  PREPARE_FOR_EXECUTION(context, Set, Add, Set);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(i_isolate, i_isolate->set_add(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Set);
  RETURN_ESCAPED(Local<Set>::Cast(Utils::ToLocal(result)));
}

Maybe<bool> Set::Has(Local<Context> context, Local<Value> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Set, Has, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(i_isolate, i_isolate->set_has(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(i_isolate));
}

Maybe<bool> Set::Delete(Local<Context> context, Local<Value> key) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Set, Delete, Nothing<bool>(), i::HandleScope);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !i::Execution::CallBuiltin(i_isolate, i_isolate->set_delete(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(result->IsTrue(i_isolate));
}

namespace {
i::Handle<i::JSArray> SetAsArray(i::Isolate* i_isolate, i::Object table_obj,
                                 int offset, SetAsArrayKind kind) {
  i::Factory* factory = i_isolate->factory();
  i::Handle<i::OrderedHashSet> table(i::OrderedHashSet::cast(table_obj),
                                     i_isolate);
  // Elements skipped by |offset| may already be deleted.
  int capacity = table->UsedCapacity();
  const bool collect_key_values = kind == SetAsArrayKind::kEntries;
  int max_length = (capacity - offset) * (collect_key_values ? 2 : 1);
  if (max_length == 0) return factory->NewJSArray(0);
  i::Handle<i::FixedArray> result = factory->NewFixedArray(max_length);
  int result_index = 0;
  {
    i::DisallowGarbageCollection no_gc;
    i::Oddball the_hole = i::ReadOnlyRoots(i_isolate).the_hole_value();
    for (int i = offset; i < capacity; ++i) {
      i::InternalIndex entry(i);
      i::Object key = table->KeyAt(entry);
      if (key == the_hole) continue;
      result->set(result_index++, key);
      if (collect_key_values) result->set(result_index++, key);
    }
  }
  DCHECK_GE(max_length, result_index);
  if (result_index == 0) return factory->NewJSArray(0);
  result->Shrink(i_isolate, result_index);
  return factory->NewJSArrayWithElements(result, i::PACKED_ELEMENTS,
                                         result_index);
}
}  // namespace

Local<Array> Set::AsArray() const {
  i::Handle<i::JSSet> obj = Utils::OpenHandle(this);
  i::Isolate* i_isolate = obj->GetIsolate();
  API_RCS_SCOPE(i_isolate, Set, AsArray);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return Utils::ToLocal(
      SetAsArray(i_isolate, obj->table(), 0, SetAsArrayKind::kValues));
}

MaybeLocal<Promise::Resolver> Promise::Resolver::New(Local<Context> context) {
  PREPARE_FOR_EXECUTION(context, Promise_Resolver, New, Resolver);
  Local<Promise::Resolver> result;
  has_pending_exception = !ToLocal<Promise::Resolver>(
      i_isolate->factory()->NewJSPromise(), &result);
  RETURN_ON_FAILED_EXECUTION(Promise::Resolver);
  RETURN_ESCAPED(result);
}

Local<Promise> Promise::Resolver::GetPromise() {
  i::Handle<i::JSReceiver> promise = Utils::OpenHandle(this);
  return Local<Promise>::Cast(Utils::ToLocal(promise));
}

Maybe<bool> Promise::Resolver::Resolve(Local<Context> context,
                                       Local<Value> value) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Promise_Resolver, Resolve, Nothing<bool>(),
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
  auto i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8(i_isolate, context, Promise_Resolver, Reject, Nothing<bool>(),
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
  i::Handle<i::Object> argv[] = {i_isolate->factory()->undefined_value(),
                                 Utils::OpenHandle(*handler)};
  i::Handle<i::Object> result;
  // Do not call the built-in Promise.prototype.catch!
  // v8::Promise should not call out to a monkeypatched Promise.prototype.then
  // as the implementation of Promise.prototype.catch does.
  has_pending_exception =
      !i::Execution::CallBuiltin(i_isolate, i_isolate->promise_then(), self,
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
      !i::Execution::CallBuiltin(i_isolate, i_isolate->promise_then(), self,
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
      !i::Execution::CallBuiltin(i_isolate, i_isolate->promise_then(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(Promise);
  RETURN_ESCAPED(Local<Promise>::Cast(Utils::ToLocal(result)));
}

bool Promise::HasHandler() const {
  i::JSReceiver promise = *Utils::OpenHandle(this);
  i::Isolate* i_isolate = promise.GetIsolate();
  API_RCS_SCOPE(i_isolate, Promise, HasRejectHandler);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  if (!promise.IsJSPromise()) return false;
  return i::JSPromise::cast(promise).has_handler();
}

Local<Value> Promise::Result() {
  i::Handle<i::JSReceiver> promise = Utils::OpenHandle(this);
  i::Isolate* i_isolate = promise->GetIsolate();
  API_RCS_SCOPE(i_isolate, Promise, Result);
  i::Handle<i::JSPromise> js_promise = i::Handle<i::JSPromise>::cast(promise);
  Utils::ApiCheck(js_promise->status() != kPending, "v8_Promise_Result",
                  "Promise is still pending");
  i::Handle<i::Object> result(js_promise->result(), i_isolate);
  return Utils::ToLocal(result);
}

Promise::PromiseState Promise::State() {
  i::Handle<i::JSReceiver> promise = Utils::OpenHandle(this);
  API_RCS_SCOPE(promise->GetIsolate(), Promise, Status);
  i::Handle<i::JSPromise> js_promise = i::Handle<i::JSPromise>::cast(promise);
  return static_cast<PromiseState>(js_promise->status());
}

void Promise::MarkAsHandled() {
  i::Handle<i::JSPromise> js_promise = Utils::OpenHandle(this);
  js_promise->set_has_handler(true);
}

void Promise::MarkAsSilent() {
  i::Handle<i::JSPromise> js_promise = Utils::OpenHandle(this);
  js_promise->set_is_silent(true);
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

bool Proxy::IsRevoked() const {
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
      !ToLocal<Proxy>(i::JSProxy::New(i_isolate, target, handler), &result);
  RETURN_ON_FAILED_EXECUTION(Proxy);
  RETURN_ESCAPED(result);
}

CompiledWasmModule::CompiledWasmModule(
    std::shared_ptr<internal::wasm::NativeModule> native_module,
    const char* source_url, size_t url_length)
    : native_module_(std::move(native_module)),
      source_url_(source_url, url_length) {
  CHECK_NOT_NULL(native_module_);
}

OwnedBuffer CompiledWasmModule::Serialize() {
#if V8_ENABLE_WEBASSEMBLY
  TRACE_EVENT0("v8.wasm", "wasm.SerializeModule");
  i::wasm::WasmSerializer wasm_serializer(native_module_.get());
  size_t buffer_size = wasm_serializer.GetSerializedNativeModuleSize();
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  if (!wasm_serializer.SerializeNativeModule({buffer.get(), buffer_size}))
    return {};
  return {std::move(buffer), buffer_size};
#else
  UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
}

MemorySpan<const uint8_t> CompiledWasmModule::GetWireBytesRef() {
#if V8_ENABLE_WEBASSEMBLY
  base::Vector<const uint8_t> bytes_vec = native_module_->wire_bytes();
  return {bytes_vec.begin(), bytes_vec.size()};
#else
  UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
}

Local<ArrayBuffer> v8::WasmMemoryObject::Buffer() {
#if V8_ENABLE_WEBASSEMBLY
  i::Handle<i::WasmMemoryObject> obj = Utils::OpenHandle(this);
  i::Handle<i::JSArrayBuffer> buffer(obj->array_buffer(), obj->GetIsolate());
  return Utils::ToLocal(buffer);
#else
  UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
}

CompiledWasmModule WasmModuleObject::GetCompiledModule() {
#if V8_ENABLE_WEBASSEMBLY
  auto obj = i::Handle<i::WasmModuleObject>::cast(Utils::OpenHandle(this));
  auto url =
      i::handle(i::String::cast(obj->script().name()), obj->GetIsolate());
  int length;
  std::unique_ptr<char[]> cstring =
      url->ToCString(i::DISALLOW_NULLS, i::FAST_STRING_TRAVERSAL, &length);
  return CompiledWasmModule(std::move(obj->shared_native_module()),
                            cstring.get(), length);
#else
  UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
}

MaybeLocal<WasmModuleObject> WasmModuleObject::FromCompiledModule(
    Isolate* v8_isolate, const CompiledWasmModule& compiled_module) {
#if V8_ENABLE_WEBASSEMBLY
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::WasmModuleObject> module_object =
      i::wasm::GetWasmEngine()->ImportNativeModule(
          i_isolate, compiled_module.native_module_,
          base::VectorOf(compiled_module.source_url()));
  return Local<WasmModuleObject>::Cast(
      Utils::ToLocal(i::Handle<i::JSObject>::cast(module_object)));
#else
  UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
}

MaybeLocal<WasmModuleObject> WasmModuleObject::Compile(
    Isolate* v8_isolate, MemorySpan<const uint8_t> wire_bytes) {
#if V8_ENABLE_WEBASSEMBLY
  const uint8_t* start = wire_bytes.data();
  size_t length = wire_bytes.size();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    return MaybeLocal<WasmModuleObject>();
  }
  i::MaybeHandle<i::JSObject> maybe_compiled;
  {
    i::wasm::ErrorThrower thrower(i_isolate, "WasmModuleObject::Compile()");
    auto enabled_features = i::wasm::WasmFeatures::FromIsolate(i_isolate);
    maybe_compiled = i::wasm::GetWasmEngine()->SyncCompile(
        i_isolate, enabled_features, &thrower,
        i::wasm::ModuleWireBytes(start, start + length));
  }
  CHECK_EQ(maybe_compiled.is_null(), i_isolate->has_pending_exception());
  if (maybe_compiled.is_null()) {
    i_isolate->OptionalRescheduleException(false);
    return MaybeLocal<WasmModuleObject>();
  }
  return Local<WasmModuleObject>::Cast(
      Utils::ToLocal(maybe_compiled.ToHandleChecked()));
#else
  Utils::ApiCheck(false, "WasmModuleObject::Compile",
                  "WebAssembly support is not enabled");
  UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
}

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

bool v8::ArrayBuffer::IsDetachable() const {
  return Utils::OpenHandle(this)->is_detachable();
}

bool v8::ArrayBuffer::WasDetached() const {
  return Utils::OpenHandle(this)->was_detached();
}

namespace {
std::shared_ptr<i::BackingStore> ToInternal(
    std::shared_ptr<i::BackingStoreBase> backing_store) {
  return std::static_pointer_cast<i::BackingStore>(backing_store);
}
}  // namespace

Maybe<bool> v8::ArrayBuffer::Detach(v8::Local<v8::Value> key) {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  i::Isolate* i_isolate = obj->GetIsolate();
  Utils::ApiCheck(obj->is_detachable(), "v8::ArrayBuffer::Detach",
                  "Only detachable ArrayBuffers can be detached");
  ENTER_V8_NO_SCRIPT(
      i_isolate, reinterpret_cast<v8::Isolate*>(i_isolate)->GetCurrentContext(),
      ArrayBuffer, Detach, Nothing<bool>(), i::HandleScope);
  if (!key.IsEmpty()) {
    i::Handle<i::Object> i_key = Utils::OpenHandle(*key);
    constexpr bool kForceForWasmMemory = false;
    has_pending_exception =
        i::JSArrayBuffer::Detach(obj, kForceForWasmMemory, i_key).IsNothing();
  } else {
    has_pending_exception = i::JSArrayBuffer::Detach(obj).IsNothing();
  }
  RETURN_ON_FAILED_EXECUTION_PRIMITIVE(bool);
  return Just(true);
}

void v8::ArrayBuffer::Detach() { Detach(Local<Value>()).Check(); }

void v8::ArrayBuffer::SetDetachKey(v8::Local<v8::Value> key) {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  i::Handle<i::Object> i_key = Utils::OpenHandle(*key);
  obj->set_detach_key(*i_key);
}

size_t v8::ArrayBuffer::ByteLength() const {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return obj->GetByteLength();
}

size_t v8::ArrayBuffer::MaxByteLength() const {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return obj->max_byte_length();
}

Local<ArrayBuffer> v8::ArrayBuffer::New(Isolate* v8_isolate,
                                        size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, ArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::MaybeHandle<i::JSArrayBuffer> result =
      i_isolate->factory()->NewJSArrayBufferAndBackingStore(
          byte_length, i::InitializedFlag::kZeroInitialized);

  i::Handle<i::JSArrayBuffer> array_buffer;
  if (!result.ToHandle(&array_buffer)) {
    // TODO(jbroman): It may be useful in the future to provide a MaybeLocal
    // version that throws an exception or otherwise does not crash.
    i::V8::FatalProcessOutOfMemory(i_isolate, "v8::ArrayBuffer::New");
  }

  return Utils::ToLocal(array_buffer);
}

Local<ArrayBuffer> v8::ArrayBuffer::New(
    Isolate* v8_isolate, std::shared_ptr<BackingStore> backing_store) {
  CHECK_IMPLIES(backing_store->ByteLength() != 0,
                backing_store->Data() != nullptr);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, ArrayBuffer, New);
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
    Isolate* v8_isolate, size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, ArrayBuffer, NewBackingStore);
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::Allocate(i_isolate, byte_length,
                                i::SharedFlag::kNotShared,
                                i::InitializedFlag::kZeroInitialized);
  if (!backing_store) {
    i::V8::FatalProcessOutOfMemory(i_isolate,
                                   "v8::ArrayBuffer::NewBackingStore");
  }
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}

std::unique_ptr<v8::BackingStore> v8::ArrayBuffer::NewBackingStore(
    void* data, size_t byte_length, v8::BackingStore::DeleterCallback deleter,
    void* deleter_data) {
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
#ifdef V8_ENABLE_SANDBOX
  Utils::ApiCheck(!data || i::GetProcessWideSandbox()->Contains(data),
                  "v8_ArrayBuffer_NewBackingStore",
                  "When the V8 Sandbox is enabled, ArrayBuffer backing stores "
                  "must be allocated inside the sandbox address space. Please "
                  "use an appropriate ArrayBuffer::Allocator to allocate these "
                  "buffers, or disable the sandbox.");
#endif  // V8_ENABLE_SANDBOX

  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::WrapAllocation(data, byte_length, deleter, deleter_data,
                                      i::SharedFlag::kNotShared);
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}

// static
std::unique_ptr<BackingStore> v8::ArrayBuffer::NewResizableBackingStore(
    size_t byte_length, size_t max_byte_length) {
  Utils::ApiCheck(i::v8_flags.harmony_rab_gsab,
                  "v8::ArrayBuffer::NewResizableBackingStore",
                  "Constructing resizable ArrayBuffers is not supported");
  Utils::ApiCheck(byte_length <= max_byte_length,
                  "v8::ArrayBuffer::NewResizableBackingStore",
                  "Cannot construct resizable ArrayBuffer, byte_length must be "
                  "<= max_byte_length");
  Utils::ApiCheck(
      byte_length <= i::JSArrayBuffer::kMaxByteLength,
      "v8::ArrayBuffer::NewResizableBackingStore",
      "Cannot construct resizable ArrayBuffer, requested length is too big");

  size_t page_size, initial_pages, max_pages;
  if (i::JSArrayBuffer::GetResizableBackingStorePageConfiguration(
          nullptr, byte_length, max_byte_length, i::kDontThrow, &page_size,
          &initial_pages, &max_pages)
          .IsNothing()) {
    i::V8::FatalProcessOutOfMemory(nullptr,
                                   "v8::ArrayBuffer::NewResizableBackingStore");
  }
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::TryAllocateAndPartiallyCommitMemory(
          nullptr, byte_length, max_byte_length, page_size, initial_pages,
          max_pages, i::WasmMemoryFlag::kNotWasm, i::SharedFlag::kNotShared);
  if (!backing_store) {
    i::V8::FatalProcessOutOfMemory(nullptr,
                                   "v8::ArrayBuffer::NewResizableBackingStore");
  }
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
  } else if (obj->IsJSRabGsabDataView()) {
    i::Handle<i::JSRabGsabDataView> data_view(i::JSRabGsabDataView::cast(*obj),
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
  size_t bytes_to_copy = std::min(byte_length, self->byte_length());
  if (bytes_to_copy) {
    i::DisallowGarbageCollection no_gc;
    i::Isolate* i_isolate = self->GetIsolate();
    const char* source;
    if (self->IsJSTypedArray()) {
      i::Handle<i::JSTypedArray> array(i::JSTypedArray::cast(*self), i_isolate);
      source = reinterpret_cast<char*>(array->DataPtr());
    } else if (self->IsJSDataView()) {
      i::Handle<i::JSDataView> data_view(i::JSDataView::cast(*self), i_isolate);
      source = reinterpret_cast<char*>(data_view->data_pointer());
    } else {
      DCHECK(self->IsJSRabGsabDataView());
      i::Handle<i::JSRabGsabDataView> data_view(
          i::JSRabGsabDataView::cast(*self), i_isolate);
      source = reinterpret_cast<char*>(data_view->data_pointer());
    }
    memcpy(dest, source, bytes_to_copy);
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
  i::DisallowGarbageCollection no_gc;
  i::JSArrayBufferView obj = *Utils::OpenHandle(this);
  if (obj.WasDetached()) {
    return 0;
  }
  if (obj.IsJSTypedArray()) {
    return i::JSTypedArray::cast(obj).GetByteLength();
  }
  if (obj.IsJSDataView()) {
    return i::JSDataView::cast(obj).byte_length();
  }
  return i::JSRabGsabDataView::cast(obj).GetByteLength();
}

size_t v8::TypedArray::Length() {
  i::DisallowGarbageCollection no_gc;
  i::JSTypedArray obj = *Utils::OpenHandle(this);
  return obj.WasDetached() ? 0 : obj.GetLength();
}

static_assert(
    v8::TypedArray::kMaxLength == i::JSTypedArray::kMaxLength,
    "v8::TypedArray::kMaxLength must match i::JSTypedArray::kMaxLength");

#define TYPED_ARRAY_NEW(Type, type, TYPE, ctype)                            \
  Local<Type##Array> Type##Array::New(Local<ArrayBuffer> array_buffer,      \
                                      size_t byte_offset, size_t length) {  \
    i::Isolate* i_isolate = Utils::OpenHandle(*array_buffer)->GetIsolate(); \
    API_RCS_SCOPE(i_isolate, Type##Array, New);                             \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);                             \
    if (!Utils::ApiCheck(length <= kMaxLength,                              \
                         "v8::" #Type                                       \
                         "Array::New(Local<ArrayBuffer>, size_t, size_t)",  \
                         "length exceeds max allowed value")) {             \
      return Local<Type##Array>();                                          \
    }                                                                       \
    i::Handle<i::JSArrayBuffer> buffer = Utils::OpenHandle(*array_buffer);  \
    i::Handle<i::JSTypedArray> obj = i_isolate->factory()->NewJSTypedArray( \
        i::kExternal##Type##Array, buffer, byte_offset, length);            \
    return Utils::ToLocal##Type##Array(obj);                                \
  }                                                                         \
  Local<Type##Array> Type##Array::New(                                      \
      Local<SharedArrayBuffer> shared_array_buffer, size_t byte_offset,     \
      size_t length) {                                                      \
    CHECK(i::v8_flags.harmony_sharedarraybuffer);                           \
    i::Isolate* i_isolate =                                                 \
        Utils::OpenHandle(*shared_array_buffer)->GetIsolate();              \
    API_RCS_SCOPE(i_isolate, Type##Array, New);                             \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);                             \
    if (!Utils::ApiCheck(                                                   \
            length <= kMaxLength,                                           \
            "v8::" #Type                                                    \
            "Array::New(Local<SharedArrayBuffer>, size_t, size_t)",         \
            "length exceeds max allowed value")) {                          \
      return Local<Type##Array>();                                          \
    }                                                                       \
    i::Handle<i::JSArrayBuffer> buffer =                                    \
        Utils::OpenHandle(*shared_array_buffer);                            \
    i::Handle<i::JSTypedArray> obj = i_isolate->factory()->NewJSTypedArray( \
        i::kExternal##Type##Array, buffer, byte_offset, length);            \
    return Utils::ToLocal##Type##Array(obj);                                \
  }

TYPED_ARRAYS(TYPED_ARRAY_NEW)
#undef TYPED_ARRAY_NEW

// TODO(v8:11111): Support creating length tracking DataViews via the API.
Local<DataView> DataView::New(Local<ArrayBuffer> array_buffer,
                              size_t byte_offset, size_t byte_length) {
  i::Handle<i::JSArrayBuffer> buffer = Utils::OpenHandle(*array_buffer);
  i::Isolate* i_isolate = buffer->GetIsolate();
  API_RCS_SCOPE(i_isolate, DataView, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSDataView> obj = i::Handle<i::JSDataView>::cast(
      i_isolate->factory()->NewJSDataViewOrRabGsabDataView(buffer, byte_offset,
                                                           byte_length));
  return Utils::ToLocal(obj);
}

Local<DataView> DataView::New(Local<SharedArrayBuffer> shared_array_buffer,
                              size_t byte_offset, size_t byte_length) {
  CHECK(i::v8_flags.harmony_sharedarraybuffer);
  i::Handle<i::JSArrayBuffer> buffer = Utils::OpenHandle(*shared_array_buffer);
  i::Isolate* i_isolate = buffer->GetIsolate();
  API_RCS_SCOPE(i_isolate, DataView, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSDataView> obj = i::Handle<i::JSDataView>::cast(
      i_isolate->factory()->NewJSDataViewOrRabGsabDataView(buffer, byte_offset,
                                                           byte_length));
  return Utils::ToLocal(obj);
}

size_t v8::SharedArrayBuffer::ByteLength() const {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return obj->GetByteLength();
}

size_t v8::SharedArrayBuffer::MaxByteLength() const {
  i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(this);
  return obj->max_byte_length();
}

Local<SharedArrayBuffer> v8::SharedArrayBuffer::New(Isolate* v8_isolate,
                                                    size_t byte_length) {
  CHECK(i::v8_flags.harmony_sharedarraybuffer);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, SharedArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

  std::unique_ptr<i::BackingStore> backing_store =
      i::BackingStore::Allocate(i_isolate, byte_length, i::SharedFlag::kShared,
                                i::InitializedFlag::kZeroInitialized);

  if (!backing_store) {
    // TODO(jbroman): It may be useful in the future to provide a MaybeLocal
    // version that throws an exception or otherwise does not crash.
    i::V8::FatalProcessOutOfMemory(i_isolate, "v8::SharedArrayBuffer::New");
  }

  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSSharedArrayBuffer(std::move(backing_store));
  return Utils::ToLocalShared(obj);
}

Local<SharedArrayBuffer> v8::SharedArrayBuffer::New(
    Isolate* v8_isolate, std::shared_ptr<BackingStore> backing_store) {
  CHECK(i::v8_flags.harmony_sharedarraybuffer);
  CHECK_IMPLIES(backing_store->ByteLength() != 0,
                backing_store->Data() != nullptr);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, SharedArrayBuffer, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  std::shared_ptr<i::BackingStore> i_backing_store(ToInternal(backing_store));
  Utils::ApiCheck(
      i_backing_store->is_shared(), "v8::SharedArrayBuffer::New",
      "Cannot construct SharedArrayBuffer with BackingStore of ArrayBuffer");
  i::Handle<i::JSArrayBuffer> obj =
      i_isolate->factory()->NewJSSharedArrayBuffer(std::move(i_backing_store));
  return Utils::ToLocalShared(obj);
}

std::unique_ptr<v8::BackingStore> v8::SharedArrayBuffer::NewBackingStore(
    Isolate* v8_isolate, size_t byte_length) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, SharedArrayBuffer, NewBackingStore);
  Utils::ApiCheck(
      byte_length <= i::JSArrayBuffer::kMaxByteLength,
      "v8::SharedArrayBuffer::NewBackingStore",
      "Cannot construct SharedArrayBuffer, requested length is too big");
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::Allocate(i_isolate, byte_length, i::SharedFlag::kShared,
                                i::InitializedFlag::kZeroInitialized);
  if (!backing_store) {
    i::V8::FatalProcessOutOfMemory(i_isolate,
                                   "v8::SharedArrayBuffer::NewBackingStore");
  }
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}

std::unique_ptr<v8::BackingStore> v8::SharedArrayBuffer::NewBackingStore(
    void* data, size_t byte_length, v8::BackingStore::DeleterCallback deleter,
    void* deleter_data) {
  CHECK_LE(byte_length, i::JSArrayBuffer::kMaxByteLength);
  std::unique_ptr<i::BackingStoreBase> backing_store =
      i::BackingStore::WrapAllocation(data, byte_length, deleter, deleter_data,
                                      i::SharedFlag::kShared);
  return std::unique_ptr<v8::BackingStore>(
      static_cast<v8::BackingStore*>(backing_store.release()));
}

Local<Symbol> v8::Symbol::New(Isolate* v8_isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, Symbol, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Symbol> result = i_isolate->factory()->NewSymbol();
  if (!name.IsEmpty()) result->set_description(*Utils::OpenHandle(*name));
  return Utils::ToLocal(result);
}

Local<Symbol> v8::Symbol::For(Isolate* v8_isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::String> i_name = Utils::OpenHandle(*name);
  return Utils::ToLocal(
      i_isolate->SymbolFor(i::RootIndex::kPublicSymbolTable, i_name, false));
}

Local<Symbol> v8::Symbol::ForApi(Isolate* v8_isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
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

#define SYMBOL_GETTER(Name, name)                                      \
  Local<Symbol> v8::Symbol::Get##Name(Isolate* v8_isolate) {           \
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate); \
    return Utils::ToLocal(i_isolate->factory()->name##_symbol());      \
  }

WELL_KNOWN_SYMBOLS(SYMBOL_GETTER)

#undef SYMBOL_GETTER
#undef WELL_KNOWN_SYMBOLS

Local<Private> v8::Private::New(Isolate* v8_isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, Private, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::Symbol> symbol = i_isolate->factory()->NewPrivateSymbol();
  if (!name.IsEmpty()) symbol->set_description(*Utils::OpenHandle(*name));
  Local<Symbol> result = Utils::ToLocal(symbol);
  return v8::Local<Private>(reinterpret_cast<Private*>(*result));
}

Local<Private> v8::Private::ForApi(Isolate* v8_isolate, Local<String> name) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::String> i_name = Utils::OpenHandle(*name);
  Local<Symbol> result = Utils::ToLocal(
      i_isolate->SymbolFor(i::RootIndex::kApiPrivateSymbolTable, i_name, true));
  return v8::Local<Private>(reinterpret_cast<Private*>(*result));
}

Local<Number> v8::Number::New(Isolate* v8_isolate, double value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  if (std::isnan(value)) {
    // Introduce only canonical NaN value into the VM, to avoid signaling NaNs.
    value = std::numeric_limits<double>::quiet_NaN();
  }
  i::Handle<i::Object> result = i_isolate->factory()->NewNumber(value);
  return Utils::NumberToLocal(result);
}

Local<Integer> v8::Integer::New(Isolate* v8_isolate, int32_t value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  if (i::Smi::IsValid(value)) {
    return Utils::IntegerToLocal(
        i::Handle<i::Object>(i::Smi::FromInt(value), i_isolate));
  }
  i::Handle<i::Object> result = i_isolate->factory()->NewNumber(value);
  return Utils::IntegerToLocal(result);
}

Local<Integer> v8::Integer::NewFromUnsigned(Isolate* v8_isolate,
                                            uint32_t value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  bool fits_into_int32_t = (value & (1 << 31)) == 0;
  if (fits_into_int32_t) {
    return Integer::New(v8_isolate, static_cast<int32_t>(value));
  }
  i::Handle<i::Object> result = i_isolate->factory()->NewNumber(value);
  return Utils::IntegerToLocal(result);
}

Local<BigInt> v8::BigInt::New(Isolate* v8_isolate, int64_t value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::BigInt> result = i::BigInt::FromInt64(i_isolate, value);
  return Utils::ToLocal(result);
}

Local<BigInt> v8::BigInt::NewFromUnsigned(Isolate* v8_isolate, uint64_t value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::BigInt> result = i::BigInt::FromUint64(i_isolate, value);
  return Utils::ToLocal(result);
}

MaybeLocal<BigInt> v8::BigInt::NewFromWords(Local<Context> context,
                                            int sign_bit, int word_count,
                                            const uint64_t* words) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  ENTER_V8_NO_SCRIPT(i_isolate, context, BigInt, NewFromWords,
                     MaybeLocal<BigInt>(), InternalEscapableScope);
  i::MaybeHandle<i::BigInt> result =
      i::BigInt::FromWords64(i_isolate, sign_bit, word_count, words);
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

HeapProfiler* Isolate::GetHeapProfiler() {
  i::HeapProfiler* heap_profiler =
      reinterpret_cast<i::Isolate*>(this)->heap_profiler();
  return reinterpret_cast<HeapProfiler*>(heap_profiler);
}

void Isolate::SetIdle(bool is_idle) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetIdle(is_idle);
}

ArrayBuffer::Allocator* Isolate::GetArrayBufferAllocator() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i_isolate->array_buffer_allocator();
}

bool Isolate::InContext() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return !i_isolate->context().is_null();
}

void Isolate::ClearKeptObjects() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->ClearKeptObjects();
}

v8::Local<v8::Context> Isolate::GetCurrentContext() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Context context = i_isolate->context();
  if (context.is_null()) return Local<Context>();
  i::Context native_context = context.native_context();
  if (native_context.is_null()) return Local<Context>();
  return Utils::ToLocal(i::Handle<i::Context>(native_context, i_isolate));
}

v8::Local<v8::Context> Isolate::GetEnteredOrMicrotaskContext() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::Object> last =
      i_isolate->handle_scope_implementer()->LastEnteredOrMicrotaskContext();
  if (last.is_null()) return Local<Context>();
  DCHECK(last->IsNativeContext());
  return Utils::ToLocal(i::Handle<i::Context>::cast(last));
}

v8::Local<v8::Context> Isolate::GetIncumbentContext() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::Context> context = i_isolate->GetIncumbentContext();
  return Utils::ToLocal(context);
}

v8::Local<Value> Isolate::ThrowError(v8::Local<v8::String> message) {
  return ThrowException(v8::Exception::Error(message));
}

v8::Local<Value> Isolate::ThrowException(v8::Local<v8::Value> value) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  ENTER_V8_BASIC(i_isolate);
  // If we're passed an empty handle, we throw an undefined exception
  // to deal more gracefully with out of memory situations.
  if (value.IsEmpty()) {
    i_isolate->ScheduleThrow(i::ReadOnlyRoots(i_isolate).undefined_value());
  } else {
    i_isolate->ScheduleThrow(*Utils::OpenHandle(*value));
  }
  return v8::Undefined(reinterpret_cast<v8::Isolate*>(i_isolate));
}

void Isolate::AddGCPrologueCallback(GCCallbackWithData callback, void* data,
                                    GCType gc_type) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->AddGCPrologueCallback(callback, gc_type, data);
}

void Isolate::RemoveGCPrologueCallback(GCCallbackWithData callback,
                                       void* data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->RemoveGCPrologueCallback(callback, data);
}

void Isolate::AddGCEpilogueCallback(GCCallbackWithData callback, void* data,
                                    GCType gc_type) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->AddGCEpilogueCallback(callback, gc_type, data);
}

void Isolate::RemoveGCEpilogueCallback(GCCallbackWithData callback,
                                       void* data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->RemoveGCEpilogueCallback(callback, data);
}

static void CallGCCallbackWithoutData(Isolate* v8_isolate, GCType type,
                                      GCCallbackFlags flags, void* data) {
  reinterpret_cast<Isolate::GCCallback>(data)(v8_isolate, type, flags);
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

void Isolate::SetEmbedderRootsHandler(EmbedderRootsHandler* handler) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->SetEmbedderRootsHandler(handler);
}

void Isolate::AttachCppHeap(CppHeap* cpp_heap) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->AttachCppHeap(cpp_heap);
}

void Isolate::DetachCppHeap() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->DetachCppHeap();
}

CppHeap* Isolate::GetCppHeap() const {
  const i::Isolate* i_isolate = reinterpret_cast<const i::Isolate*>(this);
  return i_isolate->heap()->cpp_heap();
}

void Isolate::SetGetExternallyAllocatedMemoryInBytesCallback(
    GetExternallyAllocatedMemoryInBytesCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->SetGetExternallyAllocatedMemoryInBytesCallback(callback);
}

void Isolate::TerminateExecution() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->stack_guard()->RequestTerminateExecution();
}

bool Isolate::IsExecutionTerminating() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i_isolate->is_execution_terminating();
}

void Isolate::CancelTerminateExecution() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->stack_guard()->ClearTerminateExecution();
  i_isolate->CancelTerminateExecution();
}

void Isolate::RequestInterrupt(InterruptCallback callback, void* data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->RequestInterrupt(callback, data);
}

bool Isolate::HasPendingBackgroundTasks() {
#if V8_ENABLE_WEBASSEMBLY
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i::wasm::GetWasmEngine()->HasRunningCompileJob(i_isolate);
#else
  return false;
#endif  // V8_ENABLE_WEBASSEMBLY
}

void Isolate::RequestGarbageCollectionForTesting(GarbageCollectionType type) {
  Utils::ApiCheck(i::v8_flags.expose_gc,
                  "v8::Isolate::RequestGarbageCollectionForTesting",
                  "Must use --expose-gc");
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

void Isolate::RequestGarbageCollectionForTesting(GarbageCollectionType type,
                                                 StackState stack_state) {
  base::Optional<i::EmbedderStackStateScope> stack_scope;
  if (type == kFullGarbageCollection) {
    stack_scope.emplace(reinterpret_cast<i::Isolate*>(this)->heap(),
                        i::EmbedderStackStateScope::kExplicitInvocation,
                        stack_state);
  }
  RequestGarbageCollectionForTesting(type);
}

Isolate* Isolate::GetCurrent() {
  i::Isolate* i_isolate = i::Isolate::Current();
  return reinterpret_cast<Isolate*>(i_isolate);
}

Isolate* Isolate::TryGetCurrent() {
  i::Isolate* i_isolate = i::Isolate::TryGetCurrent();
  return reinterpret_cast<Isolate*>(i_isolate);
}

bool Isolate::IsCurrent() const {
  return reinterpret_cast<const i::Isolate*>(this)->IsCurrent();
}

// static
Isolate* Isolate::Allocate() {
  return reinterpret_cast<Isolate*>(i::Isolate::New());
}

Isolate::CreateParams::CreateParams() = default;

Isolate::CreateParams::~CreateParams() = default;

// static
// This is separate so that tests can provide a different |isolate|.
void Isolate::Initialize(Isolate* v8_isolate,
                         const v8::Isolate::CreateParams& params) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  TRACE_EVENT_CALL_STATS_SCOPED(i_isolate, "v8", "V8.IsolateInitialize");
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

  if (params.fatal_error_callback) {
    v8_isolate->SetFatalErrorHandler(params.fatal_error_callback);
  }

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  if (params.oom_error_callback) {
    v8_isolate->SetOOMErrorHandler(params.oom_error_callback);
  }
#if __clang__
#pragma clang diagnostic pop
#endif

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

  i_isolate->set_api_external_references(params.external_references);
  i_isolate->set_allow_atomics_wait(params.allow_atomics_wait);

  i_isolate->heap()->ConfigureHeap(params.constraints);
  if (params.constraints.stack_limit() != nullptr) {
    uintptr_t limit =
        reinterpret_cast<uintptr_t>(params.constraints.stack_limit());
    i_isolate->stack_guard()->SetStackLimit(limit);
  }

  // TODO(v8:2487): Once we got rid of Isolate::Current(), we can remove this.
  Isolate::Scope isolate_scope(v8_isolate);
  if (i_isolate->snapshot_blob() == nullptr) {
    FATAL(
        "V8 snapshot blob was not set during initialization. This can mean "
        "that the snapshot blob file is corrupted or missing.");
  }
  if (!i::Snapshot::Initialize(i_isolate)) {
    // If snapshot data was provided and we failed to deserialize it must
    // have been corrupted.
    FATAL(
        "Failed to deserialize the V8 snapshot blob. This can mean that the "
        "snapshot blob file is corrupted or missing.");
  }

  {
    // Set up code event handlers. Needs to be after i::Snapshot::Initialize
    // because that is where we add the isolate to WasmEngine.
    auto code_event_handler = params.code_event_handler;
    if (code_event_handler) {
      v8_isolate->SetJitCodeEventHandler(kJitCodeEventEnumExisting,
                                         code_event_handler);
    }
  }

  i_isolate->set_only_terminate_in_safe_scope(
      params.only_terminate_in_safe_scope);
  i_isolate->set_embedder_wrapper_type_index(
      params.embedder_wrapper_type_index);
  i_isolate->set_embedder_wrapper_object_index(
      params.embedder_wrapper_object_index);

  if (!i::V8::GetCurrentPlatform()
           ->GetForegroundTaskRunner(v8_isolate)
           ->NonNestableTasksEnabled()) {
    FATAL(
        "The current platform's foreground task runner does not have "
        "non-nestable tasks enabled. The embedder must provide one.");
  }
}

Isolate* Isolate::New(const Isolate::CreateParams& params) {
  Isolate* v8_isolate = Allocate();
  Initialize(v8_isolate, params);
  return v8_isolate;
}

void Isolate::Dispose() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  if (!Utils::ApiCheck(!i_isolate->IsInUse(), "v8::Isolate::Dispose()",
                       "Disposing the isolate that is entered by a thread")) {
    return;
  }
  i::Isolate::Delete(i_isolate);
}

void Isolate::DumpAndResetStats() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->DumpAndResetStats();
}

void Isolate::DiscardThreadSpecificMetadata() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->DiscardPerThreadDataForThisThread();
}

void Isolate::Enter() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->Enter();
}

void Isolate::Exit() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->Exit();
}

void Isolate::SetAbortOnUncaughtExceptionCallback(
    AbortOnUncaughtExceptionCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetAbortOnUncaughtExceptionCallback(callback);
}

void Isolate::SetHostImportModuleDynamicallyCallback(
    HostImportModuleDynamicallyCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetHostImportModuleDynamicallyCallback(callback);
}

void Isolate::SetHostInitializeImportMetaObjectCallback(
    HostInitializeImportMetaObjectCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetHostInitializeImportMetaObjectCallback(callback);
}

void Isolate::SetHostCreateShadowRealmContextCallback(
    HostCreateShadowRealmContextCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetHostCreateShadowRealmContextCallback(callback);
}

void Isolate::SetPrepareStackTraceCallback(PrepareStackTraceCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetPrepareStackTraceCallback(callback);
}

Isolate::DisallowJavascriptExecutionScope::DisallowJavascriptExecutionScope(
    Isolate* v8_isolate,
    Isolate::DisallowJavascriptExecutionScope::OnFailure on_failure)
    : v8_isolate_(v8_isolate), on_failure_(on_failure) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  switch (on_failure_) {
    case CRASH_ON_FAILURE:
      i::DisallowJavascriptExecution::Open(i_isolate, &was_execution_allowed_);
      break;
    case THROW_ON_FAILURE:
      i::ThrowOnJavascriptExecution::Open(i_isolate, &was_execution_allowed_);
      break;
    case DUMP_ON_FAILURE:
      i::DumpOnJavascriptExecution::Open(i_isolate, &was_execution_allowed_);
      break;
  }
}

Isolate::DisallowJavascriptExecutionScope::~DisallowJavascriptExecutionScope() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate_);
  switch (on_failure_) {
    case CRASH_ON_FAILURE:
      i::DisallowJavascriptExecution::Close(i_isolate, was_execution_allowed_);
      break;
    case THROW_ON_FAILURE:
      i::ThrowOnJavascriptExecution::Close(i_isolate, was_execution_allowed_);
      break;
    case DUMP_ON_FAILURE:
      i::DumpOnJavascriptExecution::Close(i_isolate, was_execution_allowed_);
      break;
  }
}

Isolate::AllowJavascriptExecutionScope::AllowJavascriptExecutionScope(
    Isolate* v8_isolate)
    : v8_isolate_(v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::AllowJavascriptExecution::Open(i_isolate, &was_execution_allowed_assert_);
  i::NoThrowOnJavascriptExecution::Open(i_isolate,
                                        &was_execution_allowed_throws_);
  i::NoDumpOnJavascriptExecution::Open(i_isolate, &was_execution_allowed_dump_);
}

Isolate::AllowJavascriptExecutionScope::~AllowJavascriptExecutionScope() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate_);
  i::AllowJavascriptExecution::Close(i_isolate, was_execution_allowed_assert_);
  i::NoThrowOnJavascriptExecution::Close(i_isolate,
                                         was_execution_allowed_throws_);
  i::NoDumpOnJavascriptExecution::Close(i_isolate, was_execution_allowed_dump_);
}

Isolate::SuppressMicrotaskExecutionScope::SuppressMicrotaskExecutionScope(
    Isolate* v8_isolate, MicrotaskQueue* microtask_queue)
    : i_isolate_(reinterpret_cast<i::Isolate*>(v8_isolate)),
      microtask_queue_(microtask_queue
                           ? static_cast<i::MicrotaskQueue*>(microtask_queue)
                           : i_isolate_->default_microtask_queue()) {
  i_isolate_->thread_local_top()->IncrementCallDepth(this);
  microtask_queue_->IncrementMicrotasksSuppressions();
}

Isolate::SuppressMicrotaskExecutionScope::~SuppressMicrotaskExecutionScope() {
  microtask_queue_->DecrementMicrotasksSuppressions();
  i_isolate_->thread_local_top()->DecrementCallDepth(this);
}

Isolate::SafeForTerminationScope::SafeForTerminationScope(
    v8::Isolate* v8_isolate)
    : i_isolate_(reinterpret_cast<i::Isolate*>(v8_isolate)),
      prev_value_(i_isolate_->next_v8_call_is_safe_for_termination()) {
  i_isolate_->set_next_v8_call_is_safe_for_termination(true);
}

Isolate::SafeForTerminationScope::~SafeForTerminationScope() {
  i_isolate_->set_next_v8_call_is_safe_for_termination(prev_value_);
}

i::Address* Isolate::GetDataFromSnapshotOnce(size_t index) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::FixedArray list = i_isolate->heap()->serialized_objects();
  return GetSerializedDataFromFixedArray(i_isolate, list, index);
}

void Isolate::GetHeapStatistics(HeapStatistics* heap_statistics) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = i_isolate->heap();

  // The order of acquiring memory statistics is important here. We query in
  // this order because of concurrent allocation: 1) used memory 2) comitted
  // physical memory 3) committed memory. Therefore the condition used <=
  // committed physical <= committed should hold.
  heap_statistics->used_global_handles_size_ = heap->UsedGlobalHandlesSize();
  heap_statistics->total_global_handles_size_ = heap->TotalGlobalHandlesSize();
  DCHECK_LE(heap_statistics->used_global_handles_size_,
            heap_statistics->total_global_handles_size_);

  heap_statistics->used_heap_size_ = heap->SizeOfObjects();
  heap_statistics->total_physical_size_ = heap->CommittedPhysicalMemory();
  heap_statistics->total_heap_size_ = heap->CommittedMemory();

  heap_statistics->total_available_size_ = heap->Available();

  if (!i::ReadOnlyHeap::IsReadOnlySpaceShared()) {
    i::ReadOnlySpace* ro_space = heap->read_only_space();
    heap_statistics->used_heap_size_ += ro_space->Size();
    heap_statistics->total_physical_size_ +=
        ro_space->CommittedPhysicalMemory();
    heap_statistics->total_heap_size_ += ro_space->CommittedMemory();
  }

  // TODO(dinfuehr): Right now used <= committed physical does not hold. Fix
  // this and add DCHECK.
  DCHECK_LE(heap_statistics->used_heap_size_,
            heap_statistics->total_heap_size_);

  heap_statistics->total_heap_size_executable_ =
      heap->CommittedMemoryExecutable();
  heap_statistics->heap_size_limit_ = heap->MaxReserved();
  // TODO(7424): There is no public API for the {WasmEngine} yet. Once such an
  // API becomes available we should report the malloced memory separately. For
  // now we just add the values, thereby over-approximating the peak slightly.
  heap_statistics->malloced_memory_ =
      i_isolate->allocator()->GetCurrentMemoryUsage() +
      i_isolate->string_table()->GetCurrentMemoryUsage();
  // On 32-bit systems backing_store_bytes() might overflow size_t temporarily
  // due to concurrent array buffer sweeping.
  heap_statistics->external_memory_ =
      i_isolate->heap()->backing_store_bytes() < SIZE_MAX
          ? static_cast<size_t>(i_isolate->heap()->backing_store_bytes())
          : SIZE_MAX;
  heap_statistics->peak_malloced_memory_ =
      i_isolate->allocator()->GetMaxMemoryUsage();
  heap_statistics->number_of_native_contexts_ = heap->NumberOfNativeContexts();
  heap_statistics->number_of_detached_contexts_ =
      heap->NumberOfDetachedContexts();
  heap_statistics->does_zap_garbage_ = heap->ShouldZapGarbage();

#if V8_ENABLE_WEBASSEMBLY
  heap_statistics->malloced_memory_ +=
      i::wasm::GetWasmEngine()->allocator()->GetCurrentMemoryUsage();
  heap_statistics->peak_malloced_memory_ +=
      i::wasm::GetWasmEngine()->allocator()->GetMaxMemoryUsage();
#endif  // V8_ENABLE_WEBASSEMBLY
}

size_t Isolate::NumberOfHeapSpaces() {
  return i::LAST_SPACE - i::FIRST_SPACE + 1;
}

bool Isolate::GetHeapSpaceStatistics(HeapSpaceStatistics* space_statistics,
                                     size_t index) {
  if (!space_statistics) return false;
  if (!i::Heap::IsValidAllocationSpace(static_cast<i::AllocationSpace>(index)))
    return false;

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = i_isolate->heap();

  i::AllocationSpace allocation_space = static_cast<i::AllocationSpace>(index);
  space_statistics->space_name_ = i::BaseSpace::GetSpaceName(allocation_space);

  if (allocation_space == i::RO_SPACE) {
    if (i::ReadOnlyHeap::IsReadOnlySpaceShared()) {
      // RO_SPACE memory is accounted for elsewhere when ReadOnlyHeap is shared.
      space_statistics->space_size_ = 0;
      space_statistics->space_used_size_ = 0;
      space_statistics->space_available_size_ = 0;
      space_statistics->physical_space_size_ = 0;
    } else {
      i::ReadOnlySpace* space = heap->read_only_space();
      space_statistics->space_size_ = space->CommittedMemory();
      space_statistics->space_used_size_ = space->Size();
      space_statistics->space_available_size_ = 0;
      space_statistics->physical_space_size_ = space->CommittedPhysicalMemory();
    }
  } else {
    i::Space* space = heap->space(static_cast<int>(index));
    space_statistics->space_size_ = space ? space->CommittedMemory() : 0;
    space_statistics->space_used_size_ = space ? space->SizeOfObjects() : 0;
    space_statistics->space_available_size_ = space ? space->Available() : 0;
    space_statistics->physical_space_size_ =
        space ? space->CommittedPhysicalMemory() : 0;
  }
  return true;
}

size_t Isolate::NumberOfTrackedHeapObjectTypes() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = i_isolate->heap();
  return heap->NumberOfTrackedHeapObjectTypes();
}

bool Isolate::GetHeapObjectStatisticsAtLastGC(
    HeapObjectStatistics* object_statistics, size_t type_index) {
  if (!object_statistics) return false;
  if (V8_LIKELY(!i::TracingFlags::is_gc_stats_enabled())) return false;

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Heap* heap = i_isolate->heap();
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

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->CollectCodeStatistics();

  code_statistics->code_and_metadata_size_ =
      i_isolate->code_and_metadata_size();
  code_statistics->bytecode_and_metadata_size_ =
      i_isolate->bytecode_and_metadata_size();
  code_statistics->external_script_source_size_ =
      i_isolate->external_script_source_size();
  code_statistics->cpu_profiler_metadata_size_ =
      i::CpuProfiler::GetAllProfilersMemorySize(i_isolate);

  return true;
}

bool Isolate::MeasureMemory(std::unique_ptr<MeasureMemoryDelegate> delegate,
                            MeasureMemoryExecution execution) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i_isolate->heap()->MeasureMemory(std::move(delegate), execution);
}

std::unique_ptr<MeasureMemoryDelegate> MeasureMemoryDelegate::Default(
    Isolate* v8_isolate, Local<Context> context,
    Local<Promise::Resolver> promise_resolver, MeasureMemoryMode mode) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  if (i::TickSample::GetStackSample(i_isolate, &regs,
                                    i::TickSample::kSkipCEntryFrame, frames,
                                    frames_limit, sample_info)) {
    return;
  }
  sample_info->frames_count = 0;
  sample_info->vm_state = OTHER;
  sample_info->external_callback_entry = nullptr;
}

int64_t Isolate::AdjustAmountOfExternalAllocatedMemory(
    int64_t change_in_bytes) {
  // Try to check for unreasonably large or small values from the embedder.
  const int64_t kMaxReasonableBytes = int64_t(1) << 60;
  const int64_t kMinReasonableBytes = -kMaxReasonableBytes;
  static_assert(kMaxReasonableBytes >= i::JSArrayBuffer::kMaxByteLength);

  CHECK(kMinReasonableBytes <= change_in_bytes &&
        change_in_bytes < kMaxReasonableBytes);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  int64_t amount = i_isolate->heap()->update_external_memory(change_in_bytes);

  if (change_in_bytes <= 0) return amount;

  if (amount > i_isolate->heap()->external_memory_limit()) {
    ReportExternalAllocationLimitReached();
  }
  return amount;
}

void Isolate::SetEventLogger(LogEventCallback that) {
  // Do not overwrite the event logger if we want to log explicitly.
  if (i::v8_flags.log_internal_timer_events) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->set_event_logger(that);
}

void Isolate::AddBeforeCallEnteredCallback(BeforeCallEnteredCallback callback) {
  if (callback == nullptr) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->AddBeforeCallEnteredCallback(callback);
}

void Isolate::RemoveBeforeCallEnteredCallback(
    BeforeCallEnteredCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->RemoveBeforeCallEnteredCallback(callback);
}

void Isolate::AddCallCompletedCallback(CallCompletedCallback callback) {
  if (callback == nullptr) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->AddCallCompletedCallback(callback);
}

void Isolate::RemoveCallCompletedCallback(CallCompletedCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->RemoveCallCompletedCallback(callback);
}

void Isolate::AtomicsWaitWakeHandle::Wake() {
  reinterpret_cast<i::AtomicsWaitWakeHandle*>(this)->Wake();
}

void Isolate::SetAtomicsWaitCallback(AtomicsWaitCallback callback, void* data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetAtomicsWaitCallback(callback, data);
}

void Isolate::SetPromiseHook(PromiseHook hook) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetPromiseHook(hook);
}

void Isolate::SetPromiseRejectCallback(PromiseRejectCallback callback) {
  if (callback == nullptr) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetPromiseRejectCallback(callback);
}

void Isolate::PerformMicrotaskCheckpoint() {
  DCHECK_NE(MicrotasksPolicy::kScoped, GetMicrotasksPolicy());
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->default_microtask_queue()->PerformCheckpoint(this);
}

void Isolate::EnqueueMicrotask(Local<Function> v8_function) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i::Handle<i::JSReceiver> function = Utils::OpenHandle(*v8_function);
  i::Handle<i::NativeContext> handler_context;
  if (!i::JSReceiver::GetContextForMicrotask(function).ToHandle(
          &handler_context))
    handler_context = i_isolate->native_context();
  MicrotaskQueue* microtask_queue = handler_context->microtask_queue();
  if (microtask_queue) microtask_queue->EnqueueMicrotask(this, v8_function);
}

void Isolate::EnqueueMicrotask(MicrotaskCallback callback, void* data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->default_microtask_queue()->EnqueueMicrotask(this, callback, data);
}

void Isolate::SetMicrotasksPolicy(MicrotasksPolicy policy) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->default_microtask_queue()->set_microtasks_policy(policy);
}

MicrotasksPolicy Isolate::GetMicrotasksPolicy() const {
  i::Isolate* i_isolate =
      reinterpret_cast<i::Isolate*>(const_cast<Isolate*>(this));
  return i_isolate->default_microtask_queue()->microtasks_policy();
}

void Isolate::AddMicrotasksCompletedCallback(
    MicrotasksCompletedCallbackWithData callback, void* data) {
  DCHECK(callback);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->default_microtask_queue()->AddMicrotasksCompletedCallback(callback,
                                                                       data);
}

void Isolate::RemoveMicrotasksCompletedCallback(
    MicrotasksCompletedCallbackWithData callback, void* data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->default_microtask_queue()->RemoveMicrotasksCompletedCallback(
      callback, data);
}

void Isolate::SetUseCounterCallback(UseCounterCallback callback) {
  reinterpret_cast<i::Isolate*>(this)->SetUseCounterCallback(callback);
}

void Isolate::SetCounterFunction(CounterLookupCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->counters()->ResetCounterFunction(callback);
}

void Isolate::SetCreateHistogramFunction(CreateHistogramCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->counters()->ResetCreateHistogramFunction(callback);
}

void Isolate::SetAddHistogramSampleFunction(
    AddHistogramSampleCallback callback) {
  reinterpret_cast<i::Isolate*>(this)
      ->counters()
      ->SetAddHistogramSampleFunction(callback);
}

void Isolate::SetMetricsRecorder(
    const std::shared_ptr<metrics::Recorder>& metrics_recorder) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->metrics_recorder()->SetEmbedderRecorder(i_isolate,
                                                     metrics_recorder);
}

void Isolate::SetAddCrashKeyCallback(AddCrashKeyCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetAddCrashKeyCallback(callback);
}

bool Isolate::IdleNotificationDeadline(double deadline_in_seconds) {
  // Returning true tells the caller that it need not
  // continue to call IdleNotification.
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  if (!i::v8_flags.use_idle_notification) return true;
  return i_isolate->heap()->IdleNotification(deadline_in_seconds);
}

void Isolate::LowMemoryNotification() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  {
    i::NestedTimedHistogramScope idle_notification_scope(
        i_isolate->counters()->gc_low_memory_notification());
    TRACE_EVENT0("v8", "V8.GCLowMemoryNotification");
    i_isolate->heap()->CollectAllAvailableGarbage(
        i::GarbageCollectionReason::kLowMemoryNotification);
  }
}

int Isolate::ContextDisposedNotification(bool dependant_context) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
#if V8_ENABLE_WEBASSEMBLY
  if (!dependant_context) {
    if (!i_isolate->context().is_null()) {
      // We left the current context, we can abort all WebAssembly compilations
      // of that context.
      // A handle scope for the native context.
      i::HandleScope handle_scope(i_isolate);
      i::wasm::GetWasmEngine()->DeleteCompileJobsOnContext(
          i_isolate->native_context());
    }
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  // TODO(ahaas): move other non-heap activity out of the heap call.
  return i_isolate->heap()->NotifyContextDisposed(dependant_context);
}

void Isolate::IsolateInForegroundNotification() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i_isolate->IsolateInForegroundNotification();
}

void Isolate::IsolateInBackgroundNotification() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i_isolate->IsolateInBackgroundNotification();
}

void Isolate::MemoryPressureNotification(MemoryPressureLevel level) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  bool on_isolate_thread =
      i_isolate->was_locker_ever_used()
          ? i_isolate->thread_manager()->IsLockedByCurrentThread()
          : i::ThreadId::Current() == i_isolate->thread_id();
  i_isolate->heap()->MemoryPressureNotification(level, on_isolate_thread);
}

void Isolate::ClearCachesForTesting() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->AbortConcurrentOptimization(i::BlockingBehavior::kBlock);
  i_isolate->ClearSerializerData();
  i_isolate->compilation_cache()->Clear();
}

void Isolate::EnableMemorySavingsMode() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->EnableMemorySavingsMode();
}

void Isolate::DisableMemorySavingsMode() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->DisableMemorySavingsMode();
}

void Isolate::SetRAILMode(RAILMode rail_mode) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i_isolate->SetRAILMode(rail_mode);
}

void Isolate::UpdateLoadStartTime() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->UpdateLoadStartTime();
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  // Ensure that logging is initialized for our isolate.
  i_isolate->InitializeLoggingAndCounters();
  i_isolate->v8_file_logger()->SetCodeEventHandler(options, event_handler);
}

void Isolate::SetStackLimit(uintptr_t stack_limit) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  CHECK(stack_limit);
  i_isolate->stack_guard()->SetStackLimit(stack_limit);
}

void Isolate::GetCodeRange(void** start, size_t* length_in_bytes) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  const base::AddressRegion& code_region = i_isolate->heap()->code_region();
  *start = reinterpret_cast<void*>(code_region.begin());
  *length_in_bytes = code_region.size();
}

void Isolate::GetEmbeddedCodeRange(const void** start,
                                   size_t* length_in_bytes) {
  // Note, we should return the embedded code rande from the .text section here.
  i::EmbeddedData d = i::EmbeddedData::FromBlob();
  *start = reinterpret_cast<const void*>(d.code());
  *length_in_bytes = d.code_size();
}

JSEntryStubs Isolate::GetJSEntryStubs() {
  JSEntryStubs entry_stubs;

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  std::array<std::pair<i::Builtin, JSEntryStub*>, 3> stubs = {
      {{i::Builtin::kJSEntry, &entry_stubs.js_entry_stub},
       {i::Builtin::kJSConstructEntry, &entry_stubs.js_construct_entry_stub},
       {i::Builtin::kJSRunMicrotasksEntry,
        &entry_stubs.js_run_microtasks_entry_stub}}};
  for (auto& pair : stubs) {
    i::Code js_entry = i_isolate->builtins()->code(pair.first);
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

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  std::vector<MemoryRange>* code_pages = i_isolate->GetCodePages();

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

#define CALLBACK_SETTER(ExternalName, Type, InternalName)        \
  void Isolate::Set##ExternalName(Type callback) {               \
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this); \
    i_isolate->set_##InternalName(callback);                     \
  }

CALLBACK_SETTER(FatalErrorHandler, FatalErrorCallback, exception_behavior)
CALLBACK_SETTER(OOMErrorHandler, OOMErrorCallback, oom_behavior)
CALLBACK_SETTER(ModifyCodeGenerationFromStringsCallback,
                ModifyCodeGenerationFromStringsCallback2,
                modify_code_gen_callback2)
CALLBACK_SETTER(AllowWasmCodeGenerationCallback,
                AllowWasmCodeGenerationCallback, allow_wasm_code_gen_callback)

CALLBACK_SETTER(WasmModuleCallback, ExtensionCallback, wasm_module_callback)
CALLBACK_SETTER(WasmInstanceCallback, ExtensionCallback, wasm_instance_callback)

CALLBACK_SETTER(WasmStreamingCallback, WasmStreamingCallback,
                wasm_streaming_callback)

CALLBACK_SETTER(WasmAsyncResolvePromiseCallback,
                WasmAsyncResolvePromiseCallback,
                wasm_async_resolve_promise_callback)

CALLBACK_SETTER(WasmLoadSourceMapCallback, WasmLoadSourceMapCallback,
                wasm_load_source_map_callback)

CALLBACK_SETTER(WasmGCEnabledCallback, WasmGCEnabledCallback,
                wasm_gc_enabled_callback)

CALLBACK_SETTER(SharedArrayBufferConstructorEnabledCallback,
                SharedArrayBufferConstructorEnabledCallback,
                sharedarraybuffer_constructor_enabled_callback)

void Isolate::SetWasmExceptionsEnabledCallback(
    WasmExceptionsEnabledCallback callback) {
  // Exceptions are always enabled
}

void Isolate::SetWasmSimdEnabledCallback(WasmSimdEnabledCallback callback) {
  // SIMD is always enabled
}

void Isolate::InstallConditionalFeatures(Local<Context> context) {
  v8::HandleScope handle_scope(this);
  v8::Context::Scope context_scope(context);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  if (i_isolate->is_execution_terminating()) return;
  i_isolate->InstallConditionalFeatures(Utils::OpenHandle(*context));
#if V8_ENABLE_WEBASSEMBLY
  if (i::v8_flags.expose_wasm && !i_isolate->has_pending_exception()) {
    i::WasmJs::InstallConditionalFeatures(i_isolate,
                                          Utils::OpenHandle(*context));
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  if (i_isolate->has_pending_exception()) {
    i_isolate->OptionalRescheduleException(false);
  }
}

void Isolate::AddNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                       void* data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->AddNearHeapLimitCallback(callback, data);
}

void Isolate::RemoveNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                          size_t heap_limit) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->RemoveNearHeapLimitCallback(callback, heap_limit);
}

void Isolate::AutomaticallyRestoreInitialHeapLimit(double threshold_percent) {
  DCHECK_GT(threshold_percent, 0.0);
  DCHECK_LT(threshold_percent, 1.0);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->AutomaticallyRestoreInitialHeapLimit(threshold_percent);
}

bool Isolate::IsDead() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i_isolate->IsDead();
}

bool Isolate::AddMessageListener(MessageCallback that, Local<Value> data) {
  return AddMessageListenerWithErrorLevel(that, kMessageError, data);
}

bool Isolate::AddMessageListenerWithErrorLevel(MessageCallback that,
                                               int message_levels,
                                               Local<Value> data) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  i::Handle<i::TemplateList> list = i_isolate->factory()->message_listeners();
  i::Handle<i::FixedArray> listener = i_isolate->factory()->NewFixedArray(3);
  i::Handle<i::Foreign> foreign =
      i_isolate->factory()->NewForeign(FUNCTION_ADDR(that));
  listener->set(0, *foreign);
  listener->set(1, data.IsEmpty()
                       ? i::ReadOnlyRoots(i_isolate).undefined_value()
                       : *Utils::OpenHandle(*data));
  listener->set(2, i::Smi::FromInt(message_levels));
  list = i::TemplateList::Add(i_isolate, list, listener);
  i_isolate->heap()->SetMessageListeners(*list);
  return true;
}

void Isolate::RemoveMessageListeners(MessageCallback that) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  i::DisallowGarbageCollection no_gc;
  i::TemplateList listeners = i_isolate->heap()->message_listeners();
  for (int i = 0; i < listeners.length(); i++) {
    if (listeners.get(i).IsUndefined(i_isolate)) continue;  // skip deleted ones
    i::FixedArray listener = i::FixedArray::cast(listeners.get(i));
    i::Foreign callback_obj = i::Foreign::cast(listener.get(0));
    if (callback_obj.foreign_address() == FUNCTION_ADDR(that)) {
      listeners.set(i, i::ReadOnlyRoots(i_isolate).undefined_value());
    }
  }
}

void Isolate::SetFailedAccessCheckCallbackFunction(
    FailedAccessCheckCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetFailedAccessCheckCallback(callback);
}

void Isolate::SetCaptureStackTraceForUncaughtExceptions(
    bool capture, int frame_limit, StackTrace::StackTraceOptions options) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->SetCaptureStackTraceForUncaughtExceptions(capture, frame_limit,
                                                       options);
}

void Isolate::VisitExternalResources(ExternalResourceVisitor* visitor) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->heap()->VisitExternalResources(visitor);
}

bool Isolate::IsInUse() {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  return i_isolate->IsInUse();
}

void Isolate::SetAllowAtomicsWait(bool allow) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  i_isolate->set_allow_atomics_wait(allow);
}

void v8::Isolate::DateTimeConfigurationChangeNotification(
    TimeZoneDetection time_zone_detection) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(this);
  API_RCS_SCOPE(i_isolate, Isolate, DateTimeConfigurationChangeNotification);
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
  API_RCS_SCOPE(i_isolate, Isolate, LocaleConfigurationChangeNotification);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);

#ifdef V8_INTL_SUPPORT
  i_isolate->ResetDefaultLocale();
#endif  // V8_INTL_SUPPORT
}

bool v8::Object::IsCodeLike(v8::Isolate* v8_isolate) const {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  API_RCS_SCOPE(i_isolate, Object, IsCodeLike);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  return Utils::OpenHandle(this)->IsCodeLike(i_isolate);
}

// static
std::unique_ptr<MicrotaskQueue> MicrotaskQueue::New(Isolate* v8_isolate,
                                                    MicrotasksPolicy policy) {
  auto microtask_queue =
      i::MicrotaskQueue::New(reinterpret_cast<i::Isolate*>(v8_isolate));
  microtask_queue->set_microtasks_policy(policy);
  std::unique_ptr<MicrotaskQueue> ret(std::move(microtask_queue));
  return ret;
}

MicrotasksScope::MicrotasksScope(Isolate* v8_isolate,
                                 MicrotasksScope::Type type)
    : MicrotasksScope(v8_isolate, nullptr, type) {}

MicrotasksScope::MicrotasksScope(Local<Context> v8_context,
                                 MicrotasksScope::Type type)
    : MicrotasksScope(v8_context->GetIsolate(), v8_context->GetMicrotaskQueue(),
                      type) {}

MicrotasksScope::MicrotasksScope(Isolate* v8_isolate,
                                 MicrotaskQueue* microtask_queue,
                                 MicrotasksScope::Type type)
    : i_isolate_(reinterpret_cast<i::Isolate*>(v8_isolate)),
      microtask_queue_(microtask_queue
                           ? static_cast<i::MicrotaskQueue*>(microtask_queue)
                           : i_isolate_->default_microtask_queue()),
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
        !i_isolate_->has_scheduled_exception()) {
      microtask_queue_->PerformCheckpoint(
          reinterpret_cast<Isolate*>(i_isolate_));
      DCHECK_IMPLIES(i_isolate_->has_scheduled_exception(),
                     i_isolate_->is_execution_terminating());
    }
  }
#ifdef DEBUG
  if (!run_) microtask_queue_->DecrementDebugMicrotasksScopeDepth();
#endif
}

// static
void MicrotasksScope::PerformCheckpoint(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  auto* microtask_queue = i_isolate->default_microtask_queue();
  microtask_queue->PerformCheckpoint(v8_isolate);
}

// static
int MicrotasksScope::GetCurrentDepth(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  auto* microtask_queue = i_isolate->default_microtask_queue();
  return microtask_queue->GetMicrotasksScopeDepth();
}

// static
bool MicrotasksScope::IsRunningMicrotasks(Isolate* v8_isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  auto* microtask_queue = i_isolate->default_microtask_queue();
  return microtask_queue->IsRunningMicrotasks();
}

String::Utf8Value::Utf8Value(v8::Isolate* v8_isolate, v8::Local<v8::Value> obj)
    : str_(nullptr), length_(0) {
  if (obj.IsEmpty()) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  Local<Context> context = v8_isolate->GetCurrentContext();
  ENTER_V8_BASIC(i_isolate);
  i::HandleScope scope(i_isolate);
  TryCatch try_catch(v8_isolate);
  Local<String> str;
  if (!obj->ToString(context).ToLocal(&str)) return;
  length_ = str->Utf8Length(v8_isolate);
  str_ = i::NewArray<char>(length_ + 1);
  str->WriteUtf8(v8_isolate, str_);
}

String::Utf8Value::~Utf8Value() { i::DeleteArray(str_); }

String::Value::Value(v8::Isolate* v8_isolate, v8::Local<v8::Value> obj)
    : str_(nullptr), length_(0) {
  if (obj.IsEmpty()) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::HandleScope scope(i_isolate);
  Local<Context> context = v8_isolate->GetCurrentContext();
  ENTER_V8_BASIC(i_isolate);
  TryCatch try_catch(v8_isolate);
  Local<String> str;
  if (!obj->ToString(context).ToLocal(&str)) return;
  length_ = str->Length();
  str_ = i::NewArray<uint16_t>(length_ + 1);
  str->Write(v8_isolate, str_);
}

String::Value::~Value() { i::DeleteArray(str_); }

#define DEFINE_ERROR(NAME, name)                                           \
  Local<Value> Exception::NAME(v8::Local<v8::String> raw_message) {        \
    i::Isolate* i_isolate = i::Isolate::Current();                         \
    API_RCS_SCOPE(i_isolate, NAME, New);                                   \
    ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);                            \
    i::Object error;                                                       \
    {                                                                      \
      i::HandleScope scope(i_isolate);                                     \
      i::Handle<i::String> message = Utils::OpenHandle(*raw_message);      \
      i::Handle<i::JSFunction> constructor = i_isolate->name##_function(); \
      error = *i_isolate->factory()->NewError(constructor, message);       \
    }                                                                      \
    i::Handle<i::Object> result(error, i_isolate);                         \
    return Utils::ToLocal(result);                                         \
  }

DEFINE_ERROR(RangeError, range_error)
DEFINE_ERROR(ReferenceError, reference_error)
DEFINE_ERROR(SyntaxError, syntax_error)
DEFINE_ERROR(TypeError, type_error)
DEFINE_ERROR(WasmCompileError, wasm_compile_error)
DEFINE_ERROR(WasmLinkError, wasm_link_error)
DEFINE_ERROR(WasmRuntimeError, wasm_runtime_error)
DEFINE_ERROR(Error, error)

#undef DEFINE_ERROR

Local<Message> Exception::CreateMessage(Isolate* v8_isolate,
                                        Local<Value> exception) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*exception);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::HandleScope scope(i_isolate);
  return Utils::MessageToLocal(
      scope.CloseAndEscape(i_isolate->CreateMessage(obj, nullptr)));
}

Local<StackTrace> Exception::GetStackTrace(Local<Value> exception) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*exception);
  if (!obj->IsJSObject()) return Local<StackTrace>();
  i::Handle<i::JSObject> js_obj = i::Handle<i::JSObject>::cast(obj);
  i::Isolate* i_isolate = js_obj->GetIsolate();
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  return Utils::StackTraceToLocal(i_isolate->GetDetailedStackTrace(js_obj));
}

v8::MaybeLocal<v8::Array> v8::Object::PreviewEntries(bool* is_key_value) {
  i::Handle<i::JSReceiver> object = Utils::OpenHandle(this);
  i::Isolate* i_isolate = object->GetIsolate();
  if (i_isolate->is_execution_terminating()) return {};
  if (IsMap()) {
    *is_key_value = true;
    return Map::Cast(this)->AsArray();
  }
  if (IsSet()) {
    *is_key_value = false;
    return Set::Cast(this)->AsArray();
  }

  Isolate* v8_isolate = reinterpret_cast<Isolate*>(i_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
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
        MapAsArray(i_isolate, it->table(), i::Smi::ToInt(it->index()), kind));
  }
  if (object->IsJSSetIterator()) {
    i::Handle<i::JSSetIterator> it = i::Handle<i::JSSetIterator>::cast(object);
    SetAsArrayKind const kind =
        static_cast<SetAsArrayKind>(it->map().instance_type());
    *is_key_value = kind == SetAsArrayKind::kEntries;
    if (!it->HasMore()) return v8::Array::New(v8_isolate);
    return Utils::ToLocal(
        SetAsArray(i_isolate, it->table(), i::Smi::ToInt(it->index()), kind));
  }
  return v8::MaybeLocal<v8::Array>();
}

Local<String> CpuProfileNode::GetFunctionName() const {
  const i::ProfileNode* node = reinterpret_cast<const i::ProfileNode*>(this);
  i::Isolate* i_isolate = node->isolate();
  const i::CodeEntry* entry = node->entry();
  i::Handle<i::String> name =
      i_isolate->factory()->InternalizeUtf8String(entry->name());
  return ToApiHandle<String>(name);
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
  i::Isolate* i_isolate = node->isolate();
  return ToApiHandle<String>(i_isolate->factory()->InternalizeUtf8String(
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
  i::Isolate* i_isolate = profile->top_down()->isolate();
  return ToApiHandle<String>(
      i_isolate->factory()->InternalizeUtf8String(profile->title()));
}

const CpuProfileNode* CpuProfile::GetTopDownRoot() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->top_down()->root());
}

const CpuProfileNode* CpuProfile::GetSample(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return reinterpret_cast<const CpuProfileNode*>(profile->sample(index).node);
}

const int CpuProfileNode::kNoLineNumberInfo;
const int CpuProfileNode::kNoColumnNumberInfo;

int64_t CpuProfile::GetSampleTimestamp(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return profile->sample(index).timestamp.since_origin().InMicroseconds();
}

StateTag CpuProfile::GetSampleState(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return profile->sample(index).state_tag;
}

EmbedderStateTag CpuProfile::GetSampleEmbedderState(int index) const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return profile->sample(index).embedder_state_tag;
}

int64_t CpuProfile::GetStartTime() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return profile->start_time().since_origin().InMicroseconds();
}

int64_t CpuProfile::GetEndTime() const {
  const i::CpuProfile* profile = reinterpret_cast<const i::CpuProfile*>(this);
  return profile->end_time().since_origin().InMicroseconds();
}

static i::CpuProfile* ToInternal(const CpuProfile* profile) {
  return const_cast<i::CpuProfile*>(
      reinterpret_cast<const i::CpuProfile*>(profile));
}

void CpuProfile::Serialize(OutputStream* stream,
                           CpuProfile::SerializationFormat format) const {
  Utils::ApiCheck(format == kJSON, "v8::CpuProfile::Serialize",
                  "Unknown serialization format");
  Utils::ApiCheck(stream->GetChunkSize() > 0, "v8::CpuProfile::Serialize",
                  "Invalid stream chunk size");
  i::CpuProfileJSONSerializer serializer(ToInternal(this));
  serializer.Serialize(stream);
}

int CpuProfile::GetSamplesCount() const {
  return reinterpret_cast<const i::CpuProfile*>(this)->samples_count();
}

CpuProfiler* CpuProfiler::New(Isolate* v8_isolate,
                              CpuProfilingNamingMode naming_mode,
                              CpuProfilingLoggingMode logging_mode) {
  return reinterpret_cast<CpuProfiler*>(new i::CpuProfiler(
      reinterpret_cast<i::Isolate*>(v8_isolate), naming_mode, logging_mode));
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
    filter_context_.SetWeak();
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
void CpuProfiler::CollectSample(Isolate* v8_isolate) {
  i::CpuProfiler::CollectSample(reinterpret_cast<i::Isolate*>(v8_isolate));
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

CpuProfilingResult CpuProfiler::Start(
    CpuProfilingOptions options,
    std::unique_ptr<DiscardedSamplesDelegate> delegate) {
  return reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      std::move(options), std::move(delegate));
}

CpuProfilingResult CpuProfiler::Start(
    Local<String> title, CpuProfilingOptions options,
    std::unique_ptr<DiscardedSamplesDelegate> delegate) {
  return reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      *Utils::OpenHandle(*title), std::move(options), std::move(delegate));
}

CpuProfilingResult CpuProfiler::Start(Local<String> title,
                                      bool record_samples) {
  CpuProfilingOptions options(
      kLeafNodeLineNumbers,
      record_samples ? CpuProfilingOptions::kNoSampleLimit : 0);
  return reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      *Utils::OpenHandle(*title), std::move(options));
}

CpuProfilingResult CpuProfiler::Start(Local<String> title,
                                      CpuProfilingMode mode,
                                      bool record_samples,
                                      unsigned max_samples) {
  CpuProfilingOptions options(mode, record_samples ? max_samples : 0);
  return reinterpret_cast<i::CpuProfiler*>(this)->StartProfiling(
      *Utils::OpenHandle(*title), std::move(options));
}

CpuProfilingStatus CpuProfiler::StartProfiling(
    Local<String> title, CpuProfilingOptions options,
    std::unique_ptr<DiscardedSamplesDelegate> delegate) {
  return Start(title, std::move(options), std::move(delegate)).status;
}

CpuProfilingStatus CpuProfiler::StartProfiling(Local<String> title,
                                               bool record_samples) {
  return Start(title, record_samples).status;
}

CpuProfilingStatus CpuProfiler::StartProfiling(Local<String> title,
                                               CpuProfilingMode mode,
                                               bool record_samples,
                                               unsigned max_samples) {
  return Start(title, mode, record_samples, max_samples).status;
}

CpuProfile* CpuProfiler::StopProfiling(Local<String> title) {
  return reinterpret_cast<CpuProfile*>(
      reinterpret_cast<i::CpuProfiler*>(this)->StopProfiling(
          *Utils::OpenHandle(*title)));
}

CpuProfile* CpuProfiler::Stop(ProfilerId id) {
  return reinterpret_cast<CpuProfile*>(
      reinterpret_cast<i::CpuProfiler*>(this)->StopProfiling(id));
}

void CpuProfiler::UseDetailedSourcePositionsForProfiling(Isolate* v8_isolate) {
  reinterpret_cast<i::Isolate*>(v8_isolate)
      ->SetDetailedSourcePositionsForProfiling(true);
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
}

CodeEventHandler::CodeEventHandler(Isolate* v8_isolate) {
  internal_listener_ = new i::ExternalLogEventListener(
      reinterpret_cast<i::Isolate*>(v8_isolate));
}

CodeEventHandler::~CodeEventHandler() {
  delete reinterpret_cast<i::ExternalLogEventListener*>(internal_listener_);
}

void CodeEventHandler::Enable() {
  reinterpret_cast<i::ExternalLogEventListener*>(internal_listener_)
      ->StartListening(this);
}

void CodeEventHandler::Disable() {
  reinterpret_cast<i::ExternalLogEventListener*>(internal_listener_)
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
  i::Isolate* i_isolate = edge->isolate();
  switch (edge->type()) {
    case i::HeapGraphEdge::kContextVariable:
    case i::HeapGraphEdge::kInternal:
    case i::HeapGraphEdge::kProperty:
    case i::HeapGraphEdge::kShortcut:
    case i::HeapGraphEdge::kWeak:
      return ToApiHandle<String>(
          i_isolate->factory()->InternalizeUtf8String(edge->name()));
    case i::HeapGraphEdge::kElement:
    case i::HeapGraphEdge::kHidden:
      return ToApiHandle<Number>(
          i_isolate->factory()->NewNumberFromInt(edge->index()));
    default:
      UNREACHABLE();
  }
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
  i::Isolate* i_isolate = ToInternal(this)->isolate();
  return ToApiHandle<String>(
      i_isolate->factory()->InternalizeUtf8String(ToInternal(this)->name()));
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
  i::Isolate* i_isolate = ToInternal(this)->profiler()->isolate();
  if (i_isolate->heap_profiler()->GetSnapshotsCount() > 1 ||
      i_isolate->heap_profiler()->IsTakingSnapshot()) {
    ToInternal(this)->Delete();
  } else {
    // If this is the last snapshot, clean up all accessory data as well.
    i_isolate->heap_profiler()->DeleteAllSnapshots();
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

void HeapProfiler::QueryObjects(Local<Context> v8_context,
                                QueryObjectPredicate* predicate,
                                std::vector<Global<Object>>* objects) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_context->GetIsolate());
  i::HeapProfiler* profiler = reinterpret_cast<i::HeapProfiler*>(this);
  DCHECK_EQ(isolate, profiler->isolate());
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  profiler->QueryObjects(Utils::OpenHandle(*v8_context), predicate, objects);
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
    const HeapSnapshotOptions& options) {
  return reinterpret_cast<const HeapSnapshot*>(
      reinterpret_cast<i::HeapProfiler*>(this)->TakeSnapshot(options));
}

const HeapSnapshot* HeapProfiler::TakeHeapSnapshot(ActivityControl* control,
                                                   ObjectNameResolver* resolver,
                                                   bool hide_internals,
                                                   bool capture_numeric_value) {
  HeapSnapshotOptions options;
  options.control = control;
  options.global_object_name_resolver = resolver;
  options.snapshot_mode = hide_internals ? HeapSnapshotMode::kRegular
                                         : HeapSnapshotMode::kExposeInternals;
  options.numerics_mode = capture_numeric_value
                              ? NumericsMode::kExposeNumericValues
                              : NumericsMode::kHideNumericValues;
  return TakeHeapSnapshot(options);
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

void HeapProfiler::SetGetDetachednessCallback(GetDetachednessCallback callback,
                                              void* data) {
  reinterpret_cast<i::HeapProfiler*>(this)->SetGetDetachednessCallback(callback,
                                                                       data);
}

EmbedderStateScope::EmbedderStateScope(Isolate* v8_isolate,
                                       Local<v8::Context> context,
                                       EmbedderStateTag tag)
    : embedder_state_(new internal::EmbedderState(v8_isolate, context, tag)) {}

// std::unique_ptr's destructor is not compatible with Forward declared
// EmbedderState class.
// Default destructor must be defined in implementation file.
EmbedderStateScope::~EmbedderStateScope() = default;

void TracedReferenceBase::CheckValue() const {
#ifdef V8_HOST_ARCH_64_BIT
  if (!val_) return;

  CHECK_NE(internal::kGlobalHandleZapValue, *reinterpret_cast<uint64_t*>(val_));
#endif  // V8_HOST_ARCH_64_BIT
}

CFunction::CFunction(const void* address, const CFunctionInfo* type_info)
    : address_(address), type_info_(type_info) {
  CHECK_NOT_NULL(address_);
  CHECK_NOT_NULL(type_info_);
}

CFunctionInfo::CFunctionInfo(const CTypeInfo& return_info,
                             unsigned int arg_count, const CTypeInfo* arg_info)
    : return_info_(return_info), arg_count_(arg_count), arg_info_(arg_info) {
  if (arg_count_ > 0) {
    for (unsigned int i = 0; i < arg_count_ - 1; ++i) {
      DCHECK(arg_info_[i].GetType() != CTypeInfo::kCallbackOptionsType);
    }
  }
}

const CTypeInfo& CFunctionInfo::ArgumentInfo(unsigned int index) const {
  DCHECK_LT(index, ArgumentCount());
  return arg_info_[index];
}

void FastApiTypedArrayBase::ValidateIndex(size_t index) const {
  DCHECK_LT(index, length_);
}

RegisterState::RegisterState()
    : pc(nullptr), sp(nullptr), fp(nullptr), lr(nullptr) {}
RegisterState::~RegisterState() = default;

RegisterState::RegisterState(const RegisterState& other) { *this = other; }

RegisterState& RegisterState::operator=(const RegisterState& other) {
  if (&other != this) {
    pc = other.pc;
    sp = other.sp;
    fp = other.fp;
    lr = other.lr;
    if (other.callee_saved) {
      // Make a deep copy if {other.callee_saved} is non-null.
      callee_saved =
          std::make_unique<CalleeSavedRegisters>(*(other.callee_saved));
    } else {
      // Otherwise, set {callee_saved} to null to match {other}.
      callee_saved.reset();
    }
  }
  return *this;
}

#if !V8_ENABLE_WEBASSEMBLY
// If WebAssembly is disabled, we still need to provide an implementation of the
// WasmStreaming API. Since {WasmStreaming::Unpack} will always fail, all
// methods are unreachable.

class WasmStreaming::WasmStreamingImpl {};

WasmStreaming::WasmStreaming(std::unique_ptr<WasmStreamingImpl>) {
  UNREACHABLE();
}

WasmStreaming::~WasmStreaming() = default;

void WasmStreaming::OnBytesReceived(const uint8_t* bytes, size_t size) {
  UNREACHABLE();
}

void WasmStreaming::Finish(bool can_use_compiled_module) { UNREACHABLE(); }

void WasmStreaming::Abort(MaybeLocal<Value> exception) { UNREACHABLE(); }

bool WasmStreaming::SetCompiledModuleBytes(const uint8_t* bytes, size_t size) {
  UNREACHABLE();
}

void WasmStreaming::SetMoreFunctionsCanBeSerializedCallback(
    std::function<void(CompiledWasmModule)>) {
  UNREACHABLE();
}

void WasmStreaming::SetUrl(const char* url, size_t length) { UNREACHABLE(); }

// static
std::shared_ptr<WasmStreaming> WasmStreaming::Unpack(Isolate* v8_isolate,
                                                     Local<Value> value) {
  FATAL("WebAssembly is disabled");
}
#endif  // !V8_ENABLE_WEBASSEMBLY

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
  // The shape of |entered_contexts_| and |is_microtask_context_| stacks must
  // be in sync.
  is_microtask_context_.shrink_to_fit();
  DCHECK_EQ(entered_contexts_.capacity(), is_microtask_context_.capacity());
  DCHECK_EQ(entered_contexts_.size(), is_microtask_context_.size());
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

std::unique_ptr<PersistentHandles> HandleScopeImplementer::DetachPersistent(
    Address* first_block) {
  std::unique_ptr<PersistentHandles> ph(new PersistentHandles(isolate()));
  DCHECK_NOT_NULL(first_block);

  Address* block_start;
  do {
    block_start = blocks_.back();
    ph->blocks_.push_back(blocks_.back());
#if DEBUG
    ph->ordered_blocks_.insert(blocks_.back());
#endif
    blocks_.pop_back();
  } while (block_start != first_block);

  // ph->blocks_ now contains the blocks installed on the
  // HandleScope stack since BeginDeferredScope was called, but in
  // reverse order.

  // Switch first and last blocks, such that the last block is the one
  // that is potentially half full.
  DCHECK(!blocks_.empty() && !ph->blocks_.empty());
  std::swap(ph->blocks_.front(), ph->blocks_.back());

  ph->block_next_ = isolate()->handle_scope_data()->next;
  block_start = ph->blocks_.back();
  ph->block_limit_ = block_start + kHandleBlockSize;

  DCHECK_NOT_NULL(last_handle_before_deferred_block_);
  last_handle_before_deferred_block_ = nullptr;
  return ph;
}

void HandleScopeImplementer::BeginDeferredScope() {
  DCHECK_NULL(last_handle_before_deferred_block_);
  last_handle_before_deferred_block_ = isolate()->handle_scope_data()->next;
}

void InvokeAccessorGetterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    v8::AccessorNameGetterCallback getter) {
  // Leaving JavaScript.
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  RCS_SCOPE(i_isolate, RuntimeCallCounterId::kAccessorGetterCallback);
  Address getter_address = reinterpret_cast<Address>(getter);
  ExternalCallbackScope call_scope(i_isolate, getter_address);
  getter(property, info);
}

void InvokeFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                            v8::FunctionCallback callback) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  RCS_SCOPE(i_isolate, RuntimeCallCounterId::kFunctionCallback);
  Address callback_address = reinterpret_cast<Address>(callback);
  ExternalCallbackScope call_scope(i_isolate, callback_address);
  callback(info);
}

void InvokeFinalizationRegistryCleanupFromTask(
    Handle<Context> context,
    Handle<JSFinalizationRegistry> finalization_registry,
    Handle<Object> callback) {
  i::Isolate* i_isolate = finalization_registry->native_context().GetIsolate();
  RCS_SCOPE(i_isolate,
            RuntimeCallCounterId::kFinalizationRegistryCleanupFromTask);
  // Do not use ENTER_V8 because this is always called from a running
  // FinalizationRegistryCleanupTask within V8 and we should not log it as an
  // API call. This method is implemented here to avoid duplication of the
  // exception handling and microtask running logic in CallDepthScope.
  if (i_isolate->is_execution_terminating()) return;
  Local<v8::Context> api_context = Utils::ToLocal(context);
  CallDepthScope<true> call_depth_scope(i_isolate, api_context);
  VMState<OTHER> state(i_isolate);
  Handle<Object> argv[] = {callback};
  if (Execution::CallBuiltin(i_isolate,
                             i_isolate->finalization_registry_cleanup_some(),
                             finalization_registry, arraysize(argv), argv)
          .is_null()) {
    call_depth_scope.Escape();
  }
}

template <>
EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
int32_t ConvertDouble(double d) {
  return internal::DoubleToInt32(d);
}

template <>
EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
uint32_t ConvertDouble(double d) {
  return internal::DoubleToUint32(d);
}

template <>
EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
float ConvertDouble(double d) {
  return internal::DoubleToFloat32(d);
}

template <>
EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
double ConvertDouble(double d) {
  return d;
}

template <>
EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
int64_t ConvertDouble(double d) {
  return internal::DoubleToWebIDLInt64(d);
}

template <>
EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
uint64_t ConvertDouble(double d) {
  return internal::DoubleToWebIDLUint64(d);
}

template <>
EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
bool ConvertDouble(double d) {
  // Implements https://tc39.es/ecma262/#sec-toboolean.
  return !std::isnan(d) && d != 0;
}

// Undefine macros for jumbo build.
#undef SET_FIELD_WRAPPED
#undef NEW_STRING
#undef CALLBACK_SETTER

}  // namespace internal

template <>
bool V8_EXPORT V8_WARN_UNUSED_RESULT
TryToCopyAndConvertArrayToCppBuffer<CTypeInfoBuilder<int32_t>::Build().GetId(),
                                    int32_t>(Local<Array> src, int32_t* dst,
                                             uint32_t max_length) {
  return CopyAndConvertArrayToCppBuffer<
      CTypeInfo(CTypeInfo::Type::kInt32, CTypeInfo::SequenceType::kIsSequence)
          .GetId(),
      int32_t>(src, dst, max_length);
}

template <>
bool V8_EXPORT V8_WARN_UNUSED_RESULT
TryToCopyAndConvertArrayToCppBuffer<CTypeInfoBuilder<uint32_t>::Build().GetId(),
                                    uint32_t>(Local<Array> src, uint32_t* dst,
                                              uint32_t max_length) {
  return CopyAndConvertArrayToCppBuffer<
      CTypeInfo(CTypeInfo::Type::kUint32, CTypeInfo::SequenceType::kIsSequence)
          .GetId(),
      uint32_t>(src, dst, max_length);
}

template <>
bool V8_EXPORT V8_WARN_UNUSED_RESULT
TryToCopyAndConvertArrayToCppBuffer<CTypeInfoBuilder<float>::Build().GetId(),
                                    float>(Local<Array> src, float* dst,
                                           uint32_t max_length) {
  return CopyAndConvertArrayToCppBuffer<
      CTypeInfo(CTypeInfo::Type::kFloat32, CTypeInfo::SequenceType::kIsSequence)
          .GetId(),
      float>(src, dst, max_length);
}

template <>
bool V8_EXPORT V8_WARN_UNUSED_RESULT
TryToCopyAndConvertArrayToCppBuffer<CTypeInfoBuilder<double>::Build().GetId(),
                                    double>(Local<Array> src, double* dst,
                                            uint32_t max_length) {
  return CopyAndConvertArrayToCppBuffer<
      CTypeInfo(CTypeInfo::Type::kFloat64, CTypeInfo::SequenceType::kIsSequence)
          .GetId(),
      double>(src, dst, max_length);
}

}  // namespace v8

#include "src/api/api-macros-undef.h"
