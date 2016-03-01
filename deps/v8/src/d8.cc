// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Defined when linking against shared lib on Windows.
#if defined(USING_V8_SHARED) && !defined(V8_SHARED)
#define V8_SHARED
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef V8_SHARED
#include <assert.h>
#endif  // V8_SHARED

#ifndef V8_SHARED
#include <algorithm>
#include <vector>
#endif  // !V8_SHARED

#ifdef V8_SHARED
#include "include/v8-testing.h"
#endif  // V8_SHARED

#if !defined(V8_SHARED) && defined(ENABLE_GDB_JIT_INTERFACE)
#include "src/gdb-jit.h"
#endif

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "src/third_party/vtune/v8-vtune.h"
#endif

#include "src/d8.h"

#include "include/libplatform/libplatform.h"
#ifndef V8_SHARED
#include "src/api.h"
#include "src/base/cpu.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/sys-info.h"
#include "src/basic-block-profiler.h"
#include "src/snapshot/natives.h"
#include "src/utils.h"
#include "src/v8.h"
#endif  // !V8_SHARED

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
#ifndef V8_SHARED
const int kMaxWorkers = 50;
#endif


class ShellArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};


class MockArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override {
    size_t actual_length = length > 10 * MB ? 1 : length;
    void* data = AllocateUninitialized(actual_length);
    return data == NULL ? data : memset(data, 0, actual_length);
  }
  void* AllocateUninitialized(size_t length) override {
    return length > 10 * MB ? malloc(1) : malloc(length);
  }
  void Free(void* p, size_t) override { free(p); }
};


#ifndef V8_SHARED
// Predictable v8::Platform implementation. All background and foreground
// tasks are run immediately, delayed tasks are not executed at all.
class PredictablePlatform : public Platform {
 public:
  PredictablePlatform() {}

  void CallOnBackgroundThread(Task* task,
                              ExpectedRuntime expected_runtime) override {
    task->Run();
    delete task;
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    task->Run();
    delete task;
  }

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, Task* task,
                                     double delay_in_seconds) override {
    delete task;
  }

  void CallIdleOnForegroundThread(v8::Isolate* isolate,
                                  IdleTask* task) override {
    UNREACHABLE();
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  double MonotonicallyIncreasingTime() override {
    return synthetic_time_in_sec_ += 0.00001;
  }

  uint64_t AddTraceEvent(char phase, const uint8_t* categoryEnabledFlag,
                         const char* name, uint64_t id, uint64_t bind_id,
                         int numArgs, const char** argNames,
                         const uint8_t* argTypes, const uint64_t* argValues,
                         unsigned int flags) override {
    return 0;
  }

  void UpdateTraceEventDuration(const uint8_t* categoryEnabledFlag,
                                const char* name, uint64_t handle) override {}

  const uint8_t* GetCategoryGroupEnabled(const char* name) override {
    static uint8_t no = 0;
    return &no;
  }

  const char* GetCategoryGroupName(
      const uint8_t* categoryEnabledFlag) override {
    static const char* dummy = "dummy";
    return dummy;
  }

 private:
  double synthetic_time_in_sec_ = 0.0;

  DISALLOW_COPY_AND_ASSIGN(PredictablePlatform);
};
#endif  // !V8_SHARED


v8::Platform* g_platform = NULL;


static Local<Value> Throw(Isolate* isolate, const char* message) {
  return isolate->ThrowException(
      String::NewFromUtf8(isolate, message, NewStringType::kNormal)
          .ToLocalChecked());
}


#ifndef V8_SHARED
bool FindInObjectList(Local<Object> object, const Shell::ObjectList& list) {
  for (int i = 0; i < list.length(); ++i) {
    if (list[i]->StrictEquals(object)) {
      return true;
    }
  }
  return false;
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
#endif  // !V8_SHARED


}  // namespace


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


#ifndef V8_SHARED
CounterMap* Shell::counter_map_;
base::OS::MemoryMappedFile* Shell::counters_file_ = NULL;
CounterCollection Shell::local_counters_;
CounterCollection* Shell::counters_ = &local_counters_;
base::LazyMutex Shell::context_mutex_;
const base::TimeTicks Shell::kInitialTicks =
    base::TimeTicks::HighResolutionNow();
Global<Context> Shell::utility_context_;
base::LazyMutex Shell::workers_mutex_;
bool Shell::allow_new_workers_ = true;
i::List<Worker*> Shell::workers_;
i::List<SharedArrayBuffer::Contents> Shell::externalized_shared_contents_;
#endif  // !V8_SHARED

Global<Context> Shell::evaluation_context_;
ArrayBuffer::Allocator* Shell::array_buffer_allocator;
ShellOptions Shell::options;
base::OnceType Shell::quit_once_ = V8_ONCE_INIT;

#ifndef V8_SHARED
bool CounterMap::Match(void* key1, void* key2) {
  const char* name1 = reinterpret_cast<const char*>(key1);
  const char* name2 = reinterpret_cast<const char*>(key2);
  return strcmp(name1, name2) == 0;
}
#endif  // !V8_SHARED


