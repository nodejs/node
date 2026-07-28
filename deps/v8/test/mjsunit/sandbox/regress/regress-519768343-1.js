// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const kSlicedStringType = Sandbox.getInstanceTypeIdFor("SLICED_ONE_BYTE_STRING_TYPE");
const kParentOffset = Sandbox.getFieldOffset(kSlicedStringType, "parent");
const kOffsetOffset = Sandbox.getFieldOffset(kSlicedStringType, "offset");

let parent = "A".repeat(100);
let slices = [];
for (let i = 0; i < 3000; i++) {
  slices.push(parent.substring(1, 50));
}
assertEquals("SLICED_ONE_BYTE_STRING_TYPE", Sandbox.getInstanceTypeOf(slices[0]));
for (let i = 0; i < 5; i++) gc();

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

for (let i = 0; i < 2999; i++) {
  let addr_i = Sandbox.getAddressOf(slices[i]);
  let addr_next = Sandbox.getAddressOf(slices[i+1]);
  memory.setUint32(addr_i + kParentOffset, addr_next | 1, true);
  memory.setUint32(addr_i + kOffsetOffset, 0x60000000, true); // +0x30000000 Smi
}

slices[0].charCodeAt(0);
