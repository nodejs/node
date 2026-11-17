// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-code-coverage

// Pausing the debugger while executing Wasm triggers regeneration of the
// Liftoff debug side table. With --wasm-code-coverage, the regenerated code
// must include coverage instrumentation so its PC offsets match the original
// code. Ensure frame inspection does not crash.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/526163463.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
const builder = new WasmModuleBuilder();
const setupIdx = builder.addImport('env', 'setup', kSig_v_v);
builder.addFunction('vuln', kSig_v_v)
    .addBody([
      kExprCallFunction, setupIdx,
      kExprLoop, kWasmVoid,
      kExprEnd,
    ])
    .exportFunc();

function setup() {
  console.log('setup');
  inspector.runNestedMessageLoop();
}

const instance = builder.instantiate({env: {setup}});
`);

Protocol.Debugger.onPaused(() => Protocol.Debugger.resume());

(async () => {
  await Protocol.Runtime.enable();

  const consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  const evalPromise =
      Protocol.Runtime.evaluate({expression: 'instance.exports.vuln()'});

  await consolePromise;
  await Protocol.Debugger.enable();
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  await evalPromise;
  InspectorTest.completeTest();
})();
