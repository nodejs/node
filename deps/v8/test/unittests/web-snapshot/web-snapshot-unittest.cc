// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/web-snapshot/web-snapshot.h"

#include "include/v8-function.h"
#include "src/api/api-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

class WebSnapshotTest : public TestWithContext {
 protected:
  void TestWebSnapshotExtensive(
      const char* snapshot_source, const char* test_source,
      std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester,
      uint32_t string_count, uint32_t symbol_count,
      uint32_t builtin_object_count, uint32_t map_count, uint32_t context_count,
      uint32_t function_count, uint32_t object_count, uint32_t array_count) {
    v8::Isolate* isolate = v8_isolate();

    WebSnapshotData snapshot_data;
    {
      v8::HandleScope scope(isolate);
      v8::Local<v8::Context> new_context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(new_context);

      TryRunJS(snapshot_source);
      v8::Local<v8::PrimitiveArray> exports =
          v8::PrimitiveArray::New(isolate, 1);
      v8::Local<v8::String> str =
          v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
      exports->Set(isolate, 0, str);
      WebSnapshotSerializer serializer(isolate);
      CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
      CHECK(!serializer.has_error());
      CHECK_NOT_NULL(snapshot_data.buffer);
      CHECK_EQ(string_count, serializer.string_count());
      CHECK_EQ(symbol_count, serializer.symbol_count());
      CHECK_EQ(map_count, serializer.map_count());
      CHECK_EQ(builtin_object_count, serializer.builtin_object_count());
      CHECK_EQ(context_count, serializer.context_count());
      CHECK_EQ(function_count, serializer.function_count());
      CHECK_EQ(object_count, serializer.object_count());
      CHECK_EQ(array_count, serializer.array_count());
    }

    {
      v8::HandleScope scope(isolate);
      v8::Local<v8::Context> new_context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(new_context);
      WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                           snapshot_data.buffer_size);
      CHECK(deserializer.Deserialize());
      CHECK(!deserializer.has_error());
      tester(isolate, new_context);
      CHECK_EQ(string_count, deserializer.string_count());
      CHECK_EQ(symbol_count, deserializer.symbol_count());
      CHECK_EQ(map_count, deserializer.map_count());
      CHECK_EQ(builtin_object_count, deserializer.builtin_object_count());
      CHECK_EQ(context_count, deserializer.context_count());
      CHECK_EQ(function_count, deserializer.function_count());
      CHECK_EQ(object_count, deserializer.object_count());
      CHECK_EQ(array_count, deserializer.array_count());
    }
  }

  void TestWebSnapshot(const char* snapshot_source, const char* test_source,
                       const char* expected_result, uint32_t string_count,
                       uint32_t symbol_count, uint32_t map_count,
                       uint32_t builtin_object_count, uint32_t context_count,
                       uint32_t function_count, uint32_t object_count,
                       uint32_t array_count) {
    TestWebSnapshotExtensive(
        snapshot_source, test_source,
        [this, test_source, expected_result](
            v8::Isolate* isolate, v8::Local<v8::Context> new_context) {
          v8::Local<v8::String> result = RunJS(test_source).As<v8::String>();
          CHECK(result->Equals(new_context, NewString(expected_result))
                    .FromJust());
        },
        string_count, symbol_count, map_count, builtin_object_count,
        context_count, function_count, object_count, array_count);
  }

  void VerifyFunctionKind(const v8::Local<v8::Object>& result,
                          const v8::Local<v8::Context>& context,
                          const char* property_name,
                          FunctionKind expected_kind) {
    v8::Local<v8::Function> v8_function =
        result->Get(context, NewString(property_name))
            .ToLocalChecked()
            .As<v8::Function>();
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_function));
    CHECK_EQ(function->shared().kind(), expected_kind);
  }
};

}  // namespace

TEST_F(WebSnapshotTest, Minimal) {
  const char* snapshot_source = "var foo = {'key': 'lol'};";
  const char* test_source = "foo.key";
  const char* expected_result = "lol";
  uint32_t kStringCount = 2;  // 'foo', 'Object.prototype'; 'key' is in-place.
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 1;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, EmptyObject) {
  const char* snapshot_source = "var foo = {}";
  const char* test_source = "foo";
  uint32_t kStringCount = 2;  // 'foo', 'Object.prototype'
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 1;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [this, test_source](v8::Isolate* isolate,
                          v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = RunJS(test_source).As<v8::Object>();
        Handle<JSReceiver> foo(v8::Utils::OpenHandle(*result));
        Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
        CHECK_EQ(foo->map(),
                 i_isolate->native_context()->object_function().initial_map());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kSymbolCount, kBuiltinObjectCount, kMapCount,
                           kContextCount, kFunctionCount, kObjectCount,
                           kArrayCount);
}

