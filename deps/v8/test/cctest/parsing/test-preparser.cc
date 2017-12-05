// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"
#include "src/compiler.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/parsing/preparsed-scope-data.h"

#include "test/cctest/cctest.h"
#include "test/cctest/scope-test-helper.h"
#include "test/cctest/unicode-helpers.h"

namespace {

enum SkipTests {
  DONT_SKIP = 0,
  // Skip if the test function declares itself strict, otherwise don't skip.
  SKIP_STRICT_FUNCTION = 1,
  // Skip if there's a "use strict" directive above the test.
  SKIP_STRICT_OUTER = 1 << 1,
  SKIP_ARROW = 1 << 2,
  SKIP_STRICT = SKIP_STRICT_FUNCTION | SKIP_STRICT_OUTER
};

enum class PreciseMaybeAssigned { YES, NO };

enum class Bailout { BAILOUT_IF_OUTER_SLOPPY, NO };

}  // namespace

TEST(PreParserScopeAnalysis) {
  i::FLAG_lazy_inner_functions = true;
  i::FLAG_preparser_scope_analysis = true;
  i::FLAG_aggressive_lazy_inner_functions = true;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);
  LocalContext env;

  struct {
    const char* code;
    bool strict_outer;
    bool strict_test_function;
    bool arrow;
    std::vector<unsigned> location;  // "Directions" to the relevant scope.
  } outers[] = {
      // Normal case (test function at the laziness boundary):
      {"(function outer() { function test(%s) { %s \n"
       "function skippable() { } } })();",
       false,
       false,
       false,
       {0, 0}},

      {"(function outer() { let test2 = function test(%s) { %s \n"
       "function skippable() { } } })();",
       false,
       false,
       false,
       {0, 0}},

      // Arrow functions (they can never be at the laziness boundary):
      {"(function outer() { function inner() { (%s) => { %s } \n"
       "function skippable() { } } })();",
       false,
       false,
       true,
       {0, 0}},

      // Repeat the above mentioned cases w/ outer function declaring itself
      // strict:
      {"(function outer() { 'use strict'; function test(%s) { %s \n"
       "function skippable() { } } })();",
       true,
       false,
       false,
       {0, 0}},

      {"(function outer() { 'use strict'; function inner() { "
       "(%s) => { %s } \nfunction skippable() { } } })();",
       true,
       false,
       true,
       {0, 0}},

      // ... and with the test function declaring itself strict:
      {"(function outer() { function test(%s) { 'use strict'; %s \n"
       "function skippable() { } } })();",
       false,
       true,
       false,
       {0, 0}},

      {"(function outer() { function inner() { "
       "(%s) => { 'use strict'; %s } \nfunction skippable() { } } })();",
       false,
       true,
       true,
       {0, 0}},

      // Methods containing skippable functions.
      {"class MyClass { constructor(%s) { %s \n"
       "function skippable() { } } }",
       true,
       true,
       false,
       {0, 0}},

      {"class MyClass { test(%s) { %s \n"
       "function skippable() { } } }",
       true,
       true,
       false,
       // The default constructor is scope 0 inside the class.
       {0, 1}},

      // FIXME(marja): Generators and async functions
  };

  struct Inner {
    Inner(const char* s) : source(s) {}  // NOLINT
    Inner(const char* s, SkipTests skip) : source(s), skip(skip) {}
    Inner(const char* s, SkipTests skip, PreciseMaybeAssigned precise)
        : source(s), skip(skip), precise_maybe_assigned(precise) {}

    Inner(const char* p, const char* s) : params(p), source(s) {}
    Inner(const char* p, const char* s, SkipTests skip)
        : params(p), source(s), skip(skip) {}
    Inner(const char* p, const char* s, SkipTests skip,
          PreciseMaybeAssigned precise)
        : params(p), source(s), skip(skip), precise_maybe_assigned(precise) {}
    Inner(const char* p, const char* s, SkipTests skip, Bailout bailout)
        : params(p), source(s), skip(skip), bailout(bailout) {}

    const char* params = "";
    const char* source;
    SkipTests skip = DONT_SKIP;
    PreciseMaybeAssigned precise_maybe_assigned = PreciseMaybeAssigned::YES;
    Bailout bailout = Bailout::NO;
  } inners[] = {
      // Simple cases
      {"var1;"},
      {"var1 = 5;"},
      {"if (true) {}"},
      {"function f1() {}"},
      {"test;"},
      {"test2;"},

      // Var declarations and assignments.
      {"var var1;"},
      {"var var1; var1 = 5;"},
      {"if (true) { var var1; }", DONT_SKIP, PreciseMaybeAssigned::NO},
      {"if (true) { var var1; var1 = 5; }"},
      {"var var1; function f() { var1; }"},
      {"var var1; var1 = 5; function f() { var1; }"},
      {"var var1; function f() { var1 = 5; }"},
      {"function f1() { f2(); } function f2() {}"},

      // Let declarations and assignments.
      {"let var1;"},
      {"let var1; var1 = 5;"},
      {"if (true) { let var1; }"},
      {"if (true) { let var1; var1 = 5; }"},
      {"let var1; function f() { var1; }"},
      {"let var1; var1 = 5; function f() { var1; }"},
      {"let var1; function f() { var1 = 5; }"},

      // Const declarations.
      {"const var1 = 5;"},
      {"if (true) { const var1 = 5; }"},
      {"const var1 = 5; function f() { var1; }"},

      // Functions.
      {"function f1() { let var2; }"},
      {"var var1 = function f1() { let var2; }"},
      {"let var1 = function f1() { let var2; }"},
      {"const var1 = function f1() { let var2; }"},
      {"var var1 = function() { let var2; }"},
      {"let var1 = function() { let var2; }"},
      {"const var1 = function() { let var2; }"},

      {"function *f1() { let var2; }"},
      {"let var1 = function *f1() { let var2; }"},
      {"let var1 = function*() { let var2; }"},

      {"async function f1() { let var2; }"},
      {"let var1 = async function f1() { let var2; }"},
      {"let var1 = async function() { let var2; }"},

      // Redeclarations.
      {"var var1; var var1;"},
      {"var var1; var var1; var1 = 5;"},
      {"var var1; if (true) { var var1; }"},
      {"if (true) { var var1; var var1; }"},
      {"var var1; if (true) { var var1; var1 = 5; }"},
      {"if (true) { var var1; var var1; var1 = 5; }"},
      {"var var1; var var1; function f() { var1; }"},
      {"var var1; var var1; function f() { var1 = 5; }"},

      // Shadowing declarations.
      {"var var1; if (true) { var var1; }"},
      {"var var1; if (true) { let var1; }"},
      {"let var1; if (true) { let var1; }"},

      {"var var1; if (true) { const var1 = 0; }"},
      {"const var1 = 0; if (true) { const var1 = 0; }"},

      // Variables deeper in the subscopes (scopes without variables inbetween).
      {"if (true) { if (true) { function f() { var var1 = 5; } } }"},

      // Arguments and this.
      {"arguments;"},
      {"arguments = 5;", SKIP_STRICT},
      {"if (true) { arguments; }"},
      {"if (true) { arguments = 5; }", SKIP_STRICT},
      {"() => { arguments; }"},
      {"var1, var2, var3", "arguments;"},
      {"var1, var2, var3", "arguments = 5;", SKIP_STRICT},
      {"var1, var2, var3", "() => { arguments; }"},
      {"var1, var2, var3", "() => { arguments = 5; }", SKIP_STRICT},

      {"this;"},
      {"if (true) { this; }"},
      {"() => { this; }"},

      // Variable called "arguments"
      {"var arguments;", SKIP_STRICT},
      {"var arguments; arguments = 5;", SKIP_STRICT},
      {"if (true) { var arguments; }", SKIP_STRICT, PreciseMaybeAssigned::NO},
      {"if (true) { var arguments; arguments = 5; }", SKIP_STRICT},
      {"var arguments; function f() { arguments; }", SKIP_STRICT},
      {"var arguments; arguments = 5; function f() { arguments; }",
       SKIP_STRICT},
      {"var arguments; function f() { arguments = 5; }", SKIP_STRICT},

      {"let arguments;", SKIP_STRICT},
      {"let arguments; arguments = 5;", SKIP_STRICT},
      {"if (true) { let arguments; }", SKIP_STRICT},
      {"if (true) { let arguments; arguments = 5; }", SKIP_STRICT},
      {"let arguments; function f() { arguments; }", SKIP_STRICT},
      {"let arguments; arguments = 5; function f() { arguments; }",
       SKIP_STRICT},
      {"let arguments; function f() { arguments = 5; }", SKIP_STRICT},

      {"const arguments = 5;", SKIP_STRICT},
      {"if (true) { const arguments = 5; }", SKIP_STRICT},
      {"const arguments = 5; function f() { arguments; }", SKIP_STRICT},

      // Destructuring declarations.
      {"var [var1, var2] = [1, 2];"},
      {"var [var1, var2, [var3, var4]] = [1, 2, [3, 4]];"},
      {"var [{var1: var2}, {var3: var4}] = [{var1: 1}, {var3: 2}];"},
      {"var [var1, ...var2] = [1, 2, 3];"},

      {"var {var1: var2, var3: var4} = {var1: 1, var3: 2};"},
      {"var {var1: var2, var3: {var4: var5}} = {var1: 1, var3: {var4: 2}};"},
      {"var {var1: var2, var3: [var4, var5]} = {var1: 1, var3: [2, 3]};"},

      {"let [var1, var2] = [1, 2];"},
      {"let [var1, var2, [var3, var4]] = [1, 2, [3, 4]];"},
      {"let [{var1: var2}, {var3: var4}] = [{var1: 1}, {var3: 2}];"},
      {"let [var1, ...var2] = [1, 2, 3];"},

      {"let {var1: var2, var3: var4} = {var1: 1, var3: 2};"},
      {"let {var1: var2, var3: {var4: var5}} = {var1: 1, var3: {var4: 2}};"},
      {"let {var1: var2, var3: [var4, var5]} = {var1: 1, var3: [2, 3]};"},

      {"const [var1, var2] = [1, 2];"},
      {"const [var1, var2, [var3, var4]] = [1, 2, [3, 4]];"},
      {"const [{var1: var2}, {var3: var4}] = [{var1: 1}, {var3: 2}];"},
      {"const [var1, ...var2] = [1, 2, 3];"},

      {"const {var1: var2, var3: var4} = {var1: 1, var3: 2};"},
      {"const {var1: var2, var3: {var4: var5}} = {var1: 1, var3: {var4: 2}};"},
      {"const {var1: var2, var3: [var4, var5]} = {var1: 1, var3: [2, 3]};"},

      // Referencing the function variable.
      {"test;"},
      {"function f1() { f1; }"},
      {"function f1() { function f2() { f1; } }"},
      {"function arguments() {}", SKIP_STRICT},
      {"function f1() {} function f1() {}", SKIP_STRICT},
      {"var f1; function f1() {}"},

      // Assigning to the function variable.
      {"test = 3;"},
      {"function f1() { f1 = 3; }"},
      {"function f1() { f1; } f1 = 3;"},
      {"function arguments() {} arguments = 8;", SKIP_STRICT},
      {"function f1() {} f1 = 3; function f1() {}", SKIP_STRICT},

      // Evals.
      {"var var1; eval('');"},
      {"var var1; function f1() { eval(''); }"},
      {"let var1; eval('');"},
      {"let var1; function f1() { eval(''); }"},
      {"const var1 = 10; eval('');"},
      {"const var1 = 10; function f1() { eval(''); }"},

      // Standard for loops.
      {"for (var var1 = 0; var1 < 10; ++var1) { }"},
      {"for (let var1 = 0; var1 < 10; ++var1) { }"},
      {"for (const var1 = 0; var1 < 10; ++var1) { }"},

      {"for (var var1 = 0; var1 < 10; ++var1) { function foo() { var1; } }"},
      {"for (let var1 = 0; var1 < 10; ++var1) { function foo() { var1; } }"},
      {"for (const var1 = 0; var1 < 10; ++var1) { function foo() { var1; } }"},

      // For of loops
      {"for (var1 of [1, 2]) { }"},
      {"for (var var1 of [1, 2]) { }"},
      {"for (let var1 of [1, 2]) { }"},
      {"for (const var1 of [1, 2]) { }"},

      {"for (var1 of [1, 2]) { var1; }"},
      {"for (var var1 of [1, 2]) { var1; }"},
      {"for (let var1 of [1, 2]) { var1; }"},
      {"for (const var1 of [1, 2]) { var1; }"},

      {"for (var1 of [1, 2]) { var1 = 0; }"},
      {"for (var var1 of [1, 2]) { var1 = 0; }"},
      {"for (let var1 of [1, 2]) { var1 = 0; }"},
      {"for (const var1 of [1, 2]) { var1 = 0; }"},

      {"for (var1 of [1, 2]) { function foo() { var1; } }"},
      {"for (var var1 of [1, 2]) { function foo() { var1; } }"},
      {"for (let var1 of [1, 2]) { function foo() { var1; } }"},
      {"for (const var1 of [1, 2]) { function foo() { var1; } }"},

      {"for (var1 of [1, 2]) { function foo() { var1 = 0; } }"},
      {"for (var var1 of [1, 2]) { function foo() { var1 = 0; } }"},
      {"for (let var1 of [1, 2]) { function foo() { var1 = 0; } }"},
      {"for (const var1 of [1, 2]) { function foo() { var1 = 0; } }"},

      // For in loops
      {"for (var1 in {a: 6}) { }"},
      {"for (var var1 in {a: 6}) { }"},
      {"for (let var1 in {a: 6}) { }"},
      {"for (const var1 in {a: 6}) { }"},

      {"for (var1 in {a: 6}) { var1; }"},
      {"for (var var1 in {a: 6}) { var1; }"},
      {"for (let var1 in {a: 6}) { var1; }"},
      {"for (const var1 in {a: 6}) { var1; }"},

      {"for (var1 in {a: 6}) { var1 = 0; }"},
      {"for (var var1 in {a: 6}) { var1 = 0; }"},
      {"for (let var1 in {a: 6}) { var1 = 0; }"},
      {"for (const var1 in {a: 6}) { var1 = 0; }"},

      {"for (var1 in {a: 6}) { function foo() { var1; } }"},
      {"for (var var1 in {a: 6}) { function foo() { var1; } }"},
      {"for (let var1 in {a: 6}) { function foo() { var1; } }"},
      {"for (const var1 in {a: 6}) { function foo() { var1; } }"},

      {"for (var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (var var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (let var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (const var1 in {a: 6}) { function foo() { var1 = 0; } }"},

      {"for (var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (var var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (let var1 in {a: 6}) { function foo() { var1 = 0; } }"},
      {"for (const var1 in {a: 6}) { function foo() { var1 = 0; } }"},

      // Destructuring loop variable
      {"for ([var1, var2] of [[1, 1], [2, 2]]) { }"},
      {"for (var [var1, var2] of [[1, 1], [2, 2]]) { }"},
      {"for (let [var1, var2] of [[1, 1], [2, 2]]) { }"},
      {"for (const [var1, var2] of [[1, 1], [2, 2]]) { }"},

      {"for ([var1, var2] of [[1, 1], [2, 2]]) { var2 = 3; }"},
      {"for (var [var1, var2] of [[1, 1], [2, 2]]) { var2 = 3; }"},
      {"for (let [var1, var2] of [[1, 1], [2, 2]]) { var2 = 3; }"},
      {"for (const [var1, var2] of [[1, 1], [2, 2]]) { var2 = 3; }"},

      {"for ([var1, var2] of [[1, 1], [2, 2]]) { () => { var2 = 3; } }"},
      {"for (var [var1, var2] of [[1, 1], [2, 2]]) { () => { var2 = 3; } }"},
      {"for (let [var1, var2] of [[1, 1], [2, 2]]) { () => { var2 = 3; } }"},
      {"for (const [var1, var2] of [[1, 1], [2, 2]]) { () => { var2 = 3; } }"},

      // Skippable function in loop header
      {"for (let [var1, var2 = function() { }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var1; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var2; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var1; var2; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var1 = 0; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var2 = 0; }] of [[1]]) { }"},
      {"for (let [var1, var2 = function() { var1 = 0; var2 = 0; }] of [[1]]) { "
       "}"},

      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var1; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var2; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var1; var2; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var1 = 0; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var2 = 0; } }"},
      {"for (let [var1, var2 = function() { }] of [[1]]) { function f() { "
       "var1 = 0; var2 = 0; } }"},
      {"for (let [var1, var2 = function() { var1; }] of [[1]]) { "
       "function f() { var1; } }"},
      {"for (let [var1, var2 = function() { var1; }] of [[1]]) { "
       "function f() { var2; } }"},
      {"for (let [var1, var2 = function() { var1; }] of [[1]]) { "
       "function f() { var1; var2; } }"},
      {"for (let [var1, var2 = function() { var2; }] of [[1]]) { "
       "function f() { var1; } }"},
      {"for (let [var1, var2 = function() { var2; }] of [[1]]) { "
       "function f() { var2; } }"},
      {"for (let [var1, var2 = function() { var2; }] of [[1]]) { "
       "function f() { var1; var2; } }"},

      // Loops without declarations
      {"var var1 = 0; for ( ; var1 < 2; ++var1) { }"},
      {"var var1 = 0; for ( ; var1 < 2; ++var1) { function foo() { var1; } }"},
      {"var var1 = 0; for ( ; var1 > 2; ) { }"},
      {"var var1 = 0; for ( ; var1 > 2; ) { function foo() { var1; } }"},
      {"var var1 = 0; for ( ; var1 > 2; ) { function foo() { var1 = 6; } }"},

      {"var var1 = 0; for(var1; var1 < 2; ++var1) { }"},
      {"var var1 = 0; for (var1; var1 < 2; ++var1) { function foo() { var1; } "
       "}"},
      {"var var1 = 0; for (var1; var1 > 2; ) { }"},
      {"var var1 = 0; for (var1; var1 > 2; ) { function foo() { var1; } }"},
      {"var var1 = 0; for (var1; var1 > 2; ) { function foo() { var1 = 6; } }"},

      // Block functions (potentially sloppy).
      {"if (true) { function f1() {} }"},
      {"if (true) { function f1() {} function f1() {} }", SKIP_STRICT},
      {"if (true) { if (true) { function f1() {} } }"},
      {"if (true) { if (true) { function f1() {} function f1() {} } }",
       SKIP_STRICT},
      {"if (true) { function f1() {} f1 = 3; }"},

      {"if (true) { function f1() {} function foo() { f1; } }"},
      {"if (true) { function f1() {} } function foo() { f1; }"},
      {"if (true) { function f1() {} function f1() {} function foo() { f1; } "
       "}",
       SKIP_STRICT},
      {"if (true) { function f1() {} function f1() {} } function foo() { f1; "
       "}",
       SKIP_STRICT},
      {"if (true) { if (true) { function f1() {} } function foo() { f1; } }"},
      {"if (true) { if (true) { function f1() {} function f1() {} } function "
       "foo() { f1; } }",
       SKIP_STRICT},
      {"if (true) { function f1() {} f1 = 3; function foo() { f1; } }"},
      {"if (true) { function f1() {} f1 = 3; } function foo() { f1; }"},

      {"var f1 = 1; if (true) { function f1() {} }"},
      {"var f1 = 1; if (true) { function f1() {} } function foo() { f1; }"},

      {"if (true) { function f1() {} function f2() { f1(); } }"},

      {"if (true) { function *f1() {} }"},
      {"if (true) { async function f1() {} }"},

      // (Potentially sloppy) block function shadowing a catch variable.
      {"try { } catch(var1) { if (true) { function var1() {} } }"},

      // Simple parameters.
      {"var1", ""},
      {"var1", "var1;"},
      {"var1", "var1 = 9;"},
      {"var1", "function f1() { var1; }"},
      {"var1", "function f1() { var1 = 9; }"},

      {"var1, var2", ""},
      {"var1, var2", "var2;"},
      {"var1, var2", "var2 = 9;"},
      {"var1, var2", "function f1() { var2; }"},
      {"var1, var2", "function f1() { var2 = 9; }"},
      {"var1, var2", "var1;"},
      {"var1, var2", "var1 = 9;"},
      {"var1, var2", "function f1() { var1; }"},
      {"var1, var2", "function f1() { var1 = 9; }"},

      // Duplicate parameters.
      {"var1, var1", "", SkipTests(SKIP_STRICT | SKIP_ARROW)},
      {"var1, var1", "var1;", SkipTests(SKIP_STRICT | SKIP_ARROW)},
      {"var1, var1", "var1 = 9;", SkipTests(SKIP_STRICT | SKIP_ARROW)},
      {"var1, var1", "function f1() { var1; }",
       SkipTests(SKIP_STRICT | SKIP_ARROW)},
      {"var1, var1", "function f1() { var1 = 9; }",
       SkipTests(SKIP_STRICT | SKIP_ARROW)},

      // If the function declares itself strict, non-simple parameters aren't
      // allowed.

      // Rest parameter.
      {"...var2", "", SKIP_STRICT_FUNCTION},
      {"...var2", "var2;", SKIP_STRICT_FUNCTION},
      {"...var2", "var2 = 9;", SKIP_STRICT_FUNCTION},
      {"...var2", "function f1() { var2; }", SKIP_STRICT_FUNCTION},
      {"...var2", "function f1() { var2 = 9; }", SKIP_STRICT_FUNCTION},

      {"var1, ...var2", "", SKIP_STRICT_FUNCTION},
      {"var1, ...var2", "var2;", SKIP_STRICT_FUNCTION},
      {"var1, ...var2", "var2 = 9;", SKIP_STRICT_FUNCTION},
      {"var1, ...var2", "function f1() { var2; }", SKIP_STRICT_FUNCTION},
      {"var1, ...var2", "function f1() { var2 = 9; }", SKIP_STRICT_FUNCTION},

      // Default parameters.
      {"var1 = 3", "", SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},
      {"var1, var2 = var1", "", SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},
      {"var1, var2 = 4, ...var3", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},

      // Destructuring parameters. Because of the search space explosion, we
      // cannot test all interesting cases. Let's try to test a relevant subset.
      {"[]", "", SKIP_STRICT_FUNCTION},
      {"{}", "", SKIP_STRICT_FUNCTION},

      {"[var1]", "", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "", SKIP_STRICT_FUNCTION},
      {"{var1}", "", SKIP_STRICT_FUNCTION},

      {"[var1]", "var1;", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "var1;", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "name1;", SKIP_STRICT_FUNCTION},
      {"{var1}", "var1;", SKIP_STRICT_FUNCTION},

      {"[var1]", "var1 = 16;", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "var1 = 16;", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "name1 = 16;", SKIP_STRICT_FUNCTION},
      {"{var1}", "var1 = 16;", SKIP_STRICT_FUNCTION},

      {"[var1]", "() => { var1; }", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "() => { var1; }", SKIP_STRICT_FUNCTION},
      {"{name1: var1}", "() => { name1; }", SKIP_STRICT_FUNCTION},
      {"{var1}", "() => { var1; }", SKIP_STRICT_FUNCTION},

      {"[var1, var2, var3]", "", SKIP_STRICT_FUNCTION},
      {"{name1: var1, name2: var2, name3: var3}", "", SKIP_STRICT_FUNCTION},
      {"{var1, var2, var3}", "", SKIP_STRICT_FUNCTION},

      {"[var1, var2, var3]", "() => { var2 = 16;}", SKIP_STRICT_FUNCTION},
      {"{name1: var1, name2: var2, name3: var3}", "() => { var2 = 16;}",
       SKIP_STRICT_FUNCTION},
      {"{name1: var1, name2: var2, name3: var3}", "() => { name2 = 16;}",
       SKIP_STRICT_FUNCTION},
      {"{var1, var2, var3}", "() => { var2 = 16;}", SKIP_STRICT_FUNCTION},

      // Nesting destructuring.
      {"[var1, [var2, var3], {var4, name5: [var5, var6]}]", "",
       SKIP_STRICT_FUNCTION},

      // Complicated params.
      {"var1, [var2], var3 = 24, [var4, var5] = [2, 4], var6, {var7}, var8, "
       "{name9: var9, name10: var10}, ...var11",
       "", SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},

      // Complicated cases from bugs.
      {"var1 = {} = {}", "", SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},

      // Destructuring rest. Because we can.
      {"var1, ...[var2]", "", SKIP_STRICT_FUNCTION},
      {"var1, ...[var2]", "() => { var2; }", SKIP_STRICT_FUNCTION},
      {"var1, ...{0: var2}", "", SKIP_STRICT_FUNCTION},
      {"var1, ...{0: var2}", "() => { var2; }", SKIP_STRICT_FUNCTION},
      {"var1, ...[]", "", SKIP_STRICT_FUNCTION},
      {"var1, ...{}", "", SKIP_STRICT_FUNCTION},
      {"var1, ...[var2, var3]", "", SKIP_STRICT_FUNCTION},
      {"var1, ...{0: var2, 1: var3}", "", SKIP_STRICT_FUNCTION},

      // Default parameters for destruring parameters.
      {"[var1, var2] = [2, 4]", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},
      {"{var1, var2} = {var1: 3, var2: 3}", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},

      // Default parameters inside destruring parameters.
      {"[var1 = 4, var2 = var1]", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},
      {"{var1 = 4, var2 = var1}", "", SKIP_STRICT_FUNCTION,
       PreciseMaybeAssigned::NO},

      // Locals shadowing parameters.
      {"var1, var2", "var var1 = 16; () => { var1 = 17; }"},

      // Locals shadowing destructuring parameters and the rest parameter.
      {"[var1, var2]", "var var1 = 16; () => { var1 = 17; }",
       SKIP_STRICT_FUNCTION},
      {"{var1, var2}", "var var1 = 16; () => { var1 = 17; }",
       SKIP_STRICT_FUNCTION},
      {"var1, var2, ...var3", "var var3 = 16; () => { var3 = 17; }",
       SKIP_STRICT_FUNCTION},
      {"var1, var2 = var1", "var var1 = 16; () => { var1 = 17; }",
       SKIP_STRICT_FUNCTION, PreciseMaybeAssigned::NO},

      // Hoisted sloppy block function shadowing a parameter.
      // FIXME(marja): why is maybe_assigned inaccurate?
      {"var1, var2", "for (;;) { function var1() { } }", DONT_SKIP,
       PreciseMaybeAssigned::NO},

      // Sloppy eval in default parameter.
      {"var1, var2 = eval(''), var3", "let var4 = 0;", SKIP_STRICT_FUNCTION,
       Bailout::BAILOUT_IF_OUTER_SLOPPY},
      {"var1, var2 = eval(''), var3 = eval('')", "let var4 = 0;",
       SKIP_STRICT_FUNCTION, Bailout::BAILOUT_IF_OUTER_SLOPPY},

      // Sloppy eval in arrow function parameter list which is inside another
      // arrow function parameter list.
      {"var1, var2 = (var3, var4 = eval(''), var5) => { let var6; }, var7",
       "let var8 = 0;", SKIP_STRICT_FUNCTION, Bailout::BAILOUT_IF_OUTER_SLOPPY},

      // Sloppy eval in a function body with non-simple parameters.
      {"var1 = 1, var2 = 2", "eval('');", SKIP_STRICT_FUNCTION},

      // Catch variable
      {"try { } catch(var1) { }"},
      {"try { } catch(var1) { var1; }"},
      {"try { } catch(var1) { var1 = 3; }"},
      {"try { } catch(var1) { function f() { var1; } }"},
      {"try { } catch(var1) { function f() { var1 = 3; } }"},

      {"try { } catch({var1, var2}) { function f() { var1 = 3; } }"},
      {"try { } catch([var1, var2]) { function f() { var1 = 3; } }"},
      {"try { } catch({}) { }"},
      {"try { } catch([]) { }"},

      // Shadowing the catch variable
      {"try { } catch(var1) { var var1 = 3; }"},
      {"try { } catch(var1) { var var1 = 3; function f() { var1 = 3; } }"},

      // Classes
      {"class MyClass {}"},
      {"var1 = class MyClass {}"},
      {"var var1 = class MyClass {}"},
      {"let var1 = class MyClass {}"},
      {"const var1 = class MyClass {}"},
      {"var var1 = class {}"},
      {"let var1 = class {}"},
      {"const var1 = class {}"},

      {"class MyClass { constructor() {} }"},
      {"class MyClass { constructor() { var var1; } }"},
      {"class MyClass { constructor() { var var1 = 11; } }"},
      {"class MyClass { constructor() { var var1; function foo() { var1 = 11; "
       "} } }"},

      {"class MyClass { m() {} }"},
      {"class MyClass { m() { var var1; } }"},
      {"class MyClass { m() { var var1 = 11; } }"},
      {"class MyClass { m() { var var1; function foo() { var1 = 11; } } }"},

      {"class MyClass { static m() {} }"},
      {"class MyClass { static m() { var var1; } }"},
      {"class MyClass { static m() { var var1 = 11; } }"},
      {"class MyClass { static m() { var var1; function foo() { var1 = 11; } } "
       "}"},

      {"class MyBase {} class MyClass extends MyBase {}"},
      {"class MyClass extends MyBase { constructor() {} }"},
      {"class MyClass extends MyBase { constructor() { super(); } }"},
      {"class MyClass extends MyBase { constructor() { var var1; } }"},
      {"class MyClass extends MyBase { constructor() { var var1 = 11; } }"},
      {"class MyClass extends MyBase { constructor() { var var1; function "
       "foo() { var1 = 11; } } }"},

      {"class MyClass extends MyBase { m() {} }"},
      {"class MyClass extends MyBase { m() { super.foo; } }"},
      {"class MyClass extends MyBase { m() { var var1; } }"},
      {"class MyClass extends MyBase { m() { var var1 = 11; } }"},
      {"class MyClass extends MyBase { m() { var var1; function foo() { var1 = "
       "11; } } }"},

      {"class MyClass extends MyBase { static m() {} }"},
      {"class MyClass extends MyBase { static m() { super.foo; } }"},
      {"class MyClass extends MyBase { static m() { var var1; } }"},
      {"class MyClass extends MyBase { static m() { var var1 = 11; } }"},
      {"class MyClass extends MyBase { static m() { var var1; function foo() { "
       "var1 = 11; } } }"},
  };

  for (unsigned outer_ix = 0; outer_ix < arraysize(outers); ++outer_ix) {
    for (unsigned inner_ix = 0; inner_ix < arraysize(inners); ++inner_ix) {
      if (outers[outer_ix].strict_outer &&
          (inners[inner_ix].skip & SKIP_STRICT_OUTER)) {
        continue;
      }
      if (outers[outer_ix].strict_test_function &&
          (inners[inner_ix].skip & SKIP_STRICT_FUNCTION)) {
        continue;
      }
      if (outers[outer_ix].arrow && (inners[inner_ix].skip & SKIP_ARROW)) {
        continue;
      }

      const char* code = outers[outer_ix].code;
      int code_len = Utf8LengthHelper(code);

      int params_len = Utf8LengthHelper(inners[inner_ix].params);
      int source_len = Utf8LengthHelper(inners[inner_ix].source);
      int len = code_len + params_len + source_len;

      i::ScopedVector<char> program(len + 1);
      i::SNPrintF(program, code, inners[inner_ix].params,
                  inners[inner_ix].source);

      i::Handle<i::String> source =
          factory->InternalizeUtf8String(program.start());
      source->PrintOn(stdout);
      printf("\n");

      // First compile with the lazy inner function and extract the scope data.
      i::Handle<i::Script> script = factory->NewScript(source);
      i::ParseInfo lazy_info(script);

      // No need to run scope analysis; preparser scope data is produced when
      // parsing.
      CHECK(i::parsing::ParseProgram(&lazy_info, isolate));

      // Retrieve the scope data we produced.
      i::Scope* scope_with_data = i::ScopeTestHelper::FindScope(
          lazy_info.literal()->scope(), outers[outer_ix].location);
      i::ProducedPreParsedScopeData* produced_data =
          scope_with_data->AsDeclarationScope()
              ->produced_preparsed_scope_data();
      i::MaybeHandle<i::PreParsedScopeData> maybe_produced_data_on_heap =
          produced_data->Serialize(isolate);
      if (inners[inner_ix].bailout == Bailout::BAILOUT_IF_OUTER_SLOPPY &&
          !outers[outer_ix].strict_outer) {
        DCHECK(maybe_produced_data_on_heap.is_null());
        continue;
      }
      DCHECK(!maybe_produced_data_on_heap.is_null());
      i::Handle<i::PreParsedScopeData> produced_data_on_heap =
          maybe_produced_data_on_heap.ToHandleChecked();

      // Then parse eagerly and check against the scope data.
      script = factory->NewScript(source);

      i::ParseInfo eager_normal(script);
      eager_normal.set_allow_lazy_parsing(false);

      CHECK(i::parsing::ParseProgram(&eager_normal, isolate));
      CHECK(i::Compiler::Analyze(&eager_normal));

      // Compare the allocation of the variables in two cases: 1) normal scope
      // allocation 2) allocation based on the preparse data.

      i::Scope* normal_scope = i::ScopeTestHelper::FindScope(
          eager_normal.literal()->scope(), outers[outer_ix].location);
      CHECK_NULL(normal_scope->sibling());
      CHECK(normal_scope->is_function_scope());

      i::ParseInfo eager_using_scope_data(script);
      eager_using_scope_data.set_allow_lazy_parsing(false);

      CHECK(i::parsing::ParseProgram(&eager_using_scope_data, isolate));
      // Don't run scope analysis (that would obviously decide the correct
      // allocation for the variables).

      i::Scope* unallocated_scope = i::ScopeTestHelper::FindScope(
          eager_using_scope_data.literal()->scope(), outers[outer_ix].location);
      CHECK_NULL(unallocated_scope->sibling());
      CHECK(unallocated_scope->is_function_scope());

      // Mark all inner functions as "skipped", so that we don't try to restore
      // data for them. No test should contain eager functions, because we
      // cannot properly decide whether we have or don't have data for them.
      i::ScopeTestHelper::MarkInnerFunctionsAsSkipped(unallocated_scope);
      i::ConsumedPreParsedScopeData* consumed_preparsed_scope_data =
          lazy_info.consumed_preparsed_scope_data();
      consumed_preparsed_scope_data->SetData(produced_data_on_heap);
      consumed_preparsed_scope_data->SkipFunctionDataForTesting();
      consumed_preparsed_scope_data->RestoreScopeAllocationData(
          unallocated_scope->AsDeclarationScope());
      i::ScopeTestHelper::AllocateWithoutVariableResolution(unallocated_scope);

      i::ScopeTestHelper::CompareScopes(
          normal_scope, unallocated_scope,
          inners[inner_ix].precise_maybe_assigned == PreciseMaybeAssigned::YES);
    }
  }
}

