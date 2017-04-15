// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var largeArray = [];
largeArray[0xFFFF00] = 123;

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
    new Uint8Array(12),
    /asdf/, new RegExp(),
    Object.create, Object, Array,
    Symbol.iterator,
    [][Symbol.iterator](),
    new Map(), new Set(),
    (new Map()).entries(), (new Set()).entries()

];
for (var o of objects) %DebugPrint(o);
