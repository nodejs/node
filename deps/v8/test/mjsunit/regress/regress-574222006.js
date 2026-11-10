// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const realm = Realm.create();
Realm.eval(realm, `
  var x = 1;
  function store(o) { o.x = 2; }
  const objs = [{a: 1}, {b: 1}, {c: 1}, {d: 1}, {e: 1}];
  for (let i = 0; i < 10; i++) {
    for (const o of objs) store(o);
  }
  store(globalThis);
`);

const target = Realm.global(realm);

function write(o) {
  o.x = 42;
}
const objs = [{a: 1}, {b: 1}, {c: 1}, {d: 1}, {e: 1}];
for (let i = 0; i < 10; i++) {
  for (const o of objs) write(o);
}

Realm.eval(realm, 'store(globalThis);');
assertThrows(() => write(target), TypeError);
assertEquals(2, Realm.eval(realm, 'x'));
