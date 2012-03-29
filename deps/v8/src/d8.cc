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


#ifdef USING_V8_SHARED  // Defined when linking against shared lib on Windows.
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
#include "../include/v8-testing.h"
#endif  // V8_SHARED

#include "d8.h"

#ifndef V8_SHARED
#include "api.h"
#include "checks.h"
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

LineEditor *LineEditor::first_ = NULL;


LineEditor::LineEditor(Type type, const char* name)
    : type_(type),
      name_(name),
      next_(first_) {
  first_ = this;
}


LineEditor* LineEditor::Get() {
  LineEditor* current = first_;
  LineEditor* best = current;
  while (current != NULL) {
    if (current->type_ > best->type_)
      best = current;
    current = current->next_;
  }
  return best;
}


class DumbLineEditor: public LineEditor {
 public:
  DumbLineEditor() : LineEditor(LineEditor::DUMB, "dumb") { }
  virtual Handle<String> Prompt(const char* prompt);
};


static DumbLineEditor dumb_line_editor;


Handle<String> DumbLineEditor::Prompt(const char* prompt) {
  printf("%s", prompt);
  return Shell::ReadFromStdin();
}


#ifndef V8_SHARED
CounterMap* Shell::counter_map_;
i::OS::MemoryMappedFile* Shell::counters_file_ = NULL;
CounterCollection Shell::local_counters_;
CounterCollection* Shell::counters_ = &local_counters_;
i::Mutex* Shell::context_mutex_(i::OS::CreateMutex());
Persistent<Context> Shell::utility_context_;
#endif  // V8_SHARED

LineEditor* Shell::console = NULL;
Persistent<Context> Shell::evaluation_context_;
ShellOptions Shell::options;
const char* Shell::kPrompt = "d8> ";


const int MB = 1024 * 1024;


#ifndef V8_SHARED
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
bool Shell::ExecuteString(Handle<String> source,
                          Handle<Value> name,
                          bool print_result,
                          bool report_exceptions) {
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
  bool FLAG_debugger = i::FLAG_debugger;
#else
  bool FLAG_debugger = false;
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
  HandleScope handle_scope;
  TryCatch try_catch;
  options.script_executed = true;
  if (FLAG_debugger) {
    // When debugging make exceptions appear to be uncaught.
    try_catch.SetVerbose(true);
  }
  Handle<Script> script = Script::Compile(source, name);
  if (script.IsEmpty()) {
    // Print errors that happened during compilation.
    if (report_exceptions && !FLAG_debugger)
      ReportException(&try_catch);
    return false;
  } else {
    Handle<Value> result = script->Run();
    if (result.IsEmpty()) {
      ASSERT(try_catch.HasCaught());
      // Print errors that happened during execution.
      if (report_exceptions && !FLAG_debugger)
        ReportException(&try_catch);
      return false;
    } else {
      ASSERT(!try_catch.HasCaught());
      if (print_result && !result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        v8::String::Utf8Value str(result);
        size_t count = fwrite(*str, sizeof(**str), str.length(), stdout);
        (void) count;  // Silence GCC-4.5.x "unused result" warning.
        printf("\n");
      }
      return true;
    }
  }
}


Handle<Value> Shell::Print(const Arguments& args) {
  Handle<Value> val = Write(args);
  printf("\n");
  fflush(stdout);
  return val;
}


Handle<Value> Shell::Write(const Arguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope;
    if (i != 0) {
      printf(" ");
    }
    v8::String::Utf8Value str(args[i]);
    int n = static_cast<int>(fwrite(*str, sizeof(**str), str.length(), stdout));
    if (n != str.length()) {
      printf("Error in fwrite\n");
      Exit(1);
    }
  }
  return Undefined();
}


Handle<Value> Shell::EnableProfiler(const Arguments& args) {
  V8::ResumeProfiler();
  return Undefined();
}


Handle<Value> Shell::DisableProfiler(const Arguments& args) {
  V8::PauseProfiler();
  return Undefined();
}


Handle<Value> Shell::Read(const Arguments& args) {
  String::Utf8Value file(args[0]);
  if (*file == NULL) {
    return ThrowException(String::New("Error loading file"));
  }
  Handle<String> source = ReadFile(*file);
  if (source.IsEmpty()) {
    return ThrowException(String::New("Error loading file"));
  }
  return source;
}


