// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-symbol-as-weakmap-key --expose-gc --noincremental-marking

(function TestWeakRefWithSymbolGC() {
  let weakRef;
  {
    const innerKey = Symbol('123');
    weakRef = new WeakRef(innerKey);
  }
  // Since the WeakRef was created during this turn, it is not cleared by GC.
  gc();
  assertNotEquals(undefined, weakRef.deref());
  // Next task.
  setTimeout(() => {
    gc();
    assertEquals(undefined, weakRef.deref());
  }, 0);
})();

(function TestFinalizationRegistryWithSymbolGC() {
  let cleanUpCalled = false;
  const fg = new FinalizationRegistry((target) => {
    assertEquals('123', target);
    cleanUpCalled = true;
  });
  (function () {
    const innerKey = Symbol('123');
    fg.register(innerKey, '123');
  })();
  gc();
  assertFalse(cleanUpCalled);
  // Check that cleanup callback was called in a follow up task.
  setTimeout(() => {
    assertTrue(cleanUpCalled);
  }, 0);
})();
