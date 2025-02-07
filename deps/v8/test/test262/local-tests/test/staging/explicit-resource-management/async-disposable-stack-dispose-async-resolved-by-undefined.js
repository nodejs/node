// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
  AsyncDisposableStack resolved with undefned.
includes: [asyncHelpers.js]
flags: [async]
features: [explicit-resource-management]
---*/

asyncTest(async function() {
    async function TestAsyncDisposableStackDefer() {
      let stack = new AsyncDisposableStack();
      assert.sameValue(await stack.disposeAsync(), undefined);
    };
    await TestAsyncDisposableStackDefer();
  });
