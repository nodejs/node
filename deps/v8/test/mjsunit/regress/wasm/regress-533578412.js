// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --max-old-space-size=64

// Growing a Wasm memory in small steps beyond 2 * max-old-space-size should not
// trigger a Mark-Compact GC storm.
const ws = new WeakSet();
function putWeak() {
  ws.add({});
}
putWeak();

let gcs = 0;
const memory = new WebAssembly.Memory({initial: 0});
for (let i = 0; i < 1024; i++) {
  try {
    memory.grow(4);
  } catch (e) {
    // Growing may fail with RangeError on 32-bit platforms or under memory
    // pressure.
    assertInstanceof(e, RangeError);
    break;
  }
  if (%GetWeakCollectionSize(ws) === 0) {
    gcs++;
    putWeak();
  }
}

assertTrue(gcs <= 5, `Expected at most 5 GCs, got ${gcs}`);
