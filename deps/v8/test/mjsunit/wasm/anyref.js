// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-anyref --expose-gc

load("test/mjsunit/wasm/wasm-constants.js");
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
