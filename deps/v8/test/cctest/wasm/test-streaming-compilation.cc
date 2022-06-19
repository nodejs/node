// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/libplatform/libplatform.h"
#include "src/api/api-inl.h"
#include "src/base/vector.h"
#include "src/init/v8.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-serialization.h"
#include "test/cctest/cctest.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {

class MockPlatform final : public TestPlatform {
 public:
  MockPlatform() : task_runner_(std::make_shared<MockTaskRunner>()) {}

  ~MockPlatform() {
    for (auto* job_handle : job_handles_) job_handle->ResetPlatform();
  }

  std::unique_ptr<v8::JobHandle> PostJob(
      v8::TaskPriority priority,
      std::unique_ptr<v8::JobTask> job_task) override {
    auto orig_job_handle = v8::platform::NewDefaultJobHandle(
        this, priority, std::move(job_task), 1);
    auto job_handle =
        std::make_unique<MockJobHandle>(std::move(orig_job_handle), this);
    job_handles_.insert(job_handle.get());
    return job_handle;
  }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return task_runner_;
  }

  void CallOnWorkerThread(std::unique_ptr<v8::Task> task) override {
    task_runner_->PostTask(std::move(task));
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  void ExecuteTasks() { task_runner_->ExecuteTasks(); }

 private:
  class MockTaskRunner final : public TaskRunner {
   public:
    void PostTask(std::unique_ptr<v8::Task> task) override {
      base::MutexGuard lock_scope(&tasks_lock_);
      tasks_.push(std::move(task));
    }

    void PostNonNestableTask(std::unique_ptr<Task> task) override {
      PostTask(std::move(task));
    }

    void PostDelayedTask(std::unique_ptr<Task> task,
                         double delay_in_seconds) override {
      PostTask(std::move(task));
    }

    void PostNonNestableDelayedTask(std::unique_ptr<Task> task,
                                    double delay_in_seconds) override {
      PostTask(std::move(task));
    }

    void PostIdleTask(std::unique_ptr<IdleTask> task) override {
      UNREACHABLE();
    }

    bool IdleTasksEnabled() override { return false; }
    bool NonNestableTasksEnabled() const override { return true; }
    bool NonNestableDelayedTasksEnabled() const override { return true; }

    void ExecuteTasks() {
      std::queue<std::unique_ptr<v8::Task>> tasks;
      while (true) {
        {
          base::MutexGuard lock_scope(&tasks_lock_);
          tasks.swap(tasks_);
        }
        if (tasks.empty()) break;
        while (!tasks.empty()) {
          std::unique_ptr<Task> task = std::move(tasks.front());
          tasks.pop();
          task->Run();
        }
      }
    }

   private:
    base::Mutex tasks_lock_;
    // We do not execute tasks concurrently, so we only need one list of tasks.
    std::queue<std::unique_ptr<v8::Task>> tasks_;
  };

  class MockJobHandle : public JobHandle {
   public:
    explicit MockJobHandle(std::unique_ptr<JobHandle> orig_handle,
                           MockPlatform* platform)
        : orig_handle_(std::move(orig_handle)), platform_(platform) {}

    ~MockJobHandle() {
      if (platform_) platform_->job_handles_.erase(this);
    }

    void ResetPlatform() { platform_ = nullptr; }

    void NotifyConcurrencyIncrease() override {
      orig_handle_->NotifyConcurrencyIncrease();
    }
    void Join() override { orig_handle_->Join(); }
    void Cancel() override { orig_handle_->Cancel(); }
    void CancelAndDetach() override { orig_handle_->CancelAndDetach(); }
    bool IsActive() override { return orig_handle_->IsActive(); }
    bool IsValid() override { return orig_handle_->IsValid(); }

   private:
    std::unique_ptr<JobHandle> orig_handle_;
    MockPlatform* platform_;
  };

  std::shared_ptr<MockTaskRunner> task_runner_;
  std::unordered_set<MockJobHandle*> job_handles_;
};

