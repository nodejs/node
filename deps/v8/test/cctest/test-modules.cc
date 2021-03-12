// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/flags.h"

#include "test/cctest/cctest.h"

namespace {

using v8::Context;
using v8::Data;
using v8::FixedArray;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::Location;
using v8::MaybeLocal;
using v8::Module;
using v8::ModuleRequest;
using v8::Promise;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Value;

ScriptOrigin ModuleOrigin(Local<v8::Value> resource_name, Isolate* isolate) {
  ScriptOrigin origin(isolate, resource_name, 0, 0, false, -1,
                      Local<v8::Value>(), false, false, true);
  return origin;
}

static Local<Module> dep1;
static Local<Module> dep2;
MaybeLocal<Module> ResolveCallback(Local<Context> context,
                                   Local<String> specifier,
                                   Local<FixedArray> import_assertions,
                                   Local<Module> referrer) {
  CHECK_EQ(0, import_assertions->Length());
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
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;
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
      Local<FixedArray> module_requests = module->GetModuleRequests();
      CHECK_EQ(2, module_requests->Length());
      Local<ModuleRequest> module_request_0 =
          module_requests->Get(env.local(), 0).As<ModuleRequest>();
      CHECK(v8_str("./foo.js")->StrictEquals(module_request_0->GetSpecifier()));
      int offset = module_request_0->GetSourceOffset();
      CHECK_EQ(7, offset);
      Location loc = module->SourceOffsetToLocation(offset);
      CHECK_EQ(0, loc.GetLineNumber());
      CHECK_EQ(7, loc.GetColumnNumber());
      CHECK_EQ(0, module_request_0->GetImportAssertions()->Length());

      Local<ModuleRequest> module_request_1 =
          module_requests->Get(env.local(), 1).As<ModuleRequest>();
      CHECK(v8_str("./bar.js")->StrictEquals(module_request_1->GetSpecifier()));
      offset = module_request_1->GetSourceOffset();
      CHECK_EQ(34, offset);
      loc = module->SourceOffsetToLocation(offset);
      CHECK_EQ(1, loc.GetLineNumber());
      CHECK_EQ(15, loc.GetColumnNumber());
      CHECK_EQ(0, module_request_1->GetImportAssertions()->Length());
    }

    // Instantiation should fail.
    {
      v8::TryCatch inner_try_catch(isolate);
      CHECK(
          module->InstantiateModule(env.local(), ResolveCallback).IsNothing());
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
      CHECK(
          module->InstantiateModule(env.local(), ResolveCallback).IsNothing());
      CHECK(inner_try_catch.HasCaught());
      CHECK(inner_try_catch.Exception()->StrictEquals(v8_str("boom")));
      CHECK_EQ(Module::kUninstantiated, module->GetStatus());
    }

    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = prev_top_level_await;
}

static Local<Module> fooModule;
static Local<Module> barModule;
MaybeLocal<Module> ResolveCallbackWithImportAssertions(
    Local<Context> context, Local<String> specifier,
    Local<FixedArray> import_assertions, Local<Module> referrer) {
  Isolate* isolate = CcTest::isolate();
  LocalContext env;

  if (specifier->StrictEquals(v8_str("./foo.js"))) {
    CHECK_EQ(0, import_assertions->Length());

    return fooModule;
  } else if (specifier->StrictEquals(v8_str("./bar.js"))) {
    CHECK_EQ(3, import_assertions->Length());
    Local<String> assertion_key =
        import_assertions->Get(env.local(), 0).As<Value>().As<String>();
    CHECK(v8_str("a")->StrictEquals(assertion_key));
    Local<String> assertion_value =
        import_assertions->Get(env.local(), 1).As<Value>().As<String>();
    CHECK(v8_str("b")->StrictEquals(assertion_value));
    Local<Data> assertion_source_offset_object =
        import_assertions->Get(env.local(), 2);
    Local<Int32> assertion_source_offset_int32 =
        assertion_source_offset_object.As<Value>()
            ->ToInt32(context)
            .ToLocalChecked();
    int32_t assertion_source_offset = assertion_source_offset_int32->Value();
    CHECK_EQ(65, assertion_source_offset);
    Location loc = referrer->SourceOffsetToLocation(assertion_source_offset);
    CHECK_EQ(1, loc.GetLineNumber());
    CHECK_EQ(35, loc.GetColumnNumber());

    return barModule;
  } else {
    isolate->ThrowException(v8_str("boom"));
    return MaybeLocal<Module>();
  }
}

