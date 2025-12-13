// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-fast-api --expose-fast-api --allow-natives-syntax

const t0 = d8.test.FastCAPI;
const v3 = new t0();
let v4 = Function.prototype.call.bind(v3.add_32bit_int);


d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let import_0_v4 = builder.addImport('imports', 'import_0_v4',
    makeSig([kWasmExternRef, kWasmI32, kWasmI32], [kWasmI32]));
let w1 = builder.addFunction(undefined, makeSig([], []));
let w3 = builder.addFunction(undefined,
  makeSig([kWasmI64, kWasmI32, kWasmF32, kWasmAnyRef, kWasmExternRef], []));

w1.addBody([
  ]);

w3.addLocals(kWasmI32, 1)
  .addBody([
    kExprTry, kWasmVoid,
      kExprCallFunction, w1.index,
    kExprCatchAll,
      kExprLocalGet, 4,
      kExprLocalGet, 1,
      kExprLocalGet, 5,
      kExprCallFunction, import_0_v4,
      kExprDrop,
    kExprEnd,
  ]);

builder.addExport('w3', w3.index);

const instance = builder.instantiate({imports: {
    import_0_v4: v4,
}});

const v57 = instance.exports.w3;
v57(0n);
%WasmTierUpFunction(v57);