namespace {

enum class CompilationState {
  kPending,
  kFinished,
  kFailed,
};

class TestResolver : public CompilationResultResolver {
 public:
  TestResolver(i::Isolate* isolate, CompilationState* state,
               std::string* error_message,
               std::shared_ptr<NativeModule>* native_module)
      : isolate_(isolate),
        state_(state),
        error_message_(error_message),
        native_module_(native_module) {}

  void OnCompilationSucceeded(i::Handle<i::WasmModuleObject> module) override {
    *state_ = CompilationState::kFinished;
    if (!module.is_null()) {
      *native_module_ = module->shared_native_module();
    }
  }

  void OnCompilationFailed(i::Handle<i::Object> error_reason) override {
    *state_ = CompilationState::kFailed;
    Handle<String> str =
        Object::ToString(isolate_, error_reason).ToHandleChecked();
    error_message_->assign(str->ToCString().get());
  }

 private:
  i::Isolate* isolate_;
  CompilationState* const state_;
  std::string* const error_message_;
  std::shared_ptr<NativeModule>* const native_module_;
};

class StreamTester {
 public:
  explicit StreamTester(v8::Isolate* isolate)
      : zone_(&allocator_, "StreamTester"),
        internal_scope_(reinterpret_cast<i::Isolate*>(isolate)) {
    Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    stream_ = GetWasmEngine()->StartStreamingCompilation(
        i_isolate, WasmFeatures::All(), v8::Utils::OpenHandle(*context),
        "WebAssembly.compileStreaming()",
        std::make_shared<TestResolver>(i_isolate, &state_, &error_message_,
                                       &native_module_));
  }

  std::shared_ptr<StreamingDecoder> stream() { return stream_; }

  // Compiled native module, valid after successful compile.
  std::shared_ptr<NativeModule> native_module() { return native_module_; }

  // Run all compiler tasks, both foreground and background tasks.
  void RunCompilerTasks() {
    static_cast<MockPlatform*>(i::V8::GetCurrentPlatform())->ExecuteTasks();
  }

  bool IsPromiseFulfilled() { return state_ == CompilationState::kFinished; }

  bool IsPromiseRejected() { return state_ == CompilationState::kFailed; }

  bool IsPromisePending() { return state_ == CompilationState::kPending; }

  void OnBytesReceived(const uint8_t* start, size_t length) {
    stream_->OnBytesReceived(base::Vector<const uint8_t>(start, length));
  }

  void FinishStream() { stream_->Finish(); }

  void SetCompiledModuleBytes(const uint8_t* start, size_t length) {
    stream_->SetCompiledModuleBytes(base::Vector<const uint8_t>(start, length));
  }

  Zone* zone() { return &zone_; }

  const std::string& error_message() const { return error_message_; }

 private:
  AccountingAllocator allocator_;
  Zone zone_;
  i::HandleScope internal_scope_;
  CompilationState state_ = CompilationState::kPending;
  std::string error_message_;
  std::shared_ptr<NativeModule> native_module_;
  std::shared_ptr<StreamingDecoder> stream_;
};
}  // namespace

#define RUN_STREAM(name)                                      \
  v8::Isolate* isolate = CcTest::isolate();                   \
  v8::HandleScope handle_scope(isolate);                      \
  v8::Local<v8::Context> context = v8::Context::New(isolate); \
  v8::Context::Scope context_scope(context);                  \
  RunStream_##name(&platform, isolate);

