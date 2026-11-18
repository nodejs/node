// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests async debug builtin predictions.');

const source = `
var thenable = {
  get then() {
    throw new Error();  // Exception
  }
};

class MyPromise extends Promise {
  get then() {
    throw new Error();
  }
}

function runTests() {
  MyPromise.resolve(thenable);

  var p = Promise.resolve(thenable);

  async function foo1() {
    return thenable;
  }
  foo1();

  async function foo2() {
    await thenable;
  }
  foo2();

  async function foo3() {
    try {
      await thenable;
    } catch (e) {}
  }
  foo3();
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let exceptionCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    exceptionCount++;
    let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
        {scriptId: callFrames[0].location.scriptId});
    let line = scriptSource.split('\n')[callFrames[0].location.lineNumber];
    let match = /Exception/.exec(line);
    InspectorTest.log(`Exception #${exceptionCount} at line ${
        callFrames[0].location.lineNumber}: ${match ? 'matched' : 'FAILED'}`);
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'runTests();'});
  await InspectorTest.waitForPendingTasks();

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  InspectorTest.log(`Total exceptions: ${exceptionCount}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
