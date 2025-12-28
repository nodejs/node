// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --trace-wasm-inlining
// Flags: --allow-natives-syntax --liftoff

// TODO(manoskouk): Remove the busy-waits from this test.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

(function TestInliningDirectCall() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprLocalGet, 1, kExprCallFunction, add.index])
    .exportFunc();

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(caller.index, [{offset: 8, frequency: 64}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(2 * 20 + 10, wasm.caller(20, 10));
})();

(function TestInliningIndirectCalls() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let intermediate = builder.addFunction("intermediate", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprLocalGet, 0, kExprRefFunc, add.index,
              kExprCallRef, add.type_index])

  let table = builder.addTable(kWasmFuncRef, 10, 10);
  let caller = builder.addFunction("caller", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add,
              kExprI32Const, 0,
              kExprCallIndirect, intermediate.type_index, table.index])
    .exportFunc();

  builder.addDeclarativeElementSegment([add.index]);
  builder.addActiveElementSegment(table.index, [kExprI32Const, 0],
                                  [intermediate.index]);

  builder.setCompilationPriority(caller.index, 0, 0)

  builder.setInstructionFrequencies(
      intermediate.index, [{offset:10, frequency: 32}])
  builder.setCallTargets(
      intermediate.index, [{offset: 10, targets: [
          {function_index: add.index, frequency_percent: 99}]}]);
  builder.setInstructionFrequencies(
      caller.index, [{offset:8, frequency: 40}]);
  builder.setCallTargets(
      caller.index, [{offset:8, targets: [
          {function_index: intermediate.index, frequency_percent: 100}]}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(2 * (10 + 1) + (10 + 1), wasm.caller(10));
})();

(function TestInliningDirectCallNoOptimizationHint() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprLocalGet, 1, kExprCallFunction, add.index])
    .exportFunc();

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(caller.index, [{offset: 8, frequency: 0}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(2 * 20 + 10, wasm.caller(20, 10));
})();

(function TestInliningDirectCallAlwaysOptimizeHint() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprLocalGet, 1, kExprCallFunction, add.index])
    .exportFunc();

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 8, frequency: 127}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(2 * 20 + 10, wasm.caller(20, 10));
})();

(function TestInliningDirectCallWrongOffset() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprLocalGet, 1, kExprCallFunction, add.index])
    .exportFunc();

  builder.setCompilationPriority(caller.index, 0, 0);

  // `7` is wrong; does not take locals into account. No inlining should take
  // place.
  builder.setInstructionFrequencies(caller.index, [{offset: 7, frequency: 64}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(2 * 20 + 10, wasm.caller(20, 10));
})();

(function TestInliningDirectWithLocals() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_ii)
    .addLocals(kWasmI32, 1)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul, kExprLocalSet, 2,
              kExprLocalGet, 2, kExprLocalGet, 1, kExprCallFunction, add.index])
    .exportFunc();

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 14, frequency: 32}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(2 * 20 + 10, wasm.caller(20, 10));
})();

(function TestCallRefMultipleSitesSomeWithoutCallTargets() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let inc = builder.addFunction("inc", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])

  let caller = builder.addFunction("caller", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprRefFunc, inc.index,
              kExprCallRef, inc.type_index,
              kExprLocalGet, 0, kExprRefFunc, inc.index,
              kExprCallRef, inc.type_index,
              kExprI32Mul,
              kExprLocalGet, 0, kExprRefFunc, inc.index,
              kExprCallRef, inc.type_index,
              kExprI32Sub])
    .exportFunc();
  builder.addDeclarativeElementSegment([inc.index]);

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 5, frequency: 32},
                   {offset: 11, frequency: 32},
                   {offset: 18, frequency: 32}]);

  builder.setCallTargets(caller.index,
    [{offset: 5,
      targets: [{function_index: inc.index, frequency_percent: 100}]},
     {offset: 18,
      targets: [{function_index: inc.index, frequency_percent: 100}]}]);

  let wasm = builder.instantiate({}).exports;

  while (%IsLiftoffFunction(wasm.caller)) {}

  assertEquals((10 + 1) * (10 + 1) - (10 + 1), wasm.caller(10));
})();

(function TestHintIndicesOutOfBounds() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let imp = builder.addImport("m", "imp", kSig_i_i);

  let caller = builder.addFunction("caller", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprRefFunc, imp, kExprCallRef, 0])
    .exportFunc();

  builder.addDeclarativeElementSegment([imp]);

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 5, frequency: 32}]);

  builder.setCallTargets(caller.index,
    [{offset: 5,
      targets: [{function_index: imp, frequency_percent: 50},
                {function_index: caller.index + 1, frequency_percent: 50}]}]);

  let wasm = builder.instantiate({m: {imp: (x) => x + 1}}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(11, wasm.caller(10));
})();

(function TestCallRefHintWithWrongSignature() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let inc = builder.addFunction("inc", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])

  let wrong_sig = builder.addFunction("wrong_sig", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprRefFunc, inc.index, kExprCallRef, inc.type_index])
    .exportFunc();

  builder.addDeclarativeElementSegment([inc.index]);

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 8, frequency: 32}]);

  builder.setCallTargets(caller.index,
    [{offset: 8,
      targets: [{function_index: wrong_sig.index, frequency_percent: 100}]}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(21, wasm.caller(10));
})();

(function TestReturnCallRefHintWithWrongSignature() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let inc = builder.addFunction("inc", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])

  let wrong_sig = builder.addFunction("wrong_sig", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprRefFunc, inc.index, kExprReturnCallRef, inc.type_index])
    .exportFunc();

  builder.addDeclarativeElementSegment([inc.index]);

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 8, frequency: 32}]);

  builder.setCallTargets(caller.index,
    [{offset: 8,
      targets: [{function_index: wrong_sig.index, frequency_percent: 100}]}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(21, wasm.caller(10));
})();

(function TestCallRefHintWithWrongAndRightSignatures() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let inc = builder.addFunction("inc", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])

  let wrong_sig = builder.addFunction("wrong_sig", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprRefFunc, inc.index, kExprCallRef, inc.type_index])
    .exportFunc();

  builder.addDeclarativeElementSegment([inc.index]);

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 8, frequency: 32}]);

  builder.setCallTargets(caller.index,
    [{offset: 8,
      targets: [{function_index: wrong_sig.index, frequency_percent: 50},
                {function_index: inc.index, frequency_percent: 50}]}]);

  let wasm = builder.instantiate({}).exports;

  while(%IsLiftoffFunction(wasm.caller)) {}

  assertEquals(21, wasm.caller(10));
})();
