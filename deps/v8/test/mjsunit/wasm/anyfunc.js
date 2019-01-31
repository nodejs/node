// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-anyref --expose-gc

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function testAnyRefIdentityFunction() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_a_a)
      .addBody([kExprGetLocal, 0])
      .exportFunc();


  const instance = builder.instantiate();

  assertThrows(() => instance.exports.main(print), TypeError);
  assertThrows(() => instance.exports.main({'hello' : 'world'}), TypeError);
  assertSame(
      instance.exports.main, instance.exports.main(instance.exports.main));
})();

(function testPassAnyRefToImportedFunction() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_v_a);
  const imp_index = builder.addImport("q", "func", sig_index);
  builder.addFunction('main', sig_index)
      .addBody([kExprGetLocal, 0,
      kExprCallFunction, imp_index])
      .exportFunc();

  const main = builder.instantiate({q: {func: checkFunction}}).exports.main;

  function checkFunction(value) {
    assertSame(main, value);
  }

  main(main);
})();
