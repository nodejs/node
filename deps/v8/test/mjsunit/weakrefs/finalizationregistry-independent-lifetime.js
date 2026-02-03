// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking --no-concurrent-recompilation

(async function () {

  let cleanup_called = false;
  function cleanup(holdings) {
    cleanup_called = true;
  };

  let task_1_gc = (async function () {
    const fg = new FinalizationRegistry(cleanup);

    (function () {
      let x = {};
      fg.register(x, "holdings");
      x = null;
    })();

    // Schedule fg for cleanup.
    await gc({ type: 'major', execution: 'async' });
    assertFalse(cleanup_called);
  })();

  // Schedule a task to collect fg, which should result in cleanup not called.
  let task_2_gc = (async function () {
    await gc({ type: 'major', execution: 'async' });
    assertFalse(cleanup_called);
  })();

  // Wait for the two GC tasks to be executed.
  await task_1_gc;
  await task_2_gc;

  // Check that the cleanup will not be called.
  setTimeout(function () { assertFalse(cleanup_called); }, 0);

})();
