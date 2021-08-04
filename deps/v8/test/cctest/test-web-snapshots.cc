// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/web-snapshot/web-snapshot.h"
#include "test/cctest/cctest-utils.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

namespace {

void TestWebSnapshotExtensive(
    const char* snapshot_source, const char* test_source,
    std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester,
    uint32_t string_count, uint32_t map_count, uint32_t context_count,
    uint32_t function_count, uint32_t object_count) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();

  WebSnapshotData snapshot_data;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = CcTest::NewContext();
    v8::Context::Scope context_scope(new_context);

    CompileRun(snapshot_source);
    std::vector<std::string> exports;
    exports.push_back("foo");
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
    CHECK_EQ(string_count, serializer.string_count());
    CHECK_EQ(map_count, serializer.map_count());
    CHECK_EQ(context_count, serializer.context_count());
    CHECK_EQ(function_count, serializer.function_count());
    CHECK_EQ(object_count, serializer.object_count());
  }

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = CcTest::NewContext();
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate);
    CHECK(deserializer.UseWebSnapshot(snapshot_data.buffer,
                                      snapshot_data.buffer_size));
    CHECK(!deserializer.has_error());
    tester(isolate, new_context);
    CHECK_EQ(string_count, deserializer.string_count());
    CHECK_EQ(map_count, deserializer.map_count());
    CHECK_EQ(context_count, deserializer.context_count());
    CHECK_EQ(function_count, deserializer.function_count());
    CHECK_EQ(object_count, deserializer.object_count());
  }
}

void TestWebSnapshot(const char* snapshot_source, const char* test_source,
                     const char* expected_result, uint32_t string_count,
                     uint32_t map_count, uint32_t context_count,
                     uint32_t function_count, uint32_t object_count) {
  TestWebSnapshotExtensive(
      snapshot_source, test_source,
      [test_source, expected_result](v8::Isolate* isolate,
                                     v8::Local<v8::Context> new_context) {
        v8::Local<v8::String> result = CompileRun(test_source).As<v8::String>();
        CHECK(result->Equals(new_context, v8_str(expected_result)).FromJust());
      },
      string_count, map_count, context_count, function_count, object_count);
}

}  // namespace

TEST(Minimal) {
  const char* snapshot_source = "var foo = {'key': 'lol'};";
  const char* test_source = "foo.key";
  const char* expected_result = "lol";
  uint32_t kStringCount = 3;  // 'foo', 'key', 'lol'
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kMapCount, kContextCount, kFunctionCount, kObjectCount);
}

TEST(Numbers) {
  const char* snapshot_source =
      "var foo = {'a': 6,\n"
      "           'b': -11,\n"
      "           'c': 11.6,\n"
      "           'd': NaN,\n"
      "           'e': Number.POSITIVE_INFINITY,\n"
      "           'f': Number.NEGATIVE_INFINITY,\n"
      "}";
  const char* test_source = "foo";
  uint32_t kStringCount = 7;  // 'foo', 'a', ..., 'f'
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [test_source](v8::Isolate* isolate, v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = CompileRun(test_source).As<v8::Object>();
        int32_t a = result->Get(new_context, v8_str("a"))
                        .ToLocalChecked()
                        .As<v8::Number>()
                        ->Value();
        CHECK_EQ(a, 6);
        int32_t b = result->Get(new_context, v8_str("b"))
                        .ToLocalChecked()
                        .As<v8::Number>()
                        ->Value();
        CHECK_EQ(b, -11);
        double c = result->Get(new_context, v8_str("c"))
                       .ToLocalChecked()
                       .As<v8::Number>()
                       ->Value();
        CHECK_EQ(c, 11.6);
        double d = result->Get(new_context, v8_str("d"))
                       .ToLocalChecked()
                       .As<v8::Number>()
                       ->Value();
        CHECK(std::isnan(d));
        double e = result->Get(new_context, v8_str("e"))
                       .ToLocalChecked()
                       .As<v8::Number>()
                       ->Value();
        CHECK_EQ(e, std::numeric_limits<double>::infinity());
        double f = result->Get(new_context, v8_str("f"))
                       .ToLocalChecked()
                       .As<v8::Number>()
                       ->Value();
        CHECK_EQ(f, -std::numeric_limits<double>::infinity());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kMapCount, kContextCount, kFunctionCount,
                           kObjectCount);
}

