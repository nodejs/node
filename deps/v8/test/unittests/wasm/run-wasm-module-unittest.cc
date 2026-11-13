// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "include/v8-external.h"
#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/base/unique-array.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/code-serializer.h"
#include "src/utils/version.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/wasm/wasm-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_module {

using base::ReadLittleEndianValue;
using base::WriteLittleEndianValue;
using testing::CompileAndInstantiateForTesting;
using RunWasmModuleTest = WasmExecutionTestBase;

namespace {
void Cleanup(Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce()) {
  // By sending a low memory notifications, we will try hard to collect all
  // garbage and will therefore also invoke all weak callbacks of actually
  // unreachable persistent handles.
  reinterpret_cast<v8::Isolate*>(isolate)->LowMemoryNotification();
}

void TestModule(Zone* zone, WasmModuleBuilder* builder,
                int32_t expected_result) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(&buffer);

  Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
  HandleScope scope(isolate);
  int32_t result =
      testing::CompileAndRunWasmModule(isolate, buffer.begin(), buffer.end());
  CHECK_EQ(expected_result, result);
}

void TestModuleException(Zone* zone, WasmModuleBuilder* builder) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(&buffer);

  Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
  HandleScope scope(isolate);
  v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  testing::CompileAndRunWasmModule(isolate, buffer.begin(), buffer.end());
  CHECK(try_catch.HasCaught());
  isolate->clear_exception();
}

void ExportAsMain(WasmFunctionBuilder* f) {
  f->builder()->AddExport("main", f);
}

#define EMIT_CODE_WITH_END(f, code)  \
  do {                               \
    f->EmitCode(code, sizeof(code)); \
    f->Emit(kExprEnd);               \
  } while (false)

}  // namespace

