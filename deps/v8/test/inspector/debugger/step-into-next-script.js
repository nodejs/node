// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Debugger breaks in next script after stepOut from previous one.');

InspectorTest.addScript(`
function test() {
  setTimeout('var a = 1;//# sourceURL=timeout1.js', 0);
  setTimeout(foo, 0);
  setTimeout('var a = 3;//# sourceURL=timeout3.js', 0);
  debugger;
}
//# sourceURL=foo.js`, 7, 26);

InspectorTest.addScript(`
function foo() {
  return 42;
}
//# sourceURL=timeout2.js`)

InspectorTest.setupScriptMap();
var stepAction;
Protocol.Debugger.onPaused(message => {
  InspectorTest.logCallFrames(message.params.callFrames);
  InspectorTest.log('');
  Protocol.Debugger[stepAction]();
});
Protocol.Debugger.enable()
InspectorTest.runTestSuite([
  function testStepOut(next) {
    stepAction = 'stepOut';
    Protocol.Runtime.evaluate({ expression: 'test()' })
      .then(() => InspectorTest.waitPendingTasks())
      .then(next);
  },

  function testStepOver(next) {
    stepAction = 'stepOver';
    Protocol.Runtime.evaluate({ expression: 'test()' })
      .then(() => InspectorTest.waitPendingTasks())
      .then(next);
  },

  function testStepInto(next) {
    stepAction = 'stepInto';
    Protocol.Runtime.evaluate({ expression: 'test()' })
      .then(() => InspectorTest.waitPendingTasks())
      .then(next);
  }
]);
