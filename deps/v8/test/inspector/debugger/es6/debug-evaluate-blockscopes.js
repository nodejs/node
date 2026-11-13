// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test debug evaluation for functions without local context, but with ' +
    'nested catch contexts.');

const scriptSource = `
"use strict";

var x;

function f() {
  {                   // Line 1
    let i = 1;        // Line 2
    try {             // Line 3
      throw 'stuff';  // Line 4
    } catch (e) {     // Line 5
      x = 2;          // Line 6
    }
  }
}

function f1() {
  {
    let i = 1;
    debugger;
    if (i !== 2) throw new Error('Expected 2, got ' + i);
  }
}

function f2() {
  {
    let i = 1;
    debugger;
    if (i !== 2) throw new Error('Expected 2, got ' + i);
    return function() { return i++; }
  }
}
`;

contextGroup.addScript(scriptSource, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  InspectorTest.log('--- Test f with breakpoint on line 6 ---');
  const bp = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 11  // line 6 of f: "x = 2;"
  });

  let pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: 'f()'});
  let {params: {callFrames}} = await pausedPromise;

  let {result: {result: evalResult}} =
      await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: callFrames[0].callFrameId, expression: 'i'});
  InspectorTest.log(`Evaluated i in catch block: ${evalResult.value}`);
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bp.result.breakpointId});
  await Protocol.Debugger.resume();

  async function handleF(funcName) {
    InspectorTest.log(`--- Test ${funcName} with setVariableValue ---`);
    pausedPromise = Protocol.Debugger.oncePaused();
    let evalPromise = Protocol.Runtime.evaluate({expression: `${funcName}()`});
    ({params: {callFrames}} = await pausedPromise);

    let {result: {result: val}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: callFrames[0].callFrameId, expression: 'i'});
    InspectorTest.log(`Evaluated i before set: ${val.value}`);

    let {result: {result: props}} = await Protocol.Runtime.getProperties({
      objectId: callFrames[0].scopeChain[0].object.objectId,
      ownProperties: true
    });
    let propI = props.find(p => p.name === 'i');
    InspectorTest.log(`Scope i property value: ${propI.value.value}`);

    await Protocol.Debugger.setVariableValue({
      callFrameId: callFrames[0].callFrameId,
      scopeNumber: 0,
      variableName: 'i',
      newValue: {value: 2}
    });

    await Protocol.Debugger.resume();
    let evalResponse = await evalPromise;
    let ok = !evalResponse.result.exceptionDetails;
    InspectorTest.log(`Executed ${funcName} without exception: ${ok}`);
  }

  await handleF('f1');
  await handleF('f2');

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
