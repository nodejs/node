// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Detaching a buffer with a single view leaves the
// ArrayBufferDetachingProtector intact and changes the view's map.

// No property access on the receiver precedes the call. Each case uses its
// own typed array map, as the first detach makes the map unstable.
function testConstantReceiver(foo, ab) {
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
  ab.transfer();
  assertThrows(foo, TypeError);
}

const ab1 = new ArrayBuffer(16);
const ta1 = new Int32Array(ab1);
testConstantReceiver(() => Int32Array.prototype.values.call(ta1), ab1);
const ab2 = new ArrayBuffer(16);
const ta2 = new Uint8Array(ab2);
testConstantReceiver(() => Uint8Array.prototype.keys.call(ta2), ab2);
const ab3 = new ArrayBuffer(16);
const ta3 = new Float64Array(ab3);
testConstantReceiver(() => Float64Array.prototype.entries.call(ta3), ab3);

(function testByteOffset() {
  const ab = new ArrayBuffer(16);
  const ta = new Int16Array(ab, 8);
  const getByteOffset = Object.getOwnPropertyDescriptor(
      Object.getPrototypeOf(Int16Array.prototype), 'byteOffset').get;
  const foo = () => getByteOffset.call(ta);
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(8, foo());
  ab.transfer();
  assertEquals(0, foo());
})();

let detach = false;
function maybeDetach(ab) {
  if (detach) ab.transfer();
}
%NeverOptimizeFunction(maybeDetach);

// The receiver's map is checked before a call that detaches its buffer.
function testDetachAfterMapCheck(foo) {
  detach = false;
  %PrepareFunctionForOptimization(foo);
  for (let i = 0; i < 3; ++i) {
    const ab = new ArrayBuffer(16);
    foo(new Int32Array(ab), ab);
  }
  %OptimizeFunctionOnNextCall(foo);
  const ab0 = new ArrayBuffer(16);
  foo(new Int32Array(ab0), ab0);
  detach = true;
  const ab = new ArrayBuffer(16);
  assertThrows(() => foo(new Int32Array(ab), ab), TypeError);
}

testDetachAfterMapCheck((ta, ab) => ta.byteLength + ta.values(maybeDetach(ab)));
testDetachAfterMapCheck((ta, ab) => ta.byteLength + ta.keys(maybeDetach(ab)));
testDetachAfterMapCheck(
    (ta, ab) => ta.byteLength + ta.entries(maybeDetach(ab)));
