// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_i_i)
    .addBody([
      // body:
      kExprGetLocal, 0,      // get_local 0
      kExprGetLocal, 0,      // get_local 0
      kExprLoop, kWasmStmt,  // loop
      kExprBr, 0,            // br depth=0
      kExprEnd,              // end
      kExprUnreachable,      // unreachable
    ])
    .exportFunc();
builder.instantiate();
