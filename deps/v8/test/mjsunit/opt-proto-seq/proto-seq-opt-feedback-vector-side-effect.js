// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_feedback_vector_side_effect() {
  function outer() {
    function Class() {}
    Class.prototype.key_1 = function inner() {
      function tagged_template_literal(x) {
        return x;
      }
      return tagged_template_literal`abc`;
    };
    Class.prototype.key_2 = 1;
    Class.prototype.key_3 = 1;
    return new Class();
  }

  let inner_1 = outer();
  let inner_2 = outer();
  return [inner_1, inner_2];
}

function assert_test_feedback_vector_side_effect(inners) {
  assertEquals(inners[0].key_1(), inners[1].key_1());
}

// prettier-ignore
function run(){
assert_test_feedback_vector_side_effect(test_feedback_vector_side_effect());
%CompileBaseline(test_feedback_vector_side_effect);
assert_test_feedback_vector_side_effect(test_feedback_vector_side_effect());
%PrepareFunctionForOptimization(test_feedback_vector_side_effect);
assert_test_feedback_vector_side_effect(test_feedback_vector_side_effect());
assert_test_feedback_vector_side_effect(test_feedback_vector_side_effect());
%OptimizeMaglevOnNextCall(test_feedback_vector_side_effect);
assert_test_feedback_vector_side_effect(test_feedback_vector_side_effect());
assertOptimized(test_feedback_vector_side_effect);
assertTrue(isMaglevved(test_feedback_vector_side_effect));
assert_test_feedback_vector_side_effect(test_feedback_vector_side_effect());
%OptimizeFunctionOnNextCall(test_feedback_vector_side_effect);
assert_test_feedback_vector_side_effect(test_feedback_vector_side_effect());
assertOptimized(test_feedback_vector_side_effect);
assert_test_feedback_vector_side_effect(test_feedback_vector_side_effect());
}

run();
