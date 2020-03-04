// Copyright 2008 the V8 project authors. All rights reserved.
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

#ifndef CCTEST_H_
#define CCTEST_H_

#include <memory>

#include "include/libplatform/libplatform.h"
#include "include/v8-platform.h"
#include "src/base/enum-set.h"
#include "src/codegen/register-configuration.h"
#include "src/debug/debug-interface.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/factory.h"
#include "src/init/v8.h"
#include "src/objects/objects.h"
#include "src/zone/accounting-allocator.h"

namespace v8 {
namespace base {

class RandomNumberGenerator;

}  // namespace base

namespace internal {

const auto GetRegConfig = RegisterConfiguration::Default;

class HandleScope;
class Zone;

namespace compiler {

class JSHeapBroker;

}  // namespace compiler

}  // namespace internal

}  // namespace v8

#ifndef TEST
#define TEST(Name)                                                      \
  static void Test##Name();                                             \
  CcTest register_test_##Name(Test##Name, __FILE__, #Name, true, true); \
  static void Test##Name()
#endif

#ifndef UNINITIALIZED_TEST
#define UNINITIALIZED_TEST(Name)                                         \
  static void Test##Name();                                              \
  CcTest register_test_##Name(Test##Name, __FILE__, #Name, true, false); \
  static void Test##Name()
#endif

#ifndef DISABLED_TEST
#define DISABLED_TEST(Name)                                              \
  static void Test##Name();                                              \
  CcTest register_test_##Name(Test##Name, __FILE__, #Name, false, true); \
  static void Test##Name()
#endif

#define EXTENSION_LIST(V)                                                      \
  V(GC_EXTENSION,       "v8/gc")                                               \
  V(PRINT_EXTENSION,    "v8/print")                                            \
  V(PROFILER_EXTENSION, "v8/profiler")                                         \
  V(TRACE_EXTENSION,    "v8/trace")

#define DEFINE_EXTENSION_ID(Name, Ident) Name##_ID,
enum CcTestExtensionId { EXTENSION_LIST(DEFINE_EXTENSION_ID) kMaxExtensions };
#undef DEFINE_EXTENSION_ID

using CcTestExtensionFlags = v8::base::EnumSet<CcTestExtensionId>;

#define DEFINE_EXTENSION_NAME(Name, Ident) Ident,
static constexpr const char* kExtensionName[kMaxExtensions] = {
    EXTENSION_LIST(DEFINE_EXTENSION_NAME)};
#undef DEFINE_EXTENSION_NAME

class CcTest {
 public:
  using TestFunction = void();
  CcTest(TestFunction* callback, const char* file, const char* name,
         bool enabled, bool initialize);
  ~CcTest() { i::DeleteArray(file_); }
  void Run();
  static CcTest* last() { return last_; }
  CcTest* prev() { return prev_; }
  const char* file() { return file_; }
  const char* name() { return name_; }
  bool enabled() { return enabled_; }

  static v8::Isolate* isolate() {
    CHECK_NOT_NULL(isolate_);
    v8::base::Relaxed_Store(&isolate_used_, 1);
    return isolate_;
  }

  static i::Isolate* InitIsolateOnce() {
    if (!initialize_called_) InitializeVM();
    return i_isolate();
  }

  static i::Isolate* i_isolate() {
    return reinterpret_cast<i::Isolate*>(isolate());
  }

  static i::Heap* heap();
  static i::ReadOnlyHeap* read_only_heap();

  static void CollectGarbage(i::AllocationSpace space);
  static void CollectAllGarbage(i::Isolate* isolate = nullptr);
  static void CollectAllAvailableGarbage(i::Isolate* isolate = nullptr);
  static void PreciseCollectAllGarbage(i::Isolate* isolate = nullptr);

  static v8::base::RandomNumberGenerator* random_number_generator();

  static v8::Local<v8::Object> global();

