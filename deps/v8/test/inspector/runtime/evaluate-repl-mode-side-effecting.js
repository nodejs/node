// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {Protocol} = InspectorTest.start(
  'Tests that Runtime.evaluate with REPL mode correctly detects side-effects.');

Protocol.Runtime.enable();
(async function() {
  InspectorTest.log('Test "let" declaration is side-effecting');
  await evaluateRepl('let x = 21;');

  InspectorTest.log('Test "const" declaration is side-effecting');
  await evaluateRepl('const y = 42;');

  InspectorTest.log('Test "const" with side-effects works afterwards');
  await evaluateReplWithSideEffects('const y = 42;');

  InspectorTest.log('Test side-effect free expressions can be eagerly evaluated');
  await evaluateRepl('1 + 2');
  await evaluateRepl('"hello " + "REPL"');

  await evaluateRepl('(async function foo() { return 42; })();');

  InspectorTest.completeTest();
})();

async function evaluateRepl(expression) {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: expression,
    replMode: true,
    throwOnSideEffect: true
  }));
}

async function evaluateReplWithSideEffects(expression) {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression: expression,
    replMode: true,
    throwOnSideEffect: false
  }));
}
