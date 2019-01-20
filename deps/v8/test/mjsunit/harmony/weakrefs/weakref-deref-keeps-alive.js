// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let cleanup_count = 0;
let cleanup_cells = [];
let cleanup = function(iter) {
  for (wc of iter) {
    assertEquals(undefined, wc.deref());
    cleanup_cells.push(wc);
  }
  ++cleanup_count;
}

let wf = new WeakFactory(cleanup);
let wf_control = new WeakFactory(cleanup);
let wr;
let wr_control; // control WeakRef for testing what happens without deref
(function() {
  let o1 = {};
  wr = wf.makeRef(o1);
  let o2 = {};
  wr_control = wf_control.makeRef(o2);
})();

let strong = {a: wr.deref(), b: wr_control.deref()};

gc();

%RunMicrotasks();
// Next turn.

gc();

%RunMicrotasks();
// Next turn.

// We have a strong reference to the objects, so the WeakRefs are not cleared yet.
assertEquals(0, cleanup_count);

// Call deref inside a closure, trying to avoid accidentally storing a strong
// reference into the object in the stack frame.
(function() {
  wr.deref();
})();

strong = null;

// This GC will clear wr_control.
gc();

(function() {
  assertNotEquals(undefined, wr.deref());
  // Now the control WeakRef got cleared, since nothing was keeping it alive.
  assertEquals(undefined, wr_control.deref());
})();

%RunMicrotasks();
// Next turn.

assertEquals(1, cleanup_count);
assertEquals(1, cleanup_cells.length);
assertEquals(wc, cleanup_cells[0]);

gc();

%RunMicrotasks();
// Next turn.

assertEquals(2, cleanup_count);
assertEquals(2, cleanup_cells.length);
assertEquals(wr, cleanup_cells[1]);

assertEquals(undefined, wr.deref());
