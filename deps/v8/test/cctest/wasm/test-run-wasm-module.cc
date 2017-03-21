// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "src/snapshot/code-serializer.h"
#include "src/version.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

#include "test/cctest/cctest.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-module-runner.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

namespace {
void Cleanup(Isolate* isolate = nullptr) {
  // By sending a low memory notifications, we will try hard to collect all
  // garbage and will therefore also invoke all weak callbacks of actually
  // unreachable persistent handles.
  if (!isolate) {
    isolate = CcTest::InitIsolateOnce();
  }
  reinterpret_cast<v8::Isolate*>(isolate)->LowMemoryNotification();
}

void TestModule(Zone* zone, WasmModuleBuilder* builder,
                int32_t expected_result) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(buffer);

  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);
  int32_t result = testing::CompileAndRunWasmModule(
      isolate, buffer.begin(), buffer.end(), ModuleOrigin::kWasmOrigin);
  CHECK_EQ(expected_result, result);
}

void TestModuleException(Zone* zone, WasmModuleBuilder* builder) {
  ZoneBuffer buffer(zone);
  builder->WriteTo(buffer);

  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);
  v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  testing::CompileAndRunWasmModule(isolate, buffer.begin(), buffer.end(),
                                   ModuleOrigin::kWasmOrigin);
  CHECK(try_catch.HasCaught());
  isolate->clear_pending_exception();
}

void ExportAsMain(WasmFunctionBuilder* f) { f->ExportAs(CStrVector("main")); }

#define EMIT_CODE_WITH_END(f, code)  \
  do {                               \
    f->EmitCode(code, sizeof(code)); \
    f->Emit(kExprEnd);               \
  } while (false)

}  // namespace

TEST(Run_WasmModule_Return114) {
  {
    static const int32_t kReturnValue = 114;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_I32V_2(kReturnValue)};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kReturnValue);
  }
  Cleanup();
}

TEST(Run_WasmModule_CallAdd) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);

    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_ii());
    uint16_t param1 = 0;
    uint16_t param2 = 1;
    byte code1[] = {
        WASM_I32_ADD(WASM_GET_LOCAL(param1), WASM_GET_LOCAL(param2))};
    EMIT_CODE_WITH_END(f1, code1);

    WasmFunctionBuilder* f2 = builder->AddFunction(sigs.i_v());

    ExportAsMain(f2);
    byte code2[] = {
        WASM_CALL_FUNCTION(f1->func_index(), WASM_I32V_2(77), WASM_I32V_1(22))};
    EMIT_CODE_WITH_END(f2, code2);
    TestModule(&zone, builder, 99);
  }
  Cleanup();
}

TEST(Run_WasmModule_ReadLoadedDataSegment) {
  {
    static const byte kDataSegmentDest0 = 12;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    ExportAsMain(f);
    byte code[] = {
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(kDataSegmentDest0))};
    EMIT_CODE_WITH_END(f, code);
    byte data[] = {0xaa, 0xbb, 0xcc, 0xdd};
    builder->AddDataSegment(data, sizeof(data), kDataSegmentDest0);
    TestModule(&zone, builder, 0xddccbbaa);
  }
  Cleanup();
}

TEST(Run_WasmModule_CheckMemoryIsZero) {
  {
    static const int kCheckSize = 16 * 1024;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    uint16_t localIndex = f->AddLocal(kWasmI32);
    ExportAsMain(f);
    byte code[] = {WASM_BLOCK_I(
        WASM_WHILE(
            WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I32V_3(kCheckSize)),
            WASM_IF_ELSE(
                WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(localIndex)),
                WASM_BRV(3, WASM_I32V_1(-1)),
                WASM_INC_LOCAL_BY(localIndex, 4))),
        WASM_I32V_1(11))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 11);
  }
  Cleanup();
}

TEST(Run_WasmModule_CallMain_recursive) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());

    uint16_t localIndex = f->AddLocal(kWasmI32);
    ExportAsMain(f);
    byte code[] = {
        WASM_SET_LOCAL(localIndex,
                       WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
        WASM_IF_ELSE_I(WASM_I32_LTS(WASM_GET_LOCAL(localIndex), WASM_I32V_1(5)),
                       WASM_SEQ(WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                               WASM_INC_LOCAL(localIndex)),
                                WASM_CALL_FUNCTION0(0)),
                       WASM_I32V_1(55))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 55);
  }
  Cleanup();
}

