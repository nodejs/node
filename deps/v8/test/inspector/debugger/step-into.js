// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo

let {session, contextGroup, Protocol} = InspectorTest.start('Checks possible break locations.');

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  var frames = message.params.callFrames;
  if (frames.length === 1) {
    Protocol.Debugger.stepInto();
    return;
  }
  var scriptId = frames[0].location.scriptId;
  InspectorTest.log('break at:');
  session.logSourceLocation(frames[0].location)
    .then(() => Protocol.Debugger.stepInto());
});

contextGroup.loadScript('test/inspector/debugger/resources/break-locations.js');

Protocol.Debugger.enable();
Protocol.Runtime.evaluate({ expression: 'Object.keys(this).filter(name => name.indexOf(\'test\') === 0)', returnByValue: true })
  .then(runTests);

function runTests(message) {
  var tests = message.result.result.value;
  InspectorTest.runTestSuite(tests.map(test => eval(`(function ${test}(next) {
    Protocol.Runtime.evaluate({ expression: 'debugger; ${test}()', awaitPromise: ${test.indexOf('testPromise') === 0}})
      .then(next);
  })`)));
}
