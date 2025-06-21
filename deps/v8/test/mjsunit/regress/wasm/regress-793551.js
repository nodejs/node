// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_i_i)
    .addBody([
      // body:
      kExprLocalGet, 0,      // get_local 0
      kExprLocalGet, 0,      // get_local 0
      kExprLoop, kWasmVoid,  // loop
      kExprBr, 0,            // br depth=0
      kExprEnd,              // end
      kExprUnreachable,      // unreachable
    ])
    .exportFunc();
builder.instantiate();
