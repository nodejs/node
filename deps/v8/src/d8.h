// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_H_
#define V8_D8_H_

#ifndef V8_SHARED
#include "src/allocation.h"
#include "src/hashmap.h"
#include "src/smart-pointers.h"
#include "src/v8.h"
#else
#include "include/v8.h"
#endif  // !V8_SHARED

namespace v8 {


#ifndef V8_SHARED
// A single counter in a counter collection.
class Counter {
 public:
  static const int kMaxNameSize = 64;
  int32_t* Bind(const char* name, bool histogram);
  int32_t* ptr() { return &count_; }
  int32_t count() { return count_; }
  int32_t sample_total() { return sample_total_; }
  bool is_histogram() { return is_histogram_; }
  void AddSample(int32_t sample);
 private:
  int32_t count_;
  int32_t sample_total_;
  bool is_histogram_;
  uint8_t name_[kMaxNameSize];
};


// A set of counters and associated information.  An instance of this
// class is stored directly in the memory-mapped counters file if
// the --map-counters options is used
class CounterCollection {
 public:
  CounterCollection();
  Counter* GetNextCounter();
 private:
  static const unsigned kMaxCounters = 512;
  uint32_t magic_number_;
  uint32_t max_counters_;
  uint32_t max_name_size_;
  uint32_t counters_in_use_;
  Counter counters_[kMaxCounters];
};


class CounterMap {
 public:
  CounterMap(): hash_map_(Match) { }
  Counter* Lookup(const char* name) {
    i::HashMap::Entry* answer = hash_map_.Lookup(
        const_cast<char*>(name),
        Hash(name),
        false);
    if (!answer) return NULL;
    return reinterpret_cast<Counter*>(answer->value);
  }
  void Set(const char* name, Counter* value) {
    i::HashMap::Entry* answer = hash_map_.Lookup(
        const_cast<char*>(name),
        Hash(name),
        true);
    DCHECK(answer != NULL);
    answer->value = value;
  }
  class Iterator {
   public:
    explicit Iterator(CounterMap* map)
        : map_(&map->hash_map_), entry_(map_->Start()) { }
    void Next() { entry_ = map_->Next(entry_); }
    bool More() { return entry_ != NULL; }
    const char* CurrentKey() { return static_cast<const char*>(entry_->key); }
    Counter* CurrentValue() { return static_cast<Counter*>(entry_->value); }
   private:
    i::HashMap* map_;
    i::HashMap::Entry* entry_;
  };

 private:
  static int Hash(const char* name);
  static bool Match(void* key1, void* key2);
  i::HashMap hash_map_;
};
#endif  // !V8_SHARED


class LineEditor {
 public:
  enum Type { DUMB = 0, READLINE = 1 };
  LineEditor(Type type, const char* name);
  virtual ~LineEditor() { }

  virtual Handle<String> Prompt(const char* prompt) = 0;
  virtual bool Open(Isolate* isolate) { return true; }
  virtual bool Close() { return true; }
  virtual void AddHistory(const char* str) { }

  const char* name() { return name_; }
  static LineEditor* Get() { return current_; }
 private:
  Type type_;
  const char* name_;
  static LineEditor* current_;
};


class SourceGroup {
 public:
  SourceGroup() :
#ifndef V8_SHARED
      next_semaphore_(0),
      done_semaphore_(0),
      thread_(NULL),
#endif  // !V8_SHARED
      argv_(NULL),
      begin_offset_(0),
      end_offset_(0) {}

  ~SourceGroup();

  void Begin(char** argv, int offset) {
    argv_ = const_cast<const char**>(argv);
    begin_offset_ = offset;
  }

  void End(int offset) { end_offset_ = offset; }

  void Execute(Isolate* isolate);

#ifndef V8_SHARED
  void StartExecuteInThread();
  void WaitForThread();

 private:
  class IsolateThread : public base::Thread {
   public:
    explicit IsolateThread(SourceGroup* group)
        : base::Thread(GetThreadOptions()), group_(group) {}

    virtual void Run() {
      group_->ExecuteInThread();
    }

   private:
    SourceGroup* group_;
  };

