// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8-metrics.h"
#include "include/v8-platform.h"
#include "src/api/api-inl.h"
#include "src/base/platform/time.h"
#include "src/base/unique-array.h"
#include "src/objects/managed.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-objects-inl.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/unittests/wasm/wasm-runner.h"

namespace v8::internal::wasm {

namespace {

class MockPlatform final : public v8::Platform {
 public:
  explicit MockPlatform(v8::Platform* default_platform)
      : default_platform_(default_platform),
        no_memory_reducer_(&v8_flags.memory_reducer, false),
        task_runner_(std::make_shared<MockTaskRunner>()) {
    CHECK_NOT_NULL(default_platform_);
    i::V8::SetPlatformForTesting(this);
  }

  ~MockPlatform() override {
    for (auto* job_handle : job_handles_) job_handle->ResetPlatform();
    if (i::V8::GetCurrentPlatform() == this) {
      i::V8::SetPlatformForTesting(default_platform_);
    }
  }

  v8::PageAllocator* GetPageAllocator() override {
    return default_platform_->GetPageAllocator();
  }

  void OnCriticalMemoryPressure() override {
    default_platform_->OnCriticalMemoryPressure();
  }

  int NumberOfWorkerThreads() override {
    return default_platform_->NumberOfWorkerThreads();
  }

  std::unique_ptr<v8::JobHandle> CreateJobImpl(
      v8::TaskPriority priority, std::unique_ptr<v8::JobTask> job_task,
      const v8::SourceLocation& location) override {
    auto orig_job_handle = v8::platform::NewDefaultJobHandle(
        this, priority, std::move(job_task), 1);
    auto job_handle =
        std::make_unique<MockJobHandle>(std::move(orig_job_handle), this);
    job_handles_.insert(job_handle.get());
    return job_handle;
  }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate, v8::TaskPriority) override {
    return task_runner_;
  }

  void PostTaskOnWorkerThreadImpl(v8::TaskPriority priority,
                                  std::unique_ptr<v8::Task> task,
                                  const v8::SourceLocation& location) override {
    task_runner_->PostTask(std::move(task));
  }

  void PostDelayedTaskOnWorkerThreadImpl(
      v8::TaskPriority priority, std::unique_ptr<v8::Task> task,
      double delay_in_seconds, const v8::SourceLocation& location) override {
    default_platform_->PostDelayedTaskOnWorkerThread(priority, std::move(task),
                                                     delay_in_seconds);
  }

  double MonotonicallyIncreasingTime() override {
    return default_platform_->MonotonicallyIncreasingTime();
  }

