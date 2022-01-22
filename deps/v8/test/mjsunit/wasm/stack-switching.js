// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-wasm-stack-switching

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestStackSwitchNoSuspend() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI32, true).exportAs('g');
  builder.addFunction("test", kSig_v_v)
      .addBody([kExprI32Const, 42, kExprGlobalSet, 0]).exportFunc();
  let instance = builder.instantiate();
  let wrapper = %WasmReturnPromiseOnSuspend(instance.exports.test);
  wrapper();
  assertEquals(42, instance.exports.g.value);
})();
