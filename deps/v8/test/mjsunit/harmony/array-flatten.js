// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-array-flatten

assertEquals(Array.prototype.flatten.length, 0);
assertEquals(Array.prototype.flatten.name, 'flatten');

const input = [1, [2], [[3]]];

assertEquals(input.flatten(),          [1, 2, [3]]);
assertEquals(input.flatten(1),         [1, 2, [3]]);
assertEquals(input.flatten(true),      [1, 2, [3]]);
assertEquals(input.flatten(undefined), [1, 2, [3]]);

assertEquals(input.flatten(-Infinity), [1, [2], [[3]]]);
assertEquals(input.flatten(-1),        [1, [2], [[3]]]);
assertEquals(input.flatten(-0),        [1, [2], [[3]]]);
assertEquals(input.flatten(0),         [1, [2], [[3]]]);
assertEquals(input.flatten(false),     [1, [2], [[3]]]);
assertEquals(input.flatten(null),      [1, [2], [[3]]]);
assertEquals(input.flatten(''),        [1, [2], [[3]]]);
assertEquals(input.flatten('foo'),     [1, [2], [[3]]]);
assertEquals(input.flatten(/./),       [1, [2], [[3]]]);
assertEquals(input.flatten([]),        [1, [2], [[3]]]);
assertEquals(input.flatten({}),        [1, [2], [[3]]]);
assertEquals(
  input.flatten(new Proxy({}, {})),    [1, [2], [[3]]]);
assertEquals(input.flatten((x) => x),  [1, [2], [[3]]]);
assertEquals(
  input.flatten(String),               [1, [2], [[3]]]);

assertEquals(input.flatten(2),         [1, 2, 3]);
assertEquals(input.flatten(Infinity),  [1, 2, 3]);

assertThrows(() => { input.flatten(Symbol()); }, TypeError);
assertThrows(() => { input.flatten(Object.create(null)); }, TypeError);
