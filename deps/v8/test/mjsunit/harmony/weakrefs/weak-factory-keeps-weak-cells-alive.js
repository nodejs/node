// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_called = false;
let cleanup = function(iter) {
  assertFalse(cleanup_called);
  let cells = [];
  for (wc of iter) {
    cells.push(wc);
  }
  assertEquals(cells.length, 1);
  assertEquals(cells[0].holdings, "this is my cell");
  cleanup_called = true;
}

let wf = new WeakFactory(cleanup);
let o1 = {};
let wc1 = wf.makeCell(o1, "this is my cell");

gc();
assertFalse(cleanup_called);

// Drop the last references to o1.
o1 = null;

// Drop the last reference to the WeakCell. The WeakFactory keeps it alive, so
// the cleanup function will be called as normal.
wc1 = null;
gc();
assertFalse(cleanup_called);

let timeout_func = function() {
  assertTrue(cleanup_called);
}

setTimeout(timeout_func, 0);