// Converts a V8 value to a C string.
const char* Shell::ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
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
  Isolate* temp_isolate = Isolate::New(create_params);
  ScriptCompiler::CachedData* result = NULL;
  {
    Isolate::Scope isolate_scope(temp_isolate);
    HandleScope handle_scope(temp_isolate);
    Context::Scope context_scope(Context::New(temp_isolate));
    Local<String> source_copy =
        v8::String::NewFromTwoByte(temp_isolate, source_buffer,
                                   v8::NewStringType::kNormal,
                                   source_length).ToLocalChecked();
    Local<Value> name_copy;
    if (name_buffer) {
      name_copy = v8::String::NewFromTwoByte(temp_isolate, name_buffer,
                                             v8::NewStringType::kNormal,
                                             name_length).ToLocalChecked();
    } else {
      name_copy = v8::Undefined(temp_isolate);
    }
    ScriptCompiler::Source script_source(source_copy, ScriptOrigin(name_copy));
    if (!ScriptCompiler::CompileUnboundScript(temp_isolate, &script_source,
                                              compile_options).IsEmpty() &&
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
    ScriptCompiler::CompileOptions compile_options, SourceType source_type) {
  Local<Context> context(isolate->GetCurrentContext());
  ScriptOrigin origin(name);
  if (compile_options == ScriptCompiler::kNoCompileOptions) {
    ScriptCompiler::Source script_source(source, origin);
    return source_type == SCRIPT
               ? ScriptCompiler::Compile(context, &script_source,
                                         compile_options)
               : ScriptCompiler::CompileModule(context, &script_source,
                                               compile_options);
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
      source_type == SCRIPT
          ? ScriptCompiler::Compile(context, &cached_source, compile_options)
          : ScriptCompiler::CompileModule(context, &cached_source,
                                          compile_options);
  CHECK(data == NULL || !data->rejected);
  return result;
}


// Executes a string within the current v8 context.
bool Shell::ExecuteString(Isolate* isolate, Local<String> source,
                          Local<Value> name, bool print_result,
                          bool report_exceptions, SourceType source_type) {
  HandleScope handle_scope(isolate);
  TryCatch try_catch(isolate);

  MaybeLocal<Value> maybe_result;
  {
    PerIsolateData* data = PerIsolateData::Get(isolate);
    Local<Context> realm =
        Local<Context>::New(isolate, data->realms_[data->realm_current_]);
    Context::Scope context_scope(realm);
    Local<Script> script;
    if (!Shell::CompileString(isolate, source, name, options.compile_options,
                              source_type).ToLocal(&script)) {
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
#if !defined(V8_SHARED)
    if (options.test_shell) {
#endif
      if (!result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        v8::String::Utf8Value str(result);
        fwrite(*str, sizeof(**str), str.length(), stdout);
        printf("\n");
      }
#if !defined(V8_SHARED)
    } else {
      v8::TryCatch try_catch(isolate);
      v8::Local<v8::Context> context =
          v8::Local<v8::Context>::New(isolate, utility_context_);
      v8::Context::Scope context_scope(context);
      Local<Object> global = context->Global();
      Local<Value> fun =
          global->Get(context, String::NewFromUtf8(isolate, "Stringify",
                                                   v8::NewStringType::kNormal)
                                   .ToLocalChecked()).ToLocalChecked();
      Local<Value> argv[1] = {result};
      Local<Value> s;
      if (!Local<Function>::Cast(fun)
               ->Call(context, global, 1, argv)
               .ToLocal(&s)) {
        return true;
      }
      DCHECK(!try_catch.HasCaught());
      v8::String::Utf8Value str(s);
      fwrite(*str, sizeof(**str), str.length(), stdout);
      printf("\n");
    }
#endif
  }
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
  // Drop realms to avoid keeping them alive.
  for (int i = 0; i < data_->realm_count_; ++i)
    data_->realms_[i].Reset();
  delete[] data_->realms_;
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


#ifndef V8_SHARED
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
#endif  // !V8_SHARED


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


// Realm.create() creates a new realm and returns its index.
void Shell::RealmCreate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  TryCatch try_catch(isolate);
  PerIsolateData* data = PerIsolateData::Get(isolate);
  Global<Context>* old_realms = data->realms_;
  int index = data->realm_count_;
  data->realms_ = new Global<Context>[++data->realm_count_];
  for (int i = 0; i < index; ++i) {
    data->realms_[i].Reset(isolate, old_realms[i]);
    old_realms[i].Reset();
  }
  delete[] old_realms;
  Local<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  Local<Context> context = Context::New(isolate, NULL, global_template);
  if (context.IsEmpty()) {
    DCHECK(try_catch.HasCaught());
    try_catch.ReThrow();
    return;
  }
  data->realms_[index].Reset(isolate, context);
  args.GetReturnValue().Set(index);
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
  data->realms_[index].Reset();
  isolate->ContextDisposedNotification();
  isolate->IdleNotificationDeadline(g_platform->MonotonicallyIncreasingTime());
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
  Local<Value> result;
  if (!script->BindToCurrentContext()->Run(realm).ToLocal(&result)) {
    realm->Exit();
    return;
  }
  realm->Exit();
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


void Shell::Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Write(args);
  printf("\n");
  fflush(stdout);
}


void Shell::Write(const v8::FunctionCallbackInfo<v8::Value>& args) {
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(args.GetIsolate());
    if (i != 0) {
      printf(" ");
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
    int n = static_cast<int>(fwrite(*str, sizeof(**str), str.length(), stdout));
    if (n != str.length()) {
      printf("Error in fwrite\n");
      Exit(1);
    }
  }
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


#ifndef V8_SHARED
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

    // Initialize the internal field to NULL; if we return early without
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
  Local<Context> context = isolate->GetCurrentContext();

  if (args.Length() < 1) {
    Throw(isolate, "Invalid argument");
    return;
  }

  Worker* worker = GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker) {
    return;
  }

  Local<Value> message = args[0];
  ObjectList to_transfer;
  if (args.Length() >= 2) {
    if (!args[1]->IsArray()) {
      Throw(isolate, "Transfer list must be an Array");
      return;
    }

    Local<Array> transfer = Local<Array>::Cast(args[1]);
    uint32_t length = transfer->Length();
    for (uint32_t i = 0; i < length; ++i) {
      Local<Value> element;
      if (transfer->Get(context, i).ToLocal(&element)) {
        if (!element->IsArrayBuffer() && !element->IsSharedArrayBuffer()) {
          Throw(isolate,
                "Transfer array elements must be an ArrayBuffer or "
                "SharedArrayBuffer.");
          break;
        }

        to_transfer.Add(Local<Object>::Cast(element));
      }
    }
  }

  ObjectList seen_objects;
  SerializationData* data = new SerializationData;
  if (SerializeValue(isolate, message, to_transfer, &seen_objects, data)) {
    worker->PostMessage(data);
  } else {
    delete data;
  }
}


void Shell::WorkerGetMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  Worker* worker = GetWorkerFromInternalField(isolate, args.Holder());
  if (!worker) {
    return;
  }

  SerializationData* data = worker->GetMessage();
  if (data) {
    int offset = 0;
    Local<Value> data_value;
    if (Shell::DeserializeValue(isolate, *data, &offset).ToLocal(&data_value)) {
      args.GetReturnValue().Set(data_value);
    }
    delete data;
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
#endif  // !V8_SHARED


void Shell::QuitOnce(v8::FunctionCallbackInfo<v8::Value>* args) {
  int exit_code = (*args)[0]
                      ->Int32Value(args->GetIsolate()->GetCurrentContext())
                      .FromMaybe(0);
#ifndef V8_SHARED
  CleanupWorkers();
#endif  // !V8_SHARED
  OnExit(args->GetIsolate());
  Exit(exit_code);
}


void Shell::Quit(const v8::FunctionCallbackInfo<v8::Value>& args) {
  base::CallOnce(&quit_once_, &QuitOnce,
                 const_cast<v8::FunctionCallbackInfo<v8::Value>*>(&args));
}


void Shell::Version(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      String::NewFromUtf8(args.GetIsolate(), V8::GetVersion(),
                          NewStringType::kNormal).ToLocalChecked());
}


void Shell::ReportException(Isolate* isolate, v8::TryCatch* try_catch) {
  HandleScope handle_scope(isolate);
#ifndef V8_SHARED
  Local<Context> utility_context;
  bool enter_context = !isolate->InContext();
  if (enter_context) {
    utility_context = Local<Context>::New(isolate, utility_context_);
    utility_context->Enter();
  }
#endif  // !V8_SHARED
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  Local<Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    printf("%s\n", exception_string);
  } else {
    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
    const char* filename_string = ToCString(filename);
    int linenum =
        message->GetLineNumber(isolate->GetCurrentContext()).FromJust();
    printf("%s:%i: %s\n", filename_string, linenum, exception_string);
    // Print line of source code.
    v8::String::Utf8Value sourceline(
        message->GetSourceLine(isolate->GetCurrentContext()).ToLocalChecked());
    const char* sourceline_string = ToCString(sourceline);
    printf("%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    int start =
        message->GetStartColumn(isolate->GetCurrentContext()).FromJust();
    for (int i = 0; i < start; i++) {
      printf(" ");
    }
    int end = message->GetEndColumn(isolate->GetCurrentContext()).FromJust();
    for (int i = start; i < end; i++) {
      printf("^");
    }
    printf("\n");
    Local<Value> stack_trace_string;
    if (try_catch->StackTrace(isolate->GetCurrentContext())
            .ToLocal(&stack_trace_string) &&
        stack_trace_string->IsString()) {
      v8::String::Utf8Value stack_trace(
          Local<String>::Cast(stack_trace_string));
      printf("%s\n", ToCString(stack_trace));
    }
  }
  printf("\n");
#ifndef V8_SHARED
  if (enter_context) utility_context->Exit();
#endif  // !V8_SHARED
}


#ifndef V8_SHARED
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


class NoUseStrongForUtilityScriptScope {
 public:
  NoUseStrongForUtilityScriptScope() : flag_(i::FLAG_use_strong) {
    i::FLAG_use_strong = false;
  }
  ~NoUseStrongForUtilityScriptScope() { i::FLAG_use_strong = flag_; }

 private:
  bool flag_;
};


void Shell::InstallUtilityScript(Isolate* isolate) {
  NoUseStrongForUtilityScriptScope no_use_strong;
  HandleScope scope(isolate);
  // If we use the utility context, we have to set the security tokens so that
  // utility, evaluation and debug context can all access each other.
  Local<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  utility_context_.Reset(isolate, Context::New(isolate, NULL, global_template));
  v8::Local<v8::Context> utility_context =
      v8::Local<v8::Context>::New(isolate, utility_context_);
  v8::Local<v8::Context> evaluation_context =
      v8::Local<v8::Context>::New(isolate, evaluation_context_);
  utility_context->SetSecurityToken(Undefined(isolate));
  evaluation_context->SetSecurityToken(Undefined(isolate));
  v8::Context::Scope context_scope(utility_context);

  // Run the d8 shell utility script in the utility context
  int source_index = i::NativesCollection<i::D8>::GetIndex("d8");
  i::Vector<const char> shell_source =
      i::NativesCollection<i::D8>::GetScriptSource(source_index);
  i::Vector<const char> shell_source_name =
      i::NativesCollection<i::D8>::GetScriptName(source_index);
  Local<String> source =
      String::NewFromUtf8(isolate, shell_source.start(), NewStringType::kNormal,
                          shell_source.length()).ToLocalChecked();
  Local<String> name =
      String::NewFromUtf8(isolate, shell_source_name.start(),
                          NewStringType::kNormal,
                          shell_source_name.length()).ToLocalChecked();
  ScriptOrigin origin(name);
  Local<Script> script =
      Script::Compile(utility_context, source, &origin).ToLocalChecked();
  script->Run(utility_context).ToLocalChecked();
  // Mark the d8 shell script as native to avoid it showing up as normal source
  // in the debugger.
  i::Handle<i::Object> compiled_script = Utils::OpenHandle(*script);
  i::Handle<i::Script> script_object = compiled_script->IsJSFunction()
      ? i::Handle<i::Script>(i::Script::cast(
          i::JSFunction::cast(*compiled_script)->shared()->script()))
      : i::Handle<i::Script>(i::Script::cast(
          i::SharedFunctionInfo::cast(*compiled_script)->script()));
  script_object->set_type(i::Script::TYPE_EXTENSION);
}
#endif  // !V8_SHARED


Local<ObjectTemplate> Shell::CreateGlobalTemplate(Isolate* isolate) {
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate);
  global_template->Set(
      String::NewFromUtf8(isolate, "print", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, Print));
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
  global_template->Set(
      String::NewFromUtf8(isolate, "version", NewStringType::kNormal)
          .ToLocalChecked(),
      FunctionTemplate::New(isolate, Version));

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

#ifndef V8_SHARED
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
#endif  // !V8_SHARED

  Local<ObjectTemplate> os_templ = ObjectTemplate::New(isolate);
  AddOSMethods(isolate, os_templ);
  global_template->Set(
      String::NewFromUtf8(isolate, "os", NewStringType::kNormal)
          .ToLocalChecked(),
      os_templ);

  return global_template;
}


void Shell::Initialize(Isolate* isolate) {
#ifndef V8_SHARED
  // Set up counters
  if (i::StrLength(i::FLAG_map_counters) != 0)
    MapCounters(isolate, i::FLAG_map_counters);
#endif  // !V8_SHARED
}


Local<Context> Shell::CreateEvaluationContext(Isolate* isolate) {
#ifndef V8_SHARED
  // This needs to be a critical section since this is not thread-safe
  base::LockGuard<base::Mutex> lock_guard(context_mutex_.Pointer());
#endif  // !V8_SHARED
  // Initialize the global objects
  Local<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  EscapableHandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate, NULL, global_template);
  DCHECK(!context.IsEmpty());
  Context::Scope scope(context);

#ifndef V8_SHARED
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
#endif  // !V8_SHARED
  return handle_scope.Escape(context);
}


