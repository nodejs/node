// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100 --js-explicit-resource-management --ignore-unhandled-promises

async function loop() {
  function disposal() {}
  const stack = new AsyncDisposableStack();
  stack[Symbol.dispose] = disposal;
  stack.use(stack).disposeAsync();
  return await loop();
}
assertThrowsAsync(loop(), RangeError);
