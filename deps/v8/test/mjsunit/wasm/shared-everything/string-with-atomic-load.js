// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let kSig_i_ri = makeSig([kWasmExternRef, kWasmI32], [kWasmI32]);
let kSig_ii_ri = makeSig([kWasmExternRef, kWasmI32], [kWasmI32, kWasmI32]);

let builder = new WasmModuleBuilder();

let struct = builder.addStruct({fields: [makeField(kWasmI32, true)],
                                shared: true});

let kStringCharCodeAt =
      builder.addImport('wasm:js-string', 'charCodeAt', kSig_i_ri);

builder.addFunction("foo", kSig_ii_ri)
  .addLocals(wasmRefNullType(struct), 1)
  .addBody([kExprLocalGet, 1, kGCPrefix, kExprStructNew, struct,
            kExprLocalSet, 2,
            kExprLocalGet, 0, kExprLocalGet, 1,
            kExprCallFunction, kStringCharCodeAt,
            kExprLocalGet, 2,
            kAtomicPrefix, kExprStructAtomicGet, 0, struct, 0
  ])
  .exportFunc();

let wasm = builder.instantiate({}, { builtins: ["js-string"] }).exports;

assertEquals([108, 2], wasm.foo("hello", 2));
