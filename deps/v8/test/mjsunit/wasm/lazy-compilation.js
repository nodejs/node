// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation --allow-natives-syntax

load('test/mjsunit/wasm/wasm-module-builder.js');

(function importFromOtherInstance() {
  print(arguments.callee.name);
  const builder1 = new WasmModuleBuilder();
  builder1.addFunction('func', kSig_v_v).addBody([]).exportFunc();
  const instance1 = builder1.instantiate();

  const builder2 = new WasmModuleBuilder();
  builder2.addImport('mod', 'fn', kSig_v_v);
  builder2.instantiate({mod: {fn: instance1.exports.func}});
})();

(function testWasmToWasmWithDifferentMemory() {
  print(arguments.callee.name);
  const builder1 = new WasmModuleBuilder();
  builder1.addMemory(1, 1, true);
  builder1.addFunction('store', kSig_v_i)
      .addBody([
        kExprI32Const, 0,        // i32.const 1
        kExprGetLocal, 0,        // get_local 0
        kExprI32StoreMem, 0, 0,  // i32.store offset=0 align=0
      ])
      .exportFunc();
  const instance1 = builder1.instantiate();
  const mem1 = new Int32Array(instance1.exports.memory.buffer);

  const builder2 = new WasmModuleBuilder();
  builder2.addMemory(1, 1, true);
  builder2.addImport('mod', 'store', kSig_v_i);
  builder2.addFunction('call_store', kSig_v_i)
      .addBody([kExprGetLocal, 0, kExprCallFunction, 0])
      .exportFunc();
  const instance2 = builder2.instantiate({mod: {store: instance1.exports.store}});
  const mem2 = new Int32Array(instance2.exports.memory.buffer);

  assertEquals(0, mem1[0]);
  assertEquals(0, mem2[0]);
  instance2.exports.call_store(3);
  assertEquals(3, mem1[0]);
  assertEquals(0, mem2[0]);
  %FreezeWasmLazyCompilation(instance1);
  %FreezeWasmLazyCompilation(instance2);
  instance2.exports.call_store(7);
  assertEquals(7, mem1[0]);
})();

(function exportImportedFunction() {
  print(arguments.callee.name);
  const builder1 = new WasmModuleBuilder();
  builder1.addFunction('foo', kSig_v_v).addBody([]).exportAs('foo');
  const instance1 = builder1.instantiate();

  const builder2 = new WasmModuleBuilder();
  const imp_idx = builder2.addImport('A', 'foo', kSig_v_v);
  builder2.addExport('foo', imp_idx);
  const instance2 = builder2.instantiate({A: instance1.exports});

  instance2.exports.foo();
  %FreezeWasmLazyCompilation(instance1);
  %FreezeWasmLazyCompilation(instance2);
  instance2.exports.foo();
})();

(function exportImportedFunctionWithDifferentMemory() {
  print(arguments.callee.name);
  const builder1 = new WasmModuleBuilder();
  builder1.addMemory(1, 1, true);
  builder1.addFunction('store', kSig_v_i)
      .addBody([
        kExprI32Const, 0,        // i32.const 1
        kExprGetLocal, 0,        // get_local 0
        kExprI32StoreMem, 0, 0,  // i32.store offset=0 align=0
      ])
      .exportFunc();
  const instance1 = builder1.instantiate();
  const mem1 = new Int32Array(instance1.exports.memory.buffer);

  const builder2 = new WasmModuleBuilder();
  builder2.addMemory(1, 1, true);
  const imp_idx = builder2.addImport('A', 'store', kSig_v_i);
  builder2.addExport('exp_store', imp_idx);
  const instance2 = builder2.instantiate({A: instance1.exports});
  const mem2 = new Int32Array(instance2.exports.memory.buffer);

  instance2.exports.exp_store(3);
  assertEquals(3, mem1[0]);
  assertEquals(0, mem2[0]);
  %FreezeWasmLazyCompilation(instance1);
  %FreezeWasmLazyCompilation(instance2);
  instance2.exports.exp_store(7);
  assertEquals(7, mem1[0]);
})();
