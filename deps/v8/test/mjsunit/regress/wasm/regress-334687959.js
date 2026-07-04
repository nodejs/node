// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-inlining --wasm-tiering-budget=1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
// This was originally a regression test for a lazy validation bug, where
// function "fact_aux" had an invalid body.
builder.addFunction("fact_aux", kSig_i_ii).addBody([kExprLocalGet, 0]);
builder.addFunction("main", kSig_i_i)
       .addBody([kExprI32Const, 0,
                 kExprIf, kWasmI32,
                   kExprLocalGet, 0, kExprI32Const, 1, kExprReturnCall, 0,
                 kExprElse,
                   kExprI32Const, 1,
                 kExprEnd
                ])
       .exportFunc();
let instance = builder.instantiate();
instance.exports.main(3);
instance.exports.main(3);
