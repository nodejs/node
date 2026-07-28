// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/compilation-hints-generation.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-arguments.h"
#include "src/wasm/wasm-module-builder.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"
#include "test/unittests/test-utils.h"

namespace v8::internal {
namespace wasm {

class WasmCompilationHintsBuilder {
 public:
  WasmCompilationHintsBuilder(Isolate* isolate, Zone* zone)
      : isolate_(isolate),
        zone_(zone),
        builder_(zone),
        thrower_(isolate, "test wasm compilation hints") {}

  HeapType DefineSignature(FunctionSig* sig,
                           ModuleTypeIndex supertype = kNoSuperType,
                           bool is_final = false) {
    return HeapType::Index(builder_.ForceAddSignature(sig, is_final, supertype),
                           kNotShared, RefTypeKind::kFunction);
  }

  uint8_t DefineFunction(FunctionSig* sig,
                         std::initializer_list<ValueType> locals,
                         std::initializer_list<const uint8_t> code) {
    return DefineFunctionImpl(builder_.AddFunction(sig), locals, code);
  }

  uint8_t DefineFunction(ModuleTypeIndex sig_index,
                         std::initializer_list<ValueType> locals,
                         std::initializer_list<const uint8_t> code) {
    return DefineFunctionImpl(builder_.AddFunction(sig_index), locals, code);
  }

  void AddExport(const char* name, uint32_t index) {
    builder_.AddExport(base::CStrVector(name), kExternalFunction, index);
  }

  void CompileModule() {
    ZoneBuffer buffer(zone_);
    builder_.WriteTo(&buffer);
    MaybeDirectHandle<WasmInstanceObject> maybe_instance =
        testing::CompileAndInstantiateForTesting(isolate_, &thrower_,
                                                 base::VectorOf(buffer));
    EXPECT_FALSE(thrower_.error());
    instance_object_ = maybe_instance.ToHandleChecked();
    trusted_instance_data_ =
        direct_handle(instance_object_->trusted_data(isolate_), isolate_);
  }

  void CheckResult(uint32_t function_index, int32_t expected, int32_t arg0,
                   int32_t arg1) {
    const CanonicalSig* sig = LookupCanonicalSigFor(function_index);
    DCHECK(EquivalentNumericSig(sig, sigs.i_ii()));
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    packer.Push(arg0);
    packer.Push(arg1);
    CheckResultImpl(function_index, sig, &packer, expected);
  }

  DirectHandle<WasmInstanceObject> instance_object() {
    return instance_object_;
  }
  DirectHandle<WasmTrustedInstanceData> trusted_instance_data() {
    return trusted_instance_data_;
  }

  TestSignatures sigs;

 private:
  uint8_t DefineFunctionImpl(WasmFunctionBuilder* fun,
                             std::initializer_list<ValueType> locals,
                             std::initializer_list<const uint8_t> code) {
    for (ValueType local : locals) {
      fun->AddLocal(local);
    }
    fun->EmitCode(code);
    return fun->func_index();
  }

  void CallFunctionImpl(uint32_t function_index, const CanonicalSig* sig,
                        CWasmArgumentsPacker* packer) {
    // The signature must be canonicalized.
    DCHECK(GetTypeCanonicalizer()->Contains(sig));
    WasmCodeRefScope code_ref_scope;
    WasmCodePointer wasm_call_target =
        trusted_instance_data_->GetCallTarget(function_index);
    DirectHandle<Object> object_ref = instance_object_;
    DirectHandle<Code> c_wasm_entry =
        compiler::CompileCWasmEntry(isolate_, sig);
    Execution::CallWasm(isolate_, c_wasm_entry, wasm_call_target, object_ref,
                        packer->argv());
  }

  void CheckResultImpl(uint32_t function_index, const CanonicalSig* sig,
                       CWasmArgumentsPacker* packer, int32_t expected) {
    CallFunctionImpl(function_index, sig, packer);
    EXPECT_FALSE(isolate_->has_exception());
    packer->Reset();
    EXPECT_EQ(expected, packer->Pop<int32_t>());
  }

  const CanonicalSig* LookupCanonicalSigFor(uint32_t function_index) const {
    auto* module = instance_object_->module();
    CanonicalTypeIndex sig_id =
        module->canonical_sig_id(module->functions[function_index].sig_index);
    return GetTypeCanonicalizer()->LookupFunctionSignature(sig_id);
  }

