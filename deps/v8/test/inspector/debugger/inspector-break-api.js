// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Checks breakProgram,(schedule|cancel)PauseOnNextStatement test API");

contextGroup.addScript(`
function callBreakProgram() {
  inspector.breakProgram('reason', JSON.stringify({a: 42}));
}

function foo() {
  return 42;
}`, 7, 26);

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  InspectorTest.log('Stack:');
  session.logCallFrames(message.params.callFrames);
  delete message.params.callFrames;
  InspectorTest.log('Other data:');
  InspectorTest.logMessage(message);
  InspectorTest.log('');
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();

InspectorTest.runTestSuite([
  function testBreakProgram(next) {
    Protocol.Runtime.evaluate({ expression: 'callBreakProgram()'})
      .then(next);
  },

  function testSchedulePauseOnNextStatement(next) {
    contextGroup.schedulePauseOnNextStatement('reason', JSON.stringify({a: 42}));
    Protocol.Runtime.evaluate({ expression: 'foo()//# sourceURL=expr1.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'foo()//# sourceURL=expr2.js'}))
      .then(() => contextGroup.cancelPauseOnNextStatement())
      .then(next);
  },

  function testCancelPauseOnNextStatement(next) {
    contextGroup.schedulePauseOnNextStatement('reason', JSON.stringify({a: 42}));
    contextGroup.cancelPauseOnNextStatement();
    Protocol.Runtime.evaluate({ expression: 'foo()'})
      .then(next);
  }
]);
