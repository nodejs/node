//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// These tests are to make sure the following:
// o[i] = ...
// ... = o.x -- make sure that we don't hoist the load of o.x,
//              if previous call to o[i] has a chance to kill some o.x.

// Access via o.x (use NaN)
function test1() {
  var a = {};
  var r;
  a.NaN = "orig";
  for (var i = 0; i < 1; ++i) {
    r = a.NaN;
    i = Math.pow(1, Infinity);  // Get NaN which val we know to be a Number.
    a[i] = "new";   // This will kill a.NaN.
    r = a.NaN;      // This must cause a reload.
  }
  return r;
}

// Access via o.x (use Infinity)
function test2() {
  var a = {};
  var r;
  a.Infinity = "orig";
  for (var i = 0; i < 1; ++i) {
    r = a.Infinity;
    i = 1 / 0;      // Get Infinity which val we know to be a Number.
    a[i] = "new";   // This will kill a.Infinity.
    r = a.Infinity; // This must cause a reload.
  }
  return r;
}

// Access via o["x"] (use -Infinity)
function test3() {
  var a = {};
  a[-Infinity] = "orig";
  var r;
  for (var i = 0; i > -1; --i) {
    r = a["-Infinity"];
    i = -1 / 0;
    a[i] = "new";
    r = a["-Infinity"];
  }
  return r;
}

// Access fia o["x"], use float number
function test4() {
  var a = {};
  var r;
  a[1.23] = "orig";
  for (var i = 1; i < 2; ++i) {
    r = a[1.23];
    i = 1 + 0.23;
    a[i] = "new";   // This will kill a[1.23].
    r = a["1.23"];  // This must cause a reload.
  }
  return r;
}

// Access via o[x] (use NaN)
function test5() {
  var a = {};
  var r;
  a.NaN = "orig";
  for (var i = 0; i < 1; ++i) {
    r = a.NaN;
    i = Math.pow(1, Infinity); // Get a NaN that which val we know to be a Number.
    a[i] = "new";   // This will kill a.NaN.
    r = a[NaN];     // This must cause a reload.
  }
  return r;
}

// Access via o[x] (use float number)
function test6() {
  var a = {};
  var r;
  a[1.23] = "orig";
  for (var i = 1; i < 2; ++i) {
    r = a[1.23];
    i = 1 + 0.23;
    a[i] = "new";   // This will kill a[1.23].
    r = a[1.23];    // This must cause a reload.
  }
  return r;
}

var isPass = true;
function reportError(arg1, arg2) {
  WScript.Echo(arg1, arg2);
  isPass = false;
}

var expected = "new";
var r1 = test1();
var r2 = test2();
var r3 = test3();
var r4 = test4();
var r5 = test5();
var r6 = test6();

if (r1 != expected) reportError("bug: r1 =", r1);
if (r2 != expected) reportError("bug: r2 =", r2);
if (r3 != expected) reportError("bug: r3 =", r3);
if (r4 != expected) reportError("bug: r4 =", r4);
if (r5 != expected) reportError("bug: r5 =", r5);
if (r6 != expected) reportError("bug: r6 =", r6);

if (isPass) WScript.Echo("PASS");
