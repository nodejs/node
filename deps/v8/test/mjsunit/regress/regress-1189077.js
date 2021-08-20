// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const string_max_length = %StringMaxLength();
const longest_double = -2.2250738585105353E-308;
const s18 = "A".repeat(string_max_length - 18);
const s23 = "A".repeat(string_max_length - 23);
const s24 = "A".repeat(string_max_length - 24);
const s25 = "A".repeat(string_max_length - 25);

(function() {
  function f() {
    return s18 + longest_double;
  }

  %PrepareFunctionForOptimization(f);
  assertThrows(f, RangeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, RangeError);
})();

(function() {
  function f() {
    return s23 + longest_double;
  }

  %PrepareFunctionForOptimization(f);
  assertThrows(f, RangeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, RangeError);
})();

(function() {
  function f() {
    return s24 + longest_double;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(string_max_length, f().length);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(string_max_length, f().length);
})();

(function() {
  function f() {
    return s25 + longest_double;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(string_max_length - 1, f().length);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(string_max_length - 1, f().length);
})();
