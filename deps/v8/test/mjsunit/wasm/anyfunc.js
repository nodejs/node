// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-anyref --expose-gc

load("test/mjsunit/wasm/wasm-module-builder.js");

(function testAnyFuncIdentityFunction() {
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

(function testPassAnyFuncToImportedFunction() {
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

(function testPassAnyFuncWithGCWithLocals() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const ref_sig = builder.addType(kSig_v_a);
  const void_sig = builder.addType(kSig_v_v);
  const imp_index = builder.addImport("q", "func", ref_sig);
  const gc_index = builder.addImport("q", "gc", void_sig);
  // First call the gc, then check if the object still exists.
  builder.addFunction('main', ref_sig)
      .addLocals({anyfunc_count: 10})
      .addBody([
        kExprGetLocal, 0, kExprSetLocal, 1,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 2,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 3,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 4,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 5,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 6,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 7,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 8,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 9,             // Set local
        kExprGetLocal, 0, kExprSetLocal, 10,            // Set local
        kExprCallFunction, gc_index,                    // call gc
        kExprGetLocal, 9, kExprCallFunction, imp_index  // call import
      ])
      .exportFunc();

  const main =
      builder.instantiate({q: {func: checkFunction, gc: gc}}).exports.main;

  function checkFunction(value) {
    assertSame(main, value);
  }

  main(main);
})();

(function testPassAnyFuncWithGC() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const ref_sig = builder.addType(kSig_v_a);
  const void_sig = builder.addType(kSig_v_v);
  const imp_index = builder.addImport("q", "func", ref_sig);
  const gc_index = builder.addImport("q", "gc", void_sig);
  // First call the gc, then check if the object still exists.
  builder.addFunction('main', ref_sig)
      .addBody([
        kExprCallFunction, gc_index,                    // call gc
        kExprGetLocal, 0, kExprCallFunction, imp_index  // call import
      ])
      .exportFunc();

  function checkFunction(value) {
    assertSame(main, value);
  }

  const main = builder.instantiate({q: {func: checkFunction, gc: gc}}).exports.main;

  main(main);
})();

(function testPassAnyFuncWithGCInWrapper() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const kSig_a_iai = makeSig([kWasmI32, kWasmAnyFunc, kWasmI32], [kWasmAnyFunc]);
  const sig_index = builder.addType(kSig_a_iai);
  builder.addFunction('main', sig_index)
      .addBody([kExprGetLocal, 1])
      .exportFunc();

  const main = builder.instantiate().exports.main;

  const triggerGCParam = {
    valueOf: () => {
      gc();
      return 17;
    }
  };

  const result = main(triggerGCParam, main, triggerGCParam);
  assertSame(main, result);
})();

(function testAnyFuncDefaultValue() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_a_v);
  builder.addFunction('main', sig_index)
      .addLocals({anyfunc_count: 1})
      .addBody([kExprGetLocal, 0])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();

(function testAssignNullRefToAnyFuncLocal() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_a_a);
  builder.addFunction('main', sig_index)
      .addBody([kExprRefNull, kExprSetLocal, 0, kExprGetLocal, 0])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main(main));
})();

(function testImplicitReturnNullRefAsAnyFunc() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_a_v);
  builder.addFunction('main', sig_index)
      .addBody([kExprRefNull])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();

(function testExplicitReturnNullRefAsAnyFunc() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_a_v);
  builder.addFunction('main', sig_index)
      .addBody([kExprRefNull, kExprReturn])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();

(function testImplicitReturnAnyFuncAsAnyRef() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_r_v);
  builder.addFunction('main', sig_index)
      .addLocals({anyfunc_count: 1})
      .addBody([kExprGetLocal, 0])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();

(function testExplicitReturnAnyFuncAsAnyRef() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_r_v);
  builder.addFunction('main', sig_index)
      .addLocals({anyfunc_count: 1})
      .addBody([kExprGetLocal, 0, kExprReturn])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();
