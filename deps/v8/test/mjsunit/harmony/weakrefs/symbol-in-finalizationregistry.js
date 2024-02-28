// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let cleanUpCalled = false;
  const fg = new FinalizationRegistry((target) => {
    assertEquals('123', target);
    cleanUpCalled = true;
  });

  (function () {
    const innerKey = Symbol('123');
    fg.register(innerKey, '123');
  })();

  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });
  assertFalse(cleanUpCalled);

  // Check that cleanup callback was called in a follow up task.
  setTimeout(() => { assertTrue(cleanUpCalled); }, 0);

})();
