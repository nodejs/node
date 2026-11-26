// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const heap = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const addr = value => Sandbox.getAddressOf(value);
const u32 = p => heap.getUint32(p, true);
const w32 = (p, value) => heap.setUint32(p, value >>> 0, true);
const mapOf = value => u32(addr(value)) & ~1;
const descriptor = (map, i) => (u32(map + 24) & ~1) + 16 + 12 * i;

const seed = new TypeError('baseline');
assertEquals('string', typeof seed.stack);

const errorEntry = descriptor(mapOf(seed), 0);
const donor = {};
Object.defineProperty(donor, 'prop', {
  configurable: true,
  set(value) {
    assertUnreachable();
  }
});
const donorEntry = descriptor(mapOf(donor), 0);
const savedDetails = u32(errorEntry + 4);
const savedValue = u32(errorEntry + 8);

w32(errorEntry + 4, u32(donorEntry + 4));
w32(errorEntry + 8, u32(donorEntry + 8));
try {
  new TypeError('test');
} finally {
  w32(errorEntry + 4, savedDetails);
  w32(errorEntry + 8, savedValue);
}
assertUnreachable();
