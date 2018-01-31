// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/objects-inl.h"
#include "src/v8.h"
#include "src/vector.h"

#include "src/wasm/compilation-manager.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"

#include "test/cctest/cctest.h"

#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

class MockPlatform final : public TestPlatform {
 public:
  MockPlatform() : task_runner_(std::make_shared<MockTaskRunner>()) {
    // Now that it's completely constructed, make this the current platform.
    i::V8::SetPlatformForTesting(this);
  }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return task_runner_;
  }

  std::shared_ptr<TaskRunner> GetBackgroundTaskRunner(
      v8::Isolate* isolate) override {
    return task_runner_;
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    task_runner_->PostTask(std::unique_ptr<Task>(task));
  }

  void CallOnBackgroundThread(v8::Task* task,
                              ExpectedRuntime expected_runtime) override {
    task_runner_->PostTask(std::unique_ptr<Task>(task));
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  void ExecuteTasks() { task_runner_->ExecuteTasks(); }

 private:
  class MockTaskRunner final : public TaskRunner {
   public:
    void PostTask(std::unique_ptr<v8::Task> task) override {
      tasks_.push_back(std::move(task));
    }

    void PostDelayedTask(std::unique_ptr<Task> task,
                         double delay_in_seconds) override {
      UNREACHABLE();
    };

    void PostIdleTask(std::unique_ptr<IdleTask> task) override {
      UNREACHABLE();
    }

    bool IdleTasksEnabled() override { return false; };

    void ExecuteTasks() {
      while (!tasks_.empty()) {
        std::unique_ptr<Task> task = std::move(tasks_.back());
        tasks_.pop_back();
        task->Run();
      }
    }

   private:
    // We do not execute tasks concurrently, so we only need one list of tasks.
    std::vector<std::unique_ptr<v8::Task>> tasks_;
  };

  std::shared_ptr<MockTaskRunner> task_runner_;
};

namespace {

class StreamTester {
 public:
  StreamTester() : zone_(&allocator_, "StreamTester") {
    v8::Isolate* isolate = CcTest::isolate();
    i::Isolate* i_isolate = CcTest::i_isolate();

    // Create the promise for the streaming compilation.
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<Promise::Resolver> resolver;
    CHECK(Promise::Resolver::New(context).ToLocal(&resolver));
    CHECK(!i_isolate->has_scheduled_exception());
    promise_ = resolver->GetPromise();

    i::Handle<i::JSPromise> i_promise = v8::Utils::OpenHandle(*promise_);

    stream_ = i_isolate->wasm_compilation_manager()->StartStreamingCompilation(
        i_isolate, v8::Utils::OpenHandle(*context), i_promise);
  }

  std::shared_ptr<StreamingDecoder> stream() { return stream_; }

  // Run all compiler tasks, both foreground and background tasks.
  void RunCompilerTasks() {
    static_cast<MockPlatform*>(i::V8::GetCurrentPlatform())->ExecuteTasks();
  }

  bool IsPromiseFulfilled() {
    return promise_->State() == v8::Promise::kFulfilled;
  }

  bool IsPromiseRejected() {
    return promise_->State() == v8::Promise::kRejected;
  }

  bool IsPromisePending() { return promise_->State() == v8::Promise::kPending; }

  void OnBytesReceived(const uint8_t* start, size_t length) {
    stream_->OnBytesReceived(Vector<const uint8_t>(start, length));
  }

  void FinishStream() { stream_->Finish(); }

  Zone* zone() { return &zone_; }

 private:
  AccountingAllocator allocator_;
  Zone zone_;
  v8::Local<v8::Promise> promise_;
  std::shared_ptr<StreamingDecoder> stream_;
};
}  // namespace

#define STREAM_TEST(name)                               \
  void RunStream_##name();                              \
  TEST(name) {                                          \
    MockPlatform platform;                              \
    CcTest::InitializeVM();                             \
    v8::HandleScope handle_scope(CcTest::isolate());    \
    i::HandleScope internal_scope(CcTest::i_isolate()); \
    RunStream_##name();                                 \
  }                                                     \
  void RunStream_##name()

