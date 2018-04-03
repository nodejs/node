// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --no-experimental-wasm-simd

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
    builder.addFunction("main", kSig_i_i)
      .addBody([kExprGetLocal, 0])
      .addLocals({s128_count: 1});

  assertFalse(WebAssembly.validate(builder.toBuffer()));
