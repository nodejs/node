// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

if (!InspectorTest.load) InspectorTest.load = utils.load.bind(utils);
InspectorTest.load('test/mjsunit/wasm/wasm-module-builder.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests wasm-breakpoint-memoryestimation-concurrency.');

const builder = new WasmModuleBuilder();
const body_f = [kExprI32Const, 123];
const offset_in_body = body_f.indexOf(kExprI32Const);
const func_f = builder.addFunction('f', kSig_i_v).addBody(body_f).exportFunc();
const module_bytes = builder.toArray();

(async function test() {
  await Protocol.Debugger.enable();

  const instantiatePromise = Protocol.Runtime.evaluate({
    expression: `
      const bytes = new Uint8Array(${JSON.stringify(module_bytes)});
      const module = new WebAssembly.Module(bytes);
      const instance = new WebAssembly.Instance(module);
    `,
  });

  const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);
  await instantiatePromise;

  const breakpoint = await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId: wasmScript.scriptId,
      lineNumber: 0,
      columnNumber: func_f.body_offset + offset_in_body,
    },
  });
  InspectorTest.log(
      'Breakpoint set: ' + (breakpoint.result ? 'success' : 'failed'));

  await Protocol.Runtime.evaluate({
    expression: '%EstimateCurrentMemoryConsumption()',
  });
  InspectorTest.log('Memory estimation finished.');

  InspectorTest.completeTest();
})().catch(e => {
  InspectorTest.log('Uncaught: ' + e);
  InspectorTest.completeTest();
});
