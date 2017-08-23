// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "src/third_party/vtune/v8-vtune.h"
#endif

#include "src/d8-console.h"
#include "src/d8.h"
#include "src/ostreams.h"

#include "include/libplatform/libplatform.h"
#include "include/libplatform/v8-tracing.h"
#include "include/v8-inspector.h"
#include "src/api.h"
#include "src/base/cpu.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/sys-info.h"
#include "src/basic-block-profiler.h"
#include "src/debug/debug-interface.h"
#include "src/interpreter/interpreter.h"
#include "src/list-inl.h"
#include "src/msan.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/snapshot/natives.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils.h"
#include "src/v8.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#else
#include <windows.h>  // NOLINT
#if defined(_MSC_VER)
#include <crtdbg.h>  // NOLINT
#endif               // defined(_MSC_VER)
#endif               // !defined(_WIN32) && !defined(_WIN64)

#ifndef DCHECK
#define DCHECK(condition) assert(condition)
#endif

#ifndef CHECK
#define CHECK(condition) assert(condition)
#endif

namespace v8 {

namespace {

const int MB = 1024 * 1024;
const int kMaxWorkers = 50;
const int kMaxSerializerMemoryUsage = 1 * MB;  // Arbitrary maximum for testing.

#define USE_VM 1
#define VM_THRESHOLD 65536
// TODO(titzer): allocations should fail if >= 2gb because of
// array buffers storing the lengths as a SMI internally.
#define TWO_GB (2u * 1024u * 1024u * 1024u)

// Forwards memory reservation and protection functions to the V8 default
// allocator. Used by ShellArrayBufferAllocator and MockArrayBufferAllocator.
class ArrayBufferAllocatorBase : public v8::ArrayBuffer::Allocator {
  std::unique_ptr<Allocator> allocator_ =
      std::unique_ptr<Allocator>(NewDefaultAllocator());

 public:
  void* Reserve(size_t length) override { return allocator_->Reserve(length); }

  void Free(void*, size_t) override = 0;

  void Free(void* data, size_t length, AllocationMode mode) override {
    switch (mode) {
      case AllocationMode::kNormal: {
        return Free(data, length);
      }
      case AllocationMode::kReservation: {
        return allocator_->Free(data, length, mode);
      }
    }
  }

  void SetProtection(void* data, size_t length,
                     Protection protection) override {
    allocator_->SetProtection(data, length, protection);
  }
};

class ShellArrayBufferAllocator : public ArrayBufferAllocatorBase {
 public:
  void* Allocate(size_t length) override {
#if USE_VM
    if (RoundToPageSize(&length)) {
      void* data = VirtualMemoryAllocate(length);
#if DEBUG
      if (data) {
        // In debug mode, check the memory is zero-initialized.
        size_t limit = length / sizeof(uint64_t);
        uint64_t* ptr = reinterpret_cast<uint64_t*>(data);
        for (size_t i = 0; i < limit; i++) {
          DCHECK_EQ(0u, ptr[i]);
        }
      }
#endif
      return data;
    }
#endif
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  void* AllocateUninitialized(size_t length) override {
#if USE_VM
    if (RoundToPageSize(&length)) return VirtualMemoryAllocate(length);
#endif
// Work around for GCC bug on AIX
// See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79839
#if V8_OS_AIX && _LINUX_SOURCE_COMPAT
    return __linux_malloc(length);
#else
    return malloc(length);
#endif
  }
  void Free(void* data, size_t length) override {
#if USE_VM
    if (RoundToPageSize(&length)) {
      base::VirtualMemory::ReleaseRegion(data, length);
      return;
    }
#endif
    free(data);
  }
  // If {length} is at least {VM_THRESHOLD}, round up to next page size
  // and return {true}. Otherwise return {false}.
  bool RoundToPageSize(size_t* length) {
    const size_t kPageSize = base::OS::CommitPageSize();
    if (*length >= VM_THRESHOLD && *length < TWO_GB) {
      *length = ((*length + kPageSize - 1) / kPageSize) * kPageSize;
      return true;
    }
    return false;
  }
#if USE_VM
  void* VirtualMemoryAllocate(size_t length) {
    void* data = base::VirtualMemory::ReserveRegion(length);
    if (data && !base::VirtualMemory::CommitRegion(data, length, false)) {
      base::VirtualMemory::ReleaseRegion(data, length);
      return nullptr;
    }
    MSAN_MEMORY_IS_INITIALIZED(data, length);
    return data;
  }
#endif
};

class MockArrayBufferAllocator : public ArrayBufferAllocatorBase {
  const size_t kAllocationLimit = 10 * MB;
  size_t get_actual_length(size_t length) const {
    return length > kAllocationLimit ? base::OS::CommitPageSize() : length;
  }

 public:
  void* Allocate(size_t length) override {
    const size_t actual_length = get_actual_length(length);
    void* data = AllocateUninitialized(actual_length);
    return data == NULL ? data : memset(data, 0, actual_length);
  }
  void* AllocateUninitialized(size_t length) override {
    return malloc(get_actual_length(length));
  }
  void Free(void* p, size_t) override { free(p); }
  void Free(void* data, size_t length, AllocationMode mode) override {
    ArrayBufferAllocatorBase::Free(data, get_actual_length(length), mode);
  }
  void* Reserve(size_t length) override {
    return ArrayBufferAllocatorBase::Reserve(get_actual_length(length));
  }
};

// Predictable v8::Platform implementation. Background tasks and idle tasks are
// disallowed, and the time reported by {MonotonicallyIncreasingTime} is
// deterministic.
class PredictablePlatform : public Platform {
public:
 explicit PredictablePlatform(std::unique_ptr<Platform> platform)
     : platform_(std::move(platform)) {
   DCHECK_NOT_NULL(platform_);
 }

 void CallOnBackgroundThread(Task* task,
                             ExpectedRuntime expected_runtime) override {
   // It's not defined when background tasks are being executed, so we can just
   // execute them right away.
   task->Run();
   delete task;
 }

 void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
   platform_->CallOnForegroundThread(isolate, task);
 }

 void CallDelayedOnForegroundThread(v8::Isolate* isolate, Task* task,
                                    double delay_in_seconds) override {
   platform_->CallDelayedOnForegroundThread(isolate, task, delay_in_seconds);
 }

 void CallIdleOnForegroundThread(Isolate* isolate, IdleTask* task) override {
   UNREACHABLE();
 }

 bool IdleTasksEnabled(Isolate* isolate) override { return false; }

 double MonotonicallyIncreasingTime() override {
   return synthetic_time_in_sec_ += 0.00001;
 }

 v8::TracingController* GetTracingController() override {
   return platform_->GetTracingController();
 }

 Platform* platform() const { return platform_.get(); }

private:
 double synthetic_time_in_sec_ = 0.0;
 std::unique_ptr<Platform> platform_;

 DISALLOW_COPY_AND_ASSIGN(PredictablePlatform);
};


v8::Platform* g_platform = NULL;

v8::Platform* GetDefaultPlatform() {
  return i::FLAG_verify_predictable
             ? static_cast<PredictablePlatform*>(g_platform)->platform()
             : g_platform;
}

static Local<Value> Throw(Isolate* isolate, const char* message) {
  return isolate->ThrowException(
      String::NewFromUtf8(isolate, message, NewStringType::kNormal)
          .ToLocalChecked());
}

Worker* GetWorkerFromInternalField(Isolate* isolate, Local<Object> object) {
  if (object->InternalFieldCount() != 1) {
    Throw(isolate, "this is not a Worker");
    return NULL;
  }

  Worker* worker =
      static_cast<Worker*>(object->GetAlignedPointerFromInternalField(0));
  if (worker == NULL) {
    Throw(isolate, "Worker is defunct because main thread is terminating");
    return NULL;
  }

  return worker;
}


}  // namespace

namespace tracing {

namespace {

// String options that can be used to initialize TraceOptions.
const char kRecordUntilFull[] = "record-until-full";
const char kRecordContinuously[] = "record-continuously";
const char kRecordAsMuchAsPossible[] = "record-as-much-as-possible";

const char kRecordModeParam[] = "record_mode";
const char kEnableSystraceParam[] = "enable_systrace";
const char kEnableArgumentFilterParam[] = "enable_argument_filter";
const char kIncludedCategoriesParam[] = "included_categories";

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
        String::NewFromUtf8(isolate, json_str, NewStringType::kNormal)
            .ToLocalChecked();
    Local<Value> result = JSON::Parse(context, source).ToLocalChecked();
    Local<v8::Object> trace_config_object = Local<v8::Object>::Cast(result);

    trace_config->SetTraceRecordMode(
        GetTraceRecordMode(isolate, context, trace_config_object));
    if (GetBoolean(isolate, context, trace_config_object,
                   kEnableSystraceParam)) {
      trace_config->EnableSystrace();
    }
    if (GetBoolean(isolate, context, trace_config_object,
                   kEnableArgumentFilterParam)) {
      trace_config->EnableArgumentFilter();
    }
    UpdateIncludedCategoriesList(isolate, context, trace_config_object,
                                 trace_config);
  }

 private:
  static bool GetBoolean(v8::Isolate* isolate, Local<Context> context,
                         Local<v8::Object> object, const char* property) {
    Local<Value> value = GetValue(isolate, context, object, property);
    if (value->IsNumber()) {
      Local<Boolean> v8_boolean = value->ToBoolean(context).ToLocalChecked();
      return v8_boolean->Value();
    }
    return false;
  }

  static int UpdateIncludedCategoriesList(
      v8::Isolate* isolate, Local<Context> context, Local<v8::Object> object,
      platform::tracing::TraceConfig* trace_config) {
    Local<Value> value =
        GetValue(isolate, context, object, kIncludedCategoriesParam);
    if (value->IsArray()) {
      Local<Array> v8_array = Local<Array>::Cast(value);
      for (int i = 0, length = v8_array->Length(); i < length; ++i) {
        Local<Value> v = v8_array->Get(context, i)
                             .ToLocalChecked()
                             ->ToString(context)
                             .ToLocalChecked();
        String::Utf8Value str(v->ToString(context).ToLocalChecked());
        trace_config->AddIncludedCategory(*str);
      }
      return v8_array->Length();
    }
    return 0;
  }

