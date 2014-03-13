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


// Defined when linking against shared lib on Windows.
#if defined(USING_V8_SHARED) && !defined(V8_SHARED)
#define V8_SHARED
#endif

#ifdef COMPRESS_STARTUP_DATA_BZ2
#include <bzlib.h>
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
#endif  // !V8_SHARED

#ifdef V8_SHARED
#include "../include/v8-testing.h"
#endif  // V8_SHARED

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "third_party/vtune/v8-vtune.h"
#endif

#include "d8.h"

#ifndef V8_SHARED
#include "api.h"
#include "checks.h"
#include "cpu.h"
#include "d8-debug.h"
#include "debug.h"
#include "natives.h"
#include "platform.h"
#include "v8.h"
#endif  // V8_SHARED

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#endif

#ifndef ASSERT
#define ASSERT(condition) assert(condition)
#endif

namespace v8 {


static Handle<Value> Throw(Isolate* isolate, const char* message) {
  return isolate->ThrowException(String::NewFromUtf8(isolate, message));
}



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
  Persistent<Context>* realms_;
  Persistent<Value> realm_shared_;

  int RealmFind(Handle<Context> context);
};


LineEditor *LineEditor::current_ = NULL;


LineEditor::LineEditor(Type type, const char* name)
    : type_(type), name_(name) {
  if (current_ == NULL || current_->type_ < type) current_ = this;
}


class DumbLineEditor: public LineEditor {
 public:
  explicit DumbLineEditor(Isolate* isolate)
      : LineEditor(LineEditor::DUMB, "dumb"), isolate_(isolate) { }
  virtual Handle<String> Prompt(const char* prompt);
 private:
  Isolate* isolate_;
};


Handle<String> DumbLineEditor::Prompt(const char* prompt) {
  printf("%s", prompt);
#if defined(__native_client__)
  // Native Client libc is used to being embedded in Chrome and
  // has trouble recognizing when to flush.
  fflush(stdout);
#endif
  return Shell::ReadFromStdin(isolate_);
}


#ifndef V8_SHARED
CounterMap* Shell::counter_map_;
i::OS::MemoryMappedFile* Shell::counters_file_ = NULL;
CounterCollection Shell::local_counters_;
CounterCollection* Shell::counters_ = &local_counters_;
i::Mutex Shell::context_mutex_;
const i::TimeTicks Shell::kInitialTicks = i::TimeTicks::HighResolutionNow();
Persistent<Context> Shell::utility_context_;
#endif  // V8_SHARED

Persistent<Context> Shell::evaluation_context_;
ShellOptions Shell::options;
const char* Shell::kPrompt = "d8> ";


#ifndef V8_SHARED
const int MB = 1024 * 1024;

bool CounterMap::Match(void* key1, void* key2) {
  const char* name1 = reinterpret_cast<const char*>(key1);
  const char* name2 = reinterpret_cast<const char*>(key2);
  return strcmp(name1, name2) == 0;
}
#endif  // V8_SHARED


// Converts a V8 value to a C string.
const char* Shell::ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}


// Executes a string within the current v8 context.
bool Shell::ExecuteString(Isolate* isolate,
                          Handle<String> source,
                          Handle<Value> name,
                          bool print_result,
                          bool report_exceptions) {
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
  bool FLAG_debugger = i::FLAG_debugger;
#else
  bool FLAG_debugger = false;
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
  HandleScope handle_scope(isolate);
  TryCatch try_catch;
  options.script_executed = true;
  if (FLAG_debugger) {
    // When debugging make exceptions appear to be uncaught.
    try_catch.SetVerbose(true);
  }
  Handle<Script> script = Script::New(source, name);
  if (script.IsEmpty()) {
    // Print errors that happened during compilation.
    if (report_exceptions && !FLAG_debugger)
      ReportException(isolate, &try_catch);
    return false;
  } else {
    PerIsolateData* data = PerIsolateData::Get(isolate);
    Local<Context> realm =
        Local<Context>::New(isolate, data->realms_[data->realm_current_]);
    realm->Enter();
    Handle<Value> result = script->Run();
    realm->Exit();
    data->realm_current_ = data->realm_switch_;
    if (result.IsEmpty()) {
      ASSERT(try_catch.HasCaught());
      // Print errors that happened during execution.
      if (report_exceptions && !FLAG_debugger)
        ReportException(isolate, &try_catch);
      return false;
    } else {
      ASSERT(!try_catch.HasCaught());
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
          v8::TryCatch try_catch;
          v8::Local<v8::Context> context =
              v8::Local<v8::Context>::New(isolate, utility_context_);
          v8::Context::Scope context_scope(context);
          Handle<Object> global = context->Global();
          Handle<Value> fun =
              global->Get(String::NewFromUtf8(isolate, "Stringify"));
          Handle<Value> argv[1] = { result };
          Handle<Value> s = Handle<Function>::Cast(fun)->Call(global, 1, argv);
          if (try_catch.HasCaught()) return true;
          v8::String::Utf8Value str(s);
          fwrite(*str, sizeof(**str), str.length(), stdout);
          printf("\n");
        }
#endif
      }
      return true;
    }
  }
}


