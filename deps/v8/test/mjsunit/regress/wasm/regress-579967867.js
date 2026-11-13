// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const array32 = builder.addArray(kWasmI32, {is_shared: true});

builder.addFunction('main', makeSig([wasmRefNullType(array32)], []))
    .addBody([
      kExprLocalGet, 0,
      ...wasmI32Const(0x40000000),
      ...wasmI32Const(0),
      kAtomicPrefix, kExprArrayAtomicSet, kAtomicSeqCst, array32,
    ])
    .exportFunc();

const instance = builder.instantiate();
assertTraps(kTrapNullDereference, () => instance.exports.main(null));
