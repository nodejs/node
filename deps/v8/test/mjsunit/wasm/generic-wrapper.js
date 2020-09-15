// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-generic-wrapper --expose-gc --allow-natives-syntax

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
  assertEquals(undefined, instance.exports.main(5));
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
  assertEquals(undefined, instance.exports.main(y));
  assertEquals(36, x);
})();

(function testGenericWrapper4Param() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_iiii);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func(param1, param2, param3, param4) {
    gc();
    x += 2 * param1 + 3 * param2 + 4 * param3 + 5 * param4;
  }

  let param2 = { valueOf: () => { gc(); return 6; } };
  let param3 = { valueOf: () => { gc(); return 3; } };
  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(undefined, instance.exports.main(9, param2, param3, 0));
  assertEquals(60, x);
  // Now we test if the evaluation order of the parameters is correct.
  x = 12;
  param3 = {
    valueOf: () => {
      Object.defineProperty(param2, 'valueOf', {
        value: () => 30
      })
      return 3;
    }
  };
  assertEquals(undefined, instance.exports.main(9, param2, param3, 0));
  assertEquals(60, x);
})();

let kSig_v_iiiiiiii = makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmI32,
  kWasmI32, kWasmI32, kWasmI32, kWasmI32], []);

(function testGenericWrapper8Param() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_iiiiiiii);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kExprLocalGet, 5,
      kExprLocalGet, 6,
      kExprLocalGet, 7,
      kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func(param1, param2, param3, param4, param5, param6,
    param7, param8) {
    gc();
    x += param1 + 2 * param2 + 3 * param3 + 4 * param4 + 5 * param5
      + 6 * param6 + 7 * param7 + 8 * param8;
  }

  let param1 = { valueOf: () => { gc(); return 5; } };
  let param4 = { valueOf: () => { gc(); return 8; } };
  let param6 = { valueOf: () => { gc(); return 10; } };
  let param8 = { valueOf: () => { gc(); return 12; } };
  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(undefined, instance.exports.main(param1, 6, 7, param4, 9, param6, 11, param8));
  assertEquals(360, x);
})();

// Passing less parameters than expected.
(function testGenericWrapper4ParamWithLessParams() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_iiii);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func(param1, param2, param3, param4) {
    gc();
    x += param1 + param2 + param3 + param4;
  }

  let param2 = { valueOf: () => { gc(); return 3; } };
  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(undefined, instance.exports.main(5, param2));
  assertEquals(20, x);
})();

// Passing more parameters than expected.
(function testGenericWrapper4ParamWithMoreParams() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_iiii);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func(param1, param2, param3, param4) {
    gc();
    x += param1 + param2 + param3 + param4;
  }

  let param2 = { valueOf: () => { gc(); return 3; } };
  let param3 = { valueOf: () => { gc(); return 6; } };
  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(undefined, instance.exports.main(5, param2, param3, 7, 200, 300, 400));
  assertEquals(33, x);
})();

(function testGenericWrapper1ReturnSmi() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_i_i);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0, kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func(param) {
    gc();
    return x + param;
  }

  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(17, instance.exports.main(5));
})();

(function testGenericWrapper1ReturnHeapNumber() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_i_i);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0, kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 2147483640;
  function import_func(param) {
    let result = x + param;
    %SimulateNewspaceFull();
    return result;
  }

  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(2147483645, instance.exports.main(5));
})();

let kSig_i_lili = makeSig([kWasmI64, kWasmI32, kWasmI64, kWasmI32], [kWasmI32]);

(function testGenericWrapper4IParam1I32Ret() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_i_lili);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12n;
  function import_func(param1, param2, param3, param4) {
    x += 2n * param1 + BigInt(3 * param2) + 4n * param3 + BigInt(5 * param4);
    return Number(x);
  }

  let param2 = { valueOf: () => { gc(); return 6; } };
  let param3 = { valueOf: () => { gc(); return 3n; } };
  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(60, instance.exports.main(9n, param2, param3, 0));
})();

let kSig_v_liilliiil = makeSig([kWasmI64, kWasmI32, kWasmI32, kWasmI64,
  kWasmI64, kWasmI32, kWasmI32, kWasmI32, kWasmI64], [kWasmI32]);

(function testGenericWrapper9IParam132Ret() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_liilliiil);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kExprLocalGet, 5,
      kExprLocalGet, 6,
      kExprLocalGet, 7,
      kExprLocalGet, 8,
      kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12;
  function import_func(param1, param2, param3, param4, param5, param6,
    param7, param8, param9) {
    x += Number(param1) + 2 * param2 + 3 * param3 + Number(4n * param4) + Number(5n * param5)
      + 6 * param6 + 7 * param7 + 8 * param8 + Number(9n * param9);
    return x;
  }

  let param1 = { valueOf: () => { gc(); return 5n; } };
  let param4 = { valueOf: () => { gc(); return 8n; } };
  let param6 = { valueOf: () => { gc(); return 10; } };
  let param8 = { valueOf: () => { gc(); return 12; } };
  let instance = builder.instantiate({ mod: { func: import_func } });
  assertEquals(360, instance.exports.main(param1, 6, 7, param4, 9n, param6, 11, param8, 0n));
})();

// The function expects BigInt, but gets Number.
(function testGenericWrapperTypeError() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_l);
  let func_index = builder.addImport("mod", "func", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprLocalGet, 0, kExprCallFunction, func_index
    ])
    .exportFunc();

  let x = 12n;
  function import_func(param1) {
    x += param1;
  }

  let instance = builder.instantiate({ mod: { func: import_func } });
  assertThrows(() => { instance.exports.main(17) }, TypeError);
})();
