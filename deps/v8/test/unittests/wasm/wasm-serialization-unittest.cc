// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-serialization.h"

#include <stdlib.h>
#include <string.h>

#include <span>

#include "include/v8-wasm.h"
#include "src/api/api-inl.h"
#include "src/base/unique-array.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/code-serializer.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/version.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/common/version-utils.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/wasm/wasm-runner.h"

namespace v8::internal::wasm {

using WasmSerializationTest = WasmExecutionTestBase;
using WasmSerializationUninitializedTest = WasmUninitializedTestBase;

class WasmSerializationTester {
 public:
  explicit WasmSerializationTester(
      const char* embedder_string = Version::GetEmbedder())
      : zone_(&allocator_, ZONE_NAME) {
    SetUp(embedder_string);
  }

  ~WasmSerializationTester() {
    while (WasmExecutionTestBase::current_isolate()->InContext()) {
      WasmExecutionTestBase::current_isolate()->GetCurrentContext()->Exit();
    }
  }

  static constexpr const char* kFunctionName = "increment";

  static void BuildWireBytes(Zone* zone, ZoneBuffer* buffer) {
    WasmModuleBuilder* builder = zone->New<WasmModuleBuilder>(zone);
    TestSignatures sigs;

    // Generate 3 functions, and export the last one with the name "increment".
    WasmFunctionBuilder* f;
    for (int i = 0; i < 3; ++i) {
      f = builder->AddFunction(sigs.i_i());
      f->EmitCode({WASM_LOCAL_GET(0), kExprI32Const, 1, kExprI32Add, kExprEnd});
    }
    builder->AddExport(kFunctionName, f);

    builder->WriteTo(buffer);
  }

  void ClearSerializedData() { serialized_bytes_ = {}; }

  void InvalidateVersion() {
    uint32_t* slot = reinterpret_cast<uint32_t*>(
        const_cast<uint8_t*>(serialized_bytes_.data()) +
        WasmSerializer::kVersionHashOffset);
    *slot = Version::Hash() + 1;
  }

  void InvalidateWireBytes() {
    memset(const_cast<uint8_t*>(wire_bytes_.data()), 0, wire_bytes_.size() / 2);
  }

  MaybeDirectHandle<WasmModuleObject> Deserialize(
      std::string_view source_url = {}) {
    // Create a separate copy because some tests want to deserialize multiple
    // times.
    base::UniqueArray<const uint8_t> wire_bytes_copy =
        base::UniqueCopyOf(wire_bytes_);
    return DeserializeNativeModule(
        WasmExecutionTestBase::current_i_isolate(),
        WasmEnabledFeatures::FromIsolate(
            WasmExecutionTestBase::current_i_isolate()),
        base::VectorOf(serialized_bytes_), wire_bytes_copy, compile_imports_,
        source_url);
  }

  void DeserializeAndRun() {
    ErrorThrower thrower(WasmExecutionTestBase::current_i_isolate(), "");
    DirectHandle<WasmModuleObject> module_object;
    CHECK(Deserialize().ToHandle(&module_object));
    {
      DisallowGarbageCollection assume_no_gc;
      CppGCManaged<wasm::NativeModule>::Ptr native_module =
          module_object->native_module();
      base::Vector<const uint8_t> deserialized_module_wire_bytes =
          native_module->wire_bytes();
      CHECK_EQ(deserialized_module_wire_bytes.size(), wire_bytes_.size());
      CHECK_EQ(memcmp(deserialized_module_wire_bytes.begin(),
                      wire_bytes_.data(), wire_bytes_.size()),
               0);
    }
    DirectHandle<WasmInstanceObject> instance =
        GetWasmEngine()
            ->SyncInstantiate(WasmExecutionTestBase::current_i_isolate(),
                              &thrower, module_object,
                              DirectHandle<JSReceiver>::null())
            .ToHandleChecked();
    DirectHandle<Object> params[] = {direct_handle(
        Smi::FromInt(41), WasmExecutionTestBase::current_i_isolate())};
    int32_t result = testing::CallWasmFunctionForTesting(
        WasmExecutionTestBase::current_i_isolate(), instance, kFunctionName,
        base::ArrayVector(params));
    CHECK_EQ(42, result);
  }

