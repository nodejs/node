// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_called = false;
let cleanup = function(iter) {
  assertFalse(cleanup_called);
  let holdings_list = [];
  for (holdings of iter) {
    holdings_list.push(holdings);
  }
  assertEquals(holdings_list.length, 2);
  if (holdings_list[0] == 1) {
    assertEquals(holdings_list[1], 2);
  } else {
    assertEquals(holdings_list[0], 2);
    assertEquals(holdings_list[1], 1);
  }
  cleanup_called = true;
}

let fg = new FinalizationGroup(cleanup);
let o1 = {};
let o2 = {};
fg.register(o1, 1);
fg.register(o2, 2);

gc();
assertFalse(cleanup_called);

// Drop the last references to o1 and o2.
o1 = null;
o2 = null;
// GC will reclaim the target objects; the cleanup function will be called the
// next time we enter the event loop.
gc();
assertFalse(cleanup_called);

let timeout_func = function() {
  assertTrue(cleanup_called);
}

setTimeout(timeout_func, 0);
