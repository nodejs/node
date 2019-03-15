// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-anyref --expose-gc

load("test/mjsunit/wasm/wasm-module-builder.js");

(function testAnyRefIdentityFunction() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_r_r)
      .addBody([kExprGetLocal, 0])
      .exportFunc();


  const instance = builder.instantiate();

  let obj = {'hello' : 'world'};
  assertEquals(obj, instance.exports.main(obj));
  assertEquals(1234, instance.exports.main(1234));
  assertEquals(123.4, instance.exports.main(123.4));
  assertEquals(undefined, instance.exports.main(undefined));
  assertEquals(null, instance.exports.main(null));
  assertEquals(print, instance.exports.main(print));
})();

(function testPassAnyRefToImportedFunction() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_v_r);
  const imp_index = builder.addImport("q", "func", sig_index);
  builder.addFunction('main', sig_index)
      .addBody([kExprGetLocal, 0,
      kExprCallFunction, imp_index])
      .exportFunc();

  function checkFunction(value) {
    assertEquals('world', value.hello);
  }

  const instance = builder.instantiate({q: {func: checkFunction}});

  instance.exports.main({hello: 'world'});
})();

(function testPassAnyRefWithGCWithLocals() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const ref_sig = builder.addType(kSig_v_r);
  const void_sig = builder.addType(kSig_v_v);
  const imp_index = builder.addImport("q", "func", ref_sig);
  const gc_index = builder.addImport("q", "gc", void_sig);
  // First call the gc, then check if the object still exists.
  builder.addFunction('main', ref_sig)
      .addLocals({anyref_count: 10})
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

  function checkFunction(value) {
    assertEquals('world', value.hello);
  }

  const instance = builder.instantiate({q: {func: checkFunction, gc: gc}});

  instance.exports.main({hello: 'world'});
})();

(function testPassAnyRefWithGC() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const ref_sig = builder.addType(kSig_v_r);
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
    assertEquals('world', value.hello);
  }

  const instance = builder.instantiate({q: {func: checkFunction, gc: gc}});

  instance.exports.main({hello: 'world'});
})();

(function testPassAnyRefWithGCWithStackParameters() {
  print(arguments.callee.name);
  const num_params = 15;
  for (let index = 0; index < num_params; index++) {
    const builder = new WasmModuleBuilder();
    // Make a signature with {num_params} many anyref parameters.
    const mysig = makeSig(Array(num_params).fill(kWasmAnyRef), []);
    const main_sig = builder.addType(mysig);
    const ref_sig = builder.addType(kSig_v_r);
    const void_sig = builder.addType(kSig_v_v);
    const imp_index = builder.addImport('q', 'func', ref_sig);
    const gc_index = builder.addImport('q', 'gc', void_sig);
    // First call the gc, then check if the object still exists.
    builder.addFunction('main', main_sig)
        .addBody([
          kExprCallFunction, gc_index,                        // call gc
          kExprGetLocal, index, kExprCallFunction, imp_index  // call import
        ])
        .exportFunc();

    function checkFunction(value) {
      assertEquals(index, value.hello);
    }

    const instance = builder.instantiate({q: {func: checkFunction, gc: gc}});

    // Pass {num_params} many parameters to main. Note that it is important
    // that no other references to these objects exist. They are kept alive
    // only through references stored in the parameters slots of a stack frame.
    instance.exports.main(
        {hello: 0}, {hello: 1}, {hello: 2}, {hello: 3}, {hello: 4}, {hello: 5},
        {hello: 6}, {hello: 7}, {hello: 8}, {hello: 9}, {hello: 10},
        {hello: 11}, {hello: 12}, {hello: 13}, {hello: 14});
  }
})();

(function testPassAnyRefWithGCInWrapper() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const kSig_r_iri = makeSig([kWasmI32, kWasmAnyRef, kWasmI32], [kWasmAnyRef]);
  const sig_index = builder.addType(kSig_r_iri);
  builder.addFunction('main', sig_index)
      .addBody([kExprGetLocal, 1])
      .exportFunc();

  const instance = builder.instantiate();

  const triggerGCParam = {
    valueOf: () => {
      gc();
      return 17;
    }
  };

  const result = instance.exports.main(triggerGCParam, {hello: 'world'}, triggerGCParam);
  assertEquals('world', result.hello);
})();

(function testAnyRefNull() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_r_v)
      .addBody([kExprRefNull])
      .exportFunc();

  const instance = builder.instantiate();

  assertEquals(null, instance.exports.main());
})();

(function testAnyRefIsNull() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_i_r)
      .addBody([kExprGetLocal, 0, kExprRefIsNull])
      .exportFunc();

  const instance = builder.instantiate();

  assertEquals(0, instance.exports.main({'hello' : 'world'}));
  assertEquals(0, instance.exports.main(1234));
  assertEquals(0, instance.exports.main(0));
  assertEquals(0, instance.exports.main(123.4));
  assertEquals(0, instance.exports.main(undefined));
  assertEquals(1, instance.exports.main(null));
  assertEquals(0, instance.exports.main(print));
})();

(function testAnyRefNullIsNull() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_i_v)
      .addBody([kExprRefNull, kExprRefIsNull])
      .exportFunc();

  const instance = builder.instantiate();

  assertEquals(1, instance.exports.main());
})();

(function testAnyRefLocalDefaultValue() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_r_v)
      .addBody([kExprGetLocal, 0])
      .addLocals({anyref_count: 1})
      .exportFunc();

  const instance = builder.instantiate();

  assertEquals(null, instance.exports.main());
})();

(function testImplicitReturnNullRefAsAnyRef() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_r_v);
  builder.addFunction('main', sig_index)
      .addBody([kExprRefNull])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();

(function testExplicitReturnNullRefAsAnyRef() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const sig_index = builder.addType(kSig_r_v);
  builder.addFunction('main', sig_index)
      .addBody([kExprRefNull, kExprReturn])
      .exportFunc();

  const main = builder.instantiate().exports.main;
  assertEquals(null, main());
})();
