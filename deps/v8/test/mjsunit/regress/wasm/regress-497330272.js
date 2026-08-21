// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --experimental-wasm-stringref

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
const struct = builder.addStruct([makeField(kWasmI32, true)]);

builder.addFunction('test', makeSig([wasmRefNullType(struct)], [kWasmI32]))
  .addBody([
    kExprBlock, kWasmRef, struct,
      kExprLocalGet, 0,
      kGCPrefix, kExprBrOnCastFail, 0b11, 0, struct, kStringViewWtf8Code,
      kExprDrop,
      kExprUnreachable,
    kExprEnd,
    kGCPrefix, kExprStructGet, struct, 0,
  ])
  .exportAs('test');

assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
             /nullable string views don't exist/);
