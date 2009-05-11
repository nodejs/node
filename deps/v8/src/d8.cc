// Copyright 2009 the V8 project authors. All rights reserved.
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


#include <stdlib.h>
#include <errno.h>

#include "d8.h"
#include "d8-debug.h"
#include "debug.h"
#include "api.h"
#include "natives.h"
#include "platform.h"


namespace v8 {


const char* Shell::kHistoryFileName = ".d8_history";
const char* Shell::kPrompt = "d8> ";


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
  virtual i::SmartPointer<char> Prompt(const char* prompt);
};


static DumbLineEditor dumb_line_editor;


i::SmartPointer<char> DumbLineEditor::Prompt(const char* prompt) {
  static const int kBufferSize = 256;
  char buffer[kBufferSize];
  printf("%s", prompt);
  char* str = fgets(buffer, kBufferSize, stdin);
  return i::SmartPointer<char>(str ? i::StrDup(str) : str);
}


CounterMap* Shell::counter_map_;
i::OS::MemoryMappedFile* Shell::counters_file_ = NULL;
CounterCollection Shell::local_counters_;
CounterCollection* Shell::counters_ = &local_counters_;
Persistent<Context> Shell::utility_context_;
Persistent<Context> Shell::evaluation_context_;


bool CounterMap::Match(void* key1, void* key2) {
  const char* name1 = reinterpret_cast<const char*>(key1);
  const char* name2 = reinterpret_cast<const char*>(key2);
  return strcmp(name1, name2) == 0;
}


// Converts a V8 value to a C string.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}


