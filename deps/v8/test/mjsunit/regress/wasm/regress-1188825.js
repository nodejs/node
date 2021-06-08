// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-eh
load('test/mjsunit/wasm/wasm-module-builder.js')
let obj = {};
let proxy = new Proxy(obj, {});
let builder = new WasmModuleBuilder();
builder.addType(kSig_v_v);
let imports = builder.addImport("m","f", kSig_v_v);
let exception = builder.addException(kSig_v_v);
builder.addFunction("foo", kSig_v_v)
    .addBody([
        kExprTry,
        kWasmVoid,
        kExprCallFunction, imports,
        kExprCatch, exception,
        kExprEnd]
        ).exportFunc();
let inst = builder.instantiate({
  m: {
    f: function () {
      throw proxy;
    }
  }
});
assertThrows(inst.exports.foo);
