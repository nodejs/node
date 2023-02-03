// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestHoleySmiElementsArray() {
  function createObjects() {
    globalThis.foo = [1,,2];
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([1,,2], foo);
})();

(function TestHoleyElementsArray() {
  function createObjects() {
    globalThis.foo = [1,,'123'];
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([1,,'123'], foo);
})();

(function TestHoleyArrayContainingDoubleAndSmi() {
  function createObjects() {
    globalThis.foo = [1.2, , 1];
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([1.2, , 1], foo);
})();

(function TestHoleyArrayContainingDoubleAndObject() {
  function createObjects() {
    globalThis.foo = [1.2, , {'key': 'value'}];
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([1.2, , {'key': 'value'}], foo);
})();

(function TestHoleyDoubleElementsArray() {
  function createObjects() {
    globalThis.foo = [1.2, , 2.3];
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([1.2, , 2.3], foo);
})();
