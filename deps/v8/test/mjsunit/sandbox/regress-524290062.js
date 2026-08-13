// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-incremental-marking --concurrent-marking-high-priority-threads

const WASM_BYTES = new Uint8Array([
  0x00,0x61,0x73,0x6d, 0x01,0x00,0x00,0x00,   // magic + version
  0x01,0x04,0x01, 0x60,0x00,0x00,             // type:    () -> ()
  0x03,0x02,0x01, 0x00,                       // func:    1 func of type 0
  0x04,0x04,0x01, 0x70,0x00,0x01,             // table:   funcref, min=1
  0x07,0x09,0x02,                             // export:  2 exports
    0x01,0x74, 0x01,0x00,                     //   "t" -> table 0
    0x01,0x66, 0x00,0x00,                     //   "f" -> func 0
  0x0a,0x04,0x01, 0x02,0x00,0x0b              // code:    empty body
]);
const module = new WebAssembly.Module(WASM_BYTES);

const POOL = 4096;
const pool = new Array(POOL);
let head = 0;

// 100,000 iterations is enough to trigger the race if the bug is present,
// but runs quickly (under 2 seconds) when the bug is fixed.
const ITERATIONS = 100000;

for (let iter = 1; iter <= ITERATIONS; iter++) {
  const old = pool[head];
  if (old !== undefined) old.exports.t.grow(8, old.exports.f);

  pool[head] = new WebAssembly.Instance(module);
  head = (head + 1) % POOL;
}
