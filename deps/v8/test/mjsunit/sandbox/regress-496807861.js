// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

function makeBigInt(nDigits) {
  let result = 0n;
  for (let i = nDigits - 1; i >= 0; --i) {
    result = (result << 64n) | BigInt(i + 1);
  }
  return result;
}

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const BITFIELD_OFFSET = 4;

const x = makeBigInt(128);
const y = (1n << 64n) | 1n;

const addr = Sandbox.getAddressOf(x);
mem.setUint32(addr + BITFIELD_OFFSET, 0xfffffffe, true);

x % y;
