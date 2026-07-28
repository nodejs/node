// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const GB = 1024 * 1024 * 1024;
const kMaxInSandboxBufferSize = 32*GB - 1;
// Something reasonable, must be smaller than the maximum module size.
const kBufferSize = 1 * GB;
// When stored on-heap, these offsets and sizes are left shifted to guarantee
// that they are always smaller than the maximum buffer size.
const kBoundedSizeShift = 29;
const kShiftedBufferSize = BigInt(kBufferSize) << BigInt(kBoundedSizeShift);

let array = new Uint8Array(new ArrayBuffer(0));

Sandbox.corruptObjectField(array, 'byte_offset', 0xffffffffffffffffn, 64);
Sandbox.corruptObjectField(array, 'byte_length', kShiftedBufferSize, 64);

assertEquals(array.byteOffset, kMaxInSandboxBufferSize);
assertEquals(array.byteLength, kBufferSize);

// WebAssembly.Validate (and similar APIs) will access the TypedArray's data by
// fetching the Data() of the associated ArrayBuffer's BackingStore, then
// adding the ByteOffset(). The Data() of the BackingStore must never be
// nullptr, otherwise we'd end up accessing out-of-sandbox memory.
WebAssembly.validate(array);
