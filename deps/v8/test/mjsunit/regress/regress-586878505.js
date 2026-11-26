// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const realm = Realm.create();
const other = Realm.global(realm);
Realm.eval(realm, `
  for (const key of ['name', 'length']) {
    Object.defineProperty(this, key, {
      get() { return 'secret'; },
      set() {},
      configurable: true,
    });
  }
`);

assertThrows(
    () => Object.prototype.__lookupGetter__.call(other, 'name'), TypeError);
assertThrows(
    () => Object.prototype.__lookupSetter__.call(other, 'name'), TypeError);

const plain = Object.setPrototypeOf({}, other);
assertThrows(
    () => Object.prototype.__lookupGetter__.call(plain, 'name'), TypeError);
assertThrows(
    () => Object.prototype.__lookupSetter__.call(plain, 'name'), TypeError);

for (const [carrier, key] of [
  [function() {}, 'name'],
  [function() {}, 'length'],
  [[], 'length'],
  [new String('a'), 'length'],
]) {
  Object.setPrototypeOf(carrier, other);
  assertEquals(undefined, Object.prototype.__lookupGetter__.call(carrier, key));
  assertEquals(undefined, Object.prototype.__lookupSetter__.call(carrier, key));
}

const proto = {get name() { return 1; }, set name(v) {}};
const fn = Object.setPrototypeOf(function() {}, proto);
assertEquals(undefined, fn.__lookupGetter__('name'));
assertEquals(undefined, fn.__lookupSetter__('name'));
