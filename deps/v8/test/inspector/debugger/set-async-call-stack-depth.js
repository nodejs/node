// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kozyatinskiy): fix or remove it later with new stack traces it's almost
// imposible to hit limit.
let {session, contextGroup, Protocol} = InspectorTest.start('Checks that we report not more then maxDepth call chains.');

contextGroup.addScript(`
function promisesChain(num) {
  var p = Promise.resolve();
  for (var i = 0; i < num - 1; ++i) {
    p = p.then(() => 42);
  }
  return p;
}
`);

Protocol.Debugger.enable();
InspectorTest.runAsyncTestSuite([
  async function testPaused() {
    let callback = '() => { debugger; }';
    startTest({ generated: 8, limit: 16, callback});
    dumpCaptured((await Protocol.Debugger.oncePaused()).params.asyncStackTrace);
    await Protocol.Debugger.resume();

    startTest({ generated: 8, limit: 8, callback});
    dumpCaptured((await Protocol.Debugger.oncePaused()).params.asyncStackTrace);
    await Protocol.Debugger.resume();

    startTest({ generated: 8, limit: 7, callback});
    dumpCaptured((await Protocol.Debugger.oncePaused()).params.asyncStackTrace);
    await Protocol.Debugger.resume();

    startTest({ generated: 8, limit: 0, callback});
    dumpCaptured((await Protocol.Debugger.oncePaused()).params.asyncStackTrace);
    await Protocol.Debugger.resume();
  },

  async function testConsoleTrace() {
    await Protocol.Runtime.enable();
    let callback = '() => { console.trace(42); }';
    startTest({ generated: 8, limit: 16, callback});
    let msg = await Protocol.Runtime.onceConsoleAPICalled();
    dumpCaptured(msg.params.stackTrace.parent);

    startTest({ generated: 8, limit: 8, callback});
    msg = await Protocol.Runtime.onceConsoleAPICalled();
    dumpCaptured(msg.params.stackTrace.parent);

    startTest({ generated: 8, limit: 7, callback});
    msg = await Protocol.Runtime.onceConsoleAPICalled();
    dumpCaptured(msg.params.stackTrace.parent);

    startTest({ generated: 8, limit: 0, callback});
    msg = await Protocol.Runtime.onceConsoleAPICalled();
    dumpCaptured(msg.params.stackTrace.parent);

    await Protocol.Runtime.disable();
  }
]);

function startTest(params) {
  InspectorTest.log('Actual call chain length: ' + params.generated);
  InspectorTest.log('setAsyncCallStackDepth(maxDepth): ' + params.limit);

  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: params.limit});
  Protocol.Runtime.evaluate({expression:
      `promisesChain(${params.generated}).then(${params.callback})`});
}

function dumpCaptured(stack) {
  let count = 0;
  while (stack) {
    ++count;
    stack = stack.parent;
  }
  InspectorTest.log('reported: ' + count + '\n');
}
