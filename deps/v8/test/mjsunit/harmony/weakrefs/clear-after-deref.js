// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let cleanup_started = false;
let cleanup_succeeded = false;
let cleanup = function(iter) {
  cleanup_start = true;
  let cells = [];
  for (wc of iter) {
    cells.push(wc);
  }
  assertEquals(1, cells.length);
  assertEquals(w1, cells[0]);
  cleanup_succeeded = true;
}

let wf = new WeakFactory(cleanup);
let wr;
(function() {
  let o = { foo: "bar" };
  wr = wf.makeRef(o);
})();

// Since the WeakRef was created during this turn, they're not cleared by GC.
gc();
assertNotEquals(undefined, wr.deref());

%RunMicrotasks();
// New turn.

let o = wr.deref();
assertEquals("bar", o.foo);

wr.clear();
assertEquals(undefined, wr.deref());

let timeout_func1 = function() {
  assertFalse(cleanup_started);
  assertFalse(cleanup_succeeded);
}

// Assert that the cleanup function won't be called.
setTimeout(timeout_func1, 0);
