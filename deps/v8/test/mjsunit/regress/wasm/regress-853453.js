// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() => new WebAssembly.Module(
    new Uint8Array([
      0x00, 0x61, 0x73, 0x6d,     // wasm magic
      0x01, 0x00, 0x00, 0x00,     // wasm version
      0x04,                       // section code
      0x04,                       // section length
      /* Section: Table */
      0x01,                       // table count
      0x70,                       // table type
      0x03,                       // resizable limits flags
      0x00])),
    WebAssembly.CompileError);
