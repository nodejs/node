// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals("1", %ToStringRT(1));
assertEquals("1", %_ToStringRT(1));

assertEquals("0.5", %ToStringRT(.5));
assertEquals("0.5", %_ToStringRT(.5));

assertEquals("null", %ToStringRT(null));
assertEquals("null", %_ToStringRT(null));

assertEquals("true", %ToStringRT(true));
assertEquals("true", %_ToStringRT(true));

assertEquals("false", %ToStringRT(false));
assertEquals("false", %_ToStringRT(false));

assertEquals("undefined", %ToStringRT(undefined));
assertEquals("undefined", %_ToStringRT(undefined));

assertEquals("random text", %ToStringRT("random text"));
assertEquals("random text", %_ToStringRT("random text"));

assertThrows(function() { %ToStringRT(Symbol.toPrimitive) }, TypeError);
assertThrows(function() { %_ToStringRT(Symbol.toPrimitive) }, TypeError);

var a = { toString: function() { return "xyz" }};
assertEquals("xyz", %ToStringRT(a));
assertEquals("xyz", %_ToStringRT(a));

var b = { valueOf: function() { return 42 }};
assertEquals("[object Object]", %ToStringRT(b));
assertEquals("[object Object]", %_ToStringRT(b));

var c = {
  toString: function() { return "x"},
  valueOf: function() { return 123 }
};
assertEquals("x", %ToStringRT(c));
assertEquals("x", %_ToStringRT(c));

var d = {
  [Symbol.toPrimitive]: function(hint) { return hint }
};
assertEquals("string", %ToStringRT(d));
assertEquals("string", %_ToStringRT(d));

var e = new Date(0);
assertEquals(e.toString(), %ToStringRT(e));
assertEquals(e.toString(), %_ToStringRT(e));
