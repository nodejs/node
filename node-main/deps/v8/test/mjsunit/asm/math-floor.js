// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib) {
  "use asm";

  var floor = stdlib.Math.floor;

  // f: double -> float
  function f(a) {
    a = +a
    return +floor(a);
  }

  return { f: f };
}

var f = Module({ Math: Math }).f;

assertTrue(isNaN(f(NaN)));
assertTrue(isNaN(f(undefined)));
assertTrue(isNaN(f(function() {})));

assertEquals(0,                   f(0));
assertEquals(+0,                  f(+0));
assertEquals(-0,                  f(-0));
assertEquals(0,                   f(0.49999));
assertEquals(+0,                  f(0.6));
assertEquals(+0,                  f(0.5));
assertEquals(-1,                  f(-0.1));
assertEquals(-1,                  f(-0.5));
assertEquals(-1,                  f(-0.6));
assertEquals(-2,                  f(-1.6));
assertEquals(-1,                  f(-0.50001));

assertEquals("Infinity", String(f(Infinity)));
assertEquals("-Infinity", String(f(-Infinity)));