void Shell::Exit(int exit_code) {
  // Use _exit instead of exit to avoid races between isolate
  // threads and static destructors.
  fflush(stdout);
  fflush(stderr);
  _exit(exit_code);
}


#ifndef V8_SHARED
struct CounterAndKey {
  Counter* counter;
  const char* key;
};


inline bool operator<(const CounterAndKey& lhs, const CounterAndKey& rhs) {
  return strcmp(lhs.key, rhs.key) < 0;
}
#endif  // !V8_SHARED


void Shell::OnExit(v8::Isolate* isolate) {
#ifndef V8_SHARED
  reinterpret_cast<i::Isolate*>(isolate)->DumpAndResetCompilationStats();
  if (i::FLAG_dump_counters) {
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
    printf("+----------------------------------------------------------------+"
           "-------------+\n");
    printf("| Name                                                           |"
           " Value       |\n");
    printf("+----------------------------------------------------------------+"
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
    printf("+----------------------------------------------------------------+"
           "-------------+\n");
    delete [] counters;
  }
  delete counters_file_;
  delete counter_map_;
#endif  // !V8_SHARED
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


static char* ReadChars(Isolate* isolate, const char* name, int* size_out) {
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
  if (*filename == NULL) {
    Throw(args.GetIsolate(), "Error loading file");
    return;
  }

  Isolate* isolate = args.GetIsolate();
  DataAndPersistent* data = new DataAndPersistent;
  data->data = reinterpret_cast<uint8_t*>(
      ReadChars(args.GetIsolate(), *filename, &length));
  if (data->data == NULL) {
    delete data;
    Throw(args.GetIsolate(), "Error reading file");
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
  char* chars = ReadChars(isolate, name, &size);
  if (chars == NULL) return Local<String>();
  Local<String> result =
      String::NewFromUtf8(isolate, chars, NewStringType::kNormal, size)
          .ToLocalChecked();
  delete[] chars;
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
#if defined(__native_client__)
    // Native Client libc is used to being embedded in Chrome and
    // has trouble recognizing when to flush.
    fflush(stdout);
#endif
    Local<String> input = Shell::ReadFromStdin(isolate);
    if (input.IsEmpty()) break;
    ExecuteString(isolate, input, name, true, true);
  }
  printf("\n");
}


SourceGroup::~SourceGroup() {
#ifndef V8_SHARED
  delete thread_;
  thread_ = NULL;
#endif  // !V8_SHARED
}


void SourceGroup::Execute(Isolate* isolate) {
  bool exception_was_thrown = false;
  for (int i = begin_offset_; i < end_offset_; ++i) {
    const char* arg = argv_[i];
    Shell::SourceType source_type = Shell::SCRIPT;
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
      source_type = Shell::MODULE;
      arg = argv_[++i];
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
    if (!Shell::ExecuteString(isolate, source, file_name, false, true,
                              source_type)) {
      exception_was_thrown = true;
      break;
    }
  }
  if (exception_was_thrown != Shell::options.expected_to_throw) {
    Shell::Exit(1);
  }
}


Local<String> SourceGroup::ReadFile(Isolate* isolate, const char* name) {
  int size;
  char* chars = ReadChars(isolate, name, &size);
  if (chars == NULL) return Local<String>();
  Local<String> result =
      String::NewFromUtf8(isolate, chars, NewStringType::kNormal, size)
          .ToLocalChecked();
  delete[] chars;
  return result;
}


#ifndef V8_SHARED
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
  Isolate* isolate = Isolate::New(create_params);
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
          PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
          Execute(isolate);
        }
      }
      Shell::CollectGarbage(isolate);
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