Handle<String> Shell::ReadFromStdin() {
  static const int kBufferSize = 256;
  char buffer[kBufferSize];
  Handle<String> accumulator = String::New("");
  int length;
  while (true) {
    // Continue reading if the line ends with an escape '\\' or the line has
    // not been fully read into the buffer yet (does not end with '\n').
    // If fgets gets an error, just give up.
    char* input = NULL;
    {  // Release lock for blocking input.
      Unlocker unlock(Isolate::GetCurrent());
      input = fgets(buffer, kBufferSize, stdin);
    }
    if (input == NULL) return Handle<String>();
    length = static_cast<int>(strlen(buffer));
    if (length == 0) {
      return accumulator;
    } else if (buffer[length-1] != '\n') {
      accumulator = String::Concat(accumulator, String::New(buffer, length));
    } else if (length > 1 && buffer[length-2] == '\\') {
      buffer[length-2] = '\n';
      accumulator = String::Concat(accumulator, String::New(buffer, length-1));
    } else {
      return String::Concat(accumulator, String::New(buffer, length-1));
    }
  }
}


Handle<Value> Shell::Load(const Arguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope;
    String::Utf8Value file(args[i]);
    if (*file == NULL) {
      return ThrowException(String::New("Error loading file"));
    }
    Handle<String> source = ReadFile(*file);
    if (source.IsEmpty()) {
      return ThrowException(String::New("Error loading file"));
    }
    if (!ExecuteString(source, String::New(*file), false, true)) {
      return ThrowException(String::New("Error executing file"));
    }
  }
  return Undefined();
}

static size_t convertToUint(Local<Value> value_in, TryCatch* try_catch) {
  if (value_in->IsUint32()) {
    return value_in->Uint32Value();
  }

  Local<Value> number = value_in->ToNumber();
  if (try_catch->HasCaught()) return 0;

  ASSERT(number->IsNumber());
  Local<Int32> int32 = number->ToInt32();
  if (try_catch->HasCaught() || int32.IsEmpty()) return 0;

  int32_t raw_value = int32->Int32Value();
  if (try_catch->HasCaught()) return 0;

  if (raw_value < 0) {
    ThrowException(String::New("Array length must not be negative."));
    return 0;
  }

  static const int kMaxLength = 0x3fffffff;
#ifndef V8_SHARED
  ASSERT(kMaxLength == i::ExternalArray::kMaxLength);
#endif  // V8_SHARED
  if (raw_value > static_cast<int32_t>(kMaxLength)) {
    ThrowException(
        String::New("Array length exceeds maximum length."));
  }
  return static_cast<size_t>(raw_value);
}


const char kArrayBufferReferencePropName[] = "_is_array_buffer_";
const char kArrayBufferMarkerPropName[] = "_array_buffer_ref_";