TEST(Run_WasmModule_Global) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    uint32_t global1 = builder->AddGlobal(kWasmI32, 0);
    uint32_t global2 = builder->AddGlobal(kWasmI32, 0);
    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_v());
    byte code1[] = {
        WASM_I32_ADD(WASM_GET_GLOBAL(global1), WASM_GET_GLOBAL(global2))};
    EMIT_CODE_WITH_END(f1, code1);
    WasmFunctionBuilder* f2 = builder->AddFunction(sigs.i_v());
    ExportAsMain(f2);
    byte code2[] = {WASM_SET_GLOBAL(global1, WASM_I32V_1(56)),
                    WASM_SET_GLOBAL(global2, WASM_I32V_1(41)),
                    WASM_RETURN1(WASM_CALL_FUNCTION0(f1->func_index()))};
    EMIT_CODE_WITH_END(f2, code2);
    TestModule(&zone, builder, 97);
  }
  Cleanup();
}

// Approximate gtest TEST_F style, in case we adopt gtest.
class WasmSerializationTest {
 public:
  WasmSerializationTest() : zone_(&allocator_, ZONE_NAME) {
    // Don't call here if we move to gtest.
    SetUp();
  }

  static void BuildWireBytes(Zone* zone, ZoneBuffer* buffer) {
    WasmModuleBuilder* builder = new (zone) WasmModuleBuilder(zone);
    TestSignatures sigs;

    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    byte code[] = {WASM_GET_LOCAL(0), kExprI32Const, 1, kExprI32Add};
    EMIT_CODE_WITH_END(f, code);
    f->ExportAs(CStrVector(kFunctionName));

    builder->WriteTo(*buffer);
  }

  void ClearSerializedData() {
    serialized_bytes_.first = nullptr;
    serialized_bytes_.second = 0;
  }

  void InvalidateVersion() {
    uint32_t* buffer = reinterpret_cast<uint32_t*>(
        const_cast<uint8_t*>(serialized_bytes_.first));
    buffer[SerializedCodeData::kVersionHashOffset] = Version::Hash() + 1;
  }

  void InvalidateWireBytes() {
    memset(const_cast<uint8_t*>(wire_bytes_.first), '\0',
           wire_bytes_.second / 2);
  }

  v8::MaybeLocal<v8::WasmCompiledModule> Deserialize() {
    ErrorThrower thrower(current_isolate(), "");
    v8::MaybeLocal<v8::WasmCompiledModule> deserialized =
        v8::WasmCompiledModule::DeserializeOrCompile(
            current_isolate_v8(), serialized_bytes(), wire_bytes());
    return deserialized;
  }

  void DeserializeAndRun() {
    ErrorThrower thrower(current_isolate(), "");
    v8::Local<v8::WasmCompiledModule> deserialized_module;
    CHECK(Deserialize().ToLocal(&deserialized_module));
    Handle<WasmModuleObject> module_object = Handle<WasmModuleObject>::cast(
        v8::Utils::OpenHandle(*deserialized_module));
    {
      DisallowHeapAllocation assume_no_gc;
      Handle<WasmCompiledModule> compiled_part(
          WasmCompiledModule::cast(module_object->GetInternalField(0)),
          current_isolate());
      CHECK_EQ(memcmp(compiled_part->module_bytes()->GetCharsAddress(),
                      wire_bytes().first, wire_bytes().second),
               0);
    }
    Handle<JSObject> instance =
        WasmModule::Instantiate(current_isolate(), &thrower, module_object,
                                Handle<JSReceiver>::null(),
                                Handle<JSArrayBuffer>::null())
            .ToHandleChecked();
    Handle<Object> params[1] = {
        Handle<Object>(Smi::FromInt(41), current_isolate())};
    int32_t result = testing::CallWasmFunctionForTesting(
        current_isolate(), instance, &thrower, kFunctionName, 1, params,
        ModuleOrigin::kWasmOrigin);
    CHECK(result == 42);
  }

