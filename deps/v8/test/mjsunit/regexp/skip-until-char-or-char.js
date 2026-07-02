// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This regexp should trigger SkipUntilCharOrChar (peephole optimization
// for looping while not 'a' and not 'c').
// 'a' and 'c' are non-contiguous, so they cannot be optimized to a range check.
let re = /[^ac]*a/;

function test(str, expected) {
  let res1 = re.exec(str);
  assertEquals(expected ? expected : null, res1 ? Array.from(res1) : null);
  let res2 = re.exec(str);
  assertEquals(expected ? expected : null, res2 ? Array.from(res2) : null);
}

// 1. Matches at various lengths
test("a", ["a"]);
test("b", null);
test("c", null);
test("ca", ["a"]);
test("ba", ["ba"]);
test("ac", ["a"]);
test("x", null);
test("xa", ["xa"]);
test("xca", ["a"]);
test("xba", ["xba"]);

// Various positions in 16-byte boundary:
test("a", ["a"]);
test("xa", ["xa"]);
test("xxxxxxxxxxxxxxxa", ["xxxxxxxxxxxxxxxa"]);
test("xxxxxxxxxxxxxxxxa", ["xxxxxxxxxxxxxxxxa"]);
test("xxxxxxxxxxxxxxxxxa", ["xxxxxxxxxxxxxxxxxa"]);
test("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxa", ["xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxa"]);
test("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxa", ["xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxa"]);

// Matches with multiple 'a'/'b'/'c' before:
test("x" + "xx".repeat(10) + "a", ["x" + "xx".repeat(10) + "a"]);
test("x".repeat(100) + "a", ["x".repeat(100) + "a"]);

// No match:
test("", null);
test("x", null);
test("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", null);
test("xxxxxxxBxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", null);

// 2. UC16 (two-byte string) tests
test("\u1200a", ["\u1200a"]);
test("\u1200xa", ["\u1200xa"]);
test("\u1200" + "x".repeat(15) + "a", ["\u1200" + "x".repeat(15) + "a"]);
test("\u1200" + "x".repeat(16) + "a", ["\u1200" + "x".repeat(16) + "a"]);
test("\u1200" + "x".repeat(17) + "a", ["\u1200" + "x".repeat(17) + "a"]);

// UC16 no match:
test("\u1200", null);
test("\u1200x", null);
test("\u1200" + "x".repeat(100), null);
