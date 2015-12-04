// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertEquals(1, %ToPrimitive(1));
assertEquals(1, %ToPrimitive_Number(1));
assertEquals(1, %ToPrimitive_String(1));
assertEquals(1, %_ToPrimitive(1));
assertEquals(1, %_ToPrimitive_Number(1));
assertEquals(1, %_ToPrimitive_String(1));

assertEquals(.5, %ToPrimitive(.5));
assertEquals(.5, %ToPrimitive_Number(.5));
assertEquals(.5, %ToPrimitive_String(.5));
assertEquals(.5, %_ToPrimitive(.5));
assertEquals(.5, %_ToPrimitive_Number(.5));
assertEquals(.5, %_ToPrimitive_String(.5));

assertEquals(null, %ToPrimitive(null));
assertEquals(null, %ToPrimitive_Number(null));
assertEquals(null, %ToPrimitive_String(null));
assertEquals(null, %_ToPrimitive(null));
assertEquals(null, %_ToPrimitive_Number(null));
assertEquals(null, %_ToPrimitive_String(null));

assertEquals(true, %ToPrimitive(true));
assertEquals(true, %ToPrimitive_Number(true));
assertEquals(true, %ToPrimitive_String(true));
assertEquals(true, %_ToPrimitive(true));
assertEquals(true, %_ToPrimitive_Number(true));
assertEquals(true, %_ToPrimitive_String(true));

assertEquals(false, %ToPrimitive(false));
assertEquals(false, %ToPrimitive_Number(false));
assertEquals(false, %ToPrimitive_String(false));
assertEquals(false, %_ToPrimitive(false));
assertEquals(false, %_ToPrimitive_Number(false));
assertEquals(false, %_ToPrimitive_String(false));

assertEquals(undefined, %ToPrimitive(undefined));
assertEquals(undefined, %ToPrimitive_Number(undefined));
assertEquals(undefined, %ToPrimitive_String(undefined));
assertEquals(undefined, %_ToPrimitive(undefined));
assertEquals(undefined, %_ToPrimitive_Number(undefined));
assertEquals(undefined, %_ToPrimitive_String(undefined));

assertEquals("random text", %ToPrimitive("random text"));
assertEquals("random text", %ToPrimitive_Number("random text"));
assertEquals("random text", %ToPrimitive_String("random text"));
assertEquals("random text", %_ToPrimitive("random text"));
assertEquals("random text", %_ToPrimitive_Number("random text"));
assertEquals("random text", %_ToPrimitive_String("random text"));

assertEquals(Symbol.toPrimitive, %ToPrimitive(Symbol.toPrimitive));
assertEquals(Symbol.toPrimitive, %ToPrimitive_Number(Symbol.toPrimitive));
assertEquals(Symbol.toPrimitive, %ToPrimitive_String(Symbol.toPrimitive));
assertEquals(Symbol.toPrimitive, %_ToPrimitive(Symbol.toPrimitive));
assertEquals(Symbol.toPrimitive, %_ToPrimitive_Number(Symbol.toPrimitive));
assertEquals(Symbol.toPrimitive, %_ToPrimitive_String(Symbol.toPrimitive));

var a = { toString: function() { return "xyz" }};
assertEquals("xyz", %ToPrimitive(a));
assertEquals("xyz", %ToPrimitive_Number(a));
assertEquals("xyz", %ToPrimitive_String(a));
assertEquals("xyz", %_ToPrimitive(a));
assertEquals("xyz", %_ToPrimitive_Number(a));
assertEquals("xyz", %_ToPrimitive_String(a));

var b = { valueOf: function() { return 42 }};
assertEquals(42, %ToPrimitive(b));
assertEquals(42, %ToPrimitive_Number(b));
assertEquals("[object Object]", %ToPrimitive_String(b));
assertEquals(42, %_ToPrimitive(b));
assertEquals(42, %_ToPrimitive_Number(b));
assertEquals("[object Object]", %_ToPrimitive_String(b));

var c = {
  toString: function() { return "x"},
  valueOf: function() { return 123 }
};
assertEquals(123, %ToPrimitive(c));
assertEquals(123, %ToPrimitive_Number(c));
assertEquals("x", %ToPrimitive_String(c));
assertEquals(123, %_ToPrimitive(c));
assertEquals(123, %_ToPrimitive_Number(c));
assertEquals("x", %_ToPrimitive_String(c));

var d = {
  [Symbol.toPrimitive]: function(hint) { return hint }
};
assertEquals("default", %ToPrimitive(d));
assertEquals("number", %ToPrimitive_Number(d));
assertEquals("string", %ToPrimitive_String(d));
assertEquals("default", %_ToPrimitive(d));
assertEquals("number", %_ToPrimitive_Number(d));
assertEquals("string", %_ToPrimitive_String(d));

var e = new Date(0);
assertEquals(e.toString(), %ToPrimitive(e));
assertEquals(0, %ToPrimitive_Number(e));
assertEquals(e.toString(), %ToPrimitive_String(e));
assertEquals(e.toString(), %_ToPrimitive(e));
assertEquals(0, %_ToPrimitive_Number(e));
assertEquals(e.toString(), %_ToPrimitive_String(e));
