// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start(
    'Tests that deep serialization reports the result value only in ' +
    'RemoteObject.deepSerializedValue.');

const serializationOptions = {serialization: 'deep'};
const expressions = [
  'undefined',
  'null',
  '\'foo\'',
  'true',
  '42',
  '-0',
  'NaN',
  '123n',
  'Symbol(\'foo\')',
  '({a: \'foo\'})',
];

InspectorTest.runAsyncTestSuite([
  async function testEvaluate() {
    for (const expression of expressions) {
      InspectorTest.log(expression);
      const {result} =
          await Protocol.Runtime.evaluate({expression, serializationOptions});
      InspectorTest.logMessage(result.result);
    }
  },

  async function testCallFunctionOn() {
    const {result: {result: {objectId}}} =
        await Protocol.Runtime.evaluate({expression: '({})'});
    for (const expression of expressions) {
      InspectorTest.log(expression);
      const {result} = await Protocol.Runtime.callFunctionOn({
        objectId,
        functionDeclaration: `() => (${expression})`,
        serializationOptions,
      });
      InspectorTest.logMessage(result.result);
    }
  },

  async function testAwaitPromise() {
    InspectorTest.log('Promise.resolve(\'foo\')');
    const {result: resolved} = await Protocol.Runtime.evaluate({
      expression: 'Promise.resolve(\'foo\')',
      awaitPromise: true,
      serializationOptions,
    });
    InspectorTest.logMessage(resolved.result);

    // The rejection reason in `exceptionDetails.exception` is not deep
    // serialized, so it keeps its `value`.
    InspectorTest.log('Promise.reject(\'foo\')');
    const {result: rejected} = await Protocol.Runtime.evaluate({
      expression: 'Promise.reject(\'foo\')',
      awaitPromise: true,
      serializationOptions,
    });
    InspectorTest.logMessage(rejected.result);
    InspectorTest.logMessage(rejected.exceptionDetails.exception);
  },
]);
