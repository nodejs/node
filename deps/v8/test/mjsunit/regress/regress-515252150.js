// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

const r = Realm.create(undefined, {create_own_microtask_queue: true});

for (let i = 0; i < 5; i++) {
  gc();
}

Realm.eval(r, `
  Promise.resolve().then(() => {});
`);
