// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestObjectPrototype() {
  function createObjects() {
    globalThis.obj = {a: 1, __proto__: {x: 1}};
  }
  const realm = Realm.create();
  const {obj} = takeAndUseWebSnapshot(createObjects, ['obj'], realm);
  assertEquals(1, obj.x);
  assertEquals(1, obj.__proto__.x);
  assertSame(Realm.eval(realm, 'Object.prototype'), obj.__proto__.__proto__);
})();

(function TestEmptyObjectPrototype() {
  function createObjects() {
    globalThis.obj = {__proto__: {x: 1}};
  }
  const realm = Realm.create();
  const {obj} = takeAndUseWebSnapshot(createObjects, ['obj'], realm);
  assertEquals(1, obj.x);
  assertEquals(1, obj.__proto__.x);
  assertSame(Realm.eval(realm, 'Object.prototype'), obj.__proto__.__proto__);
})();

(function TestDictionaryObjectPrototype() {
  function createObjects() {
    const obj = {};
    // Create an object with dictionary map.
    for (let i = 0; i < 2000; i++){
      obj[`key${i}`] = `value${i}`;
    }
    obj.__proto__ = {x: 1};
    globalThis.foo = obj;
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(2000, Object.keys(foo).length);
  assertEquals(2000, Object.values(foo).length);
  for (let i = 0; i < 2000; i++){
    assertEquals(`value${i}`, foo[`key${i}`]);
  }
  assertEquals(1, foo.x);
  assertEquals(1, foo.__proto__.x);
})();

(function TestNullPrototype() {
  function createObjects() {
    globalThis.foo = Object.create(null);
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(null, Object.getPrototypeOf(foo));
})();

(function TestDefaultObjectProto() {
  function createObjects() {
    globalThis.foo = {
      str: 'hello',
      n: 42,
    };
  }
  const realm = Realm.create();
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo'], realm);
  assertSame(Realm.eval(realm, 'Object.prototype'), Object.getPrototypeOf(foo));
})();

(function TestEmptyObjectProto() {
  function createObjects() {
    globalThis.foo = {};
  }
  const realm = Realm.create();
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo'], realm);
  assertSame(Realm.eval(realm, 'Object.prototype'), Object.getPrototypeOf(foo));
})();

(function TestObjectProto() {
  function createObjects() {
    globalThis.foo = {
      __proto__ : {x : 10},
      y: 11
    };
  }
  const { foo } = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertEquals(10, Object.getPrototypeOf(foo).x);
})();

(function TestObjectProtoInSnapshot() {
  function createObjects() {
    globalThis.o1 = { x: 10};
    globalThis.o2 = {
      __proto__ : o1,
      y: 11
    };
  }
  const realm = Realm.create();
  const { o1, o2 } = takeAndUseWebSnapshot(createObjects, ['o1', 'o2'], realm);
  assertSame(o1, Object.getPrototypeOf(o2));
  assertSame(Realm.eval(realm, 'Object.prototype'), Object.getPrototypeOf(o1));
})();
