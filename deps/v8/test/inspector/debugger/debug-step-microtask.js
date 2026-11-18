// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into microtasks.');

const source = `
var print = console.log;

Promise.resolve().then(
function() {
  print(1);
}
).then(
function() {
  return 2;
}
).then(
function() {
  throw new Error();
}
).catch(
function() {
  print(3);
}  // STOP
);

debugger;
`;

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    let callFrame = message.params.callFrames[0];
    let line = source.split('\n')[callFrame.location.lineNumber];
    InspectorTest.log('Paused on line: ' + line);
    if (!/STOP/.test(line)) {
      Protocol.Debugger.stepInto();
    } else {
      Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await InspectorTest.waitForPendingTasks();
  InspectorTest.completeTest();
})();
