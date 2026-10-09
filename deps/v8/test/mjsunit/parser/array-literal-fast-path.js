// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testParsing(run_expr, run_stmt) {
  // Correct values and holes
  assertEquals([1, 2, , 3], run_expr("[1, 2, , 3]"));
  assertEquals([, 1], run_expr("[, 1]"));
  assertEquals([1, ], run_expr("[1, ]"));
  assertEquals([null, true, false, "s", 1.5, 0x10],
               run_expr('[null, true, false, "s", 1.5, 0x10]'));
  assertEquals([1n, 16n, 18446744073709551616n],
               run_expr('[1n, 0x10n, 18446744073709551616n]'));
  assertEquals([1, 2], run_expr('[1/*]*/,2]'));
  assertEquals([1, 2], run_expr('[1//comment\n,2]'));
  assertEquals([1], run_expr('[1\u00a0]'));
  assertEquals([3], run_expr('[1/*]*/+2]'));

  // Syntax errors
  assertThrows(() => run_expr("[1] = x"), SyntaxError,
               "Invalid destructuring assignment target");
  assertThrows(() => run_expr("[...1] = x"), SyntaxError,
               "Invalid destructuring assignment target");
  assertThrows(() => run_expr("[1, 2] = x"), SyntaxError,
               "Invalid destructuring assignment target");
  assertThrows(() => run_expr("[1n] = x"), SyntaxError,
               "Invalid destructuring assignment target");
  assertThrows(() => run_expr("([1n]) => 0"), SyntaxError,
               "Invalid destructuring assignment target");
  assertThrows(() => run_expr('[1, "s".x] = []'), SyntaxError,
               "Invalid destructuring assignment target");
  assertThrows(() => run_expr('[1, ...[], 2] = []'), SyntaxError,
               "Invalid destructuring assignment target");
  assertThrows(() => run_expr("([1]) => 0"), SyntaxError,
               "Invalid destructuring assignment target");
  assertThrows(() => run_stmt("for ([1] of []) {}"), SyntaxError,
               "Invalid destructuring assignment target");

  // Keep existing behavior
  assertEquals([3], run_expr("[1 + 2]"));
  assertThrows(() => run_expr("[1 2]"), SyntaxError);
  assertEquals([false], run_stmt('var o = {}; return ["a" in o];'));
  assertEquals([2], run_expr("[1 ? 2 : 3]"));
  assertThrows(() => run_expr("[1.toString]"), SyntaxError,
               "Invalid or unexpected token");
  assertEquals(["1"], run_expr("[1..toString()]"));
  assertEquals(["1"], run_expr("[1n.toString()]"));
  assertEquals([1], run_expr('["s".length]'));
  assertEquals([1], run_expr('["s"?.length]'));
  assertEquals(["s"], run_expr('["s"[0]]'));
  assertEquals([undefined], run_expr('[null?.x]'));
  assertEquals([8, 2n], run_expr('[2 ** 3, 1n + 1n]'));
  assertEquals([2, true, 3], run_expr('[1 && 2, true || false, null ?? 3]'));
  assertEquals([true], run_expr('["x" in {x: 1}]'));
  assertThrows(() => run_expr('[null ?? true || false]'), SyntaxError);
  assertThrows(() => run_expr("[1 = 2]"), SyntaxError,
               "Invalid left-hand side in assignment");

  // Legacy octal literals
  assertEquals([1, 8], run_expr("[01, 08]"));
  assertThrows(() => run_stmt("'use strict'; [01, 08]"), SyntaxError,
               "Decimals with leading zeros are not allowed in strict mode.");
  assertThrows(() => run_stmt("'use strict'; [08]"), SyntaxError,
               "Decimals with leading zeros are not allowed in strict mode.");
}

// Eagerly parsed
testParsing(
  src => eval("(" + src + ")"),
  src => {
    let f;
    eval("f = function() { " + src + " }");
    return f();
  }
);

// Lazily compiled (preparser path)
testParsing(
  src => {
    let f;
    eval("f = function() { return (" + src + "); }");
    return f();
  },
  src => {
    let f;
    eval("f = function() { " + src + " }");
    return f();
  }
);
