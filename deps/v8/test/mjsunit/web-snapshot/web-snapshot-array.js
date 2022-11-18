// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestArray() {
  function createObjects() {
    globalThis.foo = {
      array: [5, 6, 7]
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([5, 6, 7], foo.array);
})();

(function TestPackedDoubleElementsArray() {
  function createObjects() {
    globalThis.foo = [1.2, 2.3];
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([1.2, 2.3], foo);
})();

(function TestArrayContainingDoubleAndSmi() {
  function createObjects() {
    globalThis.foo = [1.2, 1];
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([1.2, 1], foo);
})();

(function TestArrayContainingDoubleAndObject() {
  function createObjects() {
    globalThis.foo = [1.2, {'key': 'value'}];
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([1.2, {'key': 'value'}], foo);
})();

(function TestEmptyArray() {
  function createObjects() {
    globalThis.foo = {
      array: []
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(0, foo.array.length);
  assertEquals([], foo.array);
})();

(function TestArrayContainingArray() {
  function createObjects() {
    globalThis.foo = {
      array: [[2, 3], [4, 5]]
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([[2, 3], [4, 5]], foo.array);
})();

(function TestArrayContainingObject() {
  function createObjects() {
    globalThis.foo = {
      array: [{ a: 1 }, { b: 2 }]
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(1, foo.array[0].a);
  assertEquals(2, foo.array[1].b);
})();

(function TestArrayContainingFunction() {
  function createObjects() {
    globalThis.foo = {
      array: [function () { return 5; }]
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(5, foo.array[0]());
})();

(function TestInPlaceStringsInArray() {
  function createObjects() {
    globalThis.foo = {
      array: ['foo', 'bar', 'baz']
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // We cannot test that the strings are really in-place; that's covered by
  // cctests.
  assertEquals('foobarbaz', foo.array.join(''));
})();

(function TestRepeatedInPlaceStringsInArray() {
  function createObjects() {
    globalThis.foo = {
      array: ['foo', 'bar', 'foo']
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // We cannot test that the strings are really in-place; that's covered by
  // cctests.
  assertEquals('foobarfoo', foo.array.join(''));
})();

(function TestArrayWithSlackElements() {
  function createObjects() {
    globalThis.foo = {
      array: [],
      doubleArray: [],
      objectArray: []
    };
    for (let i = 0; i < 100; ++i) {
      globalThis.foo.array.push(i);
      globalThis.foo.doubleArray.push(i + 0.1);
      globalThis.foo.objectArray.push({});
    }
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(100, foo.array.length);
  assertEquals(100, foo.doubleArray.length);
  assertEquals(100, foo.objectArray.length);
  for (let i = 0; i < 100; ++i){
    assertEquals(i, foo.array[i]);
    assertEquals(i + 0.1, foo.doubleArray[i]);
    assertEquals({}, foo.objectArray[i]);
  }
})();
