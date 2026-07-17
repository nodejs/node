// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start('Checks possible break locations.');
session.setupScriptMap();

Protocol.Debugger.onPaused(async message => {
  const { callFrames } = message.params;
  if (callFrames.length === 1) {
    Protocol.Debugger.stepInto();
    return;
  }

  InspectorTest.log('break at:');
  await session.logSourceLocation(callFrames[0].location);
  Protocol.Debugger.stepOver();
});

contextGroup.loadScript('test/inspector/debugger/resources/break-locations.js');

(async () => {
  await Protocol.Debugger.enable();
  const testMessage = await Protocol.Runtime.evaluate({
    expression: 'Object.keys(this).filter(name => name.indexOf(\'test\') === 0)',
    returnByValue: true,
  });
  const tests = testMessage.result.result.value;

  InspectorTest.runAsyncTestSuite(tests.map(test => eval(`
    (async function ${test}() {
      await Protocol.Runtime.evaluate({
        expression: 'debugger; ${test}()',
        awaitPromise: ${test.indexOf('testPromise') === 0},
      });
    });
  `)));
})();