TEST_F(WebSnapshotTest, Numbers) {
  const char* snapshot_source =
      "var foo = {'a': 6,\n"
      "           'b': -11,\n"
      "           'c': 11.6,\n"
      "           'd': NaN,\n"
      "           'e': Number.POSITIVE_INFINITY,\n"
      "           'f': Number.NEGATIVE_INFINITY,\n"
      "}";
  const char* test_source = "foo";
  uint32_t kStringCount =
      2;  // 'foo', 'Object.prototype'; 'a'...'f' are in-place.
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 1;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;

  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [this, test_source](v8::Isolate* isolate,
                          v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = RunJS(test_source).As<v8::Object>();
        int32_t a = result->Get(new_context, NewString("a"))
                        .ToLocalChecked()
                        .As<v8::Number>()
                        ->Value();
        CHECK_EQ(a, 6);
        int32_t b = result->Get(new_context, NewString("b"))
                        .ToLocalChecked()
                        .As<v8::Number>()
                        ->Value();
        CHECK_EQ(b, -11);
        double c = result->Get(new_context, NewString("c"))
                       .ToLocalChecked()
                       .As<v8::Number>()
                       ->Value();
        CHECK_EQ(c, 11.6);
        double d = result->Get(new_context, NewString("d"))
                       .ToLocalChecked()
                       .As<v8::Number>()
                       ->Value();
        CHECK(std::isnan(d));
        double e = result->Get(new_context, NewString("e"))
                       .ToLocalChecked()
                       .As<v8::Number>()
                       ->Value();
        CHECK_EQ(e, std::numeric_limits<double>::infinity());
        double f = result->Get(new_context, NewString("f"))
                       .ToLocalChecked()
                       .As<v8::Number>()
                       ->Value();
        CHECK_EQ(f, -std::numeric_limits<double>::infinity());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kSymbolCount, kBuiltinObjectCount, kMapCount,
                           kContextCount, kFunctionCount, kObjectCount,
                           kArrayCount);
}

TEST_F(WebSnapshotTest, Oddballs) {
  const char* snapshot_source =
      "var foo = {'a': false,\n"
      "           'b': true,\n"
      "           'c': null,\n"
      "           'd': undefined,\n"
      "}";
  const char* test_source = "foo";
  // 'foo', 'Object.prototype'; 'a'...'d' are in-place.
  uint32_t kStringCount = 2;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 1;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [this, test_source](v8::Isolate* isolate,
                          v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = RunJS(test_source).As<v8::Object>();
        Local<Value> a =
            result->Get(new_context, NewString("a")).ToLocalChecked();
        CHECK(a->IsFalse());
        Local<Value> b =
            result->Get(new_context, NewString("b")).ToLocalChecked();
        CHECK(b->IsTrue());
        Local<Value> c =
            result->Get(new_context, NewString("c")).ToLocalChecked();
        CHECK(c->IsNull());
        Local<Value> d =
            result->Get(new_context, NewString("d")).ToLocalChecked();
        CHECK(d->IsUndefined());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kSymbolCount, kBuiltinObjectCount, kMapCount,
                           kContextCount, kFunctionCount, kObjectCount,
                           kArrayCount);
}

