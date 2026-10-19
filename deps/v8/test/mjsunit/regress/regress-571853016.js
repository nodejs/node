// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

(function TestCrossRealmArrayPrototype() {
  const r = Realm.create();
  Realm.eval(
      r,
      'Object.defineProperty(Object.prototype, "prop", {set(_){}, configurable:true})');
  const ap = Realm.eval(r, 'Array.prototype');
  function write(o, k, v) {
    o[k] = v;
  }
  write(Realm.eval(r, '({prop:1})'), 'prop', 1);
  write(ap, 'prop', 1);
  write(Realm.eval(r, '[]'), 0, 1);
  write(ap, 0, 'boom');
  const res1 = Realm.eval(r, '[...[,1]]');
  const res2 = Realm.eval(r, '[,1][0]');
  assertEquals('boom', res2);
  assertEquals(res2, res1[0]);
})();

(function TestCrossRealmObjectPrototype() {
  const r = Realm.create();
  Realm.eval(
      r,
      'Object.defineProperty(Object.prototype, "prop", {set(_){}, configurable:true})');
  const op = Realm.eval(r, 'Object.prototype');
  function write(o, k, v) {
    o[k] = v;
  }
  write(Realm.eval(r, '({prop:1})'), 'prop', 1);
  write(op, 'prop', 1);
  write(Realm.eval(r, '[]'), 0, 1);
  write(op, 0, 'boom');
  const res1 = Realm.eval(r, '[...[,1]]');
  const res2 = Realm.eval(r, '[,1][0]');
  assertEquals('boom', res2);
  assertEquals(res2, res1[0]);
})();