#define STREAM_TEST(name)                                                     \
  void RunStream_##name(MockPlatform*, v8::Isolate*);                         \
  TEST_WITH_PLATFORM(Async##name, MockPlatform) { RUN_STREAM(name); }         \
                                                                              \
  TEST_WITH_PLATFORM(SingleThreaded##name, MockPlatform) {                    \
    i::FlagScope<bool> single_threaded_scope(&i::FLAG_single_threaded, true); \
    RUN_STREAM(name);                                                         \
  }                                                                           \
  void RunStream_##name(MockPlatform* platform, v8::Isolate* isolate)

// Create a valid module with 3 functions.
ZoneBuffer GetValidModuleBytes(Zone* zone) {
  ZoneBuffer buffer(zone);
  TestSignatures sigs;
  WasmModuleBuilder builder(zone);
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprLocalGet, 0, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprLocalGet, 1, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprLocalGet, 2, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  builder.WriteTo(&buffer);
  return buffer;
}

// Create the same valid module as above and serialize it to test streaming
// with compiled module caching.
ZoneBuffer GetValidCompiledModuleBytes(v8::Isolate* isolate, Zone* zone,
                                       ZoneBuffer wire_bytes) {
  // Use a tester to compile to a NativeModule.
  StreamTester tester(isolate);
  tester.OnBytesReceived(wire_bytes.begin(), wire_bytes.size());
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseFulfilled());
  // Serialize the NativeModule.
  std::shared_ptr<NativeModule> native_module = tester.native_module();
  CHECK(native_module);
  native_module->compilation_state()->WaitForTopTierFinished();
  i::wasm::WasmSerializer serializer(native_module.get());
  size_t size = serializer.GetSerializedNativeModuleSize();
  std::vector<byte> buffer(size);
  CHECK(serializer.SerializeNativeModule(base::VectorOf(buffer)));
  ZoneBuffer result(zone, size);
  result.write(buffer.data(), size);
  return result;
}

// Test that all bytes arrive before doing any compilation. FinishStream is
// called immediately.
STREAM_TEST(TestAllBytesArriveImmediatelyStreamFinishesFirst) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetValidModuleBytes(tester.zone());

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

// Test that all bytes arrive before doing any compilation. FinishStream is
// called after the compilation is done.
STREAM_TEST(TestAllBytesArriveAOTCompilerFinishesFirst) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetValidModuleBytes(tester.zone());

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());

  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

size_t GetFunctionOffset(i::Isolate* isolate, const uint8_t* buffer,
                         size_t size, size_t index) {
  ModuleResult result = DecodeWasmModule(
      WasmFeatures::All(), buffer, buffer + size, false,
      ModuleOrigin::kWasmOrigin, isolate->counters(),
      isolate->metrics_recorder(), v8::metrics::Recorder::ContextId::Empty(),
      DecodingMethod::kSyncStream, GetWasmEngine()->allocator());
  CHECK(result.ok());
  const WasmFunction* func = &result.value()->functions[index];
  return func->code.offset();
}

// Test that some functions come in the beginning, some come after some
// functions already got compiled.
STREAM_TEST(TestCutAfterOneFunctionStreamFinishesFirst) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetValidModuleBytes(tester.zone());

  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  size_t offset =
      GetFunctionOffset(i_isolate, buffer.begin(), buffer.size(), 1);
  tester.OnBytesReceived(buffer.begin(), offset);
  tester.RunCompilerTasks();
  CHECK(tester.IsPromisePending());
  tester.OnBytesReceived(buffer.begin() + offset, buffer.size() - offset);
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseFulfilled());
}

// Test that some functions come in the beginning, some come after some
// functions already got compiled. Call FinishStream after the compilation is
// done.
STREAM_TEST(TestCutAfterOneFunctionCompilerFinishesFirst) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetValidModuleBytes(tester.zone());

  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  size_t offset =
      GetFunctionOffset(i_isolate, buffer.begin(), buffer.size(), 1);
  tester.OnBytesReceived(buffer.begin(), offset);
  tester.RunCompilerTasks();
  CHECK(tester.IsPromisePending());
  tester.OnBytesReceived(buffer.begin() + offset, buffer.size() - offset);
  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseFulfilled());
}

// Create a module with an invalid global section.
ZoneBuffer GetModuleWithInvalidSection(Zone* zone) {
  ZoneBuffer buffer(zone);
  TestSignatures sigs;
  WasmModuleBuilder builder(zone);
  // Add an invalid global to the module. The decoder will fail there.
  builder.AddGlobal(kWasmVoid, true, WasmInitExpr::GlobalGet(12));
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprLocalGet, 0, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprLocalGet, 1, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprLocalGet, 2, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  builder.WriteTo(&buffer);
  return buffer;
}