TEST_F(WebSnapshotTest, Function) {
  const char* snapshot_source =
      "var foo = {'key': function() { return '11525'; }};";
  const char* test_source = "foo.key()";
  const char* expected_result = "11525";
  // 'foo', 'Object.prototype', 'Function.prototype', function source code.
  // 'key' is in-place.
  uint32_t kStringCount = 4;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 2;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 1;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, InnerFunctionWithContext) {
  const char* snapshot_source =
      "var foo = {'key': (function() {\n"
      "                     let result = '11525';\n"
      "                     function inner() { return result; }\n"
      "                     return inner;\n"
      "                   })()};";
  const char* test_source = "foo.key()";
  const char* expected_result = "11525";
  // Strings: 'foo', 'result', 'Object.prototype', 'Function.prototype'.
  // function source code (inner). 'key' is in-place.
  uint32_t kStringCount = 5;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 2;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 1;
  uint32_t kFunctionCount = 1;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, InnerFunctionWithContextAndParentContext) {
  const char* snapshot_source =
      "var foo = {'key': (function() {\n"
      "                     let part1 = '11';\n"
      "                     function inner() {\n"
      "                       let part2 = '525';\n"
      "                       function innerinner() {\n"
      "                         return part1 + part2;\n"
      "                       }\n"
      "                       return innerinner;\n"
      "                     }\n"
      "                     return inner();\n"
      "                   })()};";
  const char* test_source = "foo.key()";
  const char* expected_result = "11525";
  // Strings: 'foo', 'Object.prototype', 'Function.prototype', function source
  // code (innerinner), 'part1', 'part2'.
  uint32_t kStringCount = 6;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 2;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 2;
  uint32_t kFunctionCount = 1;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, RegExp) {
  const char* snapshot_source = "var foo = {'re': /ab+c/gi}";
  const char* test_source = "foo";
  // 'foo', 'Object.prototype', RegExp pattern, RegExp flags
  uint32_t kStringCount = 4;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 1;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [this, test_source](v8::Isolate* isolate,
                          v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = RunJS(test_source).As<v8::Object>();
        Local<v8::RegExp> re = result->Get(new_context, NewString("re"))
                                   .ToLocalChecked()
                                   .As<v8::RegExp>();
        CHECK(re->IsRegExp());
        CHECK(
            re->GetSource()->Equals(new_context, NewString("ab+c")).FromJust());
        CHECK_EQ(v8::RegExp::kGlobal | v8::RegExp::kIgnoreCase, re->GetFlags());
        v8::Local<v8::Object> match =
            re->Exec(new_context, NewString("aBc")).ToLocalChecked();
        CHECK(match->IsArray());
        v8::Local<v8::Object> no_match =
            re->Exec(new_context, NewString("ac")).ToLocalChecked();
        CHECK(no_match->IsNull());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kSymbolCount, kBuiltinObjectCount, kMapCount,
                           kContextCount, kFunctionCount, kObjectCount,
                           kArrayCount);
}

TEST_F(WebSnapshotTest, RegExpNoFlags) {
  const char* snapshot_source = "var foo = {'re': /ab+c/}";
  const char* test_source = "foo";
  // 'foo', , 'Object.prototype RegExp pattern, RegExp flags
  uint32_t kStringCount = 4;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 1;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [this, test_source](v8::Isolate* isolate,
                          v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = RunJS(test_source).As<v8::Object>();
        Local<v8::RegExp> re = result->Get(new_context, NewString("re"))
                                   .ToLocalChecked()
                                   .As<v8::RegExp>();
        CHECK(re->IsRegExp());
        CHECK(
            re->GetSource()->Equals(new_context, NewString("ab+c")).FromJust());
        CHECK_EQ(v8::RegExp::kNone, re->GetFlags());
        v8::Local<v8::Object> match =
            re->Exec(new_context, NewString("abc")).ToLocalChecked();
        CHECK(match->IsArray());
        v8::Local<v8::Object> no_match =
            re->Exec(new_context, NewString("ac")).ToLocalChecked();
        CHECK(no_match->IsNull());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kSymbolCount, kBuiltinObjectCount, kMapCount,
                           kContextCount, kFunctionCount, kObjectCount,
                           kArrayCount);
}

TEST_F(WebSnapshotTest, SFIDeduplication) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);

  WebSnapshotData snapshot_data;
  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    const char* snapshot_source =
        "let foo = {};\n"
        "foo.outer = function(a) {\n"
        "  return function() {\n"
        "   return a;\n"
        "  }\n"
        "}\n"
        "foo.inner = foo.outer('hi');";

    TryRunJS(snapshot_source);
    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 1);
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
    exports->Set(isolate, 0, str);
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                         snapshot_data.buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());

    const char* get_inner = "foo.inner";
    const char* create_new_inner = "foo.outer()";

    // Verify that foo.inner and the JSFunction which is the result of calling
    // foo.outer() after deserialization share the SFI.
    v8::Local<v8::Function> v8_inner1 = RunJS(get_inner).As<v8::Function>();
    v8::Local<v8::Function> v8_inner2 =
        RunJS(create_new_inner).As<v8::Function>();

    Handle<JSFunction> inner1 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner1));
    Handle<JSFunction> inner2 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner2));

    CHECK_EQ(inner1->shared(), inner2->shared());
  }
}

