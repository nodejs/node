// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints

load('test/mjsunit/wasm/wasm-module-builder.js');

(function testCompileWithBadLazyHint() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_i)
         .addBody([kExprGetLocal, 0])
         .giveCompilationHint(kCompilationHintStrategyLazy,
                              kCompilationHintTierOptimized,
                              kCompilationHintTierBaseline)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.compile(bytes)
    .then(assertUnreachable,
          error => assertEquals("WebAssembly.compile(): Invalid compilation " +
          "hint 0x2d (forbidden downgrade) @+49", error.message)));
})();

(function testCompileWithBadLazyFunctionBody() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_l)
         .addBody([kExprGetLocal, 0])
         .giveCompilationHint(kCompilationHintStrategyLazy,
                              kCompilationHintTierDefault,
                              kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.compile(bytes)
    .then(assertUnreachable,
          error => assertEquals("WebAssembly.compile(): type error in " +
          "merge[0] (expected i32, got i64) @+56", error.message)));
})();

(function testCompileEmptyModule() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.compile(bytes));
})();

(function testCompileLazyModule() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_i)
         .addBody([kExprGetLocal, 0])
         .giveCompilationHint(kCompilationHintStrategyLazy,
                              kCompilationHintTierDefault,
                              kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiate(bytes)
    .then(({module, instance}) => assertEquals(42, instance.exports.id(42))));
})();
