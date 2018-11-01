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
  assertEquals(cells.length, 2);
  if (cells[0] == wc1) {
    assertEquals(cells[0].holdings, 1);
    assertEquals(cells[1], wc2);
    assertEquals(cells[1].holdings, 2);
  } else {
    assertEquals(cells[0], wc2);
    assertEquals(cells[0].holdings, 2);
    assertEquals(cells[1], wc1);
    assertEquals(cells[1].holdings, 1);
  }
  cleanup_called = true;
}

let wf = new WeakFactory(cleanup);
let o1 = {};
let o2 = {};
let wc1 = wf.makeCell(o1, 1);
let wc2 = wf.makeCell(o2, 2);

gc();
assertFalse(cleanup_called);

// Drop the last references to o1 and o2.
o1 = null;
o2 = null;
// GC will clear the WeakCells; the cleanup function will be called the next time
// we enter the event loop.
gc();
assertFalse(cleanup_called);

let timeout_func = function() {
  assertTrue(cleanup_called);
}

setTimeout(timeout_func, 0);
