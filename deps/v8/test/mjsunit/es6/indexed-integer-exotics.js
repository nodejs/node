// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Object.prototype["10"] = "unreachable";
Object.prototype["7"] = "unreachable";
Object.prototype["-1"] = "unreachable";
Object.prototype["-0"] = "unreachable";
Object.prototype["4294967295"] = "unreachable";

var array = new Int32Array(10);

function check() {
  for (var i = 0; i < 4; i++) {
    assertEquals(undefined, array["-1"]);
    assertEquals(undefined, array["-0"]);
    assertEquals(undefined, array["10"]);
    assertEquals(undefined, array["4294967295"]);
  }
  assertEquals("unreachable", array.__proto__["-1"]);
  assertEquals("unreachable", array.__proto__["-0"]);
  assertEquals("unreachable", array.__proto__["10"]);
  assertEquals("unreachable", array.__proto__["4294967295"]);
}

check();

array["-1"] = "unreachable";
array["-0"] = "unreachable";
array["10"] = "unreachable";
array["4294967295"] = "unreachable";

check();

delete array["-0"];
delete array["-1"];
delete array["10"];
delete array["4294967295"];

assertEquals(undefined, Object.getOwnPropertyDescriptor(array, "-1"));
assertEquals(undefined, Object.getOwnPropertyDescriptor(array, "-0"));
assertEquals(undefined, Object.getOwnPropertyDescriptor(array, "10"));
assertEquals(undefined, Object.getOwnPropertyDescriptor(array, "4294967295"));
assertEquals(10, Object.keys(array).length);

check();

function f() { return array["-1"]; }

%PrepareFunctionForOptimization(f);
for (var i = 0; i < 3; i++) {
  assertEquals(undefined, f());
}
%OptimizeFunctionOnNextCall(f);
assertEquals(undefined, f());

assertThrows('Object.defineProperty(new Int32Array(100), -1, {value: 1})');
// -0 gets converted to the string "0", so use "-0" instead.
assertThrows('Object.defineProperty(new Int32Array(100), "-0", {value: 1})');
assertThrows('Object.defineProperty(new Int32Array(100), -10, {value: 1})');
assertThrows('Object.defineProperty(new Int32Array(), 4294967295, {value: 1})');

check();
