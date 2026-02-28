// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let main = builder.addFunction(undefined, kSig_v_v).exportAs('main');
let $mem0 = builder.addMemory64(1, 2);

main.addBody([
    ...wasmI64Const(8589934592n),
    kSimdPrefix, kExprS128Load16Splat, 1, ...wasmSignedLeb64(52599n),
    kExprDrop,
  ]);

const instance = builder.instantiate();
assertTraps(kTrapMemOutOfBounds, instance.exports.main);