  void CollectGarbage() {
    // Try hard to collect all garbage and will therefore also invoke all weak
    // callbacks of actually unreachable persistent handles.
    // We need to invoke GC without stack, otherwise some objects may survive.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        WasmExecutionTestBase::current_heap());
    InvokeMemoryReducingMajorGCs(WasmExecutionTestBase::current_i_isolate());
  }

  std::span<const uint8_t> wire_bytes() const { return wire_bytes_; }

  CompileTimeImports MakeCompileTimeImports() { return CompileTimeImports{}; }

 private:
  Zone* zone() { return &zone_; }

  void SetUp(const char* embedder_string) {
    WasmExecutionTestBase::InitIsolateOnce();
    ZoneBuffer buffer(&zone_);
    WasmSerializationTester::BuildWireBytes(zone(), &buffer);

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        WasmExecutionTestBase::current_i_isolate()->array_buffer_allocator();

    v8::Isolate* serialization_v8_isolate = v8::Isolate::New(create_params);
    Isolate* serialization_isolate =
        reinterpret_cast<Isolate*>(serialization_v8_isolate);
    ErrorThrower thrower(serialization_isolate, "");
    // Keep a weak pointer so we can check that the native module dies after
    // serialization (when the isolate is disposed).
    std::weak_ptr<NativeModule> weak_native_module;
    {
      v8::Isolate::Scope isolate_scope(serialization_v8_isolate);
      HandleScope scope(serialization_isolate);
      v8::Local<v8::Context> serialization_context =
          v8::Context::New(serialization_v8_isolate);
      serialization_context->Enter();

      MaybeDirectHandle<WasmModuleObject> maybe_module_object =
          GetWasmEngine()->SyncCompile(
              serialization_isolate,
              WasmEnabledFeatures::FromIsolate(serialization_isolate),
              MakeCompileTimeImports(), &thrower, base::UniqueCopyOf(buffer));
      DirectHandle<WasmModuleObject> module_object =
          maybe_module_object.ToHandleChecked();
      weak_native_module = module_object->native_module().as_shared_ptr();
      // Check that the native module exists at this point.
      CHECK(weak_native_module.lock());

      v8::Local<v8::Object> v8_module_obj =
          v8::Utils::ToLocal(Cast<JSObject>(module_object));
      CHECK(v8_module_obj->IsWasmModuleObject());

      v8::Local<v8::WasmModuleObject> v8_module_object =
          v8_module_obj.As<v8::WasmModuleObject>();
      v8::CompiledWasmModule compiled_module =
          v8_module_object->GetCompiledModule();
      std::span<const uint8_t> uncompiled_bytes =
          compiled_module.GetWireBytesRef();
      uint8_t* bytes_copy =
          zone()->AllocateArray<uint8_t>(uncompiled_bytes.size());
      memcpy(bytes_copy, uncompiled_bytes.data(), uncompiled_bytes.size());
      wire_bytes_ = {bytes_copy, uncompiled_bytes.size()};

      // Run the code until tier-up (of the single function) was observed.
      DirectHandle<WasmInstanceObject> instance =
          GetWasmEngine()
              ->SyncInstantiate(serialization_isolate, &thrower, module_object,
                                {})
              .ToHandleChecked();
      CHECK(data_.empty());
      while (data_.empty()) {
        testing::CallWasmFunctionForTesting(serialization_isolate, instance,
                                            kFunctionName, {});
        ScopedVersionEmbedderString embedder(embedder_string);
        data_ = compiled_module.Serialize();
      }
      CHECK(!data_.empty());
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

    serialized_bytes_ = data_.as_span();

    v8::HandleScope new_scope(WasmExecutionTestBase::current_isolate());
    v8::Local<v8::Context> deserialization_context =
        v8::Context::New(WasmExecutionTestBase::current_isolate());
    deserialization_context->Enter();
  }

  v8::internal::AccountingAllocator allocator_;
  Zone zone_;
  // TODO(14179): Add tests for de/serializing modules with compile-time
  // imports.
  CompileTimeImports compile_imports_;
  v8::OwnedBuffer data_;
  std::span<const uint8_t> wire_bytes_ = {};
  std::span<const uint8_t> serialized_bytes_ = {};
  FlagScope<int> tier_up_quickly_{&v8_flags.wasm_tiering_budget, 1000};
};

TEST_F(WasmSerializationTest, DeserializeValidModule) {
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());
    test.DeserializeAndRun();
  }
  test.CollectGarbage();
}