TEST(ModuleInstantiationWithImportAssertions) {
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  bool prev_import_assertions = i::FLAG_harmony_import_assertions;
  i::FLAG_harmony_import_assertions = true;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;
    Isolate* isolate = CcTest::isolate();
    HandleScope scope(isolate);
    LocalContext env;
    v8::TryCatch try_catch(isolate);

    Local<Module> module;
    {
      Local<String> source_text = v8_str(
          "import './foo.js' assert { };\n"
          "export {} from './bar.js' assert { a: 'b' };");
      ScriptOrigin origin = ModuleOrigin(v8_str("file.js"), CcTest::isolate());
      ScriptCompiler::Source source(source_text, origin);
      module = ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
      CHECK_EQ(Module::kUninstantiated, module->GetStatus());
      Local<FixedArray> module_requests = module->GetModuleRequests();
      CHECK_EQ(2, module_requests->Length());
      Local<ModuleRequest> module_request_0 =
          module_requests->Get(env.local(), 0).As<ModuleRequest>();
      CHECK(v8_str("./foo.js")->StrictEquals(module_request_0->GetSpecifier()));
      int offset = module_request_0->GetSourceOffset();
      CHECK_EQ(7, offset);
      Location loc = module->SourceOffsetToLocation(offset);
      CHECK_EQ(0, loc.GetLineNumber());
      CHECK_EQ(7, loc.GetColumnNumber());
      CHECK_EQ(0, module_request_0->GetImportAssertions()->Length());

      Local<ModuleRequest> module_request_1 =
          module_requests->Get(env.local(), 1).As<ModuleRequest>();
      CHECK(v8_str("./bar.js")->StrictEquals(module_request_1->GetSpecifier()));
      offset = module_request_1->GetSourceOffset();
      CHECK_EQ(45, offset);
      loc = module->SourceOffsetToLocation(offset);
      CHECK_EQ(1, loc.GetLineNumber());
      CHECK_EQ(15, loc.GetColumnNumber());

      Local<FixedArray> import_assertions_1 =
          module_request_1->GetImportAssertions();
      CHECK_EQ(3, import_assertions_1->Length());
      Local<String> assertion_key =
          import_assertions_1->Get(env.local(), 0).As<String>();
      CHECK(v8_str("a")->StrictEquals(assertion_key));
      Local<String> assertion_value =
          import_assertions_1->Get(env.local(), 1).As<String>();
      CHECK(v8_str("b")->StrictEquals(assertion_value));
      int32_t assertion_source_offset =
          import_assertions_1->Get(env.local(), 2).As<Int32>()->Value();
      CHECK_EQ(65, assertion_source_offset);
      loc = module->SourceOffsetToLocation(assertion_source_offset);
      CHECK_EQ(1, loc.GetLineNumber());
      CHECK_EQ(35, loc.GetColumnNumber());
    }

    // foo.js
    {
      Local<String> source_text = v8_str("Object.expando = 40");
      ScriptOrigin origin = ModuleOrigin(v8_str("foo.js"), CcTest::isolate());
      ScriptCompiler::Source source(source_text, origin);
      fooModule =
          ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    }

    // bar.js
    {
      Local<String> source_text = v8_str("Object.expando += 2");
      ScriptOrigin origin = ModuleOrigin(v8_str("bar.js"), CcTest::isolate());
      ScriptCompiler::Source source(source_text, origin);
      barModule =
          ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    }

    CHECK(module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackWithImportAssertions)
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
      ExpectInt32("Object.expando", 42);
    }
    CHECK(!try_catch.HasCaught());
  }
  i::FLAG_harmony_top_level_await = prev_top_level_await;
  i::FLAG_harmony_import_assertions = prev_import_assertions;
}