TEST_F(WebSnapshotTest, SFIDeduplicationClasses) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);

  WebSnapshotData snapshot_data;
  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    const char* snapshot_source =
        "let foo = {};\n"
        "foo.create = function(a) {\n"
        "  return class {\n"
        "   constructor(x) {this.x = x;};\n"
        "  }\n"
        "}\n"
        "foo.class = foo.create('hi');";

    TryRunJS(snapshot_source);
    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 1);
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
    exports->Set(isolate, 0, str);
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                         snapshot_data.buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());

    const char* get_class = "foo.class";
    const char* create_new_class = "foo.create()";

    // Verify that foo.inner and the JSFunction which is the result of calling
    // foo.outer() after deserialization share the SFI.
    v8::Local<v8::Function> v8_class1 = RunJS(get_class).As<v8::Function>();
    v8::Local<v8::Function> v8_class2 =
        RunJS(create_new_class).As<v8::Function>();

    Handle<JSFunction> class1 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_class1));
    Handle<JSFunction> class2 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_class2));

    CHECK_EQ(class1->shared(), class2->shared());
  }
}

TEST_F(WebSnapshotTest, SFIDeduplicationAfterBytecodeFlushing) {
  v8_flags.stress_flush_code = true;
  v8_flags.flush_bytecode = true;
  v8::Isolate* isolate = v8_isolate();

  WebSnapshotData snapshot_data;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);

    const char* snapshot_source =
        "let foo = {};\n"
        "foo.outer = function() {\n"
        "  let a = 'hello';\n"
        "  return function() {\n"
        "   return a;\n"
        "  }\n"
        "}\n"
        "foo.inner = foo.outer();";

    TryRunJS(snapshot_source);

    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 1);
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
    exports->Set(isolate, 0, str);
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  CollectAllGarbage();
  CollectAllGarbage();

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                         snapshot_data.buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());

    const char* get_outer = "foo.outer";
    const char* get_inner = "foo.inner";
    const char* create_new_inner = "foo.outer()";

    v8::Local<v8::Function> v8_outer = RunJS(get_outer).As<v8::Function>();
    Handle<JSFunction> outer =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_outer));
    CHECK(!outer->shared().is_compiled());

    v8::Local<v8::Function> v8_inner1 = RunJS(get_inner).As<v8::Function>();
    v8::Local<v8::Function> v8_inner2 =
        RunJS(create_new_inner).As<v8::Function>();

    Handle<JSFunction> inner1 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner1));
    Handle<JSFunction> inner2 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner2));

    CHECK(outer->shared().is_compiled());
    CHECK_EQ(inner1->shared(), inner2->shared());

    // Force bytecode flushing of "foo.outer".
    CollectAllGarbage();
    CollectAllGarbage();

    CHECK(!outer->shared().is_compiled());

    // Create another inner function.
    v8::Local<v8::Function> v8_inner3 =
        RunJS(create_new_inner).As<v8::Function>();
    Handle<JSFunction> inner3 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner3));

    // Check that it shares the SFI with the original inner function which is in
    // the snapshot.
    CHECK_EQ(inner1->shared(), inner3->shared());
  }
}

TEST_F(WebSnapshotTest, SFIDeduplicationAfterBytecodeFlushingClasses) {
  v8_flags.stress_flush_code = true;
  v8_flags.flush_bytecode = true;
  v8::Isolate* isolate = v8_isolate();

  WebSnapshotData snapshot_data;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);

    const char* snapshot_source =
        "let foo = {};\n"
        "foo.create = function(a) {\n"
        "  return class {\n"
        "   constructor(x) {this.x = x;};\n"
        "  }\n"
        "}\n"
        "foo.class = foo.create('hi');";

    TryRunJS(snapshot_source);

    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 1);
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
    exports->Set(isolate, 0, str);
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  CollectAllGarbage();
  CollectAllGarbage();

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                         snapshot_data.buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());

    const char* get_create = "foo.create";
    const char* get_class = "foo.class";
    const char* create_new_class = "foo.create()";

    v8::Local<v8::Function> v8_create = RunJS(get_create).As<v8::Function>();
    Handle<JSFunction> create =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_create));
    CHECK(!create->shared().is_compiled());

    v8::Local<v8::Function> v8_class1 = RunJS(get_class).As<v8::Function>();
    v8::Local<v8::Function> v8_class2 =
        RunJS(create_new_class).As<v8::Function>();

    Handle<JSFunction> class1 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_class1));
    Handle<JSFunction> class2 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_class2));

    CHECK(create->shared().is_compiled());
    CHECK_EQ(class1->shared(), class2->shared());

    // Force bytecode flushing of "foo.outer".
    CollectAllGarbage();
    CollectAllGarbage();

    CHECK(!create->shared().is_compiled());

    // Create another inner function.
    v8::Local<v8::Function> v8_class3 =
        RunJS(create_new_class).As<v8::Function>();
    Handle<JSFunction> class3 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_class3));

    // Check that it shares the SFI with the original inner function which is in
    // the snapshot.
    CHECK_EQ(class1->shared(), class3->shared());
  }
}

