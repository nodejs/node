// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup_weak_cell_count = 0;
let cleanup = function(iter) {
  for (wc of iter) {
    // See which WeakCell we're iterating over and clear the other one.
    if (wc == weak_cell1) {
      weak_cell2.clear();
    } else {
      assertSame(wc, weak_cell2);
      weak_cell1.clear();
    }
    ++cleanup_weak_cell_count;
  }
  ++cleanup_call_count;
}

let wf = new WeakFactory(cleanup);
// Create an object and a WeakCell pointing to it. The object needs to be inside
// a closure so that we can reliably kill them!
let weak_cell1;
let weak_cell2;

(function() {
  let object1 = {};
  weak_cell1 = wf.makeCell(object1);
  let object2 = {};
  weak_cell2 = wf.makeCell(object2);

  // object1 and object2 go out of scope.
})();

// This GC will discover dirty WeakCells and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function was called and iterated one WeakCell (but not the other one).
let timeout_func = function() {
  assertEquals(1, cleanup_call_count);
  assertEquals(1, cleanup_weak_cell_count);
}

setTimeout(timeout_func, 0);