  static platform::tracing::TraceRecordMode GetTraceRecordMode(
      v8::Isolate* isolate, Local<Context> context, Local<v8::Object> object) {
    Local<Value> value = GetValue(isolate, context, object, kRecordModeParam);
    if (value->IsString()) {
      Local<String> v8_string = value->ToString(context).ToLocalChecked();
      String::Utf8Value str(v8_string);
      if (strcmp(kRecordUntilFull, *str) == 0) {
        return platform::tracing::TraceRecordMode::RECORD_UNTIL_FULL;
      } else if (strcmp(kRecordContinuously, *str) == 0) {
        return platform::tracing::TraceRecordMode::RECORD_CONTINUOUSLY;
      } else if (strcmp(kRecordAsMuchAsPossible, *str) == 0) {
        return platform::tracing::TraceRecordMode::RECORD_AS_MUCH_AS_POSSIBLE;
      }
    }
    return platform::tracing::TraceRecordMode::RECORD_UNTIL_FULL;
  }

  static Local<Value> GetValue(v8::Isolate* isolate, Local<Context> context,
                               Local<v8::Object> object, const char* property) {
    Local<String> v8_str =
        String::NewFromUtf8(isolate, property, NewStringType::kNormal)
            .ToLocalChecked();
    return object->Get(context, v8_str).ToLocalChecked();
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

class PerIsolateData {
 public:
  explicit PerIsolateData(Isolate* isolate) : isolate_(isolate), realms_(NULL) {
    HandleScope scope(isolate);
    isolate->SetData(0, this);
  }

  ~PerIsolateData() {
    isolate_->SetData(0, NULL);  // Not really needed, just to be sure...
  }

  inline static PerIsolateData* Get(Isolate* isolate) {
    return reinterpret_cast<PerIsolateData*>(isolate->GetData(0));
  }

  class RealmScope {
   public:
    explicit RealmScope(PerIsolateData* data);
    ~RealmScope();
   private:
    PerIsolateData* data_;
  };

 private:
  friend class Shell;
  friend class RealmScope;
  Isolate* isolate_;
  int realm_count_;
  int realm_current_;
  int realm_switch_;
  Global<Context>* realms_;
  Global<Value> realm_shared_;

  int RealmIndexOrThrow(const v8::FunctionCallbackInfo<v8::Value>& args,
                        int arg_offset);
  int RealmFind(Local<Context> context);
};

class ExternalOwningOneByteStringResource
    : public String::ExternalOneByteStringResource {
 public:
  ExternalOwningOneByteStringResource() : length_(0) {}
  ExternalOwningOneByteStringResource(std::unique_ptr<const char[]> data,
                                      size_t length)
      : data_(std::move(data)), length_(length) {}
  const char* data() const override { return data_.get(); }
  size_t length() const override { return length_; }

 private:
  std::unique_ptr<const char[]> data_;
  size_t length_;
};

CounterMap* Shell::counter_map_;
base::OS::MemoryMappedFile* Shell::counters_file_ = NULL;
CounterCollection Shell::local_counters_;
CounterCollection* Shell::counters_ = &local_counters_;
base::LazyMutex Shell::context_mutex_;
const base::TimeTicks Shell::kInitialTicks =
    base::TimeTicks::HighResolutionNow();
Global<Function> Shell::stringify_function_;
base::LazyMutex Shell::workers_mutex_;
bool Shell::allow_new_workers_ = true;
i::List<Worker*> Shell::workers_;
std::vector<ExternalizedContents> Shell::externalized_contents_;
base::LazyMutex Shell::isolate_status_lock_;
std::map<v8::Isolate*, bool> Shell::isolate_status_;

Global<Context> Shell::evaluation_context_;
ArrayBuffer::Allocator* Shell::array_buffer_allocator;
ShellOptions Shell::options;
base::OnceType Shell::quit_once_ = V8_ONCE_INIT;

bool CounterMap::Match(void* key1, void* key2) {
  const char* name1 = reinterpret_cast<const char*>(key1);
  const char* name2 = reinterpret_cast<const char*>(key2);
  return strcmp(name1, name2) == 0;
}


ScriptCompiler::CachedData* CompileForCachedData(
    Local<String> source, Local<Value> name,
    ScriptCompiler::CompileOptions compile_options) {
  int source_length = source->Length();
  uint16_t* source_buffer = new uint16_t[source_length];
  source->Write(source_buffer, 0, source_length);
  int name_length = 0;
  uint16_t* name_buffer = NULL;
  if (name->IsString()) {
    Local<String> name_string = Local<String>::Cast(name);
    name_length = name_string->Length();
    name_buffer = new uint16_t[name_length];
    name_string->Write(name_buffer, 0, name_length);
  }
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = Shell::array_buffer_allocator;
  create_params.host_import_module_dynamically_callback_ =
      Shell::HostImportModuleDynamically;
  Isolate* temp_isolate = Isolate::New(create_params);
  ScriptCompiler::CachedData* result = NULL;
  {
    Isolate::Scope isolate_scope(temp_isolate);
    HandleScope handle_scope(temp_isolate);
    Context::Scope context_scope(Context::New(temp_isolate));
    Local<String> source_copy =
        v8::String::NewFromTwoByte(temp_isolate, source_buffer,
                                   v8::NewStringType::kNormal, source_length)
            .ToLocalChecked();
    Local<Value> name_copy;
    if (name_buffer) {
      name_copy =
          v8::String::NewFromTwoByte(temp_isolate, name_buffer,
                                     v8::NewStringType::kNormal, name_length)
              .ToLocalChecked();
    } else {
      name_copy = v8::Undefined(temp_isolate);
    }
    ScriptCompiler::Source script_source(source_copy, ScriptOrigin(name_copy));
    if (!ScriptCompiler::CompileUnboundScript(temp_isolate, &script_source,
                                              compile_options)
             .IsEmpty() &&
        script_source.GetCachedData()) {
      int length = script_source.GetCachedData()->length;
      uint8_t* cache = new uint8_t[length];
      memcpy(cache, script_source.GetCachedData()->data, length);
      result = new ScriptCompiler::CachedData(
          cache, length, ScriptCompiler::CachedData::BufferOwned);
    }
  }
  temp_isolate->Dispose();
  delete[] source_buffer;
  delete[] name_buffer;
  return result;
}


// Compile a string within the current v8 context.
MaybeLocal<Script> Shell::CompileString(
    Isolate* isolate, Local<String> source, Local<Value> name,
    ScriptCompiler::CompileOptions compile_options) {
  Local<Context> context(isolate->GetCurrentContext());
  ScriptOrigin origin(name);
  if (compile_options == ScriptCompiler::kNoCompileOptions) {
    ScriptCompiler::Source script_source(source, origin);
    return ScriptCompiler::Compile(context, &script_source, compile_options);
  }

  ScriptCompiler::CachedData* data =
      CompileForCachedData(source, name, compile_options);
  ScriptCompiler::Source cached_source(source, origin, data);
  if (compile_options == ScriptCompiler::kProduceCodeCache) {
    compile_options = ScriptCompiler::kConsumeCodeCache;
  } else if (compile_options == ScriptCompiler::kProduceParserCache) {
    compile_options = ScriptCompiler::kConsumeParserCache;
  } else {
    DCHECK(false);  // A new compile option?
  }
  if (data == NULL) compile_options = ScriptCompiler::kNoCompileOptions;
  MaybeLocal<Script> result =
      ScriptCompiler::Compile(context, &cached_source, compile_options);
  CHECK(data == NULL || !data->rejected);
  return result;
}


// Executes a string within the current v8 context.
bool Shell::ExecuteString(Isolate* isolate, Local<String> source,
                          Local<Value> name, bool print_result,
                          bool report_exceptions) {
  HandleScope handle_scope(isolate);
  TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  MaybeLocal<Value> maybe_result;
  {
    PerIsolateData* data = PerIsolateData::Get(isolate);
    Local<Context> realm =
        Local<Context>::New(isolate, data->realms_[data->realm_current_]);
    Context::Scope context_scope(realm);
    Local<Script> script;
    if (!Shell::CompileString(isolate, source, name, options.compile_options)
             .ToLocal(&script)) {
      // Print errors that happened during compilation.
      if (report_exceptions) ReportException(isolate, &try_catch);
      return false;
    }
    maybe_result = script->Run(realm);
    EmptyMessageQueues(isolate);
    data->realm_current_ = data->realm_switch_;
  }
  Local<Value> result;
  if (!maybe_result.ToLocal(&result)) {
    DCHECK(try_catch.HasCaught());
    // Print errors that happened during execution.
    if (report_exceptions) ReportException(isolate, &try_catch);
    return false;
  }
  DCHECK(!try_catch.HasCaught());
  if (print_result) {
    if (options.test_shell) {
      if (!result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        v8::String::Utf8Value str(result);
        fwrite(*str, sizeof(**str), str.length(), stdout);
        printf("\n");
      }
    } else {
      v8::String::Utf8Value str(Stringify(isolate, result));
      fwrite(*str, sizeof(**str), str.length(), stdout);
      printf("\n");
    }
  }
  return true;
}

namespace {

std::string ToSTLString(Local<String> v8_str) {
  String::Utf8Value utf8(v8_str);
  // Should not be able to fail since the input is a String.
  CHECK(*utf8);
  return *utf8;
}

bool IsAbsolutePath(const std::string& path) {
#if defined(_WIN32) || defined(_WIN64)
  // TODO(adamk): This is an incorrect approximation, but should
  // work for all our test-running cases.
  return path.find(':') != std::string::npos;
#else
  return path[0] == '/';
#endif
}

std::string GetWorkingDirectory() {
#if defined(_WIN32) || defined(_WIN64)
  char system_buffer[MAX_PATH];
  // TODO(adamk): Support Unicode paths.
  DWORD len = GetCurrentDirectoryA(MAX_PATH, system_buffer);
  CHECK(len > 0);
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
  std::string result;
  if (IsAbsolutePath(path)) {
    result = path;
  } else {
    result = dir_name + '/' + path;
  }
  std::replace(result.begin(), result.end(), '\\', '/');
  size_t i;
  while ((i = result.find("/./")) != std::string::npos) {
    result.erase(i, 2);
  }
  return result;
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
      : module_to_directory_map(10, ModuleGlobalHash(isolate)) {}

  // Map from normalized module specifier to Module.
  std::unordered_map<std::string, Global<Module>> specifier_to_module_map;
  // Map from Module to the directory that Module was loaded from.
  std::unordered_map<Global<Module>, std::string, ModuleGlobalHash>
      module_to_directory_map;
};

enum {
  kModuleEmbedderDataIndex,
  kInspectorClientIndex
};

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
                                         Local<Module> referrer) {
  Isolate* isolate = context->GetIsolate();
  ModuleEmbedderData* d = GetModuleDataFromContext(context);
  auto dir_name_it =
      d->module_to_directory_map.find(Global<Module>(isolate, referrer));
  CHECK(dir_name_it != d->module_to_directory_map.end());
  std::string absolute_path =
      NormalizePath(ToSTLString(specifier), dir_name_it->second);
  auto module_it = d->specifier_to_module_map.find(absolute_path);
  CHECK(module_it != d->specifier_to_module_map.end());
  return module_it->second.Get(isolate);
}

}  // anonymous namespace

MaybeLocal<Module> Shell::FetchModuleTree(Local<Context> context,
                                          const std::string& file_name) {
  DCHECK(IsAbsolutePath(file_name));
  Isolate* isolate = context->GetIsolate();
  Local<String> source_text = ReadFile(isolate, file_name.c_str());
  if (source_text.IsEmpty()) {
    std::string msg = "Error reading: " + file_name;
    Throw(isolate, msg.c_str());
    return MaybeLocal<Module>();
  }
  ScriptOrigin origin(
      String::NewFromUtf8(isolate, file_name.c_str(), NewStringType::kNormal)
          .ToLocalChecked(),
      Local<Integer>(), Local<Integer>(), Local<Boolean>(), Local<Integer>(),
      Local<Value>(), Local<Boolean>(), Local<Boolean>(), True(isolate));
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module;
  if (!ScriptCompiler::CompileModule(isolate, &source).ToLocal(&module)) {
    return MaybeLocal<Module>();
  }

  ModuleEmbedderData* d = GetModuleDataFromContext(context);
  CHECK(d->specifier_to_module_map
            .insert(std::make_pair(file_name, Global<Module>(isolate, module)))
            .second);

  std::string dir_name = DirName(file_name);
  CHECK(d->module_to_directory_map
            .insert(std::make_pair(Global<Module>(isolate, module), dir_name))
            .second);

  for (int i = 0, length = module->GetModuleRequestsLength(); i < length; ++i) {
    Local<String> name = module->GetModuleRequest(i);
    std::string absolute_path = NormalizePath(ToSTLString(name), dir_name);
    if (!d->specifier_to_module_map.count(absolute_path)) {
      if (FetchModuleTree(context, absolute_path).IsEmpty()) {
        return MaybeLocal<Module>();
      }
    }
  }

  return module;
}

namespace {

struct DynamicImportData {
  DynamicImportData(Isolate* isolate_, Local<String> referrer_,
                    Local<String> specifier_,
                    Local<DynamicImportResult> result_)
      : isolate(isolate_) {
    referrer.Reset(isolate, referrer_);
    specifier.Reset(isolate, specifier_);
    result.Reset(isolate, result_);
  }

