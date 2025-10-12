// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

let sbx_memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
let len = 0x20000;
let ar = new Int32Array(
    new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * len));

function corruptInBackground(address, valueA, valueB) {
  function workerTemplate(address, valueA, valueB) {
    let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
    while (true) {
      memory.setUint8(address, valueA, true);
      memory.setUint8(address, valueB, true);
    }
  }
  const workerCode = new Function(
      `(${workerTemplate})(${address}, ${valueA}, ${valueB})`);
  return new Worker(workerCode, { type: 'function' });
}

let ar_addr = Sandbox.getAddressOf(ar);
let ar_map = sbx_memory.getUint32(ar_addr, true);
let bit_field2_addr = ar_map + 10;
let bit_field2_val = sbx_memory.getUint8(bit_field2_addr);

let worker = corruptInBackground(
    bit_field2_addr, bit_field2_val, bit_field2_val ^ 0xff);

for (let i = 0; i < 10000; ++i) {
  ar.sort();
}
