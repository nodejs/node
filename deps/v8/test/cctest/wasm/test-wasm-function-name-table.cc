// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-function-name-table.h"
#include "src/wasm/wasm-module.h"

#include "test/cctest/cctest.h"

using namespace v8::internal;
using namespace v8::internal::wasm;

namespace {

#define CHECK_STREQ(exp, found)                                                \
  do {                                                                         \
    Vector<const char> exp_ = (exp);                                           \
    Vector<const char> found_ = (found);                                       \
    if (V8_UNLIKELY(exp_.length() != found_.length() ||                        \
                    memcmp(exp_.start(), found_.start(), exp_.length()))) {    \
      V8_Fatal(__FILE__, __LINE__,                                             \
               "Check failed: (%s) != (%s) ('%.*s' vs '%.*s').", #exp, #found, \
               exp_.length(), exp_.start(), found_.length(), found_.start());  \
    }                                                                          \
  } while (0)

void testFunctionNameTable(Vector<Vector<const char>> names) {
  Isolate *isolate = CcTest::InitIsolateOnce();
  HandleAndZoneScope scope;

  WasmModule module;
  std::vector<char> all_names;
  // No name should have offset 0, because that encodes unnamed functions.
  // In real wasm binary, offset 0 is impossible anyway.
  all_names.push_back('\0');

  uint32_t func_index = 0;
  for (Vector<const char> name : names) {
    size_t name_offset = name.start() ? all_names.size() : 0;
    all_names.insert(all_names.end(), name.start(),
                     name.start() + name.length());
    // Make every second function name null-terminated.
    if (func_index % 2) all_names.push_back('\0');
    module.functions.push_back(
        {nullptr, 0, 0, static_cast<uint32_t>(name_offset),
         static_cast<uint32_t>(name.length()), 0, 0, false, false});
    ++func_index;
  }

  module.module_start = reinterpret_cast<byte *>(all_names.data());
  module.module_end = module.module_start + all_names.size();

  Handle<Object> wasm_function_name_table =
      BuildFunctionNamesTable(isolate, &module);
  CHECK(wasm_function_name_table->IsByteArray());

  func_index = 0;
  for (Vector<const char> name : names) {
    MaybeHandle<String> string = GetWasmFunctionNameFromTable(
        Handle<ByteArray>::cast(wasm_function_name_table), func_index);
    if (name.start()) {
      CHECK(string.ToHandleChecked()->IsUtf8EqualTo(name));
    } else {
      CHECK(string.is_null());
    }
    ++func_index;
  }
}

void testFunctionNameTable(Vector<const char *> names) {
  std::vector<Vector<const char>> names_vec;
  for (const char *name : names)
    names_vec.push_back(name ? CStrVector(name) : Vector<const char>());
  testFunctionNameTable(Vector<Vector<const char>>(
      names_vec.data(), static_cast<int>(names_vec.size())));
}

}  // namespace

TEST(NoFunctions) { testFunctionNameTable(Vector<Vector<const char>>()); }

TEST(OneFunctions) {
  const char *names[] = {"foo"};
  testFunctionNameTable(ArrayVector(names));
}

TEST(ThreeFunctions) {
  const char *names[] = {"foo", "bar", "baz"};
  testFunctionNameTable(ArrayVector(names));
}

TEST(OneUnnamedFunction) {
  const char *names[] = {""};
  testFunctionNameTable(ArrayVector(names));
}

TEST(UnnamedFirstFunction) {
  const char *names[] = {"", "bar", "baz"};
  testFunctionNameTable(ArrayVector(names));
}

TEST(UnnamedLastFunction) {
  const char *names[] = {"bar", "baz", ""};
  testFunctionNameTable(ArrayVector(names));
}

TEST(ThreeUnnamedFunctions) {
  const char *names[] = {"", "", ""};
  testFunctionNameTable(ArrayVector(names));
}

TEST(UTF8Names) {
  const char *names[] = {"↱fun↰", "↺", "alpha:α beta:β"};
  testFunctionNameTable(ArrayVector(names));
}

TEST(UnnamedVsEmptyNames) {
  const char *names[] = {"", nullptr, nullptr, ""};
  testFunctionNameTable(ArrayVector(names));
}
