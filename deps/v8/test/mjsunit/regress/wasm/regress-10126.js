// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js')

let binary = new Binary;
binary.emit_bytes([
  kWasmH0,              //  0 header
  kWasmH1,              //  1 -
  kWasmH2,              //  2 -
  kWasmH3,              //  3 -
  kWasmV0,              //  4 version
  kWasmV1,              //  5 -
  kWasmV2,              //  6 -
  kWasmV3,              //  7 -
  kUnknownSectionCode,  //  8 custom section
  0x5,                  //  9 length
  0x6,                  // 10 invalid name length
  'a',                  // 11 payload
  'b',                  // 12 -
  'c',                  // 13 -
  'd',                  // 14 -
  kCodeSectionCode,     // 15 code section start
  0x1,                  // 16 code section length
  19,                   // 17 invalid number of functions
]);

const buffer = binary.trunc_buffer();
assertThrowsAsync(
    WebAssembly.compile(buffer), WebAssembly.CompileError,
    'WebAssembly.compile(): expected 6 bytes, fell off end @+11');
