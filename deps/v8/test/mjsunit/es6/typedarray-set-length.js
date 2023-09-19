// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array
];

var lengthCalled = false;
function lengthValue() {
  assertFalse(lengthCalled);
  lengthCalled = true;
  return 5;
}

// ToLength should convert these to usable lengths.
var goodNonIntegerLengths = [
  function() { return 4.6; },
  function() { return -5; },
  function() { return NaN; },
  function() { return "5"; },
  function() { return "abc"; },
  function() { return true; },
  function() { return null; },
  function() { return undefined; }
];

// This will fail if you use ToLength on it.
function badNonIntegerLength() {
  return Symbol("5");
}

for (var constructor of typedArrayConstructors) {
  lengthCalled = false;
  var a = new constructor(10);
  a.set({length: {valueOf: lengthValue}});
  assertTrue(lengthCalled);

  for (var lengthFun of goodNonIntegerLengths) {
    a.set({length: {valueOf: lengthFun}});
  }

  assertThrows(function() {
    a.set({length: {valueOf: badNonIntegerLength}});
  }, TypeError);
}
