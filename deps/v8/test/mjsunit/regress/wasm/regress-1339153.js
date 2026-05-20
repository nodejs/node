// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

builder.addFunction("main", kSig_i_iii)
  .addBody([kExprLoop, kWasmI32,
              ...wasmI32Const(10),
              ...wasmI32Const(10),
              kExprLocalGet, 0,
              kExprLocalTee, 1,
              kExprCallFunction, 0,
              kExprLocalGet, 1,
              kExprLocalGet, 0,
              kExprBrIf, 0,
              kExprI32DivS,
            kExprEnd]);

builder.instantiate();
