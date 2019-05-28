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
  assertThrows(() => builder.toModule(),
    WebAssembly.CompileError,
    "WebAssembly.Module(): Invalid compilation hint 0x2d " +
    "(forbidden downgrade) @+49");
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
  assertThrows(() => builder.toModule(),
    WebAssembly.CompileError,
    "WebAssembly.Module(): Compiling function #0:\"id\" failed: type error " +
    "in merge[0] (expected i32, got i64) @+56");
})();

(function testCompileEmptyModule() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.toModule();
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
  assertEquals(42, builder.instantiate().exports.id(42));
})();
