// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Runtime.terminateExecution');

(async function test() {
  Protocol.Runtime.enable();
  Protocol.Runtime.onConsoleAPICalled(
      message => InspectorTest.logMessage(message.params.args[0]));
  InspectorTest.log(
      'Terminate first evaluation (it forces injected-script-source compilation)');
  await Promise.all([
    Protocol.Runtime.terminateExecution().then(InspectorTest.logMessage),
    Protocol.Runtime.evaluate({expression: 'console.log(42)'})
        .then(InspectorTest.logMessage)
  ]);

  InspectorTest.log('Checks that we reset termination after evaluation');
  InspectorTest.logMessage(
      await Protocol.Runtime.evaluate({expression: 'console.log(42)'}));
  InspectorTest.log(
      'Terminate evaluation when injected-script-source already compiled');
  await Promise.all([
    Protocol.Runtime.terminateExecution().then(InspectorTest.logMessage),
    Protocol.Runtime.evaluate({expression: 'console.log(42)'})
        .then(InspectorTest.logMessage)
  ]);

  InspectorTest.log('Terminate script evaluated with v8 API');
  const terminated =
      Protocol.Runtime.terminateExecution().then(InspectorTest.logMessage);
  contextGroup.addScript('console.log(42)');
  await terminated;

  InspectorTest.log('Terminate chained callback');
  Protocol.Debugger.enable();
  const paused = Protocol.Debugger.oncePaused();
  await Protocol.Runtime.evaluate({
    expression: `let p = new Promise(resolve => setTimeout(resolve, 0));
    p.then(() => {
        while(true){ debugger; }
      }).then(() => console.log('chained after chained callback'));
    p.then(() => { console.log('another chained callback'); });
    undefined`
  });
  await paused;

  InspectorTest.log('Pause inside microtask and terminate execution');
  Protocol.Runtime.terminateExecution().then(InspectorTest.logMessage);
  await Protocol.Debugger.resume();
  await Protocol.Runtime
      .evaluate({expression: `console.log('separate eval after while(true)')`})
      .then(InspectorTest.logMessage);
  await Protocol.Debugger.disable();

  InspectorTest.log('Terminate execution with pending microtasks');
  Protocol.Debugger.enable();
  const paused2 = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: `
      Promise.resolve().then(() => { console.log('FAIL: microtask ran'); });
      debugger;
      for (;;) {}
  `});
  await paused2;
  Protocol.Runtime.terminateExecution().then(InspectorTest.logMessage);
  await Protocol.Debugger.resume();

  await Protocol.Runtime.disable();
  InspectorTest.completeTest();
})();
