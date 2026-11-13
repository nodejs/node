// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into construct call.');

session.setupScriptMap();

contextGroup.addScript(
    `
function f() {
  this.x = 1;
}

function g() {
  new f();
}

function test() {
  g();
}
`,
    0, 0, 'test.js');

(async function() {
  await Protocol.Debugger.enable();

  let break_count = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    break_count++;
    InspectorTest.log(`Break ${break_count} in ${callFrames[0].functionName}:`);
    await session.logSourceLocation(callFrames[0].location);
    if (break_count < 6) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 10, columnNumber: 0, url: 'test.js'});

  await Protocol.Runtime.evaluate({expression: 'test()'});
  InspectorTest.log(`Total breaks: ${break_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
