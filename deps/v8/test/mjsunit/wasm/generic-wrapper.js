// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-generic-wrapper --expose-gc

load("test/mjsunit/wasm/wasm-module-builder.js");

(function testGenericWrapper0Param() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_v);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func() {
    gc();
    x = 20;
  }

  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(undefined, instance.exports.main());
  assertEquals(x, 20);
})();

(function testGenericWrapper0ParamTraps() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_v);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprUnreachable
    ])
    .exportFunc();

  let instance = builder.instantiate();
  assertTraps(kTrapUnreachable, instance.exports.main);
})();

(function testGenericWrapper1ParamTrap() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_i);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0, kExprUnreachable
    ])
    .exportFunc();

  let instance = builder.instantiate();
  assertTraps(kTrapUnreachable, () => instance.exports.main(1));
})();

(function testGenericWrapper1ParamGeneral() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_i);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0, kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func(param) {
    gc();
    x += param;
  }

  let instance = builder.instantiate({ mod: { func: import_func } });
  instance.exports.main(5);
  assertEquals(17, x);
})();

(function testGenericWrapper1ParamNotSmi() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_i);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0, kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func(param) {
    gc();
    x += param;
  }

  let y = { valueOf: () => { print("Hello!"); gc(); return 24; } };
  let instance = builder.instantiate({ mod: { func: import_func } });
  instance.exports.main(y);
  assertEquals(36, x);
})();