  static v8::ArrayBuffer::Allocator* array_buffer_allocator() {
    return allocator_;
  }

  static void set_array_buffer_allocator(
      v8::ArrayBuffer::Allocator* allocator) {
    allocator_ = allocator;
  }

  // TODO(dcarney): Remove.
  // This must be called first in a test.
  static void InitializeVM();

  // Only for UNINITIALIZED_TESTs
  static void DisableAutomaticDispose();

  // Helper function to configure a context.
  // Must be in a HandleScope.
  static v8::Local<v8::Context> NewContext(
      v8::Isolate* isolate = CcTest::isolate()) {
    return NewContext({}, isolate);
  }
  static v8::Local<v8::Context> NewContext(
      CcTestExtensionFlags extension_flags,
      v8::Isolate* isolate = CcTest::isolate());
  static v8::Local<v8::Context> NewContext(
      std::initializer_list<CcTestExtensionId> extensions,
      v8::Isolate* isolate = CcTest::isolate()) {
    return NewContext(CcTestExtensionFlags{extensions}, isolate);
  }

  static void TearDown();

 private:
  friend int main(int argc, char** argv);
  TestFunction* callback_;
  const char* file_;
  const char* name_;
  bool enabled_;
  bool initialize_;
  CcTest* prev_;
  static CcTest* last_;
  static v8::ArrayBuffer::Allocator* allocator_;
  static v8::Isolate* isolate_;
  static bool initialize_called_;
  static v8::base::Atomic32 isolate_used_;
};

// Switches between all the Api tests using the threading support.
// In order to get a surprising but repeatable pattern of thread
// switching it has extra semaphores to control the order in which
// the tests alternate, not relying solely on the big V8 lock.
//
// A test is augmented with calls to ApiTestFuzzer::Fuzz() in its
// callbacks.  This will have no effect when we are not running the
// thread fuzzing test.  In the thread fuzzing test it will
// pseudorandomly select a successor thread and switch execution
// to that thread, suspending the current test.
class ApiTestFuzzer: public v8::base::Thread {
 public:
  void CallTest();

  // The ApiTestFuzzer is also a Thread, so it has a Run method.
  void Run() override;

  enum PartOfTest {
    FIRST_PART,
    SECOND_PART,
    THIRD_PART,
    FOURTH_PART,
    FIFTH_PART,
    SIXTH_PART,
    SEVENTH_PART,
    EIGHTH_PART,
    LAST_PART = EIGHTH_PART
  };

  static void SetUp(PartOfTest part);
  static void RunAllTests();
  static void TearDown();
  // This method switches threads if we are running the Threading test.
  // Otherwise it does nothing.
  static void Fuzz();

 private:
  explicit ApiTestFuzzer(int num)
      : Thread(Options("ApiTestFuzzer")),
        test_number_(num),
        gate_(0),
        active_(true) {}
  ~ApiTestFuzzer() override = default;

  static bool fuzzing_;
  static int tests_being_run_;
  static int current_;
  static int active_tests_;
  static bool NextThread();
  int test_number_;
  v8::base::Semaphore gate_;
  bool active_;
  void ContextSwitch();
  static int GetNextTestNumber();
  static v8::base::Semaphore all_tests_done_;
};


#define THREADED_TEST(Name)                                          \
  static void Test##Name();                                          \
  RegisterThreadedTest register_##Name(Test##Name, #Name);           \
  /* */ TEST(Name)

class RegisterThreadedTest {
 public:
  explicit RegisterThreadedTest(CcTest::TestFunction* callback,
                                const char* name)
      : fuzzer_(nullptr), callback_(callback), name_(name) {
    prev_ = first_;
    first_ = this;
    count_++;
  }
  static int count() { return count_; }
  static RegisterThreadedTest* nth(int i) {
    CHECK(i < count());
    RegisterThreadedTest* current = first_;
    while (i > 0) {
      i--;
      current = current->prev_;
    }
    return current;
  }
  CcTest::TestFunction* callback() { return callback_; }
  ApiTestFuzzer* fuzzer_;
  const char* name() { return name_; }