Handle<Value> Shell::CreateExternalArray(const Arguments& args,
                                         ExternalArrayType type,
                                         size_t element_size) {
  TryCatch try_catch;
  bool is_array_buffer_construct = element_size == 0;
  if (is_array_buffer_construct) {
    type = v8::kExternalByteArray;
    element_size = 1;
  }
  ASSERT(element_size == 1 || element_size == 2 || element_size == 4 ||
         element_size == 8);
  if (args.Length() == 0) {
    return ThrowException(
        String::New("Array constructor must have at least one "
                    "parameter."));
  }
  bool first_arg_is_array_buffer =
      args[0]->IsObject() &&
      args[0]->ToObject()->Get(
          String::New(kArrayBufferMarkerPropName))->IsTrue();
  // Currently, only the following constructors are supported:
  //   TypedArray(unsigned long length)
  //   TypedArray(ArrayBuffer buffer,
  //              optional unsigned long byteOffset,
  //              optional unsigned long length)
  if (args.Length() > 3) {
    return ThrowException(
        String::New("Array constructor from ArrayBuffer must "
                    "have 1-3 parameters."));
  }

  Local<Value> length_value = (args.Length() < 3)
      ? (first_arg_is_array_buffer
         ? args[0]->ToObject()->Get(String::New("length"))
         : args[0])
      : args[2];
  size_t length = convertToUint(length_value, &try_catch);
  if (try_catch.HasCaught()) return try_catch.Exception();

  void* data = NULL;
  size_t offset = 0;

  Handle<Object> array = Object::New();
  if (first_arg_is_array_buffer) {
    Handle<Object> derived_from = args[0]->ToObject();
    data = derived_from->GetIndexedPropertiesExternalArrayData();

    size_t array_buffer_length = convertToUint(
        derived_from->Get(String::New("length")),
        &try_catch);
    if (try_catch.HasCaught()) return try_catch.Exception();

    if (data == NULL && array_buffer_length != 0) {
      return ThrowException(
          String::New("ArrayBuffer doesn't have data"));
    }

    if (args.Length() > 1) {
      offset = convertToUint(args[1], &try_catch);
      if (try_catch.HasCaught()) return try_catch.Exception();

      // The given byteOffset must be a multiple of the element size of the
      // specific type, otherwise an exception is raised.
      if (offset % element_size != 0) {
        return ThrowException(
            String::New("offset must be multiple of element_size"));
      }
    }

    if (offset > array_buffer_length) {
      return ThrowException(
          String::New("byteOffset must be less than ArrayBuffer length."));
    }

    if (args.Length() == 2) {
      // If length is not explicitly specified, the length of the ArrayBuffer
      // minus the byteOffset must be a multiple of the element size of the
      // specific type, or an exception is raised.
      length = array_buffer_length - offset;
    }

    if (args.Length() != 3) {
      if (length % element_size != 0) {
        return ThrowException(
            String::New("ArrayBuffer length minus the byteOffset must be a "
                        "multiple of the element size"));
      }
      length /= element_size;
    }

    // If a given byteOffset and length references an area beyond the end of
    // the ArrayBuffer an exception is raised.
    if (offset + (length * element_size) > array_buffer_length) {
      return ThrowException(
          String::New("length references an area beyond the end of the "
                      "ArrayBuffer"));
    }

    // Hold a reference to the ArrayBuffer so its buffer doesn't get collected.
    array->Set(String::New(kArrayBufferReferencePropName), args[0], ReadOnly);
  }

  if (is_array_buffer_construct) {
    array->Set(String::New(kArrayBufferMarkerPropName), True(), ReadOnly);
  }

  Persistent<Object> persistent_array = Persistent<Object>::New(array);
  persistent_array.MakeWeak(data, ExternalArrayWeakCallback);
  persistent_array.MarkIndependent();
  if (data == NULL && length != 0) {
    data = calloc(length, element_size);
    if (data == NULL) {
      return ThrowException(String::New("Memory allocation failed."));
    }
  }

  array->SetIndexedPropertiesToExternalArrayData(
      reinterpret_cast<uint8_t*>(data) + offset, type,
      static_cast<int>(length));
  array->Set(String::New("length"),
             Int32::New(static_cast<int32_t>(length)), ReadOnly);
  array->Set(String::New("BYTES_PER_ELEMENT"),
             Int32::New(static_cast<int32_t>(element_size)));
  return array;
}


void Shell::ExternalArrayWeakCallback(Persistent<Value> object, void* data) {
  HandleScope scope;
  Handle<String> prop_name = String::New(kArrayBufferReferencePropName);
  Handle<Object> converted_object = object->ToObject();
  Local<Value> prop_value = converted_object->Get(prop_name);
  if (data != NULL && !prop_value->IsObject()) {
    free(data);
  }
  object.Dispose();
}


Handle<Value> Shell::ArrayBuffer(const Arguments& args) {
  return CreateExternalArray(args, v8::kExternalByteArray, 0);
}


Handle<Value> Shell::Int8Array(const Arguments& args) {
  return CreateExternalArray(args, v8::kExternalByteArray, sizeof(int8_t));
}


Handle<Value> Shell::Uint8Array(const Arguments& args) {
  return CreateExternalArray(args, kExternalUnsignedByteArray, sizeof(uint8_t));
}


Handle<Value> Shell::Int16Array(const Arguments& args) {
  return CreateExternalArray(args, kExternalShortArray, sizeof(int16_t));
}


Handle<Value> Shell::Uint16Array(const Arguments& args) {
  return CreateExternalArray(args, kExternalUnsignedShortArray,
                             sizeof(uint16_t));
}


Handle<Value> Shell::Int32Array(const Arguments& args) {
  return CreateExternalArray(args, kExternalIntArray, sizeof(int32_t));
}


