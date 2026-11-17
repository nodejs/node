// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that breakpoints after the last breakable position of a ' +
    'function resolve forward into nested functions.');

const source = `
function returnBeforeInner() {
  return inner;
  function inner() {
    return 1;
  }
}

function throwBeforeInner() {
  throw inner;
  function inner() {
    return 2;
  }
}

function liveCodeBeforeInner() {
  var x = inner;
  function inner() {
    return 3;
  }
  return x;
}
`;
contextGroup.addScript(source, 0, 0, 'test.js');

session.setupScriptMap();

(async () => {
  await Protocol.Debugger.enable();

  // Set breakpoints on the lines declaring the nested `inner` functions.
  for (const lineNumber of [3, 10, 17]) {
    InspectorTest.log(`Setting breakpoint at line ${lineNumber}:`);
    const {result: {locations}} = await Protocol.Debugger.setBreakpointByUrl(
        {lineNumber, url: 'test.js'});
    for (const location of locations) {
      await session.logSourceLocation(location);
    }
  }

  // Check that the breakpoints are hit in the nested `inner` functions.
  const expressions = [
    'returnBeforeInner()()',
    'try { throwBeforeInner(); } catch (inner) { inner(); }',
    'liveCodeBeforeInner()()',
  ];
  for (const expression of expressions) {
    InspectorTest.log(`Evaluating ${expression}:`);
    const evaluatePromise = Protocol.Runtime.evaluate({expression});
    const {params: {callFrames: [{location}]}} =
        await Protocol.Debugger.oncePaused();
    await session.logSourceLocation(location);
    await Protocol.Debugger.resume();
    await evaluatePromise;
  }

  InspectorTest.completeTest();
})();
