// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals(0, %ToLength(NaN));
assertEquals(0, %_ToLength(NaN));

assertEquals(0, %ToLength(-Infinity));
assertEquals(0, %_ToLength(-Infinity));

assertEquals(0, %ToLength(0));
assertEquals(0, %_ToLength(0));

assertEquals(0, %ToLength(.5));
assertEquals(0, %_ToLength(.5));

assertEquals(42, %ToLength(42.99999));
assertEquals(42, %_ToLength(42.99999));

assertEquals(9007199254740991, %ToLength(9007199254740991));
assertEquals(9007199254740991, %_ToLength(9007199254740991));

assertEquals(9007199254740991, %ToLength(Infinity));
assertEquals(9007199254740991, %_ToLength(Infinity));

assertEquals(0, %ToLength(null));
assertEquals(0, %_ToLength(null));

assertEquals(1, %ToLength(true));
assertEquals(1, %_ToLength(true));

assertEquals(0, %ToLength(false));
assertEquals(0, %_ToLength(false));

assertEquals(0, %ToLength(undefined));
assertEquals(0, %_ToLength(undefined));

assertEquals(0, %ToLength("-1"));
assertEquals(0, %_ToLength("-1"));
assertEquals(123, %ToLength("123"));
assertEquals(123, %_ToLength("123"));
assertEquals(0, %ToLength("random text"));
assertEquals(0, %_ToLength("random text"));

assertThrows(function() { %ToLength(Symbol.toPrimitive) }, TypeError);
assertThrows(function() { %_ToLength(Symbol.toPrimitive) }, TypeError);

var a = { toString: function() { return 54321 }};
assertEquals(54321, %ToLength(a));
assertEquals(54321, %_ToLength(a));

var b = { valueOf: function() { return 42 }};
assertEquals(42, %ToLength(b));
assertEquals(42, %_ToLength(b));

var c = {
  toString: function() { return "x"},
  valueOf: function() { return 123 }
};
assertEquals(123, %ToLength(c));
assertEquals(123, %_ToLength(c));

var d = {
  [Symbol.toPrimitive]: function(hint) {
    assertEquals("number", hint);
    return 987654321;
  }
};
assertEquals(987654321, %ToLength(d));
assertEquals(987654321, %_ToLength(d));

var e = new Date(0);
assertEquals(0, %ToLength(e));
assertEquals(0, %_ToLength(e));
