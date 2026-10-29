// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const GB = 1024 * 1024 * 1024;
const kMaxInSandboxBufferSize = 32*GB - 1;

let array = new BigInt64Array(new ArrayBuffer(8));

Sandbox.corruptObjectField(array, 'byte_length', 0xffffffffffffffffn, 64);
Sandbox.corruptObjectField(array, 'external_pointer', 0xffffffffffffffffn, 64);
Sandbox.corruptObjectField(array, 'base_pointer', 0xffffffff, 32);

assertEquals(array.byteLength, kMaxInSandboxBufferSize);
assertEquals(array.length, Math.floor(kMaxInSandboxBufferSize / 8));
array[array.length - 1] = 1337n;
