// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks V8InspectorSession::stop');

InspectorTest.runAsyncTestSuite([
  async function testSessionStopResumesPause() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    Protocol.Debugger.enable();
    await Protocol.Debugger.pause();
    const result = Protocol.Runtime.evaluate({expression: '42'});
    session.stop();
    InspectorTest.log(
        `Evaluation returned: ${(await result).result.result.value}`);
  },
  async function testSessionStopResumesInstrumentationPause() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    Protocol.Debugger.enable();
    await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    const paused = Protocol.Debugger.oncePaused();
    const result = Protocol.Runtime.evaluate({expression: '42'});
    InspectorTest.log(`Paused: ${(await paused).params.reason}`);
    session.stop();
    InspectorTest.log(
        `Evaluation returned: ${(await result).result.result.value}`);
  },
  async function testSessionStopDisablesDebugger() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    await Protocol.Debugger.enable();
    session.stop();
    const pauseResult = await Protocol.Debugger.pause();
    InspectorTest.log(`Pause error(?): ${pauseResult?.error?.message}`);
  },
  async function testSessionStopDisallowsReenabling() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    await Protocol.Debugger.enable();
    session.stop();
    const pauseResultAfterStop = await Protocol.Debugger.pause();
    InspectorTest.log(
        `Pause error(?) after stop: ${pauseResultAfterStop?.error?.message}`);
    await Protocol.Debugger.enable();
    const pauseResult = await Protocol.Debugger.pause();
    InspectorTest.log(
        `Pause error(?) after re-enable: ${pauseResult?.error?.message}`);
  },
  async function testSessionStopDoesNotDisableOtherSessions() {
    let contextGroup = new InspectorTest.ContextGroup();

    let session1 = contextGroup.connect();
    let Protocol1 = session1.Protocol;
    await Protocol1.Debugger.enable();

    let session2 = contextGroup.connect();
    let Protocol2 = session2.Protocol;
    await Protocol2.Debugger.enable();

    session1.stop();
    const pauseResult1 = await Protocol1.Debugger.pause();
    InspectorTest.log(
        `Session 1 pause error after stop: ${pauseResult1?.error?.message}`);

    await Protocol2.Debugger.pause();

    const paused = Protocol2.Debugger.oncePaused();
    const result = Protocol2.Runtime.evaluate({expression: '42'});
    InspectorTest.log(`Session 2 paused: ${(await paused).params.reason}`);
    await Protocol2.Debugger.resume();

    InspectorTest.log(
        `Session 2 evaluation: ${(await result).result.result.value}`);
  },
]);
