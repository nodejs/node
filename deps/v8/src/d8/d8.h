// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_D8_H_
#define V8_D8_D8_H_

#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "include/v8-array-buffer.h"
#include "include/v8-isolate.h"
#include "include/v8-script.h"
#include "src/base/once.h"
#include "src/base/platform/time.h"
#include "src/base/platform/wrappers.h"
#include "src/d8/async-hooks-wrapper.h"
#include "src/strings/string-hasher.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"

namespace v8 {

class BackingStore;
class CompiledWasmModule;
class D8Console;
class Message;
class TryCatch;

enum class ModuleType { kJavaScript, kJSON, kInvalid };

namespace internal {
class CancelableTaskManager;
}  // namespace internal

struct DynamicImportData;

// A single counter in a counter collection.
class Counter {
 public:
  static const int kMaxNameSize = 64;
  void Bind(const char* name, bool histogram);
  // TODO(12482): Return pointer to an atomic.
  int* ptr() {
    STATIC_ASSERT(sizeof(int) == sizeof(count_));
    return reinterpret_cast<int*>(&count_);
  }
  int count() const { return count_.load(std::memory_order_relaxed); }
  int sample_total() const {
    return sample_total_.load(std::memory_order_relaxed);
  }
  bool is_histogram() const { return is_histogram_; }
  void AddSample(int32_t sample);

 private:
  std::atomic<int> count_;
  std::atomic<int> sample_total_;
  bool is_histogram_;
  char name_[kMaxNameSize];
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

using CounterMap = std::unordered_map<std::string, Counter*>;

class SourceGroup {
 public:
  SourceGroup()
      : next_semaphore_(0),
        done_semaphore_(0),
        thread_(nullptr),
        argv_(nullptr),
        begin_offset_(0),
        end_offset_(0) {}

  ~SourceGroup();

  void Begin(char** argv, int offset) {
    argv_ = const_cast<const char**>(argv);
    begin_offset_ = offset;
  }

  void End(int offset) { end_offset_ = offset; }

  // Returns true on success, false if an uncaught exception was thrown.
  bool Execute(Isolate* isolate);

  void StartExecuteInThread();
  void WaitForThread();
  void JoinThread();

 private:
  class IsolateThread : public base::Thread {
   public:
    explicit IsolateThread(SourceGroup* group);

    void Run() override { group_->ExecuteInThread(); }

   private:
    SourceGroup* group_;
  };

  void ExecuteInThread();

  base::Semaphore next_semaphore_;
  base::Semaphore done_semaphore_;
  base::Thread* thread_;

  void ExitShell(int exit_code);

  const char** argv_;
  int begin_offset_;
  int end_offset_;
};

class SerializationData {
 public:
  SerializationData() = default;
  SerializationData(const SerializationData&) = delete;
  SerializationData& operator=(const SerializationData&) = delete;

  uint8_t* data() { return data_.get(); }
  size_t size() { return size_; }
  const std::vector<std::shared_ptr<v8::BackingStore>>& backing_stores() {
    return backing_stores_;
  }
  const std::vector<std::shared_ptr<v8::BackingStore>>& sab_backing_stores() {
    return sab_backing_stores_;
  }
  const std::vector<CompiledWasmModule>& compiled_wasm_modules() {
    return compiled_wasm_modules_;
  }
  const std::vector<v8::Global<v8::Value>>& shared_values() {
    return shared_values_;
  }

  void ClearSharedValuesUnderLockIfNeeded();

 private:
  struct DataDeleter {
    void operator()(uint8_t* p) const { base::Free(p); }
  };

  std::unique_ptr<uint8_t, DataDeleter> data_;
  size_t size_ = 0;
  std::vector<std::shared_ptr<v8::BackingStore>> backing_stores_;
  std::vector<std::shared_ptr<v8::BackingStore>> sab_backing_stores_;
  std::vector<CompiledWasmModule> compiled_wasm_modules_;
  std::vector<v8::Global<v8::Value>> shared_values_;

 private:
  friend class Serializer;
};

class SerializationDataQueue {
 public:
  void Enqueue(std::unique_ptr<SerializationData> data);
  bool Dequeue(std::unique_ptr<SerializationData>* data);
  bool IsEmpty();
  void Clear();