TEST_F(WebSnapshotTest, SFIDeduplicationOfFunctionsNotInSnapshot) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);

  WebSnapshotData snapshot_data;
  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    const char* snapshot_source =
        "let foo = {};\n"
        "foo.outer = function(a) {\n"
        "  return function() {\n"
        "   return a;\n"
        "  }\n"
        "}\n";

    TryRunJS(snapshot_source);
    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 1);
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
    exports->Set(isolate, 0, str);
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                         snapshot_data.buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());

    const char* create_new_inner = "foo.outer()";

    // Verify that repeated invocations of foo.outer() return functions which
    // share the SFI.
    v8::Local<v8::Function> v8_inner1 =
        RunJS(create_new_inner).As<v8::Function>();
    v8::Local<v8::Function> v8_inner2 =
        RunJS(create_new_inner).As<v8::Function>();

    Handle<JSFunction> inner1 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner1));
    Handle<JSFunction> inner2 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner2));

    CHECK_EQ(inner1->shared(), inner2->shared());
  }
}

TEST_F(WebSnapshotTest, FunctionKinds) {
  const char* snapshot_source =
      "var foo = {a: function() {},\n"
      "           b: () => {},\n"
      "           c: async function() {},\n"
      "           d: async () => {},\n"
      "           e: function*() {},\n"
      "           f: async function*() {}\n"
      "}";
  const char* test_source = "foo";
  // 'foo', 'Object.prototype', 'Function.prototype', 'AsyncFunction.prototype',
  // 'AsyncGeneratorFunction.prototype", "GeneratorFunction.prototype", source
  // code. 'a'...'f' in-place.
  uint32_t kStringCount = 7;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 5;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 6;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [this, test_source](v8::Isolate* isolate,
                          v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = RunJS(test_source).As<v8::Object>();
        // Verify all FunctionKinds.
        VerifyFunctionKind(result, new_context, "a",
                           FunctionKind::kNormalFunction);
        VerifyFunctionKind(result, new_context, "b",
                           FunctionKind::kArrowFunction);
        VerifyFunctionKind(result, new_context, "c",
                           FunctionKind::kAsyncFunction);
        VerifyFunctionKind(result, new_context, "d",
                           FunctionKind::kAsyncArrowFunction);
        VerifyFunctionKind(result, new_context, "e",
                           FunctionKind::kGeneratorFunction);
        VerifyFunctionKind(result, new_context, "f",
                           FunctionKind::kAsyncGeneratorFunction);
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kSymbolCount, kBuiltinObjectCount, kMapCount,
                           kContextCount, kFunctionCount, kObjectCount,
                           kArrayCount);
}

// Test that concatenating JS code to the snapshot works.
TEST_F(WebSnapshotTest, Concatenation) {
  v8::Isolate* isolate = v8_isolate();

  const char* snapshot_source = "var foo = {a: 1};\n";
  const char* source_to_append = "var bar = {a: 10};";
  const char* test_source = "foo.a + bar.a";
  uint32_t kObjectCount = 1;

  WebSnapshotData snapshot_data;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);

    TryRunJS(snapshot_source);
    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 1);
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
    exports->Set(isolate, 0, str);
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
    CHECK_EQ(kObjectCount, serializer.object_count());
  }

  auto buffer_size = snapshot_data.buffer_size + strlen(source_to_append);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memcpy(buffer.get(), snapshot_data.buffer, snapshot_data.buffer_size);
  memcpy(buffer.get() + snapshot_data.buffer_size, source_to_append,
         strlen(source_to_append));

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, buffer.get(), buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());
    CHECK_EQ(kObjectCount, deserializer.object_count());

    v8::Local<v8::Number> result = RunJS(test_source).As<v8::Number>();
    CHECK_EQ(11, result->Value());
  }
}

