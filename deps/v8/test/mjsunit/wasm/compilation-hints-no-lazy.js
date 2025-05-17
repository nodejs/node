// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --no-wasm-lazy-compilation
// Flags: --wasm-deopt --liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// This test case tests the combination of using eager compilation hints with
// the --no-wasm-lazy-compilation flag. In this case the compilation hint set
// the required_baseline_tier to Turbofan and the required_top_tier to Liftoff
// which doesn't make a whole lot of sense.
(function testDecodeCompilationHintsSectionTopTierDefault() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('sq', kSig_i_i)
         .addBody([kExprLocalGet, 0,
                   kExprLocalGet, 0,
                   kExprI32Mul])
         .setCompilationHint(kCompilationHintStrategyEager,
                             kCompilationHintTierOptimized,
                             kCompilationHintTierDefault)
         .exportFunc();
  let instance = builder.instantiate();
  assertEquals(9, instance.exports.sq(3))
})();
