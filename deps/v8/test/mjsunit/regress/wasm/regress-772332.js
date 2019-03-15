// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --wasm-interpret-all

load('test/mjsunit/wasm/wasm-module-builder.js');

assertThrows(() => {
let __v_50315 = 0;
function __f_15356(__v_50316, __v_50317) {
  let __v_50318 = new WasmModuleBuilder();
  if (__v_50317) {
    let __v_50319 = __v_50318.addImport('import_module', 'other_module_fn', kSig_i_i);
  }
      __v_50318.addMemory();
      __v_50318.addFunction('load', kSig_i_i).addBody([ 0, 0, 0]).exportFunc();
  return __v_50318;
}
  (function __f_15357() {
    let __v_50320 = __f_15356(__v_50350 = false, __v_50351 = kSig_i_i);
      __v_50320.addFunction('plus_one', kSig_i_i).addBody([kExprGetLocal, 0, kExprCallFunction, __v_50315, kExprI32Const, kExprI32Add, kExprReturn]).exportFunc();
    let __v_50321 = __f_15356();
    let __v_50324 = __v_50321.instantiate();
    let __v_50325 = __v_50320.instantiate({
      import_module: {
        other_module_fn: __v_50324.exports.load
      }
    });
 __v_50325.exports.plus_one();
  })();
});
