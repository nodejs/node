// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let wr;
(function() {
  let o = {};
  wr = new WeakRef(o);
  // Don't deref here, we want to test that the creation is enough to keep the
  // WeakRef alive until the end of the turn.
})();

gc();

// Since the WeakRef was created during this turn, it is not cleared by GC.
(function() {
  assertNotEquals(undefined, wr.deref());
})();

%PerformMicrotaskCheckpoint();
// Next turn.

gc();

assertEquals(undefined, wr.deref());