// Test an error in a section, found by the ModuleDecoder.
STREAM_TEST(TestErrorInSectionStreamFinishesFirst) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetModuleWithInvalidSection(tester.zone());

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestErrorInSectionCompilerFinishesFirst) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetModuleWithInvalidSection(tester.zone());

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestErrorInSectionWithCuts) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetModuleWithInvalidSection(tester.zone());

  const uint8_t* current = buffer.begin();
  size_t remaining = buffer.end() - buffer.begin();
  while (current < buffer.end()) {
    size_t size = std::min(remaining, size_t{10});
    tester.OnBytesReceived(current, size);
    tester.RunCompilerTasks();
    current += 10;
    remaining -= size;
  }
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

ZoneBuffer GetModuleWithInvalidSectionSize(Zone* zone) {
  // We get a valid module and overwrite the size of the first section with an
  // invalid value.
  ZoneBuffer buffer = GetValidModuleBytes(zone);
  // 9 == 4 (wasm magic) + 4 (version) + 1 (section code)
  uint8_t* section_size_address = const_cast<uint8_t*>(buffer.begin()) + 9;
  // 0x808080800F is an invalid module size in leb encoding.
  section_size_address[0] = 0x80;
  section_size_address[1] = 0x80;
  section_size_address[2] = 0x80;
  section_size_address[3] = 0x80;
  section_size_address[4] = 0x0F;
  return buffer;
}

STREAM_TEST(TestErrorInSectionSizeStreamFinishesFirst) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetModuleWithInvalidSectionSize(tester.zone());
  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestErrorInSectionSizeCompilerFinishesFirst) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetModuleWithInvalidSectionSize(tester.zone());
  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestErrorInSectionSizeWithCuts) {
  StreamTester tester(isolate);
  ZoneBuffer buffer = GetModuleWithInvalidSectionSize(tester.zone());
  const uint8_t* current = buffer.begin();
  size_t remaining = buffer.end() - buffer.begin();
  while (current < buffer.end()) {
    size_t size = std::min(remaining, size_t{10});
    tester.OnBytesReceived(current, size);
    tester.RunCompilerTasks();
    current += 10;
    remaining -= size;
  }
  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

// Test an error in the code section, found by the ModuleDecoder. The error is a
// functions count in the code section which differs from the functions count in
// the function section.
STREAM_TEST(TestErrorInCodeSectionDetectedByModuleDecoder) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 2),    // section size
      U32V_1(2),                          // !!! invalid function count !!!
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

// Test an error in the code section, found by the StreamingDecoder. The error
// is an invalid function body size, so that there are not enough bytes in the
// code section for the function body.
STREAM_TEST(TestErrorInCodeSectionDetectedByStreamingDecoder) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(26),                 // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 3),    // section size
      U32V_1(3),                          // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

// Test an error in the code section, found by the Compiler. The error is an
// invalid return type.
STREAM_TEST(TestErrorInCodeSectionDetectedByCompiler) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  uint8_t invalid_code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprI64Const, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 2 +
             arraysize(invalid_code)),  // section size
      U32V_1(3),                        // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.RunCompilerTasks();
  tester.OnBytesReceived(code, arraysize(code));
  tester.RunCompilerTasks();
  tester.OnBytesReceived(invalid_code, arraysize(invalid_code));
  tester.RunCompilerTasks();
  tester.OnBytesReceived(code, arraysize(code));
  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

