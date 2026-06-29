// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-incremental-marking --no-single-generation --no-gc-global --handle-weak-ref-weakly-in-minor-gc

let o1 = {};
let o2 = {};
let wr1;
let wr2;
(function() {
  wr1 = new WeakRef(o1);
  wr2 = new WeakRef(o2);
})();
o1 = null;
o2 = null;

// Since the WeakRefs were created during this turn, they're not cleared by GC.
// Here and below, we invoke GC synchronously and, with conservative stack
// scanning, there is a chance that the object is not reclaimed now. In any
// case, the WeakRef should not be cleared.
gc({ type: 'minor' });

(function() {
  assertNotEquals(undefined, wr1.deref());
  assertNotEquals(undefined, wr2.deref());
})();

// New task
setTimeout(function() {
  (function () { wr1.deref(); })();
  gc({ type: 'minor' }); // deref makes sure we don't clean up wr1
  setTimeout(() => {
    assertNotEquals(undefined, wr1.deref());
    assertEquals(undefined, wr2.deref());
  }, 0);
}, 0);
