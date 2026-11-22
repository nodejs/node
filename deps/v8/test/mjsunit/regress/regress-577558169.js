// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

(function TestDirectPartialTruncation() {
  let escape;
  const obj = { t: 2 };
  const big_arr = new Array(0x100000).fill(1);
  const small_arr = [1.1, 2.2];

  function f(a, s, val) {
    var x = a | 0;
    if (x < 100 && small_arr.length > 1) {
      var z2 = s.length * 8388608;
      var small = x + (-4294967294);
      var big = z2 + (-2147483647);
      var sum1 = (small - big) | 0;
      escape = big;
      var dup_small = x + obj.t;
      var dup = dup_small - big;
      var v;
      if ((x & 1) < 2) {
        v = dup;
      } else {
        v = "str";
      }
      var r1 = big_arr[v];
      small_arr[v] = val;
      return (sum1 | 0) + r1;
    }
    return 0;
  }

  const warm_s = "a".repeat(256);
  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 100; i++) {
    f(0.5, warm_s, 3.3);
  }
  %OptimizeFunctionOnNextCall(f);
  f(-2147483648 + 100, "", 1.23456789e100);
  assertEquals(102, small_arr.length);
  assertEquals(1.23456789e100, small_arr[101]);
})();

(function TestTransitivePartialTruncation() {
  let escape1, escape2;
  const obj = { t: 2 };
  const big_arr = new Array(0x100000).fill(1);
  const small_arr = [1.1, 2.2];

  function g(a, s1, s2, val) {
    var x = a | 0;
    if (x < 100 && small_arr.length > 1) {
      var small = x + (-4294967294);
      var big1 = s1.length * 4194304 + (-2147483646);
      var big2 = s2.length * 4194304 + (-1);
      var mid = small - big1;
      var top = mid - big2;
      var sum1 = top | 0;
      escape1 = big1;
      escape2 = big2;
      var dup_small = x + obj.t;
      var dup_mid = dup_small - big1;
      var dup_top = dup_mid - big2;
      var v;
      if ((x & 1) < 2) {
        v = dup_top;
      } else {
        v = "str";
      }
      var r1 = big_arr[v];
      small_arr[v] = val;
      return (sum1 | 0) + r1;
    }
    return 0;
  }

  const warm_s = "a".repeat(256);
  %PrepareFunctionForOptimization(g);
  for (let i = 0; i < 100; i++) {
    g(0.5, warm_s, warm_s, 3.3);
  }
  %OptimizeFunctionOnNextCall(g);
  g(-2147483648 + 100, "", "", 1.23456789e100);
  assertEquals(102, small_arr.length);
  assertEquals(1.23456789e100, small_arr[101]);
})();