// Test Abort before any bytes arrive.
STREAM_TEST(TestAbortImmediately) {
  StreamTester tester(isolate);
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort within a section.
STREAM_TEST(TestAbortWithinSection1) {
  StreamTester tester(isolate);
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                // module header
      kTypeSectionCode,                  // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),  // section size
      U32V_1(1)                          // type count
                                         // Type section is not yet complete.
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort within a section.
STREAM_TEST(TestAbortWithinSection2) {
  StreamTester tester(isolate);
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      // Function section is not yet complete.
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort just before the code section.
STREAM_TEST(TestAbortAfterSection) {
  StreamTester tester(isolate);
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after the function count in the code section. The compiler tasks
// execute before the abort.
STREAM_TEST(TestAbortAfterFunctionsCount1) {
  StreamTester tester(isolate);
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(20),                         // section size
      U32V_1(3),                          // functions count
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after the function count in the code section. The compiler tasks
// do not execute before the abort.
STREAM_TEST(TestAbortAfterFunctionsCount2) {
  StreamTester tester(isolate);
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(20),                         // section size
      U32V_1(3),                          // functions count
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after some functions got compiled. The compiler tasks execute
// before the abort.
STREAM_TEST(TestAbortAfterFunctionGotCompiled1) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(20),                         // section size
      U32V_1(3),                          // functions count
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after some functions got compiled. The compiler tasks execute
// before the abort.
STREAM_TEST(TestAbortAfterFunctionGotCompiled2) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(20),                         // section size
      U32V_1(3),                          // functions count
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after all functions got compiled.
STREAM_TEST(TestAbortAfterCodeSection1) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 3),    // section size
      U32V_1(3),                          // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after all functions got compiled.
STREAM_TEST(TestAbortAfterCodeSection2) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 3),    // section size
      U32V_1(3),                          // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

STREAM_TEST(TestAbortAfterCompilationError1) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  uint8_t invalid_code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprI64Const, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 2 +
             arraysize(invalid_code)),  // section size
      U32V_1(3),                        // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(invalid_code, arraysize(invalid_code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

STREAM_TEST(TestAbortAfterCompilationError2) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  uint8_t invalid_code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprI64Const, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 2 +
             arraysize(invalid_code)),  // section size
      U32V_1(3),                        // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(invalid_code, arraysize(invalid_code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

STREAM_TEST(TestOnlyModuleHeader) {
  StreamTester tester(isolate);

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,  // module header
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

STREAM_TEST(TestModuleWithZeroFunctions) {
  StreamTester tester(isolate);

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,    // module header
      kTypeSectionCode,      // section code
      U32V_1(1),             // section size
      U32V_1(0),             // type count
      kFunctionSectionCode,  // section code
      U32V_1(1),             // section size
      U32V_1(0),             // functions count
      kCodeSectionCode,      // section code
      U32V_1(1),             // section size
      U32V_1(0),             // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseFulfilled());
}

STREAM_TEST(TestModuleWithMultipleFunctions) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 3),    // section size
      U32V_1(3),                          // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.RunCompilerTasks();
  tester.OnBytesReceived(code, arraysize(code));
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseFulfilled());
}

