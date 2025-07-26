// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-staging

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction("main", kSig_v_v).exportFunc()
  .addLocals(kWasmStringViewWtf16, 1)
  .addBody([
    kExprLoop, kWasmVoid,
      kExprUnreachable,
      kExprRefAsNonNull,
      kExprLocalSet, 0,
    kExprEnd,
  ]);

const instance = builder.instantiate();
assertTraps(kTrapUnreachable, instance.exports.main);