 private:
  base::Mutex mutex_;
  std::vector<std::unique_ptr<SerializationData>> data_;
};

class Worker : public std::enable_shared_from_this<Worker> {
 public:
  explicit Worker(const char* script);
  ~Worker();

  // Post a message to the worker. The worker will take ownership of the
  // SerializationData. This function should only be called by the thread that
  // created the Worker.
  void PostMessage(std::unique_ptr<SerializationData> data);
  // Synchronously retrieve messages from the worker's outgoing message queue.
  // If there is no message in the queue, block until a message is available.
  // If there are no messages in the queue and the worker is no longer running,
  // return nullptr.
  // This function should only be called by the thread that created the Worker.
  std::unique_ptr<SerializationData> GetMessage();
  // Terminate the worker's event loop. Messages from the worker that have been
  // queued can still be read via GetMessage().
  // This function can be called by any thread.
  void Terminate();
  // Terminate and join the thread.
  // This function can be called by any thread.
  void TerminateAndWaitForThread();

  // Start running the given worker in another thread.
  static bool StartWorkerThread(std::shared_ptr<Worker> worker);

 private:
  friend class ProcessMessageTask;
  friend class TerminateTask;

  enum class State {
    kReady,
    kPrepareRunning,
    kRunning,
    kTerminating,
    kTerminated,
  };
  bool is_running() const;

  void ProcessMessage(std::unique_ptr<SerializationData> data);
  void ProcessMessages();

  class WorkerThread : public base::Thread {
   public:
    explicit WorkerThread(std::shared_ptr<Worker> worker)
        : base::Thread(base::Thread::Options("WorkerThread")),
          worker_(std::move(worker)) {}

    void Run() override;

   private:
    std::shared_ptr<Worker> worker_;
  };

  void ExecuteInThread();
  static void PostMessageOut(const v8::FunctionCallbackInfo<v8::Value>& args);

  base::Semaphore out_semaphore_{0};
  SerializationDataQueue out_queue_;
  base::Thread* thread_ = nullptr;
  char* script_;
  std::atomic<State> state_;
  bool is_joined_ = false;
  // For signalling that the worker has started.
  base::Semaphore started_semaphore_{0};

  // For posting tasks to the worker
  std::shared_ptr<TaskRunner> task_runner_;
  i::CancelableTaskManager* task_manager_;

  // Protects reading / writing task_runner_. (The TaskRunner itself doesn't
  // need locking, but accessing the Worker's data member does.)
  base::Mutex worker_mutex_;

  // The isolate should only be accessed by the worker itself, or when holding
  // the worker_mutex_ and after checking the worker state.
  Isolate* isolate_ = nullptr;

  // Only accessed by the worker thread.
  v8::Persistent<v8::Context> context_;
};

class PerIsolateData {
 public:
  explicit PerIsolateData(Isolate* isolate);

  ~PerIsolateData();

  inline static PerIsolateData* Get(Isolate* isolate) {
    return reinterpret_cast<PerIsolateData*>(isolate->GetData(0));
  }

  class V8_NODISCARD RealmScope {
   public:
    explicit RealmScope(PerIsolateData* data);
    ~RealmScope();

   private:
    PerIsolateData* data_;
  };

  // Contrary to RealmScope (which creates a new Realm), ExplicitRealmScope
  // allows for entering an existing Realm, as specified by its index.
  class V8_NODISCARD ExplicitRealmScope {
   public:
    explicit ExplicitRealmScope(PerIsolateData* data, int index);
    ~ExplicitRealmScope();

    Local<Context> context() const;

   private:
    PerIsolateData* data_;
    Local<Context> realm_;
    int index_;
    int previous_index_;
  };

  inline void SetTimeout(Local<Function> callback, Local<Context> context);
  inline MaybeLocal<Function> GetTimeoutCallback();
  inline MaybeLocal<Context> GetTimeoutContext();

  AsyncHooks* GetAsyncHooks() { return async_hooks_wrapper_; }

