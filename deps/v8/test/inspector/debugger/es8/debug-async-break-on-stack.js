// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-async-break-on-stack.');

const source = `
async function g() {
  setbreaks();
  throw 1;  // B1
}

async function f() {
  try {
    await g();
  } catch (e) {}
  return 2;  // B2
}

function setbreaks() {
  debugger;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  let step = 0;
  Protocol.Debugger.onPaused(async message => {
    if (step === 0) {
      await Protocol.Debugger.setBreakpointByUrl(
          {lineNumber: 3, columnNumber: 0, url: 'test.js'});
      await Protocol.Debugger.setBreakpointByUrl(
          {lineNumber: 10, columnNumber: 0, url: 'test.js'});
    } else {
      await session.logSourceLocation(message.params.callFrames[0].location);
    }
    step++;
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()', awaitPromise: true});
  InspectorTest.log(`break_count: ${step - 1}`);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
