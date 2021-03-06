// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addMemory(0, 0, false);
builder.addFunction('f', kSig_i_v)
    .addBody([kExprMemorySize, kMemoryZero])
    .exportFunc();
var instance = builder.instantiate();
instance.exports.f();