  void RemoveUnhandledPromise(Local<Promise> promise);
  void AddUnhandledPromise(Local<Promise> promise, Local<Message> message,
                           Local<Value> exception);
  int HandleUnhandledPromiseRejections();

  // Keep track of DynamicImportData so we can properly free it on shutdown
  // when LEAK_SANITIZER is active.
  void AddDynamicImportData(DynamicImportData*);
  void DeleteDynamicImportData(DynamicImportData*);

  Local<FunctionTemplate> GetTestApiObjectCtor() const;
  void SetTestApiObjectCtor(Local<FunctionTemplate> ctor);

  Local<FunctionTemplate> GetSnapshotObjectCtor() const;
  void SetSnapshotObjectCtor(Local<FunctionTemplate> ctor);

 private:
  friend class Shell;
  friend class RealmScope;
  Isolate* isolate_;
  int realm_count_;
  int realm_current_;
  int realm_switch_;
  Global<Context>* realms_;
  Global<Value> realm_shared_;
  std::queue<Global<Function>> set_timeout_callbacks_;
  std::queue<Global<Context>> set_timeout_contexts_;
  bool ignore_unhandled_promises_;
  std::vector<std::tuple<Global<Promise>, Global<Message>, Global<Value>>>
      unhandled_promises_;
  AsyncHooks* async_hooks_wrapper_;
#if defined(LEAK_SANITIZER)
  std::unordered_set<DynamicImportData*> import_data_;
#endif
  Global<FunctionTemplate> test_api_object_ctor_;
  Global<FunctionTemplate> snapshot_object_ctor_;

  int RealmIndexOrThrow(const v8::FunctionCallbackInfo<v8::Value>& args,
                        int arg_offset);
  int RealmFind(Local<Context> context);
};

extern bool check_d8_flag_contradictions;

class ShellOptions {
 public:
  enum CodeCacheOptions {
    kNoProduceCache,
    kProduceCache,
    kProduceCacheAfterExecute
  };

  ~ShellOptions() { delete[] isolate_sources; }

  // In analogy to Flag::CheckFlagChange() in src/flags/flag.cc, only allow
  // repeated flags for identical boolean values. We allow exceptions for flags
  // with enum-like arguments since their conflicts can also be specified
  // completely.
  template <class T,
            bool kAllowIdenticalAssignment = std::is_same<T, bool>::value>
  class DisallowReassignment {
   public:
    DisallowReassignment(const char* name, T value)
        : name_(name), value_(value) {}

    operator T() const { return value_; }
    T get() const { return value_; }
    DisallowReassignment& operator=(T value) {
      if (check_d8_flag_contradictions) {
        if (kAllowIdenticalAssignment) {
          if (specified_ && value_ != value) {
            FATAL("Contradictory values for d8 flag --%s", name_);
          }
        } else {
          if (specified_) {
            FATAL("Repeated specification of d8 flag --%s", name_);
          }
        }
      }
      value_ = value;
      specified_ = true;
      return *this;
    }
    void Overwrite(T value) { value_ = value; }

   private:
    const char* name_;
    T value_;
    bool specified_ = false;
  };