TEST(Oddballs) {
  const char* snapshot_source =
      "var foo = {'a': false,\n"
      "           'b': true,\n"
      "           'c': null,\n"
      "           'd': undefined,\n"
      "}";
  const char* test_source = "foo";
  uint32_t kStringCount = 5;  // 'foo', 'a', ..., 'd'
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [test_source](v8::Isolate* isolate, v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = CompileRun(test_source).As<v8::Object>();
        Local<Value> a = result->Get(new_context, v8_str("a")).ToLocalChecked();
        CHECK(a->IsFalse());
        Local<Value> b = result->Get(new_context, v8_str("b")).ToLocalChecked();
        CHECK(b->IsTrue());
        Local<Value> c = result->Get(new_context, v8_str("c")).ToLocalChecked();
        CHECK(c->IsNull());
        Local<Value> d = result->Get(new_context, v8_str("d")).ToLocalChecked();
        CHECK(d->IsUndefined());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kMapCount, kContextCount, kFunctionCount,
                           kObjectCount);
}

TEST(Function) {
  const char* snapshot_source =
      "var foo = {'key': function() { return '11525'; }};";
  const char* test_source = "foo.key()";
  const char* expected_result = "11525";
  uint32_t kStringCount = 3;  // 'foo', 'key', function source code
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 1;
  uint32_t kObjectCount = 1;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kMapCount, kContextCount, kFunctionCount, kObjectCount);
}

TEST(InnerFunctionWithContext) {
  const char* snapshot_source =
      "var foo = {'key': (function() {\n"
      "                     let result = '11525';\n"
      "                     function inner() { return result; }\n"
      "                     return inner;\n"
      "                   })()};";
  const char* test_source = "foo.key()";
  const char* expected_result = "11525";
  // Strings: 'foo', 'key', function source code (inner), 'result', '11525'
  uint32_t kStringCount = 5;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 1;
  uint32_t kFunctionCount = 1;
  uint32_t kObjectCount = 1;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kMapCount, kContextCount, kFunctionCount, kObjectCount);
}

TEST(InnerFunctionWithContextAndParentContext) {
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
  // Strings: 'foo', 'key', function source code (innerinner), 'part1', 'part2',
  // '11', '525'
  uint32_t kStringCount = 7;
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 2;
  uint32_t kFunctionCount = 1;
  uint32_t kObjectCount = 1;
  TestWebSnapshot(snapshot_source, test_source, expected_result, kStringCount,
                  kMapCount, kContextCount, kFunctionCount, kObjectCount);
}

TEST(RegExp) {
  const char* snapshot_source = "var foo = {'re': /ab+c/gi}";
  const char* test_source = "foo";
  uint32_t kStringCount = 4;  // 'foo', 're', RegExp pattern, RegExp flags
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [test_source](v8::Isolate* isolate, v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = CompileRun(test_source).As<v8::Object>();
        Local<v8::RegExp> re = result->Get(new_context, v8_str("re"))
                                   .ToLocalChecked()
                                   .As<v8::RegExp>();
        CHECK(re->IsRegExp());
        CHECK(re->GetSource()->Equals(new_context, v8_str("ab+c")).FromJust());
        CHECK_EQ(v8::RegExp::kGlobal | v8::RegExp::kIgnoreCase, re->GetFlags());
        v8::Local<v8::Object> match =
            re->Exec(new_context, v8_str("aBc")).ToLocalChecked();
        CHECK(match->IsArray());
        v8::Local<v8::Object> no_match =
            re->Exec(new_context, v8_str("ac")).ToLocalChecked();
        CHECK(no_match->IsNull());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kMapCount, kContextCount, kFunctionCount,
                           kObjectCount);
}

TEST(RegExpNoFlags) {
  const char* snapshot_source = "var foo = {'re': /ab+c/}";
  const char* test_source = "foo";
  uint32_t kStringCount = 4;  // 'foo', 're', RegExp pattern, RegExp flags
  uint32_t kMapCount = 1;
  uint32_t kContextCount = 0;
  uint32_t kFunctionCount = 0;
  uint32_t kObjectCount = 1;
  std::function<void(v8::Isolate*, v8::Local<v8::Context>)> tester =
      [test_source](v8::Isolate* isolate, v8::Local<v8::Context> new_context) {
        v8::Local<v8::Object> result = CompileRun(test_source).As<v8::Object>();
        Local<v8::RegExp> re = result->Get(new_context, v8_str("re"))
                                   .ToLocalChecked()
                                   .As<v8::RegExp>();
        CHECK(re->IsRegExp());
        CHECK(re->GetSource()->Equals(new_context, v8_str("ab+c")).FromJust());
        CHECK_EQ(v8::RegExp::kNone, re->GetFlags());
        v8::Local<v8::Object> match =
            re->Exec(new_context, v8_str("abc")).ToLocalChecked();
        CHECK(match->IsArray());
        v8::Local<v8::Object> no_match =
            re->Exec(new_context, v8_str("ac")).ToLocalChecked();
        CHECK(no_match->IsNull());
      };
  TestWebSnapshotExtensive(snapshot_source, test_source, tester, kStringCount,
                           kMapCount, kContextCount, kFunctionCount,
                           kObjectCount);
}

