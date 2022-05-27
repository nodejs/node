// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestBuiltin() {
  function createObjects() {
    globalThis.obj1 = {'a': Error};
    globalThis.obj2 = {'b': Error.prototype};
  }
  const realm = Realm.create();
  const {obj1, obj2} = takeAndUseWebSnapshot(
      createObjects, ['obj1', 'obj2'], realm);
  assertTrue(obj1.a === Realm.eval(realm, "Error"));
  assertTrue(obj2.b === Realm.eval(realm, "Error.prototype"));
})();

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