PerIsolateData::RealmScope::RealmScope(PerIsolateData* data) : data_(data) {
  data_->realm_count_ = 1;
  data_->realm_current_ = 0;
  data_->realm_switch_ = 0;
  data_->realms_ = new Persistent<Context>[1];
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


int PerIsolateData::RealmFind(Handle<Context> context) {
  for (int i = 0; i < realm_count_; ++i) {
    if (realms_[i] == context) return i;
  }
  return -1;
}


#ifndef V8_SHARED
// performance.now() returns a time stamp as double, measured in milliseconds.
void Shell::PerformanceNow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  i::TimeDelta delta = i::TimeTicks::HighResolutionNow() - kInitialTicks;
  args.GetReturnValue().Set(delta.InMillisecondsF());
}
#endif  // V8_SHARED


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
  int index = data->RealmFind(args[0]->ToObject()->CreationContext());
  if (index == -1) return;
  args.GetReturnValue().Set(index);
}


// Realm.global(i) returns the global object of realm i.
// (Note that properties of global objects cannot be read/written cross-realm.)
void Shell::RealmGlobal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  PerIsolateData* data = PerIsolateData::Get(args.GetIsolate());
  if (args.Length() < 1 || !args[0]->IsNumber()) {
    Throw(args.GetIsolate(), "Invalid argument");
    return;
  }
  int index = args[0]->Uint32Value();
  if (index >= data->realm_count_ || data->realms_[index].IsEmpty()) {
    Throw(args.GetIsolate(), "Invalid realm index");
    return;
  }
  args.GetReturnValue().Set(
      Local<Context>::New(args.GetIsolate(), data->realms_[index])->Global());
}


// Realm.create() creates a new realm and returns its index.
void Shell::RealmCreate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  Persistent<Context>* old_realms = data->realms_;
  int index = data->realm_count_;
  data->realms_ = new Persistent<Context>[++data->realm_count_];
  for (int i = 0; i < index; ++i) {
    data->realms_[i].Reset(isolate, old_realms[i]);
  }
  delete[] old_realms;
  Handle<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  data->realms_[index].Reset(
      isolate, Context::New(isolate, NULL, global_template));
  args.GetReturnValue().Set(index);
}


// Realm.dispose(i) disposes the reference to the realm i.
void Shell::RealmDispose(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  if (args.Length() < 1 || !args[0]->IsNumber()) {
    Throw(args.GetIsolate(), "Invalid argument");
    return;
  }
  int index = args[0]->Uint32Value();
  if (index >= data->realm_count_ || data->realms_[index].IsEmpty() ||
      index == 0 ||
      index == data->realm_current_ || index == data->realm_switch_) {
    Throw(args.GetIsolate(), "Invalid realm index");
    return;
  }
  data->realms_[index].Reset();
}


// Realm.switch(i) switches to the realm i for consecutive interactive inputs.
void Shell::RealmSwitch(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  if (args.Length() < 1 || !args[0]->IsNumber()) {
    Throw(args.GetIsolate(), "Invalid argument");
    return;
  }
  int index = args[0]->Uint32Value();
  if (index >= data->realm_count_ || data->realms_[index].IsEmpty()) {
    Throw(args.GetIsolate(), "Invalid realm index");
    return;
  }
  data->realm_switch_ = index;
}


// Realm.eval(i, s) evaluates s in realm i and returns the result.
void Shell::RealmEval(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  PerIsolateData* data = PerIsolateData::Get(isolate);
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsString()) {
    Throw(args.GetIsolate(), "Invalid argument");
    return;
  }
  int index = args[0]->Uint32Value();
  if (index >= data->realm_count_ || data->realms_[index].IsEmpty()) {
    Throw(args.GetIsolate(), "Invalid realm index");
    return;
  }
  Handle<Script> script = Script::New(args[1]->ToString());
  if (script.IsEmpty()) return;
  Local<Context> realm = Local<Context>::New(isolate, data->realms_[index]);
  realm->Enter();
  Handle<Value> result = script->Run();
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
    v8::TryCatch try_catch;
    Handle<String> str_obj = args[i]->ToString();
    if (try_catch.HasCaught()) {
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
  Handle<String> source = ReadFile(args.GetIsolate(), *file);
  if (source.IsEmpty()) {
    Throw(args.GetIsolate(), "Error loading file");
    return;
  }
  args.GetReturnValue().Set(source);
}


