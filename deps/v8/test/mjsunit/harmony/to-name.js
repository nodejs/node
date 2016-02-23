// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals("1", %ToName(1));
assertEquals("1", %_ToName(1));

assertEquals("0.5", %ToName(.5));
assertEquals("0.5", %_ToName(.5));

assertEquals("null", %ToName(null));
assertEquals("null", %_ToName(null));

assertEquals("true", %ToName(true));
assertEquals("true", %_ToName(true));

assertEquals("false", %ToName(false));
assertEquals("false", %_ToName(false));

assertEquals("undefined", %ToName(undefined));
assertEquals("undefined", %_ToName(undefined));

assertEquals("random text", %ToName("random text"));
assertEquals("random text", %_ToName("random text"));

assertEquals(Symbol.toPrimitive, %ToName(Symbol.toPrimitive));
assertEquals(Symbol.toPrimitive, %_ToName(Symbol.toPrimitive));

var a = { toString: function() { return "xyz" }};
assertEquals("xyz", %ToName(a));
assertEquals("xyz", %_ToName(a));

var b = { valueOf: function() { return 42 }};
assertEquals("[object Object]", %ToName(b));
assertEquals("[object Object]", %_ToName(b));

var c = {
  toString: function() { return "x"},
  valueOf: function() { return 123 }
};
assertEquals("x", %ToName(c));
assertEquals("x", %_ToName(c));

var d = {
  [Symbol.toPrimitive]: function(hint) { return hint }
};
assertEquals("string", %ToName(d));
assertEquals("string", %_ToName(d));

var e = new Date(0);
assertEquals(e.toString(), %ToName(e));
assertEquals(e.toString(), %_ToName(e));
