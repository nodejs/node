// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function testCompileWithBadLazyHint() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_i)
         .addBody([kExprLocalGet, 0])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierOptimized,
                             kCompilationHintTierBaseline)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.compile(bytes)
    .then(assertUnreachable,
          error => assertEquals("WebAssembly.compile(): Invalid compilation " +
          "hint 0x19 (forbidden downgrade) @+49", error.message)));
})();

(function testCompileWithBadLazyFunctionBody() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_l)
         .addBody([kExprLocalGet, 0])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierDefault,
                             kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.compile(bytes).then(
      assertUnreachable,
      error => assertEquals(
          'WebAssembly.compile(): Compiling function #0:"id" failed: type ' +
              'error in fallthru[0] (expected i32, got i64) @+56',
          error.message)));
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
         .addBody([kExprLocalGet, 0])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierDefault,
                             kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiate(bytes)
    .then(({module, instance}) => assertEquals(42, instance.exports.id(42))));
})();

(function testCompileLazyBaselineEagerTopTierModule() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('id', kSig_i_i)
         .addBody([kExprLocalGet, 0])
         .setCompilationHint(kCompilationHintStrategyLazyBaselineEagerTopTier,
                             kCompilationHintTierDefault,
                             kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiate(bytes)
    .then(({module, instance}) => assertEquals(42, instance.exports.id(42))));
})();