Handle<String> Shell::ReadFromStdin(Isolate* isolate) {
  static const int kBufferSize = 256;
  char buffer[kBufferSize];
  Handle<String> accumulator = String::NewFromUtf8(isolate, "");
  int length;
  while (true) {
    // Continue reading if the line ends with an escape '\\' or the line has
    // not been fully read into the buffer yet (does not end with '\n').
    // If fgets gets an error, just give up.
    char* input = NULL;
    {  // Release lock for blocking input.
      Unlocker unlock(isolate);
      input = fgets(buffer, kBufferSize, stdin);
    }
    if (input == NULL) return Handle<String>();
    length = static_cast<int>(strlen(buffer));
    if (length == 0) {
      return accumulator;
    } else if (buffer[length-1] != '\n') {
      accumulator = String::Concat(
          accumulator,
          String::NewFromUtf8(isolate, buffer, String::kNormalString, length));
    } else if (length > 1 && buffer[length-2] == '\\') {
      buffer[length-2] = '\n';
      accumulator = String::Concat(
          accumulator, String::NewFromUtf8(isolate, buffer,
                                           String::kNormalString, length - 1));
    } else {
      return String::Concat(
          accumulator, String::NewFromUtf8(isolate, buffer,
                                           String::kNormalString, length - 1));
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
    Handle<String> source = ReadFile(args.GetIsolate(), *file);
    if (source.IsEmpty()) {
      Throw(args.GetIsolate(), "Error loading file");
      return;
    }
    if (!ExecuteString(args.GetIsolate(),
                       source,
                       String::NewFromUtf8(args.GetIsolate(), *file),
                       false,
                       true)) {
      Throw(args.GetIsolate(), "Error executing file");
      return;
    }
  }
}


void Shell::Quit(const v8::FunctionCallbackInfo<v8::Value>& args) {
  int exit_code = args[0]->Int32Value();
  OnExit();
  exit(exit_code);
}


void Shell::Version(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      String::NewFromUtf8(args.GetIsolate(), V8::GetVersion()));
}


void Shell::ReportException(Isolate* isolate, v8::TryCatch* try_catch) {
  HandleScope handle_scope(isolate);
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
  Handle<Context> utility_context;
  bool enter_context = !isolate->InContext();
  if (enter_context) {
    utility_context = Local<Context>::New(isolate, utility_context_);
    utility_context->Enter();
  }
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  Handle<Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    printf("%s\n", exception_string);
  } else {
    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    printf("%s:%i: %s\n", filename_string, linenum, exception_string);
    // Print line of source code.
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    printf("%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      printf(" ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      printf("^");
    }
    printf("\n");
    v8::String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = ToCString(stack_trace);
      printf("%s\n", stack_trace_string);
    }
  }
  printf("\n");
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
  if (enter_context) utility_context->Exit();
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
}


#ifndef V8_SHARED
Handle<Array> Shell::GetCompletions(Isolate* isolate,
                                    Handle<String> text,
                                    Handle<String> full) {
  EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> utility_context =
      v8::Local<v8::Context>::New(isolate, utility_context_);
  v8::Context::Scope context_scope(utility_context);
  Handle<Object> global = utility_context->Global();
  Local<Value> fun =
      global->Get(String::NewFromUtf8(isolate, "GetCompletions"));
  static const int kArgc = 3;
  v8::Local<v8::Context> evaluation_context =
      v8::Local<v8::Context>::New(isolate, evaluation_context_);
  Handle<Value> argv[kArgc] = { evaluation_context->Global(), text, full };
  Local<Value> val = Local<Function>::Cast(fun)->Call(global, kArgc, argv);
  return handle_scope.Escape(Local<Array>::Cast(val));
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Local<Object> Shell::DebugMessageDetails(Isolate* isolate,
                                         Handle<String> message) {
  EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, utility_context_);
  v8::Context::Scope context_scope(context);
  Handle<Object> global = context->Global();
  Handle<Value> fun =
      global->Get(String::NewFromUtf8(isolate, "DebugMessageDetails"));
  static const int kArgc = 1;
  Handle<Value> argv[kArgc] = { message };
  Handle<Value> val = Handle<Function>::Cast(fun)->Call(global, kArgc, argv);
  return handle_scope.Escape(Local<Object>(Handle<Object>::Cast(val)));
}


Local<Value> Shell::DebugCommandToJSONRequest(Isolate* isolate,
                                              Handle<String> command) {
  EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, utility_context_);
  v8::Context::Scope context_scope(context);
  Handle<Object> global = context->Global();
  Handle<Value> fun =
      global->Get(String::NewFromUtf8(isolate, "DebugCommandToJSONRequest"));
  static const int kArgc = 1;
  Handle<Value> argv[kArgc] = { command };
  Handle<Value> val = Handle<Function>::Cast(fun)->Call(global, kArgc, argv);
  return handle_scope.Escape(Local<Value>(val));
}


void Shell::DispatchDebugMessages() {
  Isolate* isolate = v8::Isolate::GetCurrent();
  HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, Shell::evaluation_context_);
  v8::Context::Scope context_scope(context);
  v8::Debug::ProcessDebugMessages();
}
#endif  // ENABLE_DEBUGGER_SUPPORT
#endif  // V8_SHARED


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


