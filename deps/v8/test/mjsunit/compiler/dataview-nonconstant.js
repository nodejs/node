// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test DataView.prototype.getInt8()/setInt8() for non-constant DataViews.
(function() {
  const dv = new DataView(new ArrayBuffer(1024));
  dv.setInt8(0, 42);
  dv.setInt8(1, 24);

  function foo(dv, i) {
    const x = dv.getInt8(i);
    dv.setInt8(i, x+1);
    return x;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo(dv, 0));
  assertEquals(24, foo(dv, 1));
  assertEquals(43, foo(dv, 0));
  assertEquals(25, foo(dv, 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(44, foo(dv, 0));
  assertEquals(26, foo(dv, 1));
})();

// Test DataView.prototype.getUint8()/setUint8() for non-constant DataViews.
(function() {
  const dv = new DataView(new ArrayBuffer(1024));
  dv.setUint8(0, 42);
  dv.setUint8(1, 24);

  function foo(dv, i) {
    const x = dv.getUint8(i);
    dv.setUint8(i, x+1);
    return x;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo(dv, 0));
  assertEquals(24, foo(dv, 1));
  assertEquals(43, foo(dv, 0));
  assertEquals(25, foo(dv, 1));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(44, foo(dv, 0));
  assertEquals(26, foo(dv, 1));
})();

// Test DataView.prototype.getInt16()/setInt16() for non-constant DataViews.
(function() {
  const dv = new DataView(new ArrayBuffer(1024));
  dv.setInt16(0, 42, true);
  dv.setInt16(2, 24, true);

  function foo(dv, i) {
    const x = dv.getInt16(i, true);
    dv.setInt16(i, x+1, true);
    return x;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo(dv, 0));
  assertEquals(24, foo(dv, 2));
  assertEquals(43, foo(dv, 0));
  assertEquals(25, foo(dv, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(44, foo(dv, 0));
  assertEquals(26, foo(dv, 2));
})();

// Test DataView.prototype.getUint16()/setUint16() for non-constant DataViews.
(function() {
  const dv = new DataView(new ArrayBuffer(1024));
  dv.setUint16(0, 42, true);
  dv.setUint16(2, 24, true);

  function foo(dv, i) {
    const x = dv.getUint16(i, true);
    dv.setUint16(i, x+1, true);
    return x;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo(dv, 0));
  assertEquals(24, foo(dv, 2));
  assertEquals(43, foo(dv, 0));
  assertEquals(25, foo(dv, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(44, foo(dv, 0));
  assertEquals(26, foo(dv, 2));
})();

// Test DataView.prototype.getInt32()/setInt32() for non-constant DataViews.
(function() {
  const dv = new DataView(new ArrayBuffer(1024));
  dv.setInt32(0, 42, true);
  dv.setInt32(4, 24, true);

  function foo(dv, i) {
    const x = dv.getInt32(i, true);
    dv.setInt32(i, x+1, true);
    return x;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo(dv, 0));
  assertEquals(24, foo(dv, 4));
  assertEquals(43, foo(dv, 0));
  assertEquals(25, foo(dv, 4));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(44, foo(dv, 0));
  assertEquals(26, foo(dv, 4));
})();

// Test DataView.prototype.getUint32()/setUint32() for non-constant DataViews.
(function() {
  const dv = new DataView(new ArrayBuffer(1024));
  dv.setUint32(0, 42, true);
  dv.setUint32(4, 24, true);

  function foo(dv, i) {
    const x = dv.getUint32(i, true);
    dv.setUint32(i, x+1, true);
    return x;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo(dv, 0));
  assertEquals(24, foo(dv, 4));
  assertEquals(43, foo(dv, 0));
  assertEquals(25, foo(dv, 4));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(44, foo(dv, 0));
  assertEquals(26, foo(dv, 4));
})();

// Test DataView.prototype.getFloat32()/setFloat32() for non-constant DataViews.
(function() {
  const dv = new DataView(new ArrayBuffer(1024));
  dv.setFloat32(0, 42, true);
  dv.setFloat32(4, 24, true);

  function foo(dv, i) {
    const x = dv.getFloat32(i, true);
    dv.setFloat32(i, x+1, true);
    return x;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo(dv, 0));
  assertEquals(24, foo(dv, 4));
  assertEquals(43, foo(dv, 0));
  assertEquals(25, foo(dv, 4));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(44, foo(dv, 0));
  assertEquals(26, foo(dv, 4));
})();

// Test DataView.prototype.getFloat64()/setFloat64() for non-constant DataViews.
(function() {
  const dv = new DataView(new ArrayBuffer(1024));
  dv.setFloat64(0, 42, true);
  dv.setFloat64(8, 24, true);

  function foo(dv, i) {
    const x = dv.getFloat64(i, true);
    dv.setFloat64(i, x+1, true);
    return x;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo(dv, 0));
  assertEquals(24, foo(dv, 8));
  assertEquals(43, foo(dv, 0));
  assertEquals(25, foo(dv, 8));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(44, foo(dv, 0));
  assertEquals(26, foo(dv, 8));
})();
