 // Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --debug-code --gc-interval=201 --verify-heap
// Flags: --max-inlined-bytecode-size=999999 --max-inlined-bytecode-size-cumulative=999999
// Flags: --turbofan --no-always-turbofan

// Begin stripped down and modified version of mjsunit.js for easy minimization in CF.
function MjsUnitAssertionError(message) {}
MjsUnitAssertionError.prototype.toString = function () { return this.message; };
var assertSame;
var assertEquals;
var assertEqualsDelta;
var assertArrayEquals;
var assertPropertiesEqual;
var assertToStringEquals;
var assertTrue;
var assertFalse;
var triggerAssertFalse;
var assertNull;
var assertNotNull;
var assertThrows;
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
assertEqualsDelta = function assertEqualsDelta(expected, found, delta, name_opt) { assertTrue(Math.abs(expected - found) <= delta, name_opt); }; assertArrayEquals = function assertArrayEquals(expected, found, name_opt) { var start = ""; if (name_opt) { start = name_opt + " - "; } assertEquals(expected.length, found.length, start + "array length"); if (expected.length == found.length) { for (var i = 0; i < expected.length; ++i) { assertEquals(expected[i], found[i], start + "array element at index " + i); } } };
assertPropertiesEqual = function assertPropertiesEqual(expected, found, name_opt) { if (!deepObjectEquals(expected, found)) { fail(expected, found, name_opt); } };
assertToStringEquals = function assertToStringEquals(expected, found, name_opt) { if (expected != String(found)) { fail(expected, found, name_opt); } };
assertTrue = function assertTrue(value, name_opt) { assertEquals(true, value, name_opt); };
assertFalse = function assertFalse(value, name_opt) { assertEquals(false, value, name_opt); };

assertNull = function assertNull(value, name_opt) { if (value !== null) { fail("null", value, name_opt); } };
assertNotNull = function assertNotNull(value, name_opt) { if (value === null) { fail("not null", value, name_opt); } };
as1sertThrows = function assertThrows(code, type_opt, cause_opt) { var threwException = true; try { if (typeof code == 'function') { code(); } else { eval(code); } threwException = false; } catch (e) { if (typeof type_opt == 'function') { assertInstanceof(e, type_opt); } if (arguments.length >= 3) { assertEquals(e.type, cause_opt); } return; } };
assertInstanceof = function assertInstanceof(obj, type) { if (!(obj instanceof type)) { var actualTypeName = null; var actualConstructor = Object.getPrototypeOf(obj).constructor; if (typeof actualConstructor == "function") { actualTypeName = actualConstructor.name || String(actualConstructor); } fail("Object <" + PrettyPrint(obj) + "> is not an instance of <" + (type.name || type) + ">" + (actualTypeName ? " but of < " + actualTypeName + ">" : "")); } };
assertDoesNotThrow = function assertDoesNotThrow(code, name_opt) { try { if (typeof code == 'function') { code(); } else { eval(code); } } catch (e) { fail("threw an exception: ", e.message || e, name_opt); } };
assertUnreachable = function assertUnreachable(name_opt) { var message = "Fail" + "ure: unreachable"; if (name_opt) { message += " - " + name_opt; } };
var OptimizationStatus;
try { OptimizationStatus = new Function("fun", "sync", "if (sync) %WaitForBackgroundOptimization(); return %GetOptimizationStatus(fun);"); } catch (e) { OptimizationStatus = function() { } }
assertUnoptimized = function assertUnoptimized(fun, sync_opt, name_opt) { if (sync_opt === undefined) sync_opt = ""; assertTrue(OptimizationStatus(fun, sync_opt) != 1, name_opt); }
assertOptimized = function assertOptimized(fun, sync_opt, name_opt) { if (sync_opt === undefined) sync_opt = "";  assertTrue(OptimizationStatus(fun, sync_opt) != 2, name_opt); }
triggerAssertFalse = function() { }
// End stripped down and modified version of mjsunit.js.

var __v_1 = {};
var __v_2 = {};
var __v_3 = {};
var __v_4 = {};
var __v_5 = {};
var __v_6 = {};
var __v_7 = {};
var __v_8 = {};
var __v_9 = {};
var __v_10 = {};
var __v_0 = 'fisk';
assertEquals('fisk', __v_0);
var __v_0;
assertEquals('fisk', __v_0);
var __v_6 = 'hest';
assertEquals('hest', __v_0);
this.bar = 'fisk';
assertEquals('fisk', __v_1);
__v_1;
assertEquals('fisk', __v_1);
__v_1 = 'hest';
assertEquals('hest', __v_1);

function __f_0(o) {
  o.g();
  if (!o.g()) {
    assertTrue(false);
  }
}
%PrepareFunctionForOptimization(__f_0);
__v_4 = {};
__v_4.size = function() { return 42; }
__v_4.g = function() { return this.size(); };
__f_0({g: __v_4.g, size:__v_4.size});
for (var __v_0 = 0; __v_0 < 5; __v_0++) __f_0(__v_4);
%OptimizeFunctionOnNextCall(__f_0);
__f_0(__v_4);
__f_0({g: __v_4.g, size:__v_4.size});
