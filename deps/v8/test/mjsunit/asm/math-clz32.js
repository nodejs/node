// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = { Math: Math };

var f = (function Module(stdlib) {
  "use asm";

  var clz32 = stdlib.Math.clz32;

  function f(a) {
    a = a | 0;
    return clz32(a >>> 0) | 0;
  }

  return f;
})(stdlib);

assertEquals(32, f(0));
assertEquals(32, f(NaN));
assertEquals(32, f(undefined));
for (var i = 0; i < 32; ++i) {
  assertEquals(i, f((-1) >>> i));
}
for (var i = -2147483648; i < 2147483648; i += 3999773) {
  assertEquals(Math.clz32(i), f(i));
}