STREAM_TEST(TestModuleWithDataSection) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 3),    // section size
      U32V_1(3),                          // functions count
  };

  const uint8_t data_section[] = {
      kDataSectionCode,  // section code
      U32V_1(1),         // section size
      U32V_1(0),         // data segment count
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.RunCompilerTasks();
  tester.OnBytesReceived(data_section, arraysize(data_section));
  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseFulfilled());
}
// Test that all bytes arrive before doing any compilation. FinishStream is
// called immediately.
STREAM_TEST(TestModuleWithImportedFunction) {
  StreamTester tester(isolate);
  ZoneBuffer buffer(tester.zone());
  TestSignatures sigs;
  WasmModuleBuilder builder(tester.zone());
  builder.AddImport(base::ArrayVector("Test"), sigs.i_iii());
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprLocalGet, 0, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  builder.WriteTo(&buffer);

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

STREAM_TEST(TestIncrementalCaching) {
  FLAG_VALUE_SCOPE(wasm_dynamic_tiering, true);
  FLAG_VALUE_SCOPE(wasm_tier_up, false);
  constexpr int threshold = 10;
  FlagScope<int> caching_treshold(&FLAG_wasm_caching_threshold, threshold);
  StreamTester tester(isolate);
  int call_cache_counter = 0;
  tester.stream()->SetModuleCompiledCallback(
      [&call_cache_counter](
          const std::shared_ptr<i::wasm::NativeModule>& native_module) {
        call_cache_counter++;
      });

  ZoneBuffer buffer(tester.zone());
  TestSignatures sigs;
  WasmModuleBuilder builder(tester.zone());
  builder.SetMinMemorySize(1);

  base::Vector<const char> function_names[] = {
      base::CStrVector("f0"), base::CStrVector("f1"), base::CStrVector("f2")};
  for (int i = 0; i < 3; ++i) {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.v_v());

    constexpr int64_t val = 0x123456789abc;
    constexpr int index = 0x1234;
    uint8_t store_mem[] = {
        WASM_STORE_MEM(MachineType::Int64(), WASM_I32V(index), WASM_I64V(val))};
    constexpr uint32_t kStoreLength = 20;
    CHECK_EQ(kStoreLength, arraysize(store_mem));

    // Produce a store {threshold} many times to reach the caching threshold.
    constexpr uint32_t kCodeLength = kStoreLength * threshold + 1;
    uint8_t code[kCodeLength];
    for (int j = 0; j < threshold; ++j) {
      memcpy(code + (j * kStoreLength), store_mem, kStoreLength);
    }
    code[kCodeLength - 1] = WasmOpcode::kExprEnd;
    f->EmitCode(code, kCodeLength);
    builder.AddExport(function_names[i], f);
  }
  builder.WriteTo(&buffer);
  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseFulfilled());
  tester.native_module();
  constexpr base::Vector<const char> kNoSourceUrl{"", 0};
  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  Handle<Script> script = GetWasmEngine()->GetOrCreateScript(
      i_isolate, tester.native_module(), kNoSourceUrl);
  Handle<FixedArray> export_wrappers = i_isolate->factory()->NewFixedArray(3);
  Handle<WasmModuleObject> module_object = WasmModuleObject::New(
      i_isolate, tester.native_module(), script, export_wrappers);
  ErrorThrower thrower(i_isolate, "Instantiation");
  // We instantiated before, so the second instantiation must also succeed:
  Handle<WasmInstanceObject> instance =
      GetWasmEngine()
          ->SyncInstantiate(i_isolate, &thrower, module_object, {}, {})
          .ToHandleChecked();
  CHECK(!thrower.error());

  WasmCodeRefScope code_scope;
  CHECK(tester.native_module()->GetCode(0)->is_liftoff());
  CHECK(tester.native_module()->GetCode(1)->is_liftoff());
  CHECK(tester.native_module()->GetCode(2)->is_liftoff());
  // No TurboFan compilation happened yet, and therefore no call to the cache.
  CHECK_EQ(0, call_cache_counter);
  i::wasm::TriggerTierUp(i_isolate, tester.native_module().get(), 0, instance);
  tester.RunCompilerTasks();
  CHECK(!tester.native_module()->GetCode(0)->is_liftoff());
  CHECK(tester.native_module()->GetCode(1)->is_liftoff());
  CHECK(tester.native_module()->GetCode(2)->is_liftoff());
  CHECK_EQ(1, call_cache_counter);
  size_t serialized_size;
  {
    i::wasm::WasmSerializer serializer(tester.native_module().get());
    serialized_size = serializer.GetSerializedNativeModuleSize();
  }
  i::wasm::TriggerTierUp(i_isolate, tester.native_module().get(), 1, instance);
  tester.RunCompilerTasks();
  CHECK(!tester.native_module()->GetCode(0)->is_liftoff());
  CHECK(!tester.native_module()->GetCode(1)->is_liftoff());
  CHECK(tester.native_module()->GetCode(2)->is_liftoff());
  CHECK_EQ(2, call_cache_counter);
  {
    i::wasm::WasmSerializer serializer(tester.native_module().get());
    CHECK_LT(serialized_size, serializer.GetSerializedNativeModuleSize());
  }
}

