// Copyright 2011 the V8 project authors. All rights reserved.
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

#include <v8.h>
#include <v8-testing.h>
#include <assert.h>
#ifdef COMPRESS_STARTUP_DATA_BZ2
#include <bzlib.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// When building with V8 in a shared library we cannot use functions which
// is not explicitly a part of the public V8 API. This extensive use of
// #ifndef USING_V8_SHARED/#endif is a hack until we can resolve whether to
// still use the shell sample for testing or change to use the developer
// shell d8 TODO(1272).
#if !(defined(USING_V8_SHARED) || defined(V8_SHARED))
#include "../src/v8.h"
#endif  // USING_V8_SHARED

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>  // NOLINT
#endif

static void ExitShell(int exit_code) {
  // Use _exit instead of exit to avoid races between isolate
  // threads and static destructors.
  fflush(stdout);
  fflush(stderr);
  _exit(exit_code);
}

v8::Persistent<v8::Context> CreateShellContext();
void RunShell(v8::Handle<v8::Context> context);
bool ExecuteString(v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   bool print_result,
                   bool report_exceptions);
v8::Handle<v8::Value> Print(const v8::Arguments& args);
v8::Handle<v8::Value> Read(const v8::Arguments& args);
v8::Handle<v8::Value> Load(const v8::Arguments& args);
v8::Handle<v8::Value> Quit(const v8::Arguments& args);
v8::Handle<v8::Value> Version(const v8::Arguments& args);
v8::Handle<v8::Value> Int8Array(const v8::Arguments& args);
v8::Handle<v8::Value> Uint8Array(const v8::Arguments& args);
v8::Handle<v8::Value> Int16Array(const v8::Arguments& args);
v8::Handle<v8::Value> Uint16Array(const v8::Arguments& args);
v8::Handle<v8::Value> Int32Array(const v8::Arguments& args);
v8::Handle<v8::Value> Uint32Array(const v8::Arguments& args);
v8::Handle<v8::Value> Float32Array(const v8::Arguments& args);
v8::Handle<v8::Value> Float64Array(const v8::Arguments& args);
v8::Handle<v8::Value> PixelArray(const v8::Arguments& args);
v8::Handle<v8::String> ReadFile(const char* name);
void ReportException(v8::TryCatch* handler);


static bool last_run = true;

class SourceGroup {
 public:
  SourceGroup() :
#if !(defined(USING_V8_SHARED) || defined(V8_SHARED))
                  next_semaphore_(v8::internal::OS::CreateSemaphore(0)),
                  done_semaphore_(v8::internal::OS::CreateSemaphore(0)),
                  thread_(NULL),
#endif  // USING_V8_SHARED
                  argv_(NULL),
                  begin_offset_(0),
                  end_offset_(0) { }

  void Begin(char** argv, int offset) {
    argv_ = const_cast<const char**>(argv);
    begin_offset_ = offset;
  }

  void End(int offset) { end_offset_ = offset; }

  void Execute() {
    for (int i = begin_offset_; i < end_offset_; ++i) {
      const char* arg = argv_[i];
      if (strcmp(arg, "-e") == 0 && i + 1 < end_offset_) {
        // Execute argument given to -e option directly.
        v8::HandleScope handle_scope;
        v8::Handle<v8::String> file_name = v8::String::New("unnamed");
        v8::Handle<v8::String> source = v8::String::New(argv_[i + 1]);
        if (!ExecuteString(source, file_name, false, true)) {
          ExitShell(1);
          return;
        }
        ++i;
      } else if (arg[0] == '-') {
        // Ignore other options. They have been parsed already.
      } else {
        // Use all other arguments as names of files to load and run.
        v8::HandleScope handle_scope;
        v8::Handle<v8::String> file_name = v8::String::New(arg);
        v8::Handle<v8::String> source = ReadFile(arg);
        if (source.IsEmpty()) {
          printf("Error reading '%s'\n", arg);
          continue;
        }
        if (!ExecuteString(source, file_name, false, true)) {
          ExitShell(1);
          return;
        }
      }
    }
  }

#if !(defined(USING_V8_SHARED) || defined(V8_SHARED))
  void StartExecuteInThread() {
    if (thread_ == NULL) {
      thread_ = new IsolateThread(this);
      thread_->Start();
    }
    next_semaphore_->Signal();
  }

  void WaitForThread() {
    if (thread_ == NULL) return;
    if (last_run) {
      thread_->Join();
      thread_ = NULL;
    } else {
      done_semaphore_->Wait();
    }
  }
#endif  // USING_V8_SHARED

