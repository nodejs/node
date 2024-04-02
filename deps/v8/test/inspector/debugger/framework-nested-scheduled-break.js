// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks nested scheduled break in framework code.');

contextGroup.addInlineScript(
    `
function frameworkCall(callback) {
  inspector.callWithScheduledBreak(doFrameworkWork.bind(null, callback),
    'top-framework-scheduled-break',
    JSON.stringify({ data: 'data for top-framework-scheduled-break' }));
}

function doFrameworkWork(callback) {
  inspector.callWithScheduledBreak(doFrameworkBreak, 'should-not-be-a-reason', '');
  callback();
}

function doFrameworkBreak() {
  inspector.breakProgram('framework-break', JSON.stringify({ data: 'data for framework-break' }));
}`,
    'framework.js');

contextGroup.addInlineScript(
    `
function testFunction() {
  inspector.callWithScheduledBreak(frameworkCall.bind(null, callback),
    'top-scheduled-break', '');
}

function callback() {
  inspector.breakProgram('user-break', JSON.stringify({ data: 'data for user-break' }));
  return 42;
}`,
    'user.js');

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  InspectorTest.log('break reason: ' + message.params.reason);
  InspectorTest.log('break aux data: ' + JSON.stringify(message.params.data || {}, null, '  '));
  session.logCallFrames(message.params.callFrames);
  InspectorTest.log('');
  Protocol.Debugger.resume();
});
Protocol.Debugger.enable()
  .then(() => Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']}))
  .then(() => Protocol.Runtime.evaluate({ expression: 'testFunction()//# sourceURL=expr.js'}))
  .then(InspectorTest.completeTest);
