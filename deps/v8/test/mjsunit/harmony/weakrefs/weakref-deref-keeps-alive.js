// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let wr;
let wr_control; // control WeakRef for testing what happens without deref
(function() {
  let o1 = {};
  wr = new WeakRef(o1);
  let o2 = {};
  wr_control = new WeakRef(o2);
})();

let strong = {a: wr.deref(), b: wr_control.deref()};

gc();

%PerformMicrotaskCheckpoint();
// Next turn.

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

%PerformMicrotaskCheckpoint();
// Next turn.

gc();

assertEquals(undefined, wr.deref());
