// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestFunctionGlobalGetAndSet() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let fun1 = new WebAssemblyFunction({parameters:[], results:["i32"]}, _ => 7);
  let fun2 = new WebAssemblyFunction({parameters:[], results:["i32"]}, _ => 9);
  builder.addGlobal(kWasmAnyFunc, true, false).exportAs("f");
  builder.addFunction('get_global', kSig_a_v)
      .addBody([
        kExprGlobalGet, 0,
      ])
      .exportFunc();
  builder.addFunction('set_global', kSig_v_a)
      .addBody([
        kExprLocalGet, 0,
        kExprGlobalSet, 0,
      ])
      .exportFunc();
  let instance = builder.instantiate();

  // Test getting and setting "funcref" global via WebAssembly.
  assertEquals(null, instance.exports.get_global());
  instance.exports.set_global(fun1);
  assertEquals(fun1, instance.exports.get_global());

  // Test getting and setting "funcref" global via JavaScript.
  assertEquals(fun1, instance.exports.f.value);
  instance.exports.f.value = fun2;
  assertEquals(fun2, instance.exports.f.value);

  // Test the full round-trip of an "funcref" global.
  assertEquals(fun2, instance.exports.get_global());
})();

(function TestFunctionMultiTableSetAndCall() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let v1 = 7; let v2 = 9; let v3 = 0.0;
  let f1 = new WebAssemblyFunction({parameters:[], results:["i32"]}, _ => v1);
  let f2 = new WebAssemblyFunction({parameters:[], results:["i32"]}, _ => v2);
  let f3 = new WebAssemblyFunction({parameters:[], results:["f64"]}, _ => v3);
  let table = new WebAssembly.Table({element: "anyfunc", initial: 2});
  let table_index0 = builder.addImportedTable("m", "table", 2);
  let table_index1 = builder.addTable(kWasmAnyFunc, 1).exportAs("tbl").index;
  let sig_index = builder.addType(kSig_i_v);
  table.set(0, f1);
  builder.addFunction('call0', kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprCallIndirect, sig_index, table_index0
      ])
      .exportFunc();
  builder.addFunction('call1', kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprCallIndirect, sig_index, table_index1
      ])
      .exportFunc();
  let instance = builder.instantiate({ m: { table: table }});

  // Test table #0 first.
  assertEquals(v1, instance.exports.call0(0));
  assertSame(f1, table.get(0));
  table.set(1, f2);
  assertEquals(v2, instance.exports.call0(1));
  assertSame(f2, table.get(1));
  table.set(1, f3);
  assertTraps(kTrapFuncSigMismatch, () => instance.exports.call0(1));
  assertSame(f3, table.get(1));

  // Test table #1 next.
  assertTraps(kTrapNullFunc, () => instance.exports.call1(0));
  instance.exports.tbl.set(0, f1);
  assertEquals(v1, instance.exports.call1(0));
  assertSame(f1, instance.exports.tbl.get(0));
  instance.exports.tbl.set(0, f2);
  assertEquals(v2, instance.exports.call1(0));
  assertSame(f2, instance.exports.tbl.get(0));
  instance.exports.tbl.set(0, f3);
  assertTraps(kTrapFuncSigMismatch, () => instance.exports.call1(0));
  assertSame(f3, instance.exports.tbl.get(0));
})();
