// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test specific cases of the lazy/eager-parse decision.
//
// Note that presently most unit tests for parsing are found in
// cctest/test-parsing.cc.

#include <unordered_map>

#include "include/v8.h"
#include "src/api.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/utils.h"

#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

namespace {

// Record the 'compiled' state of all top level functions.
void GetTopLevelFunctionInfo(
    v8::Local<v8::Script> script,
    std::unordered_map<std::string, bool>* is_compiled) {
  // Get the v8::internal::Script object from the API v8::Script.
  // The API object 'wraps' the compiled top-level function, not the i::Script.
  Handle<JSFunction> toplevel_fn = v8::Utils::OpenHandle(*script);
  Handle<Script> i_script =
      handle(Script::cast(toplevel_fn->shared()->script()));
  SharedFunctionInfo::ScriptIterator iterator(i_script);

  while (SharedFunctionInfo* shared = iterator.Next()) {
    std::unique_ptr<char[]> name = String::cast(shared->name())->ToCString();
    is_compiled->insert(std::make_pair(name.get(), shared->is_compiled()));
  }
}

}  // anonymous namespace

TEST(GetTopLevelFunctionInfo) {
  if (!FLAG_lazy) return;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  LocalContext env;

  const char src[] = "function foo() { var a; }\n";
  std::unordered_map<std::string, bool> is_compiled;
  GetTopLevelFunctionInfo(v8_compile(src), &is_compiled);

  // Test that our helper function GetTopLevelFunctionInfo does what it claims:
  DCHECK(is_compiled.find("foo") != is_compiled.end());
  DCHECK(is_compiled.find("bar") == is_compiled.end());
}

TEST(EagerlyCompileImmediateUseFunctions) {
  if (!FLAG_lazy) return;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  LocalContext env;

  // Test parenthesized, exclaimed, and regular functions. Make sure these
  // occur both intermixed and after each other, to make sure the 'reset'
  // mechanism works.
  const char src[] =
      "function normal() { var a; }\n"             // Normal: Should lazy parse.
      "(function parenthesized() { var b; })()\n"  // Parenthesized: Pre-parse.
      "!function exclaimed() { var c; }() \n"      // Exclaimed: Pre-parse.
      "function normal2() { var d; }\n"
      "(function parenthesized2() { var e; })()\n"
      "function normal3() { var f; }\n"
      "!function exclaimed2() { var g; }() \n"
      "function normal4() { var h; }\n";

  std::unordered_map<std::string, bool> is_compiled;
  GetTopLevelFunctionInfo(v8_compile(src), &is_compiled);

  DCHECK(is_compiled["parenthesized"]);
  DCHECK(is_compiled["parenthesized2"]);
  DCHECK(is_compiled["exclaimed"]);
  DCHECK(is_compiled["exclaimed2"]);
  DCHECK(!is_compiled["normal"]);
  DCHECK(!is_compiled["normal2"]);
  DCHECK(!is_compiled["normal3"]);
  DCHECK(!is_compiled["normal4"]);
}

TEST(CommaFunctionSequence) {
  if (!FLAG_lazy) return;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  LocalContext env;

  const char src[] = "!function a(){}(),function b(){}(),function c(){}();";
  std::unordered_map<std::string, bool> is_compiled;
  GetTopLevelFunctionInfo(v8_compile(src), &is_compiled);

  DCHECK(is_compiled["a"]);
  DCHECK(is_compiled["b"]);
  DCHECK(is_compiled["c"]);
}

}  // namespace internal
}  // namespace v8