  DisallowReassignment<const char*> d8_path = {"d8-path", ""};
  DisallowReassignment<bool> fuzzilli_coverage_statistics = {
      "fuzzilli-coverage-statistics", false};
  DisallowReassignment<bool> fuzzilli_enable_builtins_coverage = {
      "fuzzilli-enable-builtins-coverage", true};
  DisallowReassignment<bool> send_idle_notification = {"send-idle-notification",
                                                       false};
  DisallowReassignment<bool> invoke_weak_callbacks = {"invoke-weak-callbacks",
                                                      false};
  DisallowReassignment<bool> omit_quit = {"omit-quit", false};
  DisallowReassignment<bool> wait_for_background_tasks = {
      "wait-for-background-tasks", true};
  DisallowReassignment<bool> simulate_errors = {"simulate-errors", false};
  DisallowReassignment<bool> stress_opt = {"stress-opt", false};
  DisallowReassignment<int> stress_runs = {"stress-runs", 1};
  DisallowReassignment<bool> interactive_shell = {"shell", false};
  bool test_shell = false;
  DisallowReassignment<bool> expected_to_throw = {"throws", false};
  DisallowReassignment<bool> no_fail = {"no-fail", false};
  DisallowReassignment<bool> dump_counters = {"dump-counters", false};
  DisallowReassignment<bool> dump_counters_nvp = {"dump-counters-nvp", false};
  DisallowReassignment<bool> ignore_unhandled_promises = {
      "ignore-unhandled-promises", false};
  DisallowReassignment<bool> mock_arraybuffer_allocator = {
      "mock-arraybuffer-allocator", false};
  DisallowReassignment<size_t> mock_arraybuffer_allocator_limit = {
      "mock-arraybuffer-allocator-limit", 0};
#if MULTI_MAPPED_ALLOCATOR_AVAILABLE
  DisallowReassignment<bool> multi_mapped_mock_allocator = {
      "multi-mapped-mock-allocator", false};
#endif
  DisallowReassignment<bool> enable_inspector = {"enable-inspector", false};
  int num_isolates = 1;
  DisallowReassignment<v8::ScriptCompiler::CompileOptions, true>
      compile_options = {"cache", v8::ScriptCompiler::kNoCompileOptions};
  DisallowReassignment<CodeCacheOptions, true> code_cache_options = {
      "cache", CodeCacheOptions::kNoProduceCache};
  DisallowReassignment<bool> streaming_compile = {"streaming-compile", false};
  DisallowReassignment<SourceGroup*> isolate_sources = {"isolate-sources",
                                                        nullptr};
  DisallowReassignment<const char*> icu_data_file = {"icu-data-file", nullptr};
  DisallowReassignment<const char*> icu_locale = {"icu-locale", nullptr};
  DisallowReassignment<const char*> snapshot_blob = {"snapshot_blob", nullptr};
  DisallowReassignment<bool> trace_enabled = {"trace-enabled", false};
  DisallowReassignment<const char*> trace_path = {"trace-path", nullptr};
  DisallowReassignment<const char*> trace_config = {"trace-config", nullptr};
  DisallowReassignment<const char*> lcov_file = {"lcov", nullptr};
  DisallowReassignment<bool> disable_in_process_stack_traces = {
      "disable-in-process-stack-traces", false};
  DisallowReassignment<int> read_from_tcp_port = {"read-from-tcp-port", -1};
  DisallowReassignment<bool> enable_os_system = {"enable-os-system", false};
  DisallowReassignment<bool> quiet_load = {"quiet-load", false};
  DisallowReassignment<int> thread_pool_size = {"thread-pool-size", 0};
  DisallowReassignment<bool> stress_delay_tasks = {"stress-delay-tasks", false};
  std::vector<const char*> arguments;
  DisallowReassignment<bool> include_arguments = {"arguments", true};
  DisallowReassignment<bool> cpu_profiler = {"cpu-profiler", false};
  DisallowReassignment<bool> cpu_profiler_print = {"cpu-profiler-print", false};
  DisallowReassignment<bool> fuzzy_module_file_extensions = {
      "fuzzy-module-file-extensions", true};
  DisallowReassignment<bool> enable_system_instrumentation = {
      "enable-system-instrumentation", false};
  DisallowReassignment<const char*> web_snapshot_config = {
      "web-snapshot-config", nullptr};
  DisallowReassignment<const char*> web_snapshot_output = {
      "web-snapshot-output", nullptr};
  DisallowReassignment<bool> d8_web_snapshot_api = {
      "experimental-d8-web-snapshot-api", false};
  // Applies to web snapshot and JSON deserialization.
  DisallowReassignment<bool> stress_deserialize = {"stress-deserialize", false};
  DisallowReassignment<bool> compile_only = {"compile-only", false};
  DisallowReassignment<int> repeat_compile = {"repeat-compile", 1};
#if V8_ENABLE_WEBASSEMBLY
  DisallowReassignment<bool> wasm_trap_handler = {"wasm-trap-handler", true};
#endif  // V8_ENABLE_WEBASSEMBLY
  DisallowReassignment<bool> expose_fast_api = {"expose-fast-api", false};
};

class Shell : public i::AllStatic {
 public:
  enum PrintResult : bool { kPrintResult = true, kNoPrintResult = false };
  enum ReportExceptions : bool {
    kReportExceptions = true,
    kNoReportExceptions = false
  };
  enum ProcessMessageQueue : bool {
    kProcessMessageQueue = true,
    kNoProcessMessageQueue = false
  };
  enum class CodeType { kFileName, kString, kFunction, kInvalid, kNone };

