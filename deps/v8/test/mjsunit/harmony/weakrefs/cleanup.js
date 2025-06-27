// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let cleanup_called = 0;
  let holdings_list = [];
  let cleanup = function (holdings) {
    holdings_list.push(holdings);
    cleanup_called++;
  }

  let fg = new FinalizationRegistry(cleanup);
  let o1 = {};
  let o2 = {};

  // Ignition holds references to objects in temporary registers. These will be
  // released when the function exits. So only access o inside a function to
  // prevent any references to objects in temporary registers when a gc is
  (function () {
    fg.register(o1, 1);
    fg.register(o2, 2);
  })();

  // Here and below, we need to invoke GC asynchronously and wait for it to
  // finish, so that it doesn't need to scan the stack. Otherwise, the objects
  // may not be reclaimed because of conservative stack scanning and the test
  // may not work as intended.
  await gc({ type: 'major', execution: 'async' });
  assertEquals(cleanup_called, 0);

  // Drop the last references to o1 and o2.
  (function () {
    o1 = null;
    o2 = null;
  })();

  // GC will reclaim the target objects; the cleanup function will be called the
  // next time we enter the event loop.
  await gc({ type: 'major', execution: 'async' });
  assertEquals(cleanup_called, 0);

  let timeout_func = function () {
    assertEquals(cleanup_called, 2);
    assertEquals(holdings_list.length, 2);
    if (holdings_list[0] == 1) {
      assertEquals(holdings_list[1], 2);
    } else {
      assertEquals(holdings_list[0], 2);
      assertEquals(holdings_list[1], 1);
    }
  }

  setTimeout(timeout_func, 0);

})();
