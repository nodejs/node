// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Float64CountLeadingZeros
(function() {
  function testf(o) {
    return Math.clz32(o.a);
  }

  %PrepareFunctionForOptimization(testf);
  assertEquals(0, testf({a:0xFFFFFFFF}));

  %OptimizeFunctionOnNextCall(testf);
  assertEquals(32, testf({a:0}));
  assertEquals(31, testf({a:1}));
  assertEquals(30, testf({a:2}));
  assertEquals(29, testf({a:4}));
  assertEquals(28, testf({a:8}));
  assertEquals(27, testf({a:16}));
  assertEquals(26, testf({a:32}));
  assertEquals(25, testf({a:64}));
  assertEquals(32, testf({a:NaN}));
  assertEquals(32, testf({a:Infinity}));
  assertEquals(32, testf({a:-Infinity}));
  assertEquals(32, testf({a:-0.0}));
  assertEquals(32, testf({a:0.0}));
  assertEquals(32, testf({a:-1.06099789498857051171444456326E-314}));
  assertEquals(0, testf({a:-543210.9876}));
  assertEquals(0, testf({a:-128.0}));
  assertEquals(0, testf({a:-1.0}));
  assertEquals(31, testf({a:1.0}));
  assertEquals(25, testf({a:64.0}));
  assertEquals(24, testf({a:128.0}));
  assertEquals(12, testf({a:543210.9876}));
  assertEquals(32, testf({a:1.06099789498857051171444456326E-314}));
  if (%IsMaglevEnabled() || %IsTurbofanEnabled()) {
    assertTrue(isOptimized(testf));
  }
})();

// Int32CountLeadingZeros
(function() {
  function testi(x, i) {
    return Math.clz32(x[i]);
  }

  let int_arr = new Int32Array(4);
  int_arr[0] = 0x00FF_FFFF;
  int_arr[1] = 0x0000_FFFF;
  int_arr[2] = 0x0000_00FF;
  int_arr[3] = 0x0000_0000;
  %PrepareFunctionForOptimization(testi);
  assertEquals(8, testi(int_arr, 0));

  %OptimizeFunctionOnNextCall(testi);
  assertEquals(32, testi(int_arr, 3));
  assertEquals(24, testi(int_arr, 2));
  assertEquals(16, testi(int_arr, 1));
  assertEquals(8,  testi(int_arr, 0));
  if (%IsMaglevEnabled() || %IsTurbofanEnabled()) {
    assertTrue(isOptimized(testi));
  }
})();

// Float64CountLeadingZeros (HoleyFloats)
(function() {
  function testh(a, i) {
    return Math.clz32(a[i]);
  }

  let arr = [-4.2, , 4.2, 42];
  %PrepareFunctionForOptimization(testh);
  assertEquals(32, testh(arr, 1));

  %OptimizeFunctionOnNextCall(testh);
  assertEquals(29, testh(arr, 2));
  assertEquals(32, testh(arr, 1));
  assertEquals(0,  testh(arr, 0));
  assertEquals(26, testh(arr, 3));
  if (%IsMaglevEnabled() || %IsTurbofanEnabled()) {
    assertTrue(isOptimized(testh));
  }
})();

(function() {
  function testna() {
    return Math.clz32();
  }

  %PrepareFunctionForOptimization(testna);
  assertEquals(32, testna());

  %OptimizeFunctionOnNextCall(testna);
  assertEquals(32, testna());

  if (%IsMaglevEnabled() || %IsTurbofanEnabled()) {
    assertTrue(isOptimized(testna));
  }
})();
