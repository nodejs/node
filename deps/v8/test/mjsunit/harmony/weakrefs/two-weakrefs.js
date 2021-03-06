// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let o1 = {};
let o2 = {};
let wr1;
let wr2;
(function() {
  wr1 = new WeakRef(o1);
  wr2 = new WeakRef(o2);
})();

// Since the WeakRefs were created during this turn, they're not cleared by GC.
gc();

(function() {
  assertNotEquals(undefined, wr1.deref());
  assertNotEquals(undefined, wr2.deref());
})();

// New task
setTimeout(function() {
  wr1.deref();
  o1 = null;
  gc(); // deref makes sure we don't clean up wr1

  // New task
  setTimeout(function() {
    wr2.deref();
    o2 = null;
    gc(); // deref makes sure we don't clean up wr2

    // New task
    setTimeout(function() {
      assertEquals(undefined, wr1.deref());
      gc();

      // New task
      setTimeout(function() {
        assertEquals(undefined, wr2.deref());
      }, 0);
    }, 0);
  }, 0);
}, 0);
