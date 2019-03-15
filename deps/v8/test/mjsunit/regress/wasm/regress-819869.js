// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_i_i)
    .addLocals({i32_count: 0xffffffff})
    .addBody([]);
assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
