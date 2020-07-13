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
#include "src/wasm/wasm-serialization.h"

#include "test/cctest/cctest.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_wasm_serialization {

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
    byte code[] = {WASM_GET_LOCAL(0), kExprI32Const, 1, kExprI32Add, kExprEnd};
    f->EmitCode(code, sizeof(code));
    builder->AddExport(CStrVector(kFunctionName), f);

    builder->WriteTo(buffer);
  }

  void ClearSerializedData() { serialized_bytes_ = {nullptr, 0}; }

  void InvalidateVersion() {
    uint32_t* slot = reinterpret_cast<uint32_t*>(
        const_cast<uint8_t*>(serialized_bytes_.data()) +
        WasmSerializer::kVersionHashOffset);
    *slot = Version::Hash() + 1;
  }

  void InvalidateWireBytes() {
    memset(const_cast<uint8_t*>(wire_bytes_.data()), 0, wire_bytes_.size() / 2);
  }

  void InvalidateNumFunctions() {
    Address num_functions_slot =
        reinterpret_cast<Address>(serialized_bytes_.data()) +
        WasmSerializer::kHeaderSize;
    CHECK_EQ(1, base::ReadUnalignedValue<uint32_t>(num_functions_slot));
    base::WriteUnalignedValue<uint32_t>(num_functions_slot, 0);
  }

  MaybeHandle<WasmModuleObject> Deserialize(
      Vector<const char> source_url = {}) {
    return DeserializeNativeModule(CcTest::i_isolate(),
                                   VectorOf(serialized_bytes_),
                                   VectorOf(wire_bytes_), source_url);
  }

  void DeserializeAndRun() {
    ErrorThrower thrower(CcTest::i_isolate(), "");
    Handle<WasmModuleObject> module_object;
    CHECK(Deserialize().ToHandle(&module_object));
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
        CcTest::i_isolate()
            ->wasm_engine()
            ->SyncInstantiate(CcTest::i_isolate(), &thrower, module_object,
                              Handle<JSReceiver>::null(),
                              MaybeHandle<JSArrayBuffer>())
            .ToHandleChecked();
    Handle<Object> params[1] = {
        Handle<Object>(Smi::FromInt(41), CcTest::i_isolate())};
    int32_t result = testing::CallWasmFunctionForTesting(
        CcTest::i_isolate(), instance, &thrower, kFunctionName, 1, params);
    CHECK_EQ(42, result);
  }

  void CollectGarbage() {
    // Try hard to collect all garbage and will therefore also invoke all weak
    // callbacks of actually unreachable persistent handles.
    CcTest::i_isolate()->heap()->CollectAllAvailableGarbage(
        GarbageCollectionReason::kTesting);
  }

 private:
  static const char* kFunctionName;

  Zone* zone() { return &zone_; }

  void SetUp() {
    CcTest::InitIsolateOnce();
    ZoneBuffer buffer(&zone_);
    WasmSerializationTest::BuildWireBytes(zone(), &buffer);

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        CcTest::i_isolate()->array_buffer_allocator();

    v8::Isolate* serialization_v8_isolate = v8::Isolate::New(create_params);
    Isolate* serialization_isolate =
        reinterpret_cast<Isolate*>(serialization_v8_isolate);
    ErrorThrower thrower(serialization_isolate, "");
    // Keep a weak pointer so we can check that the native module dies after
    // serialization (when the isolate is disposed).
    std::weak_ptr<NativeModule> weak_native_module;
    {
      HandleScope scope(serialization_isolate);
      v8::Local<v8::Context> serialization_context =
          v8::Context::New(serialization_v8_isolate);
      serialization_context->Enter();

      auto enabled_features = WasmFeatures::FromIsolate(serialization_isolate);
      MaybeHandle<WasmModuleObject> maybe_module_object =
          serialization_isolate->wasm_engine()->SyncCompile(
              serialization_isolate, enabled_features, &thrower,
              ModuleWireBytes(buffer.begin(), buffer.end()));
      Handle<WasmModuleObject> module_object =
          maybe_module_object.ToHandleChecked();
      weak_native_module = module_object->shared_native_module();
      // Check that the native module exists at this point.
      CHECK(weak_native_module.lock());

      v8::Local<v8::Object> v8_module_obj =
          v8::Utils::ToLocal(Handle<JSObject>::cast(module_object));
      CHECK(v8_module_obj->IsWasmModuleObject());

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
    // Dispose of serialization isolate to destroy the reference to the
    // NativeModule, which removes it from the module cache in the wasm engine
    // and forces de-serialization in the new isolate.
    serialization_v8_isolate->Dispose();

    // Busy-wait for the NativeModule to really die. Background threads might
    // temporarily keep it alive (happens very rarely, see
    // https://crbug.com/v8/10148).
    while (weak_native_module.lock()) {
    }

    serialized_bytes_ = {data_.buffer.get(), data_.size};

    v8::HandleScope new_scope(CcTest::isolate());
    v8::Local<v8::Context> deserialization_context =
        v8::Context::New(CcTest::isolate());
    deserialization_context->Enter();
  }

  v8::internal::AccountingAllocator allocator_;
  Zone zone_;
  v8::OwnedBuffer data_;
  v8::MemorySpan<const uint8_t> wire_bytes_ = {nullptr, 0};
  v8::MemorySpan<const uint8_t> serialized_bytes_ = {nullptr, 0};
};

