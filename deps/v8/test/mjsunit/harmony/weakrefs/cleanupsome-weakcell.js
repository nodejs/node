// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_count = 0;
let cleanup_cells = [];
let cleanup = function(iter) {
  for (wc of iter) {
    cleanup_cells.push(wc);
  }
  ++cleanup_count;
}

let wf = new WeakFactory(cleanup);
let weak_cell;
(function() {
  let o = {};
  weak_cell = wf.makeCell(o);

  // cleanupSome won't do anything since there are no dirty WeakCells.
  wf.cleanupSome();
  assertEquals(0, cleanup_count);
})();

// GC will detect the WeakCell as dirty.
gc();

wf.cleanupSome();
assertEquals(1, cleanup_count);
assertEquals(1, cleanup_cells.length);
assertEquals(weak_cell, cleanup_cells[0]);
