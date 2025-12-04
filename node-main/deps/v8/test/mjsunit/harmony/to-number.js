// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals(1, %ToNumber(1));

assertEquals(.5, %ToNumber(.5));

assertEquals(0, %ToNumber(null));

assertEquals(1, %ToNumber(true));

assertEquals(0, %ToNumber(false));

assertEquals(NaN, %ToNumber(undefined));

assertEquals(-1, %ToNumber("-1"));
assertEquals(123, %ToNumber("123"));
assertEquals(NaN, %ToNumber("random text"));

assertThrows(function() { %ToNumber(Symbol.toPrimitive) }, TypeError);

var a = { toString: function() { return 54321 }};
assertEquals(54321, %ToNumber(a));

var b = { valueOf: function() { return 42 }};
assertEquals(42, %ToNumber(b));

var c = {
  toString: function() { return "x"},
  valueOf: function() { return 123 }
};
assertEquals(123, %ToNumber(c));

var d = {
  [Symbol.toPrimitive]: function(hint) {
    assertEquals("number", hint);
    return 987654321;
  }
};
assertEquals(987654321, %ToNumber(d));

var e = new Date(0);
assertEquals(0, %ToNumber(e));
