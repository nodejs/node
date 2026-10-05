// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let uninhabitable = wasmRefType(kWasmNullRef);

builder.addImport("m", "f", makeSig([uninhabitable], [uninhabitable]));

builder.addImport("m", "g", makeSig([uninhabitable, kWasmI32],
                                    [uninhabitable, kWasmI32]));

builder.addExport("f_exported", 0);
builder.addExport("g_exported", 1);

let wasm = builder.instantiate(
    {m: {f: (x) => null, g: (x, y) => [null, 42]}})
  .exports;

assertThrows(() => wasm.f_exported(null), TypeError,
             "type incompatibility when transforming from/to JS")
assertThrows(() => wasm.g_exported(null, 42), TypeError,
             "type incompatibility when transforming from/to JS");
