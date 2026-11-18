// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into through Promises.');

const source = `
var done = false;
var testError = null;

Promise.resolve(42)
  .then(
    function f0() {
      debugger;  // Break 0.
    } // Break 1.
  )
  .then(callback)
  .then(callback.bind(null))
  .then(Object)
  .then(callback.bind(null).bind(null))
  .then(finalize)
  .catch(function(err) {
    testError = 'FAIL: ' + err;
  });

function callback(x) {
  return x // Break 2. // Break 4. // Break 6.
  ; // Break 3. // Break 5. // Break 7.
}

function finalize() {
  done = true; // Break 8.
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  const expectedBreaks = 9;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
        {scriptId: callFrames[0].location.scriptId});
    let line = scriptSource.split('\n')[callFrames[0].location.lineNumber];
    InspectorTest.log('paused at: ' + line);
    if (line.indexOf('// Break ' + breakCount + '.') === -1) {
      InspectorTest.log(
          'Unexpected pause at: ' + line + '\n' +
          'Expected: // Break ' + breakCount + '.');
    }
    ++breakCount;
    if (breakCount !== expectedBreaks) {
      Protocol.Debugger.stepInto();
    } else {
      Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  let {result: {result: doneResult}} =
      await Protocol.Runtime.evaluate({expression: 'done'});
  let {result: {result: errorResult}} =
      await Protocol.Runtime.evaluate({expression: 'testError'});

  InspectorTest.log(`Break count: ${breakCount}`);
  InspectorTest.log(`Done: ${doneResult.value}`);
  InspectorTest.log(`Error: ${errorResult.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
