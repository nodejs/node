// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals(1, %ToPrimitive(1));
assertEquals(1, %ToPrimitive_Number(1));

assertEquals(.5, %ToPrimitive(.5));
assertEquals(.5, %ToPrimitive_Number(.5));

assertEquals(null, %ToPrimitive(null));
assertEquals(null, %ToPrimitive_Number(null));

assertEquals(true, %ToPrimitive(true));
assertEquals(true, %ToPrimitive_Number(true));

assertEquals(false, %ToPrimitive(false));
assertEquals(false, %ToPrimitive_Number(false));

assertEquals(undefined, %ToPrimitive(undefined));
assertEquals(undefined, %ToPrimitive_Number(undefined));

assertEquals("random text", %ToPrimitive("random text"));
assertEquals("random text", %ToPrimitive_Number("random text"));

assertEquals(Symbol.toPrimitive, %ToPrimitive(Symbol.toPrimitive));
assertEquals(Symbol.toPrimitive, %ToPrimitive_Number(Symbol.toPrimitive));

var a = { toString: function() { return "xyz" }};
assertEquals("xyz", %ToPrimitive(a));
assertEquals("xyz", %ToPrimitive_Number(a));

var b = { valueOf: function() { return 42 }};
assertEquals(42, %ToPrimitive(b));
assertEquals(42, %ToPrimitive_Number(b));

var c = {
  toString: function() { return "x"},
  valueOf: function() { return 123 }
};
assertEquals(123, %ToPrimitive(c));
assertEquals(123, %ToPrimitive_Number(c));

var d = {
  [Symbol.toPrimitive]: function(hint) { return hint }
};
assertEquals("default", %ToPrimitive(d));
assertEquals("number", %ToPrimitive_Number(d));

var e = new Date(0);
assertEquals(e.toString(), %ToPrimitive(e));
assertEquals(0, %ToPrimitive_Number(e));