// Create a valid module with 3 functions.
ZoneBuffer GetValidModuleBytes(Zone* zone) {
  ZoneBuffer buffer(zone);
  TestSignatures sigs;
  WasmModuleBuilder builder(zone);
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprGetLocal, 0, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprGetLocal, 1, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprGetLocal, 2, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  builder.WriteTo(buffer);
  return buffer;
}

// Test that all bytes arrive before doing any compilation. FinishStream is
// called immediately.
STREAM_TEST(TestAllBytesArriveImmediatelyStreamFinishesFirst) {
  StreamTester tester;
  ZoneBuffer buffer = GetValidModuleBytes(tester.zone());

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

// Test that all bytes arrive before doing any compilation. FinishStream is
// called after the compilation is done.
STREAM_TEST(TestAllBytesArriveAOTCompilerFinishesFirst) {
  StreamTester tester;
  ZoneBuffer buffer = GetValidModuleBytes(tester.zone());

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());

  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

size_t GetFunctionOffset(i::Isolate* isolate, const uint8_t* buffer,
                         size_t size, size_t index) {
  ModuleResult result = SyncDecodeWasmModule(isolate, buffer, buffer + size,
                                             false, ModuleOrigin::kWasmOrigin);
  CHECK(result.ok());
  std::unique_ptr<WasmModule> module = std::move(result.val);
  const WasmFunction* func = &module->functions[1];
  return func->code.offset();
}

// Test that some functions come in the beginning, some come after some
// functions already got compiled.
STREAM_TEST(TestCutAfterOneFunctionStreamFinishesFirst) {
  i::Isolate* isolate = CcTest::i_isolate();
  StreamTester tester;
  ZoneBuffer buffer = GetValidModuleBytes(tester.zone());

  size_t offset = GetFunctionOffset(isolate, buffer.begin(), buffer.size(), 1);
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
  i::Isolate* isolate = CcTest::i_isolate();
  StreamTester tester;
  ZoneBuffer buffer = GetValidModuleBytes(tester.zone());

  size_t offset = GetFunctionOffset(isolate, buffer.begin(), buffer.size(), 1);
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
  builder.AddGlobal(kWasmStmt, false, true,
                    WasmInitExpr(WasmInitExpr::kGlobalIndex, 12));
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprGetLocal, 0, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprGetLocal, 1, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprGetLocal, 2, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  builder.WriteTo(buffer);
  return buffer;
}

// Test an error in a section, found by the ModuleDecoder.
STREAM_TEST(TestErrorInSectionStreamFinishesFirst) {
  StreamTester tester;
  ZoneBuffer buffer = GetModuleWithInvalidSection(tester.zone());

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestErrorInSectionCompilerFinishesFirst) {
  StreamTester tester;
  ZoneBuffer buffer = GetModuleWithInvalidSection(tester.zone());

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestErrorInSectionWithCuts) {
  StreamTester tester;
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
  // 0x808080800f is an invalid module size in leb encoding.
  section_size_address[0] = 0x80;
  section_size_address[1] = 0x80;
  section_size_address[2] = 0x80;
  section_size_address[3] = 0x80;
  section_size_address[4] = 0x0f;
  return buffer;
}

STREAM_TEST(TestErrorInSectionSizeStreamFinishesFirst) {
  StreamTester tester;
  ZoneBuffer buffer = GetModuleWithInvalidSectionSize(tester.zone());
  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestErrorInSectionSizeCompilerFinishesFirst) {
  StreamTester tester;
  ZoneBuffer buffer = GetModuleWithInvalidSectionSize(tester.zone());
  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.RunCompilerTasks();
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseRejected());
}

