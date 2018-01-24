// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --wasm-interpret-all

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");


const builder = new WasmModuleBuilder();

const index = builder.addFunction("huge_frame", kSig_v_v)
    .addBody([kExprCallFunction, 0])
  .addLocals({f64_count: 49555}).exportFunc().index;
// We assume above that the function we added has index 0.
assertEquals(0, index);

const module = builder.instantiate();
assertThrows(module.exports.huge_frame, RangeError);
