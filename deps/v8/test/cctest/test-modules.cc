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
      "import './foo.js';\n"
      "export {} from './bar.js';");
  ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK_EQ(Module::kUninstantiated, module->GetStatus());
  CHECK_EQ(2, module->GetModuleRequestsLength());
  CHECK(v8_str("./foo.js")->StrictEquals(module->GetModuleRequest(0)));
  v8::Location loc = module->GetModuleRequestLocation(0);
  CHECK_EQ(0, loc.GetLineNumber());
  CHECK_EQ(7, loc.GetColumnNumber());

  CHECK(v8_str("./bar.js")->StrictEquals(module->GetModuleRequest(1)));
  loc = module->GetModuleRequestLocation(1);
  CHECK_EQ(1, loc.GetLineNumber());
  CHECK_EQ(15, loc.GetColumnNumber());

  // Instantiation should fail.
  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(module->InstantiateModule(env.local(), FailAlwaysResolveCallback)
              .IsNothing());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
    CHECK_EQ(Module::kErrored, module->GetStatus());
    Local<Value> exception = module->GetException();
    CHECK(exception->StrictEquals(v8_str("boom")));
    // TODO(neis): Check object identity.
  }

  // Start over again...
  module = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();

  // Instantiation should fail if a sub-module fails to resolve.
  g_count = 0;
  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(
        module->InstantiateModule(env.local(), FailOnSecondCallResolveCallback)
            .IsNothing());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("booom")));
    CHECK_EQ(Module::kErrored, module->GetStatus());
    Local<Value> exception = module->GetException();
    CHECK(exception->StrictEquals(v8_str("booom")));
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
  CHECK_EQ(Module::kUninstantiated, module->GetStatus());
  CHECK(module
            ->InstantiateModule(env.local(),
                                CompileSpecifierAsModuleResolveCallback)
            .FromJust());
  CHECK_EQ(Module::kInstantiated, module->GetStatus());
  CHECK(!module->Evaluate(env.local()).IsEmpty());
  CHECK_EQ(Module::kEvaluated, module->GetStatus());
  ExpectInt32("Object.expando", 10);

  CHECK(!try_catch.HasCaught());
}

TEST(ModuleEvaluationError) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  Local<String> source_text =
      v8_str("Object.x = (Object.x || 0) + 1; throw 'boom';");
  ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK_EQ(Module::kUninstantiated, module->GetStatus());
  CHECK(module
            ->InstantiateModule(env.local(),
                                CompileSpecifierAsModuleResolveCallback)
            .FromJust());
  CHECK_EQ(Module::kInstantiated, module->GetStatus());

  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(module->Evaluate(env.local()).IsEmpty());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
    CHECK_EQ(Module::kErrored, module->GetStatus());
    Local<Value> exception = module->GetException();
    CHECK(exception->StrictEquals(v8_str("boom")));
    ExpectInt32("Object.x", 1);
  }

  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(module->Evaluate(env.local()).IsEmpty());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
    CHECK_EQ(Module::kErrored, module->GetStatus());
    Local<Value> exception = module->GetException();
    CHECK(exception->StrictEquals(v8_str("boom")));
    ExpectInt32("Object.x", 1);
  }

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
    CHECK_EQ(Module::kUninstantiated, module->GetStatus());
    CHECK(module
              ->InstantiateModule(env.local(),
                                  CompileSpecifierAsModuleResolveCallback)
              .FromJust());
    CHECK_EQ(Module::kInstantiated, module->GetStatus());
    CHECK(module->Evaluate(env.local()).ToLocalChecked()->IsUndefined());
    CHECK_EQ(Module::kEvaluated, module->GetStatus());
    CHECK(module->Evaluate(env.local()).ToLocalChecked()->IsUndefined());
    CHECK_EQ(Module::kEvaluated, module->GetStatus());
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
    CHECK_EQ(Module::kUninstantiated, module->GetStatus());
    CHECK(module
              ->InstantiateModule(env.local(),
                                  CompileSpecifierAsModuleResolveCallback)
              .FromJust());
    CHECK_EQ(Module::kInstantiated, module->GetStatus());
    CHECK(module->Evaluate(env.local())
              .ToLocalChecked()
              ->StrictEquals(v8_str("gaga")));
    CHECK_EQ(Module::kEvaluated, module->GetStatus());
    CHECK(module->Evaluate(env.local()).ToLocalChecked()->IsUndefined());
    CHECK_EQ(Module::kEvaluated, module->GetStatus());
  }

  CHECK(!try_catch.HasCaught());
}

