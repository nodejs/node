// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that re-entrant Atomics.wait during debug break throws an error.');

const source = `
var exception = null;

let sab = new SharedArrayBuffer(4);
let i32a = new Int32Array(sab);

let timeout = {
  valueOf: function() {
    %ScheduleBreak();
    return 10;
  }
};

function runWait() {
  Atomics.wait(i32a, 0, 0, timeout);
}

function checkException() {
  if (!exception) {
    throw new Error('exception was null');
  }
  if (!exception.message.includes('cannot be called in this context')) {
    throw new Error('Unexpected exception message: ' + exception.message);
  }
  return true;
}
`;

contextGroup.addScript(source);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async () => {
    await Protocol.Runtime.evaluate({
      expression: `
        try {
          var sab2 = new SharedArrayBuffer(4);
          var i32a2 = new Int32Array(sab2);
          Atomics.wait(i32a2, 0, 0, 10);
        } catch (e) {
          exception = e;
        }
      `
    });
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'runWait()'});

  const {result: {result: {value: passed}}} =
      await Protocol.Runtime.evaluate({expression: 'checkException()'});
  InspectorTest.log('checkException passed: ' + passed);

  const {result: {result: {value: exMsg}}} =
      await Protocol.Runtime.evaluate({expression: 'exception.message'});
  InspectorTest.log('exception message: ' + exMsg);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
