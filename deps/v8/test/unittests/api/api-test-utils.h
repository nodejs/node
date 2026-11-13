// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_API_API_TEST_UTILS_H_
#define V8_UNITTESTS_API_API_TEST_UTILS_H_

#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8-array-buffer.h"
#include "include/v8-context.h"
#include "include/v8-function-callback.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "include/v8-unwinder.h"
#include "src/api/api-inl.h"
#include "src/base/platform/semaphore.h"
#include "src/debug/debug-interface.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state.h"
#include "src/init/v8.h"
#include "src/sandbox/sandboxable-thread.h"
#include "test/common/fast-api-test-helpers.h"
#include "test/unittests/test-utils.h"

// Shared helpers for the API tests in test/unittests/api/ that were migrated
// from cctest (api-unittest.cc, api-interceptors-unittest.cc, ...).
//
// THREADED_TEST bodies are free functions that ApiTestFuzzer also runs on
// background threads that share a single isolate (see Threading1..8 in
// api-unittest.cc). There is no test fixture on those threads, so the helpers
// below use the isolate that is entered on the current thread
// (v8::Isolate::GetCurrent()). Inside TEST_F bodies, prefer the fixture
// accessors (v8_isolate(), i_isolate()).

namespace v8 {

// -----------------------------------------------------------------------------
// Checkers for API callbacks.

template <typename T>
i::Tagged<i::Object> Get(v8::ReturnValue<T> return_value) {
  return *v8::Utils::OpenDirectHandle(*return_value.Get());
}

template <typename T>
void CheckReturnValueImpl(v8::Isolate* v8_isolate,
                          v8::ReturnValue<T> return_value,
                          i::Address callback) {
  constexpr bool is_int = std::is_same_v<T, v8::Integer>;
  constexpr bool is_bool =
      std::is_same_v<T, v8::Boolean> || std::is_same_v<T, void>;
  constexpr bool is_array = std::is_same_v<T, v8::Array>;
  constexpr bool is_value = std::is_same_v<T, v8::Value>;
  static_assert(is_int || is_bool || is_array || is_value);

  CHECK_EQ(v8::Isolate::GetCurrent(), v8_isolate);
  CHECK_EQ(v8_isolate, return_value.GetIsolate());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);

  i::Tagged<i::Object> default_value;
  if constexpr (is_int) {
    default_value = i::Smi::zero();
  } else if constexpr (is_bool) {
    default_value = i::ReadOnlyRoots(isolate).true_value();
  } else if constexpr (is_array) {
    // TODO(ishell): enumerator callback's return value is initialized with
    // undefined even though it's supposed to return v8::Array.
    default_value = i::ReadOnlyRoots(isolate).undefined_value();
  } else if constexpr (is_value) {
    default_value = i::ReadOnlyRoots(isolate).undefined_value();
  }

  auto CheckValueMap = [=](v8::ReturnValue<T>& return_value) {
    i::Tagged<i::Object> obj = Get(return_value);
    if constexpr (is_int) {
      return i::IsNumber(obj);
    } else if constexpr (is_bool) {
      return i::IsBoolean(obj);
    } else if constexpr (is_array) {
      // TODO(ishell): enumerator callback's return value is initialized with
      // undefined even though it's supposed to return v8::Array.
      return i::IsUndefined(obj) || i::IsJSObject(obj);
    } else if constexpr (is_value) {
      // TODO(ishell): just `return IsJSAny(obj);` when available.
      // Similar to v8::Data::IsValue().
      if (i::IsSmi(obj)) return true;
      i::Tagged<i::HeapObject> heap_object = i::Cast<i::HeapObject>(obj);
      if (i::IsSymbol(heap_object)) {
        return !i::Cast<i::Symbol>(heap_object)->is_any_private();
      }
      return i::IsPrimitiveHeapObject(heap_object) ||
             i::IsJSReceiver(heap_object);
    }
    UNREACHABLE();
  };

  // Default state.
  CHECK_EQ(default_value, Get(return_value));
  CHECK(CheckValueMap(return_value));

  if constexpr (is_bool || is_value) {
    return_value.Set(true);
    CHECK_EQ(Get(return_value), i::ReadOnlyRoots(isolate).true_value());
    CHECK(CheckValueMap(return_value));
  }

  if constexpr (is_int || is_value) {
    return_value.Set(42);
    CHECK_EQ(Get(return_value), i::Smi::FromInt(42));
    CHECK(CheckValueMap(return_value));

    return_value.Set(-153);
    CHECK_EQ(Get(return_value), i::Smi::FromInt(-153));
    CHECK(CheckValueMap(return_value));
  }