void Shell::MapCounters(const char* name) {
  counters_file_ = i::OS::MemoryMappedFile::create(
      name, sizeof(CounterCollection), &local_counters_);
  void* memory = (counters_file_ == NULL) ?
      NULL : counters_file_->memory();
  if (memory == NULL) {
    printf("Could not map counters file %s\n", name);
    Exit(1);
  }
  counters_ = static_cast<CounterCollection*>(memory);
  V8::SetCounterFunction(LookupCounter);
  V8::SetCreateHistogramFunction(CreateHistogram);
  V8::SetAddHistogramSampleFunction(AddHistogramSample);
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
    ASSERT(counter->is_histogram() == is_histogram);
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


void Shell::InstallUtilityScript(Isolate* isolate) {
  Locker lock(isolate);
  HandleScope scope(isolate);
  // If we use the utility context, we have to set the security tokens so that
  // utility, evaluation and debug context can all access each other.
  v8::Local<v8::Context> utility_context =
      v8::Local<v8::Context>::New(isolate, utility_context_);
  v8::Local<v8::Context> evaluation_context =
      v8::Local<v8::Context>::New(isolate, evaluation_context_);
  utility_context->SetSecurityToken(Undefined(isolate));
  evaluation_context->SetSecurityToken(Undefined(isolate));
  v8::Context::Scope context_scope(utility_context);

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (i::FLAG_debugger) printf("JavaScript debugger enabled\n");
  // Install the debugger object in the utility scope
  i::Debug* debug = reinterpret_cast<i::Isolate*>(isolate)->debug();
  debug->Load();
  i::Handle<i::JSObject> js_debug
      = i::Handle<i::JSObject>(debug->debug_context()->global_object());
  utility_context->Global()->Set(String::NewFromUtf8(isolate, "$debug"),
                                 Utils::ToLocal(js_debug));
  debug->debug_context()->set_security_token(
      reinterpret_cast<i::Isolate*>(isolate)->heap()->undefined_value());
#endif  // ENABLE_DEBUGGER_SUPPORT

  // Run the d8 shell utility script in the utility context
  int source_index = i::NativesCollection<i::D8>::GetIndex("d8");
  i::Vector<const char> shell_source =
      i::NativesCollection<i::D8>::GetRawScriptSource(source_index);
  i::Vector<const char> shell_source_name =
      i::NativesCollection<i::D8>::GetScriptName(source_index);
  Handle<String> source =
      String::NewFromUtf8(isolate, shell_source.start(), String::kNormalString,
                          shell_source.length());
  Handle<String> name =
      String::NewFromUtf8(isolate, shell_source_name.start(),
                          String::kNormalString, shell_source_name.length());
  Handle<Script> script = Script::Compile(source, name);
  script->Run();
  // Mark the d8 shell script as native to avoid it showing up as normal source
  // in the debugger.
  i::Handle<i::Object> compiled_script = Utils::OpenHandle(*script);
  i::Handle<i::Script> script_object = compiled_script->IsJSFunction()
      ? i::Handle<i::Script>(i::Script::cast(
          i::JSFunction::cast(*compiled_script)->shared()->script()))
      : i::Handle<i::Script>(i::Script::cast(
          i::SharedFunctionInfo::cast(*compiled_script)->script()));
  script_object->set_type(i::Smi::FromInt(i::Script::TYPE_NATIVE));

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Start the in-process debugger if requested.
  if (i::FLAG_debugger && !i::FLAG_debugger_agent) {
    v8::Debug::SetDebugEventListener2(HandleDebugEvent);
  }
#endif  // ENABLE_DEBUGGER_SUPPORT
}
#endif  // V8_SHARED


#ifdef COMPRESS_STARTUP_DATA_BZ2
class BZip2Decompressor : public v8::StartupDataDecompressor {
 public:
  virtual ~BZip2Decompressor() { }

 protected:
  virtual int DecompressData(char* raw_data,
                             int* raw_data_size,
                             const char* compressed_data,
                             int compressed_data_size) {
    ASSERT_EQ(v8::StartupData::kBZip2,
              v8::V8::GetCompressedStartupDataAlgorithm());
    unsigned int decompressed_size = *raw_data_size;
    int result =
        BZ2_bzBuffToBuffDecompress(raw_data,
                                   &decompressed_size,
                                   const_cast<char*>(compressed_data),
                                   compressed_data_size,
                                   0, 1);
    if (result == BZ_OK) {
      *raw_data_size = decompressed_size;
    }
    return result;
  }
};
#endif


Handle<ObjectTemplate> Shell::CreateGlobalTemplate(Isolate* isolate) {
  Handle<ObjectTemplate> global_template = ObjectTemplate::New(isolate);
  global_template->Set(String::NewFromUtf8(isolate, "print"),
                       FunctionTemplate::New(isolate, Print));
  global_template->Set(String::NewFromUtf8(isolate, "write"),
                       FunctionTemplate::New(isolate, Write));
  global_template->Set(String::NewFromUtf8(isolate, "read"),
                       FunctionTemplate::New(isolate, Read));
  global_template->Set(String::NewFromUtf8(isolate, "readbuffer"),
                       FunctionTemplate::New(isolate, ReadBuffer));
  global_template->Set(String::NewFromUtf8(isolate, "readline"),
                       FunctionTemplate::New(isolate, ReadLine));
  global_template->Set(String::NewFromUtf8(isolate, "load"),
                       FunctionTemplate::New(isolate, Load));
  global_template->Set(String::NewFromUtf8(isolate, "quit"),
                       FunctionTemplate::New(isolate, Quit));
  global_template->Set(String::NewFromUtf8(isolate, "version"),
                       FunctionTemplate::New(isolate, Version));

  // Bind the Realm object.
  Handle<ObjectTemplate> realm_template = ObjectTemplate::New(isolate);
  realm_template->Set(String::NewFromUtf8(isolate, "current"),
                      FunctionTemplate::New(isolate, RealmCurrent));
  realm_template->Set(String::NewFromUtf8(isolate, "owner"),
                      FunctionTemplate::New(isolate, RealmOwner));
  realm_template->Set(String::NewFromUtf8(isolate, "global"),
                      FunctionTemplate::New(isolate, RealmGlobal));
  realm_template->Set(String::NewFromUtf8(isolate, "create"),
                      FunctionTemplate::New(isolate, RealmCreate));
  realm_template->Set(String::NewFromUtf8(isolate, "dispose"),
                      FunctionTemplate::New(isolate, RealmDispose));
  realm_template->Set(String::NewFromUtf8(isolate, "switch"),
                      FunctionTemplate::New(isolate, RealmSwitch));
  realm_template->Set(String::NewFromUtf8(isolate, "eval"),
                      FunctionTemplate::New(isolate, RealmEval));
  realm_template->SetAccessor(String::NewFromUtf8(isolate, "shared"),
                              RealmSharedGet, RealmSharedSet);
  global_template->Set(String::NewFromUtf8(isolate, "Realm"), realm_template);

#ifndef V8_SHARED
  Handle<ObjectTemplate> performance_template = ObjectTemplate::New(isolate);
  performance_template->Set(String::NewFromUtf8(isolate, "now"),
                            FunctionTemplate::New(isolate, PerformanceNow));
  global_template->Set(String::NewFromUtf8(isolate, "performance"),
                       performance_template);
#endif  // V8_SHARED

#if !defined(V8_SHARED) && !defined(_WIN32) && !defined(_WIN64)
  Handle<ObjectTemplate> os_templ = ObjectTemplate::New(isolate);
  AddOSMethods(isolate, os_templ);
  global_template->Set(String::NewFromUtf8(isolate, "os"), os_templ);
#endif  // V8_SHARED

  return global_template;
}


void Shell::Initialize(Isolate* isolate) {
#ifdef COMPRESS_STARTUP_DATA_BZ2
  BZip2Decompressor startup_data_decompressor;
  int bz2_result = startup_data_decompressor.Decompress();
  if (bz2_result != BZ_OK) {
    fprintf(stderr, "bzip error code: %d\n", bz2_result);
    Exit(1);
  }
#endif

#ifndef V8_SHARED
  Shell::counter_map_ = new CounterMap();
  // Set up counters
  if (i::StrLength(i::FLAG_map_counters) != 0)
    MapCounters(i::FLAG_map_counters);
  if (i::FLAG_dump_counters || i::FLAG_track_gc_object_stats) {
    V8::SetCounterFunction(LookupCounter);
    V8::SetCreateHistogramFunction(CreateHistogram);
    V8::SetAddHistogramSampleFunction(AddHistogramSample);
  }
#endif  // V8_SHARED
}


void Shell::InitializeDebugger(Isolate* isolate) {
  if (options.test_shell) return;
#ifndef V8_SHARED
  Locker lock(isolate);
  HandleScope scope(isolate);
  Handle<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  utility_context_.Reset(isolate,
                         Context::New(isolate, NULL, global_template));

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Start the debugger agent if requested.
  if (i::FLAG_debugger_agent) {
    v8::Debug::EnableAgent("d8 shell", i::FLAG_debugger_port, true);
    v8::Debug::SetDebugMessageDispatchHandler(DispatchDebugMessages, true);
  }
#endif  // ENABLE_DEBUGGER_SUPPORT
#endif  // V8_SHARED
}


Local<Context> Shell::CreateEvaluationContext(Isolate* isolate) {
#ifndef V8_SHARED
  // This needs to be a critical section since this is not thread-safe
  i::LockGuard<i::Mutex> lock_guard(&context_mutex_);
#endif  // V8_SHARED
  // Initialize the global objects
  Handle<ObjectTemplate> global_template = CreateGlobalTemplate(isolate);
  EscapableHandleScope handle_scope(isolate);
  Local<Context> context = Context::New(isolate, NULL, global_template);
  ASSERT(!context.IsEmpty());
  Context::Scope scope(context);

#ifndef V8_SHARED
  i::Factory* factory = reinterpret_cast<i::Isolate*>(isolate)->factory();
  i::JSArguments js_args = i::FLAG_js_arguments;
  i::Handle<i::FixedArray> arguments_array =
      factory->NewFixedArray(js_args.argc);
  for (int j = 0; j < js_args.argc; j++) {
    i::Handle<i::String> arg =
        factory->NewStringFromUtf8(i::CStrVector(js_args[j]));
    arguments_array->set(j, *arg);
  }
  i::Handle<i::JSArray> arguments_jsarray =
      factory->NewJSArrayWithElements(arguments_array);
  context->Global()->Set(String::NewFromUtf8(isolate, "arguments"),
                         Utils::ToLocal(arguments_jsarray));
#endif  // V8_SHARED
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
#endif  // V8_SHARED


void Shell::OnExit() {
  LineEditor* line_editor = LineEditor::Get();
  if (line_editor) line_editor->Close();
#ifndef V8_SHARED
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
#endif  // V8_SHARED
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
  // Release the V8 lock while reading files.
  v8::Unlocker unlocker(isolate);
  FILE* file = FOpen(name, "rb");
  if (file == NULL) return NULL;

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = static_cast<int>(fread(&chars[i], 1, size - i, file));
    i += read;
  }
  fclose(file);
  *size_out = size;
  return chars;
}


struct DataAndPersistent {
  uint8_t* data;
  Persistent<ArrayBuffer> handle;
};


static void ReadBufferWeakCallback(
    const v8::WeakCallbackData<ArrayBuffer, DataAndPersistent>& data) {
  size_t byte_length = data.GetValue()->ByteLength();
  data.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(
      -static_cast<intptr_t>(byte_length));

  delete[] data.GetParameter()->data;
  data.GetParameter()->handle.Reset();
  delete data.GetParameter();
}


void Shell::ReadBuffer(const v8::FunctionCallbackInfo<v8::Value>& args) {
  ASSERT(sizeof(char) == sizeof(uint8_t));  // NOLINT
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
  Handle<v8::ArrayBuffer> buffer =
      ArrayBuffer::New(isolate, data->data, length);
  data->handle.Reset(isolate, buffer);
  data->handle.SetWeak(data, ReadBufferWeakCallback);
  data->handle.MarkIndependent();
  isolate->AdjustAmountOfExternalAllocatedMemory(length);

  args.GetReturnValue().Set(buffer);
}


#ifndef V8_SHARED
static char* ReadToken(char* data, char token) {
  char* next = i::OS::StrChr(data, token);
  if (next != NULL) {
    *next = '\0';
    return (next + 1);
  }

  return NULL;
}


static char* ReadLine(char* data) {
  return ReadToken(data, '\n');
}


static char* ReadWord(char* data) {
  return ReadToken(data, ' ');
}
#endif  // V8_SHARED


// Reads a file into a v8 string.
Handle<String> Shell::ReadFile(Isolate* isolate, const char* name) {
  int size = 0;
  char* chars = ReadChars(isolate, name, &size);
  if (chars == NULL) return Handle<String>();
  Handle<String> result =
      String::NewFromUtf8(isolate, chars, String::kNormalString, size);
  delete[] chars;
  return result;
}


void Shell::RunShell(Isolate* isolate) {
  Locker locker(isolate);
  HandleScope outer_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, evaluation_context_);
  v8::Context::Scope context_scope(context);
  PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
  Handle<String> name = String::NewFromUtf8(isolate, "(d8)");
  LineEditor* console = LineEditor::Get();
  printf("V8 version %s [console: %s]\n", V8::GetVersion(), console->name());
  console->Open(isolate);
  while (true) {
    HandleScope inner_scope(isolate);
    Handle<String> input = console->Prompt(Shell::kPrompt);
    if (input.IsEmpty()) break;
    ExecuteString(isolate, input, name, true, true);
  }
  printf("\n");
}


