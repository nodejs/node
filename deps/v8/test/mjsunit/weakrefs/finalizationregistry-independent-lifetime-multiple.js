// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking --no-concurrent-inlining

(async function () {

  let cleanup_called = false;
  function cleanup(holdings) {
    cleanup_called = true;
  };

  let cleanup_called_2 = false;
  function cleanup2(holdings) {
    cleanup_called_2 = true;
  };

  const fg = new FinalizationRegistry(cleanup);

  let task_1_gc = (async function () {
    const fg2 = new FinalizationRegistry(cleanup2);

    (function () {
      fg.register({}, "holdings1");
      fg2.register({}, "holdings2");
    })();

    // Schedule fg and fg2 for cleanup.
    await gc({ type: 'major', execution: 'async' });
    assertFalse(cleanup_called);
    assertFalse(cleanup_called_2);
   })();

  // Schedule a task to collect fg2, but fg is still alive.
  let task_2_gc = (async function () {
    await gc({ type: 'major', execution: 'async' });
    assertFalse(cleanup_called);
    assertFalse(cleanup_called_2);
  })();

  // Wait for the two GC tasks to be executed.
  await task_1_gc;
  await task_2_gc;

  // Check that only the cleanup for fg will be called.
  setTimeout(function() {
    assertTrue(cleanup_called);
    assertFalse(cleanup_called_2);
  }, 0);

})();