STREAM_TEST(TestModuleWithErrorAfterDataSection) {
  StreamTester tester(isolate);

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 1),                      // section size
      U32V_1(1),                          // functions count
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(6),                          // section size
      U32V_1(1),                          // functions count
      U32V_1(4),                          // body size
      U32V_1(0),                          // locals count
      kExprLocalGet,                      // some code
      0,                                  // some code
      kExprEnd,                           // some code
      kDataSectionCode,                   // section code
      U32V_1(1),                          // section size
      U32V_1(0),                          // data segment count
      kUnknownSectionCode,                // section code
      U32V_1(1),                          // invalid section size
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseRejected());
}

// Test that cached bytes work.
STREAM_TEST(TestDeserializationBypassesCompilation) {
  FlagScope<bool> no_wasm_dynamic_tiering(&FLAG_wasm_dynamic_tiering, false);
  StreamTester tester(isolate);
  ZoneBuffer wire_bytes = GetValidModuleBytes(tester.zone());
  ZoneBuffer module_bytes =
      GetValidCompiledModuleBytes(isolate, tester.zone(), wire_bytes);
  tester.SetCompiledModuleBytes(module_bytes.begin(), module_bytes.size());
  tester.OnBytesReceived(wire_bytes.begin(), wire_bytes.size());
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

// Test that bad cached bytes don't cause compilation of wire bytes to fail.
STREAM_TEST(TestDeserializationFails) {
  FlagScope<bool> no_wasm_dynamic_tiering(&FLAG_wasm_dynamic_tiering, false);
  StreamTester tester(isolate);
  ZoneBuffer wire_bytes = GetValidModuleBytes(tester.zone());
  ZoneBuffer module_bytes =
      GetValidCompiledModuleBytes(isolate, tester.zone(), wire_bytes);
  // corrupt header
  byte first_byte = *module_bytes.begin();
  module_bytes.patch_u8(0, first_byte + 1);
  tester.SetCompiledModuleBytes(module_bytes.begin(), module_bytes.size());
  tester.OnBytesReceived(wire_bytes.begin(), wire_bytes.size());
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

// Test that a non-empty function section with a missing code section fails.
STREAM_TEST(TestFunctionSectionWithoutCodeSection) {
  StreamTester tester(isolate);

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestSetModuleCompiledCallback) {
  FlagScope<bool> no_wasm_dynamic_tiering(&FLAG_wasm_dynamic_tiering, false);
  StreamTester tester(isolate);
  bool callback_called = false;
  tester.stream()->SetModuleCompiledCallback(
      [&callback_called](const std::shared_ptr<NativeModule> module) {
        callback_called = true;
      });

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 3),                      // section size
      U32V_1(3),                          // functions count
      0,                                  // signature index
      0,                                  // signature index
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(1 + arraysize(code) * 3),    // section size
      U32V_1(3),                          // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK(tester.IsPromiseFulfilled());
  CHECK(callback_called);
}

// Test that a compile error contains the name of the function, even if the name
// section is not present at the time the error is detected.
STREAM_TEST(TestCompileErrorFunctionName) {
  const uint8_t bytes_module_with_code[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(2),                          // section size
      U32V_1(1),                          // functions count
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(4),                          // section size
      U32V_1(1),                          // functions count
      2,                                  // body size
      0,                                  // local definitions count
      kExprNop,                           // body
  };

  const uint8_t bytes_names[] = {
      kUnknownSectionCode,                 // section code
      U32V_1(11),                          // section size
      4,                                   // section name length
      'n',                                 // section name
      'a',                                 // section name
      'm',                                 // section name
      'e',                                 // section name
      NameSectionKindCode::kFunctionCode,  // name section kind
      4,                                   // name section kind length
      1,                                   // num function names
      0,                                   // function index
      1,                                   // function name length
      'f',                                 // function name
  };

  for (bool late_names : {false, true}) {
    StreamTester tester(isolate);

    tester.OnBytesReceived(bytes_module_with_code,
                           arraysize(bytes_module_with_code));
    if (late_names) tester.RunCompilerTasks();
    tester.OnBytesReceived(bytes_names, arraysize(bytes_names));
    tester.FinishStream();

    tester.RunCompilerTasks();

    CHECK(tester.IsPromiseRejected());
    CHECK_EQ(
        "CompileError: WebAssembly.compileStreaming(): Compiling function "
        "#0:\"f\" failed: function body must end with \"end\" opcode @+26",
        tester.error_message());
  }
}

STREAM_TEST(TestSetModuleCodeSection) {
  StreamTester tester(isolate);

  uint8_t code[] = {
      U32V_1(1),                  // functions count
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprLocalGet, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                 // module header
      kTypeSectionCode,                   // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),   // section size
      U32V_1(1),                          // type count
      SIG_ENTRY_x_x(kI32Code, kI32Code),  // signature entry
      kFunctionSectionCode,               // section code
      U32V_1(1 + 1),                      // section size
      U32V_1(1),                          // functions count
      0,                                  // signature index
      kCodeSectionCode,                   // section code
      U32V_1(arraysize(code)),            // section size
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.FinishStream();
  tester.RunCompilerTasks();
  CHECK_EQ(tester.native_module()->module()->code.offset(), arraysize(bytes));
  CHECK_EQ(tester.native_module()->module()->code.length(), arraysize(code));
  CHECK(tester.IsPromiseFulfilled());
}

// Test that profiler does not crash when module is only partly compiled.
STREAM_TEST(TestProfilingMidStreaming) {
  StreamTester tester(isolate);
  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  Zone* zone = tester.zone();

  // Build module with one exported (named) function.
  ZoneBuffer buffer(zone);
  {
    TestSignatures sigs;
    WasmModuleBuilder builder(zone);
    WasmFunctionBuilder* f = builder.AddFunction(sigs.v_v());
    uint8_t code[] = {kExprEnd};
    f->EmitCode(code, arraysize(code));
    builder.AddExport(base::VectorOf("foo", 3), f);
    builder.WriteTo(&buffer);
  }

  // Start profiler to force code logging.
  v8::CpuProfiler* cpu_profiler = v8::CpuProfiler::New(isolate);
  v8::CpuProfilingOptions profile_options;
  cpu_profiler->StartProfiling(v8::String::Empty(isolate), profile_options);

  // Send incomplete wire bytes and start compilation.
  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.RunCompilerTasks();

  // Trigger code logging explicitly like the profiler would do.
  CHECK(WasmCode::ShouldBeLogged(i_isolate));
  GetWasmEngine()->LogOutstandingCodesForIsolate(i_isolate);
  CHECK(tester.IsPromisePending());

  // Finalize stream, stop profiler and clean up.
  tester.FinishStream();
  CHECK(tester.IsPromiseFulfilled());
  v8::CpuProfile* profile =
      cpu_profiler->StopProfiling(v8::String::Empty(isolate));
  profile->Delete();
  cpu_profiler->Dispose();
}

STREAM_TEST(TierDownWithError) {
  // https://crbug.com/1160031
  StreamTester tester(isolate);
  Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  Zone* zone = tester.zone();

  ZoneBuffer buffer(zone);
  {
    TestSignatures sigs;
    WasmModuleBuilder builder(zone);
    // Type error at i32.add.
    builder.AddFunction(sigs.v_v())->Emit(kExprI32Add);
    builder.WriteTo(&buffer);
  }

  GetWasmEngine()->TierDownAllModulesPerIsolate(i_isolate);

  tester.OnBytesReceived(buffer.begin(), buffer.size());
  tester.FinishStream();
  tester.RunCompilerTasks();
}

#undef STREAM_TEST

}  // namespace wasm
}  // namespace internal
}  // namespace v8
