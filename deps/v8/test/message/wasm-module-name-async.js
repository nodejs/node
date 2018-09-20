// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --no-stress-opt

load('test/mjsunit/mjsunit.js');
load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();
builder.setName('test-module');
builder.addFunction(undefined, kSig_i_v)
    .addBody([kExprUnreachable])
    .exportAs('main');
let buffer = builder.toBuffer();
assertPromiseResult(WebAssembly.instantiate(buffer), pair => {
  try {
    pair.instance.exports.main();
  } catch (e) {
    print(e.stack);
  }
});
