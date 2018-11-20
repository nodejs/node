// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var largeArray = [];
largeArray[0xFFFF00] = 123;

function sloppyArguments() {
  return arguments;
}

function sloppyArguments2(a, b) {
  return arguments;
}

function slowSloppyArguments() {
  arguments[0xFFFFF] = -1;
  return arguments;
}

function slowSloppyArguments2(a, b) {
  arguments[0xFFFFF] = -1;
  return arguments;
}


var objects = [
    this,
    true, false, null, undefined,
    1, -1, 1.1, -2.2, -0, 0,
    Infinity, -Infinity, NaN,
    "aasdfasdfasdfasdf", "a"+"b",
    {}, {1:1}, {a:1}, {1:1, 2:2}, Object.create(null),
    [], [{}, {}], [1, 1, 1], [1.1, 1.1, 1.1, 1.1, 2], largeArray,
    new Proxy({},{}),
    new Date(), new String(" a"),
    new Uint8Array(12), new Float32Array([1, 2, 4, 5]),
    new Uint8ClampedArray(2048),
    /asdf/, new RegExp(),
    Object.create, Object, Array,
    Symbol.iterator,
    [][Symbol.iterator](),
    new Map(), new Set(),
    (new Map()).entries(), (new Set()).entries(),
    sloppyArguments(), sloppyArguments(1, 2, 3, 4),
    sloppyArguments2(), sloppyArguments2(1, 2, 3, 4),
    slowSloppyArguments(), slowSloppyArguments(1, 2, 3, 4),
    slowSloppyArguments2(), slowSloppyArguments2(1, 2, 3, 4),

];
for (var o of objects) %DebugPrint(o);