TEST_F(WasmSerializationTest, DeserializeWithSourceUrl) {
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());
    const std::string url = "http://example.com/example.wasm";
    DirectHandle<WasmModuleObject> module_object;
    CHECK(test.Deserialize(url).ToHandle(&module_object));
    Tagged<String> url_str = Cast<String>(module_object->script()->name());
    CHECK_EQ(url, url_str->ToCString().get());
  }
  test.CollectGarbage();
}

TEST_F(WasmSerializationTest, DeserializeMismatchingVersion) {
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());
    test.InvalidateVersion();
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

TEST_F(WasmSerializationTest, DeserializeEmbedderString) {
  {
    WasmSerializationTester test("");
    {
      HandleScope scope(WasmExecutionTestBase::current_i_isolate());
      ScopedVersionEmbedderString embedder("");
      CHECK(!test.Deserialize().is_null());
      ScopedVersionEmbedderString mismatching_embedder("-test");
      CHECK(test.Deserialize().is_null());
    }
    test.CollectGarbage();
  }

  {
    WasmSerializationTester test("-test");
    {
      HandleScope scope(WasmExecutionTestBase::current_i_isolate());
      ScopedVersionEmbedderString embedder("-test");
      CHECK(!test.Deserialize().is_null());
      ScopedVersionEmbedderString mismatching_embedder("-test.2");
      CHECK(test.Deserialize().is_null());
    }
    test.CollectGarbage();
  }
}

TEST_F(WasmSerializationTest, DeserializeNoSerializedData) {
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());
    test.ClearSerializedData();
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

TEST_F(WasmSerializationTest, DeserializeWireBytesAndSerializedDataInvalid) {
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());
    test.InvalidateVersion();
    test.InvalidateWireBytes();
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

bool False(v8::Local<v8::Context> context, v8::Local<v8::String> source) {
  return false;
}

TEST_F(WasmSerializationTest, BlockWasmCodeGenAtDeserialization) {
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());
    WasmExecutionTestBase::current_isolate()
        ->SetAllowWasmCodeGenerationCallback(False);
    CHECK(test.Deserialize().is_null());
  }
  test.CollectGarbage();
}

TEST_F(WasmSerializationUninitializedTest, CompiledWasmModulesTransfer) {
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  ZoneBuffer buffer(&zone);
  WasmSerializationTester::BuildWireBytes(&zone, &buffer);

  std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = array_buffer_allocator.get();
  v8::Isolate* from_isolate = v8::Isolate::New(create_params);
  std::vector<v8::CompiledWasmModule> store;
  CppGCManaged<NativeModule>::Ptr original_native_module;
  {
    v8::Isolate::Scope isolate_scope(from_isolate);
    v8::HandleScope scope(from_isolate);
    v8::Local<v8::Context> env = v8::Context::New(from_isolate);
    v8::Context::Scope context_scope(env);

    Isolate* from_i_isolate = reinterpret_cast<Isolate*>(from_isolate);
    ErrorThrower thrower(from_i_isolate, "TestCompiledWasmModulesTransfer");
    MaybeDirectHandle<WasmModuleObject> maybe_module_object =
        GetWasmEngine()->SyncCompile(
            from_i_isolate, WasmEnabledFeatures::FromIsolate(from_i_isolate),
            CompileTimeImports{}, &thrower, base::UniqueCopyOf(buffer));
    DirectHandle<WasmModuleObject> module_object =
        maybe_module_object.ToHandleChecked();
    v8::Local<v8::WasmModuleObject> v8_module =
        v8::Local<v8::WasmModuleObject>::Cast(
            v8::Utils::ToLocal(Cast<JSObject>(module_object)));
    store.push_back(v8_module->GetCompiledModule());
    original_native_module = module_object->native_module();
  }

  {
    v8::Isolate* to_isolate = v8::Isolate::New(create_params);
    {
      v8::Isolate::Scope isolate_scope(to_isolate);
      v8::HandleScope scope(to_isolate);
      v8::Local<v8::Context> env = v8::Context::New(to_isolate);
      v8::Context::Scope context_scope(env);

      v8::MaybeLocal<v8::WasmModuleObject> transferred_module =
          v8::WasmModuleObject::FromCompiledModule(to_isolate, store[0]);
      CHECK(!transferred_module.IsEmpty());
      DirectHandle<WasmModuleObject> module_object = Cast<WasmModuleObject>(
          v8::Utils::OpenDirectHandle(*transferred_module.ToLocalChecked()));
      CppGCManaged<NativeModule>::Ptr transferred_native_module =
          module_object->native_module();
      CHECK_EQ(original_native_module, transferred_native_module);
    }
    to_isolate->Dispose();
  }
  original_native_module.Reset();
  from_isolate->Dispose();
}

