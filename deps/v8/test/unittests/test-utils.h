// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_TEST_UTILS_H_
#define V8_UNITTESTS_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8-array-buffer.h"
#include "include/v8-context.h"
#include "include/v8-extension.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-primitive.h"
#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/base/macros.h"
#include "src/base/utils/random-number-generator.h"
#include "src/handles/handles.h"
#include "src/heap/parked-scope.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "testing/gtest-support.h"

namespace v8 {

class ArrayBufferAllocator;

template <typename TMixin>
class WithDefaultPlatformMixin : public TMixin {
 public:
  WithDefaultPlatformMixin() {
    platform_ = v8::platform::NewDefaultPlatform(
        0, v8::platform::IdleTaskSupport::kEnabled);
    CHECK_NOT_NULL(platform_.get());
    v8::V8::InitializePlatform(platform_.get());
    // Allow changing flags in unit tests.
    // TODO(12887): Fix tests to avoid changing flag values after
    // initialization.
    i::v8_flags.freeze_flags_after_init = false;
    v8::V8::Initialize();
  }

  virtual ~WithDefaultPlatformMixin() {
    CHECK_NOT_NULL(platform_.get());
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
  }

  v8::Platform* platform() const { return platform_.get(); }

 private:
  std::unique_ptr<v8::Platform> platform_;
};

template <typename TMixin>
class WithJSSharedMemoryFeatureFlagsMixin : public TMixin {
 public:
  WithJSSharedMemoryFeatureFlagsMixin() { i::v8_flags.harmony_struct = true; }
};

using CounterMap = std::map<std::string, int>;

enum CountersMode { kNoCounters, kEnableCounters };

// RAII-like Isolate instance wrapper.
//
// It is the caller's responsibility to ensure that the shared Isolate outlives
// all client Isolates.
class IsolateWrapper final {
 public:
  explicit IsolateWrapper(CountersMode counters_mode);

  ~IsolateWrapper();
  IsolateWrapper(const IsolateWrapper&) = delete;
  IsolateWrapper& operator=(const IsolateWrapper&) = delete;

  v8::Isolate* isolate() const { return isolate_; }
  i::Isolate* i_isolate() const {
    return reinterpret_cast<i::Isolate*>(isolate_);
  }

 private:
  std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
  std::unique_ptr<CounterMap> counter_map_;
  v8::Isolate* isolate_;
};

//
// A set of mixins from which the test fixtures will be constructed.
//
template <typename TMixin, CountersMode kCountersMode = kNoCounters>
class WithIsolateMixin : public TMixin {
 public:
  WithIsolateMixin() : isolate_wrapper_(kCountersMode) {}

  v8::Isolate* v8_isolate() const { return isolate_wrapper_.isolate(); }

  Local<Value> RunJS(const char* source, Local<Context> context) {
    return RunJS(
        v8::String::NewFromUtf8(this->v8_isolate(), source).ToLocalChecked(),
        context);
  }

  Local<Value> RunJS(Local<String> source, Local<Context> context) {
    Local<Script> script =
        v8::Script::Compile(context, source).ToLocalChecked();
    return script->Run(context).ToLocalChecked();
  }

 private:
  v8::IsolateWrapper isolate_wrapper_;
};

template <typename TMixin>
class WithIsolateScopeMixin : public TMixin {
 public:
  WithIsolateScopeMixin()
      : isolate_scope_(this->v8_isolate()), handle_scope_(this->v8_isolate()) {}
  WithIsolateScopeMixin(const WithIsolateScopeMixin&) = delete;
  WithIsolateScopeMixin& operator=(const WithIsolateScopeMixin&) = delete;

  v8::Isolate* isolate() const { return this->v8_isolate(); }

  v8::internal::Isolate* i_isolate() const {
    return reinterpret_cast<v8::internal::Isolate*>(this->v8_isolate());
  }

  i::Handle<i::String> MakeName(const char* str, int suffix) {
    v8::base::EmbeddedVector<char, 128> buffer;
    v8::base::SNPrintF(buffer, "%s%d", str, suffix);
    return MakeString(buffer.begin());
  }

  i::Handle<i::String> MakeString(const char* str) {
    i::Factory* factory = i_isolate()->factory();
    return factory->InternalizeUtf8String(str);
  }

