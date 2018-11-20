// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

[NaN, Infinity, -Infinity, 0, -0, "abc", "Infinity", "-Infinity", {}].forEach(
  function(x) {
    assertEquals(32, Math.clz32(x));
  }
);

function testclz(x) {
  for (var i = 0; i < 33; i++) {
    if (x & 0x80000000) return i;
    x <<= 1;
  }
  return 32;
}


function f(e) {
  var max = Math.pow(2, e);
  for (var x = 0; x < max; x = x * 1.01 + 1) {
    assertEquals(testclz(x), Math.clz32(x));
    assertEquals(testclz(-x), Math.clz32(-x));
    assertEquals(testclz(x), Math.clz32({ valueOf: function() { return x; } }));
    assertEquals(testclz(-x),
                 Math.clz32({ toString: function() { return -x; } }));
  }
}

f(5);
f(5);
%OptimizeFunctionOnNextCall(f);
f(40);
