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

#include "v8.h"

#ifndef TEST
#define TEST(Name)                                                             \
  static void Test##Name();                                                    \
  CcTest register_test_##Name(Test##Name, __FILE__, #Name, NULL, true, true);  \
  static void Test##Name()
#endif

#ifndef UNINITIALIZED_TEST
#define UNINITIALIZED_TEST(Name)                                               \
  static void Test##Name();                                                    \
  CcTest register_test_##Name(Test##Name, __FILE__, #Name, NULL, true, false); \
  static void Test##Name()
#endif

#ifndef DEPENDENT_TEST
#define DEPENDENT_TEST(Name, Dep)                                              \
  static void Test##Name();                                                    \
  CcTest register_test_##Name(Test##Name, __FILE__, #Name, #Dep, true, true);  \
  static void Test##Name()
#endif

#ifndef DISABLED_TEST
#define DISABLED_TEST(Name)                                                    \
  static void Test##Name();                                                    \
  CcTest register_test_##Name(Test##Name, __FILE__, #Name, NULL, false, true); \
  static void Test##Name()
#endif

#define EXTENSION_LIST(V)                                                \
  V(GC_EXTENSION,    "v8/gc")                                            \
  V(PRINT_EXTENSION, "v8/print")                                         \
  V(TRACE_EXTENSION, "v8/trace")

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
         const char* dependency, bool enabled, bool initialize);
  void Run();
  static CcTest* last() { return last_; }
  CcTest* prev() { return prev_; }
  const char* file() { return file_; }
  const char* name() { return name_; }
  const char* dependency() { return dependency_; }
  bool enabled() { return enabled_; }

  static v8::Isolate* isolate() {
    CHECK(isolate_ != NULL);
    isolate_used_ = true;
    return isolate_;
  }

  static i::Isolate* i_isolate() {
    return reinterpret_cast<i::Isolate*>(isolate());
  }

  static i::Heap* heap() {
    return i_isolate()->heap();
  }

  static v8::Local<v8::Object> global() {
    return isolate()->GetCurrentContext()->Global();
  }

  // TODO(dcarney): Remove.
  // This must be called first in a test.
  static void InitializeVM() {
    CHECK(!isolate_used_);
    CHECK(!initialize_called_);
    initialize_called_ = true;
    v8::HandleScope handle_scope(CcTest::isolate());
    v8::Context::New(CcTest::isolate())->Enter();
  }

  // Only for UNINITIALIZED_TESTs
  static void DisableAutomaticDispose();

  // Helper function to configure a context.
  // Must be in a HandleScope.
  static v8::Local<v8::Context> NewContext(
      CcTestExtensionFlags extensions,
      v8::Isolate* isolate = CcTest::isolate());

 private:
  friend int main(int argc, char** argv);
  TestFunction* callback_;
  const char* file_;
  const char* name_;
  const char* dependency_;
  bool enabled_;
  bool initialize_;
  CcTest* prev_;
  static CcTest* last_;
  static v8::Isolate* isolate_;
  static bool initialize_called_;
  static bool isolate_used_;
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
class ApiTestFuzzer: public v8::internal::Thread {
 public:
  void CallTest();

  // The ApiTestFuzzer is also a Thread, so it has a Run method.
  virtual void Run();

  enum PartOfTest { FIRST_PART,
                    SECOND_PART,
                    THIRD_PART,
                    FOURTH_PART,
                    LAST_PART = FOURTH_PART };

  static void SetUp(PartOfTest part);
  static void RunAllTests();
  static void TearDown();
  // This method switches threads if we are running the Threading test.
  // Otherwise it does nothing.
  static void Fuzz();

 private:
  explicit ApiTestFuzzer(int num)
      : Thread("ApiTestFuzzer"),
        test_number_(num),
        gate_(0),
        active_(true) {
  }
  ~ApiTestFuzzer() {}

  static bool fuzzing_;
  static int tests_being_run_;
  static int current_;
  static int active_tests_;
  static bool NextThread();
  int test_number_;
  v8::internal::Semaphore gate_;
  bool active_;
  void ContextSwitch();
  static int GetNextTestNumber();
  static v8::internal::Semaphore all_tests_done_;
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
  LocalContext(v8::Isolate* isolate,
               v8::ExtensionConfiguration* extensions = 0,
               v8::Handle<v8::ObjectTemplate> global_template =
                   v8::Handle<v8::ObjectTemplate>(),
               v8::Handle<v8::Value> global_object = v8::Handle<v8::Value>()) {
    Initialize(isolate, extensions, global_template, global_object);
  }