  Isolate* isolate;
  Global<String> referrer;
  Global<String> specifier;
  Global<DynamicImportResult> result;
};

}  // namespace
void Shell::HostImportModuleDynamically(Isolate* isolate,
                                        Local<String> referrer,
                                        Local<String> specifier,
                                        Local<DynamicImportResult> result) {
  DynamicImportData* data =
      new DynamicImportData(isolate, referrer, specifier, result);
  isolate->EnqueueMicrotask(Shell::DoHostImportModuleDynamically, data);
}

void Shell::DoHostImportModuleDynamically(void* import_data) {
  std::unique_ptr<DynamicImportData> import_data_(
      static_cast<DynamicImportData*>(import_data));
  Isolate* isolate(import_data_->isolate);
  HandleScope handle_scope(isolate);

  Local<String> referrer(import_data_->referrer.Get(isolate));
  Local<String> specifier(import_data_->specifier.Get(isolate));
  Local<DynamicImportResult> result(import_data_->result.Get(isolate));

  PerIsolateData* data = PerIsolateData::Get(isolate);
  Local<Context> realm = data->realms_[data->realm_current_].Get(isolate);
  Context::Scope context_scope(realm);

  std::string source_url = ToSTLString(referrer);
  std::string dir_name =
      IsAbsolutePath(source_url) ? DirName(source_url) : GetWorkingDirectory();
  std::string file_name = ToSTLString(specifier);
  std::string absolute_path = NormalizePath(file_name.c_str(), dir_name);

  TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  ModuleEmbedderData* d = GetModuleDataFromContext(realm);
  Local<Module> root_module;
  auto module_it = d->specifier_to_module_map.find(absolute_path);
  if (module_it != d->specifier_to_module_map.end()) {
    root_module = module_it->second.Get(isolate);
  } else if (!FetchModuleTree(realm, absolute_path).ToLocal(&root_module)) {
    CHECK(try_catch.HasCaught());
    CHECK(result->FinishDynamicImportFailure(realm, try_catch.Exception()));
    return;
  }

  MaybeLocal<Value> maybe_result;
  if (root_module->Instantiate(realm, ResolveModuleCallback)) {
    maybe_result = root_module->Evaluate(realm);
    EmptyMessageQueues(isolate);
  }

  Local<Value> module;
  if (!maybe_result.ToLocal(&module)) {
    DCHECK(try_catch.HasCaught());
    CHECK(result->FinishDynamicImportFailure(realm, try_catch.Exception()));
    return;
  }

  DCHECK(!try_catch.HasCaught());
  CHECK(result->FinishDynamicImportSuccess(realm, root_module));
}

bool Shell::ExecuteModule(Isolate* isolate, const char* file_name) {
  HandleScope handle_scope(isolate);

  PerIsolateData* data = PerIsolateData::Get(isolate);
  Local<Context> realm = data->realms_[data->realm_current_].Get(isolate);
  Context::Scope context_scope(realm);

  std::string absolute_path = NormalizePath(file_name, GetWorkingDirectory());

  TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  Local<Module> root_module;
  MaybeLocal<Value> maybe_exception;

  if (!FetchModuleTree(realm, absolute_path).ToLocal(&root_module)) {
    CHECK(try_catch.HasCaught());
    ReportException(isolate, &try_catch);
    return false;
  }

  MaybeLocal<Value> maybe_result;
  if (root_module->Instantiate(realm, ResolveModuleCallback)) {
    maybe_result = root_module->Evaluate(realm);
    EmptyMessageQueues(isolate);
  }
  Local<Value> result;
  if (!maybe_result.ToLocal(&result)) {
    DCHECK(try_catch.HasCaught());
    // Print errors that happened during execution.
    ReportException(isolate, &try_catch);
    return false;
  }
  DCHECK(!try_catch.HasCaught());
  return true;
}

PerIsolateData::RealmScope::RealmScope(PerIsolateData* data) : data_(data) {
  data_->realm_count_ = 1;
  data_->realm_current_ = 0;
  data_->realm_switch_ = 0;
  data_->realms_ = new Global<Context>[1];
  data_->realms_[0].Reset(data_->isolate_,
                          data_->isolate_->GetEnteredContext());
}


PerIsolateData::RealmScope::~RealmScope() {
  // Drop realms to avoid keeping them alive. We don't dispose the
  // module embedder data for the first realm here, but instead do
  // it in RunShell or in RunMain, if not running in interactive mode
  for (int i = 1; i < data_->realm_count_; ++i) {
    Global<Context>& realm = data_->realms_[i];
    if (realm.IsEmpty()) continue;
    DisposeModuleEmbedderData(realm.Get(data_->isolate_));
    // TODO(adamk): No need to reset manually, Globals reset when destructed.
    realm.Reset();
  }
  delete[] data_->realms_;
  // TODO(adamk): No need to reset manually, Globals reset when destructed.
  if (!data_->realm_shared_.IsEmpty())
    data_->realm_shared_.Reset();
}


int PerIsolateData::RealmFind(Local<Context> context) {
  for (int i = 0; i < realm_count_; ++i) {
    if (realms_[i] == context) return i;
  }
  return -1;
}


int PerIsolateData::RealmIndexOrThrow(
    const v8::FunctionCallbackInfo<v8::Value>& args,
    int arg_offset) {
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


// Realm.current() returns the index of the currently active realm.
void Shell::RealmCurrent(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int index = data->RealmFind(isolate->GetEnteredContext());
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
  int index = data->RealmFind(args[0]
                                  ->ToObject(isolate->GetCurrentContext())
                                  .ToLocalChecked()
                                  ->CreationContext());
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
      Context::New(isolate, NULL, global_template, global_object);
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
  DisposeModuleEmbedderData(data->realms_[index].Get(isolate));
  data->realms_[index].Reset();
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
        args.GetIsolate()->GetEnteredContext()->GetSecurityToken());
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
  DisposeRealm(args, index);
  CreateRealm(args, index, global_object);
}

// Realm.dispose(i) disposes the reference to the realm i.
void Shell::RealmDispose(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  int index = data->RealmIndexOrThrow(args, 0);
  if (index == -1) return;
  if (index == 0 ||
      index == data->realm_current_ || index == data->realm_switch_) {
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
  ScriptCompiler::Source script_source(
      args[1]->ToString(isolate->GetCurrentContext()).ToLocalChecked());
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

void Shell::RealmSharedSet(Local<String> property,
                           Local<Value> value,
                           const PropertyCallbackInfo<void>& info) {
  Isolate* isolate = info.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  data->realm_shared_.Reset(isolate, value);
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
      arg = Local<Symbol>::Cast(arg)->Name();
    }
    if (!arg->ToString(args.GetIsolate()->GetCurrentContext())
             .ToLocal(&str_obj)) {
      try_catch.ReThrow();
      return;
    }

    v8::String::Utf8Value str(str_obj);
    int n = static_cast<int>(fwrite(*str, sizeof(**str), str.length(), file));
    if (n != str.length()) {
      printf("Error in fwrite\n");
      Shell::Exit(1);
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
  String::Utf8Value file(args[0]);
  if (*file == NULL) {
    Throw(args.GetIsolate(), "Error loading file");
    return;
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
  Local<String> accumulator =
      String::NewFromUtf8(isolate, "", NewStringType::kNormal).ToLocalChecked();
  int length;
  while (true) {
    // Continue reading if the line ends with an escape '\\' or the line has
    // not been fully read into the buffer yet (does not end with '\n').
    // If fgets gets an error, just give up.
    char* input = NULL;
    input = fgets(buffer, kBufferSize, stdin);
    if (input == NULL) return Local<String>();
    length = static_cast<int>(strlen(buffer));
    if (length == 0) {
      return accumulator;
    } else if (buffer[length-1] != '\n') {
      accumulator = String::Concat(
          accumulator,
          String::NewFromUtf8(isolate, buffer, NewStringType::kNormal, length)
              .ToLocalChecked());
    } else if (length > 1 && buffer[length-2] == '\\') {
      buffer[length-2] = '\n';
      accumulator = String::Concat(
          accumulator,
          String::NewFromUtf8(isolate, buffer, NewStringType::kNormal,
                              length - 1).ToLocalChecked());
    } else {
      return String::Concat(
          accumulator,
          String::NewFromUtf8(isolate, buffer, NewStringType::kNormal,
                              length - 1).ToLocalChecked());
    }
  }
}


void Shell::Load(const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(args.GetIsolate());
    String::Utf8Value file(args[i]);
    if (*file == NULL) {
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
            String::NewFromUtf8(args.GetIsolate(), *file,
                                NewStringType::kNormal).ToLocalChecked(),
            false, true)) {
      Throw(args.GetIsolate(), "Error executing file");
      return;
    }
  }
}


void Shell::WorkerNew(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  if (args.Length() < 1 || !args[0]->IsString()) {
    Throw(args.GetIsolate(), "1st argument must be string");
    return;
  }

  if (!args.IsConstructCall()) {
    Throw(args.GetIsolate(), "Worker must be constructed with new");
    return;
  }

  {
    base::LockGuard<base::Mutex> lock_guard(workers_mutex_.Pointer());
    if (workers_.length() >= kMaxWorkers) {
      Throw(args.GetIsolate(), "Too many workers, I won't let you create more");
      return;
    }

    // Initialize the embedder field to NULL; if we return early without
    // creating a new Worker (because the main thread is terminating) we can
    // early-out from the instance calls.
    args.Holder()->SetAlignedPointerInInternalField(0, NULL);

    if (!allow_new_workers_) return;

    Worker* worker = new Worker;
    args.Holder()->SetAlignedPointerInInternalField(0, worker);
    workers_.Add(worker);

    String::Utf8Value script(args[0]);
    if (!*script) {
      Throw(args.GetIsolate(), "Can't get worker script");
      return;
    }
    worker->StartExecuteInThread(*script);
  }
}


void Shell::WorkerPostMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);