 private:
  static RegisterThreadedTest* first_;
  static int count_;
  CcTest::TestFunction* callback_;
  RegisterThreadedTest* prev_;
  const char* name_;
};

// A LocalContext holds a reference to a v8::Context.
class LocalContext {
 public:
  LocalContext(v8::Isolate* isolate,
               v8::ExtensionConfiguration* extensions = nullptr,
               v8::Local<v8::ObjectTemplate> global_template =
                   v8::Local<v8::ObjectTemplate>(),
               v8::Local<v8::Value> global_object = v8::Local<v8::Value>()) {
    Initialize(isolate, extensions, global_template, global_object);
  }

  LocalContext(v8::ExtensionConfiguration* extensions = nullptr,
               v8::Local<v8::ObjectTemplate> global_template =
                   v8::Local<v8::ObjectTemplate>(),
               v8::Local<v8::Value> global_object = v8::Local<v8::Value>()) {
    Initialize(CcTest::isolate(), extensions, global_template, global_object);
  }

  virtual ~LocalContext();

  v8::Context* operator->() {
    return *reinterpret_cast<v8::Context**>(&context_);
  }
  v8::Context* operator*() { return operator->(); }
  bool IsReady() { return !context_.IsEmpty(); }

  v8::Local<v8::Context> local() {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

 private:
  void Initialize(v8::Isolate* isolate, v8::ExtensionConfiguration* extensions,
                  v8::Local<v8::ObjectTemplate> global_template,
                  v8::Local<v8::Value> global_object);

  v8::Persistent<v8::Context> context_;
  v8::Isolate* isolate_;
};


static inline uint16_t* AsciiToTwoByteString(const char* source) {
  size_t array_length = strlen(source) + 1;
  uint16_t* converted = i::NewArray<uint16_t>(array_length);
  for (size_t i = 0; i < array_length; i++) converted[i] = source[i];
  return converted;
}

template <typename T>
static inline i::Handle<T> GetGlobal(const char* name) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Handle<i::String> str_name =
      isolate->factory()->InternalizeUtf8String(name);

  i::Handle<i::Object> value =
      i::Object::GetProperty(isolate, isolate->global_object(), str_name)
          .ToHandleChecked();
  return i::Handle<T>::cast(value);
}

static inline v8::Local<v8::Boolean> v8_bool(bool val) {
  return v8::Boolean::New(v8::Isolate::GetCurrent(), val);
}

static inline v8::Local<v8::Value> v8_num(double x) {
  return v8::Number::New(v8::Isolate::GetCurrent(), x);
}

static inline v8::Local<v8::Integer> v8_int(int32_t x) {
  return v8::Integer::New(v8::Isolate::GetCurrent(), x);
}

static inline v8::Local<v8::String> v8_str(const char* x) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), x,
                                 v8::NewStringType::kNormal)
      .ToLocalChecked();
}


static inline v8::Local<v8::String> v8_str(v8::Isolate* isolate,
                                           const char* x) {
  return v8::String::NewFromUtf8(isolate, x, v8::NewStringType::kNormal)
      .ToLocalChecked();
}


static inline v8::Local<v8::Symbol> v8_symbol(const char* name) {
  return v8::Symbol::New(v8::Isolate::GetCurrent(), v8_str(name));
}


static inline v8::Local<v8::Script> v8_compile(v8::Local<v8::String> x) {
  v8::Local<v8::Script> result;
  CHECK(v8::Script::Compile(v8::Isolate::GetCurrent()->GetCurrentContext(), x)
            .ToLocal(&result));
  return result;
}

static inline v8::Local<v8::Script> v8_compile(const char* x) {
  return v8_compile(v8_str(x));
}