const char* WasmSerializationTest::kFunctionName = "increment";

TEST(DeserializeValidModule) {
  WasmSerializationTest test;
  {
    HandleScope scope(CcTest::i_isolate());
    test.DeserializeAndRun();
  }
  test.CollectGarbage();
}

TEST(DeserializeWithSourceUrl) {
  WasmSerializationTest test;
  {
    HandleScope scope(CcTest::i_isolate());
    const std::string url = "http://example.com/example.wasm";
    Handle<WasmModuleObject> module_object;
    CHECK(test.Deserialize(VectorOf(url)).ToHandle(&module_object));
    String source_url = String::cast(module_object->script().source_url());
    CHECK_EQ(url, source_url.ToCString().get());
  }
  test.CollectGarbage();
}

TEST(DeserializeMismatchingVersion) {
  WasmSerializationTest test;
  {
    HandleScope scope(CcTest::i_isolate());
    test.InvalidateVersion();
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

TEST(DeserializeNoSerializedData) {
  WasmSerializationTest test;
  {
    HandleScope scope(CcTest::i_isolate());
    test.ClearSerializedData();
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

TEST(DeserializeInvalidNumFunctions) {
  WasmSerializationTest test;
  {
    HandleScope scope(CcTest::i_isolate());
    test.InvalidateNumFunctions();
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

TEST(DeserializeWireBytesAndSerializedDataInvalid) {
  WasmSerializationTest test;
  {
    HandleScope scope(CcTest::i_isolate());
    test.InvalidateVersion();
    test.InvalidateWireBytes();
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

bool False(v8::Local<v8::Context> context, v8::Local<v8::String> source) {
  return false;
}

TEST(BlockWasmCodeGenAtDeserialization) {
  WasmSerializationTest test;
  {
    HandleScope scope(CcTest::i_isolate());
    CcTest::isolate()->SetAllowWasmCodeGenerationCallback(False);
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

UNINITIALIZED_TEST(CompiledWasmModulesTransfer) {
  i::wasm::WasmEngine::InitializeOncePerProcess();
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneBuffer buffer(&zone);
  WasmSerializationTest::BuildWireBytes(&zone, &buffer);

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* from_isolate = v8::Isolate::New(create_params);
  std::vector<v8::CompiledWasmModule> store;
  std::shared_ptr<NativeModule> original_native_module;
  {
    v8::HandleScope scope(from_isolate);
    LocalContext env(from_isolate);

    Isolate* from_i_isolate = reinterpret_cast<Isolate*>(from_isolate);
    testing::SetupIsolateForWasmModule(from_i_isolate);
    ErrorThrower thrower(from_i_isolate, "TestCompiledWasmModulesTransfer");
    auto enabled_features = WasmFeatures::FromIsolate(from_i_isolate);
    MaybeHandle<WasmModuleObject> maybe_module_object =
        from_i_isolate->wasm_engine()->SyncCompile(
            from_i_isolate, enabled_features, &thrower,
            ModuleWireBytes(buffer.begin(), buffer.end()));
    Handle<WasmModuleObject> module_object =
        maybe_module_object.ToHandleChecked();
    v8::Local<v8::WasmModuleObject> v8_module =
        v8::Local<v8::WasmModuleObject>::Cast(
            v8::Utils::ToLocal(Handle<JSObject>::cast(module_object)));
    store.push_back(v8_module->GetCompiledModule());
    original_native_module = module_object->shared_native_module();
  }

  {
    v8::Isolate* to_isolate = v8::Isolate::New(create_params);
    {
      v8::HandleScope scope(to_isolate);
      LocalContext env(to_isolate);

      v8::MaybeLocal<v8::WasmModuleObject> transferred_module =
          v8::WasmModuleObject::FromCompiledModule(to_isolate, store[0]);
      CHECK(!transferred_module.IsEmpty());
      Handle<WasmModuleObject> module_object = Handle<WasmModuleObject>::cast(
          v8::Utils::OpenHandle(*transferred_module.ToLocalChecked()));
      std::shared_ptr<NativeModule> transferred_native_module =
          module_object->shared_native_module();
      CHECK_EQ(original_native_module, transferred_native_module);
    }
    to_isolate->Dispose();
  }
  original_native_module.reset();
  from_isolate->Dispose();
}

}  // namespace test_wasm_serialization
}  // namespace wasm
}  // namespace internal
}  // namespace v8
