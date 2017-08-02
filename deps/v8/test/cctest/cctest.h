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
#include "src/debug/debug-interface.h"
#include "src/utils.h"
#include "src/v8.h"
#include "src/zone/accounting-allocator.h"

namespace v8 {
namespace base {

class RandomNumberGenerator;

}  // namespace base

namespace internal {

class HandleScope;
class Zone;

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
enum CcTestExtensionIds {
  EXTENSION_LIST(DEFINE_EXTENSION_ID)
  kMaxExtensions
};
#undef DEFINE_EXTENSION_ID

typedef v8::internal::EnumSet<CcTestExtensionIds> CcTestExtensionFlags;
#define DEFINE_EXTENSION_FLAG(Name, Ident)                               \
  static const CcTestExtensionFlags Name(1 << Name##_ID);
  static const CcTestExtensionFlags NO_EXTENSIONS(0);
  static const CcTestExtensionFlags ALL_EXTENSIONS((1 << kMaxExtensions) - 1);
  EXTENSION_LIST(DEFINE_EXTENSION_FLAG)
#undef DEFINE_EXTENSION_FLAG


class CcTest {
 public:
  typedef void (TestFunction)();
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
    CHECK(isolate_ != NULL);
    v8::base::NoBarrier_Store(&isolate_used_, 1);
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

  static void CollectGarbage(i::AllocationSpace space);
  static void CollectAllGarbage();
  static void CollectAllGarbage(int flags);
  static void CollectAllAvailableGarbage();

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
      CcTestExtensionFlags extensions,
      v8::Isolate* isolate = CcTest::isolate());

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
  virtual void Run();

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
  ~ApiTestFuzzer() {}

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
      : fuzzer_(NULL), callback_(callback), name_(name) {
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
  LocalContext(v8::Isolate* isolate, v8::ExtensionConfiguration* extensions = 0,
               v8::Local<v8::ObjectTemplate> global_template =
                   v8::Local<v8::ObjectTemplate>(),
               v8::Local<v8::Value> global_object = v8::Local<v8::Value>()) {
    Initialize(isolate, extensions, global_template, global_object);
  }

  LocalContext(v8::ExtensionConfiguration* extensions = 0,
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
  int array_length = i::StrLength(source) + 1;
  uint16_t* converted = i::NewArray<uint16_t>(array_length);
  for (int i = 0; i < array_length; i++) converted[i] = source[i];
  return converted;
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
  if (v8::Script::Compile(v8::Isolate::GetCurrent()->GetCurrentContext(), x)
          .ToLocal(&result)) {
    return result;
  }
  return v8::Local<v8::Script>();
}


static inline v8::Local<v8::Script> v8_compile(const char* x) {
  return v8_compile(v8_str(x));
}


static inline int32_t v8_run_int32value(v8::Local<v8::Script> script) {
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  return script->Run(context).ToLocalChecked()->Int32Value(context).FromJust();
}


static inline v8::Local<v8::Script> CompileWithOrigin(
    v8::Local<v8::String> source, v8::Local<v8::String> origin_url) {
  v8::ScriptOrigin origin(origin_url);
  v8::ScriptCompiler::Source script_source(source, origin);
  return v8::ScriptCompiler::Compile(
             v8::Isolate::GetCurrent()->GetCurrentContext(), &script_source)
      .ToLocalChecked();
}


static inline v8::Local<v8::Script> CompileWithOrigin(
    v8::Local<v8::String> source, const char* origin_url) {
  return CompileWithOrigin(source, v8_str(origin_url));
}


static inline v8::Local<v8::Script> CompileWithOrigin(const char* source,
                                                      const char* origin_url) {
  return CompileWithOrigin(v8_str(source), v8_str(origin_url));
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


static inline v8::Local<v8::Value> ParserCacheCompileRun(const char* source) {
  // Compile once just to get the preparse data, then compile the second time
  // using the data.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::ScriptCompiler::Source script_source(v8_str(source));
  v8::ScriptCompiler::Compile(context, &script_source,
                              v8::ScriptCompiler::kProduceParserCache)
      .ToLocalChecked();

  // Check whether we received cached data, and if so use it.
  v8::ScriptCompiler::CompileOptions options =
      script_source.GetCachedData() ? v8::ScriptCompiler::kConsumeParserCache
                                    : v8::ScriptCompiler::kNoCompileOptions;

  return CompileRun(context, &script_source, options);
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



static inline void ExpectString(const char* code, const char* expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsString());
  v8::String::Utf8Value utf8(result);
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
  CHECK_EQ(expected,
           result->BooleanValue(v8::Isolate::GetCurrent()->GetCurrentContext())
               .FromJust());
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

  ~StaticOneByteResource() {}

  const char* data() const { return data_; }

  size_t length() const { return strlen(data_); }

 private:
  const char* data_;
};

#endif  // ifndef CCTEST_H_
