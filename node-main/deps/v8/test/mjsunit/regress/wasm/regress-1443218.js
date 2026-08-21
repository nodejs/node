// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-debug-mask-for-testing=255

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory();
builder.addFunction('main', kSig_d_v)
    .addBody([
      ...wasmI32Const(0), kSimdPrefix, kExprS128Load32Splat, 0x01, 0x02,
      kExprUnreachable
    ])
    .exportFunc();
const instance = builder.instantiate();
assertTraps(kTrapMemOutOfBounds, instance.exports.main);
