// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

// This test asserts that the cleanup function call, scheduled by GC, is
// not a microtask but a normal task.

(async function () {

  let microtaskInvoked = false;
  const microtask = () => {
    assertFalse(cleanedUp);
    assertFalse(microtaskInvoked);
    microtaskInvoked = true;
  };

  let cleanedUp = false;
  const cleanup = (holdings) => {
    assertFalse(cleanedUp);
    assertTrue(microtaskInvoked);
    cleanedUp = true;
  };

  const fg = new FinalizationRegistry(cleanup);

  (function() {
    // Use a closure here to avoid other references to object which might keep
    // it alive (e.g., stack frames pointing to it).
    const object = {};
    fg.register(object, {});
  })();

  // The GC will schedule the cleanup as a regular task.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });

  assertFalse(cleanedUp);

  // Schedule the microtask.
  Promise.resolve().then(microtask);

  // Nothing else hasn't been called yet, as we're still in synchronous
  // execution.
  assertFalse(microtaskInvoked);
  assertFalse(cleanedUp);

  // The microtask and the cleanup callbacks will verify that these two are
  // invoked in the right order: microtask -> cleanup.
  setTimeout(() => { assertTrue(cleanedUp); }, 0);

})();
