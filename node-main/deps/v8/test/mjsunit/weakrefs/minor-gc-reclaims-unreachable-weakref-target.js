// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-incremental-marking --no-single-generation --no-gc-global --handle-weak-ref-weakly-in-minor-gc

(async function () {

  let weakRef;
  (function () {
    const target = {};
    weakRef = new WeakRef(target);
  })();

  // The WeakRef was created during this turn. The object is not reclaimed
  // now.
  gc({ type: 'minor' });

  assertNotEquals(undefined, weakRef.deref());

  // Trigger minor GC asynchronously.
  await gc({ type: 'minor', execution: 'async' });

  assertEquals(undefined, weakRef.deref());

})();