  Isolate* current_isolate() {
    return reinterpret_cast<Isolate*>(current_isolate_v8_);
  }

  ~WasmSerializationTest() {
    // Don't call from here if we move to gtest
    TearDown();
  }

  v8::Isolate* current_isolate_v8() { return current_isolate_v8_; }

 private:
  static const char* kFunctionName;

  Zone* zone() { return &zone_; }
  const v8::WasmCompiledModule::CallerOwnedBuffer& wire_bytes() const {
    return wire_bytes_;
  }

  const v8::WasmCompiledModule::CallerOwnedBuffer& serialized_bytes() const {
    return serialized_bytes_;
  }

  void SetUp() {
    WasmModuleBuilder* builder = new (zone()) WasmModuleBuilder(zone());
    TestSignatures sigs;

    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    byte code[] = {WASM_GET_LOCAL(0), kExprI32Const, 1, kExprI32Add};
    EMIT_CODE_WITH_END(f, code);
    f->ExportAs(CStrVector(kFunctionName));

    ZoneBuffer buffer(&zone_);
    WasmSerializationTest::BuildWireBytes(zone(), &buffer);

    Isolate* serialization_isolate = CcTest::InitIsolateOnce();
    ErrorThrower thrower(serialization_isolate, "");
    uint8_t* bytes = nullptr;
    size_t bytes_size = 0;
    {
      HandleScope scope(serialization_isolate);
      testing::SetupIsolateForWasmModule(serialization_isolate);

      ModuleResult decoding_result =
          DecodeWasmModule(serialization_isolate, buffer.begin(), buffer.end(),
                           false, kWasmOrigin);
      CHECK(!decoding_result.failed());

      Handle<WasmModuleWrapper> module_wrapper = WasmModuleWrapper::New(
          serialization_isolate, const_cast<WasmModule*>(decoding_result.val));

      MaybeHandle<WasmCompiledModule> compiled_module =
          decoding_result.val->CompileFunctions(
              serialization_isolate, module_wrapper, &thrower,
              ModuleWireBytes(buffer.begin(), buffer.end()),
              Handle<Script>::null(), Vector<const byte>::empty());
      CHECK(!compiled_module.is_null());
      Handle<JSObject> module_obj = WasmModuleObject::New(
          serialization_isolate, compiled_module.ToHandleChecked());
      v8::Local<v8::Object> v8_module_obj = v8::Utils::ToLocal(module_obj);
      CHECK(v8_module_obj->IsWebAssemblyCompiledModule());

      v8::Local<v8::WasmCompiledModule> v8_compiled_module =
          v8_module_obj.As<v8::WasmCompiledModule>();
      v8::Local<v8::String> uncompiled_bytes =
          v8_compiled_module->GetWasmWireBytes();
      bytes_size = static_cast<size_t>(uncompiled_bytes->Length());
      bytes = zone()->NewArray<uint8_t>(bytes_size);
      uncompiled_bytes->WriteOneByte(bytes, 0, uncompiled_bytes->Length(),
                                     v8::String::NO_NULL_TERMINATION);
      // keep alive data_ until the end
      data_ = v8_compiled_module->Serialize();
    }

    wire_bytes_ = {const_cast<const uint8_t*>(bytes), bytes_size};

    serialized_bytes_ = {data_.first.get(), data_.second};

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        serialization_isolate->array_buffer_allocator();

    current_isolate_v8_ = v8::Isolate::New(create_params);
    v8::HandleScope new_scope(current_isolate_v8());
    v8::Local<v8::Context> deserialization_context =
        v8::Context::New(current_isolate_v8());
    deserialization_context->Enter();
    testing::SetupIsolateForWasmModule(current_isolate());
  }

  void TearDown() {
    current_isolate_v8()->Dispose();
    current_isolate_v8_ = nullptr;
  }

  v8::internal::AccountingAllocator allocator_;
  Zone zone_;
  v8::WasmCompiledModule::SerializedModule data_;
  v8::WasmCompiledModule::CallerOwnedBuffer wire_bytes_;
  v8::WasmCompiledModule::CallerOwnedBuffer serialized_bytes_;
  v8::Isolate* current_isolate_v8_;
};