Handle<Value> Shell::Uint32Array(const Arguments& args) {
  return CreateExternalArray(args, kExternalUnsignedIntArray, sizeof(uint32_t));
}


Handle<Value> Shell::Float32Array(const Arguments& args) {
  return CreateExternalArray(args, kExternalFloatArray,
                             sizeof(float));  // NOLINT
}


Handle<Value> Shell::Float64Array(const Arguments& args) {
  return CreateExternalArray(args, kExternalDoubleArray,
                             sizeof(double));  // NOLINT
}


Handle<Value> Shell::PixelArray(const Arguments& args) {
  return CreateExternalArray(args, kExternalPixelArray, sizeof(uint8_t));
}


Handle<Value> Shell::Yield(const Arguments& args) {
  v8::Unlocker unlocker;
  return Undefined();
}


Handle<Value> Shell::Quit(const Arguments& args) {
  int exit_code = args[0]->Int32Value();
#ifndef V8_SHARED
  OnExit();
#endif  // V8_SHARED
  exit(exit_code);
  return Undefined();
}


Handle<Value> Shell::Version(const Arguments& args) {
  return String::New(V8::GetVersion());
}


void Shell::ReportException(v8::TryCatch* try_catch) {
  HandleScope handle_scope;
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
  bool enter_context = !Context::InContext();
  if (enter_context) utility_context_->Enter();
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
  if (enter_context) utility_context_->Exit();
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
}


