// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Debugger breaks in next script after stepOut from previous one.');

contextGroup.addInlineScript(
    `
function test() {
  setTimeout('var a = 1;//# sourceURL=timeout1.js', 0);
  setTimeout(foo, 0);
  setTimeout('var a = 3;//# sourceURL=timeout3.js', 0);
  debugger;
}`,
    'foo.js');

contextGroup.addInlineScript(
    `
function foo() {
  return 42;
}`,
    'timeout2.js');

session.setupScriptMap();
var stepAction;
Protocol.Debugger.onPaused(message => {
  session.logCallFrames(message.params.callFrames);
  InspectorTest.log('');
  Protocol.Debugger[stepAction]();
});
Protocol.Debugger.enable()
InspectorTest.runTestSuite([
  function testStepOut(next) {
    stepAction = 'stepOut';
    Protocol.Runtime.evaluate({ expression: 'test()' })
      .then(() => InspectorTest.waitForPendingTasks())
      .then(next);
  },

  function testStepOver(next) {
    stepAction = 'stepOver';
    Protocol.Runtime.evaluate({ expression: 'test()' })
      .then(() => InspectorTest.waitForPendingTasks())
      .then(next);
  },

  function testStepInto(next) {
    stepAction = 'stepInto';
    Protocol.Runtime.evaluate({ expression: 'test()' })
      .then(() => InspectorTest.waitForPendingTasks())
      .then(next);
  }
]);
