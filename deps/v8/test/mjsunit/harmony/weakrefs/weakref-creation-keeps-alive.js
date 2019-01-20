// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let cleanup_called = false;
let cleanup = function(iter) {
  assertFalse(cleanup_called);
  let count = 0;
  for (wc of iter) {
    ++count;
    assertEquals(wr, wc);
    assertEquals(undefined, wc.deref());
  }
  assertEquals(1, count);
  cleanup_called = true;
}

let wf = new WeakFactory(cleanup);
let wr;
(function() {
  let o = {};
  wr = wf.makeRef(o);
  // Don't deref here, we want to test that the creation is enough to keep the
  // WeakRef alive until the end of the turn.
})();

gc();

// Since the WeakRef was created during this turn, it is not cleared by GC.
(function() {
  assertNotEquals(undefined, wr.deref());
})();

%RunMicrotasks();
// Next turn.

assertFalse(cleanup_called);

gc();

%RunMicrotasks();
// Next turn.

assertTrue(cleanup_called);