TEST_F(RunWasmModuleTest, Run_WasmModule_Return114) {
  {
    static const int32_t kReturnValue = 114;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_I32V_2(kReturnValue)};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kReturnValue);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_CallAdd) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);

    WasmFunctionBuilder* f1 = builder->AddFunction(TestSignatures::i_ii());
    uint16_t param1 = 0;
    uint16_t param2 = 1;
    uint8_t code1[] = {
        WASM_I32_ADD(WASM_LOCAL_GET(param1), WASM_LOCAL_GET(param2))};
    EMIT_CODE_WITH_END(f1, code1);

    WasmFunctionBuilder* f2 = builder->AddFunction(TestSignatures::i_v());

    ExportAsMain(f2);
    uint8_t code2[] = {
        WASM_CALL_FUNCTION(f1->func_index(), WASM_I32V_2(77), WASM_I32V_1(22))};
    EMIT_CODE_WITH_END(f2, code2);
    TestModule(&zone, builder, 99);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_ReadLoadedDataSegment) {
  {
    static const uint8_t kDataSegmentDest0 = 12;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());

    ExportAsMain(f);
    uint8_t code[] = {
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(kDataSegmentDest0))};
    EMIT_CODE_WITH_END(f, code);
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    builder->AddDataSegment(data, sizeof(data), kDataSegmentDest0);
    TestModule(&zone, builder, 0xDDCCBBAA);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_CheckMemoryIsZero) {
  {
    static const int kCheckSize = 16 * 1024;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());

    uint16_t localIndex = f->AddLocal(kWasmI32);
    ExportAsMain(f);
    uint8_t code[] = {WASM_BLOCK_I(
        WASM_WHILE(
            WASM_I32_LTS(WASM_LOCAL_GET(localIndex), WASM_I32V_3(kCheckSize)),
            WASM_IF_ELSE(
                WASM_LOAD_MEM(MachineType::Int32(), WASM_LOCAL_GET(localIndex)),
                WASM_BRV(3, WASM_I32V_1(-1)),
                WASM_INC_LOCAL_BY(localIndex, 4))),
        WASM_I32V_1(11))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 11);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_CallMain_recursive) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());

    uint16_t localIndex = f->AddLocal(kWasmI32);
    ExportAsMain(f);
    uint8_t code[] = {
        WASM_LOCAL_SET(localIndex,
                       WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
        WASM_IF_ELSE_I(WASM_I32_LTS(WASM_LOCAL_GET(localIndex), WASM_I32V_1(5)),
                       WASM_SEQ(WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                               WASM_INC_LOCAL(localIndex)),
                                WASM_CALL_FUNCTION0(0)),
                       WASM_I32V_1(55))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 55);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    uint32_t global1 = builder->AddGlobal(kWasmI32, true, WasmInitExpr(0));
    uint32_t global2 = builder->AddGlobal(kWasmI32, true, WasmInitExpr(0));
    WasmFunctionBuilder* f1 = builder->AddFunction(TestSignatures::i_v());
    uint8_t code1[] = {
        WASM_I32_ADD(WASM_GLOBAL_GET(global1), WASM_GLOBAL_GET(global2))};
    EMIT_CODE_WITH_END(f1, code1);
    WasmFunctionBuilder* f2 = builder->AddFunction(TestSignatures::i_v());
    ExportAsMain(f2);
    uint8_t code2[] = {WASM_GLOBAL_SET(global1, WASM_I32V_1(56)),
                       WASM_GLOBAL_SET(global2, WASM_I32V_1(41)),
                       WASM_RETURN(WASM_CALL_FUNCTION0(f1->func_index()))};
    EMIT_CODE_WITH_END(f2, code2);
    TestModule(&zone, builder, 97);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, MemorySize) {
  {
    // Initial memory size is 16, see wasm-module-builder.cc
    static const int kExpectedValue = 16;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_MemSize_GrowMem) {
  {
    // Initial memory size = 16 + MemoryGrow(10)
    static const int kExpectedValue = 26;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_I32V_1(10)), WASM_DROP,
                      WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, MemoryGrowZero) {
  {
    // Initial memory size is 16, see wasm-module-builder.cc
    static const int kExpectedValue = 16;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_I32V(0))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

class InterruptThread : public v8::base::Thread {
 public:
  explicit InterruptThread(Isolate* isolate, std::atomic<int32_t>* memory)
      : Thread(Options("TestInterruptLoop")),
        isolate_(isolate),
        memory_(memory) {}

  static void OnInterrupt(v8::Isolate* isolate, void* data) {
    int32_t* m = reinterpret_cast<int32_t*>(data);
    // Set the interrupt location to 0 to break the loop in {TestInterruptLoop}.
    Address ptr = reinterpret_cast<Address>(&m[interrupt_location_]);
    WriteLittleEndianValue<int32_t>(ptr, interrupt_value_);
  }

  void Run() override {
    // Wait for the main thread to write the signal value.
    int32_t val = 0;
    do {
      val = memory_[0].load(std::memory_order_relaxed);
      val = ReadLittleEndianValue<int32_t>(reinterpret_cast<Address>(&val));
    } while (val != signal_value_);
    isolate_->RequestInterrupt(&OnInterrupt, memory_);
  }

  Isolate* isolate_;
  std::atomic<int32_t>* memory_;
  static const int32_t interrupt_location_ = 10;
  static const int32_t interrupt_value_ = 154;
  static const int32_t signal_value_ = 1221;
};

TEST_F(RunWasmModuleTest, TestInterruptLoop) {
  {
    // Do not dump the module of this test because it contains an infinite loop.
    if (v8_flags.dump_wasm_module) return;

    // This test tests that WebAssembly loops can be interrupted, i.e. that if
    // an
    // InterruptCallback is registered by {Isolate::RequestInterrupt}, then the
    // InterruptCallback is eventually called even if a loop in WebAssembly code
    // is executed.
    // Test setup:
    // The main thread executes a WebAssembly function with a loop. In the loop
    // {signal_value_} is written to memory to signal a helper thread that the
    // main thread reached the loop in the WebAssembly program. When the helper
    // thread reads {signal_value_} from memory, it registers the
    // InterruptCallback. Upon exeution, the InterruptCallback write into the
    // WebAssemblyMemory to end the loop in the WebAssembly program.
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());
    ExportAsMain(f);
    uint8_t code[] = {
        WASM_LOOP(
            WASM_IF(WASM_NOT(WASM_LOAD_MEM(
                        MachineType::Int32(),
                        WASM_I32V(InterruptThread::interrupt_location_ * 4))),
                    WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                   WASM_I32V(InterruptThread::signal_value_)),
                    WASM_BR(1))),
        WASM_I32V(121)};
    EMIT_CODE_WITH_END(f, code);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);

    HandleScope scope(isolate);
    ErrorThrower thrower(isolate, "Test");
    const DirectHandle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(isolate, &thrower,
                                        base::VectorOf(buffer))
            .ToHandleChecked();

    CppGCManaged<BackingStore>::Ptr backing_store =
        instance->trusted_data(isolate)->memory_object(0)->backing_store();
    std::atomic<int32_t>* memory_array =
        reinterpret_cast<std::atomic<int32_t>*>(backing_store->buffer_start());

    InterruptThread thread(isolate, memory_array);
    CHECK(thread.Start());
    testing::CallWasmFunctionForTesting(isolate, instance, "main", {});
    Address address = reinterpret_cast<Address>(
        &memory_array[InterruptThread::interrupt_location_]);
    CHECK_EQ(InterruptThread::interrupt_value_,
             ReadLittleEndianValue<int32_t>(address));
  }
  Cleanup();
}

namespace {

class Regress577353387OneByteResource
    : public v8::String::ExternalOneByteStringResource {
 public:
  Regress577353387OneByteResource(size_t length, char ch,
                                  std::shared_ptr<bool> disposed = nullptr)
      : data_(length, ch), disposed_(std::move(disposed)) {}
  ~Regress577353387OneByteResource() override {
    std::fill(data_.begin(), data_.end(), 'X');
    if (disposed_) *disposed_ = true;
  }
  const char* data() const override { return data_.data(); }
  size_t length() const override { return data_.size(); }

 private:
  std::string data_;
  std::shared_ptr<bool> disposed_;
};

class Regress577353387TwoByteResource
    : public v8::String::ExternalStringResource {
 public:
  Regress577353387TwoByteResource(size_t length, uint16_t ch,
                                  std::shared_ptr<bool> disposed = nullptr)
      : data_(length, ch), disposed_(std::move(disposed)) {}
  ~Regress577353387TwoByteResource() override {
    std::fill(data_.begin(), data_.end(), static_cast<uint16_t>('X'));
    if (disposed_) *disposed_ = true;
  }
  const uint16_t* data() const override { return data_.data(); }
  size_t length() const override { return data_.size(); }

 private:
  std::vector<uint16_t> data_;
  std::shared_ptr<bool> disposed_;
};

DirectHandle<SeqOneByteString> MakeOldOneByteString(Isolate* isolate,
                                                    int length, uint8_t ch) {
  DirectHandle<SeqOneByteString> str =
      isolate->factory()
          ->NewRawOneByteString(length, AllocationType::kOld)
          .ToHandleChecked();
  DisallowGarbageCollection no_gc;
  std::fill_n(str->GetChars(no_gc), length, ch);
  return str;
}

DirectHandle<SeqTwoByteString> MakeOldTwoByteString(Isolate* isolate,
                                                    int length, base::uc16 ch) {
  DirectHandle<SeqTwoByteString> str =
      isolate->factory()
          ->NewRawTwoByteString(length, AllocationType::kOld)
          .ToHandleChecked();
  DisallowGarbageCollection no_gc;
  std::fill_n(str->GetChars(no_gc), length, ch);
  return str;
}

struct Regress577353387State {
  static constexpr int kStringLen = 64;

