// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let r1 = Realm.create();
let r2 = Realm.create();
let f = Realm.eval(r1, `(function f(r2) {
  Realm.switch(r2);
  throw new Error('foo');
})`);
let g = Realm.eval(r1, "(function g() { print('In g'); })");

Promise.resolve().then(() => f(r2)).catch(e => {});
Promise.resolve().then(g);

%PerformMicrotaskCheckpoint();
%PerformMicrotaskCheckpoint();