TEST_F(WasmSerializationTest, TierDownAfterDeserialization) {
  WasmSerializationTester test;

  Isolate* isolate = WasmExecutionTestBase::current_i_isolate();
  HandleScope scope(isolate);
  DirectHandle<WasmModuleObject> module_object;
  CHECK(test.Deserialize().ToHandle(&module_object));

  CppGCManaged<wasm::NativeModule>::Ptr native_module =
      module_object->native_module();
  CHECK_EQ(3, native_module->module()->functions.size());
  WasmCodeRefScope code_ref_scope;
  // The deserialized code must be TurboFan (we wait for tier-up before
  // serializing).
  WasmCode* turbofan_code = native_module->GetCode(2);
  CHECK_NOT_NULL(turbofan_code);
  CHECK_EQ(ExecutionTier::kTurbofan, turbofan_code->tier());

  GetWasmEngine()->EnterDebuggingForIsolate(isolate);

  // Entering debugging should delete all code, so that debug code gets compiled
  // lazily.
  CHECK_NULL(native_module->GetCode(0));
}

TEST_F(WasmSerializationTest, SerializeLiftoffModuleFails) {
  // Make sure that no function is tiered up to TurboFan.
  if (!v8_flags.liftoff) return;
  FlagScope<bool> no_tier_up(&v8_flags.wasm_tier_up, false);
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, "test_zone");

  WasmExecutionTestBase::InitIsolateOnce();
  Isolate* isolate = WasmExecutionTestBase::current_i_isolate();
  HandleScope scope(isolate);
  ZoneBuffer wire_bytes_buffer(&zone);
  WasmSerializationTester::BuildWireBytes(&zone, &wire_bytes_buffer);

  ErrorThrower thrower(isolate, "Test");
  MaybeDirectHandle<WasmModuleObject> maybe_module_object =
      GetWasmEngine()->SyncCompile(isolate,
                                   WasmEnabledFeatures::FromIsolate(isolate),
                                   CompileTimeImports{}, &thrower,
                                   base::UniqueCopyOf(wire_bytes_buffer));
  DirectHandle<WasmModuleObject> module_object =
      maybe_module_object.ToHandleChecked();

  CppGCManaged<wasm::NativeModule>::Ptr native_module =
      module_object->native_module();
  WasmSerializer wasm_serializer(native_module.raw());
  size_t buffer_size = wasm_serializer.GetSerializedNativeModuleSize();
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  // Serialization is expected to fail if there is no TurboFan function to
  // serialize.
  CHECK(!wasm_serializer.SerializeNativeModule({buffer.get(), buffer_size}));
}

TEST_F(WasmSerializationTest, SerializeTieringBudget) {
  WasmSerializationTester test;

  Isolate* isolate = WasmExecutionTestBase::current_i_isolate();
  v8::OwnedBuffer serialized_bytes;
  uint32_t mock_budget[3]{1, 2, 3};
  {
    HandleScope scope(isolate);
    DirectHandle<WasmModuleObject> module_object;
    CHECK(test.Deserialize().ToHandle(&module_object));

    CppGCManaged<wasm::NativeModule>::Ptr native_module =
        module_object->native_module();
    memcpy(native_module->tiering_budget_array(), mock_budget,
           arraysize(mock_budget) * sizeof(uint32_t));
    v8::Local<v8::Object> v8_module_obj =
        v8::Utils::ToLocal(Cast<JSObject>(module_object));
    CHECK(v8_module_obj->IsWasmModuleObject());

    v8::Local<v8::WasmModuleObject> v8_module_object =
        v8_module_obj.As<v8::WasmModuleObject>();
    serialized_bytes = v8_module_object->GetCompiledModule().Serialize();

    // Change one entry in the tiering budget after serialization to make sure
    // the module gets deserialized and not just loaded from the module cache.
    native_module->tiering_budget_array()[0]++;
  }
  test.CollectGarbage();
  HandleScope scope(isolate);
  DirectHandle<WasmModuleObject> module_object;
  CompileTimeImports compile_imports = test.MakeCompileTimeImports();
  base::UniqueArray<const uint8_t> wire_bytes_copy =
      base::UniqueCopyOf(test.wire_bytes());
  CHECK(DeserializeNativeModule(
            isolate, WasmEnabledFeatures::FromIsolate(isolate),
            serialized_bytes.as_span(), wire_bytes_copy, compile_imports, {})
            .ToHandle(&module_object));

  CppGCManaged<wasm::NativeModule>::Ptr native_module =
      module_object->native_module();
  for (size_t i = 0; i < arraysize(mock_budget); ++i) {
    CHECK_EQ(mock_budget[i], native_module->tiering_budget_array()[i]);
  }
}

