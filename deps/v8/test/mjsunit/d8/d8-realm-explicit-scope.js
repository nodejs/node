// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestExplicitRealmScope() {
  const r1 = Realm.create();
  Realm.eval(r1, 'globalThis.foo = 42;');
  const foo1 = Realm.eval(r1, 'globalThis.foo;');
  assertEquals(42, foo1);
  const r2 = Realm.create();
  const foo2 = Realm.eval(r2, 'globalThis.foo;');
  assertEquals(undefined, foo2);
})();
