// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that disconnecting session synchronously during argument wrapping does not cause UAF');

let session2 = contextGroup.connect();

session2.Protocol.Runtime.enable();
session2.Protocol.Runtime.onConsoleAPICalled(function(result) {
  if (result.params.args[0].value == "prepareStackTrace called") {
    InspectorTest.log("prepareStackTrace called");
    session2.Protocol.Runtime.evaluate({ expression: 'console.log("End of test")' });
  }
  if (result.params.args[0].value == "End of test") {
    InspectorTest.log("Did not crash");
    InspectorTest._sessions.delete(session);
    InspectorTest.completeTest();
  }
});

Protocol.Runtime.enable();
Protocol.Runtime.evaluate({
  expression: `
    Error.prepareStackTrace = function(error, stack) {
      console.log("prepareStackTrace called");
      inspector.disconnectSession(${session.id});
      return stack;
    };
    setTimeout(function() {
      const err = new Error("Trigger UAF");
      console.log(err);
    }, 0);
  `
});
