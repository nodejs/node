// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

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
  assertSame(Realm.eval(realm, 'Error'), obj1.a);
  assertSame(Realm.eval(realm, 'Error.prototype'), obj2.b);
})();

(function TestInheritFromBuiltin() {
  function createObjects() {
    function inherit(subclass, superclass) {
      function middle() {}
      middle.prototype = superclass.prototype;
      subclass.prototype = new middle();
      subclass.prototype.constructor = subclass;
    };
    function MyError() {}
    inherit(MyError, Error);
    globalThis.MyError = MyError;
  }
  const realm = Realm.create();
  const {MyError} = takeAndUseWebSnapshot(createObjects, ['MyError'], realm);
  const obj = new MyError();
  assertSame(Realm.eval(realm, 'Error.prototype'), obj.__proto__.__proto__);
})();
