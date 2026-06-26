// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

const pending = new Promise(() => {});

(async function () {
  let wr;

  await (async function () {
    const payload = { };
    wr = new WeakRef(payload);
    const resolved = Promise.resolve(payload);
    // The pending Promise should not prevent GC of the race Promise once the race settles.
    await Promise.race([pending, resolved]);
  })();

  await gc({ type: 'major', execution: 'async' });

  assertEquals(undefined, wr.deref());
})().catch((e) => {
  console.error(e);
  quit(1);
});
