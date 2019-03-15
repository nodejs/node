// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addCustomSection('BBBB', []);
builder.addCustomSection('AAAA', new Array(32).fill(0));
let buffer = builder.toBuffer();
// Shrink the buffer by 30 bytes (content of the unknown section named 'AAAA').
buffer = buffer.slice(0, buffer.byteLength - 30);
// Instantiation should fail on the truncated buffer.
assertThrows(() => new WebAssembly.Module(buffer), WebAssembly.CompileError);