  LocalContext(v8::ExtensionConfiguration* extensions = 0,
               v8::Handle<v8::ObjectTemplate> global_template =
                   v8::Handle<v8::ObjectTemplate>(),
               v8::Handle<v8::Value> global_object = v8::Handle<v8::Value>()) {
    Initialize(CcTest::isolate(), extensions, global_template, global_object);
  }

  virtual ~LocalContext() {
    v8::HandleScope scope(isolate_);
    v8::Local<v8::Context>::New(isolate_, context_)->Exit();
    context_.Dispose();
  }

  v8::Context* operator->() {
    return *reinterpret_cast<v8::Context**>(&context_);
  }
  v8::Context* operator*() { return operator->(); }
  bool IsReady() { return !context_.IsEmpty(); }

  v8::Local<v8::Context> local() {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

 private:
  void Initialize(v8::Isolate* isolate,
                  v8::ExtensionConfiguration* extensions,
                  v8::Handle<v8::ObjectTemplate> global_template,
                  v8::Handle<v8::Value> global_object) {
     v8::HandleScope scope(isolate);
     v8::Local<v8::Context> context = v8::Context::New(isolate,
                                                       extensions,
                                                       global_template,
                                                       global_object);
     context_.Reset(isolate, context);
     context->Enter();
     // We can't do this later perhaps because of a fatal error.
     isolate_ = isolate;
  }

  v8::Persistent<v8::Context> context_;
  v8::Isolate* isolate_;
};

static inline v8::Local<v8::Value> v8_num(double x) {
  return v8::Number::New(x);
}


static inline v8::Local<v8::String> v8_str(const char* x) {
  return v8::String::New(x);
}


static inline v8::Local<v8::Script> v8_compile(const char* x) {
  return v8::Script::Compile(v8_str(x));
}


// Helper function that compiles and runs the source.
static inline v8::Local<v8::Value> CompileRun(const char* source) {
  return v8::Script::Compile(v8::String::New(source))->Run();
}


// Helper function that compiles and runs the source with given origin.
static inline v8::Local<v8::Value> CompileRunWithOrigin(const char* source,
                                                        const char* origin_url,
                                                        int line_number,
                                                        int column_number) {
  v8::ScriptOrigin origin(v8::String::New(origin_url),
                          v8::Integer::New(line_number),
                          v8::Integer::New(column_number));
  return v8::Script::Compile(v8::String::New(source), &origin)->Run();
}


// Pick a slightly different port to allow tests to be run in parallel.
static inline int FlagDependentPortOffset() {
  return ::v8::internal::FLAG_crankshaft == false ? 100 :
         ::v8::internal::FLAG_always_opt ? 200 : 0;
}


// Helper function that simulates a full new-space in the heap.
static inline void SimulateFullSpace(v8::internal::NewSpace* space) {
  int new_linear_size = static_cast<int>(
      *space->allocation_limit_address() - *space->allocation_top_address());
  v8::internal::MaybeObject* maybe = space->AllocateRaw(new_linear_size);
  v8::internal::FreeListNode* node = v8::internal::FreeListNode::cast(maybe);
  node->set_size(space->heap(), new_linear_size);
}


// Helper function that simulates a full old-space in the heap.
static inline void SimulateFullSpace(v8::internal::PagedSpace* space) {
  int old_linear_size = static_cast<int>(space->limit() - space->top());
  space->Free(space->top(), old_linear_size);
  space->SetTop(space->limit(), space->limit());
  space->ResetFreeList();
  space->ClearStats();
}


// Helper class for new allocations tracking and checking.
// To use checking of JS allocations tracking in a test,
// just create an instance of this class.
class HeapObjectsTracker {
 public:
  HeapObjectsTracker() {
    heap_profiler_ = i::Isolate::Current()->heap_profiler();
    CHECK_NE(NULL, heap_profiler_);
    heap_profiler_->StartHeapAllocationsRecording();
  }

  ~HeapObjectsTracker() {
    i::Isolate::Current()->heap()->CollectAllAvailableGarbage();
    CHECK_EQ(0, heap_profiler_->FindUntrackedObjects());
    heap_profiler_->StopHeapAllocationsRecording();
  }

 private:
  i::HeapProfiler* heap_profiler_;
};


#endif  // ifndef CCTEST_H_