SerializationData::~SerializationData() {
  // Any ArrayBuffer::Contents are owned by this SerializationData object if
  // ownership hasn't been transferred out via ReadArrayBufferContents.
  // SharedArrayBuffer::Contents may be used by multiple threads, so must be
  // cleaned up by the main thread in Shell::CleanupWorkers().
  for (int i = 0; i < array_buffer_contents_.length(); ++i) {
    ArrayBuffer::Contents& contents = array_buffer_contents_[i];
    if (contents.Data()) {
      Shell::array_buffer_allocator->Free(contents.Data(),
                                          contents.ByteLength());
    }
  }
}


void SerializationData::WriteTag(SerializationTag tag) { data_.Add(tag); }


void SerializationData::WriteMemory(const void* p, int length) {
  if (length > 0) {
    i::Vector<uint8_t> block = data_.AddBlock(0, length);
    memcpy(&block[0], p, length);
  }
}


void SerializationData::WriteArrayBufferContents(
    const ArrayBuffer::Contents& contents) {
  array_buffer_contents_.Add(contents);
  WriteTag(kSerializationTagTransferredArrayBuffer);
  int index = array_buffer_contents_.length() - 1;
  Write(index);
}


void SerializationData::WriteSharedArrayBufferContents(
    const SharedArrayBuffer::Contents& contents) {
  shared_array_buffer_contents_.Add(contents);
  WriteTag(kSerializationTagTransferredSharedArrayBuffer);
  int index = shared_array_buffer_contents_.length() - 1;
  Write(index);
}


