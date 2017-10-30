// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks stepping with blackboxed frames on stack');

contextGroup.addScript(
    `
function frameworkCall(funcs) {
  for (var f of funcs) f();
}

function frameworkBreakAndCall(funcs) {
  inspector.breakProgram('', '');
  for (var f of funcs) f();
}
//# sourceURL=framework.js`,
    8, 4);

contextGroup.addScript(
    `
function userFoo() {
  return 1;
}

function userBoo() {
  return 2;
}

function testStepFromUser() {
  frameworkCall([userFoo, userBoo])
}

function testStepFromFramework() {
  frameworkBreakAndCall([userFoo, userBoo]);
}
//# sourceURL=user.js`,
    21, 4);

session.setupScriptMap();

Protocol.Debugger.enable()
    .then(
        () => Protocol.Debugger.setBlackboxPatterns(
            {patterns: ['framework\.js']}))
    .then(() => InspectorTest.runTestSuite(testSuite));

var testSuite = [
  function testStepIntoFromUser(next) {
    contextGroup.schedulePauseOnNextStatement('', '');
    test('testStepFromUser()', [
      'print',                          // before testStepFromUser call
      'stepInto', 'stepInto', 'print',  // userFoo
      'stepInto', 'stepInto', 'print',  // userBoo
      'stepInto', 'stepInto', 'print'   // testStepFromUser
    ]).then(next);
  },

  function testStepOverFromUser(next) {
    contextGroup.schedulePauseOnNextStatement('', '');
    test('testStepFromUser()', [
      'print',                          // before testStepFromUser call
      'stepInto', 'stepInto', 'print',  // userFoo
      'stepOver', 'stepOver', 'print',  // userBoo
      'stepOver', 'stepOver', 'print'   // testStepFromUser
    ]).then(next);
  },

  function testStepOutFromUser(next) {
    contextGroup.schedulePauseOnNextStatement('', '');
    test('testStepFromUser()', [
      'print',                          // before testStepFromUser call
      'stepInto', 'stepInto', 'print',  // userFoo
      'stepOut', 'print'                // testStepFromUser
    ]).then(next);
  },

  function testStepIntoFromFramework(next) {
    test('testStepFromFramework()', [
      'print',              // frameworkBreakAndCall
      'stepInto', 'print',  // userFoo
    ]).then(next);
  },

  function testStepOverFromFramework(next) {
    test('testStepFromFramework()', [
      'print',              // frameworkBreakAndCall
      'stepOver', 'print',  // testStepFromFramework
    ]).then(next);
  },

  function testStepOutFromFramework(next) {
    test('testStepFromFramework()', [
      'print',             // frameworkBreakAndCall
      'stepOut', 'print',  // testStepFromFramework
    ]).then(next);
  }
];

function test(entryExpression, actions) {
  Protocol.Debugger.onPaused(message => {
    var action = actions.shift() || 'resume';
    if (action === 'print') {
      session.logCallFrames(message.params.callFrames);
      InspectorTest.log('');
      action = actions.shift() || 'resume';
    }
    if (action) InspectorTest.log(`Executing ${action}...`);
    Protocol.Debugger[action]();
  });
  return Protocol.Runtime.evaluate(
      {expression: entryExpression + '//# sourceURL=expr.js'});
}
