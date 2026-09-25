// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_shadow_properties() {
  function F() {}
  // This might trigger slow path if we are not careful, or check
  // if it shadows object properties.
  // Assigning 'toString' which exists on Object.prototype.
  F.prototype.toString = function () {
    return 'F';
  };
  F.prototype.valueOf = function () {
    return 123;
  };
  return F;
}

function assert_test_shadow_properties(F) {
  const f = new F();
  assertEquals('F', f.toString());
  assertEquals(123, f.valueOf());
  assertEquals('F', F.prototype.toString());
}

// prettier-ignore
function run(){
assert_test_shadow_properties(test_shadow_properties());
%CompileBaseline(test_shadow_properties);
assert_test_shadow_properties(test_shadow_properties());
%PrepareFunctionForOptimization(test_shadow_properties);
assert_test_shadow_properties(test_shadow_properties());
assert_test_shadow_properties(test_shadow_properties());
%OptimizeMaglevOnNextCall(test_shadow_properties);
assert_test_shadow_properties(test_shadow_properties());
assertOptimized(test_shadow_properties);
assertTrue(isMaglevved(test_shadow_properties));
assert_test_shadow_properties(test_shadow_properties());
%OptimizeFunctionOnNextCall(test_shadow_properties);
assert_test_shadow_properties(test_shadow_properties());
assertOptimized(test_shadow_properties);
assert_test_shadow_properties(test_shadow_properties());
}

run();
