// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

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

%PerformMicrotaskCheckpoint();
// New turn.

wr1.deref();
o1 = null;
gc(); // deref makes sure we don't clean up wr1

%PerformMicrotaskCheckpoint();
// New turn.

wr2.deref();
o2 = null;
gc(); // deref makes sure we don't clean up wr2

%PerformMicrotaskCheckpoint();
// New turn.

assertEquals(undefined, wr1.deref());

gc();

%PerformMicrotaskCheckpoint();
// New turn.

assertEquals(undefined, wr2.deref());
