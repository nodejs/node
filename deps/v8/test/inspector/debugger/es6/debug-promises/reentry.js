// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test reentry of special try catch for Promises.');

const source = `
var p = new Promise(function(resolve, reject) { resolve(); });
var q = p.then(function() {
  new Promise(function(resolve, reject) { resolve(); });
});
`;

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});

  let paused = false;
  Protocol.Debugger.onPaused(async message => {
    paused = true;
    InspectorTest.log('FAIL: paused on unexpected exception');
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  InspectorTest.log(`Paused on exception: ${paused}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
