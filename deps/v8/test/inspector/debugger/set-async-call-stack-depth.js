// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that we report not more then maxDepth call chains.');

contextGroup.addScript(`
function asyncChain(breakAtEnd) {
  function asyncOpNested() {
    setTimeout(asyncOpNested1, 0);
  }
  function asyncOpNested1() {
    setTimeout(asyncOpNested2, 0);
  }
  function asyncOpNested2() {
    setTimeout(asyncOpNested3, 0);
  }
  function asyncOpNested3() {
    setTimeout(asyncOpNested4, 0);
  }
  function asyncOpNested4() {
    if (breakAtEnd) {
      debugger;
    } else {
      console.trace(42);
    }
  }
  asyncOpNested();
}
`);

Protocol.Debugger.enable();
InspectorTest.runAsyncTestSuite([
  async function testPaused() {
    const breakAtEnd = true;
    startTest({ limit: 8, breakAtEnd });
    dumpCaptured((await Protocol.Debugger.oncePaused()).params.asyncStackTrace);
    await Protocol.Debugger.resume();

    startTest({ limit: 4, breakAtEnd });
    dumpCaptured((await Protocol.Debugger.oncePaused()).params.asyncStackTrace);
    await Protocol.Debugger.resume();

    startTest({ limit: 3, breakAtEnd });
    dumpCaptured((await Protocol.Debugger.oncePaused()).params.asyncStackTrace);
    await Protocol.Debugger.resume();

    startTest({ limit: 0, breakAtEnd });
    dumpCaptured((await Protocol.Debugger.oncePaused()).params.asyncStackTrace);
    await Protocol.Debugger.resume();
  },

  async function testConsoleTrace() {
    await Protocol.Runtime.enable();
    const breakAtEnd = false;
    startTest({ limit: 8, breakAtEnd});
    let msg = await Protocol.Runtime.onceConsoleAPICalled();
    dumpCaptured(msg.params.stackTrace.parent);

    startTest({ limit: 4, breakAtEnd});
    msg = await Protocol.Runtime.onceConsoleAPICalled();
    dumpCaptured(msg.params.stackTrace.parent);

    startTest({ limit: 3, breakAtEnd});
    msg = await Protocol.Runtime.onceConsoleAPICalled();
    dumpCaptured(msg.params.stackTrace.parent);

    startTest({ limit: 0, breakAtEnd});
    msg = await Protocol.Runtime.onceConsoleAPICalled();
    dumpCaptured(msg.params.stackTrace.parent);

    await Protocol.Runtime.disable();
  }
]);

function startTest(params) {
  InspectorTest.log('Actual call chain length: 4');
  InspectorTest.log(`setAsyncCallStackDepth(maxDepth): ${params.limit}`);

  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: params.limit});
  Protocol.Runtime.evaluate({expression:
      `asyncChain(${params.breakAtEnd})`});
}

function dumpCaptured(stack) {
  let count = 0;
  while (stack) {
    ++count;
    stack = stack.parent;
  }
  InspectorTest.log('reported: ' + count + '\n');
}