// Executes a string within the current v8 context.
bool Shell::ExecuteString(Handle<String> source,
                          Handle<Value> name,
                          bool print_result,
                          bool report_exceptions) {
  HandleScope handle_scope;
  TryCatch try_catch;
  if (i::FLAG_debugger) {
    // When debugging make exceptions appear to be uncaught.
    try_catch.SetVerbose(true);
  }
  Handle<Script> script = Script::Compile(source, name);
  if (script.IsEmpty()) {
    // Print errors that happened during compilation.
    if (report_exceptions && !i::FLAG_debugger)
      ReportException(&try_catch);
    return false;
  } else {
    Handle<Value> result = script->Run();
    if (result.IsEmpty()) {
      // Print errors that happened during execution.
      if (report_exceptions && !i::FLAG_debugger)
        ReportException(&try_catch);
      return false;
    } else {
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


Handle<Value> Shell::Print(const Arguments& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope;
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
  return Undefined();
}


Handle<Value> Shell::Read(const Arguments& args) {
  if (args.Length() != 1) {
    return ThrowException(String::New("Bad parameters"));
  }
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
    if (!ExecuteString(source, String::New(*file), false, false)) {
      return ThrowException(String::New("Error executing  file"));
    }
  }
  return Undefined();
}


Handle<Value> Shell::Yield(const Arguments& args) {
  v8::Unlocker unlocker;
  return Undefined();
}


Handle<Value> Shell::Quit(const Arguments& args) {
  int exit_code = args[0]->Int32Value();
  OnExit();
  exit(exit_code);
  return Undefined();
}


Handle<Value> Shell::Version(const Arguments& args) {
  return String::New(V8::GetVersion());
}


void Shell::ReportException(v8::TryCatch* try_catch) {
  HandleScope handle_scope;
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
  }
}


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
#endif


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
  counters_file_ = i::OS::MemoryMappedFile::create(name,
    sizeof(CounterCollection), &local_counters_);
  void* memory = (counters_file_ == NULL) ?
      NULL : counters_file_->memory();
  if (memory == NULL) {
    printf("Could not map counters file %s\n", name);
    exit(1);
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


void Shell::Initialize() {
  Shell::counter_map_ = new CounterMap();
  // Set up counters
  if (i::FLAG_map_counters != NULL)
    MapCounters(i::FLAG_map_counters);
  if (i::FLAG_dump_counters) {
    V8::SetCounterFunction(LookupCounter);
    V8::SetCreateHistogramFunction(CreateHistogram);
    V8::SetAddHistogramSampleFunction(AddHistogramSample);
  }

  // Initialize the global objects
  HandleScope scope;
  Handle<ObjectTemplate> global_template = ObjectTemplate::New();
  global_template->Set(String::New("print"), FunctionTemplate::New(Print));
  global_template->Set(String::New("read"), FunctionTemplate::New(Read));
  global_template->Set(String::New("load"), FunctionTemplate::New(Load));
  global_template->Set(String::New("quit"), FunctionTemplate::New(Quit));
  global_template->Set(String::New("version"), FunctionTemplate::New(Version));

  Handle<ObjectTemplate> os_templ = ObjectTemplate::New();
  AddOSMethods(os_templ);
  global_template->Set(String::New("os"), os_templ);

  utility_context_ = Context::New(NULL, global_template);
  utility_context_->SetSecurityToken(Undefined());
  Context::Scope utility_scope(utility_context_);

  i::JSArguments js_args = i::FLAG_js_arguments;
  i::Handle<i::FixedArray> arguments_array =
      i::Factory::NewFixedArray(js_args.argc());
  for (int j = 0; j < js_args.argc(); j++) {
    i::Handle<i::String> arg =
        i::Factory::NewStringFromUtf8(i::CStrVector(js_args[j]));
    arguments_array->set(j, *arg);
  }
  i::Handle<i::JSArray> arguments_jsarray =
      i::Factory::NewJSArrayWithElements(arguments_array);
  global_template->Set(String::New("arguments"),
                       Utils::ToLocal(arguments_jsarray));

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Install the debugger object in the utility scope
  i::Debug::Load();
  i::JSObject* debug = i::Debug::debug_context()->global();
  utility_context_->Global()->Set(String::New("$debug"),
                                  Utils::ToLocal(&debug));
#endif

  // Run the d8 shell utility script in the utility context
  int source_index = i::NativesCollection<i::D8>::GetIndex("d8");
  i::Vector<const char> shell_source
      = i::NativesCollection<i::D8>::GetScriptSource(source_index);
  i::Vector<const char> shell_source_name
      = i::NativesCollection<i::D8>::GetScriptName(source_index);
  Handle<String> source = String::New(shell_source.start(),
                                      shell_source.length());
  Handle<String> name = String::New(shell_source_name.start(),
                                    shell_source_name.length());
  Handle<Script> script = Script::Compile(source, name);
  script->Run();

  // Mark the d8 shell script as native to avoid it showing up as normal source
  // in the debugger.
  i::Handle<i::JSFunction> script_fun = Utils::OpenHandle(*script);
  i::Handle<i::Script> script_object =
      i::Handle<i::Script>(i::Script::cast(script_fun->shared()->script()));
  script_object->set_type(i::Smi::FromInt(i::SCRIPT_TYPE_NATIVE));

  // Create the evaluation context
  evaluation_context_ = Context::New(NULL, global_template);
  evaluation_context_->SetSecurityToken(Undefined());

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Set the security token of the debug context to allow access.
  i::Debug::debug_context()->set_security_token(i::Heap::undefined_value());
#endif
}


void Shell::OnExit() {
  if (i::FLAG_dump_counters) {
    ::printf("+----------------------------------------+-------------+\n");
    ::printf("| Name                                   | Value       |\n");
    ::printf("+----------------------------------------+-------------+\n");
    for (CounterMap::Iterator i(counter_map_); i.More(); i.Next()) {
      Counter* counter = i.CurrentValue();
      if (counter->is_histogram()) {
        ::printf("| c:%-36s | %11i |\n", i.CurrentKey(), counter->count());
        ::printf("| t:%-36s | %11i |\n",
                 i.CurrentKey(),
                 counter->sample_total());
      } else {
        ::printf("| %-38s | %11i |\n", i.CurrentKey(), counter->count());
      }
    }
    ::printf("+----------------------------------------+-------------+\n");
  }
  if (counters_file_ != NULL)
    delete counters_file_;
}


static char* ReadChars(const char *name, int* size_out) {
  v8::Unlocker unlocker;  // Release the V8 lock while reading files.
  FILE* file = i::OS::FOpen(name, "rb");
  if (file == NULL) return NULL;

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
  *size_out = size;
  return chars;
}


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
  LineEditor* editor = LineEditor::Get();
  printf("V8 version %s [console: %s]\n", V8::GetVersion(), editor->name());
  editor->Open();
  while (true) {
    Locker locker;
    HandleScope handle_scope;
    Context::Scope context_scope(evaluation_context_);
    i::SmartPointer<char> input = editor->Prompt(Shell::kPrompt);
    if (input.is_empty())
      break;
    editor->AddHistory(*input);
    Handle<String> name = String::New("(d8)");
    ExecuteString(String::New(*input), name, true, true);
  }
  editor->Close();
  printf("\n");
}


class ShellThread : public i::Thread {
 public:
  ShellThread(int no, i::Vector<const char> files)
    : no_(no), files_(files) { }
  virtual void Run();
 private:
  int no_;
  i::Vector<const char> files_;
};


void ShellThread::Run() {
  // Prepare the context for this thread.
  Locker locker;
  HandleScope scope;
  Handle<ObjectTemplate> global_template = ObjectTemplate::New();
  global_template->Set(String::New("print"),
                       FunctionTemplate::New(Shell::Print));
  global_template->Set(String::New("read"),
                       FunctionTemplate::New(Shell::Read));
  global_template->Set(String::New("load"),
                       FunctionTemplate::New(Shell::Load));
  global_template->Set(String::New("yield"),
                       FunctionTemplate::New(Shell::Yield));
  global_template->Set(String::New("version"),
                       FunctionTemplate::New(Shell::Version));

  char* ptr = const_cast<char*>(files_.start());
  while ((ptr != NULL) && (*ptr != '\0')) {
    // For each newline-separated line.
    char* next_line = ReadLine(ptr);

    if (*ptr == '#') {
      // Skip comment lines.
      ptr = next_line;
      continue;
    }

    Persistent<Context> thread_context = Context::New(NULL, global_template);
    thread_context->SetSecurityToken(Undefined());
    Context::Scope context_scope(thread_context);

    while ((ptr != NULL) && (*ptr != '\0')) {
      char* filename = ptr;
      ptr = ReadWord(ptr);

      // Skip empty strings.
      if (strlen(filename) == 0) {
        break;
      }

      Handle<String> str = Shell::ReadFile(filename);
      if (str.IsEmpty()) {
        printf("WARNING: %s not found\n", filename);
        break;
      }

      Shell::ExecuteString(str, String::New(filename), false, false);
    }

    thread_context.Dispose();
    ptr = next_line;
  }
}


int Shell::Main(int argc, char* argv[]) {
  i::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (i::FLAG_help) {
    return 1;
  }
  Initialize();
  bool run_shell = (argc == 1);

  // Default use preemption if threads are created.
  bool use_preemption = true;

  // Default to use lowest possible thread preemption interval to test as many
  // edgecases as possible.
  int preemption_interval = 1;

  i::List<i::Thread*> threads(1);

  {
    // Acquire the V8 lock once initialization has finished. Since the thread
    // below may spawn new threads accessing V8 holding the V8 lock here is
    // mandatory.
    Locker locker;
    Context::Scope context_scope(evaluation_context_);
    for (int i = 1; i < argc; i++) {
      char* str = argv[i];
      if (strcmp(str, "--shell") == 0) {
        run_shell = true;
      } else if (strcmp(str, "--preemption") == 0) {
        use_preemption = true;
      } else if (strcmp(str, "--no-preemption") == 0) {
        use_preemption = false;
      } else if (strcmp(str, "--preemption-interval") == 0) {
        if (i + 1 < argc) {
          char *end = NULL;
          preemption_interval = strtol(argv[++i], &end, 10);  // NOLINT
          if (preemption_interval <= 0 || *end != '\0' || errno == ERANGE) {
            printf("Invalid value for --preemption-interval '%s'\n", argv[i]);
            return 1;
          }
        } else {
          printf("Missing value for --preemption-interval\n");
          return 1;
       }
      } else if (strcmp(str, "-f") == 0) {
        // Ignore any -f flags for compatibility with other stand-alone
        // JavaScript engines.
        continue;
      } else if (strncmp(str, "--", 2) == 0) {
        printf("Warning: unknown flag %s.\nTry --help for options\n", str);
      } else if (strcmp(str, "-e") == 0 && i + 1 < argc) {
        // Execute argument given to -e option directly.
        v8::HandleScope handle_scope;
        v8::Handle<v8::String> file_name = v8::String::New("unnamed");
        v8::Handle<v8::String> source = v8::String::New(argv[i + 1]);
        if (!ExecuteString(source, file_name, false, true)) {
          OnExit();
          return 1;
        }
        i++;
      } else if (strcmp(str, "-p") == 0 && i + 1 < argc) {
        int size = 0;
        const char *files = ReadChars(argv[++i], &size);
        if (files == NULL) return 1;
        ShellThread *thread =
            new ShellThread(threads.length(),
                            i::Vector<const char>(files, size));
        thread->Start();
        threads.Add(thread);
      } else {
        // Use all other arguments as names of files to load and run.
        HandleScope handle_scope;
        Handle<String> file_name = v8::String::New(str);
        Handle<String> source = ReadFile(str);
        if (source.IsEmpty()) {
          printf("Error reading '%s'\n", str);
          return 1;
        }
        if (!ExecuteString(source, file_name, false, true)) {
          OnExit();
          return 1;
        }
      }
    }

    // Start preemption if threads have been created and preemption is enabled.
    if (threads.length() > 0 && use_preemption) {
      Locker::StartPreemption(preemption_interval);
    }

#ifdef ENABLE_DEBUGGER_SUPPORT
    // Run the remote debugger if requested.
    if (i::FLAG_remote_debugger) {
      RunRemoteDebugger(i::FLAG_debugger_port);
      return 0;
    }

    // Start the debugger agent if requested.
    if (i::FLAG_debugger_agent) {
      v8::Debug::EnableAgent("d8 shell", i::FLAG_debugger_port);
    }

    // Start the in-process debugger if requested.
    if (i::FLAG_debugger && !i::FLAG_debugger_agent) {
      v8::Debug::SetDebugEventListener(HandleDebugEvent);
    }
#endif
  }
  if (run_shell)
    RunShell();
  for (int i = 0; i < threads.length(); i++) {
    i::Thread *thread = threads[i];
    thread->Join();
    delete thread;
  }
  OnExit();
  return 0;
}


}  // namespace v8


int main(int argc, char* argv[]) {
  return v8::Shell::Main(argc, argv);
}
