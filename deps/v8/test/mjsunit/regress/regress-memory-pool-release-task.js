// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-gc-nvp --memory-pool-timeout=1 --expose-gc

// The memory pool release task runs on a worker thread, where
// IsolateGroup::current() may not be set. Make sure tracing from it works.

function allocate() {
  let arr = [];
  for (let i = 0; i < 10000; i++) {
    arr.push(new Array(100));
  }
}

for (let i = 0; i < 10; i++) {
  allocate();
  gc();
}

// Wait for the release task to run.
let start = Date.now();
while (Date.now() - start < 1500) {}