static inline v8::MaybeLocal<v8::Script> v8_try_compile(
    v8::Local<v8::String> x) {
  return v8::Script::Compile(v8::Isolate::GetCurrent()->GetCurrentContext(), x);
}

static inline v8::MaybeLocal<v8::Script> v8_try_compile(const char* x) {
  return v8_try_compile(v8_str(x));
}

static inline int32_t v8_run_int32value(v8::Local<v8::Script> script) {
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  return script->Run(context).ToLocalChecked()->Int32Value(context).FromJust();
}

static inline v8::Local<v8::Script> CompileWithOrigin(
    v8::Local<v8::String> source, v8::Local<v8::String> origin_url,
    v8::Local<v8::Boolean> is_shared_cross_origin) {
  v8::ScriptOrigin origin(origin_url, v8::Local<v8::Integer>(),
                          v8::Local<v8::Integer>(), is_shared_cross_origin);
  v8::ScriptCompiler::Source script_source(source, origin);
  return v8::ScriptCompiler::Compile(
             v8::Isolate::GetCurrent()->GetCurrentContext(), &script_source)
      .ToLocalChecked();
}

static inline v8::Local<v8::Script> CompileWithOrigin(
    v8::Local<v8::String> source, const char* origin_url,
    bool is_shared_cross_origin) {
  return CompileWithOrigin(source, v8_str(origin_url),
                           v8_bool(is_shared_cross_origin));
}

static inline v8::Local<v8::Script> CompileWithOrigin(
    const char* source, const char* origin_url, bool is_shared_cross_origin) {
  return CompileWithOrigin(v8_str(source), v8_str(origin_url),
                           v8_bool(is_shared_cross_origin));
}

// Helper functions that compile and run the source.
static inline v8::MaybeLocal<v8::Value> CompileRun(
    v8::Local<v8::Context> context, const char* source) {
  return v8::Script::Compile(context, v8_str(source))
      .ToLocalChecked()
      ->Run(context);
}


static inline v8::Local<v8::Value> CompileRunChecked(v8::Isolate* isolate,
                                                     const char* source) {
  v8::Local<v8::String> source_string =
      v8::String::NewFromUtf8(isolate, source, v8::NewStringType::kNormal)
          .ToLocalChecked();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source_string).ToLocalChecked();
  return script->Run(context).ToLocalChecked();
}


static inline v8::Local<v8::Value> CompileRun(v8::Local<v8::String> source) {
  v8::Local<v8::Value> result;
  if (v8_compile(source)
          ->Run(v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocal(&result)) {
    return result;
  }
  return v8::Local<v8::Value>();
}


// Helper functions that compile and run the source.
static inline v8::Local<v8::Value> CompileRun(const char* source) {
  return CompileRun(v8_str(source));
}


static inline v8::Local<v8::Value> CompileRun(
    v8::Local<v8::Context> context, v8::ScriptCompiler::Source* script_source,
    v8::ScriptCompiler::CompileOptions options) {
  v8::Local<v8::Value> result;
  if (v8::ScriptCompiler::Compile(context, script_source, options)
          .ToLocalChecked()
          ->Run(context)
          .ToLocal(&result)) {
    return result;
  }
  return v8::Local<v8::Value>();
}


// Helper functions that compile and run the source with given origin.
static inline v8::Local<v8::Value> CompileRunWithOrigin(const char* source,
                                                        const char* origin_url,
                                                        int line_number,
                                                        int column_number) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::ScriptOrigin origin(v8_str(origin_url),
                          v8::Integer::New(isolate, line_number),
                          v8::Integer::New(isolate, column_number));
  v8::ScriptCompiler::Source script_source(v8_str(source), origin);
  return CompileRun(context, &script_source,
                    v8::ScriptCompiler::CompileOptions());
}


static inline v8::Local<v8::Value> CompileRunWithOrigin(
    v8::Local<v8::String> source, const char* origin_url) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::ScriptCompiler::Source script_source(
      source, v8::ScriptOrigin(v8_str(origin_url)));
  return CompileRun(context, &script_source,
                    v8::ScriptCompiler::CompileOptions());
}


