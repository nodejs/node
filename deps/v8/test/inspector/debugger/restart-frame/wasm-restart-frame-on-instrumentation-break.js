// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks restartFrame during a Wasm instrumentation break');

session.setupScriptMap();

const builder = new WasmModuleBuilder();
const imp_idx = builder.addImport('env', 'inc', kSig_v_v);
builder.addFunction('main', kSig_v_v)
    .addBody([kExprCallFunction, imp_idx])
    .exportFunc();
const wasm_bytes = JSON.stringify(builder.toArray());

contextGroup.addScript(
    `
const wasm_bytes = new Uint8Array(${wasm_bytes});
let should_restart = false;
let target_runs = 0;
let inner_completed = 0;

function runWasm() {
  const module = new WebAssembly.Module(wasm_bytes);
  const instance =
      new WebAssembly.Instance(module, {env: {inc: () => { inner_completed++; }}});
  instance.exports.main();
}

function target() {
  target_runs++;
  if (should_restart) {
    should_restart = false;
    runWasm();
  }
  return 42;
}
`,
    0, 0, 'test.js');

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  await Protocol.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});

  let should_restart_on_pause = false;
  let paused_count = 0;
  Protocol.Debugger.onPaused(async ({params}) => {
    const target_frame =
        params.callFrames.find(f => f.functionName === 'target');
    if (!target_frame) {
      await Protocol.Debugger.resume();
      return;
    }
    paused_count++;
    if (should_restart_on_pause) {
      should_restart_on_pause = false;
      await Protocol.Debugger.restartFrame(
          {callFrameId: target_frame.callFrameId, mode: 'StepInto'});
    } else {
      await Protocol.Debugger.resume();
    }
  });

  // Test restartFrame from an interpreted frame.
  should_restart_on_pause = true;
  let {result: {result}} = await Protocol.Runtime.evaluate({
    expression: `target_runs = 0; inner_completed = 0; should_restart = true;
                 [target(), target_runs, inner_completed].join(',')`
  });
  InspectorTest.log(
      `Interpreted: ${result.value}, paused_count=${paused_count}`);

  // Test restartFrame from an optimized frame (exercising lazy deopt + unwind).
  await Protocol.Runtime.evaluate({
    expression: `%PrepareFunctionForOptimization(target);
                 should_restart = false;
                 target();
                 %OptimizeFunctionOnNextCall(target);`
  });
  paused_count = 0;
  should_restart_on_pause = true;
  ({result: {result}} = await Protocol.Runtime.evaluate({
    expression: `target_runs = 0; inner_completed = 0; should_restart = true;
                 [target(), target_runs, inner_completed].join(',')`
  }));
  InspectorTest.log(
      `Optimized: ${result.value}, paused_count=${paused_count}`);

  InspectorTest.completeTest();
})();
