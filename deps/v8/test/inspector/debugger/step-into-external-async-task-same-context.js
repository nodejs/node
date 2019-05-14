// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test for step-into remote async task.');

contextGroup.addScript(`
function store(description) {
  let buffer = inspector.storeCurrentStackTrace(description);
  return '[' + new Int32Array(buffer).join(',') + ']';
}
//# sourceURL=utils.js`);

contextGroup.addScript(`
function call(id, f) {
  inspector.externalAsyncTaskStarted(Int32Array.from(JSON.parse(id)).buffer);
  f();
  inspector.externalAsyncTaskFinished(Int32Array.from(JSON.parse(id)).buffer);
}
//# sourceURL=framework.js`);

session.setupScriptMap();

(async function test() {
  InspectorTest.log('Setup debugger agents..');
  let debuggerId = (await Protocol.Debugger.enable()).result.debuggerId;

  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});
  Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});

  InspectorTest.log('Pause before stack trace is captured..');
  Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 2, columnNumber: 25, url: 'utils.js'});
  let evaluatePromise = Protocol.Runtime.evaluate({
    expression: `(function foo() {
      return store('remote-task');
    })()
    //# sourceURL=source.js`
  });
  await Protocol.Debugger.oncePaused();

  InspectorTest.log('Run stepInto with breakOnAsyncCall flag');
  Protocol.Debugger.stepInto({breakOnAsyncCall: true});
  let {params: {asyncCallStackTraceId}} = await Protocol.Debugger.oncePaused();

  InspectorTest.log('Call pauseOnAsyncCall');
  Protocol.Debugger.pauseOnAsyncCall({
    parentStackTraceId: asyncCallStackTraceId,
  });
  Protocol.Debugger.resume();

  InspectorTest.log('Trigger external async task on another context group');
  let stackTraceId = (await evaluatePromise).result.result.value;
  Protocol.Runtime.evaluate({
    expression: `call('${stackTraceId}',
      function boo() {})
    //# sourceURL=target.js`
  });

  InspectorTest.log('Dump stack trace');
  let {params: {callFrames, asyncStackTraceId}} =
      await Protocol.Debugger.oncePaused();
  while (true) {
    session.logCallFrames(callFrames);
    if (asyncStackTraceId) {
      let {result: {stackTrace}} = await Protocol.Debugger.getStackTrace(
          {stackTraceId: asyncStackTraceId});
      InspectorTest.log(`-- ${stackTrace.description} --`);
      callFrames = stackTrace.callFrames;
      asyncStackTraceId = stackTrace.parentId;
    } else {
      break;
    }
  }

  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 0});
  await Protocol.Debugger.disable();

  InspectorTest.completeTest();
})()
