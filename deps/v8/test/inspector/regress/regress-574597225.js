// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that execution really stops when Runtime.terminateExecution ' +
    'arrives while the console builds the preview of an Error value');

function makeExpression(value) {
  return `
    globalThis.ranAfterConsoleLog = false;
    {
      let pausing = false;
      const err = new Error();
      Object.setPrototypeOf(err, {
        get name() {
          if (pausing) return 'Error';
          pausing = true;
          debugger;
          for (;;) {}
        }
      });
      console.log(${value});
    }
    globalThis.ranAfterConsoleLog = true;
  `;
}

async function runTest(value) {
  // The `name` getter of the Error's prototype is invoked by
  // descriptionForError() while the console builds the preview for the
  // `value`. It pauses there, the test terminates execution, and the
  // statement after console.log must not run.
  const paused = Protocol.Debugger.oncePaused();
  const evaluated =
      Protocol.Runtime.evaluate({expression: makeExpression(value)});
  await paused;
  Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume();
  const response = await evaluated;
  InspectorTest.log('evaluate error: ' + (response.error || {}).message);
  const {result: {result: {value: ranAfterConsoleLog}}} =
      await Protocol.Runtime.evaluate(
          {expression: 'globalThis.ranAfterConsoleLog', returnByValue: true});
  InspectorTest.log('ran after console.log: ' + ranAfterConsoleLog);
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
  },

  async function testErrorAsMapEntry() {
    await runTest('new Map([[1, err]])');
  },

  async function testErrorAsProperty() {
    await runTest('{a: err}');
  },

  async function testErrorAsArgument() {
    await runTest('err');
  },
]);