TEST_F(WasmSerializationTest, SerializationFailsOnChangedFlags) {
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());

    FlagScope<bool> no_bounds_checks(&v8_flags.wasm_bounds_checks, false);
    CHECK(test.Deserialize().is_null());

    FlagScope<bool> bounds_checks(&v8_flags.wasm_bounds_checks, true);
    CHECK(!test.Deserialize().is_null());
  }
}

namespace {

// {g_is_trap_handler_enabled} is not a {FlagValue}, so {FlagScope} does not
// apply here.
class V8_NODISCARD TrapHandlerScope {
 public:
  explicit TrapHandlerScope(bool enabled)
      : previous_value_(trap_handler::g_is_trap_handler_enabled) {
    trap_handler::g_is_trap_handler_enabled = enabled;
  }
  ~TrapHandlerScope() {
    trap_handler::g_is_trap_handler_enabled = previous_value_;
  }

 private:
  const bool previous_value_;
};

}  // namespace

TEST_F(WasmSerializationTest, SerializationFailsOnChangedTrapHandler) {
  // The trap handler can only be enabled once per process, and embedders
  // decide whether to enable it independently of the flags. Emulate
  // deserializing in a process that decided differently.
  if (!V8_TRAP_HANDLER_SUPPORTED) return;
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());

    {
      TrapHandlerScope flipped(!trap_handler::g_is_trap_handler_enabled);
      CHECK(test.Deserialize().is_null());
    }
    CHECK(!test.Deserialize().is_null());
  }
}

TEST_F(WasmSerializationTest, SerializationFailsOnChangedFeatures) {
  WasmSerializationTester test;
  {
    HandleScope scope(WasmExecutionTestBase::current_i_isolate());

    WasmExecutionTestBase::current_isolate()
        ->SetWasmCustomDescriptorsEnabledCallback([](auto) { return true; });
    CHECK(test.Deserialize().is_null());

    WasmExecutionTestBase::current_isolate()
        ->SetWasmCustomDescriptorsEnabledCallback([](auto) { return false; });
    CHECK(!test.Deserialize().is_null());
  }
}