 private:
#if !(defined(USING_V8_SHARED) || defined(V8_SHARED))
  static v8::internal::Thread::Options GetThreadOptions() {
    v8::internal::Thread::Options options;
    options.name = "IsolateThread";
    // On some systems (OSX 10.6) the stack size default is 0.5Mb or less
    // which is not enough to parse the big literal expressions used in tests.
    // The stack size should be at least StackGuard::kLimitSize + some
    // OS-specific padding for thread startup code.
    options.stack_size = 2 << 20;  // 2 Mb seems to be enough
    return options;
  }

  class IsolateThread : public v8::internal::Thread {
   public:
    explicit IsolateThread(SourceGroup* group)
        : v8::internal::Thread(GetThreadOptions()), group_(group) {}

    virtual void Run() {
      group_->ExecuteInThread();
    }

   private:
    SourceGroup* group_;
  };

  void ExecuteInThread() {
    v8::Isolate* isolate = v8::Isolate::New();
    do {
      if (next_semaphore_ != NULL) next_semaphore_->Wait();
      {
        v8::Isolate::Scope iscope(isolate);
        v8::HandleScope scope;
        v8::Persistent<v8::Context> context = CreateShellContext();
        {
          v8::Context::Scope cscope(context);
          Execute();
        }
        context.Dispose();
      }
      if (done_semaphore_ != NULL) done_semaphore_->Signal();
    } while (!last_run);
    isolate->Dispose();
  }

  v8::internal::Semaphore* next_semaphore_;
  v8::internal::Semaphore* done_semaphore_;
  v8::internal::Thread* thread_;
#endif  // USING_V8_SHARED

  const char** argv_;
  int begin_offset_;
  int end_offset_;
};


static SourceGroup* isolate_sources = NULL;


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


int RunMain(int argc, char* argv[]) {
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::HandleScope handle_scope;
  v8::Persistent<v8::Context> context = CreateShellContext();
  // Enter the newly created execution environment.
  context->Enter();
  if (context.IsEmpty()) {
    printf("Error creating context\n");
    return 1;
  }

  bool run_shell = (argc == 1);
  int num_isolates = 1;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--isolate") == 0) {
#if !(defined(USING_V8_SHARED) || defined(V8_SHARED))
      ++num_isolates;
#else  // USING_V8_SHARED
      printf("Error: --isolate not supported when linked with shared "
             "library\n");
      ExitShell(1);
#endif  // USING_V8_SHARED
    }
  }
  if (isolate_sources == NULL) {
    isolate_sources = new SourceGroup[num_isolates];
    SourceGroup* current = isolate_sources;
    current->Begin(argv, 1);
    for (int i = 1; i < argc; i++) {
      const char* str = argv[i];
      if (strcmp(str, "--isolate") == 0) {
        current->End(i);
        current++;
        current->Begin(argv, i + 1);
      } else if (strcmp(str, "--shell") == 0) {
        run_shell = true;
      } else if (strcmp(str, "-f") == 0) {
        // Ignore any -f flags for compatibility with the other stand-
        // alone JavaScript engines.
        continue;
      } else if (strncmp(str, "--", 2) == 0) {
        printf("Warning: unknown flag %s.\nTry --help for options\n", str);
      }
    }
    current->End(argc);
  }
#if !(defined(USING_V8_SHARED) || defined(V8_SHARED))
  for (int i = 1; i < num_isolates; ++i) {
    isolate_sources[i].StartExecuteInThread();
  }
#endif  // USING_V8_SHARED
  isolate_sources[0].Execute();
  if (run_shell) RunShell(context);
#if !(defined(USING_V8_SHARED) || defined(V8_SHARED))
  for (int i = 1; i < num_isolates; ++i) {
    isolate_sources[i].WaitForThread();
  }
#endif  // USING_V8_SHARED
  if (last_run) {
    delete[] isolate_sources;
    isolate_sources = NULL;
  }
  context->Exit();
  context.Dispose();
  return 0;
}


int main(int argc, char* argv[]) {
  // Figure out if we're requested to stress the optimization
  // infrastructure by running tests multiple times and forcing
  // optimization in the last run.
  bool FLAG_stress_opt = false;
  bool FLAG_stress_deopt = false;
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--stress-opt") == 0) {
      FLAG_stress_opt = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--stress-deopt") == 0) {
      FLAG_stress_deopt = true;
      argv[i] = NULL;
    } else if (strcmp(argv[i], "--noalways-opt") == 0) {
      // No support for stressing if we can't use --always-opt.
      FLAG_stress_opt = false;
      FLAG_stress_deopt = false;
      break;
    }
  }

#ifdef COMPRESS_STARTUP_DATA_BZ2
  BZip2Decompressor startup_data_decompressor;
  int bz2_result = startup_data_decompressor.Decompress();
  if (bz2_result != BZ_OK) {
    fprintf(stderr, "bzip error code: %d\n", bz2_result);
    exit(1);
  }