  v8::Global<v8::String> str_one_byte;
  v8::Global<v8::String> str_two_byte;
  v8::Global<v8::String> str_thin;
  v8::Global<v8::String> str_seq_to_thin;
  v8::Global<v8::String> str_ext_to_thin;
  v8::Global<v8::String> str_ext_miss;

  std::unique_ptr<Regress577353387OneByteResource> res_one_byte;
  std::unique_ptr<Regress577353387TwoByteResource> res_two_byte;
  std::unique_ptr<Regress577353387OneByteResource> res_thin;
  std::shared_ptr<bool> ext_hit_disposed;
  std::shared_ptr<bool> ext_miss_disposed;

  int interrupt_count = 0;

  static void SetupCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
    CHECK(i::ValidateCallbackInfo(info));
    auto* state = reinterpret_cast<Regress577353387State*>(
        info.DataV2().As<v8::External>()->Value(
            v8::kExternalPointerTypeTagDefault));
    info.GetIsolate()->RequestInterrupt(&OnInterrupt, state);
  }

  static void OnInterrupt(v8::Isolate* v8_isolate, void* data) {
    auto* state = reinterpret_cast<Regress577353387State*>(data);
    Isolate* isolate = reinterpret_cast<Isolate*>(v8_isolate);
    CHECK(isolate->is_executing_api_interrupt());
    v8::HandleScope scope(v8_isolate);

    state->interrupt_count++;

    v8::Local<v8::String> s_one_byte = state->str_one_byte.Get(v8_isolate);
    v8::Local<v8::String> s_two_byte = state->str_two_byte.Get(v8_isolate);
    v8::Local<v8::String> s_thin = state->str_thin.Get(v8_isolate);
    v8::Local<v8::String> s_seq_to_thin =
        state->str_seq_to_thin.Get(v8_isolate);
    v8::Local<v8::String> s_ext_to_thin =
        state->str_ext_to_thin.Get(v8_isolate);
    v8::Local<v8::String> s_ext_miss = state->str_ext_miss.Get(v8_isolate);

    CHECK(IsThinString(*v8::Utils::OpenDirectHandle(*s_thin)));

    // 1. In-place externalization must be disallowed during API interrupts.
    CHECK(
        !s_one_byte->CanMakeExternal(v8::String::Encoding::ONE_BYTE_ENCODING));
    CHECK(!s_one_byte->MakeExternal(v8_isolate, state->res_one_byte.get()));
    CHECK(
        !s_two_byte->CanMakeExternal(v8::String::Encoding::TWO_BYTE_ENCODING));
    CHECK(!s_two_byte->MakeExternal(v8_isolate, state->res_two_byte.get()));
    CHECK(!s_thin->CanMakeExternal(v8::String::Encoding::ONE_BYTE_ENCODING));
    CHECK(!s_thin->MakeExternal(v8_isolate, state->res_thin.get()));

    // 2. Internalizing strings (on both StringTable hits and kCopy misses) must
    // skip MakeThin during API interrupts.
    v8::Local<v8::String> int_seq =
        s_seq_to_thin->InternalizeString(v8_isolate);
    v8::Local<v8::String> int_ext_hit =
        s_ext_to_thin->InternalizeString(v8_isolate);
    v8::Local<v8::String> int_ext_miss =
        s_ext_miss->InternalizeString(v8_isolate);

    CHECK(IsInternalizedString(*v8::Utils::OpenDirectHandle(*int_seq)));
    CHECK(IsInternalizedString(*v8::Utils::OpenDirectHandle(*int_ext_hit)));
    CHECK(IsInternalizedString(*v8::Utils::OpenDirectHandle(*int_ext_miss)));

    CHECK(!IsThinString(*v8::Utils::OpenDirectHandle(*s_seq_to_thin)));
    CHECK(!IsThinString(*v8::Utils::OpenDirectHandle(*s_ext_to_thin)));
    CHECK(!IsThinString(*v8::Utils::OpenDirectHandle(*s_ext_miss)));
    CHECK(!*state->ext_hit_disposed);
    CHECK(!*state->ext_miss_disposed);

    uint16_t buf[kStringLen];
    int_ext_miss->Write(v8_isolate, 0, kStringLen, buf);
    CHECK_EQ('F', buf[4]);

    InvokeMemoryReducingMajorGCs(isolate);
    isolate->heap()->EnsureSweepingCompleted(
        Heap::SweepingForcedFinalizationMode::kUnifiedHeap,
        CompleteSweepingReason::kTesting);
    CHECK(!*state->ext_hit_disposed);
    CHECK(!*state->ext_miss_disposed);
  }
};

}  // namespace

