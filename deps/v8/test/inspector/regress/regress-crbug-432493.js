// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-432493.');

contextGroup.addScript(
    `
function f() {
  var a = 1;
  var b = 2;
  return a + b;
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  await Protocol.Debugger.setBreakpointByUrl({lineNumber: 3, url: 'test.js'});

  let breakCount = 0;
  let throwCount = 0;
  Protocol.Debugger.onPaused(async message => {
    if (message.params.reason === 'exception' ||
        message.params.reason === 'promiseRejection') {
      throwCount++;
      await Protocol.Debugger.setBreakpointsActive({active: true});
    } else {
      breakCount++;
      await Protocol.Debugger.setBreakpointsActive({active: false});
    }
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f(); f();'});
  InspectorTest.log(`breakCount: ${breakCount}, throwCount: ${throwCount}`);

  await Protocol.Runtime.evaluate(
      {expression: 'try { throw 1; } catch (e) {}\nf(); f();'});
  InspectorTest.log(`breakCount: ${breakCount}, throwCount: ${throwCount}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.setBreakpointsActive({active: true});
  InspectorTest.completeTest();
})();
