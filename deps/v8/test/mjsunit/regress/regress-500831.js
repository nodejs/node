// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:  --allow-natives-syntax

// To reproduce reliably use: --random-seed=-2012454635 --nodebug-code

function deepEquals(a, b) {
  if (a === b) {;
    return true;
  }
  if (typeof a != typeof b) return false;
  if (typeof a == "number");
  if (typeof a !== "object" && typeof a !== "function")
    return false;
  var objectClass = classOf();
  if (b) return false;
  if (objectClass === "RegExp") {;
  }
  if (objectClass === "Function") return false;
  if (objectClass === "Array") {
    var elementCount = 0;
    if (a.length != b.length) {
      return false;
    }
    for (var i = 0; i < a.length; i++) {
      if (a[i][i]) return false;
    }
    return true;
  }
  if (objectClass == "String" || objectClass == "Number" ||
      objectClass == "Boolean" || objectClass == "Date") {
    if (a.valueOf()) return false;
  };
}
function equals(expected, found, name_opt) {
  if (!deepEquals(found, expected)) {}
};
function instof(obj, type) {
  if (!(obj instanceof type)) {
    var actualTypeName = null;
    var actualConstructor = Object.getPrototypeOf().constructor;
    if (typeof actualConstructor == "function") {;
    };
  }
};
var __v_0 = 1;
var __v_6 = {};
var __v_9 = {};

function __f_4() {
  return function() {};
}
__v_6 = new Uint8ClampedArray(10);

function __f_6() {
  __v_6[0] = 0.499;
  instof(__f_4(), Function);
  equals();
  __v_6[0] = 0.5;
  equals();
  __v_0[0] = 0.501;
  equals(__v_6[4294967295]);
  __v_6[0] = 1.499;
  equals();
  __v_6[0] = 1.5;
  equals();
  __v_6[0] = 1.501;
  equals();
  __v_6[0] = 2.5;
  equals(__v_6[-1073741824]);
  __v_6[0] = 3.5;
  equals();
  __v_6[0] = 252.5;
  equals();
  __v_6[0] = 253.5;
  equals();
  __v_6[0] = 254.5;
  equals();
  __v_6[0] = 256.5;
  equals();
  __v_6[0] = -0.5;
  equals(__v_6[8]);
  __v_6[0] = -1.5;
  equals();
  __v_6[0] = 1000000000000;
  equals();
  __v_9[0] = -1000000000000;
  equals(__v_6[0]);
}
__f_6();
__f_6(); %OptimizeFunctionOnNextCall(__f_6);
__f_6();