  Local<Value> RunJS(const char* source) {
    return RunJS(
        v8::String::NewFromUtf8(this->v8_isolate(), source).ToLocalChecked());
  }

  Local<Value> RunJS(Local<Context> context, const char* source) {
    return RunJS(
        context,
        v8::String::NewFromUtf8(this->v8_isolate(), source).ToLocalChecked());
  }

  MaybeLocal<Value> TryRunJS(const char* source) {
    return TryRunJS(
        v8::String::NewFromUtf8(this->v8_isolate(), source).ToLocalChecked());
  }

  static MaybeLocal<Value> TryRunJS(Isolate* isolate, Local<String> source) {
    auto context = isolate->GetCurrentContext();
    return TryRunJS(context, source);
  }

  static MaybeLocal<Value> TryRunJS(Local<Context> context,
                                    Local<String> source) {
    v8::Local<v8::Value> result;
    Local<Script> script =
        v8::Script::Compile(context, source).ToLocalChecked();
    return script->Run(context);
  }

  Local<Value> RunJS(v8::String::ExternalOneByteStringResource* source) {
    return RunJS(v8::String::NewExternalOneByte(this->v8_isolate(), source)
                     .ToLocalChecked());
  }

  Local<Script> CompileWithOrigin(Local<String> source,
                                  Local<String> origin_url,
                                  bool is_shared_cross_origin) {
    Isolate* isolate = Isolate::GetCurrent();
    ScriptOrigin origin(isolate, origin_url, 0, 0, is_shared_cross_origin);
    ScriptCompiler::Source script_source(source, origin);
    return ScriptCompiler::Compile(isolate->GetCurrentContext(), &script_source)
        .ToLocalChecked();
  }

  void CollectGarbage(i::AllocationSpace space, i::Isolate* isolate = nullptr) {
    i::Isolate* iso = isolate ? isolate : i_isolate();
    iso->heap()->CollectGarbage(space, i::GarbageCollectionReason::kTesting,
                                kNoGCCallbackFlags);
  }

  void CollectAllGarbage(i::Isolate* isolate = nullptr) {
    i::Isolate* iso = isolate ? isolate : i_isolate();
    iso->heap()->CollectAllGarbage(i::Heap::kNoGCFlags,
                                   i::GarbageCollectionReason::kTesting,
                                   kNoGCCallbackFlags);
  }

  void CollectAllAvailableGarbage(i::Isolate* isolate = nullptr) {
    i::Isolate* iso = isolate ? isolate : i_isolate();
    iso->heap()->CollectAllAvailableGarbage(
        i::GarbageCollectionReason::kTesting);
  }

  void PreciseCollectAllGarbage(i::Isolate* isolate = nullptr) {
    i::Isolate* iso = isolate ? isolate : i_isolate();
    iso->heap()->PreciseCollectAllGarbage(i::Heap::kNoGCFlags,
                                          i::GarbageCollectionReason::kTesting,
                                          kNoGCCallbackFlags);
  }

  v8::Local<v8::String> NewString(const char* string) {
    return v8::String::NewFromUtf8(this->v8_isolate(), string).ToLocalChecked();
  }

  void EmptyMessageQueues() {
    while (v8::platform::PumpMessageLoop(internal::V8::GetCurrentPlatform(),
                                         this->v8_isolate())) {
    }
  }

  void ExpectString(const char* code, const char* expected) {
    v8::Local<v8::Value> result = RunJS(code);
    CHECK(result->IsString());
    v8::String::Utf8Value utf8(v8::Isolate::GetCurrent(), result);
    CHECK_EQ(0, strcmp(expected, *utf8));
  }

 private:
  Local<Value> RunJS(Local<String> source) {
    return TryRunJS(source).ToLocalChecked();
  }

  Local<Value> RunJS(Local<Context> context, Local<String> source) {
    return TryRunJS(context, source).ToLocalChecked();
  }

  MaybeLocal<Value> TryRunJS(Local<String> source) {
    return TryRunJS(this->v8_isolate(), source);
  }

  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
};

template <typename TMixin>
class WithContextMixin : public TMixin {
 public:
  WithContextMixin()
      : context_(Context::New(this->v8_isolate())), context_scope_(context_) {}
  WithContextMixin(const WithContextMixin&) = delete;
  WithContextMixin& operator=(const WithContextMixin&) = delete;

  const Local<Context>& context() const { return v8_context(); }
  const Local<Context>& v8_context() const { return context_; }

