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

ScriptOrigin ModuleOrigin(Local<v8::Value> resource_name, Isolate* isolate) {
  ScriptOrigin origin(resource_name, Local<v8::Integer>(), Local<v8::Integer>(),
                      Local<v8::Boolean>(), Local<v8::Integer>(),
                      Local<v8::Value>(), Local<v8::Boolean>(),
                      Local<v8::Boolean>(), True(isolate));
  return origin;
}

MaybeLocal<Module> FailAlwaysResolveCallback(Local<Context> context,
                                             Local<String> specifier,
                                             Local<Module> referrer) {
  Isolate* isolate = context->GetIsolate();
  isolate->ThrowException(v8_str("boom"));
  return MaybeLocal<Module>();
}

static int g_count = 0;
MaybeLocal<Module> FailOnSecondCallResolveCallback(Local<Context> context,
                                                   Local<String> specifier,
                                                   Local<Module> referrer) {
  Isolate* isolate = CcTest::isolate();
  if (g_count++ > 0) {
    isolate->ThrowException(v8_str("booom"));
    return MaybeLocal<Module>();
  }
  Local<String> source_text = v8_str("");
  ScriptOrigin origin = ModuleOrigin(v8_str("module.js"), isolate);
  ScriptCompiler::Source source(source_text, origin);
  return ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
}

TEST(ModuleInstantiationFailures) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  Local<String> source_text = v8_str(
      "import './foo.js';"
      "export {} from './bar.js';");
  ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK_EQ(2, module->GetModuleRequestsLength());
  CHECK(v8_str("./foo.js")->StrictEquals(module->GetModuleRequest(0)));
  CHECK(v8_str("./bar.js")->StrictEquals(module->GetModuleRequest(1)));

  // Instantiation should fail.
  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(!module->Instantiate(env.local(), FailAlwaysResolveCallback));
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
  }

  // Start over again...
  module = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();

  // Instantiation should fail if a sub-module fails to resolve.
  g_count = 0;
  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(!module->Instantiate(env.local(), FailOnSecondCallResolveCallback));
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("booom")));
  }

  CHECK(!try_catch.HasCaught());
}

static MaybeLocal<Module> CompileSpecifierAsModuleResolveCallback(
    Local<Context> context, Local<String> specifier, Local<Module> referrer) {
  ScriptOrigin origin = ModuleOrigin(v8_str("module.js"), CcTest::isolate());
  ScriptCompiler::Source source(specifier, origin);
  return ScriptCompiler::CompileModule(CcTest::isolate(), &source)
      .ToLocalChecked();
}

TEST(ModuleEvaluation) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  Local<String> source_text = v8_str(
      "import 'Object.expando = 5';"
      "import 'Object.expando *= 2';");
  ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK(module->Instantiate(env.local(),
                            CompileSpecifierAsModuleResolveCallback));
  CHECK(!module->Evaluate(env.local()).IsEmpty());
  ExpectInt32("Object.expando", 10);

  CHECK(!try_catch.HasCaught());
}

TEST(ModuleEvaluationCompletion1) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  const char* sources[] = {
      "",
      "var a = 1",
      "import '42'",
      "export * from '42'",
      "export {} from '42'",
      "export {}",
      "var a = 1; export {a}",
      "export function foo() {}",
      "export class C extends null {}",
      "export let a = 1",
      "export default 1",
      "export default function foo() {}",
      "export default function () {}",
      "export default (function () {})",
      "export default class C extends null {}",
      "export default (class C extends null {})",
      "for (var i = 0; i < 5; ++i) {}",
  };

  for (auto src : sources) {
    Local<String> source_text = v8_str(src);
    ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    Local<Module> module =
        ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    CHECK(module->Instantiate(env.local(),
                              CompileSpecifierAsModuleResolveCallback));
    CHECK(module->Evaluate(env.local()).ToLocalChecked()->IsUndefined());
  }

  CHECK(!try_catch.HasCaught());
}

TEST(ModuleEvaluationCompletion2) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  const char* sources[] = {
      "'gaga'; ",
      "'gaga'; var a = 1",
      "'gaga'; import '42'",
      "'gaga'; export * from '42'",
      "'gaga'; export {} from '42'",
      "'gaga'; export {}",
      "'gaga'; var a = 1; export {a}",
      "'gaga'; export function foo() {}",
      "'gaga'; export class C extends null {}",
      "'gaga'; export let a = 1",
      "'gaga'; export default 1",
      "'gaga'; export default function foo() {}",
      "'gaga'; export default function () {}",
      "'gaga'; export default (function () {})",
      "'gaga'; export default class C extends null {}",
      "'gaga'; export default (class C extends null {})",
  };

  for (auto src : sources) {
    Local<String> source_text = v8_str(src);
    ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    Local<Module> module =
        ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    CHECK(module->Instantiate(env.local(),
                              CompileSpecifierAsModuleResolveCallback));
    CHECK(module->Evaluate(env.local())
              .ToLocalChecked()
              ->StrictEquals(v8_str("gaga")));
  }

  CHECK(!try_catch.HasCaught());
}

}  // anonymous namespace