  if constexpr (is_value) {
    return_value.SetNull();
    CHECK_EQ(Get(return_value), i::ReadOnlyRoots(isolate).null_value());
    CHECK(CheckValueMap(return_value));
  }

  if constexpr (is_value) {
    // Reset to default state.
    return_value.Set(v8::Local<v8::Object>());
    CHECK_EQ(default_value, Get(return_value));
    CHECK(CheckValueMap(return_value));
  }

  // If CPU profiler is active check that when API callback is invoked
  // VMState is set to EXTERNAL.
  if (isolate->is_profiling()) {
    CHECK(v8::IsExternal(isolate->current_vm_state()));
    CHECK(isolate->external_callback_scope());
    CHECK_EQ(callback, isolate->external_callback_scope()->callback());
  }
}

template <typename TCallbackInfo>
void CheckReturnValue(const TCallbackInfo& info, i::Address callback) {
  CheckReturnValueImpl(info.GetIsolate(), info.GetReturnValue(), callback);
}

template <typename T>
void CheckInternalFieldsAreZero(v8::Local<T> value) {
  CHECK_EQ(T::kInternalFieldCount, value->InternalFieldCount());
  for (int i = 0; i < value->InternalFieldCount(); i++) {
    v8::Local<v8::Value> field =
        value->GetInternalField(i).template As<v8::Value>();
    CHECK_EQ(0,
             field->Int32Value(v8::Isolate::GetCurrent()->GetCurrentContext())
                 .FromJust());
  }
}

// -----------------------------------------------------------------------------
// Context and script helpers that work on the isolate entered on the current
// thread.

// A LocalContext holds a reference to a v8::Context and enters it for its
// lifetime.
class LocalContext {
 public:
  LocalContext(v8::Isolate* isolate,
               v8::ExtensionConfiguration* extensions = nullptr,
               v8::Local<v8::ObjectTemplate> global_template =
                   v8::Local<v8::ObjectTemplate>(),
               v8::Local<v8::Value> global_object = v8::Local<v8::Value>())
      : isolate_(isolate) {
    Initialize(extensions, global_template, global_object);
  }

  LocalContext(v8::ExtensionConfiguration* extensions = nullptr,
               v8::Local<v8::ObjectTemplate> global_template =
                   v8::Local<v8::ObjectTemplate>(),
               v8::Local<v8::Value> global_object = v8::Local<v8::Value>())
      : LocalContext(v8::Isolate::GetCurrent(), extensions, global_template,
                     global_object) {}

  virtual ~LocalContext() {
    v8::HandleScope scope(isolate_);
    v8::Local<v8::Context>::New(isolate_, context_)->Exit();
    context_.Reset();
  }

  v8::Context* operator->() { return i::ValueHelper::HandleAsValue(context_); }
  v8::Context* operator*() { return operator->(); }
  bool IsReady() { return !context_.IsEmpty(); }

  v8::Isolate* isolate() const { return isolate_; }
  i::Isolate* i_isolate() const {
    return reinterpret_cast<i::Isolate*>(isolate_);
  }
  v8::Local<v8::Context> local() const {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

 private:
  void Initialize(v8::ExtensionConfiguration* extensions,
                  v8::Local<v8::ObjectTemplate> global_template,
                  v8::Local<v8::Value> global_object) {
    v8::HandleScope scope(isolate_);
    v8::Local<v8::Context> context =
        v8::Context::New(isolate_, extensions, global_template, global_object);
    context_.Reset(isolate_, context);
    context->Enter();
  }

  v8::Persistent<v8::Context> context_;
  v8::Isolate* const isolate_;
};

inline v8::Local<v8::Number> v8_num(double x) {
  return v8::Number::New(v8::Isolate::GetCurrent(), x);
}

inline v8::Local<v8::Integer> v8_int(int32_t x) {
  return v8::Integer::New(v8::Isolate::GetCurrent(), x);
}

inline v8::Local<v8::Integer> v8_uint(uint32_t x) {
  return v8::Integer::NewFromUnsigned(v8::Isolate::GetCurrent(), x);
}

inline v8::Local<v8::String> v8_str(const char* x) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), x).ToLocalChecked();
}

inline v8::Local<v8::String> v8_str(v8::Isolate* isolate, const char* x) {
  return v8::String::NewFromUtf8(isolate, x).ToLocalChecked();
}

inline v8::Local<v8::Symbol> v8_symbol(const char* name) {
  return v8::Symbol::New(v8::Isolate::GetCurrent(), v8_str(name));
}

inline v8::Local<v8::Script> v8_compile(v8::Local<v8::String> x) {
  v8::Local<v8::Script> result;
  CHECK(v8::Script::Compile(v8::Isolate::GetCurrent()->GetCurrentContext(), x)
            .ToLocal(&result));
  return result;
}

