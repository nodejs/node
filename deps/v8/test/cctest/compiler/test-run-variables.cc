// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "test/cctest/compiler/function-tester.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static const char* throws = NULL;

static const char* load_tests[] = {
    "var x = a; r = x",                       "123",       "0",
    "var x = (r = x)",                        "undefined", "undefined",
    "var x = (a?1:2); r = x",                 "1",         "2",
    "const x = a; r = x",                     "123",       "0",
    "const x = (r = x)",                      "undefined", "undefined",
    "const x = (a?3:4); r = x",               "3",         "4",
    "'use strict'; const x = a; r = x",       "123",       "0",
    "'use strict'; const x = (r = x)",        throws,      throws,
    "'use strict'; const x = (a?5:6); r = x", "5",         "6",
    "'use strict'; let x = a; r = x",         "123",       "0",
    "'use strict'; let x = (r = x)",          throws,      throws,
    "'use strict'; let x = (a?7:8); r = x",   "7",         "8",
    NULL};

static const char* store_tests[] = {
    "var x = 1; x = a; r = x",                     "123",  "0",
    "var x = (a?(x=4,2):3); r = x",                "2",    "3",
    "var x = (a?4:5); x = a; r = x",               "123",  "0",
    "const x = 1; x = a; r = x",                   "1",    "1",
    "const x = (a?(x=4,2):3); r = x",              "2",    "3",
    "const x = (a?4:5); x = a; r = x",             "4",    "5",
    // Assignments to 'const' are SyntaxErrors, handled by the parser,
    // hence we cannot test them here because they are early errors.
    "'use strict'; let x = 1; x = a; r = x",       "123",  "0",
    "'use strict'; let x = (a?(x=4,2):3); r = x",  throws, "3",
    "'use strict'; let x = (a?4:5); x = a; r = x", "123",  "0",
    NULL};

static const char* bind_tests[] = {
    "if (a) { const x = a }; r = x;",            "123", "undefined",
    "for (; a > 0; a--) { const x = a }; r = x", "123", "undefined",
    // Re-initialization of variables other than legacy 'const' is not
    // possible due to sane variable scoping, hence no tests here.
    NULL};


static void RunVariableTests(const char* source, const char* tests[]) {
  FLAG_harmony_scoping = true;
  EmbeddedVector<char, 512> buffer;

  for (int i = 0; tests[i] != NULL; i += 3) {
    SNPrintF(buffer, source, tests[i]);
    PrintF("#%d: %s\n", i / 3, buffer.start());
    FunctionTester T(buffer.start());

    // Check function with non-falsey parameter.
    if (tests[i + 1] != throws) {
      Handle<Object> r = v8::Utils::OpenHandle(*CompileRun(tests[i + 1]));
      T.CheckCall(r, T.Val(123), T.Val("result"));
    } else {
      T.CheckThrows(T.Val(123), T.Val("result"));
    }

    // Check function with falsey parameter.
    if (tests[i + 2] != throws) {
      Handle<Object> r = v8::Utils::OpenHandle(*CompileRun(tests[i + 2]));
      T.CheckCall(r, T.Val(0.0), T.Val("result"));
    } else {
      T.CheckThrows(T.Val(0.0), T.Val("result"));
    }
  }
}


TEST(StackLoadVariables) {
  const char* source = "(function(a,r) { %s; return r; })";
  RunVariableTests(source, load_tests);
}


TEST(ContextLoadVariables) {
  const char* source = "(function(a,r) { %s; function f() {x} return r; })";
  RunVariableTests(source, load_tests);
}


TEST(StackStoreVariables) {
  const char* source = "(function(a,r) { %s; return r; })";
  RunVariableTests(source, store_tests);
}


TEST(ContextStoreVariables) {
  const char* source = "(function(a,r) { %s; function f() {x} return r; })";
  RunVariableTests(source, store_tests);
}


TEST(StackInitializeVariables) {
  const char* source = "(function(a,r) { %s; return r; })";
  RunVariableTests(source, bind_tests);
}


TEST(ContextInitializeVariables) {
  const char* source = "(function(a,r) { %s; function f() {x} return r; })";
  RunVariableTests(source, bind_tests);
}


TEST(SelfReferenceVariable) {
  FunctionTester T("(function self() { return self; })");

  T.CheckCall(T.function);
  CompileRun("var self = 'not a function'");
  T.CheckCall(T.function);
}
