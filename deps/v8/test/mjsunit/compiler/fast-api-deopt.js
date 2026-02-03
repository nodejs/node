// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax --expose-fast-api --turbofan

const fast_c_api = new d8.test.FastCAPI();

(function deoptWithoutException() {
  print(arguments.callee.name);
  let val = -42;
  function func() {
    const ret = fast_c_api.call_to_number(val);
    assertEquals(undefined, ret);
    // Do a second fast API call. If deoptimization was triggered in the call to
    // `call_to_number`, then this second call should be a regular call.
    return fast_c_api.add_32bit_int(12, 14);
  }
  fast_c_api.reset_counts();
  %PrepareFunctionForOptimization(func);
  func();
  // `func` is unoptimized, so there should not be fast calls.
  assertEquals(0, fast_c_api.fast_call_count());
  assertEquals(2, fast_c_api.slow_call_count());
  func();
  %OptimizeFunctionOnNextCall(func);
  fast_c_api.reset_counts();
  func();
  // `func is now optimized
  assertEquals(2, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
  // `call_to_number` converts `val` to a number. ToNumber
  // conversion means for objects that the `valueOf` method gets executed, which
  // allows us to execute arbirarty JS code, and in this test here, it allows us
  // to trigger deoptimization of `func`.
  val = {
    valueOf: () => {
      %DeoptimizeFunction(func);
      return -42;
    }
  };
  fast_c_api.reset_counts();
  func();
  // `func` starts execution as optimized and therefore calls `call_to_number`
  // with a fast call. However, `func` gets deoptimized once `call_to_number`
  // returns, and thereafter, `add_32bit_int gets called from a deoptimised
  // `func` with a regular call.
  assertEquals(1, fast_c_api.fast_call_count());
  assertEquals(1, fast_c_api.slow_call_count());
  assertUnoptimized(func);
})();

(function deoptWithExceptionCaughtInFastCaller() {
  print(arguments.callee.name);
  let val = -42;
  let exceptionHappened = false;
  function func() {
    try {
      const ret = fast_c_api.call_to_number(val);
      assertEquals(undefined, ret);
    } catch (e) {
      exceptionHappened = true;
    }
    // Do a second fast API call. If deoptimization was triggered in the call to
    // `call_to_number`, then this second call should be a regular call.
    return fast_c_api.add_32bit_int(12, 14);
  }
  fast_c_api.reset_counts();
  %PrepareFunctionForOptimization(func);
  func();
  // `func` is unoptimized, so there should not be fast calls.
  assertEquals(0, fast_c_api.fast_call_count());
  assertEquals(2, fast_c_api.slow_call_count());
  func();
  %OptimizeFunctionOnNextCall(func);
  fast_c_api.reset_counts();
  func();
  // `func is now optimized
  assertEquals(2, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
  assertFalse(exceptionHappened);
  // `call_to_number` converts all fields in `val` to numbers. ToNumber
  // conversion means for objects that the `valueOf` method gets executed, which
  // allows us to execute arbirarty JS code, and in this test here, it allows us
  // to trigger deoptimization of `func`.
  val = {
    valueOf: () => {
      %DeoptimizeFunction(func);
      throw -42;
    }
  };
  fast_c_api.reset_counts();
  func();
  // `func` starts execution as optimized and therefore calls `call_to_number`
  // with a fast call. However, `func` gets deoptimized once `call_to_number`
  // returns, and thereafter, `add_32bit_int gets called from a deoptimised
  // `func` with a regular call.
  assertEquals(1, fast_c_api.fast_call_count());
  assertEquals(1, fast_c_api.slow_call_count());
  assertUnoptimized(func);
  assertTrue(exceptionHappened);
})();

(function deoptWithExceptionCaughtInFastCallerSecondCall() {
  print(arguments.callee.name);
  let val = -42;
  let exceptionHappened = false;
  function func() {
    // Another fast call so that throwing fast call is not the first one.
    fast_c_api.add_32bit_int(12, 14);
    try {
      const ret = fast_c_api.call_to_number(val);
      assertEquals(undefined, ret);
    } catch (e) {
      exceptionHappened = true;
    }
    // Do a second fast API call. If deoptimization was triggered in the call to
    // `call_to_number`, then this second call should be a regular call.
    return fast_c_api.add_32bit_int(12, 14);
  }
  fast_c_api.reset_counts();
  %PrepareFunctionForOptimization(func);
  func();
  // `func` is unoptimized, so there should not be fast calls.
  assertEquals(0, fast_c_api.fast_call_count());
  assertEquals(3, fast_c_api.slow_call_count());
  func();
  %OptimizeFunctionOnNextCall(func);
  fast_c_api.reset_counts();
  func();
  // `func is now optimized
  assertEquals(3, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
  assertFalse(exceptionHappened);
  // `call_to_number` converts all fields in `val` to numbers. ToNumber
  // conversion means for objects that the `valueOf` method gets executed, which
  // allows us to execute arbirarty JS code, and in this test here, it allows us
  // to trigger deoptimization of `func`.
  val = {
    valueOf: () => {
      %DeoptimizeFunction(func);
      throw -42;
    }
  };
  fast_c_api.reset_counts();
  func();
  // `func` starts execution as optimized and therefore calls `call_to_number`
  // with a fast call. However, `func` gets deoptimized once `call_to_number`
  // returns, and thereafter, `add_32bit_int gets called from a deoptimised
  // `func` with a regular call.
  assertEquals(2, fast_c_api.fast_call_count());
  assertEquals(1, fast_c_api.slow_call_count());
  assertUnoptimized(func);
  assertTrue(exceptionHappened);
})();

(function deoptWithExceptionWithOuterCatcher() {
  print(arguments.callee.name);
  let val = -42;
  let exceptionHappened = false;
  function func() {
    const ret = fast_c_api.call_to_number(val);
    assertEquals(undefined, ret);
    // Do a second fast API call. If deoptimization was triggered in the call to
    // `call_to_number`, then this second call should be a regular call.
    return fast_c_api.add_32bit_int(12, 14);
  }
  fast_c_api.reset_counts();
  %PrepareFunctionForOptimization(func);
  func();
  // `func` is unoptimized, so there should not be fast calls.
  assertEquals(0, fast_c_api.fast_call_count());
  assertEquals(2, fast_c_api.slow_call_count());
  func();
  %OptimizeFunctionOnNextCall(func);
  fast_c_api.reset_counts();
  func();
  // `func is now optimized
  assertEquals(2, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
  assertFalse(exceptionHappened);
  // `call_to_number` converts all fields in `val` to numbers. ToNumber
  // conversion means for objects that the `valueOf` method gets executed, which
  // allows us to execute arbirarty JS code, and in this test here, it allows us
  // to trigger deoptimization of `func`.
  val = {
    valueOf: () => {
      %DeoptimizeFunction(func);
      throw -42;
    }
  };
  fast_c_api.reset_counts();
  try {
    func();
  } catch(e) {
    exceptionHappened = true;
  }
  // `func` starts execution as optimized and therefore calls `call_to_number`
  // with a fast call. However, `func` gets deoptimized once `call_to_number`
  // returns, and thereafter, `add_32bit_int gets called from a deoptimised
  // `func` with a regular call.
  assertEquals(1, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
  assertUnoptimized(func);
  assertTrue(exceptionHappened);
})();

(function deoptWithoutExceptionNested() {
  print(arguments.callee.name);
  // In this test, the function `outer` calls to C++, which calls back to
  // JavaScript to call the function `inner`, which again calls to C++. The
  // calls to C++ can be fast API calls. Once both `inner` and `outer` are
  // optimized, the C++ function called by `inner` triggers the deoptimization
  // of `outer`.
  let innerVal = 12;
  function inner() {
    return fast_c_api.call_to_number(innerVal);
  }
  %PrepareFunctionForOptimization(inner);
  inner();
  inner();
  %OptimizeFunctionOnNextCall(inner);
  inner();
  function outer() {
    const val = {
      valueOf: () => {
        inner();
        return -42;
      }
    };
    const ret = fast_c_api.call_to_number(val);
    assertEquals(undefined, ret);
    // Do a second fast API call. If deoptimization was triggered in the call to
    // `call_to_number`, then this second call should be a regular call.
    return fast_c_api.add_32bit_int(12, 14);
  }
  %PrepareFunctionForOptimization(outer);
  fast_c_api.reset_counts();
  outer();
  // `inner` has already been optimized and therefore does fast calls.
  assertEquals(1, fast_c_api.fast_call_count());
  // 'outer' is still unoptimized and therefore uses regular calls for both API
  // calls.
  assertEquals(2, fast_c_api.slow_call_count());
  outer();
  %OptimizeFunctionOnNextCall(outer);
  fast_c_api.reset_counts();
  outer();
  assertOptimized(inner);
  assertOptimized(outer);
  // Both `inner` and `outer` are optimized now, so all calls should be fast
  // calls.
  assertEquals(3, fast_c_api.fast_call_count());
  assertEquals(0, fast_c_api.slow_call_count());
  innerVal = {
    valueOf: () => {
      %DeoptimizeFunction(outer);
      return 12;
    }
  };
  fast_c_api.reset_counts();
  outer();
  assertOptimized(inner);
  assertUnoptimized(outer);
  // The API function called by `inner` triggered the deoptimization of `outer`
  // after its first API call. Therefore the API call in `inner`, and the first
  // API call of `outer` should be fast calls, whereas the second API call in
  // `outer` happens after deoptimization and is therefore a regular API call.
  assertEquals(2, fast_c_api.fast_call_count());
  assertEquals(1, fast_c_api.slow_call_count());
})();
