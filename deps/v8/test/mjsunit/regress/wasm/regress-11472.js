// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --experimental-wasm-eh

load("test/mjsunit/wasm/wasm-module-builder.js");

(function Regress11472() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let throw_fn = builder.addFunction('throw', kSig_v_v)
      .addBody([kExprNop])
      .exportFunc();
  builder.addFunction('test', kSig_i_ii)
      .addBody([
        kExprTry, kWasmI32,
          kExprCallFunction, throw_fn.index,
          kExprCallFunction, throw_fn.index,
          kExprTry, kWasmI32,
            kExprCallFunction, throw_fn.index,
            kExprI32Const, 1,
          kExprDelegate, 0,
        kExprCatchAll,
          kExprI32Const, 2,
        kExprEnd,
      ]).exportFunc();
  instance = builder.instantiate();
})();