static inline v8::Local<v8::Value> CompileRunWithOrigin(
    const char* source, const char* origin_url) {
  return CompileRunWithOrigin(v8_str(source), origin_url);
}

// Takes a JSFunction and runs it through the test version of the optimizing
// pipeline, allocating the temporary compilation artifacts in a given Zone.
// For possible {flags} values, look at OptimizedCompilationInfo::Flag.  If
// {out_broker} is not nullptr, returns the JSHeapBroker via that (transferring
// ownership to the caller).
i::Handle<i::JSFunction> Optimize(
    i::Handle<i::JSFunction> function, i::Zone* zone, i::Isolate* isolate,
    uint32_t flags,
    std::unique_ptr<i::compiler::JSHeapBroker>* out_broker = nullptr);

static inline void ExpectString(const char* code, const char* expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsString());
  v8::String::Utf8Value utf8(v8::Isolate::GetCurrent(), result);
  CHECK_EQ(0, strcmp(expected, *utf8));
}


static inline void ExpectInt32(const char* code, int expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsInt32());
  CHECK_EQ(expected,
           result->Int32Value(v8::Isolate::GetCurrent()->GetCurrentContext())
               .FromJust());
}


static inline void ExpectBoolean(const char* code, bool expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsBoolean());
  CHECK_EQ(expected, result->BooleanValue(v8::Isolate::GetCurrent()));
}


static inline void ExpectTrue(const char* code) {
  ExpectBoolean(code, true);
}


static inline void ExpectFalse(const char* code) {
  ExpectBoolean(code, false);
}


static inline void ExpectObject(const char* code,
                                v8::Local<v8::Value> expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->SameValue(expected));
}


static inline void ExpectUndefined(const char* code) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsUndefined());
}


static inline void ExpectNull(const char* code) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsNull());
}


static inline void CheckDoubleEquals(double expected, double actual) {
  const double kEpsilon = 1e-10;
  CHECK_LE(expected, actual + kEpsilon);
  CHECK_GE(expected, actual - kEpsilon);
}

static v8::debug::DebugDelegate dummy_delegate;

static inline void EnableDebugger(v8::Isolate* isolate) {
  v8::debug::SetDebugDelegate(isolate, &dummy_delegate);
}


static inline void DisableDebugger(v8::Isolate* isolate) {
  v8::debug::SetDebugDelegate(isolate, nullptr);
}


static inline void EmptyMessageQueues(v8::Isolate* isolate) {
  while (v8::platform::PumpMessageLoop(v8::internal::V8::GetCurrentPlatform(),
                                       isolate)) {
  }
}

class InitializedHandleScopeImpl;

class InitializedHandleScope {
 public:
  InitializedHandleScope();
  ~InitializedHandleScope();

  // Prefixing the below with main_ reduces a lot of naming clashes.
  i::Isolate* main_isolate() { return main_isolate_; }

 private:
  i::Isolate* main_isolate_;
  std::unique_ptr<InitializedHandleScopeImpl> initialized_handle_scope_impl_;
};

class HandleAndZoneScope : public InitializedHandleScope {
 public:
  HandleAndZoneScope();
  ~HandleAndZoneScope();

  // Prefixing the below with main_ reduces a lot of naming clashes.
  i::Zone* main_zone() { return main_zone_.get(); }

 private:
  v8::internal::AccountingAllocator allocator_;
  std::unique_ptr<i::Zone> main_zone_;
};

class StaticOneByteResource : public v8::String::ExternalOneByteStringResource {
 public:
  explicit StaticOneByteResource(const char* data) : data_(data) {}

  ~StaticOneByteResource() override = default;

  const char* data() const override { return data_; }

  size_t length() const override { return strlen(data_); }

