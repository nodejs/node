// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that stepping into native functions does not break inside natives.');

const source = `
function f(x) {
  return x;                        // break
}                                  // break

function runTest() {
  debugger;                        // break
  new Error("123").stack;          // break
  Math.sin(0);                     // break
  f("this should break");          // break
  return 0;                        // break stop
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  const lines = source.split('\n');
  await Protocol.Debugger.enable();

  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 2, columnNumber: 0, url: 'test.js'});

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const line = lines[callFrames[0].location.lineNumber];
    InspectorTest.log(line);
    if (line.includes('stop')) {
      await Protocol.Debugger.disable();
    } else {
      await Protocol.Debugger.stepInto();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'runTest()'});
  await Protocol.Runtime.evaluate({expression: 'f("this should not break")'});
  InspectorTest.completeTest();
})();