TEST(SFIDeduplication) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  WebSnapshotData snapshot_data;
  {
    v8::Local<v8::Context> new_context = CcTest::NewContext();
    v8::Context::Scope context_scope(new_context);
    const char* snapshot_source =
        "let foo = {};\n"
        "foo.outer = function(a) {\n"
        "  return function() {\n"
        "   return a;\n"
        "  }\n"
        "}\n"
        "foo.inner = foo.outer('hi');";

    CompileRun(snapshot_source);
    std::vector<std::string> exports;
    exports.push_back("foo");
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  {
    v8::Local<v8::Context> new_context = CcTest::NewContext();
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate);
    CHECK(deserializer.UseWebSnapshot(snapshot_data.buffer,
                                      snapshot_data.buffer_size));
    CHECK(!deserializer.has_error());

    const char* get_inner = "foo.inner";
    const char* create_new_inner = "foo.outer()";

    // Verify that foo.inner and the JSFunction which is the result of calling
    // foo.outer() after deserialization share the SFI.
    v8::Local<v8::Function> v8_inner1 =
        CompileRun(get_inner).As<v8::Function>();
    v8::Local<v8::Function> v8_inner2 =
        CompileRun(create_new_inner).As<v8::Function>();

    Handle<JSFunction> inner1 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner1));
    Handle<JSFunction> inner2 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner2));

    CHECK_EQ(inner1->shared(), inner2->shared());
  }
}

TEST(SFIDeduplicationAfterBytecodeFlushing) {
  FLAG_stress_flush_bytecode = true;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();

  WebSnapshotData snapshot_data;
  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = CcTest::NewContext();
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

    CompileRun(snapshot_source);

    std::vector<std::string> exports;
    exports.push_back("foo");
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();

  {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> new_context = CcTest::NewContext();
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate);
    CHECK(deserializer.UseWebSnapshot(snapshot_data.buffer,
                                      snapshot_data.buffer_size));
    CHECK(!deserializer.has_error());

    const char* get_outer = "foo.outer";
    const char* get_inner = "foo.inner";
    const char* create_new_inner = "foo.outer()";

    v8::Local<v8::Function> v8_outer = CompileRun(get_outer).As<v8::Function>();
    Handle<JSFunction> outer =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_outer));
    CHECK(!outer->shared().is_compiled());

    v8::Local<v8::Function> v8_inner1 =
        CompileRun(get_inner).As<v8::Function>();
    v8::Local<v8::Function> v8_inner2 =
        CompileRun(create_new_inner).As<v8::Function>();

    Handle<JSFunction> inner1 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner1));
    Handle<JSFunction> inner2 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner2));

    CHECK(outer->shared().is_compiled());
    CHECK_EQ(inner1->shared(), inner2->shared());

    // Force bytecode flushing of "foo.outer".
    CcTest::CollectAllGarbage();
    CcTest::CollectAllGarbage();

    CHECK(!outer->shared().is_compiled());

    // Create another inner function.
    v8::Local<v8::Function> v8_inner3 =
        CompileRun(create_new_inner).As<v8::Function>();
    Handle<JSFunction> inner3 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner3));

    // Check that it shares the SFI with the original inner function which is in
    // the snapshot.
    CHECK_EQ(inner1->shared(), inner3->shared());
  }
}

TEST(SFIDeduplicationOfFunctionsNotInSnapshot) {
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  WebSnapshotData snapshot_data;
  {
    v8::Local<v8::Context> new_context = CcTest::NewContext();
    v8::Context::Scope context_scope(new_context);
    const char* snapshot_source =
        "let foo = {};\n"
        "foo.outer = function(a) {\n"
        "  return function() {\n"
        "   return a;\n"
        "  }\n"
        "}\n";

    CompileRun(snapshot_source);
    std::vector<std::string> exports;
    exports.push_back("foo");
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(new_context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
  }

  {
    v8::Local<v8::Context> new_context = CcTest::NewContext();
    v8::Context::Scope context_scope(new_context);
    WebSnapshotDeserializer deserializer(isolate);
    CHECK(deserializer.UseWebSnapshot(snapshot_data.buffer,
                                      snapshot_data.buffer_size));
    CHECK(!deserializer.has_error());

    const char* create_new_inner = "foo.outer()";

    // Verify that repeated invocations of foo.outer() return functions which
    // share the SFI.
    v8::Local<v8::Function> v8_inner1 =
        CompileRun(create_new_inner).As<v8::Function>();
    v8::Local<v8::Function> v8_inner2 =
        CompileRun(create_new_inner).As<v8::Function>();

    Handle<JSFunction> inner1 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner1));
    Handle<JSFunction> inner2 =
        Handle<JSFunction>::cast(Utils::OpenHandle(*v8_inner2));

    CHECK_EQ(inner1->shared(), inner2->shared());
  }
}

}  // namespace internal
}  // namespace v8
