// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_i_i)
    .addBody([
      kExprLocalGet, 0x00,    // get_local 0
      kExprBlock, kWasmVoid,  // block
      kExprBr, 0x00,          // br depth=0
      kExprEnd,               // end
      kExprBlock, kWasmVoid,  // block
      kExprBr, 0x00,          // br depth=0
      kExprEnd,               // end
      kExprBr, 0x00,          // br depth=0
    ])
    .exportFunc();
builder.instantiate();
