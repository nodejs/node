// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function regressionCaseOne() {
  var c;
  for (let [a, b = c = function() { return a + b }] of [[0]]) {
    function f() { return a };
  }
  c();
})();

(function testForInFunction() {
  for (const {length: a, b = function() { return a, b }} in {foo: 42}) {
    assertSame(b, (function() { return b() })());
  }
})();

(function testForOfFunction() {
  for (const [a, b = function() { return a, b }] of [[42]]) {
    assertSame(b, (function() { return b() })());
  }
})();

(function testForInVariableProxy() {
  for (const {length: a, b = a} in {foo: 42}) {
    assertEquals(3, a);
    assertEquals(a, b);
  }
})();

(function testForOfVariableProxy() {
  for (const [a, b = a] of [[42]]) {
    assertEquals(42, a);
    assertEquals(a, b);
  }
})();
