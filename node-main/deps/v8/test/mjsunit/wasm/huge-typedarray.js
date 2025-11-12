// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-max-mem-pages=65536

// Currently, the only way to create a huge TypedArray is via a
// WebAssembly Memory object.

const kNumPages = 65536;
const kWasmPageSize = 65536;
const kBytes = kNumPages * kWasmPageSize;
const kArrayLength = kBytes - 1;
assertEquals(2 ** 32, kBytes);
assertEquals(0xFFFFFFFF, kArrayLength);

var mem = new WebAssembly.Memory({ initial: kNumPages });
var buffer = mem.buffer;
var array = new Uint8Array(buffer, 0, kArrayLength);

assertEquals(kBytes, buffer.byteLength);
assertEquals(kArrayLength, array.length);
assertEquals(undefined, array[-1]);
assertEquals(0, array[0]);
assertEquals(0, array[kArrayLength - 1]);
assertEquals(undefined, array[kArrayLength]);
