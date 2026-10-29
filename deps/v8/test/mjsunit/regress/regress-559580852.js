// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-incremental-marking --concurrent-marking-high-priority-threads

const POOL = 2048;
const pool = new Array(POOL);
let head = 0;

for (let iter = 0; iter < 100000; iter++) {
  const ta = new Uint8Array(16);
  const old = pool[head];
  if (old !== undefined) {
    const b = old.buffer;
  }
  pool[head] = ta;
  head = (head + 1) % POOL;
}