  void SetGlobalProperty(const char* name, v8::Local<v8::Value> value) {
    CHECK(v8_context()
              ->Global()
              ->Set(v8_context(), TMixin::NewString(name), value)
              .FromJust());
  }

 private:
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

using TestWithPlatform =       //
    WithDefaultPlatformMixin<  //
        ::testing::Test>;

// Use v8::internal::TestWithIsolate if you are testing internals,
// aka. directly work with Handles.
using TestWithIsolate =                //
    WithIsolateScopeMixin<             //
        WithIsolateMixin<              //
            WithDefaultPlatformMixin<  //
                ::testing::Test>>>;

// Use v8::internal::TestWithNativeContext if you are testing internals,
// aka. directly work with Handles.
using TestWithContext =                    //
    WithContextMixin<                      //
        WithIsolateScopeMixin<             //
            WithIsolateMixin<              //
                WithDefaultPlatformMixin<  //
                    ::testing::Test>>>>;

class PrintExtension : public v8::Extension {
 public:
  PrintExtension() : v8::Extension("v8/print", "native function print();") {}
  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) override {
    return v8::FunctionTemplate::New(isolate, PrintExtension::Print);
  }
  static void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
    for (int i = 0; i < args.Length(); i++) {
      if (i != 0) printf(" ");
      v8::HandleScope scope(args.GetIsolate());
      v8::String::Utf8Value str(args.GetIsolate(), args[i]);
      if (*str == nullptr) return;
      printf("%s", *str);
    }
    printf("\n");
  }
};

template <typename TMixin>
class WithPrintExtensionMixin : public TMixin {
 public:
  WithPrintExtensionMixin() = default;
  ~WithPrintExtensionMixin() override = default;
  WithPrintExtensionMixin(const WithPrintExtensionMixin&) = delete;
  WithPrintExtensionMixin& operator=(const WithPrintExtensionMixin&) = delete;

  static void SetUpTestSuite() {
    v8::RegisterExtension(std::make_unique<PrintExtension>());
    TMixin::SetUpTestSuite();
  }

  static void TearDownTestSuite() { TMixin::TearDownTestSuite(); }

  static constexpr const char* kPrintExtensionName = "v8/print";
};

// Run a ScriptStreamingTask in a separate thread.
class StreamerThread : public v8::base::Thread {
 public:
  static void StartThreadForTaskAndJoin(
      v8::ScriptCompiler::ScriptStreamingTask* task) {
    StreamerThread thread(task);
    CHECK(thread.Start());
    thread.Join();
  }

  explicit StreamerThread(v8::ScriptCompiler::ScriptStreamingTask* task)
      : Thread(Thread::Options()), task_(task) {}

  void Run() override { task_->Run(); }

 private:
  v8::ScriptCompiler::ScriptStreamingTask* task_;
};

namespace internal {

// Forward declarations.
class Factory;

template <typename TMixin>
class WithInternalIsolateMixin : public TMixin {
 public:
  WithInternalIsolateMixin() = default;
  WithInternalIsolateMixin(const WithInternalIsolateMixin&) = delete;
  WithInternalIsolateMixin& operator=(const WithInternalIsolateMixin&) = delete;

  Factory* factory() const { return isolate()->factory(); }
  Isolate* isolate() const { return TMixin::i_isolate(); }

  Handle<NativeContext> native_context() const {
    return isolate()->native_context();
  }

  template <typename T = Object>
  Handle<T> RunJS(const char* source) {
    return Handle<T>::cast(RunJSInternal(source));
  }

  Handle<Object> RunJSInternal(const char* source) {
    return Utils::OpenHandle(*TMixin::RunJS(source));
  }

  template <typename T = Object>
  Handle<T> RunJS(::v8::String::ExternalOneByteStringResource* source) {
    return Handle<T>::cast(RunJSInternal(source));
  }

  Handle<Object> RunJSInternal(
      ::v8::String::ExternalOneByteStringResource* source) {
    return Utils::OpenHandle(*TMixin::RunJS(source));
  }

  base::RandomNumberGenerator* random_number_generator() const {
    return isolate()->random_number_generator();
  }
};

template <typename TMixin>
class WithZoneMixin : public TMixin {
 public:
  explicit WithZoneMixin(bool support_zone_compression = false)
      : zone_(&allocator_, ZONE_NAME, support_zone_compression) {}
  WithZoneMixin(const WithZoneMixin&) = delete;
  WithZoneMixin& operator=(const WithZoneMixin&) = delete;