#ifndef V8_SHARED
class ShellThread : public i::Thread {
 public:
  // Takes ownership of the underlying char array of |files|.
  ShellThread(Isolate* isolate, char* files)
      : Thread("d8:ShellThread"),
        isolate_(isolate), files_(files) { }

  ~ShellThread() {
    delete[] files_;
  }

  virtual void Run();
 private:
  Isolate* isolate_;
  char* files_;
};


void ShellThread::Run() {
  char* ptr = files_;
  while ((ptr != NULL) && (*ptr != '\0')) {
    // For each newline-separated line.
    char* next_line = ReadLine(ptr);

    if (*ptr == '#') {
      // Skip comment lines.
      ptr = next_line;
      continue;
    }

    // Prepare the context for this thread.
    Locker locker(isolate_);
    HandleScope outer_scope(isolate_);
    Local<Context> thread_context =
        Shell::CreateEvaluationContext(isolate_);
    Context::Scope context_scope(thread_context);
    PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate_));

    while ((ptr != NULL) && (*ptr != '\0')) {
      HandleScope inner_scope(isolate_);
      char* filename = ptr;
      ptr = ReadWord(ptr);

      // Skip empty strings.
      if (strlen(filename) == 0) {
        continue;
      }

      Handle<String> str = Shell::ReadFile(isolate_, filename);
      if (str.IsEmpty()) {
        printf("File '%s' not found\n", filename);
        Shell::Exit(1);
      }

      Shell::ExecuteString(
          isolate_, str, String::NewFromUtf8(isolate_, filename), false, false);
    }

    ptr = next_line;
  }
}
#endif  // V8_SHARED


