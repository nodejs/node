// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  const r = Realm.create();
  const FG = Realm.eval(r, "FinalizationRegistry");
  Realm.detachGlobal(r);

  const cleanup_not_run = function (holdings) {
    assertUnreachable();
  }
  let fg_not_run = new FG(cleanup_not_run);

  (function () {
    const object = {};
    fg_not_run.register(object, "first");
    // Object becomes unreachable.
  })();

  let cleanedUp = false;
  let fg_run;

  // Schedule a GC, which will schedule fg_not_run for cleanup.
  // Here and below, we need to invoke GC asynchronously and wait for it to
  // finish, so that it doesn't need to scan the stack. Otherwise, the objects
  // may not be reclaimed because of conservative stack scanning and the test
  // may not work as intended.
  let task_1_gc = (async function () {
    await gc({ type: 'major', execution: 'async' });

    // Disposing the realm cancels the already scheduled fg_not_run's finalizer.
    Realm.dispose(r);

    const cleanup = function (holdings) {
      assertEquals(holdings, "second");
      assertFalse(cleanedUp);
      cleanedUp = true;
    }
    fg_run = new FG(cleanup);

    // FGs that are alive after disposal can still schedule tasks.
    (function () {
      const object = {};
      fg_run.register(object, "second");
      // Object becomes unreachable.
    })();
  })();

  // Schedule a second GC for execution after that, which will now schedule
  // fg_run for cleanup.
  let task_2_gc = (async function () {
    await gc({ type: 'major', execution: 'async' });

    // Check that the cleanup task has had the chance to run yet.
    assertFalse(cleanedUp);
  })();

  // Wait for the two GCs to be executed.
  await task_1_gc;
  await task_2_gc;

  // Give the cleanup task a chance to run and check it worked correctly.
  setTimeout(function () { assertTrue(cleanedUp); }, 0);

})();
