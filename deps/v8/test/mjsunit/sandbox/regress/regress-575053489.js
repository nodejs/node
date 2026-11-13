// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-memory-corruption-api --allow-natives-syntax
// Flags: --turbolev --turbofan --no-optimize-on-next-call-optimizes-to-maglev

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builtins = Sandbox.getBuiltinNames();
const kDebugBreakTrampoline = builtins.indexOf('DebugBreakTrampoline');
const kJSToWasmWrapper = builtins.indexOf('JSToWasmWrapper');

const builder = new WasmModuleBuilder();
builder.addFunction('exp', kSig_i_i).addBody([kExprLocalGet, 0]).exportFunc();
const instance = builder.instantiate();
const exp = instance.exports.exp;

function caller(run, x) {
  if (run) return exp(x);
  return 0;
}

%PrepareFunctionForOptimization(caller);
caller(true, 1);
caller(true, 2);
caller(false, 0);

if (%IsConcurrentRecompilationSupported()) {
  // Install a non-wrapper builtin in exp's JSDispatchTable entry so Maglev
  // graph building skips wrapping the call argument in ProcessWasmArgument.
  Sandbox.setFunctionCodeToBuiltin(exp, kDebugBreakTrampoline);

  // Pause background compilation right after the Maglev frontend finishes
  // and before TurbolevGraphBuilder runs.
  %BlockAt('TurboshaftTurbolevGraphBuilding', 10000);
  %OptimizeFunctionOnNextCall(caller, 'concurrent');
  assertEquals(0, caller(false, 3));

  assertTrue(%WaitUntilBlocked('TurboshaftTurbolevGraphBuilding', 10000));

  // Restore JSToWasmWrapper on the same JSDispatchTable entry before
  // TurbolevGraphBuilder lowers the CallKnownJSFunction node.
  Sandbox.setFunctionCodeToBuiltin(exp, kJSToWasmWrapper);

  assertTrue(%Resume('TurboshaftTurbolevGraphBuilding'));
  %WaitForBackgroundOptimization();
  assertEquals(5, caller(true, 5));
}