inline v8::Local<v8::Script> v8_compile(const char* x) {
  return v8_compile(v8_str(x));
}

inline v8::MaybeLocal<v8::Script> v8_try_compile(v8::Local<v8::String> x) {
  return v8::Script::Compile(v8::Isolate::GetCurrent()->GetCurrentContext(), x);
}

inline v8::MaybeLocal<v8::Script> v8_try_compile(const char* x) {
  return v8_try_compile(v8_str(x));
}

inline v8::Local<v8::Script> CompileWithOrigin(v8::Local<v8::String> source,
                                               v8::Local<v8::String> origin_url,
                                               bool is_shared_cross_origin) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::ScriptOrigin origin(origin_url, 0, 0, is_shared_cross_origin);
  v8::ScriptCompiler::Source script_source(source, origin);
  return v8::ScriptCompiler::Compile(isolate->GetCurrentContext(),
                                     &script_source)
      .ToLocalChecked();
}

inline v8::Local<v8::Script> CompileWithOrigin(v8::Local<v8::String> source,
                                               const char* origin_url,
                                               bool is_shared_cross_origin) {
  return CompileWithOrigin(source, v8_str(origin_url), is_shared_cross_origin);
}

inline v8::Local<v8::Script> CompileWithOrigin(const char* source,
                                               const char* origin_url,
                                               bool is_shared_cross_origin) {
  return CompileWithOrigin(v8_str(source), v8_str(origin_url),
                           is_shared_cross_origin);
}

// Helper functions that compile and run the source.
inline v8::MaybeLocal<v8::Value> CompileRun(v8::Local<v8::Context> context,
                                            const char* source) {
  return v8::Script::Compile(context, v8_str(source))
      .ToLocalChecked()
      ->Run(context);
}

inline v8::Local<v8::Value> CompileRunChecked(v8::Isolate* isolate,
                                              const char* source) {
  v8::Local<v8::String> source_string =
      v8::String::NewFromUtf8(isolate, source).ToLocalChecked();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source_string).ToLocalChecked();
  return script->Run(context).ToLocalChecked();
}

inline v8::Local<v8::Value> CompileRunChecked(const char* source) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return CompileRunChecked(isolate, source);
}

// Unlike RunJS() in test-utils.h, CompileRun() returns an empty handle instead
// of crashing if the script throws.
inline v8::Local<v8::Value> CompileRun(v8::Local<v8::String> source) {
  v8::Local<v8::Value> result;
  if (v8_compile(source)
          ->Run(v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocal(&result)) {
    return result;
  }
  return v8::Local<v8::Value>();
}

inline v8::Local<v8::Value> CompileRun(const char* source) {
  return CompileRun(v8_str(source));
}

inline int32_t v8_run_int32value(v8::Local<v8::Script> script) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  return script->Run(context).ToLocalChecked()->Int32Value(context).FromJust();
}

inline int32_t v8_run_int32(const char* source) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  return CompileRunChecked(isolate, source)->Int32Value(context).FromJust();
}

inline bool v8_run_bool(const char* source) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return CompileRunChecked(isolate, source)->BooleanValue(isolate);
}

inline v8::Local<v8::Value> CompileRun(
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
inline v8::Local<v8::Value> CompileRunWithOrigin(const char* source,
                                                 const char* origin_url,
                                                 int line_number,
                                                 int column_number) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::ScriptOrigin origin(v8_str(origin_url), line_number, column_number);
  v8::ScriptCompiler::Source script_source(v8_str(source), origin);
  return CompileRun(context, &script_source,
                    v8::ScriptCompiler::CompileOptions());
}

inline v8::Local<v8::Value> CompileRunWithOrigin(v8::Local<v8::String> source,
                                                 const char* origin_url) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::ScriptCompiler::Source script_source(
      source, v8::ScriptOrigin(v8_str(origin_url)));
  return CompileRun(context, &script_source,
                    v8::ScriptCompiler::CompileOptions());
}

inline v8::Local<v8::Value> CompileRunWithOrigin(const char* source,
                                                 const char* origin_url) {
  return CompileRunWithOrigin(v8_str(source), origin_url);
}

inline void ExpectString(const char* code, const char* expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsString());
  v8::String::Utf8Value utf8(v8::Isolate::GetCurrent(), result);
  CHECK_EQ(0, strcmp(expected, *utf8));
}

inline void ExpectInt32(const char* code, int expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsInt32());
  CHECK_EQ(expected,
           result->Int32Value(v8::Isolate::GetCurrent()->GetCurrentContext())
               .FromJust());
}

inline void ExpectBoolean(const char* code, bool expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsBoolean());
  CHECK_EQ(expected, result->BooleanValue(v8::Isolate::GetCurrent()));
}

