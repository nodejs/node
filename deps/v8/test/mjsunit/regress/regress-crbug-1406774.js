// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

function worker_code(arr) {
  postMessage("worker starting");
  const large_bigint = 2n ** 100_000n;
  function f() {
    // Run until near the stack limit.
    try { f(); } catch (e) {
      postMessage("stack limit reached");
      postMessage(arr[large_bigint]);
    }
  }
  onmessage = f;
}
let w = new Worker(worker_code, { "arguments": [], "type": "function" });
assertEquals("worker starting", w.getMessage());
w.postMessage("");
assertEquals("stack limit reached", w.getMessage());
w.terminate();