  if (args.Length() < 1) {
    Throw(isolate, "Invalid argument");
    return;
  }

  Worker* worker = GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker) {
    return;
  }

  Local<Value> message = args[0];
  Local<Value> transfer =
      args.Length() >= 2 ? args[1] : Local<Value>::Cast(Undefined(isolate));
  std::unique_ptr<SerializationData> data =
      Shell::SerializeValue(isolate, message, transfer);
  if (data) {
    worker->PostMessage(std::move(data));
  }
}


void Shell::WorkerGetMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  Worker* worker = GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker) {
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
  Worker* worker = GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker) {
    return;
  }

  worker->Terminate();
}


void Shell::QuitOnce(v8::FunctionCallbackInfo<v8::Value>* args) {
  int exit_code = (*args)[0]
                      ->Int32Value(args->GetIsolate()->GetCurrentContext())
                      .FromMaybe(0);
  CleanupWorkers();
  args->GetIsolate()->Exit();
  OnExit(args->GetIsolate());
  Exit(exit_code);
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
      String::NewFromUtf8(args.GetIsolate(), V8::GetVersion(),
                          NewStringType::kNormal).ToLocalChecked());
}


void Shell::ReportException(Isolate* isolate, v8::TryCatch* try_catch) {
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

  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  Local<Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    printf("%s\n", exception_string);
  } else if (message->GetScriptOrigin().Options().IsWasm()) {
    // Print <WASM>[(function index)]((function name))+(offset): (message).
    int function_index = message->GetLineNumber(context).FromJust() - 1;
    int offset = message->GetStartColumn(context).FromJust();
    printf("<WASM>[%d]+%d: %s\n", function_index, offset, exception_string);
  } else {
    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber(context).FromMaybe(-1);
    printf("%s:%i: %s\n", filename_string, linenum, exception_string);
    Local<String> sourceline;
    if (message->GetSourceLine(context).ToLocal(&sourceline)) {
      // Print line of source code.
      v8::String::Utf8Value sourcelinevalue(sourceline);
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
  if (try_catch->StackTrace(context).ToLocal(&stack_trace_string) &&
      stack_trace_string->IsString()) {
    v8::String::Utf8Value stack_trace(Local<String>::Cast(stack_trace_string));
    printf("%s\n", ToCString(stack_trace));
  }
  printf("\n");
  if (enter_context) context->Exit();
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
  if (counters_in_use_ == kMaxCounters) return NULL;
  return &counters_[counters_in_use_++];
}


void Shell::MapCounters(v8::Isolate* isolate, const char* name) {
  counters_file_ = base::OS::MemoryMappedFile::create(
      name, sizeof(CounterCollection), &local_counters_);
  void* memory = (counters_file_ == NULL) ?
      NULL : counters_file_->memory();
  if (memory == NULL) {
    printf("Could not map counters file %s\n", name);
    Exit(1);
  }
  counters_ = static_cast<CounterCollection*>(memory);
  isolate->SetCounterFunction(LookupCounter);
  isolate->SetCreateHistogramFunction(CreateHistogram);
  isolate->SetAddHistogramSampleFunction(AddHistogramSample);
}


int CounterMap::Hash(const char* name) {
  int h = 0;
  int c;
  while ((c = *name++) != 0) {
    h += h << 5;
    h += c;
  }
  return h;
}


Counter* Shell::GetCounter(const char* name, bool is_histogram) {
  Counter* counter = counter_map_->Lookup(name);

  if (counter == NULL) {
    counter = counters_->GetNextCounter();
    if (counter != NULL) {
      counter_map_->Set(name, counter);
      counter->Bind(name, is_histogram);
    }
  } else {
    DCHECK(counter->is_histogram() == is_histogram);
  }
  return counter;
}


int* Shell::LookupCounter(const char* name) {
  Counter* counter = GetCounter(name, false);

  if (counter != NULL) {
    return counter->ptr();
  } else {
    return NULL;
  }
}


void* Shell::CreateHistogram(const char* name,
                             int min,
                             int max,
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
    int source_index = i::NativesCollection<i::D8>::GetIndex("d8");
    i::Vector<const char> source_string =
        i::NativesCollection<i::D8>::GetScriptSource(source_index);
    i::Vector<const char> source_name =
        i::NativesCollection<i::D8>::GetScriptName(source_index);
    Local<String> source =
        String::NewFromUtf8(isolate, source_string.start(),
                            NewStringType::kNormal, source_string.length())
            .ToLocalChecked();
    Local<String> name =
        String::NewFromUtf8(isolate, source_name.start(),
                            NewStringType::kNormal, source_name.length())
            .ToLocalChecked();
    ScriptOrigin origin(name);
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
  global_template->Set(
      String::NewFromUtf8(isolate, "print", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, Print));
  global_template->Set(
      String::NewFromUtf8(isolate, "printErr", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, PrintErr));
  global_template->Set(
      String::NewFromUtf8(isolate, "write", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, Write));
  global_template->Set(
      String::NewFromUtf8(isolate, "read", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, Read));
  global_template->Set(
      String::NewFromUtf8(isolate, "readbuffer", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, ReadBuffer));
  global_template->Set(
      String::NewFromUtf8(isolate, "readline", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, ReadLine));
  global_template->Set(
      String::NewFromUtf8(isolate, "load", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, Load));
  // Some Emscripten-generated code tries to call 'quit', which in turn would
  // call C's exit(). This would lead to memory leaks, because there is no way
  // we can terminate cleanly then, so we need a way to hide 'quit'.
  if (!options.omit_quit) {
    global_template->Set(
        String::NewFromUtf8(isolate, "quit", NewStringType::kNormal)
            .ToLocalChecked(),
        FunctionTemplate::New(isolate, Quit));
  }
  Local<ObjectTemplate> test_template = ObjectTemplate::New(isolate);
  global_template->Set(
      String::NewFromUtf8(isolate, "testRunner", NewStringType::kNormal)
          .ToLocalChecked(),
      test_template);
  test_template->Set(
      String::NewFromUtf8(isolate, "notifyDone", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, NotifyDone));
  test_template->Set(
      String::NewFromUtf8(isolate, "waitUntilDone", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, WaitUntilDone));
  global_template->Set(
      String::NewFromUtf8(isolate, "version", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, Version));
  global_template->Set(
      Symbol::GetToStringTag(isolate),
      String::NewFromUtf8(isolate, "global", NewStringType::kNormal)
          .ToLocalChecked());

  // Bind the Realm object.
  Local<ObjectTemplate> realm_template = ObjectTemplate::New(isolate);
  realm_template->Set(
      String::NewFromUtf8(isolate, "current", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmCurrent));
  realm_template->Set(
      String::NewFromUtf8(isolate, "owner", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmOwner));
  realm_template->Set(
      String::NewFromUtf8(isolate, "global", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmGlobal));
  realm_template->Set(
      String::NewFromUtf8(isolate, "create", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmCreate));
  realm_template->Set(
      String::NewFromUtf8(isolate, "createAllowCrossRealmAccess",
                          NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmCreateAllowCrossRealmAccess));
  realm_template->Set(
      String::NewFromUtf8(isolate, "navigate", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmNavigate));
  realm_template->Set(
      String::NewFromUtf8(isolate, "dispose", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmDispose));
  realm_template->Set(
      String::NewFromUtf8(isolate, "switch", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmSwitch));
  realm_template->Set(
      String::NewFromUtf8(isolate, "eval", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, RealmEval));
  realm_template->SetAccessor(
      String::NewFromUtf8(isolate, "shared", NewStringType::kNormal)
          .ToLocalChecked(),
      RealmSharedGet, RealmSharedSet);
  global_template->Set(
      String::NewFromUtf8(isolate, "Realm", NewStringType::kNormal)
          .ToLocalChecked(),
      realm_template);

  Local<ObjectTemplate> performance_template = ObjectTemplate::New(isolate);
  performance_template->Set(
      String::NewFromUtf8(isolate, "now", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, PerformanceNow));
  global_template->Set(
      String::NewFromUtf8(isolate, "performance", NewStringType::kNormal)
          .ToLocalChecked(),
      performance_template);

  Local<FunctionTemplate> worker_fun_template =
      FunctionTemplate::New(isolate, WorkerNew);
  Local<Signature> worker_signature =
      Signature::New(isolate, worker_fun_template);
  worker_fun_template->SetClassName(
      String::NewFromUtf8(isolate, "Worker", NewStringType::kNormal)
          .ToLocalChecked());
  worker_fun_template->ReadOnlyPrototype();
  worker_fun_template->PrototypeTemplate()->Set(
      String::NewFromUtf8(isolate, "terminate", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, WorkerTerminate, Local<Value>(),
                            worker_signature));
  worker_fun_template->PrototypeTemplate()->Set(
      String::NewFromUtf8(isolate, "postMessage", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, WorkerPostMessage, Local<Value>(),
                            worker_signature));
  worker_fun_template->PrototypeTemplate()->Set(
      String::NewFromUtf8(isolate, "getMessage", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, WorkerGetMessage, Local<Value>(),
                            worker_signature));
  worker_fun_template->InstanceTemplate()->SetInternalFieldCount(1);
  global_template->Set(
      String::NewFromUtf8(isolate, "Worker", NewStringType::kNormal)
          .ToLocalChecked(),
      worker_fun_template);

  Local<ObjectTemplate> os_templ = ObjectTemplate::New(isolate);
  AddOSMethods(isolate, os_templ);
  global_template->Set(
      String::NewFromUtf8(isolate, "os", NewStringType::kNormal)
          .ToLocalChecked(),
      os_templ);

  return global_template;
}

static void PrintNonErrorsMessageCallback(Local<Message> message,
                                          Local<Value> error) {
  // Nothing to do here for errors, exceptions thrown up to the shell will be
  // reported
  // separately by {Shell::ReportException} after they are caught.
  // Do print other kinds of messages.
  switch (message->ErrorLevel()) {
    case v8::Isolate::kMessageWarning:
    case v8::Isolate::kMessageLog:
    case v8::Isolate::kMessageInfo:
    case v8::Isolate::kMessageDebug: {
      break;
    }

    case v8::Isolate::kMessageError: {
      // Ignore errors, printed elsewhere.
      return;
    }

    default: {
      UNREACHABLE();
      break;
    }
  }
  // Converts a V8 value to a C string.
  auto ToCString = [](const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
  };
  Isolate* isolate = Isolate::GetCurrent();
  v8::String::Utf8Value msg(message->Get());
  const char* msg_string = ToCString(msg);
  // Print (filename):(line number): (message).
  v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
  const char* filename_string = ToCString(filename);
  Maybe<int> maybeline = message->GetLineNumber(isolate->GetCurrentContext());
  int linenum = maybeline.IsJust() ? maybeline.FromJust() : -1;
  printf("%s:%i: %s\n", filename_string, linenum, msg_string);
}

void Shell::Initialize(Isolate* isolate) {
  // Set up counters
  if (i::StrLength(i::FLAG_map_counters) != 0)
    MapCounters(isolate, i::FLAG_map_counters);
  // Disable default message reporting.
  isolate->AddMessageListenerWithErrorLevel(
      PrintNonErrorsMessageCallback,
      v8::Isolate::kMessageError | v8::Isolate::kMessageWarning |
          v8::Isolate::kMessageInfo | v8::Isolate::kMessageDebug |
          v8::Isolate::kMessageLog);
}


Local<Context> Shell::CreateEvaluationContext(Isolate* isolate) {
  // This needs to be a critical section since this is not thread-safe
  base::LockGuard<base::Mutex> lock_guard(context_mutex_.Pointer());
  // Initialize the global objects
  Local<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  EscapableHandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate, NULL, global_template);
  DCHECK(!context.IsEmpty());
  InitializeModuleEmbedderData(context);
  Context::Scope scope(context);

  i::Factory* factory = reinterpret_cast<i::Isolate*>(isolate)->factory();
  i::JSArguments js_args = i::FLAG_js_arguments;
  i::Handle<i::FixedArray> arguments_array =
      factory->NewFixedArray(js_args.argc);
  for (int j = 0; j < js_args.argc; j++) {
    i::Handle<i::String> arg =
        factory->NewStringFromUtf8(i::CStrVector(js_args[j])).ToHandleChecked();
    arguments_array->set(j, *arg);
  }
  i::Handle<i::JSArray> arguments_jsarray =
      factory->NewJSArrayWithElements(arguments_array);
  context->Global()
      ->Set(context,
            String::NewFromUtf8(isolate, "arguments", NewStringType::kNormal)
                .ToLocalChecked(),
            Utils::ToLocal(arguments_jsarray))
      .FromJust();
  return handle_scope.Escape(context);
}

