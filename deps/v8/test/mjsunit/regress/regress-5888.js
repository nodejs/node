// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(32, 32, false);
  builder.addFunction("test", kSig_i_iii)
    .addBodyWithEnd([
// body:
kExprI64Const, 0xb4, 0x42,
kExprI64Const, 0x7a,
kExprI64Const, 0x42,
kExprI64Const, 0x7a,
kExprI64Ior,
kExprI64Ctz,
kExprI64Ctz,
kExprI64Shl,
kExprI64Mul,
kExprI64Const, 0x41,
kExprI64Ctz,
kExprI64Ctz,
kExprI64Shl,
kExprF32SConvertI64,
kExprI64Const, 0x42,
kExprI64Const, 0x02,
kExprI64Const, 0x7a,
kExprI64Mul,
kExprI64Const, 0x42,
kExprI64Ctz,
kExprI64Shl,
kExprI64Const, 0x7a,
kExprI64Ctz,
kExprI64Shl,
kExprI64Mul,
kExprI64Const, 0x41,
kExprI64Ctz,
kExprI64Ctz,
kExprI64Shl,
kExprF32SConvertI64,
kExprUnreachable,
kExprEnd,   // @65
            ])
            .exportFunc();
  var module = new WebAssembly.Module(builder.toBuffer());
})();

(function() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(16, 32, false);
  builder.addFunction("test", kSig_i_iii)
    .addBodyWithEnd([
      // body:
      kExprI64Const, 0x42,
      kExprI64Const, 0x7a,
      kExprI64Ctz,
      kExprI64Mul,
      kExprI64Ctz,
      kExprI64Const, 0x41,
      kExprI64Ctz,
      kExprI64Ctz,
      kExprI64Shl,
      kExprI64Const, 0x41,
      kExprI64Ctz,
      kExprI64Ctz,
      kExprI64Shl,
      kExprF32SConvertI64,
      kExprUnreachable,
      kExprEnd,   // @20
    ])
    .exportFunc();
  var module = new WebAssembly.Module(builder.toBuffer());
})();
