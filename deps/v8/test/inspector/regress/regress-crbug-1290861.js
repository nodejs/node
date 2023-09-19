// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Ensure that catch prediction is correct for [[Reject]] handlers.');

contextGroup.addScript(`function throwInPromiseCaughtAndRethrown() {
  var reject;
  var promise = new Promise(function(res, rej) { reject = rej; }).catch(
    function rejectHandler(e) {
      throw e;
    }
  );
  reject(new Error());
  return promise;
}`);

Protocol.Debugger.onPaused(({params: {callFrames, data}}) => {
  InspectorTest.log(`${data.uncaught ? 'Uncaught' : 'Caught'} exception in ${
      callFrames[0].functionName}`);
  Protocol.Debugger.resume();
});

InspectorTest.runAsyncTestSuite([async function test() {
  await Promise.all([
    Protocol.Runtime.enable(),
    Protocol.Debugger.enable(),
    Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'}),
  ]);
  await Protocol.Runtime.evaluate({
    awaitPromise: true,
    expression: 'throwInPromiseCaughtAndRethrown()',
  });
  await Promise.all([
    Protocol.Runtime.disable(),
    Protocol.Debugger.disable(),
  ]);
}]);
