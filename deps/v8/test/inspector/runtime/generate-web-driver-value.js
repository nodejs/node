// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } =
  InspectorTest.start('RemoteObject.webDriverValue');

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(m => InspectorTest.logMessage(m));

InspectorTest.runAsyncTestSuite([
  async function PrimitiveValue() {
    await testExpression("undefined");
    await testExpression("null");
    await testExpression("'foo'");
    await testExpression("[true, false]");
  },
  async function Number() {
    await testExpression("[123, 0.56, -0, +Infinity, -Infinity, NaN]");
  },
  async function BigInt() {
    await testExpression("[123n, 1234567890n]");
  },
  async function Symbol() {
    await testExpression("Symbol('foo')");
  },
  async function Function() {
    await testExpression("[function qwe(){}, ()=>{}]");
  },
  async function Array() {
    await testExpression("[1,2,undefined]");
    await testExpression("new Array(1,2,undefined)");
  },
  async function RegExp() {
    await testExpression("[new RegExp('ab+c'), new RegExp('ab+c', 'ig')]");
  },
  async function Date() {
    await testExpression("new Date('Thu Apr 07 2022 16:17:18 GMT')");
    await testExpression("new Date('Thu Apr 07 2022 16:17:18 GMT+1100')");
    await testExpression("new Date('Thu Apr 07 2022 16:17:18 GMT-1100')");
  },
  async function Error() {
    await testExpression("[new Error(), new Error('qwe')]");
  },
  async function Map() {
    await testExpression("new Map([['keyString1', {valueObject1: 1}], [{keyObject2: 2}, 'valueString2'], ['keyString3', new Array()]])");
  },
  async function WeakMap() {
    await testExpression("new WeakMap([[{valueObject1: 1}, 'keyString1'],[{valueObject2: 2}, 'keyString2']])");
  },
  async function Set() {
    await testExpression("new Set([{valueObject1: 1}, 'valueString2', new Array(), undefined])");
  },
  async function Weakset() {
    await testExpression("new WeakSet([{valueObject1: 1}, {valueObject2: 2}])");
  },
  async function Proxy() {
    await testExpression("new Proxy({}, ()=>{})");
  },
  async function Promise() {
    await testExpression("new Promise(()=>{})");
  },
  async function Typedarray() {
    await testExpression("new Uint16Array()");
  },
  async function ArrayBuffer() {
    await testExpression("new ArrayBuffer()");
  },
  async function Object() {
    // Object.
    await testExpression("{nullKey: null, stringKey: 'foo',boolKey: true,numberKey: 123,bigintKey: 123n,symbolKey: Symbol('foo'),functionKey: () => {},arrayKey:[1],undefinedKey:undefined}");
    // Object in-depth serialization.
    await testExpression("{key_level_1: {key_level_2: {key_level_3: 'value_level_3'}}}");
 }]);

async function serializeViaEvaluate(expression) {
  return await Protocol.Runtime.evaluate({
    expression: "("+expression+")",
    generateWebDriverValue: true
  });
}

async function serializeViaCallFunctionOn(expression) {
  const objectId = (await Protocol.Runtime.evaluate({
    expression: "({})",
    generateWebDriverValue: true
  })).result.result.objectId;

  return await Protocol.Runtime.callFunctionOn({
    functionDeclaration: "()=>{return " + expression + "}",
    objectId,
    generateWebDriverValue: true
  });
}

async function testExpression(expression) {
  InspectorTest.logMessage("testing expression: "+expression);

  InspectorTest.logMessage("Runtime.evaluate");
  dumpResult(await serializeViaEvaluate(expression));
  InspectorTest.logMessage("Runtime.callFunctionOn");
  dumpResult(await serializeViaCallFunctionOn(expression));
}

function dumpResult(result) {
  if (result && result.result && result.result.result && result.result.result.webDriverValue) {
    InspectorTest.logMessage(result.result.result.webDriverValue);
  } else {
    InspectorTest.log("...no webDriverValue...");
    InspectorTest.logMessage(result);
  }
}
