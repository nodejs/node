// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-compilation-hints --wasm-wasmfx --liftoff
// Flags: --wasm-dynamic-tiering --wasm-tiering-budget=5 --no-predictable
// Flags: --allow-natives-syntax --wasm-deopt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function HintsAndDeopts() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let sig = builder.addType(kSig_i_ii);

  let add = builder.addFunction("add", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let sub = builder.addFunction("sub", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Sub]);

  let intermediate = builder.addFunction("intermediate", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprRefFunc, add.index,
              kExprCallRef, sig]);

  let caller = builder.addFunction("caller", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kExprRefFunc, intermediate.index, kExprCallRef, sig])
    .exportFunc();

  builder.addDeclarativeElementSegment([
    add.index, sub.index, intermediate.index]);

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    intermediate.index, [{offset: 7, frequency: 32}]);
  builder.setCallTargets(intermediate.index,
    [{offset: 7,
      targets: [
        {function_index: sub.index, frequency_percent: 100}]}]);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 7, frequency: 32}]);
  builder.setCallTargets(caller.index,
    [{offset: 7,
      targets: [
        {function_index: intermediate.index, frequency_percent: 100}]}]);

  let wasm = builder.instantiate().exports;

  while (!%IsTurboFanFunction(wasm.caller)) {}

  assertEquals(15, wasm.caller(10, 5));

  // Deopt happened.
  assertTrue(%IsLiftoffFunction(wasm.caller));

  while (%IsLiftoffFunction(wasm.caller)) {
    assertEquals(15, wasm.caller(10, 5));
  }

  // Recompiled with turbofan with dynamic tiering.
  assertTrue(%IsTurboFanFunction(wasm.caller));

  assertEquals(15, wasm.caller(10, 5));

  // Recompiled with feedback this time; no deopt loops.
  assertTrue(%IsTurboFanFunction(wasm.caller));
})();

// This test counts on cont.new not being implemented in Liftoff.
(function HintsAndDeoptsLiftoffBailout() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let sig = builder.addType(kSig_i_ii);

  let cont = builder.addCont(sig);

  let add = builder.addFunction("add", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let sub = builder.addFunction("sub", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Sub]);

  let intermediate = builder.addFunction("intermediate", sig)
    .addBody([kExprRefFunc, add.index, kExprContNew, cont, kExprDrop,
              kExprLocalGet, 0, kExprLocalGet, 1, kExprRefFunc, add.index,
              kExprCallRef, sig]);

  let caller = builder.addFunction("caller", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kExprRefFunc, intermediate.index, kExprCallRef, sig])
    .exportFunc();

  builder.addDeclarativeElementSegment([
    add.index, sub.index, intermediate.index]);

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    intermediate.index, [{offset: 12, frequency: 32}]);
  builder.setCallTargets(intermediate.index,
    [{offset: 12,
      targets: [
        {function_index: sub.index, frequency_percent: 100}]}]);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 7, frequency: 32}]);
  builder.setCallTargets(caller.index,
    [{offset: 7,
      targets: [
        {function_index: intermediate.index, frequency_percent: 100}]}]);

  let wasm = builder.instantiate().exports;

  while (!%IsTurboFanFunction(wasm.caller)) {}

  assertEquals(15, wasm.caller(10, 5));

  // Deopt did not happen: Since Liftoff compilation failed for intermediate,
  // we bailed out from deopts.
  assertTrue(%IsTurboFanFunction(wasm.caller));
})();

(function HintsAndDeoptsLiftoffBailoutInTailCaller() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let sig = builder.addType(kSig_i_ii);

  let cont = builder.addCont(sig);

  let add = builder.addFunction("add", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let sub = builder.addFunction("sub", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Sub]);

  let intermediate = builder.addFunction("intermediate", sig)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprRefFunc, add.index,
              kExprCallRef, sig]);

  let caller = builder.addFunction("caller", sig)
    .addBody([kExprRefFunc, add.index, kExprContNew, cont, kExprDrop,
              kExprLocalGet, 0, kExprLocalGet, 1,
              kExprRefFunc, intermediate.index, kExprReturnCallRef, sig])
    .exportFunc();

  builder.addDeclarativeElementSegment([
    add.index, sub.index, intermediate.index]);

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(
    intermediate.index, [{offset: 7, frequency: 32}]);
  builder.setCallTargets(intermediate.index,
    [{offset: 7,
      targets: [
        {function_index: sub.index, frequency_percent: 100}]}]);

  builder.setInstructionFrequencies(
    caller.index, [{offset: 12, frequency: 32}]);
  builder.setCallTargets(caller.index,
    [{offset: 12,
      targets: [
        {function_index: intermediate.index, frequency_percent: 100}]}]);

  let wasm = builder.instantiate().exports;

  while (!%IsTurboFanFunction(wasm.caller)) {}

  assertEquals(15, wasm.caller(10, 5));

  assertTrue(%IsTurboFanFunction(wasm.caller));
})();
