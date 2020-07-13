// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {Protocol} = InspectorTest.start(
  "Regression test for 986051");

Protocol.Runtime.enable();
(async function() {
  InspectorTest.log("Regression test");
  evaluateRepl('1', true);
  evaluateRepl('$0', false);
  evaluateRepl('Object.defineProperty(globalThis, "$0", {configurable: false});', true);
  evaluateRepl('$0', true);
  evaluateRepl('$0', false);
  InspectorTest.completeTest();
})();

async function evaluateRepl(expression, includeCommandLineAPI) {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression,
    includeCommandLineAPI,
    replMode: true,
  }));
}
