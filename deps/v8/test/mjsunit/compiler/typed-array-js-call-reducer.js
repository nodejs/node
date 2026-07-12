// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan --track-array-buffer-views

// Helper to force optimization (best effort)
function optimize(fn, ...args) {
  %PrepareFunctionForOptimization(fn);
  fn(...args);
  %OptimizeFunctionOnNextCall(fn);
  fn(...args);
}

// Test 1: TypedArray.prototype.length
(function() {
  console.log("Test 1: length");
  const ab = new ArrayBuffer(16);
  const ta = new Int32Array(ab);
  function getLength(a) { return a.length; }
  optimize(getLength, ta);
  assertEquals(4, getLength(ta));
  %ArrayBufferDetach(ab);
  assertEquals(0, getLength(ta));
})();

// Test 2: TypedArray.prototype.byteLength
(function() {
  console.log("Test 2: byteLength");
  const ab = new ArrayBuffer(16);
  const ta = new Int32Array(ab);
  function getByteLength(a) { return a.byteLength; }
  optimize(getByteLength, ta);
  assertEquals(16, getByteLength(ta));
  %ArrayBufferDetach(ab);
  assertEquals(0, getByteLength(ta));
})();

// Test 3: TypedArray.prototype.byteOffset
(function() {
  console.log("Test 3: byteOffset");
  const ab = new ArrayBuffer(16);
  const ta = new Int32Array(ab, 4);
  function getByteOffset(a) { return a.byteOffset; }
  optimize(getByteOffset, ta);
  assertEquals(4, getByteOffset(ta));
  %ArrayBufferDetach(ab);
  assertEquals(0, getByteOffset(ta));
})();

// Test 4: TypedArray Constructor (Constant size)
(function() {
  console.log("Test 4: Constructor");
  function create() { return new Int32Array(10); }
  optimize(create);
  const ta = create();
  assertEquals(10, ta.length);
})();

// Test 5: toStringTag
(function() {
  console.log("Test 5: toStringTag");
  const ta = new Int32Array(10);
  function getTag(a) { return Object.prototype.toString.call(a); }
  optimize(getTag, ta);
  assertEquals("[object Int32Array]", getTag(ta));
})();

// Test 6: ArrayBuffer.isView
(function() {
  console.log("Test 6: isView");
  const ta = new Int32Array(10);
  function check(a) { return ArrayBuffer.isView(a); }
  optimize(check, ta);
  assertTrue(check(ta));
})();

// Test 10: Iterator Creation on Detached Array
(function() {
  console.log("Test 10: Iterator Creation on Detached Array");
  const ab = new ArrayBuffer(16);
  const ta = new Int32Array(ab);
  function createIter(a) { return a.values(); }
  optimize(createIter, ta);
  %ArrayBufferDetach(ab);
  // Should throw TypeError immediately
  assertThrows(() => createIter(ta), TypeError);
})();

// Test 11: Constructor with variable size
(function() {
  console.log("Test 11: Constructor with variable size");
  function create(n) { return new Int32Array(n); }
  optimize(create, 10);
  const ta = create(20);
  assertEquals(20, ta.length);
  // Check correctness of constructed array
  assertEquals(0, ta[0]);
})();

// Test 12: length with Phi (Control Flow) - Ensures reduction handles Phi inputs
(function() {
  console.log("Test 12: length with Phi");
  const ab1 = new ArrayBuffer(16);
  const ta1 = new Int32Array(ab1); // len 4
  const ab2 = new ArrayBuffer(32);
  const ta2 = new Int32Array(ab2); // len 8
  function getLength(cond, a, b) {
    // 't' is a Phi node in TurboFan
    const t = cond ? a : b;
    return t.length;
  }
  optimize(getLength, true, ta1, ta2);
  assertEquals(4, getLength(true, ta1, ta2));
  assertEquals(8, getLength(false, ta1, ta2));
  // Detach ta1
  %ArrayBufferDetach(ab1);
  // ta1 should be 0, ta2 should be 8
  // This verifies that the optimized code (or deopt fallback) correctly handles
  // the specific detached instance even when mixed with valid ones.
  assertEquals(0, getLength(true, ta1, ta2));
  assertEquals(8, getLength(false, ta1, ta2));
})();

// Test 13: Obscured Load (Nested object)
(function() {
  console.log("Test 13: Obscured Load");
  const ab = new ArrayBuffer(16);
  const ta = new Int32Array(ab);
  const wrapper = { item: ta };
  function getLength(w) { return w.item.length; }
  optimize(getLength, wrapper);
  assertEquals(4, getLength(wrapper));
  %ArrayBufferDetach(ab);
  assertEquals(0, getLength(wrapper));
})();

// Test 14: byteLength with Phi
(function() {
  console.log("Test 14: byteLength with Phi");
  const ab1 = new ArrayBuffer(16);
  const ta1 = new Int32Array(ab1);
  const ab2 = new ArrayBuffer(32);
  const ta2 = new Int32Array(ab2);
  function getByteLength(cond, a, b) {
    const t = cond ? a : b;
    return t.byteLength;
  }
  optimize(getByteLength, true, ta1, ta2);
  assertEquals(16, getByteLength(true, ta1, ta2));
  assertEquals(32, getByteLength(false, ta1, ta2));
  %ArrayBufferDetach(ab1);
  assertEquals(0, getByteLength(true, ta1, ta2));
  assertEquals(32, getByteLength(false, ta1, ta2));
})();
