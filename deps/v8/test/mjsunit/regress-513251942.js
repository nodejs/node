// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let evil_called = false;

function trigger(P) {
  P.prototype.then = function evil(on, _) {
    evil_called = true;
    // Return a dummy promise to avoid internal errors
    return { then: function () { } };
  };
  P.prototype.dummy = 1;
}

let p = Promise.resolve(42);

// Overwrite the property without invalidating protector
trigger(Promise);

// Promise.all relies on PromiseThenProtector for the fast path.
// If the protector is stale, it will assume 'then' is unmodified
// and skip calling our 'evil' function.
Promise.all([p]);

assertTrue(evil_called, "FAIL: evil not called")
