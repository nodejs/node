// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() {return globalThis.foo}; foo");
  assertSame(foo(), foo);
}

{
  const realm = Realm.createAllowCrossRealmAccess();
  const foo = Realm.eval(realm, "function foo() {return globalThis.foo}; foo");
  assertSame(foo(), foo);
  Realm.detachGlobal(realm);
}
