// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/objects/module-inl.h"
#include "test/unittests/compiler/js-heap-broker-base.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace compiler {

using ModuleRefTest = TestWithNativeContextAndZone;

namespace {

Local<v8::Module> CompileModule(v8::Isolate* isolate, const char* name,
                                const char* source) {
  ScriptOrigin origin(v8::String::NewFromUtf8(isolate, name).ToLocalChecked(),
                      0, 0, false, -1, Local<Value>(), false, false, true);
  ScriptCompiler::Source script_source(
      v8::String::NewFromUtf8(isolate, source).ToLocalChecked(), origin);
  return ScriptCompiler::CompileModule(isolate, &script_source)
      .ToLocalChecked();
}

Global<v8::Module>* dep_module = nullptr;

MaybeLocal<v8::Module> ResolveToDep(Local<v8::Context> context,
                                    Local<v8::String> specifier,
                                    Local<v8::FixedArray> import_attributes,
                                    Local<v8::Module> referrer) {
  return dep_module->Get(v8::Isolate::GetCurrent());
}

}  // namespace

// The exports table of a module may still change until its first namespace is
// created (that is when the star exports are fetched), so the compiler must not
// look up exports before then, whatever path it took to reach the module.
TEST_F(ModuleRefTest, LookupExportCell_RequiresNamespace) {
  Isolate* isolate = i_isolate();
  HandleScope handle_scope(isolate);

  Global<v8::Module> dep(v8_isolate(), CompileModule(v8_isolate(), "dep.mjs",
                                                     "export let z = 2;"));
  dep_module = &dep;
  Local<v8::Module> api_module = CompileModule(
      v8_isolate(), "main.mjs", "export let a = 1; export * from 'dep.mjs';");
  CHECK(api_module->InstantiateModule(context(), ResolveToDep).FromJust());
  api_module->Evaluate(context()).ToLocalChecked();
  dep_module = nullptr;

  Handle<Module> module = Cast<Module>(Utils::OpenHandle(*api_module));
  CHECK_EQ(module->status(), Module::kEvaluated);

  JSHeapBrokerTestBase broker_base(isolate, zone());
  JSHeapBroker* broker = broker_base.broker();
  ModuleRef module_ref = MakeRef(broker, *module);
  NameRef a =
      MakeRef<Name>(broker, isolate->factory()->InternalizeUtf8String("a"));
  NameRef z =
      MakeRef<Name>(broker, isolate->factory()->InternalizeUtf8String("z"));

  // The local export already has its cell, but there is no namespace yet.
  CHECK(IsCell(module->exports()->Lookup(a.object())));
  CHECK(!module_ref.LookupExportCell(broker, a).has_value());
  CHECK(!module_ref.LookupExportCell(broker, z).has_value());

  Module::GetModuleNamespace(isolate, module);

  OptionalCellRef a_cell = module_ref.LookupExportCell(broker, a);
  OptionalCellRef z_cell = module_ref.LookupExportCell(broker, z);
  CHECK(a_cell.has_value());
  CHECK(z_cell.has_value());
  CHECK_EQ(a_cell->object()->value(), Smi::FromInt(1));
  CHECK_EQ(z_cell->object()->value(), Smi::FromInt(2));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