  Zone* zone() { return &zone_; }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;
};

using TestWithIsolate =                    //
    WithInternalIsolateMixin<              //
        WithIsolateScopeMixin<             //
            WithIsolateMixin<              //
                WithDefaultPlatformMixin<  //
                    ::testing::Test>>>>;

using TestWithZone = WithZoneMixin<WithDefaultPlatformMixin<  //
    ::testing::Test>>;

using TestWithIsolateAndZone =                 //
    WithZoneMixin<                             //
        WithInternalIsolateMixin<              //
            WithIsolateScopeMixin<             //
                WithIsolateMixin<              //
                    WithDefaultPlatformMixin<  //
                        ::testing::Test>>>>>;

using TestWithContextAndZone =                 //
    WithZoneMixin<                             //
        WithContextMixin<                      //
            WithIsolateScopeMixin<             //
                WithIsolateMixin<              //
                    WithDefaultPlatformMixin<  //
                        ::testing::Test>>>>>;

using TestWithNativeContext =                  //
    WithInternalIsolateMixin<                  //
        WithContextMixin<                      //
            WithIsolateScopeMixin<             //
                WithIsolateMixin<              //
                    WithDefaultPlatformMixin<  //
                        ::testing::Test>>>>>;

using TestWithNativeContextAndCounters =       //
    WithInternalIsolateMixin<                  //
        WithContextMixin<                      //
            WithIsolateScopeMixin<             //
                WithIsolateMixin<              //
                    WithDefaultPlatformMixin<  //
                        ::testing::Test>,
                    kEnableCounters>>>>;

using TestWithNativeContextAndZone =               //
    WithZoneMixin<                                 //
        WithInternalIsolateMixin<                  //
            WithContextMixin<                      //
                WithIsolateScopeMixin<             //
                    WithIsolateMixin<              //
                        WithDefaultPlatformMixin<  //
                            ::testing::Test>>>>>>;

using TestJSSharedMemoryWithPlatform =        //
    WithDefaultPlatformMixin<                 //
        WithJSSharedMemoryFeatureFlagsMixin<  //
            ::testing::Test>>;

// Using this will FATAL when !V8_CAN_CREATE_SHARED_HEAP_BOOL
using TestJSSharedMemoryWithIsolate =  //
    WithInternalIsolateMixin<          //
        WithIsolateScopeMixin<         //
            WithIsolateMixin<          //
                TestJSSharedMemoryWithPlatform>>>;

// Using this will FATAL when !V8_CAN_CREATE_SHARED_HEAP_BOOL
using TestJSSharedMemoryWithNativeContext =  //
    WithInternalIsolateMixin<                //
        WithContextMixin<                    //
            WithIsolateScopeMixin<           //
                WithIsolateMixin<            //
                    TestJSSharedMemoryWithPlatform>>>>;

class V8_NODISCARD SaveFlags {
 public:
  SaveFlags();
  ~SaveFlags();
  SaveFlags(const SaveFlags&) = delete;
  SaveFlags& operator=(const SaveFlags&) = delete;

 private:
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) ctype SAVED_##nam;
#include "src/flags/flag-definitions.h"
#undef FLAG_MODE_APPLY
};

// For GTest.
inline void PrintTo(Object o, ::std::ostream* os) {
  *os << reinterpret_cast<void*>(o.ptr());
}
inline void PrintTo(Smi o, ::std::ostream* os) {
  *os << reinterpret_cast<void*>(o.ptr());
}

// ManualGCScope allows for disabling GC heuristics. This is useful for tests
// that want to check specific corner cases around GC.
//
// The scope will finalize any ongoing GC on the provided Isolate.
class V8_NODISCARD ManualGCScope final : private SaveFlags {
 public:
  explicit ManualGCScope(i::Isolate* isolate);
  ~ManualGCScope() = default;
};

static inline uint16_t* AsciiToTwoByteString(const char* source) {
  size_t array_length = strlen(source) + 1;
  uint16_t* converted = NewArray<uint16_t>(array_length);
  for (size_t i = 0; i < array_length; i++) converted[i] = source[i];
  return converted;
}

