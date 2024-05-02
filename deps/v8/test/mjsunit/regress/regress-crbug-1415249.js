// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan
{
  const realm = Realm.createAllowCrossRealmAccess();
  const global = Realm.global(realm);
  function Base() { return global; }
  let i = 0;
  class Klass extends Base {
    field = i++;
  }
  let a = new Klass();
  assertEquals(a.field, 0);
  a = new Klass();
  assertEquals(a.field, 1);
}

{
  const realm = Realm.create();
  const global = Realm.global(realm);
  function Base() { return global; }
  let i = 0;
  class Klass extends Base {
    field = i++;
  }
  assertThrows(() => new Klass(), Error, /no access/);
  assertThrows(() => new Klass(), Error, /no access/);
}
