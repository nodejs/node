// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let re = /(?:[aA])*a/;

function test(str, expected) {
  let res1 = re.exec(str);
  assertEquals(expected ? expected : null, res1 ? Array.from(res1) : null);
  let res2 = re.exec(str);
  assertEquals(expected ? expected : null, res2 ? Array.from(res2) : null);
}

// 1. Matches at various lengths
test("a", ["a"]);
test("A", null);
test("Aa", ["Aa"]);
test("aA", ["a"]);

// Various positions in 16-byte boundary:
test("a", ["a"]);
test("xa", ["a"]);
test("xxxxxxxxxxxxxxxa", ["a"]);
test("xxxxxxxxxxxxxxxxa", ["a"]);
test("xxxxxxxxxxxxxxxxxa", ["a"]);
test("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxa", ["a"]);
test("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxa", ["a"]);

// Matches with multiple 'a'/'A' before:
test("aAaAaAaAaAaAaAaAa", ["aAaAaAaAaAaAaAaAa"]);
test("x" + "aA".repeat(10) + "a", ["aA".repeat(10) + "a"]);
test("x".repeat(100) + "aA".repeat(100) + "a", ["aA".repeat(100) + "a"]);

// No match:
test("", null);
test("x", null);
test("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", null);

// 2. UC16 (two-byte string) tests
test("\u1200a", ["a"]);
test("\u1200xa", ["a"]);
test("\u1200" + "x".repeat(15) + "a", ["a"]);
test("\u1200" + "x".repeat(16) + "a", ["a"]);
test("\u1200" + "x".repeat(17) + "a", ["a"]);
test("\u1200" + "aA".repeat(10) + "a", ["aA".repeat(10) + "a"]);
test("\u1200" + "x".repeat(100) + "aA".repeat(100) + "a", ["aA".repeat(100) + "a"]);

// UC16 no match:
test("\u1200", null);
test("\u1200x", null);
test("\u1200" + "x".repeat(100), null);
