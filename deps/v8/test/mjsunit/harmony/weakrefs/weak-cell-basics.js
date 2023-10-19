// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking

(async function () {

  let cleanup_called = false;
  let cleanup = function(holdings_arg) {
    assertFalse(cleanup_called);
    assertEquals(holdings_arg, holdings);
    cleanup_called = true;
  }

  let fg = new FinalizationRegistry(cleanup);
  let o = {};
  let holdings = {'h': 55};

  // Ignition holds references to objects in temporary registers. These will be
  // released when the function exits. So only access o inside a function to
  // prevent any references to objects in temporary registers when a gc is
  // triggered.
  (() => { fg.register(o, holdings); })()

  // Here and below, we need to invoke GC asynchronously and wait for it to
  // finish, so that it doesn't need to scan the stack. Otherwise, the objects
  // may not be reclaimed because of conservative stack scanning and the test
  // may not work as intended.
  await gc({ type: 'major', execution: 'async' });
  assertFalse(cleanup_called);

  // Drop the last reference to o.
  (() => { o = null; })()

  // GC will clear the WeakCell; the cleanup function will be called the next time
  // we enter the event loop.
  await gc({ type: 'major', execution: 'async' });
  assertFalse(cleanup_called);

  let timeout_func = function() {
    assertTrue(cleanup_called);
  }

  setTimeout(timeout_func, 0);

})();