  static bool ExecuteString(Isolate* isolate, Local<String> source,
                            Local<String> name, PrintResult print_result,
                            ReportExceptions report_exceptions,
                            ProcessMessageQueue process_message_queue);
  static bool ExecuteModule(Isolate* isolate, const char* file_name);
  static bool ExecuteWebSnapshot(Isolate* isolate, const char* file_name);
  static bool LoadJSON(Isolate* isolate, const char* file_name);
  static void ReportException(Isolate* isolate, Local<Message> message,
                              Local<Value> exception);
  static void ReportException(Isolate* isolate, TryCatch* try_catch);
  static MaybeLocal<String> ReadFile(Isolate* isolate, const char* name,
                                     bool should_throw = true);
  static Local<String> WasmLoadSourceMapCallback(Isolate* isolate,
                                                 const char* name);
  static Local<Context> CreateEvaluationContext(Isolate* isolate);
  static int RunMain(Isolate* isolate, bool last_run);
  static int Main(int argc, char* argv[]);
  static void Exit(int exit_code);
  static void OnExit(Isolate* isolate, bool dispose);
  static void CollectGarbage(Isolate* isolate);
  static bool EmptyMessageQueues(Isolate* isolate);
  static bool CompleteMessageLoop(Isolate* isolate);

  static bool HandleUnhandledPromiseRejections(Isolate* isolate);

  static void PostForegroundTask(Isolate* isolate, std::unique_ptr<Task> task);
  static void PostBlockingBackgroundTask(std::unique_ptr<Task> task);

  static std::unique_ptr<SerializationData> SerializeValue(
      Isolate* isolate, Local<Value> value, Local<Value> transfer);
  static MaybeLocal<Value> DeserializeValue(
      Isolate* isolate, std::unique_ptr<SerializationData> data);
  static int* LookupCounter(const char* name);
  static void* CreateHistogram(const char* name, int min, int max,
                               size_t buckets);
  static void AddHistogramSample(void* histogram, int sample);
  static void MapCounters(v8::Isolate* isolate, const char* name);

