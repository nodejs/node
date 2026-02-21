// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function Regress1192313() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(16, 32);
  builder.addFunction('f', kSig_i_i)
      .addBody([
          kExprTry, kWasmI32,
            kExprI32Const, 0,
            kExprI32Const, 0,
            kExprCallFunction, 0,
            kAtomicPrefix, kExprI32AtomicAnd8U,
            0x00, 0xba, 0xe2, 0x81, 0xd6, 0x0b,
          kExprCatchAll,
            kExprTry, kWasmI32,
              kExprI32Const, 0,
              kExprI32Const, 0,
              kAtomicPrefix, kExprI32AtomicAnd8U,
              0x00, 0x85, 0x97, 0xc4, 0x5f,
            kExprDelegate, 1,
          kExprEnd]).exportFunc();
  let instance = builder.instantiate();
})();
