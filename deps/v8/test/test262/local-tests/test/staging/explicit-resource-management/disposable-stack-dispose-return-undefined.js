// Copyright (C) 2024 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: |
  DisposableStack return undefned.
features: [explicit-resource-management]
---*/

(function TestDisposableStackDisposeReturnsUndefined() {
    let stack = new DisposableStack();
    assert.sameValue(stack.dispose(), undefined);
})();
