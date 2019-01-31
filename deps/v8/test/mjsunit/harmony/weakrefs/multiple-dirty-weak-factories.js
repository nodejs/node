// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup_weak_cell_count = 0;
let cleanup = function(iter) {
  for (wc of iter) {
    ++cleanup_weak_cell_count;
  }
  ++cleanup_call_count;
}

let wf1 = new WeakFactory(cleanup);
let wf2 = new WeakFactory(cleanup);

// Create two objects and WeakCells pointing to them. The objects need to be inside
// a closure so that we can reliably kill them!
let weak_cell1;
let weak_cell2;

(function() {
  let object1 = {};
  weak_cell1 = wf1.makeCell(object1);

  let object2 = {};
  weak_cell2 = wf2.makeCell(object2);

  // object1 and object2 go out of scope.
})();

// This GC will discover dirty WeakCells and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function was called and iterated the WeakCells.
let timeout_func = function() {
  assertEquals(2, cleanup_call_count);
  assertEquals(2, cleanup_weak_cell_count);
}

setTimeout(timeout_func, 0);
