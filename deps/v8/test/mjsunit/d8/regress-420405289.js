// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strict-termination-checks

for (let i = 0; i < 2; ++i) {
  function foo() {
    Realm.create("");
    const o = {};
    o[Symbol.dispose] = function() {}
    const stack = new AsyncDisposableStack();
    stack.use(o);
    stack.disposeAsync();
    foo();
  }
  new Worker(foo, { type: "function" });
}
