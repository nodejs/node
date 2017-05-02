// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax

print('Checks that breaks in framework code correctly processed.');

InspectorTest.addScript(`
function frameworkAssert() {
  console.assert(false);
}

function throwCaughtError() {
  try {
    throw new Error();
  } catch (e) {
  }
}

function throwUncaughtError() {
  throw new Error();
}

function breakpoint() {
  return 239;
}

function debuggerStatement() {
  debugger;
}

function syncDOMBreakpoint() {
  breakProgram('', '');
}

function asyncDOMBreakpoint() {
  return 42;
}

function throwCaughtSyntaxError() {
  try {
    eval('}');
  } catch (e) {
  }
}

function throwFromJSONParse() {
  try {
    JSON.parse('ping');
  } catch (e) {
  }
}

function throwInlinedUncaughtError() {
  function inlinedWrapper() {
    throwUserException();
  }
  %OptimizeFunctionOnNextCall(inlinedWrapper);
  inlinedWrapper();
}

function syncDOMBreakpointWithInlinedUserFrame() {
  function inlinedWrapper() {
    userFunction();
  }
  %OptimizeFunctionOnNextCall(inlinedWrapper);
  inlinedWrapper();
}

//# sourceURL=framework.js`, 8, 26);

InspectorTest.addScript(`
function throwUserException() {
  throw new Error();
}

function userFunction() {
  syncDOMBreakpoint();
}

//# sourceURL=user.js`, 64, 26)

InspectorTest.setupScriptMap();
Protocol.Debugger.onPaused(message => {
  InspectorTest.logCallFrames(message.params.callFrames);
  InspectorTest.log('');
  Protocol.Debugger.resume();
});

Protocol.Debugger.enable();
Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});

InspectorTest.runTestSuite([
  function testConsoleAssert(next) {
    Protocol.Debugger.setPauseOnExceptions({state: 'all'})
        .then(() => InspectorTest.log('> all frames in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'frameworkAssert()//# sourceURL=framework.js'}))
        .then(() => InspectorTest.log('> mixed, top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'frameworkAssert()//# sourceURL=user.js'}))
        .then(() => Protocol.Debugger.setPauseOnExceptions({state: 'none'}))
        .then(next);
  },

  function testCaughtException(next) {
    Protocol.Debugger.setPauseOnExceptions({state: 'all'})
        .then(() => InspectorTest.log('> all frames in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'throwCaughtError()//# sourceURL=framework.js'}))
        .then(() => InspectorTest.log('> mixed, top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'throwCaughtError()//# sourceURL=user.js'}))
        .then(() => Protocol.Debugger.setPauseOnExceptions({state: 'none'}))
        .then(next);
  },

  function testUncaughtException(next) {
    Protocol.Debugger.setPauseOnExceptions({state: 'all'})
        .then(() => InspectorTest.log('> all frames in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'setTimeout(\'throwUncaughtError()//# sourceURL=framework.js\', 0)//# sourceURL=framework.js'}))
        .then(() => Protocol.Runtime.evaluate({ expression: "new Promise(resolve => setTimeout(resolve, 0))", awaitPromise: true}))
        .then(() => InspectorTest.log('> mixed, top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'setTimeout(\'throwUncaughtError()//# sourceURL=user.js\', 0)'}))
        .then(() => Protocol.Runtime.evaluate({ expression: "new Promise(resolve => setTimeout(resolve, 0))", awaitPromise: true}))
        .then(() => Protocol.Debugger.setPauseOnExceptions({state: 'none'}))
        .then(next);
  },

  function testUncaughtExceptionWithInlinedFrame(next) {
    Protocol.Debugger.setPauseOnExceptions({state: 'all'})
        .then(() => InspectorTest.log('> mixed top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'setTimeout(\'throwInlinedUncaughtError()//# sourceURL=framework.js\', 0)//# sourceURL=framework.js'}))
        .then(() => Protocol.Runtime.evaluate({ expression: "new Promise(resolve => setTimeout(resolve, 0))", awaitPromise: true}))
        .then(next);
  },

  function testBreakpoint(next) {
    Protocol.Debugger.setBreakpointByUrl({lineNumber: 25, url: 'framework.js'})
        .then(() => InspectorTest.log('> all frames in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'breakpoint()//# sourceURL=framework.js'}))
        .then(() => InspectorTest.log('> mixed, top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'breakpoint()//# sourceURL=user.js'}))
        .then(next);
  },

  function testDebuggerStatement(next) {
    InspectorTest.log('> all frames in framework:');
    Protocol.Runtime
        .evaluate({expression: 'debuggerStatement()//# sourceURL=framework.js'})
        .then(() => InspectorTest.log('> mixed, top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'debuggerStatement()//# sourceURL=user.js'}))
        .then(next);
  },

  function testSyncDOMBreakpoint(next) {
    InspectorTest.log('> all frames in framework:');
    Protocol.Runtime
        .evaluate({expression: 'syncDOMBreakpoint()//# sourceURL=framework.js'})
        .then(() => InspectorTest.log('> mixed, top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'syncDOMBreakpoint()//# sourceURL=user.js'}))
        .then(next);
  },

  function testSyncDOMBreakpointWithInlinedUserFrame(next) {
    InspectorTest.log('> mixed, top frame in framework:');
    Protocol.Runtime
        .evaluate({expression: 'syncDOMBreakpointWithInlinedUserFrame()//# sourceURL=framework.js'})
        .then(next);
  },

  function testAsyncDOMBreakpoint(next) {
    schedulePauseOnNextStatement('', '');
    InspectorTest.log('> all frames in framework:');
    Protocol.Runtime
        .evaluate(
            {expression: 'asyncDOMBreakpoint()//# sourceURL=framework.js'})
        .then(() => cancelPauseOnNextStatement())
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: '42//# sourceURL=user.js'}))
        .then(() => schedulePauseOnNextStatement('', ''))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'asyncDOMBreakpoint()//# sourceURL=user.js'}))
        .then(next);
  },

  function testCaughtSyntaxError(next) {
    Protocol.Debugger.setPauseOnExceptions({state: 'all'})
        .then(() => InspectorTest.log('> all frames in framework:'))
        .then(() => Protocol.Runtime.evaluate({
          expression: 'throwCaughtSyntaxError()//# sourceURL=framework.js'
        }))
        .then(() => InspectorTest.log('> mixed, top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'throwCaughtSyntaxError()//# sourceURL=user.js'}))
        .then(() => Protocol.Debugger.setPauseOnExceptions({state: 'none'}))
        .then(next);
  },

  function testCaughtJSONParseError(next) {
    Protocol.Debugger.setPauseOnExceptions({state: 'all'})
        .then(() => InspectorTest.log('> all frames in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'throwFromJSONParse()//# sourceURL=framework.js'}))
        .then(() => InspectorTest.log('> mixed, top frame in framework:'))
        .then(
            () => Protocol.Runtime.evaluate(
                {expression: 'throwFromJSONParse()//# sourceURL=user.js'}))
        .then(() => Protocol.Debugger.setPauseOnExceptions({state: 'none'}))
        .then(next);
  }
]);
