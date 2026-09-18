// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that destroying context from inside Error.prepareStackTrace during console.log does not crash');

const expression = `
  Error.prepareStackTrace = function(error, trace) {
    inspector.fireContextDestroyed(); // Free InjectedScript
    return '';
  };
  console.log(new Error('trigger')); // UAF triggered on return

  setTimeout(function() {
    inspector.fireContextCreated();
    console.log("End of test");
  }, 0);
`;

Protocol.Runtime.enable();
Protocol.Runtime.evaluate({ expression: expression });

Protocol.Runtime.onConsoleAPICalled(function(result) {
  InspectorTest.logObject(result.params.args[0]);
  if (result.params.args[0].value == "End of test") {
    InspectorTest.completeTest();
  }
});
