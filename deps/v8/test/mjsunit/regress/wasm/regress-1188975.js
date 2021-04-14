// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-eh

load("test/mjsunit/wasm/wasm-module-builder.js");

(function Regress1188975() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("f", kSig_v_v)
      .addBody([
        kExprUnreachable,
        kExprTry, kWasmVoid,
        kExprElse,
        kExprCatchAll,
        kExprEnd,
      ]);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();