  static base::Thread::Options GetThreadOptions();
  void ExecuteInThread();

  base::Semaphore next_semaphore_;
  base::Semaphore done_semaphore_;
  base::Thread* thread_;
#endif  // !V8_SHARED

  void ExitShell(int exit_code);
  Handle<String> ReadFile(Isolate* isolate, const char* name);

  const char** argv_;
  int begin_offset_;
  int end_offset_;
};


class BinaryResource : public v8::String::ExternalAsciiStringResource {
 public:
  BinaryResource(const char* string, int length)
      : data_(string),
        length_(length) { }

  ~BinaryResource() {
    delete[] data_;
    data_ = NULL;
    length_ = 0;
  }

  virtual const char* data() const { return data_; }
  virtual size_t length() const { return length_; }

 private:
  const char* data_;
  size_t length_;
};


class ShellOptions {
 public:
  ShellOptions()
      : script_executed(false),
        last_run(true),
        send_idle_notification(false),
        invoke_weak_callbacks(false),
        stress_opt(false),
        stress_deopt(false),
        interactive_shell(false),
        test_shell(false),
        dump_heap_constants(false),
        expected_to_throw(false),
        mock_arraybuffer_allocator(false),
        num_isolates(1),
        compile_options(v8::ScriptCompiler::kNoCompileOptions),
        isolate_sources(NULL),
        icu_data_file(NULL),
        natives_blob(NULL),
        snapshot_blob(NULL) {}

  ~ShellOptions() {
    delete[] isolate_sources;
  }

  bool use_interactive_shell() {
    return (interactive_shell || !script_executed) && !test_shell;
  }

  bool script_executed;
  bool last_run;
  bool send_idle_notification;
  bool invoke_weak_callbacks;
  bool stress_opt;
  bool stress_deopt;
  bool interactive_shell;
  bool test_shell;
  bool dump_heap_constants;
  bool expected_to_throw;
  bool mock_arraybuffer_allocator;
  int num_isolates;
  v8::ScriptCompiler::CompileOptions compile_options;
  SourceGroup* isolate_sources;
  const char* icu_data_file;
  const char* natives_blob;
  const char* snapshot_blob;
};

#ifdef V8_SHARED
class Shell {
#else
class Shell : public i::AllStatic {
#endif  // V8_SHARED

 public:
  static Local<UnboundScript> CompileString(
      Isolate* isolate, Local<String> source, Local<Value> name,
      v8::ScriptCompiler::CompileOptions compile_options);
  static bool ExecuteString(Isolate* isolate,
                            Handle<String> source,
                            Handle<Value> name,
                            bool print_result,
                            bool report_exceptions);
  static const char* ToCString(const v8::String::Utf8Value& value);
  static void ReportException(Isolate* isolate, TryCatch* try_catch);
  static Handle<String> ReadFile(Isolate* isolate, const char* name);
  static Local<Context> CreateEvaluationContext(Isolate* isolate);
  static int RunMain(Isolate* isolate, int argc, char* argv[]);
  static int Main(int argc, char* argv[]);
  static void Exit(int exit_code);
  static void OnExit();

#ifndef V8_SHARED
  static Handle<Array> GetCompletions(Isolate* isolate,
                                      Handle<String> text,
                                      Handle<String> full);
  static int* LookupCounter(const char* name);
  static void* CreateHistogram(const char* name,
                               int min,
                               int max,
                               size_t buckets);
  static void AddHistogramSample(void* histogram, int sample);
  static void MapCounters(v8::Isolate* isolate, const char* name);

  static Local<Object> DebugMessageDetails(Isolate* isolate,
                                           Handle<String> message);
  static Local<Value> DebugCommandToJSONRequest(Isolate* isolate,
                                                Handle<String> command);

  static void PerformanceNow(const v8::FunctionCallbackInfo<v8::Value>& args);
#endif  // !V8_SHARED

