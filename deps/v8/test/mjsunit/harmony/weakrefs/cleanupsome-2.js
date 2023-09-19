// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs-with-cleanup-some --expose-gc --noincremental-marking --allow-natives-syntax

(async function () {

  let cleanup_count = 0;
  let cleanup_holdings = [];
  let cleanup = function (holdings) {
    cleanup_holdings.push(holdings);
    ++cleanup_count;
  }

  let fg = new FinalizationRegistry(cleanup);
  (function () {
    let o = {};
    fg.register(o, "holdings");

    assertEquals(0, cleanup_count);
  })();

  // GC will detect o as dead.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  await gc({ type: 'major', execution: 'async' });

  // passing no callback, should trigger cleanup function
  fg.cleanupSome();
  assertEquals(1, cleanup_count);
  assertEquals(1, cleanup_holdings.length);
  assertEquals("holdings", cleanup_holdings[0]);

})();
