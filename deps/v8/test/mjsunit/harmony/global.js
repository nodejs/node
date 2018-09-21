// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-global

assertEquals(globalThis, this);
assertEquals(this.globalThis, this);
assertEquals(globalThis.globalThis, this);
assertEquals(globalThis.globalThis.globalThis, this);
assertEquals(globalThis.globalThis.globalThis.globalThis, this);

{
  const realm = Realm.create();
  assertEquals(Realm.global(realm), Realm.eval(realm, 'globalThis'));
  assertTrue(Realm.global(realm) !== globalThis);
}

{
  const descriptor = Object.getOwnPropertyDescriptor(
    this,
    'globalThis'
  );
  assertEquals(descriptor.value, this);
  assertEquals(descriptor.writable, true);
  assertEquals(descriptor.enumerable, false);
  assertEquals(descriptor.configurable, true);
}
