// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const kTypedArrayType = Sandbox.getInstanceTypeIdFor("JS_TYPED_ARRAY_TYPE");
const kTypedArrayByteLengthOffset =
  Sandbox.getFieldOffset(kTypedArrayType, "byte_length");
const kTypedArrayExternalPointerOffset =
  Sandbox.getFieldOffset(kTypedArrayType, "external_pointer");
const kTypedArrayBasePointerOffset =
  Sandbox.getFieldOffset(kTypedArrayType, "base_pointer");
const GB = 1024 * 1024 * 1024;
const kMaxInSandboxBufferSize = 32*GB - 1;

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

let array = new BigInt64Array(new ArrayBuffer(8));
let array_address = Sandbox.getAddressOf(array);

let byte_length_address = array_address + kTypedArrayByteLengthOffset;
memory.setBigUint64(byte_length_address, 0xffffffffffffffffn, true);
let pointer_address = array_address + kTypedArrayExternalPointerOffset;
memory.setBigUint64(pointer_address, 0xffffffffffffffffn, true);
let offset_address = array_address + kTypedArrayBasePointerOffset;
memory.setUint32(offset_address, 0xffffffff, true);

assertEquals(array.byteLength, kMaxInSandboxBufferSize);
assertEquals(array.length, Math.floor(kMaxInSandboxBufferSize / 8));
array[array.length - 1] = 1337n;