const char* WasmSerializationTest::kFunctionName = "increment";

TEST(DeserializeValidModule) {
  WasmSerializationTest test;
  {
    HandleScope scope(test.current_isolate());
    test.DeserializeAndRun();
  }
  Cleanup(test.current_isolate());
  Cleanup();
}

TEST(DeserializeMismatchingVersion) {
  WasmSerializationTest test;
  {
    HandleScope scope(test.current_isolate());
    test.InvalidateVersion();
    test.DeserializeAndRun();
  }
  Cleanup(test.current_isolate());
  Cleanup();
}

TEST(DeserializeNoSerializedData) {
  WasmSerializationTest test;
  {
    HandleScope scope(test.current_isolate());
    test.ClearSerializedData();
    test.DeserializeAndRun();
  }
  Cleanup(test.current_isolate());
  Cleanup();
}

TEST(DeserializeWireBytesAndSerializedDataInvalid) {
  WasmSerializationTest test;
  {
    HandleScope scope(test.current_isolate());
    test.InvalidateVersion();
    test.InvalidateWireBytes();
    test.Deserialize();
  }
  Cleanup(test.current_isolate());
  Cleanup();
}

bool False(v8::Local<v8::Context> context) { return false; }

TEST(BlockWasmCodeGen) {
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  ZoneBuffer buffer(&zone);
  WasmSerializationTest::BuildWireBytes(&zone, &buffer);
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);
  testing::SetupIsolateForWasmModule(isolate);
  CcTest::isolate()->SetAllowCodeGenerationFromStringsCallback(False);

  ErrorThrower thrower(isolate, "block codegen");
  MaybeHandle<WasmModuleObject> ret = wasm::CreateModuleObjectFromBytes(
      isolate, buffer.begin(), buffer.end(), &thrower,
      wasm::ModuleOrigin::kWasmOrigin, Handle<v8::internal::Script>::null(),
      Vector<const byte>::empty());
  CcTest::isolate()->SetAllowCodeGenerationFromStringsCallback(nullptr);
  CHECK(ret.is_null());
  CHECK(thrower.error());
}

TEST(BlockWasmCodeGenAtDeserialization) {
  WasmSerializationTest test;
  {
    HandleScope scope(test.current_isolate());
    test.current_isolate_v8()->SetAllowCodeGenerationFromStringsCallback(False);
    v8::MaybeLocal<v8::WasmCompiledModule> nothing = test.Deserialize();
    CHECK(nothing.IsEmpty());
  }
  Cleanup(test.current_isolate());
  Cleanup();
}

TEST(MemorySize) {
  {
    // Initial memory size is 16, see wasm-module-builder.cc
    static const int kExpectedValue = 16;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

TEST(Run_WasmModule_MemSize_GrowMem) {
  {
    // Initial memory size = 16 + GrowMemory(10)
    static const int kExpectedValue = 26;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V_1(10)), WASM_DROP,
                   WASM_MEMORY_SIZE};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

TEST(GrowMemoryZero) {
  {
    // Initial memory size is 16, see wasm-module-builder.cc
    static const int kExpectedValue = 16;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V(0))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, kExpectedValue);
  }
  Cleanup();
}

class InterruptThread : public v8::base::Thread {
 public:
  explicit InterruptThread(Isolate* isolate, int32_t* memory)
      : Thread(Options("TestInterruptLoop")),
        isolate_(isolate),
        memory_(memory) {}

  static void OnInterrupt(v8::Isolate* isolate, void* data) {
    int32_t* m = reinterpret_cast<int32_t*>(data);
    // Set the interrupt location to 0 to break the loop in {TestInterruptLoop}.
    int32_t* ptr = &m[interrupt_location_];
    WriteLittleEndianValue<int32_t>(ptr, interrupt_value_);
  }

  virtual void Run() {
    // Wait for the main thread to write the signal value.
    int32_t val = 0;
    do {
      val = memory_[0];
      val = ReadLittleEndianValue<int32_t>(&val);
    } while (val != signal_value_);
    isolate_->RequestInterrupt(&OnInterrupt, const_cast<int32_t*>(memory_));
  }

