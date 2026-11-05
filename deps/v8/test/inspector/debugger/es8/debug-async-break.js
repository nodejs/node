// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-async-break.');

const source = `
var break_count = 0;

function assertEquals(expected, actual) {
  if (expected !== actual) {
    throw new Error('Expected ' + expected + ', got ' + actual);
  }
}

async function g() {
  throw 1;
}

async function f() {
  try {
    await g();                   // B1
  } catch (e) {}
  assertEquals(2, break_count);  // B2
  return 1;                      // B3
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();

  let break_count = 0;
  Protocol.Debugger.onPaused(async message => {
    break_count++;
    await session.logSourceLocation(message.params.callFrames[0].location);
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: message.params.callFrames[0].callFrameId,
      expression: 'break_count++',
    });
    Protocol.Debugger.resume();
  });

  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 15, columnNumber: 0, url: 'test.js'});
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 17, columnNumber: 0, url: 'test.js'});
  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 18, columnNumber: 0, url: 'test.js'});

  const {result} =
      await Protocol.Runtime.evaluate({expression: 'f()', awaitPromise: true});
  if (result.exceptionDetails) {
    InspectorTest.log('f() threw: ' + JSON.stringify(result.exceptionDetails));
  }
  InspectorTest.log(`break_count: ${break_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
