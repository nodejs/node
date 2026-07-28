// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

// Test that keyed loads and stores with primitive keys (undefined, null, true, false)
// can be optimized by Turbofan and Maglev, and do not deoptimize when
// passing the primitive or the corresponding string.

const obj = {
  "undefined": 1,
  "null": 2,
  "true": 3,
  "false": 4,
  "foo": 5
};

// ==================== LOADS ====================

// Test undefined load
{
  function get_undefined(obj, key) {
    return obj[key];
  }
  %PrepareFunctionForOptimization(get_undefined);
  get_undefined(obj, undefined);
  get_undefined(obj, undefined);
  %OptimizeFunctionOnNextCall(get_undefined);
  assertEquals(1, get_undefined(obj, undefined));
  assertOptimized(get_undefined);

  // String key should also work without deopt
  assertEquals(1, get_undefined(obj, "undefined"));
  assertOptimized(get_undefined);

  // Different key should deopt
  assertEquals(2, get_undefined(obj, null));
  assertUnoptimized(get_undefined);
}

// Test null load
{
  function get_null(obj, key) {
    return obj[key];
  }
  %PrepareFunctionForOptimization(get_null);
  get_null(obj, null);
  get_null(obj, null);
  %OptimizeFunctionOnNextCall(get_null);
  assertEquals(2, get_null(obj, null));
  assertOptimized(get_null);

  // String key should also work without deopt
  assertEquals(2, get_null(obj, "null"));
  assertOptimized(get_null);

  // Different key should deopt
  assertEquals(1, get_null(obj, undefined));
  assertUnoptimized(get_null);
}

// Test true load
{
  function get_true(obj, key) {
    return obj[key];
  }
  %PrepareFunctionForOptimization(get_true);
  get_true(obj, true);
  get_true(obj, true);
  %OptimizeFunctionOnNextCall(get_true);
  assertEquals(3, get_true(obj, true));
  assertOptimized(get_true);

  // String key should also work without deopt
  assertEquals(3, get_true(obj, "true"));
  assertOptimized(get_true);

  // Different key should deopt
  assertEquals(4, get_true(obj, false));
  assertUnoptimized(get_true);
}

// Test false load
{
  function get_false(obj, key) {
    return obj[key];
  }
  %PrepareFunctionForOptimization(get_false);
  get_false(obj, false);
  get_false(obj, false);
  %OptimizeFunctionOnNextCall(get_false);
  assertEquals(4, get_false(obj, false));
  assertOptimized(get_false);

  // String key should also work without deopt
  assertEquals(4, get_false(obj, "false"));
  assertOptimized(get_false);

  // Different key should deopt
  assertEquals(3, get_false(obj, true));
  assertUnoptimized(get_false);
}

// ==================== STORES ====================

// Test undefined store
{
  function set_undefined(obj, key, val) {
    obj[key] = val;
  }
  const test_obj = { ...obj };
  %PrepareFunctionForOptimization(set_undefined);
  set_undefined(test_obj, undefined, 10);
  set_undefined(test_obj, undefined, 11);
  %OptimizeFunctionOnNextCall(set_undefined);
  set_undefined(test_obj, undefined, 12);
  assertEquals(12, test_obj["undefined"]);
  assertOptimized(set_undefined);

  // String key should also work without deopt
  set_undefined(test_obj, "undefined", 13);
  assertEquals(13, test_obj["undefined"]);
  assertOptimized(set_undefined);

  // Different key should deopt
  set_undefined(test_obj, null, 14);
  assertEquals(14, test_obj["null"]);
  assertUnoptimized(set_undefined);
}

// Test null store
{
  function set_null(obj, key, val) {
    obj[key] = val;
  }
  const test_obj = { ...obj };
  %PrepareFunctionForOptimization(set_null);
  set_null(test_obj, null, 20);
  set_null(test_obj, null, 21);
  %OptimizeFunctionOnNextCall(set_null);
  set_null(test_obj, null, 22);
  assertEquals(22, test_obj["null"]);
  assertOptimized(set_null);

  // String key should also work without deopt
  set_null(test_obj, "null", 23);
  assertEquals(23, test_obj["null"]);
  assertOptimized(set_null);

  // Different key should deopt
  set_null(test_obj, undefined, 24);
  assertEquals(24, test_obj["undefined"]);
  assertUnoptimized(set_null);
}

// Test true store
{
  function set_true(obj, key, val) {
    obj[key] = val;
  }
  const test_obj = { ...obj };
  %PrepareFunctionForOptimization(set_true);
  set_true(test_obj, true, 30);
  set_true(test_obj, true, 31);
  %OptimizeFunctionOnNextCall(set_true);
  set_true(test_obj, true, 32);
  assertEquals(32, test_obj["true"]);
  assertOptimized(set_true);

  // String key should also work without deopt
  set_true(test_obj, "true", 33);
  assertEquals(33, test_obj["true"]);
  assertOptimized(set_true);

  // Different key should deopt
  set_true(test_obj, false, 34);
  assertEquals(34, test_obj["false"]);
  assertUnoptimized(set_true);
}

// Test false store
{
  function set_false(obj, key, val) {
    obj[key] = val;
  }
  const test_obj = { ...obj };
  %PrepareFunctionForOptimization(set_false);
  set_false(test_obj, false, 40);
  set_false(test_obj, false, 41);
  %OptimizeFunctionOnNextCall(set_false);
  set_false(test_obj, false, 42);
  assertEquals(42, test_obj["false"]);
  assertOptimized(set_false);

  // String key should also work without deopt
  set_false(test_obj, "false", 43);
  assertEquals(43, test_obj["false"]);
  assertOptimized(set_false);

  // Different key should deopt
  set_false(test_obj, true, 44);
  assertEquals(44, test_obj["true"]);
  assertUnoptimized(set_false);
}
