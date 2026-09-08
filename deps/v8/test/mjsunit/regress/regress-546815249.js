// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --no-lazy-feedback-allocation

// Math.atanh(42) is a signalling NaN, so folding it hands the double array store
// a Float64Constant carrying those bits. A double array element must never be a
// signalling NaN, so the store has to canonicalize it.

const f64 = new Float64Array(1);
const u64 = new BigUint64Array(f64.buffer);

function assertQuietNan(x) {
  f64[0] = x;
  const bits = u64[0];
  assertTrue((bits & 0x7FF0000000000000n) === 0x7FF0000000000000n);
  assertTrue((bits & 0x000FFFFFFFFFFFFFn) !== 0n);  // A NaN, not an infinity.
  assertTrue((bits & 0x0008000000000000n) !== 0n);  // Quiet.
}

// Into a holey double array, which is what an element that is undefined makes
// the literal below.
(function() {
  function call(f) {
    try {
      return f();
    } catch (e) {
    }
  }
  function f() {
    const nan = call(() => Math.atanh(42));
    const undef = call();
    return [nan, undef];
  }
  %PrepareFunctionForOptimization(call);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);

  const a = f();
  assertQuietNan(a[0]);
  assertEquals(undefined, a[1]);
})();

// And into a packed one.
(function() {
  function f() {
    return [Math.atanh(42), 1.5];
  }
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);

  const a = f();
  assertQuietNan(a[0]);
  assertEquals(1.5, a[1]);
})();

// A HeapNumber holding a signalling NaN reaches the store as an untagged phi
// input instead, which the phi representation selector has to silence for the
// same reason.
(function() {
  const buf = new ArrayBuffer(8);
  new BigUint64Array(buf)[0] = 0x7FF4000000000000n;
  const snan = new Float64Array(buf)[0];

  function f(out, i, c) {
    let x;
    if (c) {
      x = snan;
    } else {
      x = out[3];
    }
    out[i] = x;
  }
  const out = [1.5, 2.5, , 3.5];
  %PrepareFunctionForOptimization(f);
  f(out, 0, false);
  f(out, 0, true);
  %OptimizeMaglevOnNextCall(f);

  f(out, 0, true);
  assertQuietNan(out[0]);
})();
