// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that the skipList of stepInto and stepOver does not suppress ' +
    'assert pauses.');

const source = [
  'function skippedBody() {',                    // 0
  '  let a = 1;',                                // 1
  '  console.assert(false, "assert failed");',   // 2
  '  let b = 2;',                                // 3
  '  return a + b;',                             // 4
  '}',                                           // 5
  'function test() {',                           // 6
  '  debugger;',                                 // 7
  '  skippedBody();',                            // 8
  '  return 0;',                                 // 9
  '}',                                           // 10
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
  const {reason, data, callFrames} = paused.params;
  const reasons = reason === 'ambiguous' ?
      ` (${data.reasons.map(r => r.reason).join(', ')})` :
      '';
  InspectorTest.log(`Paused with reason: ${reason}${reasons}`);
  await session.logSourceLocation(callFrames[0].location);
}

let evaluation;

// Runs test() until the `debugger` statement in test().
async function startTest() {
  evaluation = Protocol.Runtime.evaluate({expression: 'test()'});
  await Protocol.Debugger.oncePaused();
}

// Resumes and ignores any further pauses until test() returned.
async function finishTest() {
  await Protocol.Debugger.setSkipAllPauses({skip: true});
  await Protocol.Debugger.resume();
  await evaluation;
  await Protocol.Debugger.setSkipAllPauses({skip: false});
}

async function step(action, skipList) {
  Protocol.Debugger[action]({skipList});
  await logPause(await Protocol.Debugger.oncePaused());
}

(async function() {
  Protocol.Debugger.enable();
  const {params: {scriptId}} = await Protocol.Debugger.onceScriptParsed();
  // console.assert only pauses if pause on exceptions is enabled.
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  const skippedBodyRange = range(scriptId, 1, 0, 5, 0);

  for (const action of ['stepInto', 'stepOver']) {
    InspectorTest.log(
        `\nTest: ${action} into a skipped range hits an assert in the ` +
        `skipped range`);
    await startTest();
    await step('stepOver', []);
    await step(action, [skippedBodyRange]);
    await finishTest();
  }

  InspectorTest.completeTest();
})();