class TestTransitionsAccessor : public TransitionsAccessor {
 public:
  TestTransitionsAccessor(Isolate* isolate, Map map)
      : TransitionsAccessor(isolate, map) {}
  TestTransitionsAccessor(Isolate* isolate, Handle<Map> map)
      : TransitionsAccessor(isolate, *map) {}

  // Expose internals for tests.
  bool IsUninitializedEncoding() { return encoding() == kUninitialized; }
  bool IsWeakRefEncoding() { return encoding() == kWeakRef; }

  bool IsFullTransitionArrayEncoding() {
    return encoding() == kFullTransitionArray;
  }

  int Capacity() { return TransitionsAccessor::Capacity(); }

  TransitionArray transitions() { return TransitionsAccessor::transitions(); }
};

// Helper class that allows to write tests in a slot size independent manner.
// Use helper.slot(X) to get X'th slot identifier.
class FeedbackVectorHelper {
 public:
  explicit FeedbackVectorHelper(Handle<FeedbackVector> vector)
      : vector_(vector) {
    int slot_count = vector->length();
    slots_.reserve(slot_count);
    FeedbackMetadataIterator iter(vector->metadata());
    while (iter.HasNext()) {
      FeedbackSlot slot = iter.Next();
      slots_.push_back(slot);
    }
  }

  Handle<FeedbackVector> vector() { return vector_; }

  // Returns slot identifier by numerical index.
  FeedbackSlot slot(int index) const { return slots_[index]; }

  // Returns the number of slots in the feedback vector.
  int slot_count() const { return static_cast<int>(slots_.size()); }

 private:
  Handle<FeedbackVector> vector_;
  std::vector<FeedbackSlot> slots_;
};

template <typename Spec>
Handle<FeedbackVector> NewFeedbackVector(Isolate* isolate, Spec* spec) {
  Handle<FeedbackMetadata> metadata = FeedbackMetadata::New(isolate, spec);
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->empty_string(), Builtin::kIllegal);
  // Set the raw feedback metadata to circumvent checks that we are not
  // overwriting existing metadata.
  shared->set_raw_outer_scope_info_or_feedback_metadata(*metadata);
  Handle<ClosureFeedbackCellArray> closure_feedback_cell_array =
      ClosureFeedbackCellArray::New(isolate, shared);
  IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate));
  return FeedbackVector::New(isolate, shared, closure_feedback_cell_array,
                             &is_compiled_scope);
}

class ParkingThread : public v8::base::Thread {
 public:
  explicit ParkingThread(const Options& options) : v8::base::Thread(options) {}

  void ParkedJoin(const ParkedScope& scope) {
    USE(scope);
    Join();
  }

 private:
  using v8::base::Thread::Join;
};

#ifdef V8_CC_GNU

#if V8_HOST_ARCH_X64
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("mov %%rsp, %0" : "=g"(sp_addr))
#elif V8_HOST_ARCH_IA32
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("mov %%esp, %0" : "=g"(sp_addr))
#elif V8_HOST_ARCH_ARM
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("str sp, %0" : "=g"(sp_addr))
#elif V8_HOST_ARCH_ARM64
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("mov x16, sp; str x16, %0" : "=g"(sp_addr))
#elif V8_HOST_ARCH_MIPS
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("sw $sp, %0" : "=g"(sp_addr))
#elif V8_HOST_ARCH_MIPS64
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("sd $sp, %0" : "=g"(sp_addr))
#elif defined(__s390x__) || defined(_ARCH_S390X)
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("stg %%r15, %0" : "=m"(sp_addr))
#elif defined(__s390__) || defined(_ARCH_S390)
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("st 15, %0" : "=m"(sp_addr))
#elif defined(__PPC64__) || defined(_ARCH_PPC64)
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("std 1, %0" : "=m"(sp_addr))
#elif defined(__PPC__) || defined(_ARCH_PPC)
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("stw 1, %0" : "=m"(sp_addr))
#elif V8_TARGET_ARCH_RISCV64
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("add %0, sp, x0" : "=r"(sp_addr))
#elif V8_HOST_ARCH_LOONG64
#define GET_STACK_POINTER_TO(sp_addr) \
  __asm__ __volatile__("st.d $sp, %0" : "=m"(sp_addr))
#else
#error Host architecture was not detected as supported by v8
#endif

#endif  // V8_CC_GNU

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_TEST_UTILS_H_