SourceGroup::~SourceGroup() {
#ifndef V8_SHARED
  delete thread_;
  thread_ = NULL;
#endif  // V8_SHARED
}


void SourceGroup::Execute(Isolate* isolate) {
  bool exception_was_thrown = false;
  for (int i = begin_offset_; i < end_offset_; ++i) {
    const char* arg = argv_[i];
    if (strcmp(arg, "-e") == 0 && i + 1 < end_offset_) {
      // Execute argument given to -e option directly.
      HandleScope handle_scope(isolate);
      Handle<String> file_name = String::NewFromUtf8(isolate, "unnamed");
      Handle<String> source = String::NewFromUtf8(isolate, argv_[i + 1]);
      if (!Shell::ExecuteString(isolate, source, file_name, false, true)) {
        exception_was_thrown = true;
        break;
      }
      ++i;
    } else if (arg[0] == '-') {
      // Ignore other options. They have been parsed already.
    } else {
      // Use all other arguments as names of files to load and run.
      HandleScope handle_scope(isolate);
      Handle<String> file_name = String::NewFromUtf8(isolate, arg);
      Handle<String> source = ReadFile(isolate, arg);
      if (source.IsEmpty()) {
        printf("Error reading '%s'\n", arg);
        Shell::Exit(1);
      }
      if (!Shell::ExecuteString(isolate, source, file_name, false, true)) {
        exception_was_thrown = true;
        break;
      }
    }
  }
  if (exception_was_thrown != Shell::options.expected_to_throw) {
    Shell::Exit(1);
  }
}


