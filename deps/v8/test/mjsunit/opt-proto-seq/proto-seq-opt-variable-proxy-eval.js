// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_variable_proxy_eval() {
  var foo = function () {};
  (function inner_test() {
    eval("var foo = { prototype: { set k1(x) { calls += 1;foo = {}} }}");
    var calls = 0;
    assertThrows(() => {
      foo.prototype.k1 = 1;
      foo.prototype.k2 = 2;
    }, TypeError);
    assertEquals(calls, 1);
    assertEquals(Object.keys(foo).length, 0);
  })();
}

function assert_test_variable_proxy_eval(foo) {}

// prettier-ignore
function run(){
assert_test_variable_proxy_eval(test_variable_proxy_eval());
%CompileBaseline(test_variable_proxy_eval);
assert_test_variable_proxy_eval(test_variable_proxy_eval());
%PrepareFunctionForOptimization(test_variable_proxy_eval);
assert_test_variable_proxy_eval(test_variable_proxy_eval());
assert_test_variable_proxy_eval(test_variable_proxy_eval());
%OptimizeMaglevOnNextCall(test_variable_proxy_eval);
assert_test_variable_proxy_eval(test_variable_proxy_eval());
assertOptimized(test_variable_proxy_eval);
assertTrue(isMaglevved(test_variable_proxy_eval));
assert_test_variable_proxy_eval(test_variable_proxy_eval());
%OptimizeFunctionOnNextCall(test_variable_proxy_eval);
assert_test_variable_proxy_eval(test_variable_proxy_eval());
assertOptimized(test_variable_proxy_eval);
assert_test_variable_proxy_eval(test_variable_proxy_eval());
}

run();