#endif

  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  int result = 0;
  if (FLAG_stress_opt || FLAG_stress_deopt) {
    v8::Testing::SetStressRunType(FLAG_stress_opt
                                  ? v8::Testing::kStressTypeOpt
                                  : v8::Testing::kStressTypeDeopt);
    int stress_runs = v8::Testing::GetStressRuns();
    for (int i = 0; i < stress_runs && result == 0; i++) {
      printf("============ Stress %d/%d ============\n",
             i + 1, stress_runs);
      v8::Testing::PrepareStressRun(i);
      last_run = (i == stress_runs - 1);
      result = RunMain(argc, argv);
    }
    printf("======== Full Deoptimization =======\n");
    v8::Testing::DeoptimizeAll();
  } else {
    result = RunMain(argc, argv);
  }
  v8::V8::Dispose();

  return result;
}


// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}


// Creates a new execution environment containing the built-in
// functions.
v8::Persistent<v8::Context> CreateShellContext() {
  // Create a template for the global object.
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
  // Bind the global 'print' function to the C++ Print callback.
  global->Set(v8::String::New("print"), v8::FunctionTemplate::New(Print));
  // Bind the global 'read' function to the C++ Read callback.
  global->Set(v8::String::New("read"), v8::FunctionTemplate::New(Read));
  // Bind the global 'load' function to the C++ Load callback.
  global->Set(v8::String::New("load"), v8::FunctionTemplate::New(Load));
  // Bind the 'quit' function
  global->Set(v8::String::New("quit"), v8::FunctionTemplate::New(Quit));
  // Bind the 'version' function
  global->Set(v8::String::New("version"), v8::FunctionTemplate::New(Version));

  // Bind the handlers for external arrays.
  global->Set(v8::String::New("Int8Array"),
              v8::FunctionTemplate::New(Int8Array));
  global->Set(v8::String::New("Uint8Array"),
              v8::FunctionTemplate::New(Uint8Array));
  global->Set(v8::String::New("Int16Array"),
              v8::FunctionTemplate::New(Int16Array));
  global->Set(v8::String::New("Uint16Array"),
              v8::FunctionTemplate::New(Uint16Array));
  global->Set(v8::String::New("Int32Array"),
              v8::FunctionTemplate::New(Int32Array));
  global->Set(v8::String::New("Uint32Array"),
              v8::FunctionTemplate::New(Uint32Array));
  global->Set(v8::String::New("Float32Array"),
              v8::FunctionTemplate::New(Float32Array));
  global->Set(v8::String::New("Float64Array"),
              v8::FunctionTemplate::New(Float64Array));
  global->Set(v8::String::New("PixelArray"),
              v8::FunctionTemplate::New(PixelArray));

  return v8::Context::New(NULL, global);
}


// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
v8::Handle<v8::Value> Print(const v8::Arguments& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
    if (first) {
      first = false;
    } else {
      printf(" ");
    }
    v8::String::Utf8Value str(args[i]);
    const char* cstr = ToCString(str);
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
  return v8::Undefined();
}


// The callback that is invoked by v8 whenever the JavaScript 'read'
// function is called.  This function loads the content of the file named in
// the argument into a JavaScript string.
v8::Handle<v8::Value> Read(const v8::Arguments& args) {
  if (args.Length() != 1) {
    return v8::ThrowException(v8::String::New("Bad parameters"));
  }
  v8::String::Utf8Value file(args[0]);
  if (*file == NULL) {
    return v8::ThrowException(v8::String::New("Error loading file"));
  }
  v8::Handle<v8::String> source = ReadFile(*file);
  if (source.IsEmpty()) {
    return v8::ThrowException(v8::String::New("Error loading file"));
  }
  return source;
}


// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
v8::Handle<v8::Value> Load(const v8::Arguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
    v8::String::Utf8Value file(args[i]);
    if (*file == NULL) {
      return v8::ThrowException(v8::String::New("Error loading file"));
    }
    v8::Handle<v8::String> source = ReadFile(*file);
    if (source.IsEmpty()) {
      return v8::ThrowException(v8::String::New("Error loading file"));
    }
    if (!ExecuteString(source, v8::String::New(*file), false, false)) {
      return v8::ThrowException(v8::String::New("Error executing file"));
    }
  }
  return v8::Undefined();
}


// The callback that is invoked by v8 whenever the JavaScript 'quit'
// function is called.  Quits.
v8::Handle<v8::Value> Quit(const v8::Arguments& args) {
  // If not arguments are given args[0] will yield undefined which
  // converts to the integer value 0.
  int exit_code = args[0]->Int32Value();
  ExitShell(exit_code);
  return v8::Undefined();
}


v8::Handle<v8::Value> Version(const v8::Arguments& args) {
  return v8::String::New(v8::V8::GetVersion());
}


void ExternalArrayWeakCallback(v8::Persistent<v8::Value> object, void* data) {
  free(data);
  object.Dispose();
}


