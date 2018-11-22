// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let cleanup_count = 0;
let cleanup_cells = [];
let cleanup = function(iter) {
  for (wc of iter) {
    cleanup_cells.push(wc);
  }
  ++cleanup_count;
}

let o = {};
let wf = new WeakFactory(cleanup);
let weak_ref;
(function() {
  weak_ref = wf.makeRef(o);

  // cleanupSome won't do anything since there are no dirty WeakCells.
  wf.cleanupSome();
  assertEquals(0, cleanup_count);
})();

// Clear the KeepDuringJob set.
%RunMicrotasks();

weak_ref.deref();
o = null;

// The WeakRef is not detected as dirty, since the KeepDuringJob set keeps the
// target object alive.
gc();

wf.cleanupSome();
assertEquals(0, cleanup_count);

%RunMicrotasks();
// Next turn.

// Now the WeakRef can be cleared.
gc();
wf.cleanupSome();

assertEquals(1, cleanup_count);
assertEquals(1, cleanup_cells.length);
assertEquals(weak_ref, cleanup_cells[0]);

// The cleanup task is not executed again since all WeakCells have been
// processed.

%RunMicrotasks();
// Next turn.

assertEquals(1, cleanup_count);