  static void PerformanceNow(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PerformanceMeasureMemory(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void RealmCurrent(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmOwner(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmGlobal(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmCreate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmNavigate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmCreateAllowCrossRealmAccess(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmDetachGlobal(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmDispose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmSwitch(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmEval(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmSharedGet(Local<String> property,
                             const PropertyCallbackInfo<Value>& info);
  static void RealmSharedSet(Local<String> property, Local<Value> value,
                             const PropertyCallbackInfo<void>& info);
  static void RealmTakeWebSnapshot(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RealmUseWebSnapshot(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void LogGetAndStop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void TestVerifySourcePositions(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void InstallConditionalFeatures(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void AsyncHooksCreateHook(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AsyncHooksExecutionAsyncId(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AsyncHooksTriggerAsyncId(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void SetPromiseHooks(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Print(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PrintErr(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WriteStdout(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WaitUntilDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NotifyDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void QuitOnce(v8::FunctionCallbackInfo<v8::Value>* args);
  static void Quit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Version(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReadFile(const v8::FunctionCallbackInfo<v8::Value>& args);
  static char* ReadChars(const char* name, int* size_out);
  static MaybeLocal<PrimitiveArray> ReadLines(Isolate* isolate,
                                              const char* name);
  static void ReadBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static Local<String> ReadFromStdin(Isolate* isolate);
  static void ReadLine(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(ReadFromStdin(args.GetIsolate()));
  }
  static void WriteChars(const char* name, uint8_t* buffer, size_t buffer_size);
  static void ExecuteFile(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTimeout(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReadCodeTypeAndArguments(
      const v8::FunctionCallbackInfo<v8::Value>& args, int index,
      CodeType* code_type, Local<Value>* arguments = nullptr);
  static bool FunctionAndArgumentsToString(Local<Function> function,
                                           Local<Value> arguments,
                                           Local<String>* source,
                                           Isolate* isolate);
  static MaybeLocal<String> ReadSource(
      const v8::FunctionCallbackInfo<v8::Value>& args, int index,
      CodeType default_type);
  static void WorkerNew(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WorkerPostMessage(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WorkerGetMessage(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WorkerTerminate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WorkerTerminateAndWait(
      const v8::FunctionCallbackInfo<v8::Value>& args);
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
  static void System(const v8::FunctionCallbackInfo<v8::Value>& args);

  // os.chdir(dir) changes directory to the given directory.  Throws an
  // exception/ on error.
  static void ChangeDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);

  // os.setenv(variable, value) sets an environment variable.  Repeated calls to
  // this method leak memory due to the API of setenv in the standard C library.
  static void SetEnvironment(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void UnsetEnvironment(const v8::FunctionCallbackInfo<v8::Value>& args);

  // os.umask(alue) calls the umask system call and returns the old umask.
  static void SetUMask(const v8::FunctionCallbackInfo<v8::Value>& args);

  // os.mkdirp(name, mask) creates a directory.  The mask (if present) is anded
  // with the current umask.  Intermediate directories are created if necessary.
  // An exception is not thrown if the directory already exists.  Analogous to
  // the "mkdir -p" command.
  static void MakeDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RemoveDirectory(const v8::FunctionCallbackInfo<v8::Value>& args);
  static MaybeLocal<Promise> HostImportModuleDynamically(
      Local<Context> context, Local<Data> host_defined_options,
      Local<Value> resource_name, Local<String> specifier,
      Local<FixedArray> import_assertions);

  static void ModuleResolutionSuccessCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void ModuleResolutionFailureCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void HostInitializeImportMetaObject(Local<Context> context,
                                             Local<Module> module,
                                             Local<Object> meta);
  static MaybeLocal<Context> HostCreateShadowRealmContext(
      Local<Context> initiator_context);

#ifdef V8_FUZZILLI
  static void Fuzzilli(const v8::FunctionCallbackInfo<v8::Value>& args);
#endif  // V8_FUZZILLI

  // Data is of type DynamicImportData*. We use void* here to be able
  // to conform with MicrotaskCallback interface and enqueue this
  // function in the microtask queue.
  static void DoHostImportModuleDynamically(void* data);
  static void AddOSMethods(v8::Isolate* isolate,
                           Local<ObjectTemplate> os_template);

  static const char* kPrompt;
  static ShellOptions options;
  static ArrayBuffer::Allocator* array_buffer_allocator;
  static Isolate* shared_isolate;

  static void SetWaitUntilDone(Isolate* isolate, bool value);
  static void NotifyStartStreamingTask(Isolate* isolate);
  static void NotifyFinishStreamingTask(Isolate* isolate);

  static char* ReadCharsFromTcpPort(const char* name, int* size_out);

  static void set_script_executed() { script_executed_.store(true); }
  static bool use_interactive_shell() {
    return (options.interactive_shell || !script_executed_.load()) &&
           !options.test_shell;
  }

  static void update_script_size(int size) {
    if (size > 0) valid_fuzz_script_.store(true);
  }
  static bool is_valid_fuzz_script() { return valid_fuzz_script_.load(); }

  static void WaitForRunningWorkers();
  static void AddRunningWorker(std::shared_ptr<Worker> worker);
  static void RemoveRunningWorker(const std::shared_ptr<Worker>& worker);

  static void Initialize(Isolate* isolate, D8Console* console,
                         bool isOnMainThread = true);

  static void PromiseRejectCallback(v8::PromiseRejectMessage reject_message);

  static Local<FunctionTemplate> CreateSnapshotTemplate(Isolate* isolate);

 private:
  static inline int DeserializationRunCount() {
    return options.stress_deserialize ? 1000 : 1;
  }

  static Global<Context> evaluation_context_;
  static base::OnceType quit_once_;
  static Global<Function> stringify_function_;
  static const char* stringify_source_;
  static CounterMap* counter_map_;
  static base::SharedMutex counter_mutex_;
  // We statically allocate a set of local counters to be used if we
  // don't want to store the stats in a memory-mapped file
  static CounterCollection local_counters_;
  static CounterCollection* counters_;
  static base::OS::MemoryMappedFile* counters_file_;
  static base::LazyMutex context_mutex_;
  static const base::TimeTicks kInitialTicks;

  static base::LazyMutex workers_mutex_;  // Guards the following members.
  static bool allow_new_workers_;
  static std::unordered_set<std::shared_ptr<Worker>> running_workers_;

  // Multiple isolates may update these flags concurrently.
  static std::atomic<bool> script_executed_;
  static std::atomic<bool> valid_fuzz_script_;

  static void WriteIgnitionDispatchCountersFile(v8::Isolate* isolate);
  // Append LCOV coverage data to file.
  static void WriteLcovData(v8::Isolate* isolate, const char* file);
  static Counter* GetCounter(const char* name, bool is_histogram);
  static Local<String> Stringify(Isolate* isolate, Local<Value> value);
  static void RunShell(Isolate* isolate);
  static bool SetOptions(int argc, char* argv[]);

  static void NodeTypeCallback(const v8::FunctionCallbackInfo<v8::Value>& args);

  static Local<FunctionTemplate> CreateNodeTemplates(Isolate* isolate);
  static Local<ObjectTemplate> CreateGlobalTemplate(Isolate* isolate);
  static Local<ObjectTemplate> CreateOSTemplate(Isolate* isolate);
  static Local<FunctionTemplate> CreateWorkerTemplate(Isolate* isolate);
  static Local<ObjectTemplate> CreateAsyncHookTemplate(Isolate* isolate);
  static Local<ObjectTemplate> CreateTestRunnerTemplate(Isolate* isolate);
  static Local<ObjectTemplate> CreatePerformanceTemplate(Isolate* isolate);
  static Local<ObjectTemplate> CreateRealmTemplate(Isolate* isolate);
  static Local<ObjectTemplate> CreateD8Template(Isolate* isolate);
  static Local<FunctionTemplate> CreateTestFastCApiTemplate(Isolate* isolate);
  static Local<FunctionTemplate> CreateLeafInterfaceTypeTemplate(
      Isolate* isolate);

  static MaybeLocal<Context> CreateRealm(
      const v8::FunctionCallbackInfo<v8::Value>& args, int index,
      v8::MaybeLocal<Value> global_object);
  static void DisposeRealm(const v8::FunctionCallbackInfo<v8::Value>& args,
                           int index);
  static MaybeLocal<Module> FetchModuleTree(v8::Local<v8::Module> origin_module,
                                            v8::Local<v8::Context> context,
                                            const std::string& file_name,
                                            ModuleType module_type);

  static MaybeLocal<Value> JSONModuleEvaluationSteps(Local<Context> context,
                                                     Local<Module> module);

  template <class T>
  static MaybeLocal<T> CompileString(Isolate* isolate, Local<Context> context,
                                     Local<String> source,
                                     const ScriptOrigin& origin);

  static ScriptCompiler::CachedData* LookupCodeCache(Isolate* isolate,
                                                     Local<Value> name);
  static void StoreInCodeCache(Isolate* isolate, Local<Value> name,
                               const ScriptCompiler::CachedData* data);
  // We may have multiple isolates running concurrently, so the access to
  // the isolate_status_ needs to be concurrency-safe.
  static base::LazyMutex isolate_status_lock_;
  static std::map<Isolate*, bool> isolate_status_;
  static std::map<Isolate*, int> isolate_running_streaming_tasks_;

  static base::LazyMutex cached_code_mutex_;
  static std::map<std::string, std::unique_ptr<ScriptCompiler::CachedData>>
      cached_code_map_;
  static std::atomic<int> unhandled_promise_rejections_;
};

class FuzzerMonitor : public i::AllStatic {
 public:
  static void SimulateErrors();

 private:
  static void ControlFlowViolation();
  static void DCheck();
  static void Fatal();
  static void ObservableDifference();
  static void UndefinedBehavior();
  static void UseAfterFree();
  static void UseOfUninitializedValue();
};

}  // namespace v8

#endif  // V8_D8_D8_H_
