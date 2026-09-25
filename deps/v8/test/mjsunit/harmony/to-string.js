// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals("1", %ToString(1));

assertEquals("0.5", %ToString(.5));

assertEquals("null", %ToString(null));

assertEquals("true", %ToString(true));

assertEquals("false", %ToString(false));

assertEquals("undefined", %ToString(undefined));

assertEquals("random text", %ToString("random text"));

assertThrows(function() { %ToString(Symbol.toPrimitive) }, TypeError);

var a = { toString: function() { return "xyz" }};
assertEquals("xyz", %ToString(a));

var b = { valueOf: function() { return 42 }};
assertEquals("[object Object]", %ToString(b));

var c = {
  toString: function() { return "x"},
  valueOf: function() { return 123 }
};
assertEquals("x", %ToString(c));

var d = {
  [Symbol.toPrimitive]: function(hint) { return hint }
};
assertEquals("string", %ToString(d));

var e = new Date(0);
assertEquals(e.toString(), %ToString(e));