SerializationTag SerializationData::ReadTag(int* offset) const {
  return static_cast<SerializationTag>(Read<uint8_t>(offset));
}


void SerializationData::ReadMemory(void* p, int length, int* offset) const {
  if (length > 0) {
    memcpy(p, &data_[*offset], length);
    (*offset) += length;
  }
}


void SerializationData::ReadArrayBufferContents(ArrayBuffer::Contents* contents,
                                                int* offset) const {
  int index = Read<int>(offset);
  DCHECK(index < array_buffer_contents_.length());
  *contents = array_buffer_contents_[index];
  // Ownership of this ArrayBuffer::Contents is passed to the caller. Neuter
  // our copy so it won't be double-free'd when this SerializationData is
  // destroyed.
  array_buffer_contents_[index] = ArrayBuffer::Contents();
}


void SerializationData::ReadSharedArrayBufferContents(
    SharedArrayBuffer::Contents* contents, int* offset) const {
  int index = Read<int>(offset);
  DCHECK(index < shared_array_buffer_contents_.length());
  *contents = shared_array_buffer_contents_[index];
}


void SerializationDataQueue::Enqueue(SerializationData* data) {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  data_.Add(data);
}


bool SerializationDataQueue::Dequeue(SerializationData** data) {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  *data = NULL;
  if (data_.is_empty()) return false;
  *data = data_.Remove(0);
  return true;
}


bool SerializationDataQueue::IsEmpty() {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  return data_.is_empty();
}


void SerializationDataQueue::Clear() {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  for (int i = 0; i < data_.length(); ++i) {
    delete data_[i];
  }
  data_.Clear();
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


void Worker::PostMessage(SerializationData* data) {
  in_queue_.Enqueue(data);
  in_semaphore_.Signal();
}


SerializationData* Worker::GetMessage() {
  SerializationData* data = NULL;
  while (!out_queue_.Dequeue(&data)) {
    // If the worker is no longer running, and there are no messages in the
    // queue, don't expect any more messages from it.
    if (!base::NoBarrier_Load(&running_)) break;
    out_semaphore_.Wait();
  }
  return data;
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
  Isolate* isolate = Isolate::New(create_params);
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
              SerializationData* data;
              if (!in_queue_.Dequeue(&data)) continue;
              if (data == NULL) {
                break;
              }
              int offset = 0;
              Local<Value> data_value;
              if (Shell::DeserializeValue(isolate, *data, &offset)
                      .ToLocal(&data_value)) {
                Local<Value> argv[] = {data_value};
                (void)onmessage_fun->Call(context, global, 1, argv);
              }
              delete data;
            }
          }
        }
      }
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

  // TODO(binji): Allow transferring from worker to main thread?
  Shell::ObjectList to_transfer;

  Shell::ObjectList seen_objects;
  SerializationData* data = new SerializationData;
  if (Shell::SerializeValue(isolate, message, to_transfer, &seen_objects,
                            data)) {
    DCHECK(args.Data()->IsExternal());
    Local<External> this_value = Local<External>::Cast(args.Data());
    Worker* worker = static_cast<Worker*>(this_value->Value());
    worker->out_queue_.Enqueue(data);
    worker->out_semaphore_.Signal();
  } else {
    delete data;
  }
}
#endif  // !V8_SHARED


void SetFlagsFromString(const char* flags) {
  v8::V8::SetFlagsFromString(flags, static_cast<int>(strlen(flags)));
}


