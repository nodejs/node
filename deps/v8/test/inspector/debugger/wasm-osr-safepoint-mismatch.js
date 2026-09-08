// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff --no-wasm-tier-up --no-debug-code

utils.load('test/inspector/protocol-test.js');
utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Verifies OSR safepoint consistency on ARM64');

var builder = new WasmModuleBuilder();
// 26 externref parameters to force register pressure on ARM64.
// Liftoff uses 24 GP registers for caching. The kWithBreakpoints path
// performs a function-entry break check which requests a register.
// If all registers are full, this previously caused a spill which was
// absent in the kForStepping path.
// This test verifies that both paths now have consistent spills and
// GC maps.
let num_params = 26;
let sig = {params: Array(num_params).fill(kWasmExternRef), results: []};
builder.addFunction('main', sig).addBody([kExprNop, kExprNop]).exportAs('main');

var module_bytes = builder.toArray();

Protocol.Debugger.enable();
Protocol.Runtime.enable();

InspectorTest.runAsyncTestSuite([async function test() {
  WasmInspectorTest.instantiate(module_bytes, 'instance');
  const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);

  // Set breakpoint on the second instruction.
  await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId: wasmScript.scriptId,
      lineNumber: 0,
      columnNumber: builder.functions[0].body_offset + 1
    }
  });

  InspectorTest.log('Calling main...');
  // Trigger kWithBreakpoints code.
  Protocol.Runtime.evaluate(
      {expression: `instance.exports.main(...Array(${num_params}).fill({}))`});

  await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused at breakpoint');

  InspectorTest.log('Stepping into (triggers OSR to kForStepping)...');
  // On architectures like ARM64 where we patch the return address, OSR
  // requires consistent GC maps. This StepInto will trigger the OSR
  // validation logic in UpdateReturnAddress.
  Protocol.Debugger.stepInto();

  await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused after stepInto');
  InspectorTest.completeTest();
}]);