// Regression test for
// https://bugs.chromium.org/p/chromium/issues/detail?id=753896. Should not
// crash.
TEST(Regress753896) {
  i::FLAG_preparser_scope_analysis = true;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);
  LocalContext env;

  i::Handle<i::String> source = factory->InternalizeUtf8String(
      "function lazy() { let v = 0; if (true) { var v = 0; } }");
  i::Handle<i::Script> script = factory->NewScript(source);
  i::ParseInfo info(script);

  // We don't assert that parsing succeeded or that it failed; currently the
  // error is not detected inside lazy functions, but it might be in the future.
  i::parsing::ParseProgram(&info, isolate);
}

TEST(ProducingAndConsumingByteData) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);
  LocalContext env;

  i::Zone zone(isolate->allocator(), ZONE_NAME);
  i::ProducedPreParsedScopeData::ByteData bytes(&zone);
  // Write some data.
  bytes.WriteUint32(1983);  // This will be overwritten.
  bytes.WriteUint32(2147483647);
  bytes.WriteUint8(4);
  bytes.WriteUint8(255);
  bytes.WriteUint32(0);
  bytes.WriteUint8(0);
  bytes.OverwriteFirstUint32(2017);
  bytes.WriteUint8(100);

  i::Handle<i::PodArray<uint8_t>> data_on_heap = bytes.Serialize(isolate);
  i::ConsumedPreParsedScopeData::ByteData bytes_for_reading;
  i::ConsumedPreParsedScopeData::ByteData::ReadingScope reading_scope(
      &bytes_for_reading, *data_on_heap);

  // Read the data back.
  CHECK_EQ(bytes_for_reading.ReadUint32(), 2017);
  CHECK_EQ(bytes_for_reading.ReadUint32(), 2147483647);
  CHECK_EQ(bytes_for_reading.ReadUint8(), 4);
  CHECK_EQ(bytes_for_reading.ReadUint8(), 255);
  CHECK_EQ(bytes_for_reading.ReadUint32(), 0);
  CHECK_EQ(bytes_for_reading.ReadUint8(), 0);
  CHECK_EQ(bytes_for_reading.ReadUint8(), 100);
}
