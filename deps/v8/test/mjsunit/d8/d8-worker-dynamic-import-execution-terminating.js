// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-incremental-marking

function workerLoop() {
  import('data:text/javascript,export function f(){}');
  this.postMessage(true);
}

const w = new Worker(workerLoop, {
  type: "function"
});
w.getMessage();
