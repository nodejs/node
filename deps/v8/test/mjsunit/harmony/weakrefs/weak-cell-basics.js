// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_called = false;
let cleanup = function(iter) {
  assertFalse(cleanup_called);
  let result = iter.next();
  assertEquals(result.value, holdings);
  assertFalse(result.done);
  result = iter.next();
  assertTrue(result.done);
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

gc();
assertFalse(cleanup_called);

// Drop the last reference to o.
(() => { o = null; })()

// GC will clear the WeakCell; the cleanup function will be called the next time
// we enter the event loop.
gc();
assertFalse(cleanup_called);

let timeout_func = function() {
  assertTrue(cleanup_called);
}

setTimeout(timeout_func, 0);
