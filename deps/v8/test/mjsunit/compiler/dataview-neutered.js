// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --noalways-opt

// Invalidate the neutering protector.
%ArrayBufferNeuter(new ArrayBuffer(1));

// Check DataView.prototype.getInt8() optimization.
(function() {
  const ab = new ArrayBuffer(1);
  const dv = new DataView(ab);

  function foo(dv) {
    return dv.getInt8(0);
  }

  assertEquals(0, foo(dv));
  assertEquals(0, foo(dv));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(dv));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.getUint8() optimization.
(function() {
  const ab = new ArrayBuffer(1);
  const dv = new DataView(ab);

  function foo(dv) {
    return dv.getUint8(0);
  }

  assertEquals(0, foo(dv));
  assertEquals(0, foo(dv));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(dv));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.getInt16() optimization.
(function() {
  const ab = new ArrayBuffer(2);
  const dv = new DataView(ab);

  function foo(dv) {
    return dv.getInt16(0, true);
  }

  assertEquals(0, foo(dv));
  assertEquals(0, foo(dv));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(dv));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.getUint16() optimization.
(function() {
  const ab = new ArrayBuffer(2);
  const dv = new DataView(ab);

  function foo(dv) {
    return dv.getUint16(0, true);
  }

  assertEquals(0, foo(dv));
  assertEquals(0, foo(dv));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(dv));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.getInt32() optimization.
(function() {
  const ab = new ArrayBuffer(4);
  const dv = new DataView(ab);

  function foo(dv) {
    return dv.getInt32(0, true);
  }

  assertEquals(0, foo(dv));
  assertEquals(0, foo(dv));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(dv));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.getUint32() optimization.
(function() {
  const ab = new ArrayBuffer(4);
  const dv = new DataView(ab);

  function foo(dv) {
    return dv.getUint32(0, true);
  }

  assertEquals(0, foo(dv));
  assertEquals(0, foo(dv));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(dv));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.getFloat32() optimization.
(function() {
  const ab = new ArrayBuffer(4);
  const dv = new DataView(ab);

  function foo(dv) {
    return dv.getFloat32(0, true);
  }

  assertEquals(0, foo(dv));
  assertEquals(0, foo(dv));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(dv));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.getFloat64() optimization.
(function() {
  const ab = new ArrayBuffer(8);
  const dv = new DataView(ab);

  function foo(dv) {
    return dv.getFloat64(0, true);
  }

  assertEquals(0, foo(dv));
  assertEquals(0, foo(dv));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(dv));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.setInt8() optimization.
(function() {
  const ab = new ArrayBuffer(1);
  const dv = new DataView(ab);

  function foo(dv, x) {
    return dv.setInt8(0, x);
  }

  assertEquals(undefined, foo(dv, 1));
  assertEquals(1, dv.getInt8(0));
  assertEquals(undefined, foo(dv, 2));
  assertEquals(2, dv.getInt8(0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(dv, 3));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv, 4), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv, 5), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.setUint8() optimization.
(function() {
  const ab = new ArrayBuffer(1);
  const dv = new DataView(ab);

  function foo(dv, x) {
    return dv.setUint8(0, x);
  }

  assertEquals(undefined, foo(dv, 1));
  assertEquals(1, dv.getUint8(0));
  assertEquals(undefined, foo(dv, 2));
  assertEquals(2, dv.getUint8(0));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(dv, 3));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv, 4), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv, 5), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.setInt16() optimization.
(function() {
  const ab = new ArrayBuffer(2);
  const dv = new DataView(ab);

  function foo(dv, x) {
    return dv.setInt16(0, x, true);
  }

  assertEquals(undefined, foo(dv, 1));
  assertEquals(1, dv.getInt16(0, true));
  assertEquals(undefined, foo(dv, 2));
  assertEquals(2, dv.getInt16(0, true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(dv, 3));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv, 4), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv, 5), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.setUint16() optimization.
(function() {
  const ab = new ArrayBuffer(2);
  const dv = new DataView(ab);

  function foo(dv, x) {
    return dv.setUint16(0, x, true);
  }

  assertEquals(undefined, foo(dv, 1));
  assertEquals(1, dv.getUint16(0, true));
  assertEquals(undefined, foo(dv, 2));
  assertEquals(2, dv.getUint16(0, true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(dv, 3));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv, 4), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv, 5), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.setInt32() optimization.
(function() {
  const ab = new ArrayBuffer(4);
  const dv = new DataView(ab);

  function foo(dv, x) {
    return dv.setInt32(0, x, true);
  }

  assertEquals(undefined, foo(dv, 1));
  assertEquals(1, dv.getInt32(0, true));
  assertEquals(undefined, foo(dv, 2));
  assertEquals(2, dv.getInt32(0, true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(dv, 3));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv, 4), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv, 5), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.setUint32() optimization.
(function() {
  const ab = new ArrayBuffer(4);
  const dv = new DataView(ab);

  function foo(dv, x) {
    return dv.setUint32(0, x, true);
  }

  assertEquals(undefined, foo(dv, 1));
  assertEquals(1, dv.getUint32(0, true));
  assertEquals(undefined, foo(dv, 2));
  assertEquals(2, dv.getUint32(0, true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(dv, 3));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv, 4), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv, 5), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.setFloat32() optimization.
(function() {
  const ab = new ArrayBuffer(4);
  const dv = new DataView(ab);

  function foo(dv, x) {
    return dv.setFloat32(0, x, true);
  }

  assertEquals(undefined, foo(dv, 1));
  assertEquals(1, dv.getFloat32(0, true));
  assertEquals(undefined, foo(dv, 2));
  assertEquals(2, dv.getFloat32(0, true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(dv, 3));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv, 4), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv, 5), TypeError);
  assertOptimized(foo);
})();

// Check DataView.prototype.setFloat64() optimization.
(function() {
  const ab = new ArrayBuffer(8);
  const dv = new DataView(ab);

  function foo(dv, x) {
    return dv.setFloat64(0, x, true);
  }

  assertEquals(undefined, foo(dv, 1));
  assertEquals(1, dv.getFloat64(0, true));
  assertEquals(undefined, foo(dv, 2));
  assertEquals(2, dv.getFloat64(0, true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(dv, 3));
  assertOptimized(foo);
  %ArrayBufferNeuter(ab);
  assertThrows(() => foo(dv, 4), TypeError);
  assertUnoptimized(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(dv, 5), TypeError);
  assertOptimized(foo);
})();
