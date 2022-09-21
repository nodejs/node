// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestDictionaryElementsArray() {
  function createObjects() {
    const array = [];
    // Add a large index to force dictionary elements.
    array[2 ** 30] = 10;
    for (let i = 0; i < 10; i++) {
      array[i * 101] = i;
    }
    globalThis.foo = array;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertTrue(%HasDictionaryElements(foo));
  assertEquals(2 ** 30 + 1, foo.length);
  for (let i = 0; i < 10; i++) {
    assertEquals(i, foo[i * 101]);
  }
})();

(function TestDictionaryElementsArrayContainingArray() {
  function createObjects() {
    const array = [];
    // Add a large index to force dictionary elements.
    array[2 ** 30] = 10;
    for (let i = 0; i < 10; i++) {
      array[i * 101] = [i];
    }
    globalThis.foo = array;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertTrue(%HasDictionaryElements(foo));
  assertEquals(2 ** 30 + 1, foo.length);
  for (let i = 0; i < 10; i++) {
    assertEquals([i], foo[i * 101]);
  }
})();

(function TestDictionaryElementsArrayContainingObject() {
  function createObjects() {
    const array = [];
    // Add a large index to force dictionary elements.
    array[2 ** 30] = 10;
    for (let i = 0; i < 10; i++) {
      array[i * 101] = {i: i};
    }
    globalThis.foo = array;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertTrue(%HasDictionaryElements(foo));
  assertEquals(2 ** 30 + 1, foo.length);
  for (let i = 0; i < 10; i++) {
    assertEquals({i: i}, foo[i * 101]);
  }
})();

(function TestDictionaryElementsArrayContainingFunction() {
  function createObjects() {
    const array = [];
    // Add a large index to force dictionary elements.
    array[2 ** 30] = 10;
    for (let i = 0; i < 10; i++) {
      array[i * 101] = function() { return i; };
    }
    globalThis.foo = array;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertTrue(%HasDictionaryElements(foo));
  assertEquals(2 ** 30 + 1, foo.length);
  for (let i = 0; i < 10; i++) {
    assertEquals(i, foo[i * 101]());
  }
})();

(function TestDictionaryElementsArrayContainingString() {
  function createObjects() {
    const array = [];
    // Add a large index to force dictionary elements.
    array[2 ** 30] = 10;
    for (let i = 0; i < 10; i++) {
      array[i * 101] = `${i}`;
    }
    globalThis.foo = array;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertTrue(%HasDictionaryElements(foo));
  assertEquals(2 ** 30 + 1, foo.length);
  for (let i = 0; i < 10; i++) {
    assertEquals(`${i}`, foo[i * 101]);
  }
})();
