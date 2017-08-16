// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-do-expressions

"use strict";

var p = {};
var testCases = [
  { s:"[1, do { _OSR_ 2 }, 3]",                      r:[1, 2, 3] },
  { s:"[1, ...[2], do { _OSR_ 3 }, 4]",              r:[1, 2, 3, 4] },
  { s:"[1, ...do { _OSR_ [2,3] }, 4]",               r:[1, 2, 3, 4] },
  { s:"{ a:do { _OSR_ 1 } }",                        r:{ a:1 } },
  { s:"{ a:do { _OSR_ 2 }, __proto__:p }",           r:{ a:2, __proto__:p } },
  { s:"{ a:do { _OSR_ 3 }, get b() { return 4; } }", r:{ a:3, b:4 } },
  { s:"{ [do { _OSR_ 'b' }]: 3 }",                   r:{ b:3 } },
  { s:"{ [do { _OSR_ 'b' }]: 3, c: 4 }",             r:{ b:3, c:4 } },
  { s:"{ [do { _OSR_ 'b' }]: 3, __proto__:p }",      r:{ b:3, __proto__:p } },
  { s:"{ get [do { _OSR_ 'c' }]() { return 4; } }",  r:{ c:4 } },
  { s:"class { [do { _OSR_ 'f' }]() {} }" },
  { s:"class { [do { _OSR_ 'f' }]() {}; g() {} }" },
];

for (var i = 0; i < testCases.length; ++i) {
  var source = "(function f" + i + "(x) { return " + testCases[i].s + "})";
  var osr = "for (var i = 0; i < 10; i++) { if (i == 5) %OptimizeOsr(); }";
  var result = eval(source.replace("_OSR_", osr))();
  if (testCases[i].r) assertEquals(testCases[i].r, result);
}