TEST_F(WasmSerializationTest, DeserializeIndirectCallWithDifferentCanonicalId) {
  // This test compiles and serializes a module with an indirect call, then
  // resets the type canonicalizer, compiles another module, and then
  // deserializes the original module. This ensures that a different canonical
  // signature ID is used for the indirect call.
  // We then call the deserialized module to check that the right canonical
  // signature ID is being used.

  // Compile with Turbofan right away.
  FlagScope<bool> no_liftoff{&v8_flags.liftoff, false};
  FlagScope<bool> no_lazy_compilation{&v8_flags.wasm_lazy_compilation, false};
  FlagScope<bool> expose_gc{&v8_flags.expose_gc, true};

  i::Isolate* i_isolate = WasmExecutionTestBase::InitIsolateOnce();
  v8::Isolate* v8_isolate = WasmExecutionTestBase::current_isolate();
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  HandleScope handle_scope(i_isolate);

  // Build a small module with an indirect call.
  ZoneBuffer zone_buffer(&zone);
  {
    WasmModuleBuilder builder{&zone};
    TestSignatures sigs;

    // Add the "call_indirect" function which calls table0[0].
    ModuleTypeIndex sig_id = builder.AddSignature(sigs.i_i(), true);
    WasmFunctionBuilder* f = builder.AddFunction(sig_id);
    f->EmitCode({// (i) => i != 0 ? f(i-1) : 42
                 WASM_IF_ELSE_I(
                     // cond:
                     WASM_LOCAL_GET(0),
                     // if_true:
                     WASM_CALL_INDIRECT(
                         SIG_INDEX(sig_id.index),
                         WASM_I32_SUB(WASM_LOCAL_GET(0), WASM_ONE), WASM_ZERO),
                     // if_false:
                     WASM_I32V_1(42)),
                 WASM_END});
    builder.AddExport("call_indirect", f);
    // Add a function table.
    uint32_t table_id = builder.AddTable(kWasmFuncRef, 1);
    builder.SetIndirectFunction(table_id, 0, f->func_index());
    // Write the final module into {buffer}.
    builder.WriteTo(&zone_buffer);
  }

  // Compile the module and serialize it.
  // Keep a weak pointer so we can check that the original native module died.
  std::weak_ptr<NativeModule> weak_native_module;
  v8::OwnedBuffer serialized_module;
  CanonicalTypeIndex canonical_sig_id_before_serialization;
  {
    ErrorThrower thrower(i_isolate, "");

    {
      v8::Isolate::Scope isolate_scope(v8_isolate);
      HandleScope scope(i_isolate);
      v8::Local<v8::Context> serialization_context =
          v8::Context::New(v8_isolate);
      serialization_context->Enter();

      DirectHandle<WasmModuleObject> module_object =
          GetWasmEngine()
              ->SyncCompile(i_isolate,
                            WasmEnabledFeatures::FromIsolate(i_isolate),
                            CompileTimeImports{}, &thrower,
                            base::UniqueCopyOf(zone_buffer))
              .ToHandleChecked();
      CppGCManaged<wasm::NativeModule>::Ptr native_module =
          module_object->native_module();
      weak_native_module = native_module.as_shared_ptr();

      // Retrieve the canonicalized signature ID.
      const std::vector<CanonicalTypeIndex>& canonical_type_ids =
          native_module->module()->isorecursive_canonical_type_ids;
      CHECK_EQ(1, canonical_type_ids.size());
      canonical_sig_id_before_serialization = canonical_type_ids[0];

      // Check that the embedded constant in the code is right.
      WasmCodeRefScope code_ref_scope;
      WasmCode* code = native_module->GetCode(0);
      RelocIterator reloc_it{
          code->instructions(), code->reloc_info(), code->constant_pool(),
          RelocInfo::ModeMask(RelocInfo::WASM_CANONICAL_SIG_ID)};
      CHECK(!reloc_it.done());
      CHECK_EQ(canonical_sig_id_before_serialization.index,
               reloc_it.rinfo()->wasm_canonical_sig_id());
      reloc_it.next();
      CHECK(reloc_it.done());

      // Convert to API objects and serialize.
      v8::Local<v8::WasmModuleObject> v8_module_object =
          v8::Utils::ToLocal(module_object);
      serialized_module = v8_module_object->GetCompiledModule().Serialize();
    }

    CHECK(!serialized_module.empty());

    // Run GC until the NativeModule died. Add a manual timeout of 60 seconds to
    // get a better error message than just a test timeout if this fails.
    const auto start_time = std::chrono::steady_clock::now();
    const auto end_time = start_time + std::chrono::seconds(60);
    while (weak_native_module.lock()) {
      // We need to invoke GC without stack, otherwise the native module may
      // survive.
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(
          i_isolate->heap());
      v8_isolate->RequestGarbageCollectionForTesting(
          v8::Isolate::kFullGarbageCollection);
      if (std::chrono::steady_clock::now() > end_time) {
        FATAL("NativeModule did not die within 60 seconds");
      }
    }
  }

  // Clear canonicalized types, then compile another module which adds a
  // canonical type at the same index we used in the previous module.
  GetTypeCanonicalizer()->EmptyStorageForTesting();
  {
    ZoneBuffer buffer(&zone);
    WasmModuleBuilder builder{&zone};
    TestSignatures sigs;

    ModuleTypeIndex sig_id = builder.AddSignature(sigs.v_v(), true);
    WasmFunctionBuilder* f = builder.AddFunction(sig_id);
    f->EmitByte(kExprEnd);
    builder.WriteTo(&buffer);
    ErrorThrower thrower(i_isolate, "");
    GetWasmEngine()
        ->SyncCompile(i_isolate, WasmEnabledFeatures::FromIsolate(i_isolate),
                      CompileTimeImports{}, &thrower,
                      base::UniqueCopyOf(buffer))
        .ToHandleChecked();
  }

  // Now deserialize the previous module.
  CanonicalTypeIndex canonical_sig_id_after_deserialization{
      canonical_sig_id_before_serialization.index + 1};
  {
    v8::Local<v8::Context> deserialization_context =
        v8::Context::New(WasmExecutionTestBase::current_isolate());
    deserialization_context->Enter();
    ErrorThrower thrower(i_isolate, "");
    std::string_view kNoSourceUrl;
    base::UniqueArray<const uint8_t> wire_bytes_copy =
        base::UniqueCopyOf(zone_buffer);
    DirectHandle<WasmModuleObject> module_object =
        DeserializeNativeModule(i_isolate,
                                WasmEnabledFeatures::FromIsolate(i_isolate),
                                serialized_module.as_span(), wire_bytes_copy,
                                CompileTimeImports{}, kNoSourceUrl)
            .ToHandleChecked();

    // Check that the signature ID got canonicalized to index 1.
    CppGCManaged<wasm::NativeModule>::Ptr native_module =
        module_object->native_module();
    const std::vector<CanonicalTypeIndex>& canonical_type_ids =
        native_module->module()->isorecursive_canonical_type_ids;
    CHECK_EQ(1, canonical_type_ids.size());
    CHECK_EQ(canonical_sig_id_after_deserialization, canonical_type_ids[0]);

    // Check that the embedded constant in the code is right.
    WasmCodeRefScope code_ref_scope;
    WasmCode* code = native_module->GetCode(0);
    RelocIterator reloc_it{
        code->instructions(), code->reloc_info(), code->constant_pool(),
        RelocInfo::ModeMask(RelocInfo::WASM_CANONICAL_SIG_ID)};
    CHECK(!reloc_it.done());
    CHECK_EQ(canonical_sig_id_after_deserialization.index,
             reloc_it.rinfo()->wasm_canonical_sig_id());
    reloc_it.next();
    CHECK(reloc_it.done());

    // Now call the function.
    DirectHandle<WasmInstanceObject> instance =
        GetWasmEngine()
            ->SyncInstantiate(i_isolate, &thrower, module_object,
                              DirectHandle<JSReceiver>::null())
            .ToHandleChecked();
    DirectHandle<Object> params[] = {direct_handle(Smi::FromInt(1), i_isolate)};
    int32_t result = testing::CallWasmFunctionForTesting(
        i_isolate, instance, "call_indirect", base::ArrayVector(params));
    CHECK_EQ(42, result);
  }
}

