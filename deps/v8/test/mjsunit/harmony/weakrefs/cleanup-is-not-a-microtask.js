// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

// This test asserts that the cleanup function call, scheduled by GC, is a
// microtask and not a normal task.

// Inside a microtask, cause GC (which should schedule the cleanup as
// microtask).  lso schedule another microtask. Assert that the cleanup
// function ran before the other microtask.

let cleanedUp = false;

function scheduleMicrotask(func) {
  Promise.resolve().then(func);
}

let log = [];

let cleanup = (iter) => {
  cleanedUp = true;
  for (holdings of iter) { }
}

let fg = new FinalizationGroup(cleanup);
let o = null;

(function() {
  // Use a closure here to avoid other references to o which might keep it alive
  // (e.g., stack frames pointing to it).
  o = {};
  fg.register(o, {});
})();

let microtask = function() {
  log.push("first_microtask");

  // cause GC during a microtask
  o = null;
  gc();
}

assertFalse(cleanedUp);

// enqueue microtask that triggers GC
Promise.resolve().then(microtask);

// but cleanup callback hasn't been called yet, as we're still in
// synchronous execution
assertFalse(cleanedUp);

// flush the microtask queue to run the microtask that triggers GC
%PerformMicrotaskCheckpoint();

// still no cleanup callback, because it runs after as a separate task
assertFalse(cleanedUp);

setTimeout(() => {
  assertTrue(cleanedUp);
}, 0);
