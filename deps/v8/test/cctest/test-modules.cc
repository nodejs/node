// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags.h"

#include "test/cctest/cctest.h"

namespace {

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Module;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Value;

MaybeLocal<Module> AlwaysEmptyResolveCallback(Local<Context> context,
                                              Local<String> specifier,
                                              Local<Module> referrer,
                                              Local<Value> data) {
  return MaybeLocal<Module>();
}

static int g_count = 0;
MaybeLocal<Module> FailOnSecondCallResolveCallback(Local<Context> context,
                                                   Local<String> specifier,
                                                   Local<Module> referrer,
                                                   Local<Value> data) {
  if (g_count++ > 0) return MaybeLocal<Module>();
  Local<String> source_text = v8_str("");
  ScriptOrigin origin(v8_str("module.js"));
  ScriptCompiler::Source source(source_text, origin);
  return ScriptCompiler::CompileModule(CcTest::isolate(), &source)
      .ToLocalChecked();
}

TEST(ModuleInstantiationFailures) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;

  Local<String> source_text = v8_str(
      "import './foo.js';"
      "export {} from './bar.js';");
  ScriptOrigin origin(v8_str("file.js"));
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK_EQ(2, module->GetModuleRequestsLength());
  CHECK(v8_str("./foo.js")->StrictEquals(module->GetModuleRequest(0)));
  CHECK(v8_str("./bar.js")->StrictEquals(module->GetModuleRequest(1)));

  // Instantiation should fail.
  CHECK(!module->Instantiate(env.local(), AlwaysEmptyResolveCallback));

  // Start over again...
  module = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();

  // Instantiation should fail if a sub-module fails to resolve.
  g_count = 0;
  CHECK(!module->Instantiate(env.local(), FailOnSecondCallResolveCallback));
}

static MaybeLocal<Module> CompileSpecifierAsModuleResolveCallback(
    Local<Context> context, Local<String> specifier, Local<Module> referrer,
    Local<Value> data) {
  ScriptOrigin origin(v8_str("module.js"));
  ScriptCompiler::Source source(specifier, origin);
  return ScriptCompiler::CompileModule(CcTest::isolate(), &source)
      .ToLocalChecked();
}

TEST(ModuleEvaluation) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;

  Local<String> source_text = v8_str(
      "import 'Object.expando = 5';"
      "import 'Object.expando *= 2';");
  ScriptOrigin origin(v8_str("file.js"));
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK(module->Instantiate(env.local(),
                            CompileSpecifierAsModuleResolveCallback));
  CHECK(!module->Evaluate(env.local()).IsEmpty());
  ExpectInt32("Object.expando", 10);
}

TEST(EmbedderData) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;

  Local<String> source_text = v8_str("");
  ScriptOrigin origin(v8_str("file.js"));
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK(module->GetEmbedderData()->IsUndefined());
  module->SetEmbedderData(v8_num(42));
  CHECK_EQ(42, Local<v8::Int32>::Cast(module->GetEmbedderData())->Value());
}

}  // anonymous namespace
