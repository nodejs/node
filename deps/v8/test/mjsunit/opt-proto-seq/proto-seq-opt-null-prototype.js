// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_null_prototype() {
  function test_function() {}

  test_function.prototype = null;
  assertThrows(() => {
    test_function.prototype.func = function () {
      return "test_function.prototype.func";
    };
    test_function.prototype.arrow_func = () => {
      return "test_function.prototype.arrow_func";
    };
    test_function.prototype.smi = 1;
    test_function.prototype.str = "test_function.prototype.str";
    test_function.prototype.key = 1;

    let test_instance = new test_function();
    assertEquals(test_instance.func(), "test_function.prototype.func");
    assertEquals(
      test_instance.arrow_func(),
      "test_function.prototype.arrow_func",
    );
    assertEquals(test_instance.smi, 1);
    assertEquals(test_instance.str, "test_function.prototype.str");
    assertEquals(test_instance.key, 0);
  }, TypeError);
}

test_null_prototype();
%CompileBaseline(test_null_prototype);
test_null_prototype();
%PrepareFunctionForOptimization(test_null_prototype);
test_null_prototype();
test_null_prototype();
%OptimizeMaglevOnNextCall(test_null_prototype);
test_null_prototype();
assertOptimized(test_null_prototype);
assertTrue(isMaglevved(test_null_prototype));
test_null_prototype();
%OptimizeFunctionOnNextCall(test_null_prototype);
test_null_prototype();
assertOptimized(test_null_prototype);
test_null_prototype();
