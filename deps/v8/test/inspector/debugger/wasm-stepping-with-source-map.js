// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping through wasm scripts with source maps');
session.setupScriptMap();

var builder = new WasmModuleBuilder();

var func_a_idx =
    builder.addFunction('wasm_A', kSig_v_v).addBody([kExprNop, kExprNop]).index;

// wasm_B calls wasm_A <param0> times.
builder.addFunction('wasm_B', kSig_v_i)
    .addBody([
      // clang-format off
      kExprLoop, kWasmVoid,               // while
        kExprLocalGet, 0,                 // -
        kExprIf, kWasmVoid,               // if <param0> != 0
          kExprLocalGet, 0,               // -
          kExprI32Const, 1,               // -
          kExprI32Sub,                    // -
          kExprLocalSet, 0,               // decrease <param0>
          kExprCallFunction, func_a_idx,  // -
          kExprBr, 1,                     // continue
          kExprEnd,                       // -
        kExprEnd,                         // break
      // clang-format on
    ])
    .exportAs('main');

builder.addCustomSection('sourceMappingURL', [3, 97, 98, 99]);

var module_bytes = builder.toArray();

InspectorTest.runAsyncTestSuite([
  async function test() {
    for (const action of ['stepInto', 'stepOver', 'stepOut', 'resume'])
      InspectorTest.logProtocolCommandCalls('Debugger.' + action);

    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    WasmInspectorTest.instantiate(module_bytes);
    const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);

    InspectorTest.log('Got wasm script: ' + wasmScript.url);
    InspectorTest.log('Script sourceMapURL: ' + wasmScript.sourceMapURL);
    InspectorTest.log('Requesting source for ' + wasmScript.url + '...');
    const msg =
        await Protocol.Debugger.getScriptSource({scriptId: wasmScript.scriptId});
    InspectorTest.log(`Source retrieved without error: ${!msg.error}`);
    InspectorTest.log(
        `Setting breakpoint on offset 54 (on the setlocal before the call), url ${wasmScript.url}`);
    const {result: {actualLocation}} = await Protocol.Debugger.setBreakpoint({
      location:{scriptId: wasmScript.scriptId, lineNumber: 0, columnNumber: 54}});
    InspectorTest.logMessage(actualLocation);
    Protocol.Runtime.evaluate({expression: 'instance.exports.main(4)'});
    await waitForPauseAndStep('stepInto');  // == stepOver, to call instruction
    await waitForPauseAndStep('stepInto');  // into call to wasm_A
    await waitForPauseAndStep('stepOver');  // over first nop
    await waitForPauseAndStep('stepOut');   // out of wasm_A
    await waitForPauseAndStep('stepOut');  // out of wasm_B, stop on breakpoint again
    await waitForPauseAndStep('stepOver');  // to call
    await waitForPauseAndStep('stepOver');  // over call
    await waitForPauseAndStep('resume');  // to next breakpoint (third iteration)
    await waitForPauseAndStep('stepInto');  // to call
    await waitForPauseAndStep('stepInto');  // into wasm_A
    await waitForPauseAndStep('stepOut');   // out to wasm_B
    // Now step 8 times, until we are in wasm_A again.
    for (let i = 0; i < 8; ++i) await waitForPauseAndStep('stepInto');
    // 3 more times, back to wasm_B.
    for (let i = 0; i < 3; ++i) await waitForPauseAndStep('stepInto');
    // then just resume.
    await waitForPauseAndStep('resume');
    InspectorTest.log('exports.main returned!');
  }
]);

async function waitForPauseAndStep(stepAction) {
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(callFrames[0].location);
  for (var frame of callFrames) {
    const functionName = frame.functionName || '(anonymous)';
    const lineNumber = frame.location.lineNumber;
    const columnNumber = frame.location.columnNumber;
    InspectorTest.log(`at ${functionName} (${lineNumber}:${columnNumber}):`);
    for (var scope of frame.scopeChain) {
      InspectorTest.logObject(' - scope (' + scope.type + '):');
      if (scope.type === 'global' || scope.type === 'module') {
        InspectorTest.logObject('   -- skipped');
      } else {
        var { objectId } = scope.object;
        if (scope.type == 'wasm-expression-stack') {
          objectId = (await Protocol.Runtime.callFunctionOn({
            functionDeclaration: 'function() { return this.stack }',
            objectId
          })).result.result.objectId;
        }
        let properties = await Protocol.Runtime.getProperties(
            {objectId});
        for (let {name, value} of properties.result.result) {
          value = await WasmInspectorTest.getWasmValue(value);
          InspectorTest.log(`   ${name}: ${value}`);
        }
      }
    }
  }
  Protocol.Debugger[stepAction]();
}
