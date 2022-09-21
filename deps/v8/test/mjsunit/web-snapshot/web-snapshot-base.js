// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestMinimal() {
  function createObjects() {
    globalThis.foo = {
      str: 'hello',
      n: 42,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('hello', foo.str);
  assertEquals(42, foo.n);
})();

(function TestEmptyObject() {
  function createObjects() {
    globalThis.foo = {};
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals([], Object.keys(foo));
})();

(function TestNumbers() {
  function createObjects() {
    globalThis.foo = {
      a: 6,
      b: -7,
      c: 7.3,
      d: NaN,
      e: Number.POSITIVE_INFINITY,
      f: Number.NEGATIVE_INFINITY,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(6, foo.a);
  assertEquals(-7, foo.b);
  assertEquals(7.3, foo.c);
  assertEquals(NaN, foo.d);
  assertEquals(Number.POSITIVE_INFINITY, foo.e);
  assertEquals(Number.NEGATIVE_INFINITY, foo.f);
})();

(function TestTopLevelNumbers() {
  function createObjects() {
    globalThis.a = 6;
    globalThis.b = -7;
  }
  const { a, b } = takeAndUseWebSnapshot(createObjects, ['a', 'b']);
  assertEquals(6, a);
  assertEquals(-7, b);
})();

(function TestOddballs() {
  function createObjects() {
    globalThis.foo = {
      a: true,
      b: false,
      c: null,
      d: undefined,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertTrue(foo.a);
  assertFalse(foo.b);
  assertEquals(null, foo.c);
  assertEquals(undefined, foo.d);
})();

(function TestTopLevelOddballs() {
  function createObjects() {
    globalThis.a = true;
    globalThis.b = false;
  }
  const { a, b } = takeAndUseWebSnapshot(createObjects, ['a', 'b']);
  assertTrue(a);
  assertFalse(b);
})();

(function TestStringWithNull() {
  function createObjects() {
    globalThis.s = 'l\0l';
  }
  const { s } = takeAndUseWebSnapshot(createObjects, ['s']);
  assertEquals(108, s.charCodeAt(0));
  assertEquals(0, s.charCodeAt(1));
  assertEquals(108, s.charCodeAt(2));
})();

(function TestTwoByteString() {
  function createObjects() {
    globalThis.s = '\u{1F600}';
  }
  const { s } = takeAndUseWebSnapshot(createObjects, ['s']);
  assertEquals('\u{1F600}', s);
})();

(function TestTwoByteStringWithNull() {
  function createObjects() {
    globalThis.s = 'l\0l\u{1F600}';
  }
  const { s } = takeAndUseWebSnapshot(createObjects, ['s']);
  assertEquals(108, s.charCodeAt(0));
  assertEquals(0, s.charCodeAt(1));
  assertEquals(108, s.charCodeAt(2));
})();

(function TestRegExp() {
  function createObjects() {
    globalThis.foo = {
      re: /ab+c/gi,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('/ab+c/gi', foo.re.toString());
  assertTrue(foo.re.test('aBc'));
  assertFalse(foo.re.test('ac'));
})();

(function TestRegExpNoFlags() {
  function createObjects() {
    globalThis.foo = {
      re: /ab+c/,
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals('/ab+c/', foo.re.toString());
  assertTrue(foo.re.test('abc'));
  assertFalse(foo.re.test('ac'));
})();

(function TestTopLevelRegExp() {
  function createObjects() {
    globalThis.re = /ab+c/gi;
  }
  const { re } = takeAndUseWebSnapshot(createObjects, ['re']);
  assertEquals('/ab+c/gi', re.toString());
  assertTrue(re.test('aBc'));
  assertFalse(re.test('ac'));
})();
