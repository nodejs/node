// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string.h>

#include "src/api/api-inl.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/code-serializer.h"
#include "src/utils/version.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"

#include "test/cctest/cctest.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_wasm_serialization {

namespace {
void Cleanup(Isolate* isolate = CcTest::InitIsolateOnce()) {
  // By sending a low memory notifications, we will try hard to collect all
  // garbage and will therefore also invoke all weak callbacks of actually
  // unreachable persistent handles.
  reinterpret_cast<v8::Isolate*>(isolate)->LowMemoryNotification();
}

#define EMIT_CODE_WITH_END(f, code)  \
  do {                               \
    f->EmitCode(code, sizeof(code)); \
    f->Emit(kExprEnd);               \
  } while (false)

}  // namespace

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
    builder->AddExport(CStrVector(kFunctionName), f);

    builder->WriteTo(buffer);
  }

  void ClearSerializedData() { serialized_bytes_ = {nullptr, 0}; }

  void InvalidateVersion() {
    uint32_t* slot = reinterpret_cast<uint32_t*>(
        const_cast<uint8_t*>(serialized_bytes_.data()) +
        SerializedCodeData::kVersionHashOffset);
    *slot = Version::Hash() + 1;
  }

  void InvalidateWireBytes() {
    memset(const_cast<uint8_t*>(wire_bytes_.data()), 0, wire_bytes_.size() / 2);
  }

  void InvalidateLength() {
    uint32_t* slot = reinterpret_cast<uint32_t*>(
        const_cast<uint8_t*>(serialized_bytes_.data()) +
        SerializedCodeData::kPayloadLengthOffset);
    *slot = 0u;
  }

  v8::MaybeLocal<v8::WasmModuleObject> Deserialize() {
    ErrorThrower thrower(current_isolate(), "");
    v8::MaybeLocal<v8::WasmModuleObject> deserialized =
        v8::WasmModuleObject::DeserializeOrCompile(
            current_isolate_v8(), serialized_bytes_, wire_bytes_);
    return deserialized;
  }

  void DeserializeAndRun() {
    ErrorThrower thrower(current_isolate(), "");
    v8::Local<v8::WasmModuleObject> deserialized_module;
    CHECK(Deserialize().ToLocal(&deserialized_module));
    Handle<WasmModuleObject> module_object = Handle<WasmModuleObject>::cast(
        v8::Utils::OpenHandle(*deserialized_module));
    {
      DisallowHeapAllocation assume_no_gc;
      Vector<const byte> deserialized_module_wire_bytes =
          module_object->native_module()->wire_bytes();
      CHECK_EQ(deserialized_module_wire_bytes.size(), wire_bytes_.size());
      CHECK_EQ(memcmp(deserialized_module_wire_bytes.begin(),
                      wire_bytes_.data(), wire_bytes_.size()),
               0);
    }
    Handle<WasmInstanceObject> instance =
        current_isolate()
            ->wasm_engine()
            ->SyncInstantiate(current_isolate(), &thrower, module_object,
                              Handle<JSReceiver>::null(),
                              MaybeHandle<JSArrayBuffer>())
            .ToHandleChecked();
    Handle<Object> params[1] = {
        Handle<Object>(Smi::FromInt(41), current_isolate())};
    int32_t result = testing::CallWasmFunctionForTesting(
        current_isolate(), instance, &thrower, kFunctionName, 1, params);
    CHECK_EQ(42, result);
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

  void SetUp() {
    ZoneBuffer buffer(&zone_);
    WasmSerializationTest::BuildWireBytes(zone(), &buffer);

    Isolate* serialization_isolate = CcTest::InitIsolateOnce();
    ErrorThrower thrower(serialization_isolate, "");
    {
      HandleScope scope(serialization_isolate);
      testing::SetupIsolateForWasmModule(serialization_isolate);

      auto enabled_features = WasmFeaturesFromIsolate(serialization_isolate);
      MaybeHandle<WasmModuleObject> maybe_module_object =
          serialization_isolate->wasm_engine()->SyncCompile(
              serialization_isolate, enabled_features, &thrower,
              ModuleWireBytes(buffer.begin(), buffer.end()));
      Handle<WasmModuleObject> module_object =
          maybe_module_object.ToHandleChecked();

      v8::Local<v8::Object> v8_module_obj =
          v8::Utils::ToLocal(Handle<JSObject>::cast(module_object));
      CHECK(v8_module_obj->IsWebAssemblyCompiledModule());

      v8::Local<v8::WasmModuleObject> v8_module_object =
          v8_module_obj.As<v8::WasmModuleObject>();
      v8::CompiledWasmModule compiled_module =
          v8_module_object->GetCompiledModule();
      v8::MemorySpan<const uint8_t> uncompiled_bytes =
          compiled_module.GetWireBytesRef();
      uint8_t* bytes_copy = zone()->NewArray<uint8_t>(uncompiled_bytes.size());
      memcpy(bytes_copy, uncompiled_bytes.data(), uncompiled_bytes.size());
      wire_bytes_ = {bytes_copy, uncompiled_bytes.size()};
      // keep alive data_ until the end
      data_ = compiled_module.Serialize();
    }

    serialized_bytes_ = {data_.buffer.get(), data_.size};

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
  v8::OwnedBuffer data_;
  v8::MemorySpan<const uint8_t> wire_bytes_ = {nullptr, 0};
  v8::MemorySpan<const uint8_t> serialized_bytes_ = {nullptr, 0};
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

TEST(DeserializeInvalidLength) {
  WasmSerializationTest test;
  {
    HandleScope scope(test.current_isolate());
    test.InvalidateLength();
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

bool False(v8::Local<v8::Context> context, v8::Local<v8::String> source) {
  return false;
}

TEST(BlockWasmCodeGenAtDeserialization) {
  WasmSerializationTest test;
  {
    HandleScope scope(test.current_isolate());
    test.current_isolate_v8()->SetAllowCodeGenerationFromStringsCallback(False);
    v8::MaybeLocal<v8::WasmModuleObject> nothing = test.Deserialize();
    CHECK(nothing.IsEmpty());
  }
  Cleanup(test.current_isolate());
  Cleanup();
}

namespace {

void TestTransferrableWasmModules(bool should_share) {
  i::wasm::WasmEngine::InitializeOncePerProcess();
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneBuffer buffer(&zone);
  WasmSerializationTest::BuildWireBytes(&zone, &buffer);

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* from_isolate = v8::Isolate::New(create_params);
  std::vector<v8::WasmModuleObject::TransferrableModule> store;
  std::shared_ptr<NativeModule> original_native_module;
  {
    v8::HandleScope scope(from_isolate);
    LocalContext env(from_isolate);

    Isolate* from_i_isolate = reinterpret_cast<Isolate*>(from_isolate);
    testing::SetupIsolateForWasmModule(from_i_isolate);
    ErrorThrower thrower(from_i_isolate, "TestTransferrableWasmModules");
    auto enabled_features = WasmFeaturesFromIsolate(from_i_isolate);
    MaybeHandle<WasmModuleObject> maybe_module_object =
        from_i_isolate->wasm_engine()->SyncCompile(
            from_i_isolate, enabled_features, &thrower,
            ModuleWireBytes(buffer.begin(), buffer.end()));
    Handle<WasmModuleObject> module_object =
        maybe_module_object.ToHandleChecked();
    v8::Local<v8::WasmModuleObject> v8_module =
        v8::Local<v8::WasmModuleObject>::Cast(
            v8::Utils::ToLocal(Handle<JSObject>::cast(module_object)));
    store.push_back(v8_module->GetTransferrableModule());
    original_native_module = module_object->shared_native_module();
  }

  {
    v8::Isolate* to_isolate = v8::Isolate::New(create_params);
    {
      v8::HandleScope scope(to_isolate);
      LocalContext env(to_isolate);

      v8::MaybeLocal<v8::WasmModuleObject> transferred_module =
          v8::WasmModuleObject::FromTransferrableModule(to_isolate, store[0]);
      CHECK(!transferred_module.IsEmpty());
      Handle<WasmModuleObject> module_object = Handle<WasmModuleObject>::cast(
          v8::Utils::OpenHandle(*transferred_module.ToLocalChecked()));
      std::shared_ptr<NativeModule> transferred_native_module =
          module_object->shared_native_module();
      bool is_sharing = (original_native_module == transferred_native_module);
      CHECK_EQ(should_share, is_sharing);
    }
    to_isolate->Dispose();
  }
  original_native_module.reset();
  from_isolate->Dispose();
}

}  // namespace

UNINITIALIZED_TEST(TransferrableWasmModulesCloned) {
  FlagScope<bool> flag_scope_code(&FLAG_wasm_shared_code, false);
  TestTransferrableWasmModules(false);
}

UNINITIALIZED_TEST(TransferrableWasmModulesShared) {
  FlagScope<bool> flag_scope_engine(&FLAG_wasm_shared_engine, true);
  FlagScope<bool> flag_scope_code(&FLAG_wasm_shared_code, true);
  TestTransferrableWasmModules(true);
}

#undef EMIT_CODE_WITH_END

}  // namespace test_wasm_serialization
}  // namespace wasm
}  // namespace internal
}  // namespace v8
