// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_i_v)
    .addBody([kExprI32Const, 2, kExprI32Const, 0, kExprI32DivU])
    .exportFunc();
var module = builder.instantiate();
module.exports.main();
