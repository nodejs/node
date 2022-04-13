// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestObjectWithPackedElements() {
  function createObjects() {
    globalThis.foo = {
      '0': 'zero', '1': 'one', '2': 'two', '3': 'three'
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // Objects always get HOLEY_ELEMENTS; no PACKED or SMI_ELEMENTS.
  const elementsKindTest = {0: 0, 1: 1, 2: 2};
  assertFalse(%HasPackedElements(elementsKindTest));
  assertFalse(%HasSmiElements(elementsKindTest));

  assertFalse(%HasPackedElements(foo));
  assertFalse(%HasSmiElements(foo));
  assertEquals('zeroonetwothree', foo[0] + foo[1] + foo[2] + foo[3]);
})();

(function TestObjectWithPackedSmiElements() {
  function createObjects() {
    globalThis.foo = {
      '0': 0, '1': 1, '2': 2, '3': 3
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertFalse(%HasPackedElements(foo));
  assertFalse(%HasSmiElements(foo));
  assertEquals('0123', '' + foo[0] + foo[1] + foo[2] + foo[3]);
})();

(function TestObjectWithHoleyElements() {
  function createObjects() {
    globalThis.foo = {
      '1': 'a', '11': 'b', '111': 'c'
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertFalse(%HasPackedElements(foo));
  assertFalse(%HasSmiElements(foo));
  assertEquals('abc', foo[1] + foo[11] + foo[111]);
})();

(function TestObjectWithHoleySmiElements() {
  function createObjects() {
    globalThis.foo = {
      '1': 0, '11': 1, '111': 2
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertFalse(%HasPackedElements(foo));
  assertFalse(%HasSmiElements(foo));
  assertEquals('012', '' + foo[1] + foo[11] + foo[111]);
})();

(function TestObjectWithPropertiesAndElements() {
  function createObjects() {
    globalThis.foo = {
      'prop1': 'value1', '1': 'a', 'prop2': 'value2', '11': 'b', '111': 'c'
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertFalse(%HasPackedElements(foo));
  assertFalse(%HasSmiElements(foo));
  assertEquals('abc', foo[1] + foo[11] + foo[111]);
  assertEquals('value1value2', foo.prop1 + foo.prop2);
})();

(function TestObjectsWithSamePropertiesButDifferentElementsKind() {
  function createObjects() {
    globalThis.foo = {
      'prop1': 'value1', 'prop2': 'value2', '1': 'a', '11': 'b', '111': 'c'
    };
    globalThis.bar = {
      'prop1': 'value1', 'prop2': 'value2', '0': 0, '1': 0
    }
  }
  const { foo, bar } = takeAndUseWebSnapshot(createObjects, ['foo', 'bar']);
  assertFalse(%HasPackedElements(foo));
  assertFalse(%HasSmiElements(foo));
  assertEquals('abc', foo[1] + foo[11] + foo[111]);
  assertEquals('value1value2', foo.prop1 + foo.prop2);
  assertFalse(%HasPackedElements(bar));
  assertFalse(%HasSmiElements(bar));
  assertEquals('00', '' + bar[0] + bar[1]);
  assertEquals('value1value2', bar.prop1 + bar.prop2);
})();

(function TestObjectWithEmptyMap() {
  function createObjects() {
    globalThis.foo = [{a:1}, {}, {b: 2}];
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(1, foo[0].a);
  assertEquals(2, foo[2].b);
})();
