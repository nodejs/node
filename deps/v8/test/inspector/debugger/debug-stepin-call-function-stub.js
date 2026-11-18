// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into call function stub.');

const source = `
function g() {
  return "s";  // expected line
}

function testFunction() {
  var f = g;
  var s = 1 + f(10);
}

function g2() {
  return "s2";  // expected line
}

function testFunction2() {
  var f = g2;
  var s = 1 + f(10, 20);
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

const sourceLines = source.split('\n');

(async function test() {
  await Protocol.Debugger.enable();

  // Part 1: Breakpoint set at 'var f = g;' (line 6).
  // Step into twice to reach g.
  // Run 3 times (lazy compile first, cached version subsequent).
  for (let i = 0; i < 3; i++) {
    let state = 0;
    let expectedReached = false;
    let targetLineText = '';

    const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl(
        {lineNumber: 6, columnNumber: 0, url: 'test.js'});

    const onPaused = async ({params: {callFrames}}) => {
      const topFrame = callFrames[0];
      const lineText = sourceLines[topFrame.location.lineNumber].trim();
      if (state < 2) {
        state++;
        await Protocol.Debugger.stepInto();
      } else {
        if (topFrame.functionName === 'g' && lineText.includes('return "s";')) {
          expectedReached = true;
          targetLineText = lineText;
        }
        state = 3;
        await Protocol.Debugger.resume();
      }
    };

    Protocol.Debugger.onPaused(onPaused);
    await Protocol.Runtime.evaluate({expression: 'testFunction();'});
    Protocol.Debugger.onPaused(null);

    await Protocol.Debugger.removeBreakpoint({breakpointId});
    InspectorTest.log(
        `testFunction run ${i}: reached='${targetLineText}', state=${state}`);
  }

  // Part 2: Breakpoint set at place of call (line 16).
  // Step into once to reach g2.
  // Run 3 times (lazy compile first, cached version subsequent).
  for (let i = 0; i < 3; i++) {
    let state = 0;
    let expectedReached = false;
    let targetLineText = '';

    const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl(
        {lineNumber: 16, columnNumber: 0, url: 'test.js'});

    const onPaused = async ({params: {callFrames}}) => {
      const topFrame = callFrames[0];
      const lineText = sourceLines[topFrame.location.lineNumber].trim();
      if (state < 1) {
        state++;
        await Protocol.Debugger.stepInto();
      } else {
        if (topFrame.functionName === 'g2' &&
            lineText.includes('return "s2";')) {
          expectedReached = true;
          targetLineText = lineText;
        }
        state = 3;
        await Protocol.Debugger.resume();
      }
    };

    Protocol.Debugger.onPaused(onPaused);
    await Protocol.Runtime.evaluate({expression: 'testFunction2();'});
    Protocol.Debugger.onPaused(null);

    await Protocol.Debugger.removeBreakpoint({breakpointId});
    InspectorTest.log(
        `testFunction2 run ${i}: reached='${targetLineText}', state=${state}`);
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
