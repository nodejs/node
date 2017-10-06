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
    await Protocol.Debugger.resume();
    await Protocol.Runtime.evaluate({expression: 'this.undebug(foo)'});
    await Protocol.Runtime.evaluate({expression: 'foo()'});

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
