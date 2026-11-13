// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests evaluate across microtasks.');

const source = `
var listenerComplete = false;
var count = 0;
var log = [];

function LogX(x) {
  var stored_count = count;
  return function() {
    log.push(\`[\${stored_count}] \${x}\`);
  };
}

function DebuggerStatement() {
  log.push(\`[\${count}] debugger\`);
  if (count++ < 3) {
    debugger;
  }
}

function run() {
  DebuggerStatement();
  LogX("start")();
  if (!listenerComplete) throw new Error('listener did not complete');
  %PerformMicrotaskCheckpoint();
  return log;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: `
        var p = Promise.resolve();
        var q = p.then(LogX("then 1"));
        p.then(LogX("then 2"));
        q.then(LogX("then 3"));
        q.then(DebuggerStatement);
        var r = q.then(() => { throw 1; });
        r.catch(LogX("catch"));
        listenerComplete = true;
      `
    });
    await Protocol.Debugger.resume();
  });

  let {result: {result}} = await Protocol.Runtime.evaluate(
      {expression: 'run()', returnByValue: true});
  InspectorTest.log('Log:');
  for (let entry of result.value) {
    InspectorTest.log(entry);
  }

  const expectation = [
    '[0] debugger',
    '[1] start',
    '[1] then 1',
    '[1] then 2',
    '[1] then 3',
    '[1] debugger',
    '[2] then 1',
    '[2] then 2',
    '[1] catch',
    '[2] then 3',
    '[2] debugger',
    '[3] then 1',
    '[3] then 2',
    '[2] catch',
    '[3] then 3',
    '[3] debugger',
    '[3] catch',
  ];

  let matches = JSON.stringify(result.value) === JSON.stringify(expectation);
  InspectorTest.log(`Matches expectation: ${matches}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