  Isolate* isolate_;
  Zone* zone_;
  WasmModuleBuilder builder_;
  ErrorThrower thrower_;
  DirectHandle<WasmInstanceObject> instance_object_;
  DirectHandle<WasmTrustedInstanceData> trusted_instance_data_;
};

class WasmCompilationHintsUnittest : public TestWithIsolateAndZone {};

TEST_F(WasmCompilationHintsUnittest, RecoverCompilationHints) {
  const FlagScope<bool> compilation_hints_scope(
      &v8_flags.experimental_wasm_compilation_hints, true);
  const FlagScope<bool> wasm_generate_compilation_hints_scope(
      &v8_flags.wasm_generate_compilation_hints, true);
  const FlagScope<int> wasm_tiering_budget_scope(&v8_flags.wasm_tiering_budget,
                                                 5);
  v8::Context::New(reinterpret_cast<v8::Isolate*>(isolate()))->Enter();
  WasmCompilationHintsBuilder builder(isolate(), zone());
  HeapType module_sig_i_i = builder.DefineSignature(builder.sigs.i_i());
  uint8_t inc = builder.DefineFunction(
      module_sig_i_i.ref_index(), {},
      {WASM_I32_ADD(WASM_LOCAL_GET(0), WASM_I32V_1(1)), WASM_END});
  uint8_t dec = builder.DefineFunction(
      module_sig_i_i.ref_index(), {},
      {WASM_I32_SUB(WASM_LOCAL_GET(0), WASM_I32V_1(1)), WASM_END});

  uint8_t caller = builder.DefineFunction(
      builder.sigs.i_ii(), {ValueType::RefNull(module_sig_i_i)},
      // clang-format off
      {kExprLocalGet, 0,
       kExprIf, kVoidCode,
         kExprRefFunc, inc, kExprLocalSet, 2,
       kExprElse,
         kExprRefFunc, dec, kExprLocalSet, 2,
       kExprEnd,
       kExprLocalGet, 1, kExprLocalGet, 2, kExprCallRef,
         static_cast<uint8_t>(module_sig_i_i.ref_index().index),
       kExprLocalGet, 0,
       kExprIf, static_cast<uint8_t>(module_sig_i_i.ref_index().index),
         kExprLocalGet, 1, kExprCallFunction, inc,
         kExprI32Add,
       kExprEnd,
       kExprEnd});
  // clang-format on

  builder.AddExport("inc", inc);
  builder.AddExport("dec", dec);
  builder.AddExport("caller", caller);

  builder.CompileModule();

  builder.CheckResult(caller, 9, 0, 10);
  builder.CheckResult(caller, 9, 0, 10);
  builder.CheckResult(caller, 9, 0, 10);
  builder.CheckResult(caller, 9, 0, 10);
  builder.CheckResult(caller, 22, 1, 10);
  builder.CheckResult(caller, 22, 1, 10);

  // Emit compilation hints to a buffer.
  ZoneBuffer buffer(zone());
  {
    DisallowGarbageCollection no_gc;
    wasm::TransitiveTypeFeedbackProcessor::ProcessAll(
        isolate(), *builder.trusted_instance_data());
  }
  buffer.write_u32(kWasmMagic);
  buffer.write_u32(kWasmVersion);
  EmitCompilationHintsToBuffer(
      buffer, builder.trusted_instance_data()->native_module());

  // Now compile the new module emitted in {buffer}.
  ErrorThrower thrower(isolate(), "test wasm compilation hints");

  MaybeDirectHandle<WasmInstanceObject> maybe_instance =
      testing::CompileAndInstantiateForTesting(isolate(), &thrower,
                                               base::VectorOf(buffer));
  EXPECT_FALSE(thrower.error());

  const WasmModule* module = maybe_instance.ToHandleChecked()->module();

  // Check compilation priorities.
  EXPECT_EQ(size_t{3}, module->compilation_priorities.size());
  for (uint32_t i = 0; i < module->compilation_priorities.size(); i++) {
    EXPECT_EQ(
        uint32_t{0},
        module->compilation_priorities.find(i)->second.compilation_priority);
  }
  EXPECT_EQ(
      kOptimizationPriorityNotSpecifiedSentinel,
      module->compilation_priorities.find(inc)->second.optimization_priority);
  EXPECT_EQ(
      kOptimizationPriorityNotSpecifiedSentinel,
      module->compilation_priorities.find(dec)->second.optimization_priority);
  EXPECT_EQ(0, module->compilation_priorities.find(caller)
                   ->second.optimization_priority);

  // Check instruction frequencies.
  EXPECT_EQ(size_t{1}, module->instruction_frequencies.size());
  const std::vector<std::pair<uint32_t, uint8_t>>& caller_frequencies =
      module->instruction_frequencies.find(caller)->second;
  EXPECT_EQ(size_t{2}, caller_frequencies.size());
  EXPECT_EQ(uint32_t{22}, caller_frequencies[0].first);
  EXPECT_EQ(uint8_t{32}, caller_frequencies[0].second);
  EXPECT_EQ(uint32_t{30}, caller_frequencies[1].first);
  EXPECT_EQ(uint8_t{30}, caller_frequencies[1].second);

  // Check call targets.
  EXPECT_EQ(size_t{1}, module->call_targets.size());
  const std::vector<std::pair<uint32_t, CallTargetVector>>& caller_targets =
      module->call_targets.find(caller)->second;
  // No call targets for direct calls.
  EXPECT_EQ(size_t{1}, caller_targets.size());
  EXPECT_EQ(uint32_t{22}, caller_targets[0].first);
  const CallTargetVector& call_targets_at_offset = caller_targets[0].second;
  EXPECT_EQ(dec, call_targets_at_offset[0].function_index);
  EXPECT_EQ(uint32_t{66}, call_targets_at_offset[0].call_frequency_percent);
  EXPECT_EQ(inc, call_targets_at_offset[1].function_index);
  EXPECT_EQ(uint32_t{33}, call_targets_at_offset[1].call_frequency_percent);
}

}  // namespace wasm
}  // namespace v8::internal
