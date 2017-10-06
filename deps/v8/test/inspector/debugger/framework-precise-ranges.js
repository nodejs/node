// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks framework debugging with blackboxed ranges.');

contextGroup.addScript(
    `
function foo() {
  return boo();
}
function boo() {
  return 42;
}
function testFunction() {
  foo();
}
//# sourceURL=test.js`,
    7, 26);

session.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  session.logCallFrames(message.params.callFrames);
  InspectorTest.log('');
  Protocol.Debugger.stepInto();
});
var scriptId;
Protocol.Debugger.onScriptParsed(message => {
  if (message.params.url === 'test.js') {
    scriptId = message.params.scriptId;
  }
});

Protocol.Debugger.enable()
    .then(() => Protocol.Debugger.setBlackboxPatterns({patterns: ['expr\.js']}))
    .then(() => InspectorTest.runTestSuite(testSuite));

var testSuite = [
  function testEntireScript(next) {
    testPositions([position(0, 0)]).then(next);
  },
  function testFooNotBlackboxed(next) {
    testPositions([position(11, 0)]).then(next);
  },
  function testFooBlackboxed(next) {
    testPositions([position(8, 0), position(10, 3)]).then(next);
  },
  function testBooPartiallyBlackboxed1(next) {
    // first line is not blackboxed, second and third - blackboxed.
    testPositions([position(12, 0)]).then(next);
  },
  function testBooPartiallyBlackboxed2(next) {
    // first line is blackboxed, second - not, third - blackboxed.
    testPositions([
      position(11, 0), position(12, 0), position(13, 0)
    ]).then(next);
  },
  function testBooPartiallyBlackboxed3(next) {
    // first line is blackboxed, second and third - not.
    testPositions([
      position(11, 0), position(12, 0), position(14, 0)
    ]).then(next);
  }
];

function testPositions(positions) {
  contextGroup.schedulePauseOnNextStatement('', '');
  return Protocol.Debugger
      .setBlackboxedRanges({scriptId: scriptId, positions: positions})
      .then(InspectorTest.logMessage)
      .then(
          () => Protocol.Runtime.evaluate(
              {expression: 'testFunction()//# sourceURL=expr.js'}));
}

function position(line, column) {
  return {lineNumber: line, columnNumber: column};
}
