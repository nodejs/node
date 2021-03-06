// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --opt --no-always-opt

var assertDoesNotThrow;
var assertInstanceof;
var assertUnreachable;
var assertOptimized;
var assertUnoptimized;
function classOf(object) { var string = Object.prototype.toString.call(object); return string.substring(8, string.length - 1); }
function PrettyPrint(value) { return ""; }
function PrettyPrintArrayElement(value, index, array) { return ""; }
function fail(expectedText, found, name_opt) { }
function deepObjectEquals(a, b) { var aProps = Object.keys(a); aProps.sort(); var bProps = Object.keys(b); bProps.sort(); if (!deepEquals(aProps, bProps)) { return false; } for (var i = 0; i < aProps.length; i++) { if (!deepEquals(a[aProps[i]], b[aProps[i]])) { return false; } } return true; }
function deepEquals(a, b) { if (a === b) { if (a === 0) return (1 / a) === (1 / b); return true; } if (typeof a != typeof b) return false; if (typeof a == "number") return isNaN(a) && isNaN(b); if (typeof a !== "object" && typeof a !== "function") return false; var objectClass = classOf(a); if (objectClass !== classOf(b)) return false; if (objectClass === "RegExp") { return (a.toString() === b.toString()); } if (objectClass === "Function") return false; if (objectClass === "Array") { var elementCount = 0; if (a.length != b.length) { return false; } for (var i = 0; i < a.length; i++) { if (!deepEquals(a[i], b[i])) return false; } return true; } if (objectClass == "String" || objectClass == "Number" || objectClass == "Boolean" || objectClass == "Date") { if (a.valueOf() !== b.valueOf()) return false; } return deepObjectEquals(a, b); }
assertSame = function assertSame(expected, found, name_opt) { if (found === expected) { if (expected !== 0 || (1 / expected) == (1 / found)) return; } else if ((expected !== expected) && (found !== found)) { return; } fail(PrettyPrint(expected), found, name_opt); }; assertEquals = function assertEquals(expected, found, name_opt) { if (!deepEquals(found, expected)) { fail(PrettyPrint(expected), found, name_opt); } };
assertEqualsDelta = function assertEqualsDelta(expected, found, delta, name_opt) { assertTrue(Math.abs(expected - found) <= delta, name_opt); };
assertArrayEquals = function assertArrayEquals(expected, found, name_opt) { var start = ""; if (name_opt) { start = name_opt + " - "; } assertEquals(expected.length, found.length, start + "array length"); if (expected.length == found.length) { for (var i = 0; i < expected.length; ++i) { assertEquals(expected[i], found[i], start + "array element at index " + i); } } };
assertPropertiesEqual = function assertPropertiesEqual(expected, found, name_opt) { if (!deepObjectEquals(expected, found)) { fail(expected, found, name_opt); } };
assertToStringEquals = function assertToStringEquals(expected, found, name_opt) { if (expected != String(found)) { fail(expected, found, name_opt); } };
assertTrue = function assertTrue(value, name_opt) { assertEquals(true, value, name_opt); };
assertFalse = function assertFalse(value, name_opt) { assertEquals(false, value, name_opt); };
assertNull = function assertNull(value, name_opt) { if (value !== null) { fail("null", value, name_opt); } };
assertNotNull = function assertNotNull(value, name_opt) { if (value === null) { fail("not null", value, name_opt); } };
var __v_39 = {};
var __v_40 = {};
var __v_41 = {};
var __v_42 = {};
var __v_43 = {};
var __v_44 = {};
try {
__v_0 = [1.5,,1.7];
__v_1 = {__v_0:1.8};
} catch(e) { print("Caught: " + e); }
function __f_0(__v_1,__v_0,i) {
  __v_1.a = __v_0[i];
  gc();
}
%PrepareFunctionForOptimization(__f_0);
try {
  %PrepareFunctionForOptimization(__f_0);
__f_0(__v_1,__v_0,0);
__f_0(__v_1,__v_0,0);
%OptimizeFunctionOnNextCall(__f_0);
__f_0(__v_1,__v_0,1);
assertEquals(undefined, __v_1.a);
__v_0 = [1,,3];
__v_1 = {ab:5};
} catch(e) { print("Caught: " + e); }
function __f_1(__v_1,__v_0,i) {
  __v_1.ab = __v_0[i];
}
try {
__f_1(__v_1,__v_0,1);
} catch(e) { print("Caught: " + e); }
function __f_5(x) {
  return ~x;
}
try {
__f_5(42);
assertEquals(~12, __f_5(12.45));
assertEquals(~46, __f_5(42.87));
__v_2 = 1, __v_4 = 2, __v_3 = 4, __v_6 = 8;
} catch(e) { print("Caught: " + e); }
function __f_4() {
  return __v_2 | (__v_4 | (__v_3 | __v_6));
}
try {
__f_4();
__v_3 = "16";
assertEquals(17 | -13 | 0 | -5, __f_4());
} catch(e) { print("Caught: " + e); }
function __f_6() {
  return __f_4();
}
try {
assertEquals(1 | 2 | 16 | 8, __f_6());
__f_4 = function() { return 42; };
assertEquals(42, __f_6());
__v_5 = {};
__v_5.__f_4 = __f_4;
} catch(e) { print("Caught: " + e); }
function __f_7(o) {
  return o.__f_4();
}
try {
  %PrepareFunctionForOptimization(__f_7);
for (var __v_7 = 0; __v_7 < 5; __v_7++) __f_7(__v_5);
%OptimizeFunctionOnNextCall(__f_7);
__f_7(__v_5);
assertEquals(42, __f_7(__v_5));
assertEquals(87, __f_7({__f_4: function() { return 87; }}));
} catch(e) { print("Caught: " + e); }
function __f_8(x,y) {
  x = 42;
  y = 1;
  y = y << "0";
  return x | y;
}
try {
assertEquals(43, __f_8(0,0));
} catch(e) { print("Caught: " + e); }
function __f_2(x) {
  return 'lit[' + (x + ']');
}
try {
assertEquals('lit[-87]', __f_2(-87));
assertEquals('lit[0]', __f_2(0));
assertEquals('lit[42]', __f_2(42));
__v_9 = "abc";
gc();
var __v_8;
} catch(e) { print("Caught: " + e); }
function __f_9(n) { return __v_9.charAt(n); }
%PrepareFunctionForOptimization(__f_9);
try {
for (var __v_7 = 0; __v_7 < 5; __v_7++) {
  __v_8 = __f_9(0);
}
%OptimizeFunctionOnNextCall(__f_9);
__v_8 = __f_9(0);
} catch(e) { print("Caught: " + e); }
function __f_3(__v_2,__v_4,__v_3,__v_6) {
  return __v_2+__v_4+__v_3+__v_6;
}
try {
assertEquals(0x40000003, __f_3(1,1,2,0x3fffffff));
} catch(e) { print("Caught: " + e); }
try {
__v_19 = {
  fast_smi_only             :  'fast smi only elements',
  fast                      :  'fast elements',
  fast_double               :  'fast double elements',
  dictionary                :  'dictionary elements',
  external_int32            :  'external int8 elements',
  external_uint8            :  'external uint8 elements',
  external_int16            :  'external int16 elements',
  external_uint16           :  'external uint16 elements',
  external_int32            :  'external int32 elements',
  external_uint32           :  'external uint32 elements',
  external_float32          :  'external float32 elements',
  external_float64          :  'external float64 elements',
  external_uint8_clamped    :  'external uint8_clamped elements',
  fixed_int32               :  'fixed int8 elements',
  fixed_uint8               :  'fixed uint8 elements',
  fixed_int16               :  'fixed int16 elements',
  fixed_uint16              :  'fixed uint16 elements',
  fixed_int32               :  'fixed int32 elements',
  fixed_uint32              :  'fixed uint32 elements',
  fixed_float32             :  'fixed float32 elements',
  fixed_float64             :  'fixed float64 elements',
  fixed_uint8_clamped       :  'fixed uint8_clamped elements'
}
} catch(e) { print("Caught: " + e); }
function __f_12() {
}
__v_10 = {};
__v_10.dance = 0xD15C0;
__v_10.drink = 0xC0C0A;
__f_12(__v_19.fast, __v_10);
__v_24 = [1,2,3];
__f_12(__v_19.fast_smi_only, __v_24);
__v_24.dance = 0xD15C0;
__v_24.drink = 0xC0C0A;
__f_12(__v_19.fast_smi_only, __v_24);
function __f_18() {
  var __v_27 = new Array();
  __f_12(__v_19.fast_smi_only, __v_27);
  for (var __v_18 = 0; __v_18 < 1337; __v_18++) {
    var __v_16 = __v_18;
    if (__v_18 == 1336) {
      __f_12(__v_19.fast_smi_only, __v_27);
      __v_16 = new Object();
    }
    __v_27[__v_18] = __v_16;
  }
  __f_12(__v_19.fast, __v_27);
  var __v_15 = [];
  __v_15[912570] = 7;
  __f_12(__v_19.dictionary, __v_15);
  var __v_26 = new Array(912561);
  %SetAllocationTimeout(100000000, 10000000);
  for (var __v_18 = 0; __v_18 < 0x20000; __v_18++) {
    __v_26[0] = __v_18 / 2;
  }
  __f_12(__v_19.fixed_int8,    new Int8Array(007));
  __f_12(__v_19.fixed_uint8,   new Uint8Array(007));
  __f_12(__v_19.fixed_int16,   new Int16Array(666));
  __f_12(__v_19.fixed_uint16,  new Uint16Array(42));
  __f_12(__v_19.fixed_int32,   new Int32Array(0xF));
  __f_12(__v_19.fixed_uint32,  new Uint32Array(23));
  __f_12(__v_19.fixed_float32, new Float32Array(7));
  __f_12(__v_19.fixed_float64, new Float64Array(0));
  __f_12(__v_19.fixed_uint8_clamped, new Uint8ClampedArray(512));
  var __v_13 = new ArrayBuffer(128);
  __f_12(__v_19.external_int8,    new Int8Array(__v_13));
  __f_12(__v_37.external_uint8,   new Uint8Array(__v_13));
  __f_12(__v_19.external_int16,   new Int16Array(__v_13));
  __f_12(__v_19.external_uint16,  new Uint16Array(__v_13));
  __f_12(__v_19.external_int32,   new Int32Array(__v_13));
  __f_12(__v_19.external_uint32,  new Uint32Array(__v_13));
  __f_12(__v_19.external_float32, new Float32Array(__v_13));
  __f_12(__v_19.external_float64, new Float64Array(__v_13));
  __f_12(__v_19.external_uint8_clamped, new Uint8ClampedArray(__v_13));
}
try {
__f_18();
} catch(e) { }
