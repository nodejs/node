// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Sequential string (1 byte string).
(function() {
  let s = "my string";
  function foo() {
    return s[3];
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals("s", foo());
  %OptimizeMaglevOnNextCall(foo);
  assertEquals("s", foo());
})();

// Sequential string (2 byte string).
(function() {
  let s = "my ⚡tring";
  function foo() {
    return s[3];
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals("⚡", foo());
  %OptimizeMaglevOnNextCall(foo);
  assertEquals("⚡", foo());
})();

// Cons string.
(function() {
  let s = "my"
  s += " ";
  s += "string";
  function foo() {
    return s[3];
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals("s", foo());
  %OptimizeMaglevOnNextCall(foo);
  assertEquals("s", foo());
})();

// Thin string.
(function() {
  let s = %ConstructThinString("my string is thin!");
  function foo() {
    return s[3];
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals("s", foo());
  %OptimizeMaglevOnNextCall(foo);
  assertEquals("s", foo());
})();

// Sliced string.
(function() {
  let main_string = "The quick brown fox jumps over the lazy dog.";
  let s = main_string.slice(4, 19);
  function foo() {
    return s[0];
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals("q", foo());
  %OptimizeMaglevOnNextCall(foo);
  assertEquals("q", foo());
})();
