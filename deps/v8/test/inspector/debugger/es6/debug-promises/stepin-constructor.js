// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into Promise constructor resolver.');

const source = `
var print = Math.sin;

function resolver(resolve, reject) {
  print(1);
  print(2);
  print(3);
  resolve();
}

debugger;
var p = new Promise(resolver);
var done = true;
`;

(async function test() {
  await Protocol.Debugger.enable();

  let breaks = [];
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
        {scriptId: callFrames[0].location.scriptId});
    let line =
        scriptSource.split('\n')[callFrames[0].location.lineNumber].trim();
    breaks.push(line);
    InspectorTest.log(`Paused at: ${line}`);
    if (line !== 'var done = true;' && breaks.length < 10) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  InspectorTest.log(`Total breaks: ${breaks.length}`);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
