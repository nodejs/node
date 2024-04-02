// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test for console.timeLog');

(async function test() {
  Protocol.Runtime.enable();
  utils.setCurrentTimeMSForTest(0.0);
  await Protocol.Runtime.evaluate({expression: `console.time('42')`});
  utils.setCurrentTimeMSForTest(1.0);
  Protocol.Runtime.evaluate({expression: `console.timeLog('42', 'a')`});
  logArgs(await Protocol.Runtime.onceConsoleAPICalled());
  utils.setCurrentTimeMSForTest(2.0);
  Protocol.Runtime.evaluate({expression: `console.timeLog('42', 'a', 'b')`});
  logArgs(await Protocol.Runtime.onceConsoleAPICalled());
  utils.setCurrentTimeMSForTest(3.0);
  Protocol.Runtime.evaluate({expression: `console.timeEnd('42')`});
  logArgs(await Protocol.Runtime.onceConsoleAPICalled());
  utils.setCurrentTimeMSForTest(4.0);
  Protocol.Runtime.evaluate({expression: `console.timeLog('42', 'text')`});
  logArgs(await Protocol.Runtime.onceConsoleAPICalled());
  InspectorTest.completeTest();
})()

function logArgs(message) {
  InspectorTest.logMessage(message.params.args);
}
