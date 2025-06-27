// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks command line API.');

InspectorTest.runAsyncTestSuite([
  async function testKeys() {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: 'keys', includeCommandLineAPI: true}));
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: 'keys({a : 1})', includeCommandLineAPI: true, returnByValue: true}));

    Protocol.Runtime.evaluate({expression: 'this.keys = keys', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: 'this.keys({a : 1})', returnByValue: true}));
  },

  async function testInspect() {
    InspectorTest.log(await Protocol.Runtime.evaluate({expression: 'inspect', includeCommandLineAPI: true}));
    await Protocol.Runtime.enable();
    Protocol.Runtime.onInspectRequested(InspectorTest.logMessage);
    await Protocol.Runtime.evaluate({expression: 'inspect({})', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({expression: 'inspect(239)', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({expression: 'inspect(-0)', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({expression: 'copy(\'hello\')', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: '$0', includeCommandLineAPI: true}));

    Protocol.Runtime.evaluate({expression: 'this.inspect = inspect', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({expression: 'this.inspect({})'});

    Protocol.Runtime.onInspectRequested(null);
    await Protocol.Runtime.disable();
  },

  async function testQueryObjects() {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'queryObjects', includeCommandLineAPI: true}));
    await Protocol.Runtime.enable();
    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({expression: 'Promise.prototype'});
    Protocol.Runtime.evaluate({expression: 'queryObjects(Promise)', includeCommandLineAPI: true});
    let request = await Protocol.Runtime.onceInspectRequested();
    InspectorTest.logMessage(request);
    InspectorTest.logMessage('Is Promise.prototype: ' + await isEqual(objectId, request.params.object.objectId));

    Protocol.Runtime.evaluate({expression: 'queryObjects(Promise.prototype)', includeCommandLineAPI: true});
    request = await Protocol.Runtime.onceInspectRequested();
    InspectorTest.logMessage(request);
    InspectorTest.logMessage('Is Promise.prototype: ' + await isEqual(objectId, request.params.object.objectId));

    ({result:{result:{objectId}}} = await Protocol.Runtime.evaluate({expression:'p = {a:1}'}));
    Protocol.Runtime.evaluate({expression: 'queryObjects(p)', includeCommandLineAPI: true});
    request = await Protocol.Runtime.onceInspectRequested();
    InspectorTest.logMessage(request);
    InspectorTest.logMessage('Is p: ' + await isEqual(objectId, request.params.object.objectId));

    Protocol.Runtime.evaluate({expression: 'queryObjects(1)', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.onceInspectRequested());
    await Protocol.Runtime.disable();
  },

  async function testEvaluationResult() {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));
    await Protocol.Runtime.evaluate({expression: '42', objectGroup: 'console', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));
    await Protocol.Runtime.evaluate({expression: '239', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));
    await Protocol.Runtime.evaluate({expression: '-0', objectGroup: 'console', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true}));
    await Protocol.Runtime.evaluate({expression: '({})', objectGroup: 'console', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: '$_', includeCommandLineAPI: true, returnByValue: true}));
  },

  async function testDebug() {
    session.setupScriptMap();
    await Protocol.Debugger.enable();
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'debug', includeCommandLineAPI: true}));
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'undebug', includeCommandLineAPI: true}));
    await Protocol.Runtime.evaluate({expression: 'function foo() {}'});
    await Protocol.Runtime.evaluate({expression: 'debug(foo)', includeCommandLineAPI: true});
    Protocol.Runtime.evaluate({ expression: 'foo()'});
    let message = await Protocol.Debugger.oncePaused();
    session.logCallFrames(message.params.callFrames);
    InspectorTest.logMessage(message.params.hitBreakpoints);
    InspectorTest.logMessage(message.params.reason);
    await Protocol.Debugger.resume();
    await Protocol.Runtime.evaluate({expression: 'undebug(foo)', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({ expression: 'foo()'});

    Protocol.Runtime.evaluate({
      expression: 'this.debug = debug; this.undebug = undebug;', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({expression: 'this.debug(foo)'});
    Protocol.Runtime.evaluate({ expression: 'foo()'});
    message = await Protocol.Debugger.oncePaused();
    session.logCallFrames(message.params.callFrames);
    InspectorTest.logMessage(message.params.hitBreakpoints);
    InspectorTest.logMessage(message.params.reason);
    await Protocol.Debugger.resume();
    await Protocol.Runtime.evaluate({expression: 'this.undebug(foo)'});
    await Protocol.Runtime.evaluate({expression: 'foo()'});

    // Test builtin.
    await Protocol.Runtime.evaluate({expression: 'function toUpper(x) { return x.toUpperCase() }'});
    await Protocol.Runtime.evaluate({expression: 'debug(String.prototype.toUpperCase)', includeCommandLineAPI: true});
    Protocol.Runtime.evaluate({ expression: 'toUpper("first call")'});
    message = await Protocol.Debugger.oncePaused();
    session.logCallFrames(message.params.callFrames);
    InspectorTest.logMessage(message.params.hitBreakpoints);
    InspectorTest.logMessage(message.params.reason);
    await Protocol.Debugger.resume();
    await Protocol.Runtime.evaluate({expression: 'undebug(String.prototype.toUpperCase)', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({ expression: 'toUpper("second call")'});

    // Test API callback.
    await Protocol.Runtime.evaluate({expression: 'function callSetTimeout() { setTimeout(function(){}, 0) }'});
    await Protocol.Runtime.evaluate({expression: 'debug(setTimeout)', includeCommandLineAPI: true});
    Protocol.Runtime.evaluate({ expression: 'callSetTimeout()'});
    message = await Protocol.Debugger.oncePaused();
    session.logCallFrames(message.params.callFrames);
    InspectorTest.logMessage(message.params.hitBreakpoints);
    InspectorTest.logMessage(message.params.reason);
    let breakpointId = message.params.hitBreakpoints[0];
    await Protocol.Debugger.resume();
    await Protocol.Runtime.evaluate({expression: 'undebug(setTimeout)', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({ expression: 'callSetTimeout()'});

    // Test remove break via protocol.
    await Protocol.Runtime.evaluate({expression: 'function callSetTimeout() { setTimeout(function(){}, 0) }'});
    await Protocol.Runtime.evaluate({expression: 'debug(setTimeout)', includeCommandLineAPI: true});
    Protocol.Runtime.evaluate({ expression: 'callSetTimeout()'});
    message = await Protocol.Debugger.oncePaused();
    session.logCallFrames(message.params.callFrames);
    InspectorTest.logMessage(message.params.hitBreakpoints);
    InspectorTest.logMessage(message.params.reason);
    await Protocol.Debugger.resume();
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Protocol.Runtime.evaluate({ expression: 'callSetTimeout()'});

    // Test condition.
    await Protocol.Runtime.evaluate({expression: 'function fromCharCode(...args) { String.fromCharCode(...args) }'});
    await Protocol.Runtime.evaluate({expression: 'debug(String.fromCharCode, "arguments.length == 3")'});
    Protocol.Runtime.evaluate({ expression: 'fromCharCode("1", "2", "3")'});
    message = await Protocol.Debugger.oncePaused();
    session.logCallFrames(message.params.callFrames);
    InspectorTest.logMessage(message.params.hitBreakpoints);
    InspectorTest.logMessage(message.params.reason);
    await Protocol.Runtime.evaluate({expression: 'undebug(String.fromCharCode)'});
    await Protocol.Runtime.evaluate({ expression: 'fromCharCode()'});

    await Protocol.Debugger.disable();
  },

  async function testMonitor() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();
    Protocol.Runtime.onConsoleAPICalled(message => InspectorTest.log(message.params.args[0].value));
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'monitor', includeCommandLineAPI: true}));
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'unmonitor', includeCommandLineAPI: true}));
    await Protocol.Runtime.evaluate({expression: 'function foo() {}'});

    await Protocol.Runtime.evaluate({expression: 'monitor(foo)', includeCommandLineAPI: true});
    Protocol.Runtime.evaluate({ expression: 'foo(); console.log(\'after first call\')'});
    await Protocol.Runtime.evaluate({expression: 'unmonitor(foo)', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({ expression: 'foo()'});

    Protocol.Runtime.evaluate({
      expression: 'console.log(\'store functions..\'); this.monitor = monitor; this.unmonitor = unmonitor;', includeCommandLineAPI: true});
    await Protocol.Runtime.evaluate({expression: 'this.monitor(foo)'});
    Protocol.Runtime.evaluate({ expression: 'foo(); console.log(\'after first call\')'});
    await Protocol.Runtime.evaluate({expression: 'this.unmonitor(foo)'});
    await Protocol.Runtime.evaluate({ expression: 'foo()'});

    // Test builtin.
    await Protocol.Runtime.evaluate({expression: 'function fromCharCode(...args) { String.fromCharCode(...args) }'});
    await Protocol.Runtime.evaluate({expression: 'monitor(String.fromCharCode)'});
    Protocol.Runtime.evaluate({ expression: 'fromCharCode("1", "2", "3")'});
    await Protocol.Runtime.evaluate({expression: 'unmonitor(String.fromCharCode)'});
    await Protocol.Runtime.evaluate({ expression: 'fromCharCode()'});

    Protocol.Runtime.onConsoleAPICalled(null);
    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  async function testProfile() {
    await Protocol.Profiler.enable();
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'profile', includeCommandLineAPI: true}));
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'profileEnd', includeCommandLineAPI: true}));

    Protocol.Runtime.evaluate({expression: 'profile(42)', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Profiler.onceConsoleProfileStarted());
    Protocol.Runtime.evaluate({expression: 'profileEnd(42)', includeCommandLineAPI: true});
    let message = await Protocol.Profiler.onceConsoleProfileFinished();
    message.params.profile = '<profile>';
    InspectorTest.logMessage(message);

    Protocol.Runtime.evaluate({
      expression: 'this.profile = profile; this.profileEnd = profileEnd;', includeCommandLineAPI: true});
    Protocol.Runtime.evaluate({expression: 'this.profile(239)'});
    InspectorTest.logMessage(await Protocol.Profiler.onceConsoleProfileStarted());
    Protocol.Runtime.evaluate({expression: 'this.profileEnd(239)'});
    message = await Protocol.Profiler.onceConsoleProfileFinished();
    message.params.profile = '<profile>';
    InspectorTest.logMessage(message);

    await Protocol.Profiler.disable();
  },

  async function testDir() {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'dir', includeCommandLineAPI: true}));

    await Protocol.Runtime.enable();
    Protocol.Runtime.evaluate({expression: 'dir({})', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    Protocol.Runtime.evaluate({expression: 'dir(42)', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

    Protocol.Runtime.evaluate({expression: 'this.dir = dir', includeCommandLineAPI: true});
    Protocol.Runtime.evaluate({expression: 'this.dir({})'});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    await Protocol.Runtime.disable();
  },

  async function testDirXML() {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'dirxml', includeCommandLineAPI: true}));

    await Protocol.Runtime.enable();
    Protocol.Runtime.evaluate({expression: 'dirxml({})', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    Protocol.Runtime.evaluate({expression: 'dirxml(42)', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    await Protocol.Runtime.disable();
  },

  async function testTable() {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'table', includeCommandLineAPI: true}));

    await Protocol.Runtime.enable();
    Protocol.Runtime.evaluate({expression: 'table({})', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    Protocol.Runtime.evaluate({expression: 'table(42)', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    await Protocol.Runtime.disable();
  },

  async function testClear() {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({expression: 'clear', includeCommandLineAPI: true}));

    await Protocol.Runtime.enable();
    Protocol.Runtime.evaluate({expression: 'clear()', includeCommandLineAPI: true});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

    Protocol.Runtime.evaluate({expression: 'this.clear = clear', includeCommandLineAPI: true});
    Protocol.Runtime.evaluate({expression: 'this.clear()'});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    await Protocol.Runtime.disable();
  }
]);

async function isEqual(objectId1, objectId2) {
  return (await Protocol.Runtime.callFunctionOn({
    objectId: objectId1,
    functionDeclaration: 'function(arg){return this === arg;}',
    returnByValue: true,
    arguments: [{objectId: objectId2}]
  })).result.result.value;
}