  Isolate* isolate_;
  volatile int32_t* memory_;
  static const int32_t interrupt_location_ = 10;
  static const int32_t interrupt_value_ = 154;
  static const int32_t signal_value_ = 1221;
};

TEST(TestInterruptLoop) {
  {
    // Do not dump the module of this test because it contains an infinite loop.
    if (FLAG_dump_wasm_module) return;

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
    TestSignatures sigs;
    Isolate* isolate = CcTest::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {
        WASM_LOOP(
            WASM_IFB(WASM_NOT(WASM_LOAD_MEM(
                         MachineType::Int32(),
                         WASM_I32V(InterruptThread::interrupt_location_ * 4))),
                     WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                                    WASM_I32V(InterruptThread::signal_value_)),
                     WASM_BR(1))),
        WASM_I32V(121)};
    EMIT_CODE_WITH_END(f, code);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(buffer);

    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "Test");
    const Handle<WasmInstanceObject> instance =
        testing::CompileInstantiateWasmModuleForTesting(
            isolate, &thrower, buffer.begin(), buffer.end(),
            ModuleOrigin::kWasmOrigin);
    CHECK(!instance.is_null());

    MaybeHandle<JSArrayBuffer> maybe_memory =
        GetInstanceMemory(isolate, instance);
    Handle<JSArrayBuffer> memory = maybe_memory.ToHandleChecked();
    int32_t* memory_array = reinterpret_cast<int32_t*>(memory->backing_store());

    InterruptThread thread(isolate, memory_array);
    thread.Start();
    testing::RunWasmModuleForTesting(isolate, instance, 0, nullptr,
                                     ModuleOrigin::kWasmOrigin);
    int32_t val = memory_array[InterruptThread::interrupt_location_];
    CHECK_EQ(InterruptThread::interrupt_value_,
             ReadLittleEndianValue<int32_t>(&val));
  }
  Cleanup();
}

TEST(Run_WasmModule_GrowMemoryInIf) {
  {
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_IF_ELSE_I(WASM_I32V(0), WASM_GROW_MEMORY(WASM_I32V(1)),
                                  WASM_I32V(12))};
    EMIT_CODE_WITH_END(f, code);
    TestModule(&zone, builder, 12);
  }
  Cleanup();
}

TEST(Run_WasmModule_GrowMemOobOffset) {
  {
    static const int kPageSize = 0x10000;
    // Initial memory size = 16 + GrowMemory(10)
    static const int index = kPageSize * 17 + 4;
    int value = 0xaced;
    TestSignatures sigs;
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_v());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V_1(1)),
                   WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                                  WASM_I32V(value))};
    EMIT_CODE_WITH_END(f, code);
    TestModuleException(&zone, builder);
  }
  Cleanup();
}

TEST(Run_WasmModule_GrowMemOobFixedIndex) {
  {
    static const int kPageSize = 0x10000;
    // Initial memory size = 16 + GrowMemory(10)
    static const int index = kPageSize * 26 + 4;
    int value = 0xaced;
    TestSignatures sigs;
    Isolate* isolate = CcTest::InitIsolateOnce();
    Zone zone(isolate->allocator(), ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_GET_LOCAL(0)), WASM_DROP,
                   WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                                  WASM_I32V(value)),
                   WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(index))};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(buffer);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Test");
    Handle<JSObject> instance = testing::CompileInstantiateWasmModuleForTesting(
        isolate, &thrower, buffer.begin(), buffer.end(),
        ModuleOrigin::kWasmOrigin);
    CHECK(!instance.is_null());

    // Initial memory size is 16 pages, should trap till index > MemSize on
    // consecutive GrowMem calls
    for (uint32_t i = 1; i < 5; i++) {
      Handle<Object> params[1] = {Handle<Object>(Smi::FromInt(i), isolate)};
      v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
      testing::RunWasmModuleForTesting(isolate, instance, 1, params,
                                       ModuleOrigin::kWasmOrigin);
      CHECK(try_catch.HasCaught());
      isolate->clear_pending_exception();
    }

    Handle<Object> params[1] = {Handle<Object>(Smi::FromInt(1), isolate)};
    int32_t result = testing::RunWasmModuleForTesting(
        isolate, instance, 1, params, ModuleOrigin::kWasmOrigin);
    CHECK(result == 0xaced);
  }
  Cleanup();
}

