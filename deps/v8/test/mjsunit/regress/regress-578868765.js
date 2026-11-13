// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api

const fast_c_api = new d8.test.FastCAPI();

(function testDirectCallerCatches() {
  function func(val) {
    try {
      fast_c_api.call_to_number(val);
    } catch (e) {
      return e;
    }
  }

  %PrepareFunctionForOptimization(func);
  func(1);
  func(1);
  %OptimizeFunctionOnNextCall(func);
  func(1);

  const throwing_deopting_arg = {
    valueOf() {
      %DeoptimizeFunction(func);
      throw "expected_exception";
    }
  };

  assertEquals("expected_exception", func(throwing_deopting_arg));
})();

(function testInlinedCallerCatchesInOuter() {
  function inner(val) {
    fast_c_api.call_to_number(val);
  }
  function outer(val) {
    try {
      inner(val);
    } catch (e) {
      return e;
    }
  }

  %PrepareFunctionForOptimization(inner);
  %PrepareFunctionForOptimization(outer);
  outer(1);
  outer(1);
  %OptimizeFunctionOnNextCall(outer);
  outer(1);

  const throwing_deopting_arg = {
    valueOf() {
      %DeoptimizeFunction(outer);
      throw "expected_exception_outer";
    }
  };

  assertEquals("expected_exception_outer", outer(throwing_deopting_arg));
})();

(function testInlinedCallerCatchesInInner() {
  function inner(val) {
    try {
      fast_c_api.call_to_number(val);
    } catch (e) {
      return e;
    }
  }
  function outer(val) {
    return inner(val);
  }

  %PrepareFunctionForOptimization(inner);
  %PrepareFunctionForOptimization(outer);
  outer(1);
  outer(1);
  %OptimizeFunctionOnNextCall(outer);
  outer(1);

  const throwing_deopting_arg = {
    valueOf() {
      %DeoptimizeFunction(outer);
      throw "expected_exception_inner";
    }
  };

  assertEquals("expected_exception_inner", outer(throwing_deopting_arg));
})();
