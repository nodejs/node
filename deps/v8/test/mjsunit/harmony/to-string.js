// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals("1", %ToString(1));
assertEquals("1", %_ToString(1));

assertEquals("0.5", %ToString(.5));
assertEquals("0.5", %_ToString(.5));

assertEquals("null", %ToString(null));
assertEquals("null", %_ToString(null));

assertEquals("true", %ToString(true));
assertEquals("true", %_ToString(true));

assertEquals("false", %ToString(false));
assertEquals("false", %_ToString(false));

assertEquals("undefined", %ToString(undefined));
assertEquals("undefined", %_ToString(undefined));

assertEquals("random text", %ToString("random text"));
assertEquals("random text", %_ToString("random text"));

assertThrows(function() { %ToString(Symbol.toPrimitive) }, TypeError);
assertThrows(function() { %_ToString(Symbol.toPrimitive) }, TypeError);

var a = { toString: function() { return "xyz" }};
assertEquals("xyz", %ToString(a));
assertEquals("xyz", %_ToString(a));

var b = { valueOf: function() { return 42 }};
assertEquals("[object Object]", %ToString(b));
assertEquals("[object Object]", %_ToString(b));

var c = {
  toString: function() { return "x"},
  valueOf: function() { return 123 }
};
assertEquals("x", %ToString(c));
assertEquals("x", %_ToString(c));

var d = {
  [Symbol.toPrimitive]: function(hint) { return hint }
};
assertEquals("string", %ToString(d));
assertEquals("string", %_ToString(d));

var e = new Date(0);
assertEquals(e.toString(), %ToString(e));
assertEquals(e.toString(), %_ToString(e));
