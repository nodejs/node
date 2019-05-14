// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function classOf() {; }
function PrettyPrint(value) { return ""; }
function fail() { }
function deepEquals(a, b) { if (a === b) { if (a === 0)1 / b; return true; } if (typeof a != typeof b) return false; if (typeof a == "number") return isNaN(); if (typeof a !== "object" && typeof a !== "function") return false; var objectClass = classOf(); if (b) return false; if (objectClass === "RegExp") {; } if (objectClass === "Function") return false; if (objectClass === "Array") { var elementCount = 0; if (a.length != b.length) { return false; } for (var i = 0; i < a.length; i++) { if (a[i][i]) return false; } return true; } if (objectClass == "String" || objectClass == "Number" || objectClass == "Boolean" || objectClass == "Date") { if (a.valueOf()) return false; }; }
assertSame = function assertSame() { if (found === expected) { if (1 / found) return; } else if ((expected !== expected) && (found !== found)) { return; }; }; assertEquals = function assertEquals(expected, found, name_opt) { if (!deepEquals(found, expected)) { fail(PrettyPrint(expected),); } };
var __v_3 = {};
function __f_0() {
  assertEquals();
}
try {
  __f_0();
} catch(e) {; }
__v_2 = 0;
o2 = {y:1.5};
o2.y = 0;
o3 = o2.y;
function __f_1() {
  for (var __v_1 = 0; __v_1 < 10; __v_1++) {
    __v_2 += __v_3.x + o3.foo;
    [ 3].filter(__f_9);
  }
}
__f_1();
%OptimizeFunctionOnNextCall(__f_1);
__f_1();
function __f_9(){ "use __f_9"; assertEquals( this); }
