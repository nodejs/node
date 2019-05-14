// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that we passed correct arguments in ' +
  'V8InspectorClient::consoleAPIMessage. Note: lines and columns are 1-based.');

contextGroup.addScript(`
function consoleTrace() {
  function a() {
    function b() {
      console.trace(239);
    }
    b();
  }
  a();
}
`, 8, 26);

Protocol.Runtime.enable();
utils.setLogConsoleApiMessageCalls(true);
(async function test() {
  Protocol.Runtime.evaluate({expression: 'console.log(42)'});
  await Protocol.Runtime.onceConsoleAPICalled()
  Protocol.Runtime.evaluate({expression: 'consoleTrace()'});
  await Protocol.Runtime.onceConsoleAPICalled()
  InspectorTest.completeTest();
})();
