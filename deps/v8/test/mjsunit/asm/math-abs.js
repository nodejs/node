// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib) {
  "use asm";

  var abs = stdlib.Math.abs;

  // f: double -> double
  function f(a) {
    a = +a;
    return +abs(a);
  }

  // g: unsigned -> double
  function g(a) {
    a = a>>>0;
    return +abs(a);
  }

  // h: signed -> double
  function h(a) {
    a = a|0;
    return +abs(a);
  }

  return { f: f, g: g, h: h };
}

var m = Module({ Math: Math });
var f = m.f;
var g = m.g;
var h = m.h;

assertTrue(isNaN(f(NaN)));
assertTrue(isNaN(f(undefined)));
assertTrue(isNaN(f(function() {})));

assertEquals("Infinity", String(1/f(0)));
assertEquals("Infinity", String(1/f(-0)));
assertEquals("Infinity", String(f(Infinity)));
assertEquals("Infinity", String(f(-Infinity)));

assertEquals(0,   f(0));
assertEquals(0.1, f(0.1));
assertEquals(0.5, f(0.5));
assertEquals(0.1, f(-0.1));
assertEquals(0.5, f(-0.5));
assertEquals(1,   f(1));
assertEquals(1.1, f(1.1));
assertEquals(1.5, f(1.5));
assertEquals(1,   f(-1));
assertEquals(1.1, f(-1.1));
assertEquals(1.5, f(-1.5));

assertEquals(0,          g(0));
assertEquals(0,          g(0.1));
assertEquals(0,          g(0.5));
assertEquals(0,          g(-0.1));
assertEquals(0,          g(-0.5));
assertEquals(1,          g(1));
assertEquals(1,          g(1.1));
assertEquals(1,          g(1.5));
assertEquals(4294967295, g(-1));
assertEquals(4294967295, g(-1.1));
assertEquals(4294967295, g(-1.5));

assertEquals(0, h(0));
assertEquals(0, h(0.1));
assertEquals(0, h(0.5));
assertEquals(0, h(-0.1));
assertEquals(0, h(-0.5));
assertEquals(1, h(1));
assertEquals(1, h(1.1));
assertEquals(1, h(1.5));
assertEquals(1, h(-1));
assertEquals(1, h(-1.1));
assertEquals(1, h(-1.5));

assertEquals(Number.MIN_VALUE, f(Number.MIN_VALUE));
assertEquals(Number.MIN_VALUE, f(-Number.MIN_VALUE));
assertEquals(Number.MAX_VALUE, f(Number.MAX_VALUE));
assertEquals(Number.MAX_VALUE, f(-Number.MAX_VALUE));
