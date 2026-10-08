// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --sandbox-testing

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function addrOf(obj) { return Sandbox.getAddressOf(obj); }
function w32(off, v) { mem.setUint32(off, v, true); }
function w64(off, v) { mem.setBigUint64(off, v, true); }

const M = 0xFFFFFFFFFFFFFFFFn;

const corrupt_len = 58;

let fake = 1n << 2047n;
for (let i = 0; i < 32; i++) {
  fake |= 0x414141414141n << BigInt(i * 64);
}
const x = 1n << BigInt((corrupt_len * 2 - 1) * 64);
const y = (1n << 64n) | 1n;
for (let i = 0; i < 110; i++) (y * 3n) % y;

gc(); gc();  // Make object addresses (more) stable.

const y_addr = addrOf(y);
const y_digits_base = y_addr + 8;
const fake_addr = addrOf(fake);
const x_addr = addrOf(x);

const X112 = (1n << 64n) - (BigInt(Sandbox.base + fake_addr + 8));
w64(x_addr + 904, X112);
for (let i = 2; i < corrupt_len; i++) {
  w64(y_digits_base + i * 8, M);
}
w32(y_addr + 4, (corrupt_len << 8) | 0);

x % y;

new Date().toString();  // Before the fix, this ran into corruption and crashed.
