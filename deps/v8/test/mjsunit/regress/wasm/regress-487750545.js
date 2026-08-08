// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

let bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x02,                    // section length 2
  0x01, 0x65, 0xab,        // types count 1: type #0 $type0 shared kind: unknown

]);
assertThrows(() => new WebAssembly.Module(bytes), WebAssembly.CompileError);
