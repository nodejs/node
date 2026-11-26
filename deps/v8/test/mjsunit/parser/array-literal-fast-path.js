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

  for (const element of ['0', '1', '2147483648', '999999999999999', 'null',
                          '1000000000000000', '01', '08', '077', '0x10', '0b10',
                          '0o10', '1n', '1.5', '1e2', '1_000', '-1', '+1',
                          'null?.x', 'null ?? 3', '1/*x*/', '1//x\n']) {
    const expected = [, 0, null, eval(element), 2];
    assertEquals(expected, run_expr('[,0,null,' + element + ',2]'));
  }
  assertEquals([0, null, 2], run_expr('[0,null,2,]'));
  assertEquals([0, null, , 2], run_expr('[0,null,,2]'));
  assertEquals([0, null, 2], run_expr('[...[],0,null,2]'));
  assertEquals([0, null, 2], run_expr('[0,null,/*x*/2]'));
  assertEquals([0, null, '\u03bb'], run_expr('[0,null,"\u03bb"]'));
  const longSource = '[0,' + '123456789012345,null,'.repeat(1000) + '2]';
  const longArray = run_expr(longSource);
  assertEquals(2002, longArray.length);
  for (let i = 1; i < 2001; i += 2) {
    assertEquals(123456789012345, longArray[i]);
    assertNull(longArray[i + 1]);
  }
  for (const source of ['[,0,null,x] = []', '([,0,null,x]) => 0',
                         '([,null,0,x]) => 0']) {
    assertThrows(() => run_expr(source), SyntaxError,
                 'Invalid destructuring assignment target');
  }
  assertThrows(() => run_stmt('for ([,0,null,x] of []) {}'), SyntaxError,
               'Invalid destructuring assignment target');
  assertThrows(() => run_expr('[...[],0,null,x] = []'), SyntaxError,
               'Rest element must be last element');
  for (const suffix of ['1e+', '1foo', '1__0', '0nfoo']) {
    assertThrows(() => run_expr('[0,null,' + suffix + ',2]'), SyntaxError);
  }
  assertThrows(() => run_stmt("'use strict'; [0,null,01,2]"), SyntaxError);
  assertThrows(() => run_stmt("'use strict'; [0,null,08,2]"), SyntaxError);

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