// Regression test for https://crbug.com/577353387: API interrupts firing at a
// Wasm loop stack check must not externalize strings in place or transition
// strings to ThinStrings while hoisted StringPrepareForGetCodeUnit pointers
// remain live across loop back edges.
TEST_F(RunWasmModuleTest, Regress577353387) {
#if !defined(V8_ENABLE_TURBOFAN)
  return;
#else
  if (v8_flags.jitless) return;
#if V8_ENABLE_DRUMBRAKE
  if (v8_flags.wasm_jitless) return;
#endif
  FlagScope<bool> no_liftoff(&v8_flags.liftoff, false);
  FlagScope<bool> no_liftoff_only(&v8_flags.liftoff_only, false);
  FlagScope<bool> no_wasm_deopt(&v8_flags.wasm_deopt, false);
#ifdef DEBUG
  FlagScope<bool> wasm_opt(&v8_flags.wasm_opt, true);
  FlagScope<bool> verify_le(&v8_flags.turboshaft_verify_load_elimination, true);
#endif

  {
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    HandleScope scope(isolate);
    v8::Local<v8::Context> context = v8_isolate->GetCurrentContext();

    AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    uint32_t setup_idx =
        builder->AddImport("setup", TestSignatures::v_v(), "env");
    ValueType char_code_at_types[] = {kWasmI32, kWasmExternRef, kWasmI32};
    FunctionSig char_code_at_sig(1, 2, char_code_at_types);
    uint32_t char_code_at_idx =
        builder->AddImport("charCodeAt", &char_code_at_sig, "wasm:js-string");

    ValueType probe_types[] = {kWasmI32,       kWasmExternRef, kWasmExternRef,
                               kWasmExternRef, kWasmExternRef, kWasmExternRef,
                               kWasmExternRef};
    FunctionSig probe_sig(1, 6, probe_types);
    WasmFunctionBuilder* probe = builder->AddFunction(&probe_sig);
    builder->AddExport("probe", probe);
    uint16_t loop_i = probe->AddLocal(kWasmI32);
    uint16_t sum = probe->AddLocal(kWasmI32);

    // 1. Schedule an API interrupt via `setup()`.
    // 2. Hoist `StringPrepareForGetCodeUnit` for all 6 strings before the loop.
    // 3. Loop 3 times: the loop stack check fires `OnInterrupt` while cached
    //    `StringPrepareForGetCodeUnit` pointers remain live, then reads
    //    character offset 4 bytes into the payload (index 4 for one-byte, index
    //    2 for two-byte) of each string.
    uint8_t code[] = {
        WASM_CALL_FUNCTION0(setup_idx),
        WASM_LOCAL_SET(
            sum,
            WASM_I32_ADD(
                WASM_CALL_FUNCTION(char_code_at_idx, WASM_LOCAL_GET(0),
                                   WASM_ZERO),
                WASM_I32_ADD(
                    WASM_CALL_FUNCTION(char_code_at_idx, WASM_LOCAL_GET(1),
                                       WASM_ZERO),
                    WASM_I32_ADD(
                        WASM_CALL_FUNCTION(char_code_at_idx, WASM_LOCAL_GET(2),
                                           WASM_ZERO),
                        WASM_I32_ADD(
                            WASM_CALL_FUNCTION(char_code_at_idx,
                                               WASM_LOCAL_GET(3), WASM_ZERO),
                            WASM_I32_ADD(WASM_CALL_FUNCTION(char_code_at_idx,
                                                            WASM_LOCAL_GET(4),
                                                            WASM_ZERO),
                                         WASM_CALL_FUNCTION(char_code_at_idx,
                                                            WASM_LOCAL_GET(5),
                                                            WASM_ZERO))))))),
        WASM_LOOP(
            WASM_LOCAL_SET(
                sum,
                WASM_I32_ADD(
                    WASM_LOCAL_GET(sum),
                    WASM_I32_ADD(
                        WASM_CALL_FUNCTION(char_code_at_idx, WASM_LOCAL_GET(0),
                                           WASM_I32V_1(4)),
                        WASM_I32_ADD(
                            WASM_CALL_FUNCTION(char_code_at_idx,
                                               WASM_LOCAL_GET(1),
                                               WASM_I32V_1(2)),
                            WASM_I32_ADD(
                                WASM_CALL_FUNCTION(char_code_at_idx,
                                                   WASM_LOCAL_GET(2),
                                                   WASM_I32V_1(4)),
                                WASM_I32_ADD(
                                    WASM_CALL_FUNCTION(char_code_at_idx,
                                                       WASM_LOCAL_GET(3),
                                                       WASM_I32V_1(4)),
                                    WASM_I32_ADD(
                                        WASM_CALL_FUNCTION(char_code_at_idx,
                                                           WASM_LOCAL_GET(4),
                                                           WASM_I32V_1(4)),
                                        WASM_CALL_FUNCTION(
                                            char_code_at_idx, WASM_LOCAL_GET(5),
                                            WASM_I32V_1(4))))))))),
            WASM_LOCAL_SET(loop_i,
                           WASM_I32_ADD(WASM_LOCAL_GET(loop_i), WASM_ONE)),
            WASM_BR_IF(0,
                       WASM_I32_LTS(WASM_LOCAL_GET(loop_i), WASM_I32V_1(3)))),
        WASM_LOCAL_GET(sum)};
    EMIT_CODE_WITH_END(probe, code);

    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);

    ErrorThrower thrower(isolate, "Regress577353387");
    CompileTimeImports compile_imports;
    compile_imports.Add(CompileTimeImport::kJsString);
    DirectHandle<WasmModuleObject> module_object =
        GetWasmEngine()
            ->SyncCompile(isolate, WasmEnabledFeatures::FromIsolate(isolate),
                          std::move(compile_imports), &thrower,
                          base::UniqueCopyOf(base::VectorOf(buffer)))
            .ToHandleChecked();
    CHECK(!thrower.error());

    Regress577353387State state;
    v8::Local<v8::Function> setup_fn =
        v8::FunctionTemplate::New(
            v8_isolate, &Regress577353387State::SetupCallback,
            v8::External::New(v8_isolate, &state,
                              v8::kExternalPointerTypeTagDefault))
            ->GetFunction(context)
            .ToLocalChecked();
    DirectHandle<JSObject> ffi =
        isolate->factory()->NewJSObject(isolate->object_function());
    DirectHandle<JSObject> env =
        isolate->factory()->NewJSObject(isolate->object_function());
    JSObject::AddProperty(isolate, env,
                          isolate->factory()->InternalizeUtf8String("setup"),
                          v8::Utils::OpenDirectHandle(*setup_fn), NONE);
    JSObject::AddProperty(isolate, ffi,
                          isolate->factory()->InternalizeUtf8String("env"), env,
                          NONE);
    DirectHandle<WasmInstanceObject> instance =
        GetWasmEngine()
            ->SyncInstantiate(isolate, &thrower, module_object, ffi)
            .ToHandleChecked();
    CHECK(!thrower.error());

    constexpr int kLen = Regress577353387State::kStringLen;
    constexpr base::uc16 kTwoByteChar = 0x0142;

    // Pre-internalize targets for ThinString setup and StringTable hits.
    DirectHandle<String> thin_target = isolate->factory()->InternalizeString(
        MakeOldOneByteString(isolate, kLen, 'C'));
    CHECK(IsSeqOneByteString(*thin_target));
    isolate->factory()->InternalizeString(
        MakeOldOneByteString(isolate, kLen, 'D'));
    isolate->factory()->InternalizeString(
        MakeOldOneByteString(isolate, kLen, 'E'));

    DirectHandle<String> str_one_byte =
        MakeOldOneByteString(isolate, kLen, 'A');
    DirectHandle<String> str_two_byte =
        MakeOldTwoByteString(isolate, kLen, kTwoByteChar);
    DirectHandle<String> str_thin = MakeOldOneByteString(isolate, kLen, 'C');
    isolate->factory()->InternalizeString(str_thin);
    CHECK(IsThinString(*str_thin));
    DirectHandle<String> str_seq_to_thin =
        MakeOldOneByteString(isolate, kLen, 'D');

    state.ext_hit_disposed = std::make_shared<bool>(false);
    auto res_ext_hit = std::make_unique<Regress577353387OneByteResource>(
        kLen, 'E', state.ext_hit_disposed);
    v8::Local<v8::String> v8_str_ext_to_thin =
        v8::String::NewExternalOneByte(v8_isolate, res_ext_hit.get())
            .ToLocalChecked();
    res_ext_hit.release();

    state.ext_miss_disposed = std::make_shared<bool>(false);
    // Use a two-byte external string with one-byte content ('F') not yet in
    // the StringTable so LookupString takes the kCopy +
    // SetInternalizedReference path on a table miss rather than internalizing
    // the external map in place.
    auto res_ext_miss = std::make_unique<Regress577353387TwoByteResource>(
        kLen, 'F', state.ext_miss_disposed);
    v8::Local<v8::String> v8_str_ext_miss =
        v8::String::NewExternalTwoByte(v8_isolate, res_ext_miss.get())
            .ToLocalChecked();
    res_ext_miss.release();

    state.res_one_byte =
        std::make_unique<Regress577353387OneByteResource>(kLen, 'A');
    state.res_two_byte =
        std::make_unique<Regress577353387TwoByteResource>(kLen, kTwoByteChar);
    state.res_thin =
        std::make_unique<Regress577353387OneByteResource>(kLen, 'C');

    v8::Local<v8::String> v8_str_one_byte = v8::Utils::ToLocal(str_one_byte);
    v8::Local<v8::String> v8_str_two_byte = v8::Utils::ToLocal(str_two_byte);
    v8::Local<v8::String> v8_str_thin = v8::Utils::ToLocal(str_thin);
    v8::Local<v8::String> v8_str_seq_to_thin =
        v8::Utils::ToLocal(str_seq_to_thin);

    CHECK(v8_str_one_byte->CanMakeExternal(
        v8::String::Encoding::ONE_BYTE_ENCODING));
    CHECK(v8_str_two_byte->CanMakeExternal(
        v8::String::Encoding::TWO_BYTE_ENCODING));
    CHECK(
        v8_str_thin->CanMakeExternal(v8::String::Encoding::ONE_BYTE_ENCODING));

    state.str_one_byte.Reset(v8_isolate, v8_str_one_byte);
    state.str_two_byte.Reset(v8_isolate, v8_str_two_byte);
    state.str_thin.Reset(v8_isolate, v8_str_thin);
    state.str_seq_to_thin.Reset(v8_isolate, v8_str_seq_to_thin);
    state.str_ext_to_thin.Reset(v8_isolate, v8_str_ext_to_thin);
    state.str_ext_miss.Reset(v8_isolate, v8_str_ext_miss);

    DirectHandle<Object> args[] = {
        str_one_byte,
        str_two_byte,
        str_thin,
        str_seq_to_thin,
        v8::Utils::OpenDirectHandle(*v8_str_ext_to_thin),
        v8::Utils::OpenDirectHandle(*v8_str_ext_miss),
    };
    int32_t result = testing::CallWasmFunctionForTesting(
        isolate, instance, "probe", base::VectorOf(args));

    constexpr int32_t kExpectedIterSum =
        'A' + kTwoByteChar + 'C' + 'D' + 'E' + 'F';
    CHECK_EQ(1, state.interrupt_count);
    CHECK_EQ(4 * kExpectedIterSum, result);
    CHECK(!isolate->is_executing_api_interrupt());

    // Once `probe` has returned, both externalization and MakeThin succeed.
    CHECK(v8_str_one_byte->CanMakeExternal(
        v8::String::Encoding::ONE_BYTE_ENCODING));
    CHECK(v8_str_one_byte->MakeExternal(v8_isolate, state.res_one_byte.get()));
    state.res_one_byte.release();

    CHECK(v8_str_two_byte->CanMakeExternal(
        v8::String::Encoding::TWO_BYTE_ENCODING));
    CHECK(v8_str_two_byte->MakeExternal(v8_isolate, state.res_two_byte.get()));
    state.res_two_byte.release();

    CHECK(
        v8_str_thin->CanMakeExternal(v8::String::Encoding::ONE_BYTE_ENCODING));
    CHECK(v8_str_thin->MakeExternal(v8_isolate, state.res_thin.get()));
    state.res_thin.release();

    v8_str_seq_to_thin->InternalizeString(v8_isolate);
    v8_str_ext_to_thin->InternalizeString(v8_isolate);
    v8_str_ext_miss->InternalizeString(v8_isolate);
    CHECK(IsThinString(*str_seq_to_thin));
    CHECK(IsThinString(*v8::Utils::OpenDirectHandle(*v8_str_ext_to_thin)));
    CHECK(IsThinString(*v8::Utils::OpenDirectHandle(*v8_str_ext_miss)));
    {
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(
          isolate->heap());
      InvokeMemoryReducingMajorGCs(isolate);
      isolate->heap()->EnsureSweepingCompleted(
          Heap::SweepingForcedFinalizationMode::kUnifiedHeap,
          CompleteSweepingReason::kTesting);
    }
    CHECK(*state.ext_hit_disposed);
    CHECK(*state.ext_miss_disposed);
  }
  Cleanup();
