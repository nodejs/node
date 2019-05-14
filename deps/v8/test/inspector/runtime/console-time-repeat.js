// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that repeated console.time do not reset');

Protocol.Runtime.onConsoleAPICalled(InspectorTest.logMessage);
Protocol.Runtime.enable();
(async function() {
  utils.setCurrentTimeMSForTest(0.0);
  await Protocol.Runtime.evaluate({expression: `console.time('a')`});
  utils.setCurrentTimeMSForTest(1.0);
  await Protocol.Runtime.evaluate({expression: `console.time('a')`});
  utils.setCurrentTimeMSForTest(2.0);
  await Protocol.Runtime.evaluate({expression: `console.timeEnd('a')`});
  utils.setCurrentTimeMSForTest(5.0);
  await Protocol.Runtime.evaluate({expression: `console.timeEnd('a')`});

  InspectorTest.completeTest();
})();