 private:
  const char* data_;
};

class ManualGCScope {
 public:
  ManualGCScope()
      : flag_concurrent_marking_(i::FLAG_concurrent_marking),
        flag_concurrent_sweeping_(i::FLAG_concurrent_sweeping),
        flag_stress_incremental_marking_(i::FLAG_stress_incremental_marking),
        flag_parallel_marking_(i::FLAG_parallel_marking),
        flag_detect_ineffective_gcs_near_heap_limit_(
            i::FLAG_detect_ineffective_gcs_near_heap_limit) {
    i::FLAG_concurrent_marking = false;
    i::FLAG_concurrent_sweeping = false;
    i::FLAG_stress_incremental_marking = false;
    // Parallel marking has a dependency on concurrent marking.
    i::FLAG_parallel_marking = false;
    i::FLAG_detect_ineffective_gcs_near_heap_limit = false;
  }
  ~ManualGCScope() {
    i::FLAG_concurrent_marking = flag_concurrent_marking_;
    i::FLAG_concurrent_sweeping = flag_concurrent_sweeping_;
    i::FLAG_stress_incremental_marking = flag_stress_incremental_marking_;
    i::FLAG_parallel_marking = flag_parallel_marking_;
    i::FLAG_detect_ineffective_gcs_near_heap_limit =
        flag_detect_ineffective_gcs_near_heap_limit_;
  }

 private:
  bool flag_concurrent_marking_;
  bool flag_concurrent_sweeping_;
  bool flag_stress_incremental_marking_;
  bool flag_parallel_marking_;
  bool flag_detect_ineffective_gcs_near_heap_limit_;
};

// This is an abstract base class that can be overridden to implement a test
// platform. It delegates all operations to a given platform at the time
// of construction.
class TestPlatform : public v8::Platform {
 public:
  // v8::Platform implementation.
  v8::PageAllocator* GetPageAllocator() override {
    return old_platform_->GetPageAllocator();
  }

  void OnCriticalMemoryPressure() override {
    old_platform_->OnCriticalMemoryPressure();
  }

  bool OnCriticalMemoryPressure(size_t length) override {
    return old_platform_->OnCriticalMemoryPressure(length);
  }

  int NumberOfWorkerThreads() override {
    return old_platform_->NumberOfWorkerThreads();
  }

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return old_platform_->GetForegroundTaskRunner(isolate);
  }

  void CallOnWorkerThread(std::unique_ptr<v8::Task> task) override {
    old_platform_->CallOnWorkerThread(std::move(task));
  }

  void CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task,
                                 double delay_in_seconds) override {
    old_platform_->CallDelayedOnWorkerThread(std::move(task), delay_in_seconds);
  }

  void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) override {
    // This is a deprecated function and should not be called anymore.
    UNREACHABLE();
  }

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, v8::Task* task,
                                     double delay_in_seconds) override {
    // This is a deprecated function and should not be called anymore.
    UNREACHABLE();
  }

  double MonotonicallyIncreasingTime() override {
    return old_platform_->MonotonicallyIncreasingTime();
  }

  double CurrentClockTimeMillis() override {
    return old_platform_->CurrentClockTimeMillis();
  }

  void CallIdleOnForegroundThread(v8::Isolate* isolate,
                                  v8::IdleTask* task) override {
    // This is a deprecated function and should not be called anymore.
    UNREACHABLE();
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override {
    return old_platform_->IdleTasksEnabled(isolate);
  }

  v8::TracingController* GetTracingController() override {
    return old_platform_->GetTracingController();
  }

 protected:
  TestPlatform() : old_platform_(i::V8::GetCurrentPlatform()) {}
  ~TestPlatform() override { i::V8::SetPlatformForTesting(old_platform_); }

  v8::Platform* old_platform() const { return old_platform_; }

 private:
  v8::Platform* old_platform_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatform);
};

#endif  // ifndef CCTEST_H_
