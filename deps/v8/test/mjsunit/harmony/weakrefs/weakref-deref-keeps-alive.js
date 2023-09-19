// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

let wr;
let wr_control; // control WeakRef for testing what happens without deref
(function () {
  let o1 = {};
  wr = new WeakRef(o1);
  let o2 = {};
  wr_control = new WeakRef(o2);
})();

let strong = { a: wr.deref(), b: wr_control.deref() };

// Here and below, we invoke GC synchronously and, with conservative stack
// scanning, there is a chance that the object is not reclaimed now. In any
// case, the WeakRefs should not be cleared.
gc();

// Next task.
setTimeout(function() {
  // Call deref inside a closure, trying to avoid accidentally storing a strong
  // reference into the object in the stack frame.
  (function () { wr.deref(); })();

  strong = null;

  // This GC should clear wr_control (modulo CSS), since nothing was keeping it
  // alive, but it should not clear wr.
  gc();
  (function () { assertNotEquals(undefined, wr.deref()); })();

  // Next task.
  (async function () {
    // Trigger GC again to make sure the two WeakRefs are cleared.
    // We need to invoke GC asynchronously and wait for it to finish, so that
    // it doesn't need to scan the stack. Otherwise, the objects may not be
    // reclaimed because of conservative stack scanning and the test may not
    // work as intended.
    await gc({ type: 'major', execution: 'async' });

    assertEquals(undefined, wr.deref());
    assertEquals(undefined, wr_control.deref());
  })();
}, 0);
