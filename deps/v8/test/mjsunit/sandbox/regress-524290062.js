// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-incremental-marking --concurrent-marking-high-priority-threads

const WASM_BYTES = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x04,                    // section length 4
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $f

  0x04,                    // section kind: Table
  0x04,                    // section length 4
  0x01, 0x70, 0x00,        // table count 1:  funcref no maximum
  0x01,                    // initial size 1

  0x07,                    // section kind: Export
  0x09,                    // section length 9
  0x02,                    // exports count 2
                           // export # 0
  0x01,                    // field name length:  1
  0x74,                    // field name: t
  0x01, 0x00,              // kind: table index:  0
                           // export # 1
  0x01,                    // field name length:  1
  0x66,                    // field name: f
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x04,                    // section length 4
  0x01,                    // functions count 1
                           // function #0 $f
  0x02,                    // body size 2
  0x00,                    // 0 entries in locals list
  0x0b,                    // end
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
