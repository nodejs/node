// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

// This test asserts that the cleanup function call, scheduled by GC, is a
// microtask and not a normal task.

// Inside a microtask, cause GC (which should schedule the cleanup as
// microtask).  lso schedule another microtask. Assert that the cleanup
// function ran before the other microtask.

function scheduleMicrotask(func) {
  Promise.resolve().then(func);
}

let log = [];

let cleanup = (iter) => {
  log.push("cleanup");
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

let microtask_after_cleanup = () => {
  log.push("microtask_after_cleanup");
}

let first_microtask = function() {
  log.push("first_microtask");

  // This schedules the cleanup function as microtask.
  o = null;
  gc();

  // Schedule a microtask which should run after the cleanup microtask.
  scheduleMicrotask(microtask_after_cleanup);
}

scheduleMicrotask(first_microtask);

setTimeout(() => {
  // Assert that the functions got called in the right order.
  let wanted_log = ["first_microtask", "cleanup", "microtask_after_cleanup"];
  assertEquals(wanted_log, log);
}, 0);