Handle<String> SourceGroup::ReadFile(Isolate* isolate, const char* name) {
  int size;
  char* chars = ReadChars(isolate, name, &size);
  if (chars == NULL) return Handle<String>();
  Handle<String> result =
      String::NewFromUtf8(isolate, chars, String::kNormalString, size);
  delete[] chars;
  return result;
}


#ifndef V8_SHARED
i::Thread::Options SourceGroup::GetThreadOptions() {
  // On some systems (OSX 10.6) the stack size default is 0.5Mb or less
  // which is not enough to parse the big literal expressions used in tests.
  // The stack size should be at least StackGuard::kLimitSize + some
  // OS-specific padding for thread startup code.  2Mbytes seems to be enough.
  return i::Thread::Options("IsolateThread", 2 * MB);
}


void SourceGroup::ExecuteInThread() {
  Isolate* isolate = Isolate::New();
  do {
    next_semaphore_.Wait();
    {
      Isolate::Scope iscope(isolate);
      Locker lock(isolate);
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
      if (Shell::options.send_idle_notification) {
        const int kLongIdlePauseInMs = 1000;
        V8::ContextDisposedNotification();
        V8::IdleNotification(kLongIdlePauseInMs);
      }
    }
    done_semaphore_.Signal();
  } while (!Shell::options.last_run);
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
  if (Shell::options.last_run) {
    thread_->Join();
  } else {
    done_semaphore_.Wait();
  }
}
#endif  // V8_SHARED


bool Shell::SetOptions(int argc, char* argv[]) {
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
    } else if (strcmp(argv[i], "--shell") == 0) {
      options.interactive_shell = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--test") == 0) {
      options.test_shell = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--send-idle-notification") == 0) {
      options.send_idle_notification = true;
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
    } else if (strcmp(argv[i], "-p") == 0) {
#ifdef V8_SHARED
      printf("D8 with shared library does not support multi-threading\n");
      return false;
#else
      options.num_parallel_files++;
#endif  // V8_SHARED
    } else if (strcmp(argv[i], "--dump-heap-constants") == 0) {
#ifdef V8_SHARED
      printf("D8 with shared library does not support constant dumping\n");
      return false;
#else
      options.dump_heap_constants = true;
      argv[i] = NULL;
#endif
    } else if (strcmp(argv[i], "--throws") == 0) {
      options.expected_to_throw = true;
      argv[i] = NULL;
    }
#ifdef V8_SHARED
    else if (strcmp(argv[i], "--dump-counters") == 0) {
      printf("D8 with shared library does not include counters\n");
      return false;
    } else if (strcmp(argv[i], "--debugger") == 0) {
      printf("Javascript debugger not included\n");
      return false;
    }
#endif  // V8_SHARED
  }

#ifndef V8_SHARED
  // Run parallel threads if we are not using --isolate
  options.parallel_files = new char*[options.num_parallel_files];
  int parallel_files_set = 0;
  for (int i = 1; i < argc; i++) {
    if (argv[i] == NULL) continue;
    if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
      if (options.num_isolates > 1) {
        printf("-p is not compatible with --isolate\n");
        return false;
      }
      argv[i] = NULL;
      i++;
      options.parallel_files[parallel_files_set] = argv[i];
      parallel_files_set++;
      argv[i] = NULL;
    }
  }
  if (parallel_files_set != options.num_parallel_files) {
    printf("-p requires a file containing a list of files as parameter\n");
    return false;
  }
#endif  // V8_SHARED

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
    } else if (strncmp(argv[i], "--", 2) == 0) {
      printf("Warning: unknown flag %s.\nTry --help for options\n", argv[i]);
    }
  }
  current->End(argc);

  return true;
}


int Shell::RunMain(Isolate* isolate, int argc, char* argv[]) {
#ifndef V8_SHARED
  i::List<i::Thread*> threads(1);
  if (options.parallel_files != NULL) {
    for (int i = 0; i < options.num_parallel_files; i++) {
      char* files = NULL;
      { Locker lock(isolate);
        int size = 0;
        files = ReadChars(isolate, options.parallel_files[i], &size);
      }
      if (files == NULL) {
        printf("File list '%s' not found\n", options.parallel_files[i]);
        Exit(1);
      }
      ShellThread* thread = new ShellThread(isolate, files);
      thread->Start();
      threads.Add(thread);
    }
  }
  for (int i = 1; i < options.num_isolates; ++i) {
    options.isolate_sources[i].StartExecuteInThread();
  }
#endif  // V8_SHARED
  {  // NOLINT
    Locker lock(isolate);
    {
      HandleScope scope(isolate);
      Local<Context> context = CreateEvaluationContext(isolate);
      if (options.last_run) {
        // Keep using the same context in the interactive shell.
        evaluation_context_.Reset(isolate, context);
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
        // If the interactive debugger is enabled make sure to activate
        // it before running the files passed on the command line.
        if (i::FLAG_debugger) {
          InstallUtilityScript(isolate);
        }
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
      }
      {
        Context::Scope cscope(context);
        PerIsolateData::RealmScope realm_scope(PerIsolateData::Get(isolate));
        options.isolate_sources[0].Execute(isolate);
      }
    }
    if (!options.last_run) {
      if (options.send_idle_notification) {
        const int kLongIdlePauseInMs = 1000;
        V8::ContextDisposedNotification();
        V8::IdleNotification(kLongIdlePauseInMs);
      }
    }
  }

#ifndef V8_SHARED
  for (int i = 1; i < options.num_isolates; ++i) {
    options.isolate_sources[i].WaitForThread();
  }

  for (int i = 0; i < threads.length(); i++) {
    i::Thread* thread = threads[i];
    thread->Join();
    delete thread;
  }
#endif  // V8_SHARED
  return 0;
}


