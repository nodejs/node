// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Optional chains ending in a private field (`a?.#b`) are not valid
// LeftHandSideExpressions for assignment, update expressions, destructuring,
// or for-in/of targets, including when lazy-parsed by PreParser.

const cases = [
  "class C { #b; m(a) { a?.#b = 1; } }",
  "class C { #b; m(a) { a?.#b += 1; } }",
  "class C { #b; m(a) { ++a?.#b; } }",
  "class C { #b; m(a) { a?.#b++; } }",
  "class C { #b; m(a) { [a?.#b] = []; } }",
  "class C { #b; m(a) { ({x: a?.#b} = {}); } }",
  "class C { #b; m(a) { [...a?.#b] = []; } }",
  "class C { #b; m(a) { ({...a?.#b} = {}); } }",
  "class C { #b; m(a) { let [a?.#b] = []; } }",
  "class C { #b; m(a) { for (a?.#b of []) {} } }",
  "class C { #b; m(a) { for (a?.#b in {}) {} } }",
];

for (const code of cases) {
  assertThrows(() => eval(code), SyntaxError);
  assertThrows(() => eval("(" + code + ")"), SyntaxError);
}
