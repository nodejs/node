// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-incremental-marking --no-single-generation --no-gc-global --handle-weak-ref-weakly-in-minor-gc

(async function () {

  let cleanup_called = false;
  const cleanup = function(holdings) {
    assertFalse(cleanup_called);
    assertEquals("holdings", holdings);
    cleanup_called = true;
  }

  const fg = new FinalizationRegistry(cleanup);
  let weak_ref;
  (function() {
    const o = {};
    weak_ref = new WeakRef(o);
    fg.register(o, "holdings");
  })();

  // Since the WeakRef was created during this turn, it is not cleared by GC. The
  // pointer inside the FinalizationRegistry is not cleared either, since the WeakRef
  // keeps the target object alive.
  // Here we invoke GC synchronously and, with conservative stack scanning, there is
  // a chance that the object is not reclaimed now. In any case, the WeakRef should
  // not be cleared.
  gc({ type: 'minor' });

  assertNotEquals(undefined, weak_ref.deref());
  assertFalse(cleanup_called);

  // Check that the cleanup callback is not called in a task.
  setTimeout(() => { assertFalse(cleanup_called); }, 0);

  // Trigger GC in next task. Now the WeakRef is cleared but the cleanup has
  // not been called yet.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'minor', execution: 'async' });

  assertEquals(undefined, weak_ref.deref());
  assertFalse(cleanup_called);

  // Check that the cleanup callback was called in a follow up task.
  setTimeout(() => { assertTrue(cleanup_called); }, 0);

})();
