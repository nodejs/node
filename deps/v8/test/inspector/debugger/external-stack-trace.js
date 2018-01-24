// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests external stack traces');

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

function started(id) {
  inspector.externalAsyncTaskStarted(Int32Array.from(JSON.parse(id)).buffer);
}

function finished(id) {
  inspector.externalAsyncTaskFinished(Int32Array.from(JSON.parse(id)).buffer);
}
//# sourceURL=utils.js`;

contextGroup1.addScript(utilsScript);
contextGroup2.addScript(utilsScript);

InspectorTest.runAsyncTestSuite([
  async function testDebuggerId() {
    InspectorTest.log('Enabling debugger first time..');
    let {result: {debuggerId}} = await Protocol1.Debugger.enable();
    let firstDebuggerId = debuggerId;
    InspectorTest.log('Enabling debugger again..');
    ({result: {debuggerId}} = await Protocol1.Debugger.enable());
    if (firstDebuggerId !== debuggerId) {
      InspectorTest.log(
          'FAIL: second Debugger.enable returns different debugger id');
    } else {
      InspectorTest.log(
          '> second Debugger.enable returns the same debugger id');
    }
    InspectorTest.log('Enabling debugger in another context group..');
    ({result: {debuggerId}} = await Protocol2.Debugger.enable());
    if (firstDebuggerId === debuggerId) {
      InspectorTest.log(
          'FAIL: Debugger.enable in another context group returns the same debugger id');
    } else {
      InspectorTest.log(
          '> Debugger.enable in another context group returns own debugger id');
    }
  },

  async function testInstrumentation() {
    Protocol1.Debugger.enable();
    Protocol1.Debugger.setAsyncCallStackDepth({maxDepth: 32});
    let result = await Protocol1.Runtime.evaluate(
        {expression: 'id = inspector.storeCurrentStackTrace(\'stack\')'});
    let stackTraceId = result.result.result.objectId;
    Protocol1.Runtime.evaluate({
      expression: `inspector.externalAsyncTaskStarted(id);
      debugger;
      inspector.externalAsyncTaskFinished(id);`
    });
    let {params: {callFrames, asyncStackTraceId}} =
        await Protocol1.Debugger.oncePaused();
    result = await Protocol1.Debugger.getStackTrace(
        {stackTraceId: asyncStackTraceId});
    InspectorTest.logMessage(result);
    await Protocol1.Debugger.disable();
  },

  async function testDisableStacksAfterStored() {
    Protocol1.Debugger.enable();
    Protocol1.Debugger.setAsyncCallStackDepth({maxDepth: 32});
    let result = await Protocol1.Runtime.evaluate(
        {expression: 'id = inspector.storeCurrentStackTrace(\'stack\')'});
    let stackTraceId = result.result.result.objectId;
    Protocol1.Debugger.setAsyncCallStackDepth({maxDepth: 0});
    Protocol1.Runtime.evaluate({
      expression: `inspector.externalAsyncTaskStarted(id);
      debugger;
      inspector.externalAsyncTaskFinished(id);`
    });
    let {params: {callFrames, asyncStackTraceId}} =
        await Protocol1.Debugger.oncePaused();
    if (!asyncStackTraceId) {
      InspectorTest.log('> external async stack trace is empty');
    } else {
      InspectorTest.log('FAIL: external async stack trace is reported');
    }
    await Protocol1.Debugger.disable();
  },

  async function testDisableStacksAfterStarted() {
    Protocol1.Debugger.enable();
    Protocol1.Debugger.setAsyncCallStackDepth({maxDepth: 32});
    let result = await Protocol1.Runtime.evaluate(
        {expression: 'id = inspector.storeCurrentStackTrace(\'stack\')'});
    let stackTraceId = result.result.result.objectId;
    Protocol1.Runtime.evaluate(
        {expression: 'inspector.externalAsyncTaskStarted(id);'});
    Protocol1.Debugger.setAsyncCallStackDepth({maxDepth: 0});
    Protocol1.Runtime.evaluate({
      expression: `debugger;
      inspector.externalAsyncTaskFinished(id);`
    });
    let {params: {callFrames, asyncStackTraceId}} =
        await Protocol1.Debugger.oncePaused();
    if (!asyncStackTraceId) {
      InspectorTest.log('> external async stack trace is empty');
    } else {
      InspectorTest.log('FAIL: external async stack trace is reported');
    }
    await Protocol1.Debugger.disable();
  },

  async function testExternalStacks() {

    let debuggerId1 = (await Protocol1.Debugger.enable()).result.debuggerId;
    let debuggerId2 = (await Protocol2.Debugger.enable()).result.debuggerId;
    Protocol1.Debugger.setAsyncCallStackDepth({maxDepth: 32});
    Protocol2.Debugger.setAsyncCallStackDepth({maxDepth: 32});
    let stackTraceId1 = (await Protocol1.Runtime.evaluate({
                          expression: 'store(\'stack\')//# sourceURL=expr1-1.js'
                        })).result.result.value;
    let stackTraceId2 = (await Protocol2.Runtime.evaluate({
                          expression: `started('${stackTraceId1}');
      id = store('stack2');
      finished('${stackTraceId1}');
      id
      //# sourceURL=expr2.js`
                        })).result.result.value;
    Protocol1.Runtime.evaluate({
      expression: `started('${stackTraceId2}');
      debugger;
      finished('${stackTraceId2}');
      id
      //# sourceURL=expr1-2.js`
    });

    let {params: {callFrames, asyncStackTraceId}} =
        await Protocol1.Debugger.oncePaused();
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

    Protocol1.Debugger.disable();
    await Protocol2.Debugger.disable();
  }
]);