struct CounterAndKey {
  Counter* counter;
  const char* key;
};


inline bool operator<(const CounterAndKey& lhs, const CounterAndKey& rhs) {
  return strcmp(lhs.key, rhs.key) < 0;
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
      JSON::Stringify(context, dispatch_counters).ToLocalChecked());
}

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
    std::string file_name = ToSTLString(name);
    // Skip scripts not backed by a file.
    if (!std::ifstream(file_name).good()) continue;
    sink << "SF:";
    sink << NormalizePath(file_name, GetWorkingDirectory()) << std::endl;
    std::vector<uint32_t> lines;
    for (size_t j = 0; j < script_data.FunctionCount(); j++) {
      debug::Coverage::FunctionData function_data =
          script_data.GetFunctionData(j);
      debug::Location start =
          script->GetSourceLocation(function_data.StartOffset());
      debug::Location end =
          script->GetSourceLocation(function_data.EndOffset());
      int start_line = start.GetLineNumber();
      int end_line = end.GetLineNumber();
      uint32_t count = function_data.Count();
      // Ensure space in the array.
      lines.resize(std::max(static_cast<size_t>(end_line + 1), lines.size()),
                   0);
      // Boundary lines could be shared between two functions with different
      // invocation counts. Take the maximum.
      lines[start_line] = std::max(lines[start_line], count);
      lines[end_line] = std::max(lines[end_line], count);
      // Invocation counts for non-boundary lines are overwritten.
      for (int k = start_line + 1; k < end_line; k++) lines[k] = count;
      // Write function stats.
      Local<String> name;
      std::stringstream name_stream;
      if (function_data.Name().ToLocal(&name)) {
        name_stream << ToSTLString(name);
      } else {
        name_stream << "<" << start_line + 1 << "-";
        name_stream << start.GetColumnNumber() << ">";
      }
      sink << "FN:" << start_line + 1 << "," << name_stream.str() << std::endl;
      sink << "FNDA:" << count << "," << name_stream.str() << std::endl;
    }
    // Write per-line coverage. LCOV uses 1-based line numbers.
    for (size_t i = 0; i < lines.size(); i++) {
      sink << "DA:" << (i + 1) << "," << lines[i] << std::endl;
    }
    sink << "end_of_record" << std::endl;
  }
}

void Shell::OnExit(v8::Isolate* isolate) {
  // Dump basic block profiling data.
  if (i::BasicBlockProfiler* profiler =
          reinterpret_cast<i::Isolate*>(isolate)->basic_block_profiler()) {
    i::OFStream os(stdout);
    os << *profiler;
  }
  isolate->Dispose();

  if (i::FLAG_dump_counters || i::FLAG_dump_counters_nvp) {
    int number_of_counters = 0;
    for (CounterMap::Iterator i(counter_map_); i.More(); i.Next()) {
      number_of_counters++;
    }
    CounterAndKey* counters = new CounterAndKey[number_of_counters];
    int j = 0;
    for (CounterMap::Iterator i(counter_map_); i.More(); i.Next(), j++) {
      counters[j].counter = i.CurrentValue();
      counters[j].key = i.CurrentKey();
    }
    std::sort(counters, counters + number_of_counters);

    if (i::FLAG_dump_counters_nvp) {
      // Dump counters as name-value pairs.
      for (j = 0; j < number_of_counters; j++) {
        Counter* counter = counters[j].counter;
        const char* key = counters[j].key;
        if (counter->is_histogram()) {
          printf("\"c:%s\"=%i\n", key, counter->count());
          printf("\"t:%s\"=%i\n", key, counter->sample_total());
        } else {
          printf("\"%s\"=%i\n", key, counter->count());
        }
      }
    } else {
      // Dump counters in formatted boxes.
      printf(
          "+----------------------------------------------------------------+"
          "-------------+\n");
      printf(
          "| Name                                                           |"
          " Value       |\n");
      printf(
          "+----------------------------------------------------------------+"
          "-------------+\n");
      for (j = 0; j < number_of_counters; j++) {
        Counter* counter = counters[j].counter;
        const char* key = counters[j].key;
        if (counter->is_histogram()) {
          printf("| c:%-60s | %11i |\n", key, counter->count());
          printf("| t:%-60s | %11i |\n", key, counter->sample_total());
        } else {
          printf("| %-62s | %11i |\n", key, counter->count());
        }
      }
      printf(
          "+----------------------------------------------------------------+"
          "-------------+\n");
    }
    delete [] counters;
  }

  delete counters_file_;
  delete counter_map_;
}


static FILE* FOpen(const char* path, const char* mode) {
#if defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64))
  FILE* result;
  if (fopen_s(&result, path, mode) == 0) {
    return result;
  } else {
    return NULL;
  }
#else
  FILE* file = fopen(path, mode);
  if (file == NULL) return NULL;
  struct stat file_stat;
  if (fstat(fileno(file), &file_stat) != 0) return NULL;
  bool is_regular_file = ((file_stat.st_mode & S_IFREG) != 0);
  if (is_regular_file) return file;
  fclose(file);
  return NULL;
#endif
}

static char* ReadChars(const char* name, int* size_out) {
  FILE* file = FOpen(name, "rb");
  if (file == NULL) return NULL;

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (size_t i = 0; i < size;) {
    i += fread(&chars[i], 1, size - i, file);
    if (ferror(file)) {
      fclose(file);
      delete[] chars;
      return nullptr;
    }
  }
  fclose(file);
  *size_out = static_cast<int>(size);
  return chars;
}


struct DataAndPersistent {
  uint8_t* data;
  int byte_length;
  Global<ArrayBuffer> handle;
};


static void ReadBufferWeakCallback(
    const v8::WeakCallbackInfo<DataAndPersistent>& data) {
  int byte_length = data.GetParameter()->byte_length;
  data.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(
      -static_cast<intptr_t>(byte_length));

  delete[] data.GetParameter()->data;
  data.GetParameter()->handle.Reset();
  delete data.GetParameter();
}


void Shell::ReadBuffer(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(sizeof(char) == sizeof(uint8_t));  // NOLINT
  String::Utf8Value filename(args[0]);
  int length;
  Isolate* isolate = args.GetIsolate();
  if (*filename == NULL) {
    Throw(isolate, "Error loading file");
    return;
  }

  DataAndPersistent* data = new DataAndPersistent;
  data->data = reinterpret_cast<uint8_t*>(ReadChars(*filename, &length));
  if (data->data == NULL) {
    delete data;
    Throw(isolate, "Error reading file");
    return;
  }
  data->byte_length = length;
  Local<v8::ArrayBuffer> buffer = ArrayBuffer::New(isolate, data->data, length);
  data->handle.Reset(isolate, buffer);
  data->handle.SetWeak(data, ReadBufferWeakCallback,
                       v8::WeakCallbackType::kParameter);
  data->handle.MarkIndependent();
  isolate->AdjustAmountOfExternalAllocatedMemory(length);

  args.GetReturnValue().Set(buffer);
}

