// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const expression = `
  Object.defineProperty(Object.prototype, 'RemoteObject', {
    configurable: true,
    set(v) {
      delete Object.prototype.RemoteObject;
      this.RemoteObject = v;

      detachInspector();
      setTimeout(function() {
        // Attach the inspector again for the sake of establishing a
        // communication channel with the frontend test runner.
        attachInspector();
        console.log("End of test");
      }, 0);
    },
  });

  // Before the whole script runs, the inspector is already attached.
  // Re-attach the inspector and trigger the console API to make sure that the
  // injected inspector script runs again (and triggers the above setter).
  detachInspector();
  attachInspector();
  console.log("First inspector activity after attaching inspector");
`;

Protocol.Runtime.enable();
Protocol.Runtime.evaluate({ expression: expression });

Protocol.Runtime.onConsoleAPICalled(function(result) {
  InspectorTest.logObject(result.params.args[0]);
  if (result.params.args[0].value == "End of test") {
    InspectorTest.completeTest();
  }
});
