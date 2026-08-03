// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
  'Test that var does not leak into object in parent scope. crbug.com/1085693');

const inputSnippet = `
function testFunction() {
  const objectWeShouldNotLeakInto = {};
  (() => {
    debugger; // evaluate "var leakedProperty = 1" at this break
    console.log(objectWeShouldNotLeakInto);
  })();
}
testFunction();
`;

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  Protocol.Runtime.evaluate({
    expression: inputSnippet,
    replMode: true,
  });

  const pausedMessage = await Protocol.Debugger.oncePaused();

  InspectorTest.log('Paused in arrow function');

  const topFrame = pausedMessage.params.callFrames[0];
  await Protocol.Debugger.evaluateOnCallFrame({
    expression: 'var leakedProperty = 1;',
    callFrameId: topFrame.callFrameId,
  });
  Protocol.Debugger.resume();

  const { params } = await Protocol.Runtime.onceConsoleAPICalled();
  InspectorTest.logMessage(params.args[0]);

  InspectorTest.completeTest();
})();
