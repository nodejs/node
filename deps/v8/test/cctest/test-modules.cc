// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/flags.h"

#include "test/cctest/cctest.h"

namespace {

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Module;
using v8::Promise;
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

static Local<Module> dep1;
static Local<Module> dep2;
MaybeLocal<Module> ResolveCallback(Local<Context> context,
                                   Local<String> specifier,
                                   Local<Module> referrer) {
  Isolate* isolate = CcTest::isolate();
  if (specifier->StrictEquals(v8_str("./dep1.js"))) {
    return dep1;
  } else if (specifier->StrictEquals(v8_str("./dep2.js"))) {
    return dep2;
  } else {
    isolate->ThrowException(v8_str("boom"));
    return MaybeLocal<Module>();
  }
}

TEST(ModuleInstantiationFailures1) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  Local<Module> module;
  {
    Local<String> source_text = v8_str(
        "import './foo.js';\n"
        "export {} from './bar.js';");
    ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    module = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
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
  }

  // Instantiation should fail.
  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(module->InstantiateModule(env.local(), ResolveCallback).IsNothing());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
    CHECK_EQ(Module::kUninstantiated, module->GetStatus());
  }

  // Start over again...
  {
    Local<String> source_text = v8_str(
        "import './dep1.js';\n"
        "export {} from './bar.js';");
    ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    module = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  }

  // dep1.js
  {
    Local<String> source_text = v8_str("");
    ScriptOrigin origin = ModuleOrigin(v8_str("dep1.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    dep1 = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  }

  // Instantiation should fail because a sub-module fails to resolve.
  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(module->InstantiateModule(env.local(), ResolveCallback).IsNothing());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
    CHECK_EQ(Module::kUninstantiated, module->GetStatus());
  }

  CHECK(!try_catch.HasCaught());
}

TEST(ModuleInstantiationFailures2) {
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  // root1.js
  Local<Module> root;
  {
    Local<String> source_text =
        v8_str("import './dep1.js'; import './dep2.js'");
    ScriptOrigin origin = ModuleOrigin(v8_str("root1.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    root = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  }

  // dep1.js
  {
    Local<String> source_text = v8_str("export let x = 42");
    ScriptOrigin origin = ModuleOrigin(v8_str("dep1.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    dep1 = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  }

  // dep2.js
  {
    Local<String> source_text = v8_str("import {foo} from './dep3.js'");
    ScriptOrigin origin = ModuleOrigin(v8_str("dep2.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    dep2 = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  }

  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(root->InstantiateModule(env.local(), ResolveCallback).IsNothing());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
    CHECK_EQ(Module::kUninstantiated, root->GetStatus());
    CHECK_EQ(Module::kUninstantiated, dep1->GetStatus());
    CHECK_EQ(Module::kUninstantiated, dep2->GetStatus());
  }

  // Change dep2.js
  {
    Local<String> source_text = v8_str("import {foo} from './dep2.js'");
    ScriptOrigin origin = ModuleOrigin(v8_str("dep2.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    dep2 = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  }

  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(root->InstantiateModule(env.local(), ResolveCallback).IsNothing());
    CHECK(inner_try_catch.HasCaught());
    CHECK(!inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
    CHECK_EQ(Module::kUninstantiated, root->GetStatus());
    CHECK_EQ(Module::kInstantiated, dep1->GetStatus());
    CHECK_EQ(Module::kUninstantiated, dep2->GetStatus());
  }

  // Change dep2.js again
  {
    Local<String> source_text = v8_str("import {foo} from './dep3.js'");
    ScriptOrigin origin = ModuleOrigin(v8_str("dep2.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    dep2 = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  }

  {
    v8::TryCatch inner_try_catch(isolate);
    CHECK(root->InstantiateModule(env.local(), ResolveCallback).IsNothing());
    CHECK(inner_try_catch.HasCaught());
    CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
    CHECK_EQ(Module::kUninstantiated, root->GetStatus());
    CHECK_EQ(Module::kInstantiated, dep1->GetStatus());
    CHECK_EQ(Module::kUninstantiated, dep2->GetStatus());
  }
}

static MaybeLocal<Module> CompileSpecifierAsModuleResolveCallback(
    Local<Context> context, Local<String> specifier, Local<Module> referrer) {
  ScriptOrigin origin = ModuleOrigin(v8_str("module.js"), CcTest::isolate());
  ScriptCompiler::Source source(specifier, origin);
  return ScriptCompiler::CompileModule(CcTest::isolate(), &source)
      .ToLocalChecked();
}

TEST(ModuleEvaluation) {
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;

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

    MaybeLocal<Value> result = module->Evaluate(env.local());
    CHECK_EQ(Module::kEvaluated, module->GetStatus());
    if (i::FLAG_harmony_top_level_await) {
      Local<Promise> promise = Local<Promise>::Cast(result.ToLocalChecked());
      CHECK_EQ(promise->State(), v8::Promise::kFulfilled);
      CHECK(promise->Result()->IsUndefined());
    } else {
      CHECK(!result.IsEmpty());
      ExpectInt32("Object.expando", 10);
    }
    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = prev_top_level_await;
}

TEST(ModuleEvaluationError1) {
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;

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
      MaybeLocal<Value> result = module->Evaluate(env.local());
      CHECK_EQ(Module::kErrored, module->GetStatus());
      Local<Value> exception = module->GetException();
      CHECK(exception->StrictEquals(v8_str("boom")));
      ExpectInt32("Object.x", 1);

      if (i::FLAG_harmony_top_level_await) {
        // With top level await, we do not throw and errored evaluation returns
        // a rejected promise with the exception.
        CHECK(!inner_try_catch.HasCaught());
        Local<Promise> promise = Local<Promise>::Cast(result.ToLocalChecked());
        CHECK_EQ(promise->State(), v8::Promise::kRejected);
        CHECK_EQ(promise->Result(), module->GetException());
      } else {
        CHECK(inner_try_catch.HasCaught());
        CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
        CHECK(result.IsEmpty());
      }
    }

    {
      v8::TryCatch inner_try_catch(isolate);
      MaybeLocal<Value> result = module->Evaluate(env.local());
      CHECK_EQ(Module::kErrored, module->GetStatus());
      Local<Value> exception = module->GetException();
      CHECK(exception->StrictEquals(v8_str("boom")));
      ExpectInt32("Object.x", 1);

      if (i::FLAG_harmony_top_level_await) {
        // With top level await, we do not throw and errored evaluation returns
        // a rejected promise with the exception.
        CHECK(!inner_try_catch.HasCaught());
        Local<Promise> promise = Local<Promise>::Cast(result.ToLocalChecked());
        CHECK_EQ(promise->State(), v8::Promise::kRejected);
        CHECK_EQ(promise->Result(), module->GetException());
      } else {
        CHECK(inner_try_catch.HasCaught());
        CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
        CHECK(result.IsEmpty());
      }
    }

    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = prev_top_level_await;
}

static Local<Module> failure_module;
static Local<Module> dependent_module;
MaybeLocal<Module> ResolveCallbackForModuleEvaluationError2(
    Local<Context> context, Local<String> specifier, Local<Module> referrer) {
  if (specifier->StrictEquals(v8_str("./failure.js"))) {
    return failure_module;
  } else {
    CHECK(specifier->StrictEquals(v8_str("./dependent.js")));
    return dependent_module;
  }
}

TEST(ModuleEvaluationError2) {
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;

    Isolate* isolate = CcTest::isolate();
    HandleScope scope(isolate);
    LocalContext env;
    v8::TryCatch try_catch(isolate);

    Local<String> failure_text = v8_str("throw 'boom';");
    ScriptOrigin failure_origin =
        ModuleOrigin(v8_str("failure.js"), CcTest::isolate());
    ScriptCompiler::Source failure_source(failure_text, failure_origin);
    failure_module = ScriptCompiler::CompileModule(isolate, &failure_source)
                         .ToLocalChecked();
    CHECK_EQ(Module::kUninstantiated, failure_module->GetStatus());
    CHECK(failure_module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackForModuleEvaluationError2)
              .FromJust());
    CHECK_EQ(Module::kInstantiated, failure_module->GetStatus());

    {
      v8::TryCatch inner_try_catch(isolate);
      MaybeLocal<Value> result = failure_module->Evaluate(env.local());
      CHECK_EQ(Module::kErrored, failure_module->GetStatus());
      Local<Value> exception = failure_module->GetException();
      CHECK(exception->StrictEquals(v8_str("boom")));

      if (i::FLAG_harmony_top_level_await) {
        // With top level await, we do not throw and errored evaluation returns
        // a rejected promise with the exception.
        CHECK(!inner_try_catch.HasCaught());
        Local<Promise> promise = Local<Promise>::Cast(result.ToLocalChecked());
        CHECK_EQ(promise->State(), v8::Promise::kRejected);
        CHECK_EQ(promise->Result(), failure_module->GetException());
      } else {
        CHECK(inner_try_catch.HasCaught());
        CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
        CHECK(result.IsEmpty());
      }
    }

    Local<String> dependent_text =
        v8_str("import './failure.js'; export const c = 123;");
    ScriptOrigin dependent_origin =
        ModuleOrigin(v8_str("dependent.js"), CcTest::isolate());
    ScriptCompiler::Source dependent_source(dependent_text, dependent_origin);
    dependent_module = ScriptCompiler::CompileModule(isolate, &dependent_source)
                           .ToLocalChecked();
    CHECK_EQ(Module::kUninstantiated, dependent_module->GetStatus());
    CHECK(dependent_module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackForModuleEvaluationError2)
              .FromJust());
    CHECK_EQ(Module::kInstantiated, dependent_module->GetStatus());

    {
      v8::TryCatch inner_try_catch(isolate);
      MaybeLocal<Value> result = dependent_module->Evaluate(env.local());
      CHECK_EQ(Module::kErrored, dependent_module->GetStatus());
      Local<Value> exception = dependent_module->GetException();
      CHECK(exception->StrictEquals(v8_str("boom")));
      CHECK_EQ(exception, failure_module->GetException());

      if (i::FLAG_harmony_top_level_await) {
        // With top level await, we do not throw and errored evaluation returns
        // a rejected promise with the exception.
        CHECK(!inner_try_catch.HasCaught());
        Local<Promise> promise = Local<Promise>::Cast(result.ToLocalChecked());
        CHECK_EQ(promise->State(), v8::Promise::kRejected);
        CHECK_EQ(promise->Result(), failure_module->GetException());
      } else {
        CHECK(inner_try_catch.HasCaught());
        CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
        CHECK(result.IsEmpty());
      }
    }

    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = prev_top_level_await;
}

TEST(ModuleEvaluationCompletion1) {
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;

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

      // Evaluate twice.
      Local<Value> result_1 = module->Evaluate(env.local()).ToLocalChecked();
      CHECK_EQ(Module::kEvaluated, module->GetStatus());
      Local<Value> result_2 = module->Evaluate(env.local()).ToLocalChecked();
      CHECK_EQ(Module::kEvaluated, module->GetStatus());

      if (i::FLAG_harmony_top_level_await) {
        Local<Promise> promise = Local<Promise>::Cast(result_1);
        CHECK_EQ(promise->State(), v8::Promise::kFulfilled);
        CHECK(promise->Result()->IsUndefined());

        // Second evaluation should return the same promise.
        Local<Promise> promise_too = Local<Promise>::Cast(result_2);
        CHECK_EQ(promise, promise_too);
        CHECK_EQ(promise_too->State(), v8::Promise::kFulfilled);
        CHECK(promise_too->Result()->IsUndefined());
      } else {
        CHECK(result_1->IsUndefined());
        CHECK(result_2->IsUndefined());
      }
    }
    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = prev_top_level_await;
}

TEST(ModuleEvaluationCompletion2) {
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;

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

      Local<Value> result_1 = module->Evaluate(env.local()).ToLocalChecked();
      CHECK_EQ(Module::kEvaluated, module->GetStatus());

      Local<Value> result_2 = module->Evaluate(env.local()).ToLocalChecked();
      CHECK_EQ(Module::kEvaluated, module->GetStatus());
      if (i::FLAG_harmony_top_level_await) {
        Local<Promise> promise = Local<Promise>::Cast(result_1);
        CHECK_EQ(promise->State(), v8::Promise::kFulfilled);
        CHECK(promise->Result()->IsUndefined());

        // Second Evaluation should return the same promise.
        Local<Promise> promise_too = Local<Promise>::Cast(result_2);
        CHECK_EQ(promise, promise_too);
        CHECK_EQ(promise_too->State(), v8::Promise::kFulfilled);
        CHECK(promise_too->Result()->IsUndefined());
      } else {
        CHECK(result_1->StrictEquals(v8_str("gaga")));
        CHECK(result_2->IsUndefined());
      }
    }
    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = prev_top_level_await;
}

TEST(ModuleNamespace) {
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;

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
    CHECK_EQ(nsobj->CreationContext(), env.local());

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
      CHECK(getb.As<v8::Function>()
                ->Call(env.local(), getb, 0, nullptr)
                .IsEmpty());
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
  i::FLAG_harmony_top_level_await = prev_top_level_await;
}

TEST(ModuleEvaluationTopLevelAwait) {
  bool previous_top_level_await_flag_value = i::FLAG_harmony_top_level_await;
  i::FLAG_harmony_top_level_await = true;
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);
  const char* sources[] = {
      "await 42",
      "import 'await 42';",
      "import '42'; import 'await 42';",
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
    Local<Promise> promise =
        Local<Promise>::Cast(module->Evaluate(env.local()).ToLocalChecked());
    CHECK_EQ(Module::kEvaluated, module->GetStatus());
    CHECK_EQ(promise->State(), v8::Promise::kFulfilled);
    CHECK(promise->Result()->IsUndefined());
    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = previous_top_level_await_flag_value;
}

TEST(ModuleEvaluationTopLevelAwaitError) {
  bool previous_top_level_await_flag_value = i::FLAG_harmony_top_level_await;
  i::FLAG_harmony_top_level_await = true;
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  const char* sources[] = {
      "await 42; throw 'boom';",
      "import 'await 42; throw \"boom\";';",
      "import '42'; import 'await 42; throw \"boom\";';",
  };

  for (auto src : sources) {
    v8::TryCatch try_catch(isolate);
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
    Local<Promise> promise =
        Local<Promise>::Cast(module->Evaluate(env.local()).ToLocalChecked());
    CHECK_EQ(Module::kErrored, module->GetStatus());
    CHECK_EQ(promise->State(), v8::Promise::kRejected);
    CHECK(promise->Result()->StrictEquals(v8_str("boom")));
    CHECK(module->GetException()->StrictEquals(v8_str("boom")));

    // TODO(joshualitt) I am not sure, but this might not be supposed to throw
    // because it is async.
    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = previous_top_level_await_flag_value;
}

namespace {
struct DynamicImportData {
  DynamicImportData(Isolate* isolate_, Local<Promise::Resolver> resolver_,
                    Local<Context> context_, bool should_resolve_)
      : isolate(isolate_), should_resolve(should_resolve_) {
    resolver.Reset(isolate, resolver_);
    context.Reset(isolate, context_);
  }

  Isolate* isolate;
  v8::Global<Promise::Resolver> resolver;
  v8::Global<Context> context;
  bool should_resolve;
};

void DoHostImportModuleDynamically(void* import_data) {
  std::unique_ptr<DynamicImportData> import_data_(
      static_cast<DynamicImportData*>(import_data));
  Isolate* isolate(import_data_->isolate);
  HandleScope handle_scope(isolate);

  Local<Promise::Resolver> resolver(import_data_->resolver.Get(isolate));
  Local<Context> realm(import_data_->context.Get(isolate));
  Context::Scope context_scope(realm);

  if (import_data_->should_resolve) {
    resolver->Resolve(realm, True(isolate)).ToChecked();
  } else {
    resolver->Reject(realm, v8_str("boom")).ToChecked();
  }
}

v8::MaybeLocal<v8::Promise> HostImportModuleDynamicallyCallbackResolve(
    Local<Context> context, Local<v8::ScriptOrModule> referrer,
    Local<String> specifier) {
  Isolate* isolate = context->GetIsolate();
  Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();

  DynamicImportData* data =
      new DynamicImportData(isolate, resolver, context, true);
  isolate->EnqueueMicrotask(DoHostImportModuleDynamically, data);
  return resolver->GetPromise();
}

v8::MaybeLocal<v8::Promise> HostImportModuleDynamicallyCallbackReject(
    Local<Context> context, Local<v8::ScriptOrModule> referrer,
    Local<String> specifier) {
  Isolate* isolate = context->GetIsolate();
  Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();

  DynamicImportData* data =
      new DynamicImportData(isolate, resolver, context, false);
  isolate->EnqueueMicrotask(DoHostImportModuleDynamically, data);
  return resolver->GetPromise();
}

}  // namespace

TEST(ModuleEvaluationTopLevelAwaitDynamicImport) {
  bool previous_top_level_await_flag_value = i::FLAG_harmony_top_level_await;
  bool previous_dynamic_import_flag_value = i::FLAG_harmony_dynamic_import;
  i::FLAG_harmony_top_level_await = true;
  i::FLAG_harmony_dynamic_import = true;
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
  isolate->SetHostImportModuleDynamicallyCallback(
      HostImportModuleDynamicallyCallbackResolve);
  LocalContext env;
  v8::TryCatch try_catch(isolate);
  const char* sources[] = {
      "await import('foo');",
      "import 'await import(\"foo\");';",
      "import '42'; import 'await import(\"foo\");';",
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

    Local<Promise> promise =
        Local<Promise>::Cast(module->Evaluate(env.local()).ToLocalChecked());
    CHECK_EQ(Module::kEvaluated, module->GetStatus());
    CHECK_EQ(promise->State(), v8::Promise::kPending);
    CHECK(!try_catch.HasCaught());

    isolate->PerformMicrotaskCheckpoint();
    CHECK_EQ(promise->State(), v8::Promise::kFulfilled);
  }
  i::FLAG_harmony_top_level_await = previous_top_level_await_flag_value;
  i::FLAG_harmony_dynamic_import = previous_dynamic_import_flag_value;
}

TEST(ModuleEvaluationTopLevelAwaitDynamicImportError) {
  bool previous_top_level_await_flag_value = i::FLAG_harmony_top_level_await;
  bool previous_dynamic_import_flag_value = i::FLAG_harmony_dynamic_import;
  i::FLAG_harmony_top_level_await = true;
  i::FLAG_harmony_dynamic_import = true;
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
  isolate->SetHostImportModuleDynamicallyCallback(
      HostImportModuleDynamicallyCallbackReject);
  LocalContext env;
  v8::TryCatch try_catch(isolate);
  const char* sources[] = {
      "await import('foo');",
      "import 'await import(\"foo\");';",
      "import '42'; import 'await import(\"foo\");';",
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

    Local<Promise> promise =
        Local<Promise>::Cast(module->Evaluate(env.local()).ToLocalChecked());
    CHECK_EQ(Module::kEvaluated, module->GetStatus());
    CHECK_EQ(promise->State(), v8::Promise::kPending);
    CHECK(!try_catch.HasCaught());

    isolate->PerformMicrotaskCheckpoint();
    CHECK_EQ(Module::kErrored, module->GetStatus());
    CHECK_EQ(promise->State(), v8::Promise::kRejected);
    CHECK(promise->Result()->StrictEquals(v8_str("boom")));
    CHECK(module->GetException()->StrictEquals(v8_str("boom")));
    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = previous_top_level_await_flag_value;
  i::FLAG_harmony_dynamic_import = previous_dynamic_import_flag_value;
}

TEST(TerminateExecutionTopLevelAwaitSync) {
  bool previous_top_level_await_flag_value = i::FLAG_harmony_top_level_await;
  i::FLAG_harmony_top_level_await = true;

  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  env.local()
      ->Global()
      ->Set(env.local(), v8_str("terminate"),
            v8::Function::New(env.local(),
                              [](const v8::FunctionCallbackInfo<Value>& info) {
                                info.GetIsolate()->TerminateExecution();
                              })
                .ToLocalChecked())
      .ToChecked();

  Local<String> source_text = v8_str("terminate(); while (true) {}");
  ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK(module
            ->InstantiateModule(env.local(),
                                CompileSpecifierAsModuleResolveCallback)
            .FromJust());

  CHECK(module->Evaluate(env.local()).IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.HasTerminated());
  CHECK_EQ(module->GetStatus(), Module::kErrored);
  CHECK_EQ(module->GetException(), v8::Null(isolate));

  i::FLAG_harmony_top_level_await = previous_top_level_await_flag_value;
}

TEST(TerminateExecutionTopLevelAwaitAsync) {
  bool previous_top_level_await_flag_value = i::FLAG_harmony_top_level_await;
  i::FLAG_harmony_top_level_await = true;

  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;
  v8::TryCatch try_catch(isolate);

  env.local()
      ->Global()
      ->Set(env.local(), v8_str("terminate"),
            v8::Function::New(env.local(),
                              [](const v8::FunctionCallbackInfo<Value>& info) {
                                info.GetIsolate()->TerminateExecution();
                              })
                .ToLocalChecked())
      .ToChecked();

  Local<Promise::Resolver> eval_promise =
      Promise::Resolver::New(env.local()).ToLocalChecked();
  env.local()
      ->Global()
      ->Set(env.local(), v8_str("evalPromise"), eval_promise)
      .ToChecked();

  Local<String> source_text =
      v8_str("await evalPromise; terminate(); while (true) {}");
  ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module =
      ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
  CHECK(module
            ->InstantiateModule(env.local(),
                                CompileSpecifierAsModuleResolveCallback)
            .FromJust());

  Local<Promise> promise =
      Local<Promise>::Cast(module->Evaluate(env.local()).ToLocalChecked());
  CHECK_EQ(module->GetStatus(), Module::kEvaluated);
  CHECK_EQ(promise->State(), Promise::PromiseState::kPending);
  CHECK(!try_catch.HasCaught());
  CHECK(!try_catch.HasTerminated());

  eval_promise->Resolve(env.local(), v8::Undefined(isolate)).ToChecked();

  CHECK(try_catch.HasCaught());
  CHECK(try_catch.HasTerminated());
  CHECK_EQ(promise->State(), Promise::PromiseState::kPending);

  // The termination exception doesn't trigger the module's
  // catch handler, so the module isn't transitioned to kErrored.
  CHECK_EQ(module->GetStatus(), Module::kEvaluated);

  i::FLAG_harmony_top_level_await = previous_top_level_await_flag_value;
}

}  // anonymous namespace
