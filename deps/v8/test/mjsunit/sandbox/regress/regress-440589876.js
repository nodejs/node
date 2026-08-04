// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const kSeqStringType = Sandbox.getInstanceTypeIdFor("SEQ_TWO_BYTE_STRING_TYPE");
const kStringLengthOffset = Sandbox.getFieldOffset(kSeqStringType, "length");

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const s1 = "💥aaaa";
const s2 = "💥bbbb";

let s1_addr = Sandbox.getAddressOf(s1);
let s2_addr = Sandbox.getAddressOf(s2);

memory.setUint32(s1_addr, 0x105, true);  // kSeqOneByteStringMap

memory.setUint32(s1_addr + kStringLengthOffset, 0xff000000, true);
memory.setUint32(s2_addr + kStringLengthOffset, 0xff000000, true);

s2.localeCompare(s1, "tr", { sensitivity: "base" });
