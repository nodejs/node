// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } =
  InspectorTest.start('RemoteObject.deepSerializedValue');

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(m => InspectorTest.logMessage(m));

InspectorTest.runAsyncTestSuite([
  async function Object() {
    await testExpression("{key_level_1: {key_level_2: {key_level_3: 'value_level_3'}}}");
  }]);

async function testExpression(expression) {
  InspectorTest.logMessage("testing expression: " + expression);

  await testSerializationOptions(expression, { serialization: "deep" })
  await testSerializationOptions(expression, { serialization: "deep", maxDepth: 0 })
  await testSerializationOptions(expression, { serialization: "deep", maxDepth: 1 })
  await testSerializationOptions(expression, { serialization: "deep", maxDepth: 2 })
  await testSerializationOptions(expression, { serialization: "deep", maxDepth: 999 })
  await testSerializationOptions(expression, {
    serialization: "deep",
    maxDepth: 999,
    additionalParameters: {
      "foo": "bar",
      "baz": "qux"
    }
  });
  await testSerializationOptions(expression, {
    serialization: "deep",
    maxDepth: 999,
    additionalParameters: {
      "INCORRECT_ADDITIONAL_PARAMETER": {}
    }
  });
  await testSerializationOptions(expression, { serialization: "json" })
  await testSerializationOptions(expression, { serialization: "json", maxDepth: 1 })
  await testSerializationOptions(expression, { serialization: "idOnly" })
  await testSerializationOptions(expression, { serialization: "INCORRECT_SERIALIZATION_TYPE" })
}

async function testSerializationOptions(expression, serializationOptions) {
  InspectorTest.logMessage(await serializeViaEvaluate(expression, serializationOptions));
  InspectorTest.logMessage(await serializeViaCallFunctionOn(expression, serializationOptions));
}

async function serializeViaEvaluate(expression, serializationOptions) {
  InspectorTest.logMessage("Runtime.evaluate");
  InspectorTest.logMessage(`expression: ${JSON.stringify(expression)}`);
  InspectorTest.logMessage(`serializationOptions: ${JSON.stringify(serializationOptions)}`);

  return await Protocol.Runtime.evaluate({
    expression: "(" + expression + ")",
    serializationOptions
  });
}

async function serializeViaCallFunctionOn(expression, serializationOptions) {
  const objectId = (await Protocol.Runtime.evaluate({
    expression: "({})",
  })).result.result.objectId;

  InspectorTest.logMessage("Runtime.callFunctionOn");
  InspectorTest.logMessage(`expression: ${JSON.stringify(expression)}`);
  InspectorTest.logMessage(`serializationOptions: ${JSON.stringify(serializationOptions)}`);

  return await Protocol.Runtime.callFunctionOn({
    functionDeclaration: "()=>{return " + expression + "}",
    objectId,
    serializationOptions
  });
}
