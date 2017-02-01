// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(f) {
  assertFalse(f(0));
  assertFalse(f(Number.MIN_VALUE));
  assertFalse(f(Number.MAX_VALUE));
  assertFalse(f(Number.MIN_SAFE_INTEGER - 13));
  assertFalse(f(Number.MAX_SAFE_INTEGER + 23));
  assertTrue(f(Number.NaN));
  assertFalse(f(Number.POSITIVE_INFINITY));
  assertFalse(f(Number.NEGATIVE_INFINITY));
  assertFalse(f(Number.EPSILON));
  assertFalse(f(1 / 0));
  assertFalse(f(-1 / 0));
}

function f(x) {
  return Number.isNaN(+x);
}

test(f);
test(f);
%OptimizeFunctionOnNextCall(f);
test(f);
