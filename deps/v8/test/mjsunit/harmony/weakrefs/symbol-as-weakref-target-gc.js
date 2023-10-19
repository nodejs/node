// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let weakRef;
  (function () {
    const innerKey = Symbol('123');
    weakRef = new WeakRef(innerKey);
  })();

  // Since the WeakRef was created during this turn, it is not cleared by GC.
  // Here we invoke GC synchronously and, with conservative stack scanning, there is
  // a chance that the object is not reclaimed now. In any case, the WeakRef should
  // not be cleared.
  gc();

  assertNotEquals(undefined, weakRef.deref());

  // Trigger GC again in next task. Now the WeakRef is cleared.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });

  assertEquals(undefined, weakRef.deref());

})();