#endif  // !defined(V8_ENABLE_TURBOFAN)
}

TEST_F(RunWasmModuleTest, Run_WasmModule_MemoryGrowInIf) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_IF_ELSE_I(
        WASM_I32V(0), WASM_MEMORY_GROW(WASM_I32V(1)), WASM_I32V(12))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 12);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_GrowMemOobOffset) {
  {
    static const int kPageSize = 0x10000;
    // Initial memory size = 16 + MemoryGrow(10)
    static const int index = kPageSize * 17 + 4;
    int value = 0xACED;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_v());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_I32V_1(1)),
                      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                                     WASM_I32V(value))};
    EMIT_CODE_WITH_END(f, code);
    TestModuleException(&zone, builder);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_GrowMemOobFixedIndex) {
  {
    static const int kPageSize = 0x10000;
    // Initial memory size = 16 + MemoryGrow(10)
    static const int index = kPageSize * 26 + 4;
    int value = 0xACED;
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    Zone zone(isolate->allocator(), ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_i());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_LOCAL_GET(0)), WASM_DROP,
                      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                                     WASM_I32V(value)),
                      WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(index))};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);

    ErrorThrower thrower(isolate, "Test");
    DirectHandle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(isolate, &thrower,
                                        base::VectorOf(buffer))
            .ToHandleChecked();

    // Initial memory size is 16 pages, should trap till index > MemSize on
    // consecutive GrowMem calls
    for (uint32_t i = 1; i < 5; i++) {
      DirectHandle<Object> params[1] = {
          direct_handle(Smi::FromInt(i), isolate)};
      v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
      testing::CallWasmFunctionForTesting(isolate, instance, "main",
                                          base::ArrayVector(params));
      CHECK(try_catch.HasCaught());
      isolate->clear_exception();
    }

    DirectHandle<Object> params[1] = {direct_handle(Smi::FromInt(1), isolate)};
    int32_t result = testing::CallWasmFunctionForTesting(
        isolate, instance, "main", base::ArrayVector(params));
    CHECK_EQ(0xACED, result);
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_GrowMemOobVariableIndex) {
  {
    static const int kPageSize = 0x10000;
    int value = 0xACED;
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    builder->AddMemory(16);
    WasmFunctionBuilder* f = builder->AddFunction(TestSignatures::i_i());
    ExportAsMain(f);
    uint8_t code[] = {WASM_MEMORY_GROW(WASM_I32V_1(1)), WASM_DROP,
                      WASM_STORE_MEM(MachineType::Int32(), WASM_LOCAL_GET(0),
                                     WASM_I32V(value)),
                      WASM_LOAD_MEM(MachineType::Int32(), WASM_LOCAL_GET(0))};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);

    ErrorThrower thrower(isolate, "Test");
    DirectHandle<WasmInstanceObject> instance =
        CompileAndInstantiateForTesting(isolate, &thrower,
                                        base::VectorOf(buffer))
            .ToHandleChecked();

    // Initial memory size is 16 pages, should trap till index > MemSize on
    // consecutive GrowMem calls
    for (int i = 1; i < 5; i++) {
      DirectHandle<Object> params[] = {
          direct_handle(Smi::FromInt((16 + i) * kPageSize - 3), isolate)};
      v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
      testing::CallWasmFunctionForTesting(isolate, instance, "main",
                                          base::ArrayVector(params));
      CHECK(try_catch.HasCaught());
      isolate->clear_exception();
    }

    for (int i = 1; i < 5; i++) {
      DirectHandle<Object> params[] = {
          direct_handle(Smi::FromInt((20 + i) * kPageSize - 4), isolate)};
      int32_t result = testing::CallWasmFunctionForTesting(
          isolate, instance, "main", base::ArrayVector(params));
      CHECK_EQ(0xACED, result);
    }

    v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    DirectHandle<Object> params[] = {
        direct_handle(Smi::FromInt(25 * kPageSize), isolate)};
    testing::CallWasmFunctionForTesting(isolate, instance, "main",
                                        base::ArrayVector(params));
    CHECK(try_catch.HasCaught());
    isolate->clear_exception();
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global_init) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    uint32_t global1 =
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(777777));
    uint32_t global2 =
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(222222));
    WasmFunctionBuilder* f1 = builder->AddFunction(TestSignatures::i_v());
    uint8_t code[] = {
        WASM_I32_ADD(WASM_GLOBAL_GET(global1), WASM_GLOBAL_GET(global2))};
    EMIT_CODE_WITH_END(f1, code);
    ExportAsMain(f1);
    TestModule(&zone, builder, 999999);
  }
  Cleanup();
}

