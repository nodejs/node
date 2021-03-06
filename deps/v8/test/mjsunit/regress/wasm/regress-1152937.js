// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that decoding of 'br' exits early and does not invoke the codegen
// interface when reading the LEB128 branch target fails.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_v_v)
  .addBodyWithEnd([kExprBr, 0xFF]);

assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
