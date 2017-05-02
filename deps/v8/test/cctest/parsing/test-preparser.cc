// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"
#include "src/compiler.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"

#include "test/cctest/cctest.h"
#include "test/cctest/scope-test-helper.h"
#include "test/cctest/unicode-helpers.h"

namespace {

enum SkipStrict {
  DONT_SKIP = 0,
  // Skip if the test function declares itself strict, otherwise don't skip.
  SKIP_STRICT_FUNCTION = 1,
  // Skip if there's a "use strict" directive above the test.
  SKIP_STRICT_OUTER = 1 << 1,
  SKIP_STRICT = SKIP_STRICT_FUNCTION | SKIP_STRICT_OUTER
};

}  // namespace

TEST(PreParserScopeAnalysis) {
  i::FLAG_lazy_inner_functions = true;
  i::FLAG_preparser_scope_analysis = true;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();
  i::HandleScope scope(isolate);
  LocalContext env;

  /* Test the following cases:
     1)
     (function outer() {
        function test() { ... }
     })();
     against:
     (function outer() {
        (function test() { ... })();
     })();

     2)
     (function outer() {
        function inner() { function test() { ... } }
     })();
     against:
     (function outer() {
        (function inner() { function test() { ... } })();
     })();
     (Modified function is deeper inside the laziness boundary.)

     3)
     (function outer() {
        function inner() { () => { ... } }
     })();
     against:
     (function outer() {
        (function inner() { () => { ... } })();
     })();

     Inner arrow functions are never lazy, so the corresponding case is missing.
  */

  struct {
    const char* prefix;
    const char* suffix;
    const char* lazy_inner;
    const char* eager_inner;
    bool strict_outer;
    bool strict_test_function;
  } outers[] = {
      // The scope start positions must match; note the extra space in
      // lazy_inner.
      {"(function outer() { ", "})();", " function test(%s) { %s }",
       "(function test(%s) { %s })()", false, false},
      {"(function outer() { ", "})();",
       " function inner() { function test(%s) { %s } }",
       "(function inner() { function test(%s) { %s } })()", false, false},
      // FIXME(marja): enable test for arrow functions once it passes.
      // {"(function outer() { ", "})();",
      // " function inner() { (%s) => { %s } }",
      // "(function inner() { (%s) => { %s } })()", false},
      {"(function outer() { 'use strict'; ", "})();",
       " function test(%s) { %s }", "(function test(%s) { %s })()", true,
       false},
      {"(function outer() { 'use strict'; ", "})();",
       " function inner() { function test(%s) { %s } }",
       "(function inner() { function test(%s) { %s } })()", true, false},
      {"(function outer() { ", "})();",
       " function test(%s) { 'use strict'; %s }",
       "(function test(%s) { 'use strict'; %s })()", false, true},
      {"(function outer() { ", "})();",
       " function inner() { function test(%s) { 'use strict'; %s } }",
       "(function inner() { function test(%s) { 'use strict'; %s } })()", false,
       true},
  };

  struct Inner {
    Inner(const char* s) : source(s) {}  // NOLINT
    Inner(const char* s, SkipStrict skip)
        : source(s), skip_in_strict_mode(skip) {}
    Inner(const char* s, SkipStrict skip, bool precise)
        : source(s),
          skip_in_strict_mode(skip),
          precise_maybe_assigned(precise) {}

    Inner(const char* p, const char* s) : params(p), source(s) {}
    Inner(const char* p, const char* s, SkipStrict skip)
        : params(p), source(s), skip_in_strict_mode(skip) {}
    Inner(const char* p, const char* s, SkipStrict skip, bool precise)
        : params(p),
          source(s),
          skip_in_strict_mode(skip),
          precise_maybe_assigned(precise) {}

    const char* params = "";
    const char* source;
    SkipStrict skip_in_strict_mode = DONT_SKIP;
    bool precise_maybe_assigned = true;
  } inners[] = {
      // Simple cases
      {"var1;"},
      {"var1 = 5;"},
      {"if (true) {}"},
      {"function f1() {}"},

      // Var declarations and assignments.
      {"var var1;"},
      {"var var1; var1 = 5;"},
      {"if (true) { var var1; }", DONT_SKIP, false},
      {"if (true) { var var1; var1 = 5; }"},
      {"var var1; function f() { var1; }"},
      {"var var1; var1 = 5; function f() { var1; }"},
      {"var var1; function f() { var1 = 5; }"},

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

      {"this;"},
      {"if (true) { this; }"},

      // Variable called "arguments"
      {"var arguments;", SKIP_STRICT},
      {"var arguments; arguments = 5;", SKIP_STRICT},
      {"if (true) { var arguments; }", SKIP_STRICT, false},
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
      {"var1, var1", "", SKIP_STRICT},
      {"var1, var1", "var1;", SKIP_STRICT},
      {"var1, var1", "var1 = 9;", SKIP_STRICT},
      {"var1, var1", "function f1() { var1; }", SKIP_STRICT},
      {"var1, var1", "function f1() { var1 = 9; }", SKIP_STRICT},

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
      {"var1 = 3", "", SKIP_STRICT_FUNCTION},
      {"var1, var2 = var1", "", SKIP_STRICT_FUNCTION},
      {"var1, var2 = 4, ...var3", "", SKIP_STRICT_FUNCTION},

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
      {"var1, [var2], var3, [var4, var5], var6, {var7}, var8, {name9: var9, "
       "name10: var10}, ...var11",
       "", SKIP_STRICT_FUNCTION},

      // Destructuring rest. Because we can.
      {"var1, ...[var2]", "() => { }", SKIP_STRICT_FUNCTION},
      {"var1, ...[var2]", "() => { var2; }", SKIP_STRICT_FUNCTION},

      // Default parameters for destruring parameters.
      {"[var1 = 4, var2 = var1]", "", SKIP_STRICT_FUNCTION, false},
      {"{var1 = 4, var2 = var1}", "", SKIP_STRICT_FUNCTION, false},

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
       SKIP_STRICT_FUNCTION},

      // Hoisted sloppy block function shadowing a parameter.
      {"var1, var2", "for (;;) { function var1() { } }"},

      // Eval in default parameter.
      {"var1, var2 = eval(''), var3", "let var4 = 0;", SKIP_STRICT_FUNCTION},
      {"var1, var2 = eval(''), var3 = eval('')", "let var4 = 0;",
       SKIP_STRICT_FUNCTION},

      // FIXME(marja): arguments parameter
  };

  for (unsigned outer_ix = 0; outer_ix < arraysize(outers); ++outer_ix) {
    for (unsigned inner_ix = 0; inner_ix < arraysize(inners); ++inner_ix) {
      if (outers[outer_ix].strict_outer &&
          (inners[inner_ix].skip_in_strict_mode & SKIP_STRICT_OUTER)) {
        continue;
      }
      if (outers[outer_ix].strict_test_function &&
          (inners[inner_ix].skip_in_strict_mode & SKIP_STRICT_FUNCTION)) {
        continue;
      }

      const char* prefix = outers[outer_ix].prefix;
      const char* suffix = outers[outer_ix].suffix;
      int prefix_len = Utf8LengthHelper(prefix);
      int suffix_len = Utf8LengthHelper(suffix);

      // First compile with the lazy inner function and extract the scope data.
      const char* inner_function = outers[outer_ix].lazy_inner;
      int inner_function_len = Utf8LengthHelper(inner_function) - 4;

      int params_len = Utf8LengthHelper(inners[inner_ix].params);
      int source_len = Utf8LengthHelper(inners[inner_ix].source);
      int len = prefix_len + inner_function_len + params_len + source_len +
                suffix_len;

      i::ScopedVector<char> lazy_program(len + 1);
      i::SNPrintF(lazy_program, "%s", prefix);
      i::SNPrintF(lazy_program + prefix_len, inner_function,
                  inners[inner_ix].params, inners[inner_ix].source);
      i::SNPrintF(lazy_program + prefix_len + inner_function_len + params_len +
                      source_len,
                  "%s", suffix);

      i::Handle<i::String> source =
          factory->InternalizeUtf8String(lazy_program.start());
      source->PrintOn(stdout);
      printf("\n");

      i::Handle<i::Script> script = factory->NewScript(source);
      i::ParseInfo lazy_info(script);

      // No need to run scope analysis; preparser scope data is produced when
      // parsing.
      CHECK(i::parsing::ParseProgram(&lazy_info));

      // Then parse eagerly and check against the scope data.
      inner_function = outers[outer_ix].eager_inner;
      inner_function_len = Utf8LengthHelper(inner_function) - 4;
      len = prefix_len + inner_function_len + params_len + source_len +
            suffix_len;

      i::ScopedVector<char> eager_program(len + 1);
      i::SNPrintF(eager_program, "%s", prefix);
      i::SNPrintF(eager_program + prefix_len, inner_function,
                  inners[inner_ix].params, inners[inner_ix].source);
      i::SNPrintF(eager_program + prefix_len + inner_function_len + params_len +
                      source_len,
                  "%s", suffix);

      source = factory->InternalizeUtf8String(eager_program.start());
      source->PrintOn(stdout);
      printf("\n");

      script = factory->NewScript(source);
      i::ParseInfo eager_info(script);
      eager_info.set_allow_lazy_parsing(false);

      CHECK(i::parsing::ParseProgram(&eager_info));
      CHECK(i::Compiler::Analyze(&eager_info));

      i::Scope* scope =
          eager_info.literal()->scope()->inner_scope()->inner_scope();
      DCHECK_NOT_NULL(scope);
      DCHECK_NULL(scope->sibling());
      DCHECK(scope->is_function_scope());

      size_t index = 0;
      i::ScopeTestHelper::CompareScopeToData(
          scope, lazy_info.preparsed_scope_data(), index,
          inners[inner_ix].precise_maybe_assigned);
    }
  }
}
