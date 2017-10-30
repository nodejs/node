// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Check that inspector correctly change break on exception state.");

contextGroup.addScript(`
function scheduleUncaughtException()
{
  setTimeout(throwUncaughtException, 0);
}
function throwUncaughtException()
{
  throw new Error();
}
function throwCaughtException()
{
  throw new Error();
}`);

Protocol.Debugger.onPaused(message => {
  InspectorTest.log("paused in " + message.params.callFrames[0].functionName);
  Protocol.Debugger.resume();
});

Protocol.Runtime.enable();

InspectorTest.runTestSuite([
  function noBreakOnExceptionAfterEnabled(next)
  {
    Protocol.Debugger.enable();
    Protocol.Debugger.setPauseOnExceptions({ state: "all" });
    Protocol.Debugger.disable();
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({ expression: "scheduleUncaughtException()" })
      .then(() => Protocol.Runtime.evaluate({ expression: "throwCaughtException()" }))
      .then(() => Protocol.Debugger.disable())
      .then(next);
  },

  function breakOnUncaughtException(next)
  {
    Protocol.Debugger.enable();
    Protocol.Debugger.setPauseOnExceptions({ state: "uncaught" });
    Protocol.Runtime.evaluate({ expression: "scheduleUncaughtException()" })
      .then(() => Protocol.Runtime.onceExceptionThrown())
      .then(() => Protocol.Runtime.evaluate({ expression: "throwCaughtException()" }))
      .then(() => Protocol.Debugger.disable())
      .then(next);
  },

  function breakOnCaughtException(next)
  {
    Protocol.Debugger.enable();
    Protocol.Debugger.setPauseOnExceptions({ state: "all" });
    Protocol.Runtime.evaluate({ expression: "scheduleUncaughtException()" })
      .then(() => Protocol.Runtime.onceExceptionThrown())
      .then(() => Protocol.Runtime.evaluate({ expression: "throwCaughtException()" }))
      .then(() => Protocol.Debugger.disable())
      .then(next);
  },

  function noBreakInEvaluateInSilentMode(next)
  {
    Protocol.Debugger.enable();
    Protocol.Debugger.setPauseOnExceptions({ state: "all" })
      .then(() => Protocol.Runtime.evaluate({ expression: "throwCaughtException()", silent: true }))
      .then(() => Protocol.Debugger.disable())
      .then(next);
  }
]);
