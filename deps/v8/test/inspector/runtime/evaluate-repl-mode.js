// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {Protocol} = InspectorTest.start(
  "Tests that Runtime.evaluate works with REPL mode.");

Protocol.Runtime.enable();
(async function() {
  InspectorTest.log("Test 'let' re-declaration");
  evaluateRepl('let x = 21;');
  evaluateRepl('x;');
  evaluateRepl('let x = 42;');
  evaluateRepl('x;');

  // Regression test for crbug.com/1040034.
  InspectorTest.log("SyntaxError in REPL mode does not crash the parser");
  evaluateRepl('if (true) const x');

  InspectorTest.completeTest();
})();

async function evaluateRepl(expression) {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: expression,
    replMode: true,
  }));
}
