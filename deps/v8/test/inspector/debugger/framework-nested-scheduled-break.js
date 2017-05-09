// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print('Checks nested scheduled break in framework code.');

InspectorTest.addScript(`
function frameworkCall(callback) {
  callWithScheduledBreak(doFrameworkWork.bind(null, callback),
    'top-framework-scheduled-break',
    JSON.stringify({ data: 'data for top-framework-scheduled-break' }));
}

function doFrameworkWork(callback) {
  callWithScheduledBreak(doFrameworkBreak, 'should-not-be-a-reason', '');
  callback();
}

function doFrameworkBreak() {
  breakProgram('framework-break', JSON.stringify({ data: 'data for framework-break' }));
}

//# sourceURL=framework.js`, 7, 26);

InspectorTest.addScript(`
function testFunction() {
  callWithScheduledBreak(frameworkCall.bind(null, callback),
    'top-scheduled-break', '');
}

function callback() {
  breakProgram('user-break', JSON.stringify({ data: 'data for user-break' }));
  return 42;
}

//# sourceURL=user.js`, 25, 26);

InspectorTest.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  InspectorTest.log('break reason: ' + message.params.reason);
  InspectorTest.log('break aux data: ' + JSON.stringify(message.params.data || {}, null, '  '));
  InspectorTest.logCallFrames(message.params.callFrames);
  InspectorTest.log('');
  Protocol.Debugger.resume();
});
Protocol.Debugger.enable()
  .then(() => Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']}))
  .then(() => Protocol.Runtime.evaluate({ expression: 'testFunction()//# sourceURL=expr.js'}))
  .then(InspectorTest.completeTest);
