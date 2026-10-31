// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that execution really stops when Runtime.terminateExecution ' +
    'arrives while the console builds the message string for the first of ' +
    'several console.log() arguments');

function makeExpression(args) {
  // The element getter of `arr` runs inside V8ValueStringBuilder::append()
  // while console.log() builds the message string for the first argument.
  // It pauses there, the test terminates execution, and the statement after
  // console.log() must not run - no matter how many arguments are logged.
  return `
    globalThis.ranAfterConsoleLog = false;
    const arr = [];
    Object.defineProperty(arr, '0', {
      get() { debugger; return 1; },
      enumerable: true,
      configurable: true,
    });
    arr.length = 1;
    console.log(${args});
    globalThis.ranAfterConsoleLog = true;
  `;
}

async function runTest(args) {
  const paused = Protocol.Debugger.oncePaused();
  const evaluated =
      Protocol.Runtime.evaluate({expression: makeExpression(args)});
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

  async function testTwoArguments() {
    await runTest('arr, {}');
  },
]);