// Test that errors from invalid concatenated code are handled correctly.
TEST_F(WebSnapshotTest, ConcatenationErrors) {
  v8::Isolate* isolate = v8_isolate();

  const char* snapshot_source = "var foo = {a: 1};\n";
  const char* source_to_append = "wontparse+[)";
  uint32_t kObjectCount = 1;

  WebSnapshotData snapshot_data;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);

    TryRunJS(snapshot_source);
    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 1);
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked();
    exports->Set(isolate, 0, str);
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
    CHECK_EQ(kObjectCount, serializer.object_count());
  }

  auto buffer_size = snapshot_data.buffer_size + strlen(source_to_append);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memcpy(buffer.get(), snapshot_data.buffer, snapshot_data.buffer_size);
  memcpy(buffer.get() + snapshot_data.buffer_size, source_to_append,
         strlen(source_to_append));

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, buffer.get(), buffer_size);
    CHECK(!deserializer.Deserialize());
  }
}

TEST_F(WebSnapshotTest, CompactedSourceCode) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);

  WebSnapshotData snapshot_data;
  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    const char* snapshot_source =
        "function foo() { 'foo' }\n"
        "function bar() { 'bar' }\n"
        "function baz() { 'baz' }\n"
        "let e = [foo, bar, baz]";
    TryRunJS(snapshot_source);
    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 1);
    v8::Local<v8::String> str =
        v8::String::NewFromUtf8(isolate, "e").ToLocalChecked();
    exports->Set(isolate, 0, str);
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                         snapshot_data.buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());

    const char* get_function = "e[0]";

    // Verify that the source code got compacted.
    v8::Local<v8::Function> v8_function =
        RunJS(get_function).As<v8::Function>();
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_function));
    Handle<String> function_script_source =
        handle(String::cast(Script::cast(function->shared().script()).source()),
               i_isolate());
    const char* raw_expected_source = "() { 'foo' }() { 'bar' }() { 'baz' }";

    Handle<String> expected_source = Utils::OpenHandle(
        *v8::String::NewFromUtf8(isolate, raw_expected_source).ToLocalChecked(),
        i_isolate());
    CHECK(function_script_source->Equals(*expected_source));
  }
}

TEST_F(WebSnapshotTest, InPlaceStringsInArrays) {
  const char* snapshot_source = "var foo = ['one', 'two', 'three'];";
  const char* test_source = "foo.join('');";
  const char* expected_result = "onetwothree";
  uint32_t kStringCount = 1;  // 'foo'; Other strings are in-place.
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 0;
  uint32_t kMapCount = 0;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 0;
  uint32_t kArrayCount = 1;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, RepeatedInPlaceStringsInArrays) {
  const char* snapshot_source = "var foo = ['one', 'two', 'one'];";
  const char* test_source = "foo.join('');";
  const char* expected_result = "onetwoone";
  uint32_t kStringCount = 2;  // 'foo', 'one'; Other strings are in-place.
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 0;
  uint32_t kMapCount = 0;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 0;
  uint32_t kArrayCount = 1;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, InPlaceStringsInObjects) {
  const char* snapshot_source = "var foo =  {a: 'one', b: 'two', c: 'three'};";
  const char* test_source = "foo.a + foo.b + foo.c;";
  const char* expected_result = "onetwothree";
  // 'foo', 'Object.prototype'. Other strings are in-place.
  uint32_t kStringCount = 2;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 1;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, RepeatedInPlaceStringsInObjects) {
  const char* snapshot_source = "var foo =  {a: 'one', b: 'two', c: 'one'};";
  const char* test_source = "foo.a + foo.b + foo.c;";
  const char* expected_result = "onetwoone";
  // 'foo', 'one', 'Object.prototype'. Other strings are in-place.
  uint32_t kStringCount = 3;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 1;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, BuiltinObjects) {
  const char* snapshot_source = "var foo = {a: Error.prototype};";
  const char* test_source = "foo.a == Error.prototype ? \"pass\" : \"fail\"";
  const char* expected_result = "pass";
  // 'foo', 'Error.prototype', 'Object.prototype'. Other strings are in-place.
  uint32_t kStringCount = 3;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 2;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, BuiltinObjectsDeduplicated) {
  const char* snapshot_source =
      "var foo = {a: Error.prototype, b: Error.prototype}";
  const char* test_source = "foo.a === Error.prototype ? \"pass\" : \"fail\"";
  const char* expected_result = "pass";
  // 'foo', 'Error.prototype', 'Object.prototype'. Other strings are in-place.
  uint32_t kStringCount = 3;
  uint32_t kSymbolCount = 0;
  uint32_t kBuiltinObjectCount = 2;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  uint32_t kArrayCount = 0;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kSymbolCount, kBuiltinObjectCount, kMapCount, kContextCount,
                  kFunctionCount, kObjectCount, kArrayCount);
}