  static void RealmCurrent(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmOwner(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmGlobal(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmCreate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmDispose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmSwitch(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmEval(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmSharedGet(Local<String> property,
                             const  PropertyCallbackInfo<Value>& info);
  static void RealmSharedSet(Local<String> property,
                             Local<Value> value,
                             const  PropertyCallbackInfo<void>& info);

  static void Print(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Quit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Version(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Read(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReadBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static Handle<String> ReadFromStdin(Isolate* isolate);
  static void ReadLine(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(ReadFromStdin(args.GetIsolate()));
  }
  static void Load(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ArrayBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Int8Array(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Uint8Array(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Int16Array(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Uint16Array(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Int32Array(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Uint32Array(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Float32Array(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Float64Array(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Uint8ClampedArray(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ArrayBufferSlice(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ArraySubArray(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ArraySet(const v8::FunctionCallbackInfo<v8::Value>& args);
  // The OS object on the global object contains methods for performing
  // operating system calls:
  //
  // os.system("program_name", ["arg1", "arg2", ...], timeout1, timeout2) will
  // run the command, passing the arguments to the program.  The standard output
  // of the program will be picked up and returned as a multiline string.  If
  // timeout1 is present then it should be a number.  -1 indicates no timeout
  // and a positive number is used as a timeout in milliseconds that limits the
  // time spent waiting between receiving output characters from the program.
  // timeout2, if present, should be a number indicating the limit in
  // milliseconds on the total running time of the program.  Exceptions are
  // thrown on timeouts or other errors or if the exit status of the program
  // indicates an error.
  //
  // os.chdir(dir) changes directory to the given directory.  Throws an
  // exception/ on error.
  //
  // os.setenv(variable, value) sets an environment variable.  Repeated calls to
  // this method leak memory due to the API of setenv in the standard C library.
  //
  // os.umask(alue) calls the umask system call and returns the old umask.
  //
  // os.mkdirp(name, mask) creates a directory.  The mask (if present) is anded
  // with the current umask.  Intermediate directories are created if necessary.
  // An exception is not thrown if the directory already exists.  Analogous to
  // the "mkdir -p" command.
  static void OSObject(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void System(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ChangeDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetEnvironment(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void UnsetEnvironment(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetUMask(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void MakeDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RemoveDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void AddOSMethods(v8::Isolate* isolate,
                           Handle<ObjectTemplate> os_template);

  static const char* kPrompt;
  static ShellOptions options;

 private:
  static Persistent<Context> evaluation_context_;
#ifndef V8_SHARED
  static Persistent<Context> utility_context_;
  static CounterMap* counter_map_;
  // We statically allocate a set of local counters to be used if we
  // don't want to store the stats in a memory-mapped file
  static CounterCollection local_counters_;
  static CounterCollection* counters_;
  static base::OS::MemoryMappedFile* counters_file_;
  static base::Mutex context_mutex_;
  static const base::TimeTicks kInitialTicks;

  static Counter* GetCounter(const char* name, bool is_histogram);
  static void InstallUtilityScript(Isolate* isolate);
#endif  // !V8_SHARED
  static void Initialize(Isolate* isolate);
  static void InitializeDebugger(Isolate* isolate);
  static void RunShell(Isolate* isolate);
  static bool SetOptions(int argc, char* argv[]);
  static Handle<ObjectTemplate> CreateGlobalTemplate(Isolate* isolate);
  static Handle<FunctionTemplate> CreateArrayBufferTemplate(FunctionCallback);
  static Handle<FunctionTemplate> CreateArrayTemplate(FunctionCallback);
  static Handle<Value> CreateExternalArrayBuffer(Isolate* isolate,
                                                 Handle<Object> buffer,
                                                 int32_t size);
  static Handle<Object> CreateExternalArray(Isolate* isolate,
                                            Handle<Object> array,
                                            Handle<Object> buffer,
                                            ExternalArrayType type,
                                            int32_t length,
                                            int32_t byteLength,
                                            int32_t byteOffset,
                                            int32_t element_size);
  static void CreateExternalArray(
      const v8::FunctionCallbackInfo<v8::Value>& args,
      ExternalArrayType type,
      int32_t element_size);
  static void ExternalArrayWeakCallback(Isolate* isolate,
                                        Persistent<Object>* object,
                                        uint8_t* data);
};


}  // namespace v8


#endif  // V8_D8_H_
