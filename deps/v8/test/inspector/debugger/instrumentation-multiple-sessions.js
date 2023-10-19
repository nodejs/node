// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks instrumentation pause with multiple sessions');

InspectorTest.runAsyncTestSuite([
  async function testTwoInstrumentationBreaksResume() {
    // Initialize two sessions with instrumentation breakpoints.
    let contextGroup = new InspectorTest.ContextGroup();
    let session1 = contextGroup.connect();
    let Protocol1 = session1.Protocol;
    Protocol1.Debugger.enable();
    await Protocol1.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    const session2 = contextGroup.connect();
    const Protocol2 = session2.Protocol;
    await Protocol2.Debugger.enable();
    await Protocol2.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    InspectorTest.log('Created two sessions.');

    // Expect both sessions pausing on instrumentation breakpoint.
    const paused1 = Protocol1.Debugger.oncePaused();
    const paused2 = Protocol2.Debugger.oncePaused();
    const evaluationFinished =
        Protocol1.Runtime.evaluate({expression: '42'})
            .then(
                r => InspectorTest.log(
                    `Evaluation result: ${r.result.result.value}`));

    // Verify the instrumentation breakpoint puased the sessions.
    InspectorTest.log(`Paused 1: ${(await paused1).params.reason}`);
    InspectorTest.log(`Paused 2: ${(await paused2).params.reason}`);

    // Let us call resume in the first session and make sure that this
    // does not resume the instrumentation pause (the instrumentation
    // pause should only resume once all sessions ask for resumption).
    //
    // Unfortunately, we cannot check for absence of resumptions, so
    // let's just give the evaluation chance to finish early by calling
    // 'resume' on the first session multiple times.
    for (let i = 0; i < 20; i++) {
      await Protocol1.Debugger.resume();
    }
    InspectorTest.log('Resumed session 1');

    // Resuming the second session should allow the evaluation to
    // finish.
    await Protocol2.Debugger.resume();
    InspectorTest.log('Resumed session 2');

    await evaluationFinished;
    InspectorTest.log('Evaluation finished');
  },
  async function testInstrumentedSessionNotification() {
    // Initialize two debugger sessions - one with instrumentation
    // breakpoints, one without.
    let contextGroup = new InspectorTest.ContextGroup();
    let session1 = contextGroup.connect();
    let Protocol1 = session1.Protocol;
    Protocol1.Debugger.enable();
    await Protocol1.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    const session2 = contextGroup.connect();
    const Protocol2 = session2.Protocol;
    await Protocol2.Debugger.enable();
    InspectorTest.log('Created two sessions.');

    // Verify that the instrumented session sees the instrumentation pause.
    const paused1 = Protocol1.Debugger.oncePaused();
    const paused2 = Protocol2.Debugger.oncePaused();
    const evaluationFinished =
        Protocol1.Runtime.evaluate({expression: '42'})
            .then(
                r => InspectorTest.log(
                    `Evaluation result: ${r.result.result.value}`));
    InspectorTest.log(`Session 1 paused (${(await paused1).params.reason})`);
    InspectorTest.log(`Session 2 paused (${(await paused2).params.reason})`);

    const onResume1 = Protocol1.Debugger.onceResumed();
    const onResume2 = Protocol2.Debugger.onceResumed();
    await Protocol1.Debugger.resume();
    await onResume1;
    InspectorTest.log('Resumed session 1');
    await onResume2;
    InspectorTest.log('Resumed session 2');

    await evaluationFinished;
    InspectorTest.log('Evaluation finished');
  },
  async function testNonInstrumentedSessionCannotsResumeInstrumentationPause() {
    // Initialize two debugger sessions - one with instrumentation
    // breakpoints, one without.
    let contextGroup = new InspectorTest.ContextGroup();
    let session1 = contextGroup.connect();
    let Protocol1 = session1.Protocol;
    Protocol1.Debugger.enable();
    await Protocol1.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    const session2 = contextGroup.connect();
    const Protocol2 = session2.Protocol;
    await Protocol2.Debugger.enable();
    InspectorTest.log('Created two sessions.');

    // Make sure the non-instrumentation session does not pause or resume on
    // instrumentation.
    Protocol2.Debugger.onResumed(
        m => InspectorTest.log('[Unexpected] Session 2 resumed'));

    // Induce instrumentation pause.
    const paused1 = Protocol1.Debugger.oncePaused();
    const paused2 = Protocol2.Debugger.oncePaused();
    const evaluationFinished =
        Protocol1.Runtime.evaluate({expression: '42'})
            .then(
                r => InspectorTest.log(
                    `Evaluation result: ${r.result.result.value}`));
    InspectorTest.log(`Session 1 paused (${(await paused1).params.reason})`);
    InspectorTest.log(`Session 2 paused (${(await paused2).params.reason})`);

    // Calling 'resume' on the non-instrumented session should not have any
    // effect on the session in the instrumentation pause.
    for (let i = 0; i < 10; i++) {
      await Protocol2.Debugger.resume();
    }
    InspectorTest.log('Called "resume" on session 2');

    const onResume1 = Protocol1.Debugger.onceResumed();
    const onResume2 = Protocol2.Debugger.onceResumed();
    await Protocol1.Debugger.resume();
    InspectorTest.log('Called "resume" on session 1');
    await onResume1;
    InspectorTest.log('Resumed session 1');
    await onResume2;
    InspectorTest.log('Resumed session 2');

    await evaluationFinished;
    InspectorTest.log('Evaluation finished');
  },
  async function testEvaluationFromNonInstrumentedSession() {
    // Initialize two debugger sessions - one with instrumentation
    // breakpoints, one without.
    let contextGroup = new InspectorTest.ContextGroup();
    let session1 = contextGroup.connect();
    let Protocol1 = session1.Protocol;
    Protocol1.Debugger.enable();
    await Protocol1.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    const session2 = contextGroup.connect();
    const Protocol2 = session2.Protocol;
    await Protocol2.Debugger.enable();
    InspectorTest.log('Created two sessions.');

    // Start evaluation in the non-instrumentation session and expect that
    // the instrumentation session is paused.
    const paused1 = Protocol1.Debugger.oncePaused();
    const paused2 = Protocol2.Debugger.oncePaused();
    const evaluationFinished =
        Protocol2.Runtime.evaluate({expression: '42'})
            .then(
                r => InspectorTest.log(
                    `Evaluation result: ${r.result.result.value}`));
    InspectorTest.log(`Session 1 paused (${(await paused1).params.reason})`);
    InspectorTest.log(`Session 2 paused (${(await paused2).params.reason})`);

    const onResume1 = Protocol1.Debugger.onceResumed();
    const onResume2 = Protocol2.Debugger.onceResumed();
    await Protocol1.Debugger.resume();
    InspectorTest.log('Called "resume" on session 1');
    await onResume1;
    InspectorTest.log('Resumed session 1');
    await onResume2;
    InspectorTest.log('Resumed session 2');

    await evaluationFinished;
    InspectorTest.log('Evaluation finished');
  },
  async function
      testTransparentEvaluationFromNonInstrumentedSessionDuringPause() {
        // Initialize two debugger sessions - one with instrumentation
        // breakpoints, one without.
        let contextGroup = new InspectorTest.ContextGroup();
        let session1 = contextGroup.connect();
        let Protocol1 = session1.Protocol;
        Protocol1.Debugger.enable();
        await Protocol1.Debugger.setInstrumentationBreakpoint(
            {instrumentation: 'beforeScriptExecution'});
        const session2 = contextGroup.connect();
        const Protocol2 = session2.Protocol;
        await Protocol2.Debugger.enable();
        InspectorTest.log('Created two sessions.');

        // Enter instrumentation pause.
        const paused1 = Protocol1.Debugger.oncePaused();
        const paused2 = Protocol2.Debugger.oncePaused();
        Protocol1.Runtime.evaluate({expression: 'null'})
        InspectorTest.log(
            `Session 1 paused (${(await paused1).params.reason})`);
        InspectorTest.log(
            `Session 2 paused (${(await paused2).params.reason})`);

        // Start evaluation in session 2.
        const evaluation = Protocol2.Runtime.evaluate({expression: '42'});

        await Protocol1.Debugger.resume();
        InspectorTest.log('Resumed session 1');

        // Make sure the evaluation finished.
        InspectorTest.log(`Session 2 evaluation result: ${
            (await evaluation).result.result.value}`);
      },
  async function testInstrumentationStopResumesWithOtherSessions() {
    // Initialize two debugger sessions - one with instrumentation
    // breakpoints, one without.
    let contextGroup = new InspectorTest.ContextGroup();

    let session1 = contextGroup.connect();
    let Protocol1 = session1.Protocol;
    Protocol1.Debugger.enable();
    await Protocol1.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});

    const session2 = contextGroup.connect();
    const Protocol2 = session2.Protocol;
    await Protocol2.Debugger.enable();

    InspectorTest.log('Created two sessions.');

    // Enter instrumentation pause.
    const paused1 = Protocol1.Debugger.oncePaused();
    Protocol1.Runtime.evaluate({expression: 'null'})
    InspectorTest.log(`Session 1 paused (${(await paused1).params.reason})`);

    // Start evaluation in session 2.
    const evaluation = Protocol2.Runtime.evaluate({expression: '42'});

    // Stop the first session.
    const onResume2 = Protocol2.Debugger.onceResumed();
    session1.stop();
    InspectorTest.log('Stopped session 1');

    await onResume2;
    InspectorTest.log('Resumed session 2');

    // Make sure the second session gets the evaluation result.
    InspectorTest.log(`Session 2 evaluation result: ${
        (await evaluation).result.result.value}`);
  },
  async function testInstrumentationPauseAndNormalPause() {
    // Initialize two debugger sessions - one with instrumentation
    // breakpoints, one without.
    let contextGroup = new InspectorTest.ContextGroup();
    let session1 = contextGroup.connect();
    let Protocol1 = session1.Protocol;
    Protocol1.Debugger.enable();
    await Protocol1.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});

    const session2 = contextGroup.connect();
    const Protocol2 = session2.Protocol;
    await Protocol2.Debugger.enable();

    InspectorTest.log('Created two sessions.');

    // Enter instrumentation pause.
    const paused1 = Protocol1.Debugger.oncePaused();
    const instrumentationPaused2 = Protocol2.Debugger.oncePaused();
    const evaluation = Protocol1.Runtime.evaluate({expression: '42'})
    InspectorTest.log(`Session 1 paused (${(await paused1).params.reason})`);
    InspectorTest.log(
        `Session 2 paused (${(await instrumentationPaused2).params.reason})`);

    await Protocol2.Debugger.pause();
    InspectorTest.log('Session 2 pause requested');
    await Protocol1.Debugger.resume();
    InspectorTest.log('Session 1 instrumentation resume requested');

    // Check that the second session pauses and resumes correctly.
    const userPaused2 = Protocol1.Debugger.oncePaused();
    InspectorTest.log(
        `Session 2 paused (${(await userPaused2).params.reason})`);

    const resumed2 = Protocol2.Debugger.onceResumed();
    Protocol2.Debugger.resume();
    await resumed2;
    InspectorTest.log('Session 2 resumed');

    InspectorTest.log(`Session 1 evaluation result: ${
        (await evaluation).result.result.value}`);
  }
]);