// Reads a file into a v8 string.
Local<String> Shell::ReadFile(Isolate* isolate, const char* name) {
  int size = 0;
  char* chars = ReadChars(name, &size);
  if (chars == NULL) return Local<String>();
  Local<String> result;
  if (i::FLAG_use_external_strings && internal::String::IsAscii(chars, size)) {
    String::ExternalOneByteStringResource* resource =
        new ExternalOwningOneByteStringResource(
            std::unique_ptr<const char[]>(chars), size);
    result = String::NewExternalOneByte(isolate, resource).ToLocalChecked();
  } else {
    result = String::NewFromUtf8(isolate, chars, NewStringType::kNormal, size)
                 .ToLocalChecked();
    delete[] chars;
  }
  return result;
}


void Shell::RunShell(Isolate* isolate) {
  HandleScope outer_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, evaluation_context_);
  v8::Context::Scope context_scope(context);
  PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
  Local<String> name =
      String::NewFromUtf8(isolate, "(d8)", NewStringType::kNormal)
          .ToLocalChecked();
  printf("V8 version %s\n", V8::GetVersion());
  while (true) {
    HandleScope inner_scope(isolate);
    printf("d8> ");
    Local<String> input = Shell::ReadFromStdin(isolate);
    if (input.IsEmpty()) break;
    ExecuteString(isolate, input, name, true, true);
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
  virtual ~InspectorFrontend() = default;

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
    int length = static_cast<int>(string.length());
    DCHECK(length < v8::String::kMaxLength);
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
    Local<String> callback_name =
        v8::String::NewFromUtf8(isolate_, "receive", v8::NewStringType::kNormal)
            .ToLocalChecked();
    Local<Context> context = context_.Get(isolate_);
    Local<Value> callback =
        context->Global()->Get(context, callback_name).ToLocalChecked();
    if (callback->IsFunction()) {
      v8::TryCatch try_catch(isolate_);
      Local<Value> args[] = {message};
      USE(Local<Function>::Cast(callback)->Call(context, Undefined(isolate_), 1,
                                                args));
#ifdef DEBUG
      if (try_catch.HasCaught()) {
        Local<Object> exception = Local<Object>::Cast(try_catch.Exception());
        Local<String> key = v8::String::NewFromUtf8(isolate_, "message",
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked();
        Local<String> expected =
            v8::String::NewFromUtf8(isolate_,
                                    "Maximum call stack size exceeded",
                                    v8::NewStringType::kNormal)
                .ToLocalChecked();
        Local<Value> value = exception->Get(context, key).ToLocalChecked();
        CHECK(value->StrictEquals(expected));
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
    Local<String> function_name =
        String::NewFromUtf8(isolate_, "send", NewStringType::kNormal)
            .ToLocalChecked();
    CHECK(context->Global()->Set(context, function_name, function).FromJust());

    v8::debug::SetLiveEditEnabled(isolate_, true);

    context_.Reset(isolate_, context);
  }

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
    message->Write(buffer.get(), 0, length);
    v8_inspector::StringView message_view(buffer.get(), length);
    session->dispatchProtocolMessage(message_view);
    args.GetReturnValue().Set(True(isolate));
  }

  static const int kContextGroupId = 1;

  std::unique_ptr<v8_inspector::V8Inspector> inspector_;
  std::unique_ptr<v8_inspector::V8InspectorSession> session_;
  std::unique_ptr<v8_inspector::V8Inspector::Channel> channel_;
  Global<Context> context_;
  Isolate* isolate_;
};

SourceGroup::~SourceGroup() {
  delete thread_;
  thread_ = NULL;
}


void SourceGroup::Execute(Isolate* isolate) {
  bool exception_was_thrown = false;
  for (int i = begin_offset_; i < end_offset_; ++i) {
    const char* arg = argv_[i];
    if (strcmp(arg, "-e") == 0 && i + 1 < end_offset_) {
      // Execute argument given to -e option directly.
      HandleScope handle_scope(isolate);
      Local<String> file_name =
          String::NewFromUtf8(isolate, "unnamed", NewStringType::kNormal)
              .ToLocalChecked();
      Local<String> source =
          String::NewFromUtf8(isolate, argv_[i + 1], NewStringType::kNormal)
              .ToLocalChecked();
      Shell::options.script_executed = true;
      if (!Shell::ExecuteString(isolate, source, file_name, false, true)) {
        exception_was_thrown = true;
        break;
      }
      ++i;
      continue;
    } else if (strcmp(arg, "--module") == 0 && i + 1 < end_offset_) {
      // Treat the next file as a module.
      arg = argv_[++i];
      Shell::options.script_executed = true;
      if (!Shell::ExecuteModule(isolate, arg)) {
        exception_was_thrown = true;
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
        String::NewFromUtf8(isolate, arg, NewStringType::kNormal)
            .ToLocalChecked();
    Local<String> source = ReadFile(isolate, arg);
    if (source.IsEmpty()) {
      printf("Error reading '%s'\n", arg);
      Shell::Exit(1);
    }
    Shell::options.script_executed = true;
    if (!Shell::ExecuteString(isolate, source, file_name, false, true)) {
      exception_was_thrown = true;
      break;
    }
  }
  if (exception_was_thrown != Shell::options.expected_to_throw) {
    Shell::Exit(1);
  }
}

Local<String> SourceGroup::ReadFile(Isolate* isolate, const char* name) {
  return Shell::ReadFile(isolate, name);
}


base::Thread::Options SourceGroup::GetThreadOptions() {
  // On some systems (OSX 10.6) the stack size default is 0.5Mb or less
  // which is not enough to parse the big literal expressions used in tests.
  // The stack size should be at least StackGuard::kLimitSize + some
  // OS-specific padding for thread startup code.  2Mbytes seems to be enough.
  return base::Thread::Options("IsolateThread", 2 * MB);
}

void SourceGroup::ExecuteInThread() {
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = Shell::array_buffer_allocator;
  create_params.host_import_module_dynamically_callback_ =
      Shell::HostImportModuleDynamically;
  Isolate* isolate = Isolate::New(create_params);

  Shell::EnsureEventLoopInitialized(isolate);
  D8Console console(isolate);
  debug::SetConsoleDelegate(isolate, &console);
  for (int i = 0; i < Shell::options.stress_runs; ++i) {
    next_semaphore_.Wait();
    {
      Isolate::Scope iscope(isolate);
      {
        HandleScope scope(isolate);
        PerIsolateData data(isolate);
        Local<Context> context = Shell::CreateEvaluationContext(isolate);
        {
          Context::Scope cscope(context);
          InspectorClient inspector_client(context,
                                           Shell::options.enable_inspector);
          PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
          Execute(isolate);
        }
        DisposeModuleEmbedderData(context);
      }
      Shell::CollectGarbage(isolate);
      Shell::CompleteMessageLoop(isolate);
    }
    done_semaphore_.Signal();
  }

  isolate->Dispose();
}


void SourceGroup::StartExecuteInThread() {
  if (thread_ == NULL) {
    thread_ = new IsolateThread(this);
    thread_->Start();
  }
  next_semaphore_.Signal();
}


void SourceGroup::WaitForThread() {
  if (thread_ == NULL) return;
  done_semaphore_.Wait();
}


void SourceGroup::JoinThread() {
  if (thread_ == NULL) return;
  thread_->Join();
}

ExternalizedContents::~ExternalizedContents() {
  Shell::array_buffer_allocator->Free(data_, size_);
}

void SerializationDataQueue::Enqueue(std::unique_ptr<SerializationData> data) {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  data_.push_back(std::move(data));
}

bool SerializationDataQueue::Dequeue(
    std::unique_ptr<SerializationData>* out_data) {
  out_data->reset();
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  if (data_.empty()) return false;
  *out_data = std::move(data_[0]);
  data_.erase(data_.begin());
  return true;
}


bool SerializationDataQueue::IsEmpty() {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  return data_.empty();
}


void SerializationDataQueue::Clear() {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  data_.clear();
}


Worker::Worker()
    : in_semaphore_(0),
      out_semaphore_(0),
      thread_(NULL),
      script_(NULL),
      running_(false) {}


Worker::~Worker() {
  delete thread_;
  thread_ = NULL;
  delete[] script_;
  script_ = NULL;
  in_queue_.Clear();
  out_queue_.Clear();
}


void Worker::StartExecuteInThread(const char* script) {
  running_ = true;
  script_ = i::StrDup(script);
  thread_ = new WorkerThread(this);
  thread_->Start();
}

void Worker::PostMessage(std::unique_ptr<SerializationData> data) {
  in_queue_.Enqueue(std::move(data));
  in_semaphore_.Signal();
}

std::unique_ptr<SerializationData> Worker::GetMessage() {
  std::unique_ptr<SerializationData> result;
  while (!out_queue_.Dequeue(&result)) {
    // If the worker is no longer running, and there are no messages in the
    // queue, don't expect any more messages from it.
    if (!base::NoBarrier_Load(&running_)) break;
    out_semaphore_.Wait();
  }
  return result;
}


void Worker::Terminate() {
  base::NoBarrier_Store(&running_, false);
  // Post NULL to wake the Worker thread message loop, and tell it to stop
  // running.
  PostMessage(NULL);
}


void Worker::WaitForThread() {
  Terminate();
  thread_->Join();
}


void Worker::ExecuteInThread() {
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = Shell::array_buffer_allocator;
  create_params.host_import_module_dynamically_callback_ =
      Shell::HostImportModuleDynamically;
  Isolate* isolate = Isolate::New(create_params);
  D8Console console(isolate);
  debug::SetConsoleDelegate(isolate, &console);
  {
    Isolate::Scope iscope(isolate);
    {
      HandleScope scope(isolate);
      PerIsolateData data(isolate);
      Local<Context> context = Shell::CreateEvaluationContext(isolate);
      {
        Context::Scope cscope(context);
        PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));

        Local<Object> global = context->Global();
        Local<Value> this_value = External::New(isolate, this);
        Local<FunctionTemplate> postmessage_fun_template =
            FunctionTemplate::New(isolate, PostMessageOut, this_value);

        Local<Function> postmessage_fun;
        if (postmessage_fun_template->GetFunction(context)
                .ToLocal(&postmessage_fun)) {
          global->Set(context, String::NewFromUtf8(isolate, "postMessage",
                                                   NewStringType::kNormal)
                                   .ToLocalChecked(),
                      postmessage_fun).FromJust();
        }

        // First run the script
        Local<String> file_name =
            String::NewFromUtf8(isolate, "unnamed", NewStringType::kNormal)
                .ToLocalChecked();
        Local<String> source =
            String::NewFromUtf8(isolate, script_, NewStringType::kNormal)
                .ToLocalChecked();
        if (Shell::ExecuteString(isolate, source, file_name, false, true)) {
          // Get the message handler
          Local<Value> onmessage =
              global->Get(context, String::NewFromUtf8(isolate, "onmessage",
                                                       NewStringType::kNormal)
                                       .ToLocalChecked()).ToLocalChecked();
          if (onmessage->IsFunction()) {
            Local<Function> onmessage_fun = Local<Function>::Cast(onmessage);
            // Now wait for messages
            while (true) {
              in_semaphore_.Wait();
              std::unique_ptr<SerializationData> data;
              if (!in_queue_.Dequeue(&data)) continue;
              if (!data) {
                break;
              }
              v8::TryCatch try_catch(isolate);
              Local<Value> value;
              if (Shell::DeserializeValue(isolate, std::move(data))
                      .ToLocal(&value)) {
                Local<Value> argv[] = {value};
                (void)onmessage_fun->Call(context, global, 1, argv);
              }
              if (try_catch.HasCaught()) {
                Shell::ReportException(isolate, &try_catch);
              }
            }
          }
        }
      }
      DisposeModuleEmbedderData(context);
    }
    Shell::CollectGarbage(isolate);
  }
  isolate->Dispose();

  // Post NULL to wake the thread waiting on GetMessage() if there is one.
  out_queue_.Enqueue(NULL);
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
    Local<External> this_value = Local<External>::Cast(args.Data());
    Worker* worker = static_cast<Worker*>(this_value->Value());
    worker->out_queue_.Enqueue(std::move(data));
    worker->out_semaphore_.Signal();
  }
}