  double CurrentClockTimeMillis() override {
    return default_platform_->CurrentClockTimeMillis();
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  v8::TracingController* GetTracingController() override {
    return default_platform_->GetTracingController();
  }

  v8::ThreadIsolatedAllocator* GetThreadIsolatedAllocator() override {
    return default_platform_->GetThreadIsolatedAllocator();
  }

  void ExecuteTasks() { task_runner_->ExecuteTasks(); }

 private:
  class MockTaskRunner final : public TaskRunner {
   public:
    void PostTaskImpl(std::unique_ptr<v8::Task> task,
                      const SourceLocation& location) override {
      base::MutexGuard lock_scope(&tasks_lock_);
      tasks_.push(std::move(task));
    }

    void PostNonNestableTaskImpl(std::unique_ptr<Task> task,
                                 const SourceLocation& location) override {
      PostTask(std::move(task));
    }

    void PostDelayedTaskImpl(std::unique_ptr<Task> task,
                             double delay_in_seconds,
                             const SourceLocation& location) override {
      PostTask(std::move(task));
    }

    void PostNonNestableDelayedTaskImpl(
        std::unique_ptr<Task> task, double delay_in_seconds,
        const SourceLocation& location) override {
      PostTask(std::move(task));
    }

    void PostIdleTaskImpl(std::unique_ptr<IdleTask> task,
                          const SourceLocation& location) override {
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
    bool IsValid() override { return orig_handle_->IsValid(); }
    bool IsActive() override { return orig_handle_->IsActive(); }

   private:
    std::unique_ptr<JobHandle> orig_handle_;
    MockPlatform* platform_;
  };

  v8::Platform* default_platform_;
  FlagScope<bool> no_memory_reducer_;
  std::shared_ptr<MockTaskRunner> task_runner_;
  std::unordered_set<MockJobHandle*> job_handles_;
};

enum class CompilationStatus {
  kPending,
  kFinished,
  kFailed,
};

class InstantiateResolver : public InstantiationResultResolver {
 public:
  InstantiateResolver(Isolate* isolate, CompilationStatus* status,
                      std::string* error_message)
      : isolate_(isolate), status_(status), error_message_(error_message) {}

  void OnInstantiationSucceeded(
      i::DirectHandle<i::WasmInstanceObject> instance) override {
    *status_ = CompilationStatus::kFinished;
  }

  void OnInstantiationFailed(i::DirectHandle<i::JSAny> error_reason) override {
    *status_ = CompilationStatus::kFailed;
    DirectHandle<String> str =
        Object::ToString(isolate_, error_reason).ToHandleChecked();
    error_message_->assign(str->ToCString().get());
  }

 private:
  Isolate* isolate_;
  CompilationStatus* const status_;
  std::string* const error_message_;
};

class CompileResolver : public CompilationResultResolver {
 public:
  CompileResolver(CompilationStatus* status, std::string* error_message,
                  Isolate* isolate,
                  CppGCManaged<NativeModule>::Ptr* native_module)
      : status_(status),
        error_message_(error_message),
        isolate_(isolate),
        native_module_(native_module) {}

  void OnCompilationSucceeded(
      i::DirectHandle<i::WasmModuleObject> module) override {
    if (!module.is_null()) {
      *native_module_ = module->native_module();
      GetWasmEngine()->AsyncInstantiate(isolate_,
                                        std::make_unique<InstantiateResolver>(
                                            isolate_, status_, error_message_),
                                        module,
                                        MaybeDirectHandle<JSReceiver>());
    }
  }

  void OnCompilationFailed(i::DirectHandle<i::JSAny> error_reason) override {
    *status_ = CompilationStatus::kFailed;
    DirectHandle<String> str =
        Object::ToString(isolate_, error_reason).ToHandleChecked();
    error_message_->assign(str->ToCString().get());
  }

 private:
  CompilationStatus* const status_;
  std::string* const error_message_;
  Isolate* isolate_;
  CppGCManaged<NativeModule>::Ptr* const native_module_;
};

template <typename TMixin>
class WithMockPlatformMixin : public TMixin {
 public:
  WithMockPlatformMixin() : mock_platform_(this->platform()) {}
  WithMockPlatformMixin(const WithMockPlatformMixin&) = delete;
  WithMockPlatformMixin& operator=(const WithMockPlatformMixin&) = delete;

  void TearDown() override {
    // Restore default_platform_ before ~IsolateWrapper() calls
    // v8::platform::PumpMessageLoop.
    if (i::V8::GetCurrentPlatform() == &mock_platform_) {
      i::V8::SetPlatformForTesting(this->platform());
    }
    TMixin::TearDown();
  }

  MockPlatform* mock_platform() { return &mock_platform_; }

 private:
  MockPlatform mock_platform_;
};

}  // namespace

using WasmMetricsTest = WithIsolateScopeMixin<WithIsolateMixin<
    WithMockPlatformMixin<WithWasmTrapHandlerMixin<TestWithPlatform>>>>;

#define RUN_COMPILE(name)                                       \
  v8::HandleScope handle_scope(isolate());                      \
  v8::Local<v8::Context> context = v8::Context::New(isolate()); \
  v8::Context::Scope context_scope(context);                    \
  RunCompile_##name(mock_platform(), i_isolate());

#define COMPILE_TEST(name)                                                 \
  void RunCompile_##name(MockPlatform*, i::Isolate*);                      \
  TEST_F(WasmMetricsTest, Sync##name) {                                    \
    i::FlagScope<bool> single_threaded_scope(&i::v8_flags.single_threaded, \
                                             true);                        \
    RUN_COMPILE(name);                                                     \
  }                                                                        \
                                                                           \
  TEST_F(WasmMetricsTest, Async##name) { RUN_COMPILE(name); }              \
                                                                           \
  TEST_F(WasmMetricsTest, Streaming##name) {                               \
    i::FlagScope<bool> streaming_scope(&i::v8_flags.wasm_test_streaming,   \
                                       true);                              \
    RUN_COMPILE(name);                                                     \
  }                                                                        \
  void RunCompile_##name(MockPlatform* platform, i::Isolate* isolate)

class MetricsRecorder : public v8::metrics::Recorder {
 public:
  std::vector<v8::metrics::WasmModuleDecoded> module_decoded_;
  std::vector<v8::metrics::WasmModuleCompiled> module_compiled_;
  std::vector<v8::metrics::WasmModuleInstantiated> module_instantiated_;

  void AddMainThreadEvent(const v8::metrics::WasmModuleDecoded& event,
                          v8::metrics::Recorder::ContextId id) override {
    CHECK(!id.IsEmpty());
    module_decoded_.emplace_back(event);
  }
  void AddMainThreadEvent(const v8::metrics::WasmModuleCompiled& event,
                          v8::metrics::Recorder::ContextId id) override {
    CHECK(!id.IsEmpty());
    module_compiled_.emplace_back(event);
  }
  void AddMainThreadEvent(const v8::metrics::WasmModuleInstantiated& event,
                          v8::metrics::Recorder::ContextId id) override {
    CHECK(!id.IsEmpty());
    module_instantiated_.emplace_back(event);
  }
};

COMPILE_TEST(TestEventMetrics) {
  if (v8_flags.memory_balancer) return;
  FlagScope<bool> no_wasm_dynamic_tiering(&v8_flags.wasm_dynamic_tiering,
                                          false);
  std::shared_ptr<MetricsRecorder> recorder =
      std::make_shared<MetricsRecorder>();
  reinterpret_cast<v8::Isolate*>(isolate)->SetMetricsRecorder(recorder);
  if (v8::base::ThreadTicks::IsSupported()) {
    v8::base::ThreadTicks::WaitUntilInitialized();
  }

  TestSignatures sigs;
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
  WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
  f->builder()->AddExport("main", f);
  f->EmitCode({WASM_I32V_2(0), WASM_END});
  ZoneBuffer buffer(&zone);
  builder->WriteTo(&buffer);

  auto enabled_features = WasmEnabledFeatures::FromIsolate(isolate);
  CompilationStatus status = CompilationStatus::kPending;
  std::string error_message;
  CppGCManaged<NativeModule>::Ptr native_module;
  base::UniqueArray<const uint8_t> bytes = base::UniqueCopyOf(buffer);
  GetWasmEngine()->AsyncCompile(
      isolate, enabled_features, CompileTimeImports{},
      std::make_shared<CompileResolver>(&status, &error_message, isolate,
                                        &native_module),
      std::move(bytes), "CompileAndInstantiateWasmModuleForTesting");

  // Finish compilation tasks.
  while (status == CompilationStatus::kPending) {
    platform->ExecuteTasks();
  }
  platform->ExecuteTasks();  // Complete pending tasks beyond compilation.
  CHECK_EQ(CompilationStatus::kFinished, status);

  CHECK_EQ(1, recorder->module_decoded_.size());
  CHECK(recorder->module_decoded_.back().success);
  CHECK(recorder->module_decoded_.back().async);
  CHECK_EQ(i::v8_flags.wasm_test_streaming,
           recorder->module_decoded_.back().streamed);
  CHECK_EQ(buffer.size(),
           recorder->module_decoded_.back().module_size_in_bytes);
  CHECK_EQ(1, recorder->module_decoded_.back().function_count);
  CHECK_LE(0, recorder->module_decoded_.back().wall_clock_duration_in_us);

  CHECK_EQ(1, recorder->module_compiled_.size());
  CHECK(recorder->module_compiled_.back().success);
  CHECK(recorder->module_compiled_.back().async);
  CHECK_EQ(i::v8_flags.wasm_test_streaming,
           recorder->module_compiled_.back().streamed);
  CHECK(!recorder->module_compiled_.back().cached);
  CHECK(!recorder->module_compiled_.back().deserialized);
  CHECK_EQ(v8_flags.wasm_lazy_compilation,
           recorder->module_compiled_.back().lazy);
  CHECK_LT(0, recorder->module_compiled_.back().code_size_in_bytes);
  CHECK_EQ(native_module->generated_code_size(),
           recorder->module_compiled_.back().code_size_in_bytes);
  CHECK_LE(0, recorder->module_compiled_.back().wall_clock_duration_in_us);

  CHECK_EQ(1, recorder->module_instantiated_.size());
  CHECK(recorder->module_instantiated_.back().success);
  // We currently don't support true async instantiation.
  CHECK(!recorder->module_instantiated_.back().async);
  CHECK_EQ(0, recorder->module_instantiated_.back().imported_function_count);
  CHECK_LE(0, recorder->module_instantiated_.back().wall_clock_duration_in_us);
}

}  // namespace v8::internal::wasm
