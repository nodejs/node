// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --wasm-interpret-all

load('test/mjsunit/wasm/wasm-module-builder.js');

assertThrows(() => {
 __v_29 = 0;
function __f_1() {
 __v_19 = new WasmModuleBuilder();
  if (__v_25) {
 __v_23 = __v_19.addImport('__v_24', '__v_30', __v_25);
  }
  if (__v_18) {
 __v_19.addMemory();
 __v_19.addFunction('load', kSig_i_i)
        .addBody([ 0])
        .exportFunc();
  }
 return __v_19;
}
 (function TestExternalCallBetweenTwoWasmModulesWithoutAndWithMemory() {
 __v_21 = __f_1(__v_18 = false, __v_25 = kSig_i_i);
 __v_21.addFunction('plus_one', kSig_i_i)
      .addBody([
        kExprGetLocal, 0,                   // -
        kExprCallFunction, __v_29      ])
      .exportFunc();
 __v_32 =
      __f_1(__v_18 = true, __v_25 = undefined);
 __v_31 = __v_32.instantiate(); try { __v_32[__getRandomProperty()] = __v_0; delete __v_18[__getRandomProperty()]; delete __v_34[__getRandomProperty()]; } catch(e) {; };
 __v_20 = __v_21.instantiate(
      {__v_24: {__v_30: __v_31.exports.load}});
 __v_20.exports.plus_one(); __v_33 = __v_43;
})();
});
