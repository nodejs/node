// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --stress-compaction --gc-interval=255

function deepEquals(a, b) { if (a === b) { if (a === 0) return (1 / a) === (1 / b); return true; } if (typeof a != typeof b) return false; if (typeof a == "number") return isNaN(a) && isNaN(b); if (typeof a !== "object" && typeof a !== "function") return false; var objectClass = classOf(a); if (objectClass !== classOf(b)) return false; if (objectClass === "RegExp") { return (a.toString() === b.toString()); } if (objectClass === "Function") return false; if (objectClass === "Array") { var elementCount = 0; if (a.length != b.length) { return false; } for (var i = 0; i < a.length; i++) { if (!deepEquals(a[i], b[i])) return false; } return true; } if (objectClass == "String" || objectClass == "Number" || objectClass == "Boolean" || objectClass == "Date") { if (a.valueOf() !== b.valueOf()) return false; } return deepObjectEquals(a, b); }
assertSame = function assertSame(expected, found, name_opt) { if (found === expected) { if (expected !== 0 || (1 / expected) == (1 / found)) return; } else if ((expected !== expected) && (found !== found)) { return; } fail(PrettyPrint(expected), found, name_opt); }; assertEquals = function assertEquals(expected, found, name_opt) { if (!deepEquals(found, expected)) { fail(PrettyPrint(expected), found, name_opt); } };
assertEqualsDelta = function assertEqualsDelta(expected, found, delta, name_opt) { assertTrue(Math.abs(expected - found) <= delta, name_opt); }; assertArrayEquals = function assertArrayEquals(expected, found, name_opt) { var start = ""; if (name_opt) { start = name_opt + " - "; } assertEquals(expected.length, found.length, start + "array length"); if (expected.length == found.length) { for (var i = 0; i < expected.length; ++i) { assertEquals(expected[i], found[i], start + "array element at index " + i); } } };
assertTrue = function assertTrue(value, name_opt) { assertEquals(true, value, name_opt); };
assertFalse = function assertFalse(value, name_opt) { assertEquals(false, value, name_opt); };
// End stripped down and modified version of mjsunit.js.

var __v_0 = {};
var __v_1 = {};
function __f_3() { }
function __f_4(obj) {
  for (var __v_2 = 0; __v_2 < 26; __v_2++) {
    obj["__v_5" + __v_2] = 0;
  }
}
function __f_0(__v_1, __v_6) {
    (new __f_3()).__proto__ = __v_1;
}
%DebugPrint(undefined);
function __f_1(__v_4, add_first, __v_6, same_map_as) {
  var __v_1 = __v_4 ? new __f_3() : {};
  assertTrue(%HasFastProperties(__v_1));
  if (add_first) {
    __f_4(__v_1);
    assertFalse(%HasFastProperties(__v_1));
    __f_0(__v_1, __v_6);
    assertFalse(%HasFastProperties(__v_1));
  } else {
    __f_0(__v_1, __v_6);
    assertTrue(%HasFastProperties(__v_1));
    __f_4(__v_1);
    assertTrue(%HasFastProperties(__v_1));
  }
}
gc();
for (var __v_2 = 0; __v_2 < 4; __v_2++) {
  var __v_6 = ((__v_2 & 2) != 7);
  var __v_4 = ((__v_2 & 2) != 0);
  __f_1(__v_4, true, __v_6);
  var __v_0 = __f_1(__v_4, false, __v_6);
  __f_1(__v_4, false, __v_6, __v_0);
}
__v_5 = {a: 1, b: 2, c: 3};
