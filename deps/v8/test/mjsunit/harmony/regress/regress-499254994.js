// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-explicit-resource-management

(function testSyncUsing() {
  for (let iterations = 0; iterations <= 3; iterations++) {
    let disposeCount = 0;
    let i = 0;
    for (using x = { [Symbol.dispose]() { disposeCount++; } }; i < iterations; i++) {
    }
    assertEquals(1, disposeCount, `Failed for ${iterations} iterations`);
  }
})();

(async function testAwaitUsing() {
  for (let iterations = 0; iterations <= 3; iterations++) {
    let disposeCount = 0;
    let i = 0;
    for (await using x = { async [Symbol.asyncDispose]() { disposeCount++; } }; i < iterations; i++) {
    }
    assertEquals(1, disposeCount, `Failed for ${iterations} iterations`);
  }
})().catch(e => {
  print("Async test failed:");
  print(e);
  quit(1);
});