TEST(Run_WasmModule_GrowMemOobVariableIndex) {
  {
    static const int kPageSize = 0x10000;
    int value = 0xaced;
    TestSignatures sigs;
    Isolate* isolate = CcTest::InitIsolateOnce();
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    ExportAsMain(f);
    byte code[] = {WASM_GROW_MEMORY(WASM_I32V_1(1)), WASM_DROP,
                   WASM_STORE_MEM(MachineType::Int32(), WASM_GET_LOCAL(0),
                                  WASM_I32V(value)),
                   WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0))};
    EMIT_CODE_WITH_END(f, code);

    HandleScope scope(isolate);
    ZoneBuffer buffer(&zone);
    builder->WriteTo(buffer);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Test");
    Handle<JSObject> instance = testing::CompileInstantiateWasmModuleForTesting(
        isolate, &thrower, buffer.begin(), buffer.end(),
        ModuleOrigin::kWasmOrigin);

    CHECK(!instance.is_null());

    // Initial memory size is 16 pages, should trap till index > MemSize on
    // consecutive GrowMem calls
    for (int i = 1; i < 5; i++) {
      Handle<Object> params[1] = {
          Handle<Object>(Smi::FromInt((16 + i) * kPageSize - 3), isolate)};
      v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
      testing::RunWasmModuleForTesting(isolate, instance, 1, params,
                                       ModuleOrigin::kWasmOrigin);
      CHECK(try_catch.HasCaught());
      isolate->clear_pending_exception();
    }

    for (int i = 1; i < 5; i++) {
      Handle<Object> params[1] = {
          Handle<Object>(Smi::FromInt((20 + i) * kPageSize - 4), isolate)};
      int32_t result = testing::RunWasmModuleForTesting(
          isolate, instance, 1, params, ModuleOrigin::kWasmOrigin);
      CHECK(result == 0xaced);
    }

    v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    Handle<Object> params[1] = {
        Handle<Object>(Smi::FromInt(25 * kPageSize), isolate)};
    testing::RunWasmModuleForTesting(isolate, instance, 1, params,
                                     ModuleOrigin::kWasmOrigin);
    CHECK(try_catch.HasCaught());
    isolate->clear_pending_exception();
  }
  Cleanup();
}

TEST(Run_WasmModule_Global_init) {
  {
    v8::internal::AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);
    TestSignatures sigs;

    WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
    uint32_t global1 =
        builder->AddGlobal(kWasmI32, false, false, WasmInitExpr(777777));
    uint32_t global2 =
        builder->AddGlobal(kWasmI32, false, false, WasmInitExpr(222222));
    WasmFunctionBuilder* f1 = builder->AddFunction(sigs.i_v());
    byte code[] = {
        WASM_I32_ADD(WASM_GET_GLOBAL(global1), WASM_GET_GLOBAL(global2))};
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
    TestSignatures sigs;

    ValueType types[] = {type};
    FunctionSig sig(1, 0, types);

    for (int padding = 0; padding < 5; padding++) {
      // Test with a simple initializer
      WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);

      for (int i = 0; i < padding; i++) {  // pad global before
        builder->AddGlobal(kWasmI32, false, false, WasmInitExpr(i + 20000));
      }
      uint32_t global =
          builder->AddGlobal(type, false, false, WasmInitExpr(expected));
      for (int i = 0; i < padding; i++) {  // pad global after
        builder->AddGlobal(kWasmI32, false, false, WasmInitExpr(i + 30000));
      }

      WasmFunctionBuilder* f1 = builder->AddFunction(&sig);
      byte code[] = {WASM_GET_GLOBAL(global)};
      EMIT_CODE_WITH_END(f1, code);
      ExportAsMain(f1);
      TestModule(&zone, builder, expected);
    }
  }
  Cleanup();
}

TEST(Run_WasmModule_Global_i32) {
  RunWasmModuleGlobalInitTest<int32_t>(kWasmI32, -983489);
  RunWasmModuleGlobalInitTest<int32_t>(kWasmI32, 11223344);
}

