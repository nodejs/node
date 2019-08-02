// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --wasm-test-streaming

load('test/mjsunit/wasm/wasm-module-builder.js');

(function testInstantiateStreamingWithLazyHint() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow2', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierDefault,
                             kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiateStreaming(Promise.resolve(bytes),
                                                       {mod: {pow: Math.pow}})
    .then(({module, instance}) => assertEquals(27, instance.exports.upow2(3))));
})();

(function testInstantiateStreamingWithBadLazyHint() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow2', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierOptimized,
                             kCompilationHintTierBaseline)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiateStreaming(Promise.resolve(bytes),
                                                       {mod: {pow: Math.pow}})
    .then(assertUnreachable,
          error => assertEquals("WebAssembly.instantiateStreaming(): Invalid " +
                                "compilation hint 0x2d (forbidden downgrade) " +
                                "@+78",
                                error.message)));
})();

(function testInstantiateStreamingWithBadLazyFunctionBody() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_f_ff);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
  builder.addFunction('upow2', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierDefault,
                             kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiateStreaming(Promise.resolve(bytes),
                                                       {mod: {pow: Math.pow}})
    .then(assertUnreachable,
          error => assertEquals("WebAssembly.instantiateStreaming(): call[1] " +
                                "expected type f32, found local.get of type " +
                                "i32 @+94",
                                error.message)));
})();

(function testInstantiateStreamingEmptyModule() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiateStreaming(Promise.resolve(bytes),
                                                       {mod: {pow: Math.pow}}));
})();

(function testInstantiateStreamingLazyModule() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyLazy,
                             kCompilationHintTierDefault,
                             kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiateStreaming(Promise.resolve(bytes),
                                                       {mod: {pow: Math.pow}})
    .then(({module, instance}) => assertEquals(27, instance.exports.upow(3))));
})();

(function testInstantiateStreamingLazyBaselineModule() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('mod', 'pow', kSig_i_ii);
  builder.addFunction('upow', kSig_i_i)
         .addBody([kExprGetLocal, 0,
                   kExprGetLocal, 0,
                   kExprCallFunction, 0])
         .setCompilationHint(kCompilationHintStrategyLazyBaselineEagerTopTier,
                             kCompilationHintTierDefault,
                             kCompilationHintTierDefault)
         .exportFunc();
  let bytes = builder.toBuffer();
  assertPromiseResult(WebAssembly.instantiateStreaming(Promise.resolve(bytes),
                                                       {mod: {pow: Math.pow}})
    .then(({module, instance}) => assertEquals(27, instance.exports.upow(3))));
})();
