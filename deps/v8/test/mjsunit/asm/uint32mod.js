// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = {};
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);

var mod = (function Module(stdlib, foreign, heap) {
  "use asm";
  function mod(dividend, divisor) {
    dividend = dividend | 0;
    divisor = divisor | 0;
    return ((dividend >>> 0) % (divisor >>> 0)) | 0;
  }
  return { mod: mod };
})(stdlib, foreign, heap).mod;

var divisors = [0, 1, 3, 4, 10, 42, 64, 100, 1024, 2147483647, 4294967295];
for (var i in divisors) {
  var divisor = divisors[i];
  for (var dividend = 0; dividend < 4294967296; dividend += 3999773) {
    assertEquals((dividend % divisor) | 0, mod(dividend, divisor));
  }
}
