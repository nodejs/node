// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);
builder.addFunction("test", kSig_i_v).addBody([
  kExprI32Const, 12,         // i32.const 0
]);

let module = new WebAssembly.Module(builder.toBuffer());
module.then = () => {
  // Use setTimeout to get out of the promise chain.
  setTimeout(assertUnreachable);
};

WebAssembly.instantiate(module);
