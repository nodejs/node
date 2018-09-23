// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = {};
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);

var mod = (function Module(stdlib, foreign, heap) {
  "use asm";
  function mod(dividend, divisor) {
    dividend = dividend|0;
    divisor = divisor|0;
    return (dividend % divisor) | 0;
  }
  return { mod: mod };
})(stdlib, foreign, heap).mod;

var divisors = [-2147483648, -32 * 1024, -1000, -16, -7, -2, -1, 0,
                1, 3, 4, 10, 64, 99, 1023, 1024, 2147483647];
for (var i = 0; i < divisors.length; i++) {
  var divisor = divisors[i];
  for (var dividend = -2147483648; dividend < 2147483648; dividend += 3999773) {
    assertEquals((dividend % divisor) | 0, mod(dividend, divisor));
  }
}
