// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Checks breakProgram,(schedule|cancel)PauseOnNextStatement test API");

InspectorTest.addScript(`
function callBreakProgram() {
  breakProgram('reason', JSON.stringify({a: 42}));
}

function foo() {
  return 42;
}`, 7, 26);

InspectorTest.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  InspectorTest.log('Stack:');
  InspectorTest.logCallFrames(message.params.callFrames);
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
    schedulePauseOnNextStatement('reason', JSON.stringify({a: 42}));
    Protocol.Runtime.evaluate({ expression: 'foo()//# sourceURL=expr1.js'})
      .then(() => Protocol.Runtime.evaluate({
        expression: 'foo()//# sourceURL=expr2.js'}))
      .then(() => cancelPauseOnNextStatement())
      .then(next);
  },

  function testCancelPauseOnNextStatement(next) {
    schedulePauseOnNextStatement('reason', JSON.stringify({a: 42}));
    cancelPauseOnNextStatement();
    Protocol.Runtime.evaluate({ expression: 'foo()'})
      .then(next);
  }
]);
