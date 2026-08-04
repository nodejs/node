// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

function makeBigInt(nDigits) {
  let result = 0n;
  for (let i = nDigits - 1; i >= 0; i--) {
    result = (result << 64n) | BigInt(i + 1);
  }
  return result;
}

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const BITFIELD_OFFSET = 4;

let y = makeBigInt(50);
let y_addr = Sandbox.getAddressOf(y);
const original_sign = mem.getUint32(y_addr + BITFIELD_OFFSET, true) & 1;

mem.setUint32(y_addr + BITFIELD_OFFSET, (30 << 8) | original_sign, true);

let x = makeBigInt(50);

for (let i = 0; i < 99; i++) {
  x % y;
}

y_addr = Sandbox.getAddressOf(y);
mem.setUint32(y_addr + BITFIELD_OFFSET, (50 << 5) | original_sign, true);
x % y;
x % y;
