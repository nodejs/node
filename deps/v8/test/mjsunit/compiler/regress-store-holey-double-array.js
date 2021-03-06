// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function StoreHoleBitPattern() {
  function g(src, dst, i) {
    dst[i] = src[i];
  }

  var b = new ArrayBuffer(16);
  var i32 = new Int32Array(b);
  i32[0] = 0xFFF7FFFF;
  i32[1] = 0xFFF7FFFF;
  i32[3] = 0xFFF7FFFF;
  i32[4] = 0xFFF7FFFF;
  var f64 = new Float64Array(b);

  var a = [,0.1];

  %PrepareFunctionForOptimization(g);
  g(f64, a, 1);
  g(f64, a, 1);
  %OptimizeFunctionOnNextCall(g);
  g(f64, a, 0);

  assertTrue(Number.isNaN(a[0]));
})();


(function ConvertHoleToNumberAndStore() {
  function g(a, i) {
    var x = a[i];
    a[i] = +x;
  }

  var a=[,0.1];

  %PrepareFunctionForOptimization(g);
  g(a, 1);
  g(a, 1);
  %OptimizeFunctionOnNextCall(g);
  g(a, 0);
  assertTrue(Number.isNaN(a[0]));
})();
