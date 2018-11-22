// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let cleanup_called = false;
let cleanup = function(iter) {
  assertFalse(cleanup_called);
  let cells = [];
  for (wc of iter) {
    cells.push(wc);
  }
  assertEquals(2, cells.length);
  assertTrue(cells.includes(weak_ref));
  assertTrue(cells.includes(weak_cell));
  cleanup_called = true;
}

let wf = new WeakFactory(cleanup);
let weak_ref;
let weak_cell;
(function() {
  let o = {};
  weak_ref = wf.makeRef(o);
  weak_cell = wf.makeRef(o);
})();

// Since the WeakRef was created during this turn, it is not cleared by GC. The
// WeakCell is not cleared either, since the WeakRef keeps the target object
// alive.
gc();
(function() {
  assertNotEquals(undefined, weak_ref.deref());
})();

%RunMicrotasks();
// Next turn.

gc();

%RunMicrotasks();
// Next turn.

assertTrue(cleanup_called);