#ifdef V8_SHARED
static void SetStandaloneFlagsViaCommandLine() {
  int fake_argc = 3;
  char **fake_argv = new char*[3];
  fake_argv[0] = NULL;
  fake_argv[1] = strdup("--trace-hydrogen-file=hydrogen.cfg");
  fake_argv[2] = strdup("--redirect-code-traces-to=code.asm");
  v8::V8::SetFlagsFromCommandLine(&fake_argc, fake_argv, false);
  free(fake_argv[1]);
  free(fake_argv[2]);
  delete[] fake_argv;
}
#endif


#ifndef V8_SHARED
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
#endif  // V8_SHARED


class ShellArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* result = malloc(length);
    memset(result, 0, length);
    return result;
  }
  virtual void* AllocateUninitialized(size_t length) {
    return malloc(length);
  }
  virtual void Free(void* data, size_t) { free(data); }
  // TODO(dslomov): Remove when v8:2823 is fixed.
  virtual void Free(void* data) {
#ifndef V8_SHARED
    UNREACHABLE();
#endif
  }
};


class MockArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t) V8_OVERRIDE {
    return malloc(0);
  }
  virtual void* AllocateUninitialized(size_t length) V8_OVERRIDE {
    return malloc(0);
  }
  virtual void Free(void*, size_t) V8_OVERRIDE {
  }
};


int Shell::Main(int argc, char* argv[]) {
  if (!SetOptions(argc, argv)) return 1;
  v8::V8::InitializeICU();
#ifndef V8_SHARED
  i::FLAG_trace_hydrogen_file = "hydrogen.cfg";
  i::FLAG_redirect_code_traces_to = "code.asm";
#else
  SetStandaloneFlagsViaCommandLine();
#endif
  ShellArrayBufferAllocator array_buffer_allocator;
  MockArrayBufferAllocator mock_arraybuffer_allocator;
  if (options.mock_arraybuffer_allocator) {
    v8::V8::SetArrayBufferAllocator(&mock_arraybuffer_allocator);
  } else {
    v8::V8::SetArrayBufferAllocator(&array_buffer_allocator);
  }
  int result = 0;
  Isolate* isolate = Isolate::GetCurrent();
#ifndef V8_SHARED
  v8::ResourceConstraints constraints;
  constraints.ConfigureDefaults(i::OS::TotalPhysicalMemory(),
                                i::CPU::NumberOfProcessorsOnline());
  v8::SetResourceConstraints(isolate, &constraints);
#endif
  DumbLineEditor dumb_line_editor(isolate);
  {
    Initialize(isolate);
#ifdef ENABLE_VTUNE_JIT_INTERFACE
    vTune::InitializeVtuneForV8();
#endif
    PerIsolateData data(isolate);
    InitializeDebugger(isolate);

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
      int stress_runs = Testing::GetStressRuns();
      for (int i = 0; i < stress_runs && result == 0; i++) {
        printf("============ Stress %d/%d ============\n", i + 1, stress_runs);
        Testing::PrepareStressRun(i);
        options.last_run = (i == stress_runs - 1);
        result = RunMain(isolate, argc, argv);
      }
      printf("======== Full Deoptimization =======\n");
      Testing::DeoptimizeAll();
#if !defined(V8_SHARED)
    } else if (i::FLAG_stress_runs > 0) {
      int stress_runs = i::FLAG_stress_runs;
      for (int i = 0; i < stress_runs && result == 0; i++) {
        printf("============ Run %d/%d ============\n", i + 1, stress_runs);
        options.last_run = (i == stress_runs - 1);
        result = RunMain(isolate, argc, argv);
      }
#endif
    } else {
      result = RunMain(isolate, argc, argv);
    }


#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
    // Run remote debugger if requested, but never on --test
    if (i::FLAG_remote_debugger && !options.test_shell) {
      InstallUtilityScript(isolate);
      RunRemoteDebugger(isolate, i::FLAG_debugger_port);
      return 0;
    }
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT

    // Run interactive shell if explicitly requested or if no script has been
    // executed, but never on --test

    if (( options.interactive_shell || !options.script_executed )
        && !options.test_shell ) {
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
      if (!i::FLAG_debugger) {
        InstallUtilityScript(isolate);
      }
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
      RunShell(isolate);
    }
  }
  V8::Dispose();

  OnExit();

  return result;
}

}  // namespace v8


#ifndef GOOGLE3
int main(int argc, char* argv[]) {
  return v8::Shell::Main(argc, argv);
}
#endif