// Regression test for https://crbug.com/372840600 /
// https://crbug.com/369793713 / https://crbug.com/369869947.
TEST_F(WasmSerializationTest, SerializeDetectedFeatures) {
  // This test compiles and serializes a module which uses a use-counter-tracked
  // feature (tail calls). We check that the set of detected features is
  // preserved across serialization and deserialization. Otherwise we would
  // fail a DCHECK in lazy compilation later.

  FlagScope<int> tier_up_quickly{&v8_flags.wasm_tiering_budget, 10};
  FlagScope<bool> expose_gc{&v8_flags.expose_gc, true};

  i::Isolate* i_isolate = WasmExecutionTestBase::InitIsolateOnce();
  v8::Isolate* v8_isolate = WasmExecutionTestBase::current_isolate();
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  HandleScope handle_scope(i_isolate);

  // Build a small module with a tail call.
  ZoneBuffer buffer(&zone);
  {
    WasmModuleBuilder builder{&zone};

    // Add a function which is tail-called by another one.
    ModuleTypeIndex sig_i_v = builder.AddSignature(TestSignatures::i_v(), true);
    WasmFunctionBuilder* a = builder.AddFunction(sig_i_v);
    a->EmitCode({WASM_I32V_1(11), WASM_END});
    builder.AddExport("a", a);
    // Add the function which tail-calls the first one.
    WasmFunctionBuilder* b = builder.AddFunction(sig_i_v);
    b->EmitCode({WASM_RETURN_CALL_FUNCTION0(a->func_index()), WASM_END});
    builder.AddExport("b", b);
    // Write the final module into {buffer}.
    builder.WriteTo(&buffer);
  }

  // Compile and initialize the module and serialize it.
  // Keep a weak pointer so we can check that the original native module died.
  std::weak_ptr<NativeModule> weak_native_module;
  v8::OwnedBuffer serialized_module;
  {
    ErrorThrower thrower(i_isolate, "");

    {
      v8::Isolate::Scope isolate_scope(v8_isolate);
      HandleScope scope(i_isolate);
      v8::Local<v8::Context> serialization_context =
          v8::Context::New(v8_isolate);
      serialization_context->Enter();

      DirectHandle<WasmModuleObject> module_object =
          GetWasmEngine()
              ->SyncCompile(
                  i_isolate, WasmEnabledFeatures::FromIsolate(i_isolate),
                  CompileTimeImports{}, &thrower, base::UniqueCopyOf(buffer))
              .ToHandleChecked();
      // Check that "return_call" is in the set of detected features.
      CHECK_EQ(WasmDetectedFeatures{{WasmDetectedFeature::return_call}},
               module_object->native_module()
                   ->compilation_state()
                   ->detected_features());
      weak_native_module = module_object->native_module().as_shared_ptr();

      // Now call the tail-calling function "b". This triggers lazy compilation,
      // which should not DCHECK because of a new detected feature.
      DirectHandle<WasmInstanceObject> instance =
          GetWasmEngine()
              ->SyncInstantiate(i_isolate, &thrower, module_object,
                                DirectHandle<JSReceiver>::null())
              .ToHandleChecked();

      v8::Local<v8::WasmModuleObject> v8_module_object =
          v8::Utils::ToLocal(module_object);
      // Call function "a" until serialization succeeds (once we have TF code).
      const auto start_time = std::chrono::steady_clock::now();
      const auto end_time = start_time + std::chrono::seconds(60);
      while (true) {
        int32_t result =
            testing::CallWasmFunctionForTesting(i_isolate, instance, "a", {});
        CHECK_EQ(11, result);
        serialized_module = v8_module_object->GetCompiledModule().Serialize();
        if (!serialized_module.empty()) break;
        v8_isolate->RequestGarbageCollectionForTesting(
            v8::Isolate::kFullGarbageCollection);
        if (std::chrono::steady_clock::now() > end_time) {
          FATAL("Tier-up didn't complete within 60 seconds");
        }
      }
    }

    CHECK(!serialized_module.empty());

    // Run GC until the NativeModule died. Add a manual timeout of 60 seconds to
    // get a better error message than just a test timeout if this fails.
    const auto start_time = std::chrono::steady_clock::now();
    const auto end_time = start_time + std::chrono::seconds(60);
    while (weak_native_module.lock()) {
      // We need to invoke GC without stack, otherwise the native module may
      // survive.
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(
          i_isolate->heap());
      v8_isolate->RequestGarbageCollectionForTesting(
          v8::Isolate::kFullGarbageCollection);
      if (std::chrono::steady_clock::now() > end_time) {
        FATAL("NativeModule did not die within 60 seconds");
      }
    }
  }

  // Now deserialize the module and check the detected features again.
  {
    v8::Local<v8::Context> deserialization_context =
        v8::Context::New(WasmExecutionTestBase::current_isolate());
    deserialization_context->Enter();
    ErrorThrower thrower(i_isolate, "");
    std::string_view kNoSourceUrl;
    base::UniqueArray<const uint8_t> wire_bytes_copy =
        base::UniqueCopyOf(buffer);
    DirectHandle<WasmModuleObject> module_object =
        DeserializeNativeModule(i_isolate,
                                WasmEnabledFeatures::FromIsolate(i_isolate),
                                serialized_module.as_span(), wire_bytes_copy,
                                CompileTimeImports{}, kNoSourceUrl)
            .ToHandleChecked();

    CHECK_EQ(WasmDetectedFeatures{{WasmDetectedFeature::return_call}},
             module_object->native_module()
                 ->compilation_state()
                 ->detected_features());

    // Now call the tail-calling function "b". This triggers lazy compilation,
    // which should not DCHECK because of a new detected feature.
    DirectHandle<WasmInstanceObject> instance =
        GetWasmEngine()
            ->SyncInstantiate(i_isolate, &thrower, module_object,
                              DirectHandle<JSReceiver>::null())
            .ToHandleChecked();
    int32_t result =
        testing::CallWasmFunctionForTesting(i_isolate, instance, "b", {});
    CHECK_EQ(11, result);
  }
}

}  // namespace v8::internal::wasm
