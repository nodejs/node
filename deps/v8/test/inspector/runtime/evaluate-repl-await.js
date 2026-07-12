// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {Protocol} = InspectorTest.start(
  "Tests that top-level await in Runtime.evaluate REPL mode includes stack trace.");

Protocol.Debugger.enable();
Protocol.Runtime.enable();

(async function() {
  evaluateRepl('throw undefined;');

  evaluateRepl(`
    function bar() {
      throw new Error('ba dum tsh');
    }

    function foo() {
      bar();
    }

    foo();
  `);

  InspectorTest.completeTest();
})();

async function evaluateRepl(expression) {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: expression,
    replMode: true,
  }));
}
