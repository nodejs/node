// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-array-flat

assertEquals(Array.prototype.flat.length, 0);
assertEquals(Array.prototype.flat.name, 'flat');

{
  const input = [1, [2], [[3]]];

  assertEquals(input.flat(),          [1, 2, [3]]);
  assertEquals(input.flat(1),         [1, 2, [3]]);
  assertEquals(input.flat(true),      [1, 2, [3]]);
  assertEquals(input.flat(undefined), [1, 2, [3]]);

  assertEquals(input.flat(-Infinity), [1, [2], [[3]]]);
  assertEquals(input.flat(-1),        [1, [2], [[3]]]);
  assertEquals(input.flat(-0),        [1, [2], [[3]]]);
  assertEquals(input.flat(0),         [1, [2], [[3]]]);
  assertEquals(input.flat(false),     [1, [2], [[3]]]);
  assertEquals(input.flat(null),      [1, [2], [[3]]]);
  assertEquals(input.flat(''),        [1, [2], [[3]]]);
  assertEquals(input.flat('foo'),     [1, [2], [[3]]]);
  assertEquals(input.flat(/./),       [1, [2], [[3]]]);
  assertEquals(input.flat([]),        [1, [2], [[3]]]);
  assertEquals(input.flat({}),        [1, [2], [[3]]]);
  assertEquals(
    input.flat(new Proxy({}, {})),    [1, [2], [[3]]]);
  assertEquals(input.flat((x) => x),  [1, [2], [[3]]]);
  assertEquals(
    input.flat(String),               [1, [2], [[3]]]);

  assertEquals(input.flat(2),         [1, 2, 3]);
  assertEquals(input.flat(Infinity),  [1, 2, 3]);

  assertThrows(() => { input.flat(Symbol()); }, TypeError);
  assertThrows(() => { input.flat(Object.create(null)); }, TypeError);
}

{
  const input = { 0: 'a', 1: 'b', 2: 'c', length: 'wat' };
  assertEquals(Array.prototype.flat.call(input, Infinity), []);
}

{
  let count = 0;
  const input = {
    get length() { ++count; return 0; }
  };
  const result = Array.prototype.flat.call(input, Infinity);
  assertEquals(count, 1);
}

{
  const descriptor = Object.getOwnPropertyDescriptor(
    Array.prototype,
    'flat'
  );
  assertEquals(descriptor.value, Array.prototype.flat);
  assertEquals(descriptor.writable, true);
  assertEquals(descriptor.enumerable, false);
  assertEquals(descriptor.configurable, true);
}
