// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_v_v).addBody([]);
builder.addFunction(undefined, kSig_i_i)
  .addBody([
    kExprGetLocal, 0,
    kExprGetLocal, 0,
    // Stack now contains two copies of the first param register.
    // Start a loop to create a merge point (values still in registers).
    kExprLoop, kWasmStmt,
      // The call spills all values.
      kExprCallFunction, 0,
      // Break to the loop. Now the spilled values need to be loaded back *into
      // the same register*.
      kExprBr, 0,
      kExprEnd,
    kExprDrop
]);
builder.instantiate();