void SetFlagsFromString(const char* flags) {
  v8::V8::SetFlagsFromString(flags, static_cast<int>(strlen(flags)));
}


bool Shell::SetOptions(int argc, char* argv[]) {
  bool logfile_per_isolate = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--stress-opt") == 0) {
      options.stress_opt = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--nostress-opt") == 0 ||
               strcmp(argv[i], "--no-stress-opt") == 0) {
      options.stress_opt = false;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--stress-deopt") == 0) {
      options.stress_deopt = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--mock-arraybuffer-allocator") == 0) {
      options.mock_arraybuffer_allocator = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--noalways-opt") == 0 ||
               strcmp(argv[i], "--no-always-opt") == 0) {
      // No support for stressing if we can't use --always-opt.
      options.stress_opt = false;
      options.stress_deopt = false;
    } else if (strcmp(argv[i], "--logfile-per-isolate") == 0) {
      logfile_per_isolate = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--shell") == 0) {
      options.interactive_shell = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--test") == 0) {
      options.test_shell = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--notest") == 0 ||
               strcmp(argv[i], "--no-test") == 0) {
      options.test_shell = false;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--send-idle-notification") == 0) {
      options.send_idle_notification = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--invoke-weak-callbacks") == 0) {
      options.invoke_weak_callbacks = true;
      // TODO(jochen) See issue 3351
      options.send_idle_notification = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--omit-quit") == 0) {
      options.omit_quit = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "-f") == 0) {
      // Ignore any -f flags for compatibility with other stand-alone
      // JavaScript engines.
      continue;
    } else if (strcmp(argv[i], "--isolate") == 0) {
      options.num_isolates++;
    } else if (strcmp(argv[i], "--throws") == 0) {
      options.expected_to_throw = true;
      argv[i] = NULL;
    } else if (strncmp(argv[i], "--icu-data-file=", 16) == 0) {
      options.icu_data_file = argv[i] + 16;
      argv[i] = NULL;
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
    } else if (strncmp(argv[i], "--natives_blob=", 15) == 0) {
      options.natives_blob = argv[i] + 15;
      argv[i] = NULL;
    } else if (strncmp(argv[i], "--snapshot_blob=", 16) == 0) {
      options.snapshot_blob = argv[i] + 16;
      argv[i] = NULL;
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
    } else if (strcmp(argv[i], "--cache") == 0 ||
               strncmp(argv[i], "--cache=", 8) == 0) {
      const char* value = argv[i] + 7;
      if (!*value || strncmp(value, "=code", 6) == 0) {
        options.compile_options = v8::ScriptCompiler::kProduceCodeCache;
      } else if (strncmp(value, "=parse", 7) == 0) {
        options.compile_options = v8::ScriptCompiler::kProduceParserCache;
      } else if (strncmp(value, "=none", 6) == 0) {
        options.compile_options = v8::ScriptCompiler::kNoCompileOptions;
      } else {
        printf("Unknown option to --cache.\n");
        return false;
      }
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--enable-tracing") == 0) {
      options.trace_enabled = true;
      argv[i] = NULL;
    } else if (strncmp(argv[i], "--trace-config=", 15) == 0) {
      options.trace_config = argv[i] + 15;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--enable-inspector") == 0) {
      options.enable_inspector = true;
      argv[i] = NULL;
    } else if (strncmp(argv[i], "--lcov=", 7) == 0) {
      options.lcov_file = argv[i] + 7;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--disable-in-process-stack-traces") == 0) {
      options.disable_in_process_stack_traces = true;
      argv[i] = NULL;
    }
  }

  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);

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
    } else if (strncmp(argv[i], "--", 2) == 0) {
      printf("Warning: unknown flag %s.\nTry --help for options\n", argv[i]);
    } else if (strcmp(str, "-e") == 0 && i + 1 < argc) {
      options.script_executed = true;
    } else if (strncmp(str, "-", 1) != 0) {
      // Not a flag, so it must be a script to execute.
      options.script_executed = true;
    }
  }
  current->End(argc);

  if (!logfile_per_isolate && options.num_isolates) {
    SetFlagsFromString("--nologfile_per_isolate");
  }

  return true;
}