TEST_F(WebSnapshotTest, ConstructorFunctionKinds) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);

  WebSnapshotData snapshot_data;
  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    const char* snapshot_source =
        "class Base { constructor() {} };\n"
        "class Derived extends Base { constructor() {} };\n"
        "class BaseDefault {};\n"
        "class DerivedDefault extends BaseDefault {};\n";

    TryRunJS(snapshot_source);
    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 4);
    exports->Set(isolate, 0,
                 v8::String::NewFromUtf8(isolate, "Base").ToLocalChecked());
    exports->Set(isolate, 1,
                 v8::String::NewFromUtf8(isolate, "Derived").ToLocalChecked());
    exports->Set(
        isolate, 2,
        v8::String::NewFromUtf8(isolate, "BaseDefault").ToLocalChecked());
    exports->Set(
        isolate, 3,
        v8::String::NewFromUtf8(isolate, "DerivedDefault").ToLocalChecked());
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                         snapshot_data.buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());

    v8::Local<v8::Function> v8_base = RunJS("Base").As<v8::Function>();
    Handle<JSFunction> base =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_base));
    CHECK_EQ(FunctionKind::kBaseConstructor, base->shared().kind());

    v8::Local<v8::Function> v8_derived = RunJS("Derived").As<v8::Function>();
    Handle<JSFunction> derived =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_derived));
    CHECK_EQ(FunctionKind::kDerivedConstructor, derived->shared().kind());

    v8::Local<v8::Function> v8_base_default =
        RunJS("BaseDefault").As<v8::Function>();
    Handle<JSFunction> base_default =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_base_default));
    CHECK_EQ(FunctionKind::kDefaultBaseConstructor,
             base_default->shared().kind());

    v8::Local<v8::Function> v8_derived_default =
        RunJS("DerivedDefault").As<v8::Function>();
    Handle<JSFunction> derived_default =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_derived_default));
    CHECK_EQ(FunctionKind::kDefaultDerivedConstructor,
             derived_default->shared().kind());
  }
}

TEST_F(WebSnapshotTest, SlackElementsInObjects) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);

  WebSnapshotData snapshot_data;
  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    const char* snapshot_source =
        "var foo = {};"
        "for (let i = 0; i < 100; ++i) {"
        "  foo[i] = i;"
        "}"
        "var bar = {};"
        "for (let i = 0; i < 100; ++i) {"
        "  bar[i] = {};"
        "}";

    RunJS(snapshot_source);
    v8::Local<v8::PrimitiveArray> exports = v8::PrimitiveArray::New(isolate, 2);
    exports->Set(isolate, 0,
                 v8::String::NewFromUtf8(isolate, "foo").ToLocalChecked());
    exports->Set(isolate, 1,
                 v8::String::NewFromUtf8(isolate, "bar").ToLocalChecked());
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  {
    v8::Local<v8::Context> new_context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate, snapshot_data.buffer,
                                         snapshot_data.buffer_size);
    CHECK(deserializer.Deserialize());
    CHECK(!deserializer.has_error());

    Handle<JSObject> foo =
        Handle<JSObject>::cast(Utils::OpenHandle<v8::Object, JSReceiver>(
            RunJS("foo").As<v8::Object>()));
    CHECK_EQ(100, foo->elements().length());
    CHECK_EQ(HOLEY_ELEMENTS, foo->GetElementsKind());

    Handle<JSObject> bar =
        Handle<JSObject>::cast(Utils::OpenHandle<v8::Object, JSReceiver>(
            RunJS("bar").As<v8::Object>()));
    CHECK_EQ(100, bar->elements().length());
    CHECK_EQ(HOLEY_ELEMENTS, bar->GetElementsKind());
  }
}

}  // namespace internal
}  // namespace v8
