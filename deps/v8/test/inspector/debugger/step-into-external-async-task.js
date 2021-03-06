// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Test for step-into remote async task');

let contextGroup1 = new InspectorTest.ContextGroup();
let session1 = contextGroup1.connect();
let Protocol1 = session1.Protocol;
let contextGroup2 = new InspectorTest.ContextGroup();
let session2 = contextGroup2.connect();
let Protocol2 = session2.Protocol;

let utilsScript = `
function store(description) {
  let buffer = inspector.storeCurrentStackTrace(description);
  return '[' + new Int32Array(buffer).join(',') + ']';
}
//# sourceURL=utils.js`;

// TODO(rmcilroy): This has to be in this order since the i::Script object gets
// reused via the CompilationCache, and we want OnAfterCompile to be called
// for contextGroup1 last on this script.
contextGroup2.addScript(utilsScript);
contextGroup1.addScript(utilsScript);

let frameworkScript = `
function call(id, f) {
  inspector.externalAsyncTaskStarted(Int32Array.from(JSON.parse(id)).buffer);
  f();
  inspector.externalAsyncTaskFinished(Int32Array.from(JSON.parse(id)).buffer);
}
//# sourceURL=framework.js`;

contextGroup1.addScript(frameworkScript);
contextGroup2.addScript(frameworkScript);

session1.setupScriptMap();
session2.setupScriptMap();

(async function test() {
  InspectorTest.log('Setup debugger agents..');
  let debuggerId1 = (await Protocol1.Debugger.enable()).result.debuggerId;
  let debuggerId2 = (await Protocol2.Debugger.enable()).result.debuggerId;

  Protocol1.Debugger.setAsyncCallStackDepth({maxDepth: 128});
  Protocol2.Debugger.setAsyncCallStackDepth({maxDepth: 128});

  Protocol1.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});
  Protocol2.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});

  InspectorTest.log('Pause before stack trace is captured..');
  Protocol1.Debugger.setBreakpointByUrl(
      {lineNumber: 2, columnNumber: 25, url: 'utils.js'});
  let evaluatePromise = Protocol1.Runtime.evaluate({
    expression: `(function foo() {
      return store('remote-task');
    })()
    //# sourceURL=source.js`
  });
  await Protocol1.Debugger.oncePaused();

  InspectorTest.log('Run stepInto with breakOnAsyncCall flag');
  Protocol1.Debugger.stepInto({breakOnAsyncCall: true});

  InspectorTest.log('Trigger external async task on another context group');
  let stackTraceId = (await evaluatePromise).result.result.value;
  Protocol2.Runtime.evaluate({
    expression: `call('${stackTraceId}',
      function boo() {})
    //# sourceURL=target.js`
  });

  InspectorTest.log('Dump stack trace');
  let {params: {callFrames, asyncStackTraceId}} =
      await Protocol2.Debugger.oncePaused();
  let debuggers = new Map(
      [[debuggerId1, Protocol1.Debugger], [debuggerId2, Protocol2.Debugger]]);
  let sessions = new Map([[debuggerId1, session1], [debuggerId2, session2]]);
  let currentDebuggerId = debuggerId1;
  while (true) {
    sessions.get(currentDebuggerId).logCallFrames(callFrames);
    if (asyncStackTraceId) {
      currentDebuggerId = asyncStackTraceId.debuggerId;
      let {result: {stackTrace}} =
          await debuggers.get(currentDebuggerId).getStackTrace({
            stackTraceId: asyncStackTraceId
          });
      InspectorTest.log(`-- ${stackTrace.description} --`);
      callFrames = stackTrace.callFrames;
      asyncStackTraceId = stackTrace.parentId;
    } else {
      break;
    }
  }

  Protocol1.Debugger.setAsyncCallStackDepth({maxDepth: 0});
  Protocol2.Debugger.setAsyncCallStackDepth({maxDepth: 0});
  await Protocol1.Debugger.disable();
  await Protocol2.Debugger.disable();

  InspectorTest.completeTest();
})()