STREAM_TEST(TestErrorInSectionSizeWithCuts) {
  StreamTester tester;
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
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
      U32V_1(1 + arraysize(code) * 2),      // section size
      U32V_1(2),                            // !!! invalid function count !!!
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
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(26),                 // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
      U32V_1(1 + arraysize(code) * 3),      // section size
      U32V_1(3),                            // functions count
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
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  uint8_t invalid_code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprI64Const, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
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
  StreamTester tester;
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort within a section.
STREAM_TEST(TestAbortWithinSection1) {
  StreamTester tester;
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
  StreamTester tester;
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      // Function section is not yet complete.
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort just before the code section.
STREAM_TEST(TestAbortAfterSection) {
  StreamTester tester;
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after the function count in the code section. The compiler tasks
// execute before the abort.
STREAM_TEST(TestAbortAfterFunctionsCount1) {
  StreamTester tester;
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
      U32V_1(20),                           // section size
      U32V_1(3),                            // functions count
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.RunCompilerTasks();
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after the function count in the code section. The compiler tasks
// do not execute before the abort.
STREAM_TEST(TestAbortAfterFunctionsCount2) {
  StreamTester tester;
  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
      U32V_1(20),                           // section size
      U32V_1(3),                            // functions count
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after some functions got compiled. The compiler tasks execute
// before the abort.
STREAM_TEST(TestAbortAfterFunctionGotCompiled1) {
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
      U32V_1(20),                           // section size
      U32V_1(3),                            // functions count
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
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
      U32V_1(20),                           // section size
      U32V_1(3),                            // functions count
  };
  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

// Test Abort after all functions got compiled.
STREAM_TEST(TestAbortAfterCodeSection1) {
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
      U32V_1(1 + arraysize(code) * 3),      // section size
      U32V_1(3),                            // functions count
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
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(4),                  // body size
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
      U32V_1(1 + arraysize(code) * 3),      // section size
      U32V_1(3),                            // functions count
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.OnBytesReceived(code, arraysize(code));
  tester.stream()->Abort();
  tester.RunCompilerTasks();
}

STREAM_TEST(TestAbortAfterCompilationError1) {
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  uint8_t invalid_code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprI64Const, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
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
  StreamTester tester;

  uint8_t code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprGetLocal, 0, kExprEnd  // body
  };

  uint8_t invalid_code[] = {
      U32V_1(4),                  // !!! invalid body size !!!
      U32V_1(0),                  // locals count
      kExprI64Const, 0, kExprEnd  // body
  };

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,                   // module header
      kTypeSectionCode,                     // section code
      U32V_1(1 + SIZEOF_SIG_ENTRY_x_x),     // section size
      U32V_1(1),                            // type count
      SIG_ENTRY_x_x(kLocalI32, kLocalI32),  // signature entry
      kFunctionSectionCode,                 // section code
      U32V_1(1 + 3),                        // section size
      U32V_1(3),                            // functions count
      0,                                    // signature index
      0,                                    // signature index
      0,                                    // signature index
      kCodeSectionCode,                     // section code
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
  StreamTester tester;

  const uint8_t bytes[] = {
      WASM_MODULE_HEADER,  // module header
  };

  tester.OnBytesReceived(bytes, arraysize(bytes));
  tester.FinishStream();
  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}

STREAM_TEST(TestModuleWithZeroFunctions) {
  StreamTester tester;

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

// Test that all bytes arrive before doing any compilation. FinishStream is
// called immediately.
STREAM_TEST(TestModuleWithImportedFunction) {
  StreamTester tester;
  ZoneBuffer buffer(tester.zone());
  TestSignatures sigs;
  WasmModuleBuilder builder(tester.zone());
  builder.AddImport(ArrayVector("Test"), sigs.i_iii());
  {
    WasmFunctionBuilder* f = builder.AddFunction(sigs.i_iii());
    uint8_t code[] = {kExprGetLocal, 0, kExprEnd};
    f->EmitCode(code, arraysize(code));
  }
  builder.WriteTo(buffer);

  tester.OnBytesReceived(buffer.begin(), buffer.end() - buffer.begin());
  tester.FinishStream();

  tester.RunCompilerTasks();

  CHECK(tester.IsPromiseFulfilled());
}
#undef STREAM_TEST

}  // namespace wasm
}  // namespace internal
}  // namespace v8