bool Shell::SetOptions(int argc, char* argv[]) {
  bool logfile_per_isolate = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--stress-opt") == 0) {
      options.stress_opt = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--nostress-opt") == 0) {
      options.stress_opt = false;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--stress-deopt") == 0) {
      options.stress_deopt = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--mock-arraybuffer-allocator") == 0) {
      options.mock_arraybuffer_allocator = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--noalways-opt") == 0) {
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
#ifdef V8_SHARED
      printf("D8 with shared library does not support multi-threading\n");
      return false;
#endif  // V8_SHARED
      options.num_isolates++;
    } else if (strcmp(argv[i], "--dump-heap-constants") == 0) {
#ifdef V8_SHARED
      printf("D8 with shared library does not support constant dumping\n");
      return false;
#else
      options.dump_heap_constants = true;
      argv[i] = NULL;
#endif  // V8_SHARED
    } else if (strcmp(argv[i], "--throws") == 0) {
      options.expected_to_throw = true;
      argv[i] = NULL;
    } else if (strncmp(argv[i], "--icu-data-file=", 16) == 0) {
      options.icu_data_file = argv[i] + 16;
      argv[i] = NULL;
#ifdef V8_SHARED
    } else if (strcmp(argv[i], "--dump-counters") == 0) {
      printf("D8 with shared library does not include counters\n");
      return false;
#endif  // V8_SHARED
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
    }
  }

  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);

  bool enable_harmony_modules = false;

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
      enable_harmony_modules = true;
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

  if (enable_harmony_modules) {
    SetFlagsFromString("--harmony-modules");
  }

  return true;
}


