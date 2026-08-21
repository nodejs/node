// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestWasmI64ToJSBigInt() {
  let builder = new WasmModuleBuilder();

  builder
    .addFunction("fn", kSig_l_v) // () -> i64
    .addBody([
      kExprI64Const, 0x3,
    ])
    .exportFunc();

  let module = builder.instantiate();

  assertEquals(typeof module.exports.fn(), "bigint");
  assertEquals(module.exports.fn(), 3n);
})();

(function TestJSBigIntToWasmI64Global() {
  let builder = new WasmModuleBuilder();

  let a_global_index = builder
    .addImportedGlobal("mod", "a", kWasmI64);

  let b_global_index = builder
    .addImportedGlobal("mod", "b", kWasmI64);

  builder
    .addExportOfKind('a', kExternalGlobal, a_global_index)
    .addExportOfKind('b', kExternalGlobal, b_global_index)

  let module = builder.instantiate({
    mod: {
      a: 1n,
      b: 2n ** 63n,
    }
  });

  assertEquals(module.exports.a.value, 1n);
  assertEquals(module.exports.b.value, - (2n ** 63n));
})();

(function TestJSBigIntGlobalImportInvalidType() {
  let builder = new WasmModuleBuilder();
  builder.addImportedGlobal("mod", "a", kWasmI64);
  assertThrows(() => builder.instantiate({mod: { a: {} } }), WebAssembly.LinkError);
})();

(function TestJSBigIntToWasmI64MutableGlobal() {
  let builder = new WasmModuleBuilder();

  let a_global_index = builder
    .addImportedGlobal("mod", "a", kWasmI64, /* mutable = */ true)

  builder
    .addExportOfKind('a', kExternalGlobal, a_global_index);

  // as non object
  let fn = () => builder.instantiate({
    mod: {
      a: 1n,
    }
  });

  assertThrows(fn, WebAssembly.LinkError);

  // as WebAssembly.Global object
  let module = builder.instantiate({
    mod: {
      a: new WebAssembly.Global({ value: "i64", mutable: true }, 1n),
    }
  });

  assertEquals(module.exports.a.value, 1n);
})();

(function TestJSBigIntToWasmI64Identity() {
  let builder = new WasmModuleBuilder();

  builder
    .addFunction("f", kSig_l_l) // i64 -> i64
    .addBody([
      kExprLocalGet, 0,
    ])
    .exportFunc();

  let module = builder.instantiate();
  let f = module.exports.f;

  assertEquals(f(0n), 0n);
  assertEquals(f(123n), 123n);
  assertEquals(f(-123n), -123n);

  assertEquals(f("5"), 5n);

  assertThrows(() => f(5), TypeError);
})();

(function TestJSBigIntToWasmI64Projection() {
  let builder = new WasmModuleBuilder();

  builder
    .addFunction("f", kSig_l_ll) // i64 -> i64
    .addBody([
      kExprLocalGet, 1,
    ])
    .exportFunc();

  let module = builder.instantiate();
  let f = module.exports.f;

  assertEquals(f(1n, 0n), 0n);
  assertEquals(f(1n, 123n), 123n);
  assertEquals(f(1n, -123n), -123n);

  assertEquals(f(1n, "5"), 5n);

  assertThrows(() => f(1n, 5), TypeError);
})();

(function TestI64Global() {
  let argument = { "value": "i64", "mutable": true };
  let global = new WebAssembly.Global(argument);

  assertEquals(global.value, 0n); // initial value

  global.value = 123n;
  assertEquals(global.valueOf(), 123n);

  global.value = 2n ** 63n;
  assertEquals(global.valueOf(), - (2n ** 63n));
})();

(function TestI64GlobalValueOf() {
  let argument = { "value": "i64" };

  // as literal
  let global = new WebAssembly.Global(argument, {
    valueOf() {
      return 123n;
    }
  });
  assertEquals(global.value, 123n);

  // as string
  let global2 = new WebAssembly.Global(argument, {
    valueOf() {
      return "321";
    }
  });
  assertEquals(global2.value, 321n);
})();

(function TestInvalidValtypeGlobalErrorMessage() {
  let argument = { "value": "some string" };
  assertThrows(() => new WebAssembly.Global(argument), TypeError);

  try {
    new WebAssembly.Global(argument);
  } catch (e) {
    assertContains("'value' must be a WebAssembly type", e.message);
  }
})();

(function TestGlobalI64ValueWrongType() {
  let argument = { "value": "i64" };
  assertThrows(() => new WebAssembly.Global(argument, 666), TypeError);
})();

(function TestGlobalI64SetWrongType() {
  let argument = { "value": "i64", "mutable": true };
  let global = new WebAssembly.Global(argument);

  assertThrows(() => global.value = 1, TypeError);
})();

(function TestFuncParamF64PassingBigInt() {
  let builder = new WasmModuleBuilder();

  builder
      .addFunction("f", kSig_v_d) // f64 -> ()
      .addBody([])
      .exportFunc();

  let module = builder.instantiate();

  assertThrows(() => module.exports.f(123n), TypeError);
})();
