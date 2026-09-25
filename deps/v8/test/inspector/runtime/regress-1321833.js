// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/1321833');

Protocol.Runtime.onConsoleAPICalled(({params: {args, type}}) => {
  InspectorTest.logObject(args, type);
});

InspectorTest.runAsyncTestSuite([
  async function testNumberNaN() {
    await Protocol.Runtime.enable();
    const {result} = await Protocol.Runtime.evaluate(
        {expression: 'console.log(new Number(NaN))'});
    if ('exceptionDetails' in result) {
      InspectorTest.logMessage(result.exceptionDetails);
    }
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testNumberInfinity() {
    await Protocol.Runtime.enable();
    const {result} = await Protocol.Runtime.evaluate(
        {expression: 'console.log(new Number(Infinity))'});
    if ('exceptionDetails' in result) {
      InspectorTest.logMessage(result.exceptionDetails);
    }
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testNumberMinusInfinity() {
    await Protocol.Runtime.enable();
    const {result} = await Protocol.Runtime.evaluate(
        {expression: 'console.log(new Number(-Infinity))'});
    if ('exceptionDetails' in result) {
      InspectorTest.logMessage(result.exceptionDetails);
    }
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },
]);