int Shell::RunMain(Isolate* isolate, int argc, char* argv[], bool last_run) {
  for (int i = 1; i < options.num_isolates; ++i) {
    options.isolate_sources[i].StartExecuteInThread();
  }
  {
    EnsureEventLoopInitialized(isolate);
    if (options.lcov_file) {
      debug::Coverage::SelectMode(isolate, debug::Coverage::kPreciseCount);
    }
    HandleScope scope(isolate);
    Local<Context> context = CreateEvaluationContext(isolate);
    bool use_existing_context = last_run && options.use_interactive_shell();
    if (use_existing_context) {
      // Keep using the same context in the interactive shell.
      evaluation_context_.Reset(isolate, context);
    }
    {
      Context::Scope cscope(context);
      InspectorClient inspector_client(context, options.enable_inspector);
      PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
      options.isolate_sources[0].Execute(isolate);
    }
    if (!use_existing_context) {
      DisposeModuleEmbedderData(context);
    }
    WriteLcovData(isolate, options.lcov_file);
  }
  CollectGarbage(isolate);
  CompleteMessageLoop(isolate);
  for (int i = 1; i < options.num_isolates; ++i) {
    if (last_run) {
      options.isolate_sources[i].JoinThread();
    } else {
      options.isolate_sources[i].WaitForThread();
    }
  }
  CleanupWorkers();
  return 0;
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

void Shell::EnsureEventLoopInitialized(Isolate* isolate) {
  v8::platform::EnsureEventLoopInitialized(GetDefaultPlatform(), isolate);
  SetWaitUntilDone(isolate, false);
}

void Shell::SetWaitUntilDone(Isolate* isolate, bool value) {
  base::LockGuard<base::Mutex> guard(isolate_status_lock_.Pointer());
  if (isolate_status_.count(isolate) == 0) {
    isolate_status_.insert(std::make_pair(isolate, value));
  } else {
    isolate_status_[isolate] = value;
  }
}

bool Shell::IsWaitUntilDone(Isolate* isolate) {
  base::LockGuard<base::Mutex> guard(isolate_status_lock_.Pointer());
  DCHECK_GT(isolate_status_.count(isolate), 0);
  return isolate_status_[isolate];
}

void Shell::CompleteMessageLoop(Isolate* isolate) {
  Platform* platform = GetDefaultPlatform();
  while (v8::platform::PumpMessageLoop(
      platform, isolate,
      Shell::IsWaitUntilDone(isolate)
          ? platform::MessageLoopBehavior::kWaitForWork
          : platform::MessageLoopBehavior::kDoNotWait)) {
    isolate->RunMicrotasks();
  }
  if (platform->IdleTasksEnabled(isolate)) {
    v8::platform::RunIdleTasks(platform, isolate,
                               50.0 / base::Time::kMillisecondsPerSecond);
  }
}

void Shell::EmptyMessageQueues(Isolate* isolate) {
  Platform* platform = GetDefaultPlatform();
  // Pump the message loop until it is empty.
  while (v8::platform::PumpMessageLoop(
      platform, isolate, platform::MessageLoopBehavior::kDoNotWait)) {
    isolate->RunMicrotasks();
  }
  // Run the idle tasks.
  if (platform->IdleTasksEnabled(isolate)) {
    v8::platform::RunIdleTasks(platform, isolate,
                               50.0 / base::Time::kMillisecondsPerSecond);
  }
}

class Serializer : public ValueSerializer::Delegate {
 public:
  explicit Serializer(Isolate* isolate)
      : isolate_(isolate),
        serializer_(isolate, this),
        current_memory_usage_(0) {}

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

 protected:
  // Implements ValueSerializer::Delegate.
  void ThrowDataCloneError(Local<String> message) override {
    isolate_->ThrowException(Exception::Error(message));
  }

  Maybe<uint32_t> GetSharedArrayBufferId(
      Isolate* isolate, Local<SharedArrayBuffer> shared_array_buffer) override {
    DCHECK(data_ != nullptr);
    for (size_t index = 0; index < shared_array_buffers_.size(); ++index) {
      if (shared_array_buffers_[index] == shared_array_buffer) {
        return Just<uint32_t>(static_cast<uint32_t>(index));
      }
    }

    size_t index = shared_array_buffers_.size();
    shared_array_buffers_.emplace_back(isolate_, shared_array_buffer);
    return Just<uint32_t>(static_cast<uint32_t>(index));
  }

  void* ReallocateBufferMemory(void* old_buffer, size_t size,
                               size_t* actual_size) override {
    // Not accurate, because we don't take into account reallocated buffers,
    // but this is fine for testing.
    current_memory_usage_ += size;
    if (current_memory_usage_ > kMaxSerializerMemoryUsage) return nullptr;

    void* result = realloc(old_buffer, size);
    *actual_size = result ? size : 0;
    return result;
  }

  void FreeBufferMemory(void* buffer) override { free(buffer); }

 private:
  Maybe<bool> PrepareTransfer(Local<Context> context, Local<Value> transfer) {
    if (transfer->IsArray()) {
      Local<Array> transfer_array = Local<Array>::Cast(transfer);
      uint32_t length = transfer_array->Length();
      for (uint32_t i = 0; i < length; ++i) {
        Local<Value> element;
        if (transfer_array->Get(context, i).ToLocal(&element)) {
          if (!element->IsArrayBuffer()) {
            Throw(isolate_, "Transfer array elements must be an ArrayBuffer");
            break;
          }

          Local<ArrayBuffer> array_buffer = Local<ArrayBuffer>::Cast(element);
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

  template <typename T>
  typename T::Contents MaybeExternalize(Local<T> array_buffer) {
    if (array_buffer->IsExternal()) {
      return array_buffer->GetContents();
    } else {
      typename T::Contents contents = array_buffer->Externalize();
      data_->externalized_contents_.emplace_back(contents);
      return contents;
    }
  }

  Maybe<bool> FinalizeTransfer() {
    for (const auto& global_array_buffer : array_buffers_) {
      Local<ArrayBuffer> array_buffer =
          Local<ArrayBuffer>::New(isolate_, global_array_buffer);
      if (!array_buffer->IsNeuterable()) {
        Throw(isolate_, "ArrayBuffer could not be transferred");
        return Nothing<bool>();
      }

      ArrayBuffer::Contents contents = MaybeExternalize(array_buffer);
      array_buffer->Neuter();
      data_->array_buffer_contents_.push_back(contents);
    }

    for (const auto& global_shared_array_buffer : shared_array_buffers_) {
      Local<SharedArrayBuffer> shared_array_buffer =
          Local<SharedArrayBuffer>::New(isolate_, global_shared_array_buffer);
      data_->shared_array_buffer_contents_.push_back(
          MaybeExternalize(shared_array_buffer));
    }

    return Just(true);
  }

  Isolate* isolate_;
  ValueSerializer serializer_;
  std::unique_ptr<SerializationData> data_;
  std::vector<Global<ArrayBuffer>> array_buffers_;
  std::vector<Global<SharedArrayBuffer>> shared_array_buffers_;
  size_t current_memory_usage_;

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};

class Deserializer : public ValueDeserializer::Delegate {
 public:
  Deserializer(Isolate* isolate, std::unique_ptr<SerializationData> data)
      : isolate_(isolate),
        deserializer_(isolate, data->data(), data->size(), this),
        data_(std::move(data)) {
    deserializer_.SetSupportsLegacyWireFormat(true);
  }

  MaybeLocal<Value> ReadValue(Local<Context> context) {
    bool read_header;
    if (!deserializer_.ReadHeader(context).To(&read_header)) {
      return MaybeLocal<Value>();
    }

    uint32_t index = 0;
    for (const auto& contents : data_->array_buffer_contents()) {
      Local<ArrayBuffer> array_buffer =
          ArrayBuffer::New(isolate_, contents.Data(), contents.ByteLength());
      deserializer_.TransferArrayBuffer(index++, array_buffer);
    }

    index = 0;
    for (const auto& contents : data_->shared_array_buffer_contents()) {
      Local<SharedArrayBuffer> shared_array_buffer = SharedArrayBuffer::New(
          isolate_, contents.Data(), contents.ByteLength());
      deserializer_.TransferSharedArrayBuffer(index++, shared_array_buffer);
    }

    return deserializer_.ReadValue(context);
  }

 private:
  Isolate* isolate_;
  ValueDeserializer deserializer_;
  std::unique_ptr<SerializationData> data_;

  DISALLOW_COPY_AND_ASSIGN(Deserializer);
};

std::unique_ptr<SerializationData> Shell::SerializeValue(
    Isolate* isolate, Local<Value> value, Local<Value> transfer) {
  bool ok;
  Local<Context> context = isolate->GetCurrentContext();
  Serializer serializer(isolate);
  if (serializer.WriteValue(context, value, transfer).To(&ok)) {
    std::unique_ptr<SerializationData> data = serializer.Release();
    base::LockGuard<base::Mutex> lock_guard(workers_mutex_.Pointer());
    data->AppendExternalizedContentsTo(&externalized_contents_);
    return data;
  }
  return nullptr;
}

MaybeLocal<Value> Shell::DeserializeValue(
    Isolate* isolate, std::unique_ptr<SerializationData> data) {
  Local<Value> value;
  Local<Context> context = isolate->GetCurrentContext();
  Deserializer deserializer(isolate, std::move(data));
  return deserializer.ReadValue(context);
}


void Shell::CleanupWorkers() {
  // Make a copy of workers_, because we don't want to call Worker::Terminate
  // while holding the workers_mutex_ lock. Otherwise, if a worker is about to
  // create a new Worker, it would deadlock.
  i::List<Worker*> workers_copy;
  {
    base::LockGuard<base::Mutex> lock_guard(workers_mutex_.Pointer());
    allow_new_workers_ = false;
    workers_copy.AddAll(workers_);
    workers_.Clear();
  }

  for (int i = 0; i < workers_copy.length(); ++i) {
    Worker* worker = workers_copy[i];
    worker->WaitForThread();
    delete worker;
  }

  // Now that all workers are terminated, we can re-enable Worker creation.
  base::LockGuard<base::Mutex> lock_guard(workers_mutex_.Pointer());
  allow_new_workers_ = true;
  externalized_contents_.clear();
}

int Shell::Main(int argc, char* argv[]) {
  std::ofstream trace_file;
#if (defined(_WIN32) || defined(_WIN64))
  UINT new_flags =
      SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
#if defined(_MSC_VER)
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _set_error_mode(_OUT_TO_STDERR);
#endif  // defined(_MSC_VER)
#endif  // defined(_WIN32) || defined(_WIN64)
  if (!SetOptions(argc, argv)) return 1;
  v8::V8::InitializeICUDefaultLocation(argv[0], options.icu_data_file);

  v8::platform::InProcessStackDumping in_process_stack_dumping =
      options.disable_in_process_stack_traces
          ? v8::platform::InProcessStackDumping::kDisabled
          : v8::platform::InProcessStackDumping::kEnabled;

  g_platform = v8::platform::CreateDefaultPlatform(
      0, v8::platform::IdleTaskSupport::kEnabled, in_process_stack_dumping);
  if (i::FLAG_verify_predictable) {
    g_platform = new PredictablePlatform(std::unique_ptr<Platform>(g_platform));
  }

  platform::tracing::TracingController* tracing_controller = nullptr;
  if (options.trace_enabled && !i::FLAG_verify_predictable) {
    trace_file.open("v8_trace.json");
    tracing_controller = new platform::tracing::TracingController();
    platform::tracing::TraceBuffer* trace_buffer =
        platform::tracing::TraceBuffer::CreateTraceBufferRingBuffer(
            platform::tracing::TraceBuffer::kRingBufferChunks,
            platform::tracing::TraceWriter::CreateJSONTraceWriter(trace_file));
    tracing_controller->Initialize(trace_buffer);
    platform::SetTracingController(g_platform, tracing_controller);
  }

  v8::V8::InitializePlatform(g_platform);
  v8::V8::Initialize();
  if (options.natives_blob || options.snapshot_blob) {
    v8::V8::InitializeExternalStartupData(options.natives_blob,
                                          options.snapshot_blob);
  } else {
    v8::V8::InitializeExternalStartupData(argv[0]);
  }
  SetFlagsFromString("--trace-hydrogen-file=hydrogen.cfg");
  SetFlagsFromString("--trace-turbo-cfg-file=turbo.cfg");
  SetFlagsFromString("--redirect-code-traces-to=code.asm");
  int result = 0;
  Isolate::CreateParams create_params;
  ShellArrayBufferAllocator shell_array_buffer_allocator;
  MockArrayBufferAllocator mock_arraybuffer_allocator;
  if (options.mock_arraybuffer_allocator) {
    Shell::array_buffer_allocator = &mock_arraybuffer_allocator;
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
  if (i::FLAG_dump_counters || i::FLAG_dump_counters_nvp || i::FLAG_gc_stats) {
    create_params.counter_lookup_callback = LookupCounter;
    create_params.create_histogram_callback = CreateHistogram;
    create_params.add_histogram_sample_callback = AddHistogramSample;
  }

  create_params.host_import_module_dynamically_callback_ =
      Shell::HostImportModuleDynamically;

  if (i::trap_handler::UseTrapHandler()) {
    if (!v8::V8::RegisterDefaultSignalHandler()) {
      fprintf(stderr, "Could not register signal handler");
      exit(1);
    }
  }

  Isolate* isolate = Isolate::New(create_params);

  D8Console console(isolate);
  {
    Isolate::Scope scope(isolate);
    Initialize(isolate);
    PerIsolateData data(isolate);
    debug::SetConsoleDelegate(isolate, &console);

    if (options.trace_enabled) {
      platform::tracing::TraceConfig* trace_config;
      if (options.trace_config) {
        int size = 0;
        char* trace_config_json_str = ReadChars(options.trace_config, &size);
        trace_config =
            tracing::CreateTraceConfigFromJSON(isolate, trace_config_json_str);
        delete[] trace_config_json_str;
      } else {
        trace_config =
            platform::tracing::TraceConfig::CreateDefaultTraceConfig();
      }
      tracing_controller->StartTracing(trace_config);
    }

    if (options.stress_opt || options.stress_deopt) {
      Testing::SetStressRunType(options.stress_opt
                                ? Testing::kStressTypeOpt
                                : Testing::kStressTypeDeopt);
      options.stress_runs = Testing::GetStressRuns();
      for (int i = 0; i < options.stress_runs && result == 0; i++) {
        printf("============ Stress %d/%d ============\n", i + 1,
               options.stress_runs);
        Testing::PrepareStressRun(i);
        bool last_run = i == options.stress_runs - 1;
        result = RunMain(isolate, argc, argv, last_run);
      }
      printf("======== Full Deoptimization =======\n");
      Testing::DeoptimizeAll(isolate);
    } else if (i::FLAG_stress_runs > 0) {
      options.stress_runs = i::FLAG_stress_runs;
      for (int i = 0; i < options.stress_runs && result == 0; i++) {
        printf("============ Run %d/%d ============\n", i + 1,
               options.stress_runs);
        bool last_run = i == options.stress_runs - 1;
        result = RunMain(isolate, argc, argv, last_run);
      }
    } else {
      bool last_run = true;
      result = RunMain(isolate, argc, argv, last_run);
    }

    // Run interactive shell if explicitly requested or if no script has been
    // executed, but never on --test
    if (options.use_interactive_shell()) {
      RunShell(isolate);
    }

    if (i::FLAG_trace_ignition_dispatches &&
        i::FLAG_trace_ignition_dispatches_output_file != nullptr) {
      WriteIgnitionDispatchCountersFile(isolate);
    }

    // Shut down contexts and collect garbage.
    evaluation_context_.Reset();
    stringify_function_.Reset();
    CollectGarbage(isolate);
  }
  OnExit(isolate);
  V8::Dispose();
  V8::ShutdownPlatform();
  delete g_platform;

  return result;
}

}  // namespace v8


#ifndef GOOGLE3
int main(int argc, char* argv[]) {
  return v8::Shell::Main(argc, argv);
}
#endif