int Shell::RunMain(Isolate* isolate, int argc, char* argv[], bool last_run) {
#ifndef V8_SHARED
  for (int i = 1; i < options.num_isolates; ++i) {
    options.isolate_sources[i].StartExecuteInThread();
  }
#endif  // !V8_SHARED
  {
    HandleScope scope(isolate);
    Local<Context> context = CreateEvaluationContext(isolate);
    if (last_run && options.use_interactive_shell()) {
      // Keep using the same context in the interactive shell.
      evaluation_context_.Reset(isolate, context);
    }
    {
      Context::Scope cscope(context);
      PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
      options.isolate_sources[0].Execute(isolate);
    }
  }
  CollectGarbage(isolate);
#ifndef V8_SHARED
  for (int i = 1; i < options.num_isolates; ++i) {
    if (last_run) {
      options.isolate_sources[i].JoinThread();
    } else {
      options.isolate_sources[i].WaitForThread();
    }
  }
  CleanupWorkers();
#endif  // !V8_SHARED
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


void Shell::EmptyMessageQueues(Isolate* isolate) {
#ifndef V8_SHARED
  if (!i::FLAG_verify_predictable) {
#endif
    while (v8::platform::PumpMessageLoop(g_platform, isolate)) continue;
#ifndef V8_SHARED
  }
#endif
}


#ifndef V8_SHARED
bool Shell::SerializeValue(Isolate* isolate, Local<Value> value,
                           const ObjectList& to_transfer,
                           ObjectList* seen_objects,
                           SerializationData* out_data) {
  DCHECK(out_data);
  Local<Context> context = isolate->GetCurrentContext();

  if (value->IsUndefined()) {
    out_data->WriteTag(kSerializationTagUndefined);
  } else if (value->IsNull()) {
    out_data->WriteTag(kSerializationTagNull);
  } else if (value->IsTrue()) {
    out_data->WriteTag(kSerializationTagTrue);
  } else if (value->IsFalse()) {
    out_data->WriteTag(kSerializationTagFalse);
  } else if (value->IsNumber()) {
    Local<Number> num = Local<Number>::Cast(value);
    double value = num->Value();
    out_data->WriteTag(kSerializationTagNumber);
    out_data->Write(value);
  } else if (value->IsString()) {
    v8::String::Utf8Value str(value);
    out_data->WriteTag(kSerializationTagString);
    out_data->Write(str.length());
    out_data->WriteMemory(*str, str.length());
  } else if (value->IsArray()) {
    Local<Array> array = Local<Array>::Cast(value);
    if (FindInObjectList(array, *seen_objects)) {
      Throw(isolate, "Duplicated arrays not supported");
      return false;
    }
    seen_objects->Add(array);
    out_data->WriteTag(kSerializationTagArray);
    uint32_t length = array->Length();
    out_data->Write(length);
    for (uint32_t i = 0; i < length; ++i) {
      Local<Value> element_value;
      if (array->Get(context, i).ToLocal(&element_value)) {
        if (!SerializeValue(isolate, element_value, to_transfer, seen_objects,
                            out_data))
          return false;
      } else {
        Throw(isolate, "Failed to serialize array element.");
        return false;
      }
    }
  } else if (value->IsArrayBuffer()) {
    Local<ArrayBuffer> array_buffer = Local<ArrayBuffer>::Cast(value);
    if (FindInObjectList(array_buffer, *seen_objects)) {
      Throw(isolate, "Duplicated array buffers not supported");
      return false;
    }
    seen_objects->Add(array_buffer);
    if (FindInObjectList(array_buffer, to_transfer)) {
      // Transfer ArrayBuffer
      if (!array_buffer->IsNeuterable()) {
        Throw(isolate, "Attempting to transfer an un-neuterable ArrayBuffer");
        return false;
      }

      ArrayBuffer::Contents contents = array_buffer->IsExternal()
                                           ? array_buffer->GetContents()
                                           : array_buffer->Externalize();
      array_buffer->Neuter();
      out_data->WriteArrayBufferContents(contents);
    } else {
      ArrayBuffer::Contents contents = array_buffer->GetContents();
      // Clone ArrayBuffer
      if (contents.ByteLength() > i::kMaxInt) {
        Throw(isolate, "ArrayBuffer is too big to clone");
        return false;
      }

      int32_t byte_length = static_cast<int32_t>(contents.ByteLength());
      out_data->WriteTag(kSerializationTagArrayBuffer);
      out_data->Write(byte_length);
      out_data->WriteMemory(contents.Data(), byte_length);
    }
  } else if (value->IsSharedArrayBuffer()) {
    Local<SharedArrayBuffer> sab = Local<SharedArrayBuffer>::Cast(value);
    if (FindInObjectList(sab, *seen_objects)) {
      Throw(isolate, "Duplicated shared array buffers not supported");
      return false;
    }
    seen_objects->Add(sab);
    if (!FindInObjectList(sab, to_transfer)) {
      Throw(isolate, "SharedArrayBuffer must be transferred");
      return false;
    }

    SharedArrayBuffer::Contents contents;
    if (sab->IsExternal()) {
      contents = sab->GetContents();
    } else {
      contents = sab->Externalize();
      base::LockGuard<base::Mutex> lock_guard(workers_mutex_.Pointer());
      externalized_shared_contents_.Add(contents);
    }
    out_data->WriteSharedArrayBufferContents(contents);
  } else if (value->IsObject()) {
    Local<Object> object = Local<Object>::Cast(value);
    if (FindInObjectList(object, *seen_objects)) {
      Throw(isolate, "Duplicated objects not supported");
      return false;
    }
    seen_objects->Add(object);
    Local<Array> property_names;
    if (!object->GetOwnPropertyNames(context).ToLocal(&property_names)) {
      Throw(isolate, "Unable to get property names");
      return false;
    }

    uint32_t length = property_names->Length();
    out_data->WriteTag(kSerializationTagObject);
    out_data->Write(length);
    for (uint32_t i = 0; i < length; ++i) {
      Local<Value> name;
      Local<Value> property_value;
      if (property_names->Get(context, i).ToLocal(&name) &&
          object->Get(context, name).ToLocal(&property_value)) {
        if (!SerializeValue(isolate, name, to_transfer, seen_objects, out_data))
          return false;
        if (!SerializeValue(isolate, property_value, to_transfer, seen_objects,
                            out_data))
          return false;
      } else {
        Throw(isolate, "Failed to serialize property.");
        return false;
      }
    }
  } else {
    Throw(isolate, "Don't know how to serialize object");
    return false;
  }

  return true;
}


MaybeLocal<Value> Shell::DeserializeValue(Isolate* isolate,
                                          const SerializationData& data,
                                          int* offset) {
  DCHECK(offset);
  EscapableHandleScope scope(isolate);
  // This function should not use utility_context_ because it is running on a
  // different thread.
  Local<Value> result;
  SerializationTag tag = data.ReadTag(offset);

  switch (tag) {
    case kSerializationTagUndefined:
      result = Undefined(isolate);
      break;
    case kSerializationTagNull:
      result = Null(isolate);
      break;
    case kSerializationTagTrue:
      result = True(isolate);
      break;
    case kSerializationTagFalse:
      result = False(isolate);
      break;
    case kSerializationTagNumber:
      result = Number::New(isolate, data.Read<double>(offset));
      break;
    case kSerializationTagString: {
      int length = data.Read<int>(offset);
      CHECK(length >= 0);
      std::vector<char> buffer(length + 1);  // + 1 so it is never empty.
      data.ReadMemory(&buffer[0], length, offset);
      MaybeLocal<String> str =
          String::NewFromUtf8(isolate, &buffer[0], NewStringType::kNormal,
                              length).ToLocalChecked();
      if (!str.IsEmpty()) result = str.ToLocalChecked();
      break;
    }
    case kSerializationTagArray: {
      uint32_t length = data.Read<uint32_t>(offset);
      Local<Array> array = Array::New(isolate, length);
      for (uint32_t i = 0; i < length; ++i) {
        Local<Value> element_value;
        CHECK(DeserializeValue(isolate, data, offset).ToLocal(&element_value));
        array->Set(isolate->GetCurrentContext(), i, element_value).FromJust();
      }
      result = array;
      break;
    }
    case kSerializationTagObject: {
      int length = data.Read<int>(offset);
      Local<Object> object = Object::New(isolate);
      for (int i = 0; i < length; ++i) {
        Local<Value> property_name;
        CHECK(DeserializeValue(isolate, data, offset).ToLocal(&property_name));
        Local<Value> property_value;
        CHECK(DeserializeValue(isolate, data, offset).ToLocal(&property_value));
        object->Set(isolate->GetCurrentContext(), property_name, property_value)
            .FromJust();
      }
      result = object;
      break;
    }
    case kSerializationTagArrayBuffer: {
      int32_t byte_length = data.Read<int32_t>(offset);
      Local<ArrayBuffer> array_buffer = ArrayBuffer::New(isolate, byte_length);
      ArrayBuffer::Contents contents = array_buffer->GetContents();
      DCHECK(static_cast<size_t>(byte_length) == contents.ByteLength());
      data.ReadMemory(contents.Data(), byte_length, offset);
      result = array_buffer;
      break;
    }
    case kSerializationTagTransferredArrayBuffer: {
      ArrayBuffer::Contents contents;
      data.ReadArrayBufferContents(&contents, offset);
      result = ArrayBuffer::New(isolate, contents.Data(), contents.ByteLength(),
                                ArrayBufferCreationMode::kInternalized);
      break;
    }
    case kSerializationTagTransferredSharedArrayBuffer: {
      SharedArrayBuffer::Contents contents;
      data.ReadSharedArrayBufferContents(&contents, offset);
      result = SharedArrayBuffer::New(isolate, contents.Data(),
                                      contents.ByteLength());
      break;
    }
    default:
      UNREACHABLE();
  }

  return scope.Escape(result);
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

  for (int i = 0; i < externalized_shared_contents_.length(); ++i) {
    const SharedArrayBuffer::Contents& contents =
        externalized_shared_contents_[i];
    Shell::array_buffer_allocator->Free(contents.Data(), contents.ByteLength());
  }
  externalized_shared_contents_.Clear();
}


static void DumpHeapConstants(i::Isolate* isolate) {
  i::Heap* heap = isolate->heap();

  // Dump the INSTANCE_TYPES table to the console.
  printf("# List of known V8 instance types.\n");
#define DUMP_TYPE(T) printf("  %d: \"%s\",\n", i::T, #T);
  printf("INSTANCE_TYPES = {\n");
  INSTANCE_TYPE_LIST(DUMP_TYPE)
  printf("}\n");
#undef DUMP_TYPE

  // Dump the KNOWN_MAP table to the console.
  printf("\n# List of known V8 maps.\n");
#define ROOT_LIST_CASE(type, name, camel_name) \
  if (n == NULL && o == heap->name()) n = #camel_name;
#define STRUCT_LIST_CASE(upper_name, camel_name, name) \
  if (n == NULL && o == heap->name##_map()) n = #camel_name "Map";
  i::HeapObjectIterator it(heap->map_space());
  printf("KNOWN_MAPS = {\n");
  for (i::Object* o = it.Next(); o != NULL; o = it.Next()) {
    i::Map* m = i::Map::cast(o);
    const char* n = NULL;
    intptr_t p = reinterpret_cast<intptr_t>(m) & 0xfffff;
    int t = m->instance_type();
    ROOT_LIST(ROOT_LIST_CASE)
    STRUCT_LIST(STRUCT_LIST_CASE)
    if (n == NULL) continue;
    printf("  0x%05" V8PRIxPTR ": (%d, \"%s\"),\n", p, t, n);
  }
  printf("}\n");
#undef STRUCT_LIST_CASE
#undef ROOT_LIST_CASE

  // Dump the KNOWN_OBJECTS table to the console.
  printf("\n# List of known V8 objects.\n");
#define ROOT_LIST_CASE(type, name, camel_name) \
  if (n == NULL && o == heap->name()) n = #camel_name;
  i::OldSpaces spit(heap);
  printf("KNOWN_OBJECTS = {\n");
  for (i::PagedSpace* s = spit.next(); s != NULL; s = spit.next()) {
    i::HeapObjectIterator it(s);
    const char* sname = AllocationSpaceName(s->identity());
    for (i::Object* o = it.Next(); o != NULL; o = it.Next()) {
      const char* n = NULL;
      intptr_t p = reinterpret_cast<intptr_t>(o) & 0xfffff;
      ROOT_LIST(ROOT_LIST_CASE)
      if (n == NULL) continue;
      printf("  (\"%s\", 0x%05" V8PRIxPTR "): \"%s\",\n", sname, p, n);
    }
  }
  printf("}\n");
#undef ROOT_LIST_CASE
}
#endif  // !V8_SHARED


int Shell::Main(int argc, char* argv[]) {
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
  v8::V8::InitializeICU(options.icu_data_file);
#ifndef V8_SHARED
  g_platform = i::FLAG_verify_predictable
                   ? new PredictablePlatform()
                   : v8::platform::CreateDefaultPlatform();
#else
  g_platform = v8::platform::CreateDefaultPlatform();
#endif  // !V8_SHARED

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
#if !defined(V8_SHARED) && defined(ENABLE_GDB_JIT_INTERFACE)
  if (i::FLAG_gdbjit) {
    create_params.code_event_handler = i::GDBJITInterface::EventHandler;
  }
#endif
#ifdef ENABLE_VTUNE_JIT_INTERFACE
  create_params.code_event_handler = vTune::GetVtuneCodeEventHandler();
#endif
#ifndef V8_SHARED
  create_params.constraints.ConfigureDefaults(
      base::SysInfo::AmountOfPhysicalMemory(),
      base::SysInfo::AmountOfVirtualMemory());

  Shell::counter_map_ = new CounterMap();
  if (i::FLAG_dump_counters || i::FLAG_track_gc_object_stats) {
    create_params.counter_lookup_callback = LookupCounter;
    create_params.create_histogram_callback = CreateHistogram;
    create_params.add_histogram_sample_callback = AddHistogramSample;
  }
#endif
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope scope(isolate);
    Initialize(isolate);
    PerIsolateData data(isolate);

#ifndef V8_SHARED
    if (options.dump_heap_constants) {
      DumpHeapConstants(reinterpret_cast<i::Isolate*>(isolate));
      return 0;
    }
#endif

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
#if !defined(V8_SHARED)
    } else if (i::FLAG_stress_runs > 0) {
      options.stress_runs = i::FLAG_stress_runs;
      for (int i = 0; i < options.stress_runs && result == 0; i++) {
        printf("============ Run %d/%d ============\n", i + 1,
               options.stress_runs);
        bool last_run = i == options.stress_runs - 1;
        result = RunMain(isolate, argc, argv, last_run);
      }
#endif
    } else {
      bool last_run = true;
      result = RunMain(isolate, argc, argv, last_run);
    }

    // Run interactive shell if explicitly requested or if no script has been
    // executed, but never on --test
    if (options.use_interactive_shell()) {
#ifndef V8_SHARED
      InstallUtilityScript(isolate);
#endif  // !V8_SHARED
      RunShell(isolate);
    }

    // Shut down contexts and collect garbage.
    evaluation_context_.Reset();
#ifndef V8_SHARED
    utility_context_.Reset();
#endif  // !V8_SHARED
    CollectGarbage(isolate);
  }
  OnExit(isolate);
#ifndef V8_SHARED
  // Dump basic block profiling data.
  if (i::BasicBlockProfiler* profiler =
          reinterpret_cast<i::Isolate*>(isolate)->basic_block_profiler()) {
    i::OFStream os(stdout);
    os << *profiler;
  }
#endif  // !V8_SHARED
  isolate->Dispose();
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
