// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } =
  InspectorTest.start('RemoteObject.deepSerializedValueErrors');

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(m => InspectorTest.logMessage(m));

InspectorTest.runAsyncTestSuite([
  async function ObjectThrowingGetter() {
    await testExpression("({ get foo() { throw new Error('getter error'); } })");
  },
  async function ArrayThrowingGetter() {
    await testExpression("(()=>{ const a = [1]; Object.defineProperty(a, '1', { get() { throw new Error('array element error'); }, enumerable: true }); return a; })()");
  },
  async function NestedThrowingGetter() {
    await testExpression("({ a: { b: [ { get c() { throw new Error('nested error'); } } ] } })");
  },
  async function SpecialCharacterKeyThrowingGetter() {
    await testExpression("({ 'invalid identifier!': { get ['another special']() { throw new Error('special key error'); } } })");
  },
  async function NumericKeyThrowingGetter() {
    await testExpression("({ '0': { get ['1']() { throw new Error('numeric key error'); } } })");
  }
]);

async function serializeViaEvaluate(expression) {
  return await Protocol.Runtime.evaluate({
    expression: "(" + expression + ")",
    serializationOptions: { serialization: "deep" }
  });
}

async function serializeViaCallFunctionOn(expression) {
  const objectId = (await Protocol.Runtime.evaluate({
    expression: "({})",
  })).result.result.objectId;

  return await Protocol.Runtime.callFunctionOn({
    functionDeclaration: "()=>{return " + expression + "}",
    objectId,
    serializationOptions: { serialization: "deep" }
  });
}

async function testExpression(expression) {
  InspectorTest.logMessage("testing expression: " + expression);

  InspectorTest.logMessage("Runtime.evaluate");
  dumpResult(await serializeViaEvaluate(expression));
  InspectorTest.logMessage("Runtime.callFunctionOn");
  dumpResult(await serializeViaCallFunctionOn(expression));
}

function dumpResult(result) {
  if (result?.result?.result?.deepSerializedValue) {
    InspectorTest.logMessage(result.result.result.deepSerializedValue);
  } else {
    InspectorTest.log("...no deepSerializedValue...");
    InspectorTest.logMessage(result);
  }
}
