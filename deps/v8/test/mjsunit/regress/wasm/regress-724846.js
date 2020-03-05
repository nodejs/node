// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

// Flags: --wasm-max-mem-pages=49151

let builder = new WasmModuleBuilder();
const num_pages = 49152;
builder.addMemory(num_pages, num_pages);
// num_pages * 64k (page size) > kMaxInt.
assertThrows(() => builder.instantiate(), RangeError);
