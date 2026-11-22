// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const other_realm = Realm.create();
Realm.shared = {};

Realm.eval(other_realm, `
  function victim(secret) {
    Realm.shared.spy();
  }
  function victim_caller() {
    victim("secret");
  }
  Realm.shared.victim = victim;
  Realm.shared.victim_caller = victim_caller;
`);

const args_getter =
    Object.getOwnPropertyDescriptor(Function.prototype, 'arguments').get;
const caller_getter =
    Object.getOwnPropertyDescriptor(Function.prototype, 'caller').get;

let spy_called = false;
Realm.shared.spy = function() {
  spy_called = true;
  assertEquals(null, caller_getter.call(Realm.shared.victim));
  assertEquals(null, Realm.shared.victim.caller);
  assertEquals(null, args_getter.call(Realm.shared.victim));
  assertEquals(null, Realm.shared.victim.arguments);
};

Realm.eval(other_realm, `Realm.shared.victim_caller();`);
assertTrue(spy_called);
