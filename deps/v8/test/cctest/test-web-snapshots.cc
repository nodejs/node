// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  CompileRun(snapshot_source);
  WebSnapshotData snapshot_data;
  {
    std::vector<std::string> exports;
    exports.push_back("foo");
    WebSnapshotSerializer serializer(isolate);
    CHECK(serializer.TakeSnapshot(context, exports, snapshot_data));
    CHECK(!serializer.has_error());
    CHECK_NOT_NULL(snapshot_data.buffer);
    CHECK_EQ(string_count, serializer.string_count());
    CHECK_EQ(map_count, serializer.map_count());
    CHECK_EQ(context_count, serializer.context_count());
    CHECK_EQ(function_count, serializer.function_count());
    CHECK_EQ(object_count, serializer.object_count());
  }

  {
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

}  // namespace internal
}  // namespace v8
