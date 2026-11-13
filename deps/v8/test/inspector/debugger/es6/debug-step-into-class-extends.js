// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into class extends expression.');

const source = `
var done = false;

function GetBase() {
  var x = 1;   // 1.
  var y = 2;   // 2.
  done = true; // 3.
  return null;
}

function f() {
  class Derived extends GetBase() {} // 0.
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let stepCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
        {scriptId: callFrames[0].location.scriptId});
    let line = scriptSource.split('\n')[callFrames[0].location.lineNumber];
    InspectorTest.log(`Paused at line: ${line.trim()}`);

    let {result: {result: doneResult}} =
        await Protocol.Runtime.evaluate({expression: 'done'});
    if (!doneResult.value) {
      InspectorTest.log(`Step ${stepCount} matches '// ${stepCount}.': ${
          line.includes('// ' + stepCount + '.')}`);
      stepCount++;
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 11,
    columnNumber: 24,
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`Total step count: ${stepCount}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