inline void ExpectTrue(const char* code) { ExpectBoolean(code, true); }

inline void ExpectFalse(const char* code) { ExpectBoolean(code, false); }

inline void ExpectObject(const char* code, v8::Local<v8::Value> expected) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->SameValue(expected));
}

inline void ExpectUndefined(const char* code) {
  v8::Local<v8::Value> result = CompileRun(code);
  CHECK(result->IsUndefined());
}

inline void CheckDoubleEquals(double expected, double actual) {
  const double kEpsilon = 1e-10;
  CHECK_LE(expected, actual + kEpsilon);
  CHECK_GE(expected, actual - kEpsilon);
}

inline void EnableDebugger(v8::Isolate* isolate) {
  static v8::debug::DebugDelegate dummy_delegate;
  v8::debug::SetDebugDelegate(isolate, &dummy_delegate);
}

inline void DisableDebugger(v8::Isolate* isolate) {
  v8::debug::SetDebugDelegate(isolate, nullptr);
}

inline void EmptyMessageQueues(v8::Isolate* isolate) {
  while (v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(), isolate)) {
  }
}

// Process-wide ArrayBuffer allocator shared by all isolates that tests create
// themselves (e.g. in TestWithPlatform-based fixtures).
inline v8::ArrayBuffer::Allocator* DefaultArrayBufferAllocator() {
  static v8::ArrayBuffer::Allocator* allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  return allocator;
}

// -----------------------------------------------------------------------------
// Threaded tests.

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
class ApiTestFuzzer : public v8::internal::SandboxableThread {
 public:
  ~ApiTestFuzzer() override = default;

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

  // Starts one fuzzer thread per THREADED_TEST in the given part. All of them
  // share the isolate that is entered on the calling thread.
  static void SetUp(PartOfTest part);
  static void RunAllTests();
  static void TearDown();
  // This method switches threads if we are running the Threading test.
  // Otherwise it does nothing.
  static void Fuzz();

 private:
  explicit ApiTestFuzzer(int num)
      : SandboxableThread(Options("ApiTestFuzzer")),
        test_number_(num),
        gate_(0),
        active_(true) {}

  static bool NextThread();
  void ContextSwitch();
  static int GetNextFuzzer();

  static v8::Isolate* isolate_;
  static unsigned linear_congruential_generator;
  static std::vector<std::unique_ptr<ApiTestFuzzer>> fuzzers_;
  static bool fuzzing_;
  static v8::base::Semaphore all_tests_done_;
  static int tests_being_run_;
  static int active_tests_;
  static int current_fuzzer_;

  int test_number_;
  v8::base::Semaphore gate_;
  bool active_;
};

class RegisterThreadedTest {
 public:
  using TestFunction = void();

  explicit RegisterThreadedTest(TestFunction* callback, const char* name)
      : callback_(callback), name_(name) {
    tests_.push_back(this);
  }
  static int count() { return static_cast<int>(tests_.size()); }
  static const RegisterThreadedTest* nth(int i) {
    DCHECK_LE(0, i);
    DCHECK_LT(i, count());
    // Added tests used to be prepended to a linked list and therefore the last
    // one to be added was at index 0. This ensures that we keep this behavior.
    return tests_[count() - i - 1];
  }
  TestFunction* callback() const { return callback_; }
  const char* name() const { return name_; }

 private:
  static std::vector<const RegisterThreadedTest*> tests_;
  TestFunction* callback_;
  const char* name_;
};

// In threaded tests, control flow alternates between different threads, each
// of which runs a single test. All threaded tests share the same isolate and
// a heap. With conservative stack scanning (CSS), whenever a thread invokes a
// GC for the common heap, the stacks of all threads are scanned. In this
// setting, it is not possible to disable CSS without losing correctness.
// Therefore, tests defined with THREADED_TEST:
//
// 1.  must not explicitly disable CSS, using the scope
//     internal::DisableConservativeStackScanningScopeForTesting, and
// 2.  cannot rely on the assumption that garbage collection will reclaim all
//     non-live objects.
//
// THREADED_TEST(Fixture, Name) defines the regular test Fixture.Name and also
// registers its body for the Threading1..8 tests in api-unittest.cc. The body
// is a free function, so it must not use members of Fixture.
#define THREADED_TEST(Fixture, Name)                                    \
  static void Test##Name();                                             \
  static ::v8::RegisterThreadedTest register_##Name(Test##Name, #Name); \
  TEST_F(Fixture, Name) { Test##Name(); }                               \
  static void Test##Name()

}  // namespace v8

#endif  // V8_UNITTESTS_API_API_TEST_UTILS_H_
