// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-array-flatten --allow-natives-syntax

assertEquals(Array.prototype.flatMap.length, 1);
assertEquals(Array.prototype.flatMap.name, 'flatMap');

assertEquals(
  [1, 2, 3, 4].flatMap((element) => [element, element ** 2]),
  [1, 1, 2, 4, 3, 9, 4, 16]
);
assertEquals(
  [1, 2, 3, 4].flatMap((element) => [[element, element ** 2]]),
  [[1, 1], [2, 4], [3, 9], [4, 16]]
);

const elements = new Set([
  -Infinity,
  -1,
  -0,
  +0,
  +1,
  Infinity,
  null,
  undefined,
  true,
  false,
  '',
  'foo',
  /./,
  [],
  {},
  Object.create(null),
  new Proxy({}, {}),
  Symbol(),
  x => x ** 2,
  String
]);

for (const value of elements) {
  assertEquals(
    [value].flatMap((element) => [element, element]),
    [value, value]
  );
}

const array = [42];
assertEquals(
  [array].flatMap((element) => [element, element]),
  [array, array]
);

const nonCallables = new Set([
  -Infinity,
  -1,
  -0,
  +0,
  +1,
  Infinity,
  null,
  undefined,
  true,
  false,
  '',
  'foo',
  /./,
  [],
  {},
  Object.create(null),
  new Proxy({}, {}),
  Symbol(),
]);
for (const nonCallable of nonCallables) {
  assertThrows(() => {
    [].flatMap(nonCallable);
  }, TypeError);
}

const object = {
  foo: 42,
  get length() {
    object.foo = 0;
  }
};
const result = [object].flatMap((element) => [element, element]);
%HeapObjectVerify(result);
assertEquals(result, [object, object]);
assertEquals(result[0].foo, 42);

assertThrows(() => {
  Array.prototype.flatMap.call(null, (element) => element);
}, TypeError);
assertThrows(() => {
  Array.prototype.flatMap.call(undefined, (element) => element);
}, TypeError);

assertEquals(
  Array.prototype.flatMap.call(
    {
      length: 1,
      0: 'a',
      1: 'b',
    },
    (element) => element
  ),
  ['a']
);
assertEquals(
  Array.prototype.flatMap.call(
    {
      length: 2,
      0: 'a',
      1: 'b',
    },
    (element) => element
  ),
  ['a', 'b']
);
