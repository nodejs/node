// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestObjectReferencingObject() {
  function createObjects() {
    globalThis.foo = {
      bar: { baz: 11525 }
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(11525, foo.bar.baz);
})();

(function TestInPlaceStringsInObject() {
  function createObjects() {
    globalThis.foo = {a: 'foo', b: 'bar', c: 'baz'};
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // We cannot test that the strings are really in-place; that's covered by
  // cctests.
  assertEquals('foobarbaz', foo.a + foo.b + foo.c);
})();

(function TestRepeatedInPlaceStringsInObject() {
  function createObjects() {
    globalThis.foo = {a: 'foo', b: 'bar', c: 'foo'};
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  // We cannot test that the strings are really in-place; that's covered by
  // cctests.
  assertEquals('foobarfoo', foo.a + foo.b + foo.c);
})();

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

(function TestObjectWithDictionaryMap() {
  function createObjects() {
    const obj = {};
    // Create an object with dictionary map.
    for (let i = 0; i < 2000; i++){
      obj[`key${i}`] = `value${i}`;
    }
    globalThis.foo = obj;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(2000, Object.keys(foo).length);
  assertEquals(2000, Object.values(foo).length);
  for (let i = 0; i < 2000; i++){
    assertEquals(`value${i}`, foo[`key${i}`]);
  }
})();

(function TwoExportedObjects() {
  function createObjects() {
    globalThis.one = {x: 1};
    globalThis.two = {x: 2};
  }
  const { one, two } = takeAndUseWebSnapshot(createObjects, ['one', 'two']);
  assertEquals(1, one.x);
  assertEquals(2, two.x);
})();

(function TestObjectWithDictionaryElements() {
  function createObjects() {
    globalThis.obj = {
      10: 1,
      100: 2,
      1000: 3,
      10000: 4
    };
  }
  const { obj } = takeAndUseWebSnapshot(createObjects, ['obj']);
  assertEquals(['10', '100', '1000', '10000'], Object.getOwnPropertyNames(obj));
  assertEquals[1, obj[10]];
  assertEquals[2, obj[100]];
  assertEquals[3, obj[1000]];
  assertEquals[4, obj[10000]];
})();

(function TestObjectWithDictionaryElementsWithLargeIndex() {
  function createObjects() {
    globalThis.obj = {};
    globalThis.obj[4394967296] = 'lol';
  }
  const { obj } = takeAndUseWebSnapshot(createObjects, ['obj']);
  assertEquals(['4394967296'], Object.getOwnPropertyNames(obj));
  assertEquals['lol', obj[4394967296]];
})();

(function TestObjectWithSlackElements() {
  function createObjects() {
    globalThis.foo = {};
    globalThis.bar = {};
    for (let i = 0; i < 100; ++i) {
      globalThis.foo[i] = i;
      globalThis.bar[i] = {};
    }
  }
  const { foo, bar } = takeAndUseWebSnapshot(createObjects, ['foo', 'bar']);
  for (let i = 0; i < 100; ++i) {
    assertEquals(i, foo[i]);
    assertEquals({}, bar[i]);
  }
})();
