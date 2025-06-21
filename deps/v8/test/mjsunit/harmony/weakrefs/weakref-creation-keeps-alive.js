// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

let wr;
(function () {
  let o = {};
  wr = new WeakRef(o);
  // Don't deref here, we want to test that the creation is enough to keep the
  // WeakRef alive until the end of the turn.
})();

// Here we invoke GC synchronously and, with conservative stack scanning,
// there is a chance that the object is not reclaimed now. In any case,
// the WeakRef should not be cleared.
gc();
// Since the WeakRef was created during this turn, it is not cleared by GC.
assertNotEquals(undefined, wr.deref());

// Next task.
setTimeout(() => {
  (async function () {
    // Trigger GC again to make sure the two WeakRefs are cleared.
    // We need to invoke GC asynchronously and wait for it to finish, so that
    // it doesn't need to scan the stack. Otherwise, the objects may not be
    // reclaimed because of conservative stack scanning and the test may not
    // work as intended.
    await gc({ type: 'major', execution: 'async' });
    assertEquals(undefined, wr.deref());
  })();
}, 0);
