// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $array = builder.addArray(kWasmI32, true, kNoSuperType, false);
let $sig = builder.addType(kSig_i_v);
let main = builder.addFunction(undefined, $sig).exportAs('main');

main.addBody([
    kExprI32Const, 1,
    kExprI32Const, 2,
    kGCPrefix, kExprArrayNew, $array,
    kExprI32Const, 0,
    kExprI32Const, 42,
    kAtomicPrefix, kExprArrayAtomicXor, kAtomicAcqRel, $array,
  ]);

const instance = builder.instantiate({});
assertEquals(1, instance.exports.main())
