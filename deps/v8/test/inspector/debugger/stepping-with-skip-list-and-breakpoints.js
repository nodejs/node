// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that the skipList of stepInto and stepOver does not suppress ' +
    'breakpoints or debugger statements.');

const source = [
  'function inlinedBody() {',       // 0
  '  let a = 1;',                   // 1
  '  let b = 2;',                   // 2
  '  return a + b;',                // 3
  '}',                              // 4
  'function withDebuggerStmt() {',  // 5
  '  let c = 1;',                   // 6
  '  debugger;',                    // 7
  '  return c;',                    // 8
  '}',                              // 9
  'function test() {',              // 10
  '  debugger;',                    // 11
  '  inlinedBody();',               // 12
  '  withDebuggerStmt();',          // 13
  '  return 0;',                    // 14
  '}',                              // 15
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
  const {reason, hitBreakpoints, callFrames} = paused.params;
  InspectorTest.log(`Paused with reason: ${reason}, hitBreakpoints: ${
      (hitBreakpoints || []).length}`);
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
  const inlinedBodyRange = range(scriptId, 1, 0, 4, 0);
  const withDebuggerStmtRange = range(scriptId, 6, 0, 9, 0);

  for (const action of ['stepInto', 'stepOver']) {
    InspectorTest.log(`\nTest: ${action} into a skipped range hits a ` +
                      `breakpoint in the skipped range`);
    await startTest();
    let {result: {breakpointId}} = await Protocol.Debugger.setBreakpoint(
        {location: {scriptId, lineNumber: 2, columnNumber: 2}});
    await step('stepOver', []);
    await step(action, [inlinedBodyRange]);
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await finishTest();

    InspectorTest.log(`\nTest: ${action} into a skipped range stops at a ` +
                      `debugger statement in the skipped range`);
    await startTest();
    await step('stepOver', []);
    await step('stepOver', []);
    await step(action, [withDebuggerStmtRange]);
    await finishTest();

    InspectorTest.log(`\nTest: ${action} into a skipped range without ` +
                      `breakpoints skips the range`);
    await startTest();
    await step('stepOver', []);
    await step(action, [inlinedBodyRange]);
    await finishTest();

    InspectorTest.log(`\nTest: ${action} into a skipped range skips a ` +
                      `breakpoint with a false condition`);
    await startTest();
    ({result: {breakpointId}} = await Protocol.Debugger.setBreakpoint({
      location: {scriptId, lineNumber: 2, columnNumber: 2},
      condition: 'false'
    }));
    await step('stepOver', []);
    await step(action, [inlinedBodyRange]);
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await finishTest();
  }

  InspectorTest.log(
      '\nTest: stepOver stops at a breakpoint in the skipped range');
  await startTest();
  const {result: {breakpointId}} = await Protocol.Debugger.setBreakpoint(
      {location: {scriptId, lineNumber: 13, columnNumber: 2}});
  await step('stepOver', [range(scriptId, 12, 0, 14, 0)]);
  await Protocol.Debugger.removeBreakpoint({breakpointId});
  await finishTest();

  InspectorTest.completeTest();
})();
