// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

let o1 = {};
let o2 = {};
let wr1;
let wr2;
(function() {
  wr1 = new WeakRef(o1);
  wr2 = new WeakRef(o2);
})();

// Since the WeakRefs were created during this turn, they're not cleared by GC.
// Here and below, we invoke GC synchronously and, with conservative stack
// scanning, there is a chance that the object is not reclaimed now. In any
// case, the WeakRef should not be cleared.
gc();

(function() {
  assertNotEquals(undefined, wr1.deref());
  assertNotEquals(undefined, wr2.deref());
})();

// New task
setTimeout(function() {
  (function () { wr1.deref(); })();
  o1 = null;
  gc(); // deref makes sure we don't clean up wr1
  (function () { assertNotEquals(undefined, wr1.deref()); })();

  // New task
  setTimeout(function() {
    (function () { wr2.deref(); })();
    o2 = null;
    gc(); // deref makes sure we don't clean up wr2
    (function () { assertNotEquals(undefined, wr2.deref()); })();

    // New task
    (async function () {
      // Trigger GC again to make sure the two WeakRefs are cleared.
      // We need to invoke GC asynchronously and wait for it to finish, so that
      // it doesn't need to scan the stack. Otherwise, the objects may not be
      // reclaimed because of conservative stack scanning and the test may not
      // work as intended.
      await gc({ type: 'major', execution: 'async' });

      assertEquals(undefined, wr1.deref());
      assertEquals(undefined, wr2.deref());
    })();
  }, 0);
}, 0);
