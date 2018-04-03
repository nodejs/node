// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --wasm-interpret-all

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

 __v_6 = new WasmModuleBuilder();
__v_6.addFunction('exp1', kSig_i_i).addBody([kExprUnreachable]).exportFunc();
 __v_7 = new WasmModuleBuilder();
 __v_7.addImport('__v_11', '__v_11', kSig_i_i);
try {
; } catch(e) {; }
 __v_8 = __v_6.instantiate().exports.exp1;
 __v_9 = __v_7.instantiate({__v_11: {__v_11: __v_8}}).exports.call_imp;
