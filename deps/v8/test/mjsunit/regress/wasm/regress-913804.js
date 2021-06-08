// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_v_v).addBody([
  kExprLoop, kWasmVoid,        // loop
  /**/ kExprBr, 0x01,          //   br depth=1
  /**/ kExprBlock, kWasmVoid,  //   block
  /**/ /**/ kExprBr, 0x02,     //     br depth=2
  /**/ /**/ kExprEnd,          //     end [block]
  /**/ kExprEnd                //   end [loop]
]);
builder.instantiate();