v8::Handle<v8::Value> CreateExternalArray(const v8::Arguments& args,
                                          v8::ExternalArrayType type,
                                          size_t element_size) {
  assert(element_size == 1 ||
         element_size == 2 ||
         element_size == 4 ||
         element_size == 8);
  if (args.Length() != 1) {
    return v8::ThrowException(
        v8::String::New("Array constructor needs one parameter."));
  }
  static const int kMaxLength = 0x3fffffff;
  size_t length = 0;
  if (args[0]->IsUint32()) {
    length = args[0]->Uint32Value();
  } else if (args[0]->IsNumber()) {
    double raw_length = args[0]->NumberValue();
    if (raw_length < 0) {
      return v8::ThrowException(
          v8::String::New("Array length must not be negative."));
    }
    if (raw_length > kMaxLength) {
      return v8::ThrowException(
          v8::String::New("Array length exceeds maximum length."));
    }
    length = static_cast<size_t>(raw_length);
  } else {
    return v8::ThrowException(
        v8::String::New("Array length must be a number."));
  }
  if (length > static_cast<size_t>(kMaxLength)) {
    return v8::ThrowException(
        v8::String::New("Array length exceeds maximum length."));
  }
  void* data = calloc(length, element_size);
  if (data == NULL) {
    return v8::ThrowException(v8::String::New("Memory allocation failed."));
  }
  v8::Handle<v8::Object> array = v8::Object::New();
  v8::Persistent<v8::Object> persistent_array =
      v8::Persistent<v8::Object>::New(array);
  persistent_array.MakeWeak(data, ExternalArrayWeakCallback);
  persistent_array.MarkIndependent();
  array->SetIndexedPropertiesToExternalArrayData(data, type, length);
  array->Set(v8::String::New("length"), v8::Int32::New(length),
             v8::ReadOnly);
  array->Set(v8::String::New("BYTES_PER_ELEMENT"),
             v8::Int32::New(element_size));
  return array;
}


v8::Handle<v8::Value> Int8Array(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalByteArray, sizeof(int8_t));
}


v8::Handle<v8::Value> Uint8Array(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalUnsignedByteArray,
                             sizeof(uint8_t));
}


v8::Handle<v8::Value> Int16Array(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalShortArray, sizeof(int16_t));
}


v8::Handle<v8::Value> Uint16Array(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalUnsignedShortArray,
                             sizeof(uint16_t));
}

v8::Handle<v8::Value> Int32Array(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalIntArray, sizeof(int32_t));
}


v8::Handle<v8::Value> Uint32Array(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalUnsignedIntArray,
                             sizeof(uint32_t));
}


v8::Handle<v8::Value> Float32Array(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalFloatArray,
                             sizeof(float));  // NOLINT
}


v8::Handle<v8::Value> Float64Array(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalDoubleArray,
                             sizeof(double));  // NOLINT
}


v8::Handle<v8::Value> PixelArray(const v8::Arguments& args) {
  return CreateExternalArray(args, v8::kExternalPixelArray, sizeof(uint8_t));
}


// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL) return v8::Handle<v8::String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);
  v8::Handle<v8::String> result = v8::String::New(chars, size);
  delete[] chars;
  return result;
}


// The read-eval-execute loop of the shell.
void RunShell(v8::Handle<v8::Context> context) {
  printf("V8 version %s\n", v8::V8::GetVersion());
  static const int kBufferSize = 256;
  // Enter the execution environment before evaluating any code.
  v8::Context::Scope context_scope(context);
  while (true) {
    char buffer[kBufferSize];
    printf("> ");
    char* str = fgets(buffer, kBufferSize, stdin);
    if (str == NULL) break;
    v8::HandleScope handle_scope;
    ExecuteString(v8::String::New(str),
                  v8::String::New("(shell)"),
                  true,
                  true);
  }
  printf("\n");
}


// Executes a string within the current v8 context.
bool ExecuteString(v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   bool print_result,
                   bool report_exceptions) {
  v8::HandleScope handle_scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
  if (script.IsEmpty()) {
    // Print errors that happened during compilation.
    if (report_exceptions)
      ReportException(&try_catch);
    return false;
  } else {
    v8::Handle<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
      assert(try_catch.HasCaught());
      // Print errors that happened during execution.
      if (report_exceptions)
        ReportException(&try_catch);
      return false;
    } else {
      assert(!try_catch.HasCaught());
      if (print_result && !result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        v8::String::Utf8Value str(result);
        const char* cstr = ToCString(str);
        printf("%s\n", cstr);
      }
      return true;
    }
  }
}


void ReportException(v8::TryCatch* try_catch) {
  v8::HandleScope handle_scope;
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  v8::Handle<v8::Message> message = try_catch->Message();
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
}