template <typename CType>
static void RunWasmModuleGlobalInitTest(ValueType type, CType expected) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    ValueType types[] = {type};
    FunctionSig sig(1, 0, types);

    for (int padding = 0; padding < 5; padding++) {
      // Test with a simple initializer
      WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);

      for (int i = 0; i < padding; i++) {  // pad global before
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(i + 20000));
      }
      uint32_t global = builder->AddGlobal(type, false, WasmInitExpr(expected));
      for (int i = 0; i < padding; i++) {  // pad global after
        builder->AddGlobal(kWasmI32, false, WasmInitExpr(i + 30000));
      }

      WasmFunctionBuilder* f1 = builder->AddFunction(&sig);
      uint8_t code[] = {WASM_GLOBAL_GET(global)};
      EMIT_CODE_WITH_END(f1, code);
      ExportAsMain(f1);
      TestModule(&zone, builder, expected);
    }
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global_i32) {
  RunWasmModuleGlobalInitTest<int32_t>(kWasmI32, -983489);
  RunWasmModuleGlobalInitTest<int32_t>(kWasmI32, 11223344);
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global_f32) {
  RunWasmModuleGlobalInitTest<float>(kWasmF32, -983.9f);
  RunWasmModuleGlobalInitTest<float>(kWasmF32, 1122.99f);
}

