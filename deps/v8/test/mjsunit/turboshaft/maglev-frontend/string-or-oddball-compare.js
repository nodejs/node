// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbolev
// Flags: --no-always-use-string-forwarding-table

let internalized1234 = %ConstructInternalizedString("1234123412341234");
let nonInternalized1234 = %ConstructConsString("12341234", "12341234");
let uniqueId = 0;

function warmUpMaglevTestFn(test_fn_src) {
  // Create a fresh and uncached function
  test_fn_src = test_fn_src.toString();
  const pattern = '(x, y) {\n';
  assertTrue(test_fn_src.includes(pattern))
  parts = test_fn_src.split(pattern)
  assertEquals(parts.length, 2)
  let test_fn = new Function('x', 'y', `{/*${uniqueId++}*/\n${parts[1]}`);

  assertUnoptimized(test_fn);
  %PrepareFunctionForOptimization(test_fn);

  // Warm up with internalized strings and oddballs.
  assertEquals(1, test_fn("2", "2"));
  assertEquals(0, test_fn("2", null));
  assertEquals(1, test_fn(internalized1234, internalized1234));

  %OptimizeFunctionOnNextCall(test_fn);
  assertEquals(0, test_fn("1", "2"));
  assertOptimized(test_fn);
  return test_fn;
}

function test(test_fn_src) {
  assertEquals(internalized1234, nonInternalized1234);
  assertTrue(1234123412341234 == nonInternalized1234);
  assertTrue(%IsInternalizedString(internalized1234));
  assertFalse(%IsInternalizedString(nonInternalized1234));
  assertTrue(%IsInternalizedString(internalized1234));

  let test_fn = warmUpMaglevTestFn(test_fn_src);
  assertEquals(1, test_fn("1", "1"));
  assertOptimized(test_fn);

  assertEquals(0, test_fn(internalized1234, "1"));
  assertEquals(0, test_fn(undefined, "foo"));
  assertEquals(0, test_fn("foo", null));
  assertEquals(0, test_fn(false, "foo"));
  assertEquals(0, test_fn("foo", true));
  assertEquals(1, test_fn(undefined, undefined));
  assertEquals(1, test_fn(null, null));
  assertEquals(1, test_fn(true, true));
  assertEquals(1, test_fn(false, false));
  assertEquals(0, test_fn(undefined, null));
  assertEquals(0, test_fn(false, true));
  assertOptimized(test_fn);

  // The GC might have already migrated the thin string, create a new one
  let thin1234 = %ConstructThinString( "1234" + "1234" + "1234" + "1234");
  assertFalse(%IsInternalizedString(thin1234));

  assertEquals(1, test_fn(thin1234, "1234123412341234"));
  assertOptimized(test_fn);

  assertEquals(1, test_fn(thin1234, thin1234));
  assertOptimized(test_fn);

  assertEquals(1, test_fn(internalized1234, "1234123412341234"));
  assertOptimized(test_fn);

  // Passing something else than a string or oddball will cause a deopt.
  assertEquals(0, test_fn(internalized1234, 1234123412341234));
  assertUnoptimized(test_fn);

  // Non-internalized strings won't cause a deopt.
  test_fn = warmUpMaglevTestFn(test_fn_src);
  assertEquals(1, test_fn(nonInternalized1234, "1234123412341234"));
  assertOptimized(test_fn);
}

function strict_equals_fn(x, y) {
  if (x === y) {
    return 1;
  }
  return 0;
}
test(strict_equals_fn);

function strict_not_equals_fn(x, y) {
  if (x !== y) {
    return 0;
  }
  return 1;
}
test(strict_not_equals_fn);
