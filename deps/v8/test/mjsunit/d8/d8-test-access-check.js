// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 1. Allowed access.
{
  let obj = d8.test.createAccessCheckedObject(true);
  obj.a = 42;
  assertEquals(42, obj.a);

  // 2. Denied access throws TypeError.
  d8.test.setAccessPolicy(obj, false);
  assertThrows(() => obj.a, TypeError);

  // 3. Restore access.
  d8.test.setAccessPolicy(obj, true);
  assertEquals(42, obj.a);
}

// 4. "same-context" policy.
{
  let obj = d8.test.createAccessCheckedObject("same-context");
  obj.a = 42;
  assertEquals(42, obj.a);

  let realm = Realm.create();
  Realm.shared = obj;
  let otherTypeError = Realm.eval(realm, "TypeError");
  assertThrows(() => Realm.eval(realm, "Realm.shared.a"), otherTypeError);

  d8.test.setAccessPolicy(obj, true);
  assertEquals(42, Realm.eval(realm, "Realm.shared.a"));
}

// 5. "security-token" policy.
{
  let obj = d8.test.createAccessCheckedObject("security-token");
  obj.a = 42;
  assertEquals(42, obj.a);

  let realm = Realm.create();
  Realm.shared = obj;
  let otherTypeError = Realm.eval(realm, "TypeError");
  assertThrows(() => Realm.eval(realm, "Realm.shared.a"), otherTypeError);

  let sameTokenRealm = Realm.createAllowCrossRealmAccess();
  assertEquals(42, Realm.eval(sameTokenRealm, "Realm.shared.a"));

  d8.test.setAccessPolicy(obj, true);
  assertEquals(42, Realm.eval(realm, "Realm.shared.a"));
}

// 6. Input validation.
assertThrows(() => d8.test.createAccessCheckedObject(), Error);
assertThrows(() => d8.test.createAccessCheckedObject(123), Error);
assertThrows(() => d8.test.createAccessCheckedObject("bad"), Error);
assertThrows(() => d8.test.createAccessCheckedObject({}), Error);
assertThrows(() => d8.test.createAccessCheckedObject(() => true), Error);

let obj = d8.test.createAccessCheckedObject(true);
assertThrows(() => d8.test.setAccessPolicy(obj, "bad"), Error);
assertThrows(() => d8.test.setAccessPolicy(obj, 123), Error);
assertThrows(() => d8.test.setAccessPolicy({}, true), Error);
assertThrows(() => d8.test.setAccessPolicy(123, true), Error);
