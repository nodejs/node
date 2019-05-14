// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
builder.setName('test-module');
builder.addFunction(undefined, kSig_i_v)
    .addBody([kExprUnreachable])
    .exportAs('main');
builder.instantiate().exports.main();
