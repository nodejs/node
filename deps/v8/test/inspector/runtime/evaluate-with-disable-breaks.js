// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that Runtime.evaluate can run with breaks disabled.");

session.setupScriptMap();
contextGroup.addScript(`
    function f() {
      debugger;
    } //# sourceURL=test.js`);
Protocol.Runtime.enable();
Protocol.Debugger.enable();

Protocol.Debugger.onPaused(message => {
  InspectorTest.log("paused");
  Protocol.Debugger.resume();
});

(async function() {
  InspectorTest.log("Test disableBreaks: false");
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "debugger;",
    disableBreaks: false
  }));

  InspectorTest.log("Test disableBreaks: true");
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "debugger;",
    disableBreaks: true
  }));

  InspectorTest.log("Test calling out with disableBreaks: false");
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "f();",
    disableBreaks: false
  }));

  InspectorTest.log("Test calling out with disableBreaks: true");
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "f();",
    disableBreaks: true
  }));

  InspectorTest.log("Test Debugger.pause with disableBreaks: false");
        InspectorTest.logMessage(await Protocol.Debugger.pause());
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "1",
    disableBreaks: false
  }));

  InspectorTest.log("Test Debugger.pause with disableBreaks: true");
        InspectorTest.logMessage(await Protocol.Debugger.pause());
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: "1",
    disableBreaks: true
  }));

  InspectorTest.completeTest();
})();
