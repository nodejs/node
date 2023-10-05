// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --wasm-disable-deprecated

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");


(function DeprecatedRefTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let fctType = builder.addType(makeSig([], []));

  builder.addFunction('refTest',
      makeSig([kWasmAnyRef], [kWasmI32]))
    .addBody([
      kExprRefNull, kNullFuncRefCode,
      kGCPrefix, kExprRefTestDeprecated, fctType,
    ])
    .exportFunc();

  assertThrows(
    () => builder.instantiate({}),
    WebAssembly.CompileError,
    /opcode ref.test is a deprecated gc instruction/);
})();