TEST(ModuleNamespace) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  Local<v8::Object> ReferenceError =
      CompileRun("ReferenceError")->ToObject(env.local()).ToLocalChecked();

  Local<String> source_text = v8_str(
      "import {a, b} from 'export var a = 1; export let b = 2';"
      "export function geta() {return a};"
      "export function getb() {return b};"
      "export let radio = 3;"
      "export var gaga = 4;");
  ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK_EQ(Module::kUninstantiated, module->GetStatus());
  CHECK(module
            ->InstantiateModule(env.local(),
                                CompileSpecifierAsModuleResolveCallback)
            .FromJust());
  CHECK_EQ(Module::kInstantiated, module->GetStatus());
  Local<Value> ns = module->GetModuleNamespace();
  CHECK_EQ(Module::kInstantiated, module->GetStatus());
  Local<v8::Object> nsobj = ns->ToObject(env.local()).ToLocalChecked();

  // a, b
  CHECK(nsobj->Get(env.local(), v8_str("a")).ToLocalChecked()->IsUndefined());
  CHECK(nsobj->Get(env.local(), v8_str("b")).ToLocalChecked()->IsUndefined());

  // geta
  {
    auto geta = nsobj->Get(env.local(), v8_str("geta")).ToLocalChecked();
    auto a = geta.As<v8::Function>()
                 ->Call(env.local(), geta, 0, nullptr)
                 .ToLocalChecked();
    CHECK(a->IsUndefined());
  }

  // getb
  {
    v8::TryCatch inner_try_catch(isolate);
    auto getb = nsobj->Get(env.local(), v8_str("getb")).ToLocalChecked();
    CHECK(
        getb.As<v8::Function>()->Call(env.local(), getb, 0, nullptr).IsEmpty());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()
              ->InstanceOf(env.local(), ReferenceError)
              .FromJust());
  }

  // radio
  {
    v8::TryCatch inner_try_catch(isolate);
    // https://bugs.chromium.org/p/v8/issues/detail?id=7235
    // CHECK(nsobj->Get(env.local(), v8_str("radio")).IsEmpty());
    CHECK(nsobj->Get(env.local(), v8_str("radio"))
              .ToLocalChecked()
              ->IsUndefined());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()
              ->InstanceOf(env.local(), ReferenceError)
              .FromJust());
  }

  // gaga
  {
    auto gaga = nsobj->Get(env.local(), v8_str("gaga")).ToLocalChecked();
    CHECK(gaga->IsUndefined());
  }

  CHECK(!try_catch.HasCaught());
  CHECK_EQ(Module::kInstantiated, module->GetStatus());
  module->Evaluate(env.local()).ToLocalChecked();
  CHECK_EQ(Module::kEvaluated, module->GetStatus());

  // geta
  {
    auto geta = nsobj->Get(env.local(), v8_str("geta")).ToLocalChecked();
    auto a = geta.As<v8::Function>()
                 ->Call(env.local(), geta, 0, nullptr)
                 .ToLocalChecked();
    CHECK_EQ(1, a->Int32Value(env.local()).FromJust());
  }

  // getb
  {
    auto getb = nsobj->Get(env.local(), v8_str("getb")).ToLocalChecked();
    auto b = getb.As<v8::Function>()
                 ->Call(env.local(), getb, 0, nullptr)
                 .ToLocalChecked();
    CHECK_EQ(2, b->Int32Value(env.local()).FromJust());
  }

  // radio
  {
    auto radio = nsobj->Get(env.local(), v8_str("radio")).ToLocalChecked();
    CHECK_EQ(3, radio->Int32Value(env.local()).FromJust());
  }

  // gaga
  {
    auto gaga = nsobj->Get(env.local(), v8_str("gaga")).ToLocalChecked();
    CHECK_EQ(4, gaga->Int32Value(env.local()).FromJust());
  }

  CHECK(!try_catch.HasCaught());
}
}  // anonymous namespace