TEST(ModuleInstantiationFailures2) {
  bool prev_top_level_await = i::FLAG_harmony_top_level_await;
  for (auto top_level_await : {true, false}) {
    i::FLAG_harmony_top_level_await = top_level_await;

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
  i::FLAG_harmony_top_level_await = prev_top_level_await;
}

static MaybeLocal<Module> CompileSpecifierAsModuleResolveCallback(
    Local<Context> context, Local<String> specifier,
    Local<FixedArray> import_assertions, Local<Module> referrer) {
  CHECK_EQ(0, import_assertions->Length());
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
      Local<Promise> promise = result.ToLocalChecked().As<Promise>();
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
        Local<Promise> promise = result.ToLocalChecked().As<Promise>();
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
        Local<Promise> promise = result.ToLocalChecked().As<Promise>();
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
    Local<Context> context, Local<String> specifier,
    Local<FixedArray> import_assertions, Local<Module> referrer) {
  CHECK_EQ(0, import_assertions->Length());
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
        Local<Promise> promise = result.ToLocalChecked().As<Promise>();
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
        Local<Promise> promise = result.ToLocalChecked().As<Promise>();
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
        Local<Promise> promise = result_1.As<Promise>();
        CHECK_EQ(promise->State(), v8::Promise::kFulfilled);
        CHECK(promise->Result()->IsUndefined());

        // Second evaluation should return the same promise.
        Local<Promise> promise_too = result_2.As<Promise>();
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
        Local<Promise> promise = result_1.As<Promise>();
        CHECK_EQ(promise->State(), v8::Promise::kFulfilled);
        CHECK(promise->Result()->IsUndefined());

        // Second Evaluation should return the same promise.
        Local<Promise> promise_too = result_2.As<Promise>();
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
    CHECK_EQ(nsobj->GetCreationContext().ToLocalChecked(), env.local());

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

    // TODO(cbruni) I am not sure, but this might not be supposed to throw
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
    Local<String> specifier, Local<FixedArray> import_assertions) {
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
    Local<String> specifier, Local<FixedArray> import_assertions) {
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
  i::FLAG_harmony_top_level_await = true;
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
}

TEST(ModuleEvaluationTopLevelAwaitDynamicImportError) {
  bool previous_top_level_await_flag_value = i::FLAG_harmony_top_level_await;
  i::FLAG_harmony_top_level_await = true;
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

static Local<Module> async_leaf_module;
static Local<Module> sync_leaf_module;
static Local<Module> cycle_self_module;
static Local<Module> cycle_one_module;
static Local<Module> cycle_two_module;
MaybeLocal<Module> ResolveCallbackForIsGraphAsyncTopLevelAwait(
    Local<Context> context, Local<String> specifier,
    Local<FixedArray> import_assertions, Local<Module> referrer) {
  CHECK_EQ(0, import_assertions->Length());
  if (specifier->StrictEquals(v8_str("./async_leaf.js"))) {
    return async_leaf_module;
  } else if (specifier->StrictEquals(v8_str("./sync_leaf.js"))) {
    return sync_leaf_module;
  } else if (specifier->StrictEquals(v8_str("./cycle_self.js"))) {
    return cycle_self_module;
  } else if (specifier->StrictEquals(v8_str("./cycle_one.js"))) {
    return cycle_one_module;
  } else {
    CHECK(specifier->StrictEquals(v8_str("./cycle_two.js")));
    return cycle_two_module;
  }
}

TEST(IsGraphAsyncTopLevelAwait) {
  bool previous_top_level_await_flag_value = i::FLAG_harmony_top_level_await;
  i::FLAG_harmony_top_level_await = true;

  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext env;

  {
    Local<String> source_text = v8_str("await notExecuted();");
    ScriptOrigin origin =
        ModuleOrigin(v8_str("async_leaf.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    async_leaf_module =
        ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    CHECK(async_leaf_module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackForIsGraphAsyncTopLevelAwait)
              .FromJust());
    CHECK(async_leaf_module->IsGraphAsync());
  }

  {
    Local<String> source_text = v8_str("notExecuted();");
    ScriptOrigin origin =
        ModuleOrigin(v8_str("sync_leaf.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    sync_leaf_module =
        ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    CHECK(sync_leaf_module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackForIsGraphAsyncTopLevelAwait)
              .FromJust());
    CHECK(!sync_leaf_module->IsGraphAsync());
  }

  {
    Local<String> source_text = v8_str("import './async_leaf.js'");
    ScriptOrigin origin =
        ModuleOrigin(v8_str("import_async.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    Local<Module> module =
        ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    CHECK(module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackForIsGraphAsyncTopLevelAwait)
              .FromJust());
    CHECK(module->IsGraphAsync());
  }

  {
    Local<String> source_text = v8_str("import './sync_leaf.js'");
    ScriptOrigin origin =
        ModuleOrigin(v8_str("import_sync.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    Local<Module> module =
        ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    CHECK(module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackForIsGraphAsyncTopLevelAwait)
              .FromJust());
    CHECK(!module->IsGraphAsync());
  }

  {
    Local<String> source_text = v8_str(
        "import './cycle_self.js'\n"
        "import './async_leaf.js'");
    ScriptOrigin origin =
        ModuleOrigin(v8_str("cycle_self.js"), CcTest::isolate());
    ScriptCompiler::Source source(source_text, origin);
    cycle_self_module =
        ScriptCompiler::CompileModule(isolate, &source).ToLocalChecked();
    CHECK(cycle_self_module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackForIsGraphAsyncTopLevelAwait)
              .FromJust());
    CHECK(cycle_self_module->IsGraphAsync());
  }

  {
    Local<String> source_text1 = v8_str("import './cycle_two.js'");
    ScriptOrigin origin1 =
        ModuleOrigin(v8_str("cycle_one.js"), CcTest::isolate());
    ScriptCompiler::Source source1(source_text1, origin1);
    cycle_one_module =
        ScriptCompiler::CompileModule(isolate, &source1).ToLocalChecked();
    Local<String> source_text2 = v8_str(
        "import './cycle_one.js'\n"
        "import './async_leaf.js'");
    ScriptOrigin origin2 =
        ModuleOrigin(v8_str("cycle_two.js"), CcTest::isolate());
    ScriptCompiler::Source source2(source_text2, origin2);
    cycle_two_module =
        ScriptCompiler::CompileModule(isolate, &source2).ToLocalChecked();
    CHECK(cycle_one_module
              ->InstantiateModule(env.local(),
                                  ResolveCallbackForIsGraphAsyncTopLevelAwait)
              .FromJust());
    CHECK(cycle_one_module->IsGraphAsync());
    CHECK(cycle_two_module->IsGraphAsync());
  }

  i::FLAG_harmony_top_level_await = previous_top_level_await_flag_value;
}

}  // anonymous namespace
