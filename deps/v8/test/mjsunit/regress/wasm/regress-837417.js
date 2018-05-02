// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addMemory(16, 32);
builder.addFunction("test", kSig_i_v).addBody([
  kExprI32Const, 12,         // i32.const 0
]);

WebAssembly.Module.prototype.then = resolve => resolve(
    String.fromCharCode(null, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41));

// WebAssembly.instantiate should not actually throw a TypeError in this case.
// However, this is a workaround for
assertPromiseResult(
    WebAssembly.instantiate(builder.toBuffer()), assertUnreachable,
    exception => {
      assertInstanceof(exception, TypeError);
    });
