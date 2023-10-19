// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function enterAndLeaveDebugging() {
  const builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_i_v).exportFunc().addBody(wasmI32Const(42));
  let main = builder.instantiate().exports.main;
  assertEquals(42, main());
  assertTrue(%IsLiftoffFunction(main));
  %WasmTierUpFunction(main);
  assertEquals(42, main());
  assertTrue(%IsTurboFanFunction(main));
  %WasmEnterDebugging();
  assertEquals(42, main());
  assertTrue(%IsWasmDebugFunction(main));
  %WasmLeaveDebugging();
  %WasmTierUpFunction(main);
  assertEquals(42, main());
  assertTrue(%IsTurboFanFunction(main));
})();