TEST(Run_WasmModule_Global_f32) {
  RunWasmModuleGlobalInitTest<float>(kWasmF32, -983.9f);
  RunWasmModuleGlobalInitTest<float>(kWasmF32, 1122.99f);
}

TEST(Run_WasmModule_Global_f64) {
  RunWasmModuleGlobalInitTest<double>(kWasmF64, -833.9);
  RunWasmModuleGlobalInitTest<double>(kWasmF64, 86374.25);
}

TEST(InitDataAtTheUpperLimit) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const byte data[] = {
        WASM_MODULE_HEADER,     // --
        kMemorySectionCode,     // --
        U32V_1(4),              // section size
        ENTRY_COUNT(1),         // --
        kResizableMaximumFlag,  // --
        1,                      // initial size
        2,                      // maximum size
        kDataSectionCode,       // --
        U32V_1(9),              // section size
        ENTRY_COUNT(1),         // --
        0,                      // linear memory index
        WASM_I32V_3(0xffff),    // destination offset
        kExprEnd,
        U32V_1(1),  // source size
        'c'         // data bytes
    };

    testing::CompileInstantiateWasmModuleForTesting(isolate, &thrower, data,
                                                    data + arraysize(data),
                                                    ModuleOrigin::kWasmOrigin);
    if (thrower.error()) {
      thrower.Reify()->Print();
      CHECK(false);
    }
  }
  Cleanup();
}

TEST(EmptyMemoryNonEmptyDataSegment) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const byte data[] = {
        WASM_MODULE_HEADER,     // --
        kMemorySectionCode,     // --
        U32V_1(4),              // section size
        ENTRY_COUNT(1),         // --
        kResizableMaximumFlag,  // --
        0,                      // initial size
        0,                      // maximum size
        kDataSectionCode,       // --
        U32V_1(7),              // section size
        ENTRY_COUNT(1),         // --
        0,                      // linear memory index
        WASM_I32V_1(8),         // destination offset
        kExprEnd,
        U32V_1(1),  // source size
        'c'         // data bytes
    };

    testing::CompileInstantiateWasmModuleForTesting(isolate, &thrower, data,
                                                    data + arraysize(data),
                                                    ModuleOrigin::kWasmOrigin);
    // It should not be possible to instantiate this module.
    CHECK(thrower.error());
  }
  Cleanup();
}

TEST(EmptyMemoryEmptyDataSegment) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const byte data[] = {
        WASM_MODULE_HEADER,     // --
        kMemorySectionCode,     // --
        U32V_1(4),              // section size
        ENTRY_COUNT(1),         // --
        kResizableMaximumFlag,  // --
        0,                      // initial size
        0,                      // maximum size
        kDataSectionCode,       // --
        U32V_1(6),              // section size
        ENTRY_COUNT(1),         // --
        0,                      // linear memory index
        WASM_I32V_1(0),         // destination offset
        kExprEnd,
        U32V_1(0),  // source size
    };

    testing::CompileInstantiateWasmModuleForTesting(isolate, &thrower, data,
                                                    data + arraysize(data),
                                                    ModuleOrigin::kWasmOrigin);
    // It should be possible to instantiate this module.
    CHECK(!thrower.error());
  }
  Cleanup();
}

TEST(MemoryWithOOBEmptyDataSegment) {
  {
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);

    ErrorThrower thrower(isolate, "Run_WasmModule_InitDataAtTheUpperLimit");

    const byte data[] = {
        WASM_MODULE_HEADER,      // --
        kMemorySectionCode,      // --
        U32V_1(4),               // section size
        ENTRY_COUNT(1),          // --
        kResizableMaximumFlag,   // --
        1,                       // initial size
        1,                       // maximum size
        kDataSectionCode,        // --
        U32V_1(9),               // section size
        ENTRY_COUNT(1),          // --
        0,                       // linear memory index
        WASM_I32V_4(0x2468ace),  // destination offset
        kExprEnd,
        U32V_1(0),  // source size
    };

    testing::CompileInstantiateWasmModuleForTesting(isolate, &thrower, data,
                                                    data + arraysize(data),
                                                    ModuleOrigin::kWasmOrigin);
    // It should not be possible to instantiate this module.
    CHECK(thrower.error());
  }
  Cleanup();
}
