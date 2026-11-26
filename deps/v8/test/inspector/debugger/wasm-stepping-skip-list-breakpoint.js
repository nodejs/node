// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that stepping with a skipList does not skip breakpoints');
session.setupScriptMap();

const builder = new WasmModuleBuilder();

const func = builder.addFunction('main', kSig_i_v)
                 .addBody([
                   kExprI32Const,
                   11,
                   kExprI32Const,
                   3,
                   kExprI32Add,
                 ])
                 .exportAs('main');

const module_bytes = builder.toArray();

function createLocationRange(scriptId, startColumn, endColumn) {
  return {
    scriptId: scriptId,
    start: {lineNumber: 0, columnNumber: startColumn},
    end: {lineNumber: 0, columnNumber: endColumn}
  };
}

InspectorTest.runAsyncTestSuite([async function test() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  WasmInspectorTest.instantiate(module_bytes);
  const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);
  const scriptId = wasmScript.scriptId;

  const bp1_offset = func.body_offset + 1;
  const bp2_offset = func.body_offset + 3;

  InspectorTest.log('Setting breakpoint 1 at offset ' + bp1_offset);
  const bp1 = await Protocol.Debugger.setBreakpoint(
      {location: {scriptId, lineNumber: 0, columnNumber: bp1_offset}});

  InspectorTest.log('Setting breakpoint 2 at offset ' + bp2_offset);
  const bp2 = await Protocol.Debugger.setBreakpoint(
      {location: {scriptId, lineNumber: 0, columnNumber: bp2_offset}});

  Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});

  const paused1 = await Protocol.Debugger.oncePaused();
  InspectorTest.log(
      'Paused at: ' + paused1.params.callFrames[0].location.columnNumber);
  InspectorTest.log(
      'Hit breakpoint 1: ' +
      paused1.params.hitBreakpoints.includes(bp1.result.breakpointId));

  InspectorTest.log('Stepping with skipList covering breakpoint 2');
  const skipList = [createLocationRange(scriptId, bp2_offset, bp2_offset + 2)];
  Protocol.Debugger.stepOver({skipList});

  const paused2 = await Protocol.Debugger.oncePaused();
  InspectorTest.log(
      'Paused at: ' + paused2.params.callFrames[0].location.columnNumber);
  InspectorTest.log(
      'Hit breakpoint 2: ' +
      paused2.params.hitBreakpoints.includes(bp2.result.breakpointId));

  await Protocol.Debugger.resume();
}]);
