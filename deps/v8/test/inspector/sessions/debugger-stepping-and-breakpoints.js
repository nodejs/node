// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests how multiple sessions interact while pausing, stepping, setting breakpoints and blackboxing.');

var contextGroup = new InspectorTest.ContextGroup();

contextGroup.addScript(`
function foo() {
  return 1;
}
function baz() {
  return 2;
}
function stepping() {
  debugger;
  var a = 1;
  var b = 1;
}
//# sourceURL=test.js`, 9, 25);

contextGroup.addScript(`
function bar() {
  debugger;
}
//# sourceURL=test2.js`, 23, 25);

(async function test() {
  InspectorTest.log('Connecting session 1');
  var session1 = contextGroup.connect();
  await session1.Protocol.Debugger.enable();
  InspectorTest.log('Pausing in 1');
  session1.Protocol.Runtime.evaluate({expression: 'debugger;'});
  await waitForPaused(session1, 1);
  InspectorTest.log('Connecting session 2');
  var session2 = contextGroup.connect();
  var enabledPromise = session2.Protocol.Debugger.enable();
  await waitForPaused(session2, 2);
  await enabledPromise;
  InspectorTest.log('Resuming in 2');
  session2.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Setting breakpoints in 1');
  await session1.Protocol.Debugger.setBreakpointByUrl({url: 'test.js', lineNumber: 11});
  await session1.Protocol.Debugger.setBreakpointByUrl({url: 'test.js', lineNumber: 14});
  InspectorTest.log('Setting breakpoints in 2');
  await session2.Protocol.Debugger.setBreakpointByUrl({url: 'test.js', lineNumber: 11});

  InspectorTest.log('Evaluating common breakpoint in 1');
  session1.Protocol.Runtime.evaluate({expression: 'foo();'});
  await waitForBothPaused();
  InspectorTest.log('Resuming in 1');
  session1.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Evaluating debugger in 1');
  session1.Protocol.Runtime.evaluate({expression: 'bar();'});
  await waitForBothPaused();
  InspectorTest.log('Resuming in 2');
  session2.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Evaluating exclusive breakpoint in 1');
  session1.Protocol.Runtime.evaluate({expression: 'baz();'});
  await waitForBothPaused();
  InspectorTest.log('Resuming in 1');
  session1.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Evaluating common breakpoint in 2');
  session2.Protocol.Runtime.evaluate({expression: 'foo();'});
  await waitForBothPaused();
  InspectorTest.log('Resuming in 2');
  session2.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Evaluating debugger in 2');
  session2.Protocol.Runtime.evaluate({expression: 'bar();'});
  await waitForBothPaused();
  InspectorTest.log('Resuming in 2');
  session2.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Evaluating exclusive breakpoint in 2');
  session2.Protocol.Runtime.evaluate({expression: 'baz();'});
  await waitForBothPaused();
  InspectorTest.log('Resuming in 1');
  session1.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Evaluating stepping in 1');
  session1.Protocol.Runtime.evaluate({expression: 'stepping();'});
  await waitForBothPaused();
  InspectorTest.log('Stepping into in 2');
  session2.Protocol.Debugger.stepInto();
  await waitForBothResumed();
  await waitForBothPaused();
  InspectorTest.log('Stepping over in 1');
  session1.Protocol.Debugger.stepOver();
  await waitForBothResumed();
  await waitForBothPaused();
  InspectorTest.log('Stepping out in 2');
  session2.Protocol.Debugger.stepOut();
  await waitForBothResumed();
  await waitForBothPaused();
  InspectorTest.log('Resuming in 1');
  session1.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Pausing in next statement');
  contextGroup.schedulePauseOnNextStatement('some-reason', JSON.stringify({a: 42}));
  session2.Protocol.Runtime.evaluate({expression: 'var a = 1;'});
  await waitForBothPaused();
  InspectorTest.log('Resuming in 1');
  session1.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Pausing in next statement');
  contextGroup.schedulePauseOnNextStatement('some-reason', JSON.stringify({a: 42}));
  session2.Protocol.Runtime.evaluate({expression: 'var a = 1;'});
  await waitForBothPaused();
  InspectorTest.log('Resuming in 2');
  session2.Protocol.Debugger.resume();
  await waitForBothResumed();

  InspectorTest.log('Blackboxing bar() in 2');
  await session2.Protocol.Debugger.setBlackboxPatterns({patterns: ['test2.js']});
  InspectorTest.log('Evaluating bar() in 2');
  session2.Protocol.Runtime.evaluate({expression: 'bar();'});
  await waitForPaused(session1, 1);
  InspectorTest.log('Resuming in 1');
  session1.Protocol.Debugger.resume();
  await waitForResumed(session1, 1);

  InspectorTest.log('Blackboxing bar() in 1');
  await session1.Protocol.Debugger.setBlackboxPatterns({patterns: ['test2.js']});
  InspectorTest.log('Evaluating bar() in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'bar();'});

  InspectorTest.log('Skipping pauses in 1');
  await session1.Protocol.Debugger.setSkipAllPauses({skip: true});
  InspectorTest.log('Evaluating common breakpoint in 1');
  session1.Protocol.Runtime.evaluate({expression: 'foo();'});
  await waitForPaused(session2, 2);
  InspectorTest.log('Resuming in 2');
  session2.Protocol.Debugger.resume();
  await waitForResumed(session2, 2);

  InspectorTest.log('Skipping pauses in 2');
  await session2.Protocol.Debugger.setSkipAllPauses({skip: true});
  InspectorTest.log('Evaluating common breakpoint in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'foo();'});

  InspectorTest.log('Unskipping pauses in 1');
  await session1.Protocol.Debugger.setSkipAllPauses({skip: false});
  InspectorTest.log('Unskipping pauses in 2');
  await session2.Protocol.Debugger.setSkipAllPauses({skip: false});

  InspectorTest.log('Deactivating breakpoints in 1');
  await session1.Protocol.Debugger.setBreakpointsActive({active: false});
  InspectorTest.log('Evaluating common breakpoint in 1');
  session1.Protocol.Runtime.evaluate({expression: 'foo();'});
  await waitForPaused(session2, 2);
  InspectorTest.log('Resuming in 2');
  session2.Protocol.Debugger.resume();
  await waitForResumed(session2, 2);

  InspectorTest.log('Deactivating breakpoints in 2');
  await session2.Protocol.Debugger.setBreakpointsActive({active: false});
  InspectorTest.log('Evaluating common breakpoint in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'foo();'});

  InspectorTest.log('Activating breakpoints in 1');
  await session1.Protocol.Debugger.setBreakpointsActive({active: true});
  InspectorTest.log('Activating breakpoints in 2');
  await session2.Protocol.Debugger.setBreakpointsActive({active: true});
  InspectorTest.log('Disabling debugger agent in 1');
  await session1.Protocol.Debugger.disable();
  InspectorTest.log('Evaluating breakpoint in 1 (should not be triggered)');
  session2.Protocol.Runtime.evaluate({expression: 'baz();\ndebugger;'});
  await waitForPaused(session2, 2);

  session1.Protocol.Debugger.onResumed(() => InspectorTest.log('session1 is resumed'));
  session2.Protocol.Debugger.onResumed(() => InspectorTest.log('session2 is resumed'));
  InspectorTest.log('Activating debugger agent in 1');
  await session1.Protocol.Debugger.enable();
  InspectorTest.log('Disabling debugger agent in 2');
  await session2.Protocol.Debugger.disable();
  InspectorTest.log('Disabling debugger agent in 1');
  await session1.Protocol.Debugger.disable();

  InspectorTest.completeTest();

  function waitForBothPaused() {
    return Promise.all([waitForPaused(session1, 1), waitForPaused(session2, 2)]);
  }

  function waitForBothResumed() {
    return Promise.all([waitForResumed(session1, 1), waitForResumed(session2, 2)]);
  }
})();

function waitForPaused(session, num) {
  return session.Protocol.Debugger.oncePaused().then(message => {
    InspectorTest.log(`Paused in ${num}:`);
    InspectorTest.log(`  reason: ${message.params.reason}`);
    InspectorTest.log(`  hit breakpoints: ${(message.params.hitBreakpoints || []).join(';')}`);
    var callFrame = message.params.callFrames[0];
    InspectorTest.log(`  location: ${callFrame.functionName || '<anonymous>'}@${callFrame.location.lineNumber}`);
    InspectorTest.log(`  data: ${JSON.stringify(message.params.data || null)}`);
  });
}

function waitForResumed(session, num) {
  return session.Protocol.Debugger.onceResumed().then(message => {
    InspectorTest.log(`Resumed in ${num}`);
  });
}
