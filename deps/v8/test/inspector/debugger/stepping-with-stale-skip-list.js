// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that the skipList of a step does not outlive that step.');

const source = [
  'function callee() {',  // 0
  '  debugger;',          // 1
  '  let x = 1;',         // 2
  '  return x;',          // 3
  '}',                    // 4
  'function test() {',    // 5
  '  callee();',          // 6
  '  let y = 2;',         // 7
  '  return y;',          // 8
  '}',                    // 9
].join('\n');

contextGroup.addScript(source);
session.setupScriptMap();

function range(scriptId, startLine, startColumn, endLine, endColumn) {
  return {
    scriptId,
    start: {lineNumber: startLine, columnNumber: startColumn},
    end: {lineNumber: endLine, columnNumber: endColumn},
  };
}

async function logPause(paused) {
  const {reason, callFrames} = paused.params;
  InspectorTest.log(`Paused with reason: ${reason}`);
  await session.logSourceLocation(callFrames[0].location);
}

let evaluation;

// Runs test() until the `debugger` statement in callee().
async function startTest() {
  evaluation = Protocol.Runtime.evaluate({expression: 'test()'});
  await logPause(await Protocol.Debugger.oncePaused());
}

async function finishTest() {
  await Protocol.Debugger.resume();
  await evaluation;
}

async function step(action, args) {
  InspectorTest.log(`${action}(${JSON.stringify(args)})`);
  Protocol.Debugger[action](args);
  await logPause(await Protocol.Debugger.oncePaused());
}

(async function() {
  Protocol.Debugger.enable();
  const {params: {scriptId}} = await Protocol.Debugger.onceScriptParsed();
  const letYRange = range(scriptId, 7, 0, 8, 0);

  InspectorTest.log('\nTest: stepOut after stepInto with skipList');
  await startTest();
  await step('stepInto', {skipList: [letYRange]});
  await step('stepOut', {});
  await finishTest();

  InspectorTest.log('\nTest: stepOut after resume after stepInto with skipList');
  await startTest();
  await step('stepInto', {skipList: [letYRange]});
  await finishTest();
  await startTest();
  await step('stepOut', {});
  await finishTest();

  InspectorTest.log('\nTest: stepInto with skipList after stepInto with ' +
                    'skipList uses the new skipList');
  await startTest();
  await step('stepInto', {skipList: [range(scriptId, 2, 0, 3, 0)]});
  await step('stepInto', {skipList: [letYRange]});
  await finishTest();

  InspectorTest.completeTest();
})();
