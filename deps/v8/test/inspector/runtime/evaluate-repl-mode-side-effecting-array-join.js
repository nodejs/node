// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {Protocol} = InspectorTest.start(
  'Tests that Runtime.evaluate with REPL mode correctly handles \
Array.prototype.join.');

Protocol.Runtime.enable();
(async function () {
  await evaluateReplWithSideEffects('a=[/a/]')
  await evaluateRepl('a.toString()');
  await evaluateReplWithSideEffects('a.toString()');

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
