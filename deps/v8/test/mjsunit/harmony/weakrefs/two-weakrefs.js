// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let cleanup_count = 0;
let cleared_cells1 = [];
let cleared_cells2 = [];
let cleanup = function(iter) {
  if (cleanup_count == 0) {
    for (wc of iter) {
      cleared_cells1.push(wc);
    }
  } else {
    assertEquals(1, cleanup_count);
    for (wc of iter) {
      cleared_cells2.push(wc);
    }
  }
  ++cleanup_count;
}

let wf = new WeakFactory(cleanup);
let o1 = {};
let o2 = {};
let wr1;
let wr2;
(function() {
  wr1 = wf.makeRef(o1);
  wr2 = wf.makeRef(o2);
})();

// Since the WeakRefs were created during this turn, they're not cleared by GC.
gc();
(function() {
  assertNotEquals(undefined, wr1.deref());
  assertNotEquals(undefined, wr2.deref());
})();

%RunMicrotasks();
// New turn.

assertEquals(0, cleanup_count);

wr1.deref();
o1 = null;
gc(); // deref makes sure we don't clean up wr1

%RunMicrotasks();
// New turn.

assertEquals(0, cleanup_count);

wr2.deref();
o2 = null;
gc(); // deref makes sure we don't clean up wr2

%RunMicrotasks();
// New turn.

assertEquals(1, cleanup_count);
assertEquals(wr1, cleared_cells1[0]);

gc();

%RunMicrotasks();
// New turn.

assertEquals(2, cleanup_count);
assertEquals(wr2, cleared_cells2[0]);