TEST_F(RunWasmModuleTest, Run_WasmModule_Global_f64) {
  RunWasmModuleGlobalInitTest<double>(kWasmF64, -833.9);
  RunWasmModuleGlobalInitTest<double>(kWasmF64, 86374.25);
}

TEST_F(RunWasmModuleTest, InitDataAtTheUpperLimit) {
  {
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    HandleScope scope(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const uint8_t data[] = {
        WASM_MODULE_HEADER,   // --
        kMemorySectionCode,   // --
        U32V_1(4),            // section size
        ENTRY_COUNT(1),       // --
        kWithMaximum,         // --
        1,                    // initial size
        2,                    // maximum size
        kDataSectionCode,     // --
        U32V_1(9),            // section size
        ENTRY_COUNT(1),       // --
        0,                    // linear memory index
        WASM_I32V_3(0xFFFF),  // destination offset
        kExprEnd,
        U32V_1(1),  // source size
        'c'         // data bytes
    };

    CompileAndInstantiateForTesting(isolate, &thrower, base::VectorOf(data));
    if (thrower.error()) {
      Print(*thrower.Reify());
      FATAL("compile or instantiate error");
    }
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, EmptyMemoryNonEmptyDataSegment) {
  {
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    HandleScope scope(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const uint8_t data[] = {
        WASM_MODULE_HEADER,  // --
        kMemorySectionCode,  // --
        U32V_1(4),           // section size
        ENTRY_COUNT(1),      // --
        kWithMaximum,        // --
        0,                   // initial size
        0,                   // maximum size
        kDataSectionCode,    // --
        U32V_1(7),           // section size
        ENTRY_COUNT(1),      // --
        0,                   // linear memory index
        WASM_I32V_1(8),      // destination offset
        kExprEnd,
        U32V_1(1),  // source size
        'c'         // data bytes
    };

    CompileAndInstantiateForTesting(isolate, &thrower, base::VectorOf(data));
    // It should not be possible to instantiate this module.
    CHECK(thrower.error());
  }
  Cleanup();
}

TEST_F(RunWasmModuleTest, EmptyMemoryEmptyDataSegment) {
  {
    Isolate* isolate = WasmExecutionTestBase::InitIsolateOnce();
    HandleScope scope(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const uint8_t data[] = {
        WASM_MODULE_HEADER,  // --
        kMemorySectionCode,  // --
        U32V_1(4),           // section size
        ENTRY_COUNT(1),      // --
        kWithMaximum,        // --
        0,                   // initial size
        0,                   // maximum size
        kDataSectionCode,    // --
        U32V_1(6),           // section size
        ENTRY_COUNT(1),      // --
        0,                   // linear memory index
        WASM_I32V_1(0),      // destination offset
        kExprEnd,
        U32V_1(0),  // source size
    };

    CompileAndInstantiateForTesting(isolate, &thrower, base::VectorOf(data));
    // It should be possible to instantiate this module.
    CHECK(!thrower.error());
  }
  Cleanup();
}

void TestFailedInstantiation(base::Vector<const uint8_t> module_bytes,
                             const char* scenario) {
  Isolate* isolate = WasmExecutionTestBase::current_i_isolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(isolate, scenario);
  DirectHandle<WasmModuleObject> module_object =
      testing::CompileForTesting(isolate, &thrower, module_bytes)
          .ToHandleChecked();
  DirectHandle<WasmMemoryObject> memory_object =
      WasmMemoryObject::New(isolate, 1, WasmMemoryObject::kNoMaximum,
                            SharedFlag{false}, AddressType::kI32)
          .ToHandleChecked();
  DirectHandle<JSObject> ffi =
      isolate->factory()->NewJSObject(isolate->object_function());
  DirectHandle<JSObject> mod =
      isolate->factory()->NewJSObject(isolate->object_function());
  DirectHandle<String> m = isolate->factory()->InternalizeUtf8String("m");
  JSObject::AddProperty(isolate, mod, m, memory_object, NONE);
  JSObject::AddProperty(isolate, ffi, m, mod, NONE);

  MaybeDirectHandle<WasmInstanceObject> maybe_instance =
      GetWasmEngine()->SyncInstantiate(isolate, &thrower, module_object, ffi);
  CHECK(maybe_instance.is_null());
  CHECK(thrower.error());

  // Failed instantiation in Build_Phase1 must not register the instance in
  // any lists.
  CHECK_EQ(
      0u, module_object->script()->wasm_weak_instance_list()->length().value());
  CHECK_EQ(0u, memory_object->instances()->length().value());
}

TEST_F(RunWasmModuleTest, FailedInitGlobalsAndTablesUnpublishesInstance) {
  WasmExecutionTestBase::InitIsolateOnce();

  // 1. Trapping constant expression in InitGlobals() (global 0 traps with
  // kWasmTrapArrayTooLarge, leaving global 1 uninitialized).
  const uint8_t data1[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(1), WASM_ARRAY_DEF(kI64Code, true)),
      SECTION(Import, ENTRY_COUNT(1), ADD_COUNT('m'), ADD_COUNT('m'),
              kExternalMemory, kNoMaximum, 1),
      SECTION(Global, ENTRY_COUNT(2),
              // global 0: (ref 0), traps with array.new_default -1
              kRefCode, 0, 0, kExprI32Const, 0x7f,
              WASM_GC_OP(kExprArrayNewDefault), 0, kExprEnd,
              // global 1: (ref 0), left uninitialized in tagged_globals_buffer
              kRefCode, 0, 0, kExprI32Const, 0,
              WASM_GC_OP(kExprArrayNewDefault), 0, kExprEnd),
  };
  TestFailedInstantiation(base::VectorOf(data1), "FailedInitGlobals");

  // 2. Trapping constant expression in SetTableInitialValues() (table 0 traps
  // with kWasmTrapArrayTooLarge, leaving non-nullable table 1 uninitialized).
  const uint8_t data2[] = {
      WASM_MODULE_HEADER,
      SECTION(Type, ENTRY_COUNT(2), SIG_ENTRY_v_v,
              WASM_ARRAY_DEF(kI64Code, true)),
      SECTION(Import, ENTRY_COUNT(1), ADD_COUNT('m'), ADD_COUNT('m'),
              kExternalMemory, kNoMaximum, 1),
      SECTION(Function, ENTRY_COUNT(1), 0),
      SECTION(Table, ENTRY_COUNT(2),
              // table 0: (ref 1) with trapping initializer
              0x40, 0x00, kRefCode, 1, kNoMaximum, 1, kExprI32Const, 0x7f,
              WASM_GC_OP(kExprArrayNewDefault), 1, kExprEnd,
              // table 1: non-nullable function table (ref 0)
              0x40, 0x00, kRefCode, 0, kNoMaximum, 1, kExprRefFunc, 0,
              kExprEnd),
      SECTION(Code, ENTRY_COUNT(1), ADD_COUNT(0, kExprEnd)),
  };
  TestFailedInstantiation(base::VectorOf(data2), "FailedSetTableInitialValues");

  Cleanup();
}

#undef EMIT_CODE_WITH_END

}  // namespace test_run_wasm_module
}  // namespace wasm
}  // namespace internal
}  // namespace v8
