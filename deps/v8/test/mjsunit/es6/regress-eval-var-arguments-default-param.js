// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Direct eval declaring `var arguments` or `function arguments` in a
// non-simple parameter default initializer must throw a SyntaxError.

assertThrows(() => {
  function f(a = eval("var arguments = 42")) {
    return arguments;
  }
  f();
}, SyntaxError);

assertThrows(() => {
  function f(a = eval("var arguments")) {
    return arguments;
  }
  f();
}, SyntaxError);

assertThrows(() => {
  function f(a = eval("function arguments() {}")) {
    return arguments;
  }
  f();
}, SyntaxError);

assertThrows(() => {
  function f(a = { Let: eval("var arguments = 42") }) {
    return arguments;
  }
  f();
}, SyntaxError);

assertThrows(() => {
  function f(a = eval("var arguments")) {}
  f();
}, SyntaxError);

assertThrows(() => {
  function f(a = eval("var arguments")) {
    var arguments;
  }
  f();
}, SyntaxError);

assertThrows(() => {
  function f(a = eval("var arguments")) {
    let arguments;
  }
  f();
}, SyntaxError);

assertThrows(() => {
  function f(a = eval("var arguments")) {
    function arguments() {}
  }
  f();
}, SyntaxError);

// Direct eval in the function body with non-simple parameters is still allowed
// to declare `var arguments`.
(function() {
  function f(a = 1) {
    eval("var arguments = 42");
    return arguments;
  }
  assertEquals(42, f());
})();

// Direct eval in the function body with simple parameters is still allowed
// to declare `var arguments`.
(function() {
  function f() {
    eval("var arguments = 42");
    return arguments;
  }
  assertEquals(42, f());
})();

// Direct eval inside an inner function in a default parameter initializer
// declares `var arguments` in the inner function's scope and does not conflict
// with the outer function's `arguments`.
(function() {
  function f(a = (() => eval("var arguments = 42; arguments"))()) {
    return [a, arguments[1]];
  }
  assertEquals([42, 10], f(undefined, 10));
})();

(function() {
  function f(a = (function() { return eval("var arguments = 42; arguments"); })()) {
    return [a, arguments[1]];
  }
  assertEquals([42, 10], f(undefined, 10));
})();

// Direct eval in an arrow function's default parameter initializer hoists to
// the enclosing non-arrow function's body scope if allowed.
(function() {
  function outer() {
    let arrow = (a = eval("var arguments = 42")) => arguments;
    return arrow();
  }
  assertEquals(42, outer());
})();
