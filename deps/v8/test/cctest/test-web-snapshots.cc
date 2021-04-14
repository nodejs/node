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

void TestWebSnapshot(const char* snapshot_source, const char* test_source,
                     const char* expected_result, uint32_t string_count,
                     uint32_t map_count, uint32_t context_count,
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
    v8::Local<v8::String> result = CompileRun(test_source).As<v8::String>();
    CHECK(result->Equals(new_context, v8_str(expected_result)).FromJust());
    CHECK_EQ(string_count, deserializer.string_count());
    CHECK_EQ(map_count, deserializer.map_count());
    CHECK_EQ(context_count, deserializer.context_count());
    CHECK_EQ(function_count, deserializer.function_count());
    CHECK_EQ(object_count, deserializer.object_count());
  }
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
