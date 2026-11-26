// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests generator relocation.');

function prog(a, b, c) {
  return a + ';\n' +
      'yield ' + b + ';\n' +
      'yield ' + c;
}

const source = `
function prog(a, b, c) {
  return a + ';\\n' + 'yield ' + b + ';\\n' + 'yield ' + c;
}

var currentGen;
var currentObj;

function step1(formals_and_body, args) {
  currentGen = (function*(){}).constructor.apply(null, formals_and_body);
  currentObj = currentGen.apply(null, args);
  return currentObj.next();
}

function step2() {
  return currentObj.next();
}

function step3() {
  return currentObj.next();
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  let onPausedCallback = null;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    if (onPausedCallback) {
      await onPausedCallback(callFrames[0]);
    }
    await Protocol.Debugger.resume();
  });

  async function runTest(name, formals_and_body, args, value1, value2) {
    InspectorTest.log(`--- ${name} ---`);
    let res1 = await Protocol.Runtime.evaluate({
      expression:
          `step1(${JSON.stringify(formals_and_body)}, ${JSON.stringify(args)})`,
      returnByValue: true
    });
    InspectorTest.log(`step 1: ${JSON.stringify(res1.result.result.value)}`);

    await Protocol.Debugger.enable();
    await Protocol.Runtime.evaluate({expression: 'gc()'});

    let {result: {result: genObj}} =
        await Protocol.Runtime.evaluate({expression: 'currentGen'});
    let {result: {internalProperties}} =
        await Protocol.Runtime.getProperties({objectId: genObj.objectId});
    let funcLocation =
        internalProperties.find(p => p.name === '[[FunctionLocation]]');
    let scriptId = funcLocation.value.value.scriptId;

    let {result: {scriptSource}} =
        await Protocol.Debugger.getScriptSource({scriptId});
    let lines = scriptSource.split('\n');
    let yieldLines =
        lines.map((l, i) => l.includes('yield') ? i : -1).filter(i => i !== -1);
    let targetLine = yieldLines[1];

    let bp = await Protocol.Debugger.setBreakpoint(
        {location: {scriptId: scriptId, lineNumber: targetLine}});

    let paused = false;
    onPausedCallback = async (frame) => {
      paused = true;
      InspectorTest.log(`Paused at line ${frame.location.lineNumber}`);
    };

    let res2 = await Protocol.Runtime.evaluate(
        {expression: 'step2()', returnByValue: true});
    InspectorTest.log(`step 2: ${
        JSON.stringify(res2.result.result.value)}, paused: ${paused}`);

    onPausedCallback = null;
    await Protocol.Debugger.removeBreakpoint(
        {breakpointId: bp.result.breakpointId});
    await Protocol.Debugger.disable();

    let res3 = await Protocol.Runtime.evaluate(
        {expression: 'step3()', returnByValue: true});
    InspectorTest.log(`step 3: ${JSON.stringify(res3.result.result.value)}`);
  }

  await runTest('Test 1', [prog('', '1', '2')], [], 1, 2);
  await runTest('Test 2', [prog('for (;;) break', '1', '2')], [], 1, 2);
  await runTest('Test 3', [prog('while (0) foo()', '1', '2')], [], 1, 2);
  await runTest('Test 4', ['a', prog('var x = 3', 'a', 'x')], [1], 1, 3);
  await runTest('Test 5', ['a', prog('', '1', '2')], [42], 1, 2);
  await runTest('Test 6', ['a', prog('for (;;) break', '1', '2')], [42], 1, 2);

  InspectorTest.completeTest();
})();
