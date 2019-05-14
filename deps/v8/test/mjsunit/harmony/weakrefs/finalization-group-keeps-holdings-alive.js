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
  assertEquals(holdings_list.length, 1);
  assertEquals(holdings_list[0].a, "this is the holdings object");
  cleanup_called = true;
}

let fg = new FinalizationGroup(cleanup);
let o1 = {};
let holdings = {'a': 'this is the holdings object'};
fg.register(o1, holdings);

gc();
assertFalse(cleanup_called);

// Drop the last references to o1.
o1 = null;

// Drop the last reference to the holdings. The FinalizationGroup keeps it
// alive, so the cleanup function will be called as normal.
holdings = null;
gc();
assertFalse(cleanup_called);

let timeout_func = function() {
  assertTrue(cleanup_called);
}

setTimeout(timeout_func, 0);
