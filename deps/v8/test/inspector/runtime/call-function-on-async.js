// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that Runtime.callFunctionOn works with awaitPromise flag.');
let callFunctionOn = Protocol.Runtime.callFunctionOn.bind(Protocol.Runtime);

let remoteObject1;
let remoteObject2;

InspectorTest.runAsyncTestSuite([
  async function prepareTestSuite() {
    let result = await Protocol.Runtime.evaluate({ expression: '({a : 1})' });
    remoteObject1 = result.result.result;
    result = await Protocol.Runtime.evaluate({ expression: '({a : 2})' });
    remoteObject2 = result.result.result;
  },

  async function testArguments() {
    InspectorTest.logMessage(await callFunctionOn({
      objectId: remoteObject1.objectId,
      functionDeclaration: 'function(arg1, arg2, arg3, arg4) { return \'\' + arg1 + \'|\' + arg2 + \'|\' + arg3 + \'|\' + arg4; }',
      arguments: prepareArguments([undefined, NaN, remoteObject2, remoteObject1]),
      returnByValue: true,
      generatePreview: false,
      awaitPromise: false
    }));
  },

  async function testComplexArguments() {
    InspectorTest.logMessage(await callFunctionOn({
      objectId: remoteObject1.objectId,
      functionDeclaration: 'function(arg) { return arg.foo; }',
      arguments: prepareArguments([{foo: 'bar'}]),
      returnByValue: true,
      generatePreview: false,
      awaitPromise: false
    }));
  },

  async function testSyntaxErrorInFunction() {
    InspectorTest.logMessage(await callFunctionOn({
      objectId: remoteObject1.objectId,
      functionDeclaration: '\n  }',
      arguments: prepareArguments([]),
      returnByValue: false,
      generatePreview: false,
      awaitPromise: true
    }));
  },

  async function testExceptionInFunctionExpression() {
    InspectorTest.logMessage(await callFunctionOn({
      objectId: remoteObject1.objectId,
      functionDeclaration: '(function() { throw new Error() })()',
      arguments: prepareArguments([]),
      returnByValue: false,
      generatePreview: false,
      awaitPromise: true
    }));
  },

  async function testFunctionReturnNotPromise() {
   InspectorTest.logMessage(await callFunctionOn({
      objectId: remoteObject1.objectId,
      functionDeclaration: '(function() { return 239; })',
      arguments: prepareArguments([]),
      returnByValue: true,
      generatePreview: false,
      awaitPromise: true
    }));
  },

  async function testFunctionReturnResolvedPromiseReturnByValue() {
    InspectorTest.logMessage(await callFunctionOn({
      objectId: remoteObject1.objectId,
      functionDeclaration: '(function(arg) { return Promise.resolve({a : this.a + arg.a}); })',
      arguments: prepareArguments([ remoteObject2 ]),
      returnByValue: true,
      generatePreview: false,
      awaitPromise: true
    }));
  },

  async function testFunctionReturnResolvedPromiseWithPreview() {
    InspectorTest.logMessage(await callFunctionOn({
      objectId: remoteObject1.objectId,
      functionDeclaration: '(function(arg) { return Promise.resolve({a : this.a + arg.a}); })',
      arguments: prepareArguments([ remoteObject2 ]),
      returnByValue: false,
      generatePreview: true,
      awaitPromise: true
    }));
  },

  async function testFunctionReturnRejectedPromise() {
    InspectorTest.logMessage(await callFunctionOn({
      objectId: remoteObject1.objectId,
      functionDeclaration: '(function(arg) { return Promise.reject({a : this.a + arg.a}); })',
      arguments: prepareArguments([ remoteObject2 ]),
      returnByValue: true,
      generatePreview: false,
      awaitPromise: true
    }));
  }
]);

function prepareArguments(args) {
  return args.map(arg => {
    if (Object.is(arg, -0))
      return {unserializableValue: '-0'};
    if (Object.is(arg, NaN) || Object.is(arg, Infinity) || Object.is(arg, -Infinity))
      return {unserializableValue: arg + ''};
    if (arg && arg.objectId)
      return {objectId: arg.objectId};
    return {value: arg};
  });
}