#ifndef V8_SHARED
Handle<Array> Shell::GetCompletions(Handle<String> text, Handle<String> full) {
  HandleScope handle_scope;
  Context::Scope context_scope(utility_context_);
  Handle<Object> global = utility_context_->Global();
  Handle<Value> fun = global->Get(String::New("GetCompletions"));
  static const int kArgc = 3;
  Handle<Value> argv[kArgc] = { evaluation_context_->Global(), text, full };
  Handle<Value> val = Handle<Function>::Cast(fun)->Call(global, kArgc, argv);
  return handle_scope.Close(Handle<Array>::Cast(val));
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Handle<Object> Shell::DebugMessageDetails(Handle<String> message) {
  Context::Scope context_scope(utility_context_);
  Handle<Object> global = utility_context_->Global();
  Handle<Value> fun = global->Get(String::New("DebugMessageDetails"));
  static const int kArgc = 1;
  Handle<Value> argv[kArgc] = { message };
  Handle<Value> val = Handle<Function>::Cast(fun)->Call(global, kArgc, argv);
  return Handle<Object>::Cast(val);
}


Handle<Value> Shell::DebugCommandToJSONRequest(Handle<String> command) {
  Context::Scope context_scope(utility_context_);
  Handle<Object> global = utility_context_->Global();
  Handle<Value> fun = global->Get(String::New("DebugCommandToJSONRequest"));
  static const int kArgc = 1;
  Handle<Value> argv[kArgc] = { command };
  Handle<Value> val = Handle<Function>::Cast(fun)->Call(global, kArgc, argv);
  return val;
}


void Shell::DispatchDebugMessages() {
  v8::Context::Scope scope(Shell::evaluation_context_);
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


void Shell::InstallUtilityScript() {
  Locker lock;
  HandleScope scope;
  // If we use the utility context, we have to set the security tokens so that
  // utility, evaluation and debug context can all access each other.
  utility_context_->SetSecurityToken(Undefined());
  evaluation_context_->SetSecurityToken(Undefined());
  Context::Scope utility_scope(utility_context_);

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (i::FLAG_debugger) printf("JavaScript debugger enabled\n");
  // Install the debugger object in the utility scope
  i::Debug* debug = i::Isolate::Current()->debug();
  debug->Load();
  i::Handle<i::JSObject> js_debug
      = i::Handle<i::JSObject>(debug->debug_context()->global());
  utility_context_->Global()->Set(String::New("$debug"),
                                  Utils::ToLocal(js_debug));
  debug->debug_context()->set_security_token(HEAP->undefined_value());
#endif  // ENABLE_DEBUGGER_SUPPORT

  // Run the d8 shell utility script in the utility context
  int source_index = i::NativesCollection<i::D8>::GetIndex("d8");
  i::Vector<const char> shell_source =
      i::NativesCollection<i::D8>::GetRawScriptSource(source_index);
  i::Vector<const char> shell_source_name =
      i::NativesCollection<i::D8>::GetScriptName(source_index);
  Handle<String> source = String::New(shell_source.start(),
      shell_source.length());
  Handle<String> name = String::New(shell_source_name.start(),
      shell_source_name.length());
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
    v8::Debug::SetDebugEventListener(HandleDebugEvent);
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

Handle<ObjectTemplate> Shell::CreateGlobalTemplate() {
  Handle<ObjectTemplate> global_template = ObjectTemplate::New();
  global_template->Set(String::New("print"), FunctionTemplate::New(Print));
  global_template->Set(String::New("write"), FunctionTemplate::New(Write));
  global_template->Set(String::New("read"), FunctionTemplate::New(Read));
  global_template->Set(String::New("readbinary"),
                       FunctionTemplate::New(ReadBinary));
  global_template->Set(String::New("readline"),
                       FunctionTemplate::New(ReadLine));
  global_template->Set(String::New("load"), FunctionTemplate::New(Load));
  global_template->Set(String::New("quit"), FunctionTemplate::New(Quit));
  global_template->Set(String::New("version"), FunctionTemplate::New(Version));
  global_template->Set(String::New("enableProfiler"),
                       FunctionTemplate::New(EnableProfiler));
  global_template->Set(String::New("disableProfiler"),
                       FunctionTemplate::New(DisableProfiler));

  // Bind the handlers for external arrays.
  global_template->Set(String::New("ArrayBuffer"),
                       FunctionTemplate::New(ArrayBuffer));
  global_template->Set(String::New("Int8Array"),
                       FunctionTemplate::New(Int8Array));
  global_template->Set(String::New("Uint8Array"),
                       FunctionTemplate::New(Uint8Array));
  global_template->Set(String::New("Int16Array"),
                       FunctionTemplate::New(Int16Array));
  global_template->Set(String::New("Uint16Array"),
                       FunctionTemplate::New(Uint16Array));
  global_template->Set(String::New("Int32Array"),
                       FunctionTemplate::New(Int32Array));
  global_template->Set(String::New("Uint32Array"),
                       FunctionTemplate::New(Uint32Array));
  global_template->Set(String::New("Float32Array"),
                       FunctionTemplate::New(Float32Array));
  global_template->Set(String::New("Float64Array"),
                       FunctionTemplate::New(Float64Array));
  global_template->Set(String::New("PixelArray"),
                       FunctionTemplate::New(PixelArray));

#ifdef LIVE_OBJECT_LIST
  global_template->Set(String::New("lol_is_enabled"), True());
#else
  global_template->Set(String::New("lol_is_enabled"), False());
#endif

#if !defined(V8_SHARED) && !defined(_WIN32) && !defined(_WIN64)
  Handle<ObjectTemplate> os_templ = ObjectTemplate::New();
  AddOSMethods(os_templ);
  global_template->Set(String::New("os"), os_templ);
#endif  // V8_SHARED

  return global_template;
}


void Shell::Initialize() {
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
  if (i::FLAG_dump_counters) {
    V8::SetCounterFunction(LookupCounter);
    V8::SetCreateHistogramFunction(CreateHistogram);
    V8::SetAddHistogramSampleFunction(AddHistogramSample);
  }
#endif  // V8_SHARED
  if (options.test_shell) return;

#ifndef V8_SHARED
  Locker lock;
  HandleScope scope;
  Handle<ObjectTemplate> global_template = CreateGlobalTemplate();
  utility_context_ = Context::New(NULL, global_template);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Start the debugger agent if requested.
  if (i::FLAG_debugger_agent) {
    v8::Debug::EnableAgent("d8 shell", i::FLAG_debugger_port, true);
    v8::Debug::SetDebugMessageDispatchHandler(DispatchDebugMessages, true);
  }
#endif  // ENABLE_DEBUGGER_SUPPORT
#endif  // V8_SHARED
}


Persistent<Context> Shell::CreateEvaluationContext() {
#ifndef V8_SHARED
  // This needs to be a critical section since this is not thread-safe
  i::ScopedLock lock(context_mutex_);
#endif  // V8_SHARED
  // Initialize the global objects
  Handle<ObjectTemplate> global_template = CreateGlobalTemplate();
  Persistent<Context> context = Context::New(NULL, global_template);
  ASSERT(!context.IsEmpty());
  Context::Scope scope(context);

#ifndef V8_SHARED
  i::JSArguments js_args = i::FLAG_js_arguments;
  i::Handle<i::FixedArray> arguments_array =
      FACTORY->NewFixedArray(js_args.argc());
  for (int j = 0; j < js_args.argc(); j++) {
    i::Handle<i::String> arg =
        FACTORY->NewStringFromUtf8(i::CStrVector(js_args[j]));
    arguments_array->set(j, *arg);
  }
  i::Handle<i::JSArray> arguments_jsarray =
      FACTORY->NewJSArrayWithElements(arguments_array);
  context->Global()->Set(String::New("arguments"),
                         Utils::ToLocal(arguments_jsarray));
#endif  // V8_SHARED
  return context;
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


int CompareKeys(const void* a, const void* b) {
  return strcmp(static_cast<const CounterAndKey*>(a)->key,
                static_cast<const CounterAndKey*>(b)->key);
}


void Shell::OnExit() {
  if (console != NULL) console->Close();
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
    qsort(counters, number_of_counters, sizeof(counters[0]), CompareKeys);
    printf("+--------------------------------------------+-------------+\n");
    printf("| Name                                       | Value       |\n");
    printf("+--------------------------------------------+-------------+\n");
    for (j = 0; j < number_of_counters; j++) {
      Counter* counter = counters[j].counter;
      const char* key = counters[j].key;
      if (counter->is_histogram()) {
        printf("| c:%-40s | %11i |\n", key, counter->count());
        printf("| t:%-40s | %11i |\n", key, counter->sample_total());
      } else {
        printf("| %-42s | %11i |\n", key, counter->count());
      }
    }
    printf("+--------------------------------------------+-------------+\n");
    delete [] counters;
  }
  if (counters_file_ != NULL)
    delete counters_file_;
}
#endif  // V8_SHARED


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
  // Release the V8 lock while reading files.
  v8::Unlocker unlocker(Isolate::GetCurrent());
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


Handle<Value> Shell::ReadBinary(const Arguments& args) {
  String::Utf8Value filename(args[0]);
  int size;
  if (*filename == NULL) {
    return ThrowException(String::New("Error loading file"));
  }
  char* chars = ReadChars(*filename, &size);
  if (chars == NULL) {
    return ThrowException(String::New("Error reading file"));
  }
  // We skip checking the string for UTF8 characters and use it raw as
  // backing store for the external string with 8-bit characters.
  BinaryResource* resource = new BinaryResource(chars, size);
  return String::NewExternal(resource);
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
Handle<String> Shell::ReadFile(const char* name) {
  int size = 0;
  char* chars = ReadChars(name, &size);
  if (chars == NULL) return Handle<String>();
  Handle<String> result = String::New(chars);
  delete[] chars;
  return result;
}


void Shell::RunShell() {
  Locker locker;
  Context::Scope context_scope(evaluation_context_);
  HandleScope outer_scope;
  Handle<String> name = String::New("(d8)");
  console = LineEditor::Get();
  printf("V8 version %s [console: %s]\n", V8::GetVersion(), console->name());
  console->Open();
  while (true) {
    HandleScope inner_scope;
    Handle<String> input = console->Prompt(Shell::kPrompt);
    if (input.IsEmpty()) break;
    ExecuteString(input, name, true, true);
  }
  printf("\n");
}


#ifndef V8_SHARED
class ShellThread : public i::Thread {
 public:
  // Takes ownership of the underlying char array of |files|.
  ShellThread(int no, char* files)
      : Thread("d8:ShellThread"),
        no_(no), files_(files) { }

  ~ShellThread() {
    delete[] files_;
  }

  virtual void Run();
 private:
  int no_;
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
    Locker locker;
    HandleScope outer_scope;
    Persistent<Context> thread_context = Shell::CreateEvaluationContext();
    Context::Scope context_scope(thread_context);

    while ((ptr != NULL) && (*ptr != '\0')) {
      HandleScope inner_scope;
      char* filename = ptr;
      ptr = ReadWord(ptr);

      // Skip empty strings.
      if (strlen(filename) == 0) {
        continue;
      }

      Handle<String> str = Shell::ReadFile(filename);
      if (str.IsEmpty()) {
        printf("File '%s' not found\n", filename);
        Shell::Exit(1);
      }

      Shell::ExecuteString(str, String::New(filename), false, false);
    }

    thread_context.Dispose();
    ptr = next_line;
  }
}
#endif  // V8_SHARED


SourceGroup::~SourceGroup() {
#ifndef V8_SHARED
  delete next_semaphore_;
  next_semaphore_ = NULL;
  delete done_semaphore_;
  done_semaphore_ = NULL;
  delete thread_;
  thread_ = NULL;
#endif  // V8_SHARED
}


void SourceGroup::Execute() {
  for (int i = begin_offset_; i < end_offset_; ++i) {
    const char* arg = argv_[i];
    if (strcmp(arg, "-e") == 0 && i + 1 < end_offset_) {
      // Execute argument given to -e option directly.
      HandleScope handle_scope;
      Handle<String> file_name = String::New("unnamed");
      Handle<String> source = String::New(argv_[i + 1]);
      if (!Shell::ExecuteString(source, file_name, false, true)) {
        Shell::Exit(1);
      }
      ++i;
    } else if (arg[0] == '-') {
      // Ignore other options. They have been parsed already.
    } else {
      // Use all other arguments as names of files to load and run.
      HandleScope handle_scope;
      Handle<String> file_name = String::New(arg);
      Handle<String> source = ReadFile(arg);
      if (source.IsEmpty()) {
        printf("Error reading '%s'\n", arg);
        Shell::Exit(1);
      }
      if (!Shell::ExecuteString(source, file_name, false, true)) {
        Shell::Exit(1);
      }
    }
  }
}


Handle<String> SourceGroup::ReadFile(const char* name) {
  int size;
  const char* chars = ReadChars(name, &size);
  if (chars == NULL) return Handle<String>();
  Handle<String> result = String::New(chars, size);
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
    if (next_semaphore_ != NULL) next_semaphore_->Wait();
    {
      Isolate::Scope iscope(isolate);
      Locker lock(isolate);
      HandleScope scope;
      Persistent<Context> context = Shell::CreateEvaluationContext();
      {
        Context::Scope cscope(context);
        Execute();
      }
      context.Dispose();
    }
    if (done_semaphore_ != NULL) done_semaphore_->Signal();
  } while (!Shell::options.last_run);
  isolate->Dispose();
}


void SourceGroup::StartExecuteInThread() {
  if (thread_ == NULL) {
    thread_ = new IsolateThread(this);
    thread_->Start();
  }
  next_semaphore_->Signal();
}


void SourceGroup::WaitForThread() {
  if (thread_ == NULL) return;
  if (Shell::options.last_run) {
    thread_->Join();
  } else {
    done_semaphore_->Wait();
  }
}
#endif  // V8_SHARED


bool Shell::SetOptions(int argc, char* argv[]) {
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--stress-opt") == 0) {
      options.stress_opt = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--stress-deopt") == 0) {
      options.stress_deopt = true;
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
    } else if (strcmp(argv[i], "--preemption") == 0) {
#ifdef V8_SHARED
      printf("D8 with shared library does not support multi-threading\n");
      return false;
#else
      options.use_preemption = true;
      argv[i] = NULL;
#endif  // V8_SHARED
    } else if (strcmp(argv[i], "--nopreemption") == 0) {
#ifdef V8_SHARED
      printf("D8 with shared library does not support multi-threading\n");
      return false;
#else
      options.use_preemption = false;
      argv[i] = NULL;
#endif  // V8_SHARED
    } else if (strcmp(argv[i], "--preemption-interval") == 0) {
#ifdef V8_SHARED
      printf("D8 with shared library does not support multi-threading\n");
      return false;
#else
      if (++i < argc) {
        argv[i-1] = NULL;
        char* end = NULL;
        options.preemption_interval = strtol(argv[i], &end, 10);  // NOLINT
        if (options.preemption_interval <= 0
            || *end != '\0'
            || errno == ERANGE) {
          printf("Invalid value for --preemption-interval '%s'\n", argv[i]);
          return false;
        }
        argv[i] = NULL;
      } else {
        printf("Missing value for --preemption-interval\n");
        return false;
      }
#endif  // V8_SHARED
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


int Shell::RunMain(int argc, char* argv[]) {
#ifndef V8_SHARED
  i::List<i::Thread*> threads(1);
  if (options.parallel_files != NULL) {
    for (int i = 0; i < options.num_parallel_files; i++) {
      char* files = NULL;
      { Locker lock(Isolate::GetCurrent());
        int size = 0;
        files = ReadChars(options.parallel_files[i], &size);
      }
      if (files == NULL) {
        printf("File list '%s' not found\n", options.parallel_files[i]);
        Exit(1);
      }
      ShellThread* thread = new ShellThread(threads.length(), files);
      thread->Start();
      threads.Add(thread);
    }
  }
  for (int i = 1; i < options.num_isolates; ++i) {
    options.isolate_sources[i].StartExecuteInThread();
  }
#endif  // V8_SHARED
  {  // NOLINT
    Locker lock;
    HandleScope scope;
    Persistent<Context> context = CreateEvaluationContext();
    if (options.last_run) {
      // Keep using the same context in the interactive shell.
      evaluation_context_ = context;
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
      // If the interactive debugger is enabled make sure to activate
      // it before running the files passed on the command line.
      if (i::FLAG_debugger) {
        InstallUtilityScript();
      }
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
    }
    {
      Context::Scope cscope(context);
      options.isolate_sources[0].Execute();
    }
    if (!options.last_run) {
      context.Dispose();
#if !defined(V8_SHARED)
      if (i::FLAG_send_idle_notification) {
        const int kLongIdlePauseInMs = 1000;
        V8::ContextDisposedNotification();
        V8::IdleNotification(kLongIdlePauseInMs);
      }
#endif  // !V8_SHARED
    }

#ifndef V8_SHARED
    // Start preemption if threads have been created and preemption is enabled.
    if (threads.length() > 0
        && options.use_preemption) {
      Locker::StartPreemption(options.preemption_interval);
    }
#endif  // V8_SHARED
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

  if (threads.length() > 0 && options.use_preemption) {
    Locker lock;
    Locker::StopPreemption();
  }
#endif  // V8_SHARED
  return 0;
}


int Shell::Main(int argc, char* argv[]) {
  if (!SetOptions(argc, argv)) return 1;
  Initialize();

  int result = 0;
  if (options.stress_opt || options.stress_deopt) {
    Testing::SetStressRunType(
        options.stress_opt ? Testing::kStressTypeOpt
                           : Testing::kStressTypeDeopt);
    int stress_runs = Testing::GetStressRuns();
    for (int i = 0; i < stress_runs && result == 0; i++) {
      printf("============ Stress %d/%d ============\n", i + 1, stress_runs);
      Testing::PrepareStressRun(i);
      options.last_run = (i == stress_runs - 1);
      result = RunMain(argc, argv);
    }
    printf("======== Full Deoptimization =======\n");
    Testing::DeoptimizeAll();
#if !defined(V8_SHARED)
  } else if (i::FLAG_stress_runs > 0) {
    int stress_runs = i::FLAG_stress_runs;
    for (int i = 0; i < stress_runs && result == 0; i++) {
      printf("============ Run %d/%d ============\n", i + 1, stress_runs);
      options.last_run = (i == stress_runs - 1);
      result = RunMain(argc, argv);
    }
#endif
  } else {
    result = RunMain(argc, argv);
  }


#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
  // Run remote debugger if requested, but never on --test
  if (i::FLAG_remote_debugger && !options.test_shell) {
    InstallUtilityScript();
    RunRemoteDebugger(i::FLAG_debugger_port);
    return 0;
  }
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT

  // Run interactive shell if explicitly requested or if no script has been
  // executed, but never on --test

  if (( options.interactive_shell
      || !options.script_executed )
      && !options.test_shell ) {
#if !defined(V8_SHARED) && defined(ENABLE_DEBUGGER_SUPPORT)
    if (!i::FLAG_debugger) {
      InstallUtilityScript();
    }
#endif  // !V8_SHARED && ENABLE_DEBUGGER_SUPPORT
    RunShell();
  }

  V8::Dispose();

#ifndef V8_SHARED
  OnExit();
#endif  // V8_SHARED

  return result;
}

}  // namespace v8


#ifndef GOOGLE3
int main(int argc, char* argv[]) {
  return v8::Shell::Main(argc, argv);
}
#endif
