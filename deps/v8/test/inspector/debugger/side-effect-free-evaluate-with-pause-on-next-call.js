// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks side-effect free evaluation while a pause on the next function ' +
    'call is requested.');

contextGroup.addScript(`
var x = 0;
function f() {
  return 42;
}
function g() {
  x = 1;
}
//# sourceURL=test.js`);

(async function() {
  await Protocol.Debugger.enable();
  // Not running JavaScript, so this requests a pause on the next call.
  await Protocol.Debugger.pause();

  InspectorTest.log('Evaluate side-effect free call:');
  InspectorTest.logMessage((await Protocol.Runtime.evaluate(
      {expression: 'f()', throwOnSideEffect: true})).result.result);

  InspectorTest.log('Evaluate side-effecting call:');
  InspectorTest.logMessage((await Protocol.Runtime.evaluate(
      {expression: 'g()', throwOnSideEffect: true})).result.exceptionDetails
      .exception.description);

  InspectorTest.log('Pause request is still honored afterwards:');
  Protocol.Runtime.evaluate({expression: 'f()'});
  const {params: {callFrames: [{functionName}]}} =
      await Protocol.Debugger.oncePaused();
  InspectorTest.log(`Paused in ${functionName || '(anonymous)'}`);
  await Protocol.Debugger.resume();
  InspectorTest.completeTest();
})();
