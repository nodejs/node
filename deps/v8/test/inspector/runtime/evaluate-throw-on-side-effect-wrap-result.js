// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that side-effect-free evaluation enforces side-effect checks during result serialization');

contextGroup.addScript(`
  var sideEffects = [];
  function makeObjectWithSideEffectGetter(tag) {
    return {
      get a() {
        sideEffects.push(tag);
        return 42;
      }
    };
  }
  function makeProxyWithSideEffectTrap(tag) {
    return new Proxy({a: 1}, {
      ownKeys() {
        sideEffects.push(tag);
        return ['a'];
      }
    });
  }
  function makePureObject() {
    return {a: 42, nested: [1, 2]};
  }
  var globalObjEvalJson = makeObjectWithSideEffectGetter('eval-json');
  var globalObjEvalDeep = makeObjectWithSideEffectGetter('eval-deep');
  var globalProxyEvalJson = makeProxyWithSideEffectTrap('proxy-ownKeys');
  var globalObjCallJson = makeObjectWithSideEffectGetter('callFunctionOn-json');
  var globalObjCallDeep = makeObjectWithSideEffectGetter('callFunctionOn-deep');
  var resolvedPromiseJson = Promise.resolve(makeObjectWithSideEffectGetter('promise-json'));
  var resolvedPromiseDeep = Promise.resolve(makeObjectWithSideEffectGetter('promise-deep'));
  var resolvedPromisePure = Promise.resolve(makePureObject());
  var rejectedPromiseJson = Promise.reject(makeObjectWithSideEffectGetter('reject-json'));
  rejectedPromiseJson.catch(function() {});
  function pauseWithLocal() {
    const localObj = makeObjectWithSideEffectGetter('evaluateOnCallFrame-json');
    debugger;
  }
`);

async function logResult(label, response) {
  const {result} = response;
  if (result.exceptionDetails) {
    InspectorTest.log(`${label}: exception (${result.result.description.split('\n')[0]})`);
  } else {
    InspectorTest.log(`${label}: ok (${JSON.stringify(result.result.value ?? result.result.deepSerializedValue)})`);
  }
}

InspectorTest.runAsyncTestSuite([
  async function testRuntimeEvaluateReturnByValue() {
    await logResult(
        'returnByValue with side-effect getter',
        await Protocol.Runtime.evaluate({
          expression: 'globalObjEvalJson',
          throwOnSideEffect: true,
          returnByValue: true,
        }));
    await logResult(
        'returnByValue with pure object',
        await Protocol.Runtime.evaluate({
          expression: 'makePureObject()',
          throwOnSideEffect: true,
          returnByValue: true,
        }));
  },

  async function testRuntimeEvaluateDeepSerialization() {
    await logResult(
        'deep serialization with side-effect getter',
        await Protocol.Runtime.evaluate({
          expression: 'globalObjEvalDeep',
          throwOnSideEffect: true,
          serializationOptions: {serialization: 'deep'},
        }));
  },

  async function testRuntimeEvaluateProxyTrap() {
    await logResult(
        'returnByValue with proxy ownKeys side-effect',
        await Protocol.Runtime.evaluate({
          expression: 'globalProxyEvalJson',
          throwOnSideEffect: true,
          returnByValue: true,
        }));
  },

  async function testRuntimeCallFunctionOn() {
    const {result: {result: {objectId}}} = await Protocol.Runtime.evaluate({
      expression: '({})',
    });
    await logResult(
        'callFunctionOn returnByValue with side-effect getter',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return globalObjCallJson; }',
          throwOnSideEffect: true,
          returnByValue: true,
        }));
    await logResult(
        'callFunctionOn deep serialization with side-effect getter',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return globalObjCallDeep; }',
          throwOnSideEffect: true,
          serializationOptions: {serialization: 'deep'},
        }));
  },

  async function testDebuggerEvaluateOnCallFrame() {
    await Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: 'setTimeout(pauseWithLocal, 0)'});
    const {params: {callFrames: [{callFrameId}]}} = await Protocol.Debugger.oncePaused();
    await logResult(
        'evaluateOnCallFrame returnByValue with side-effect getter',
        await Protocol.Debugger.evaluateOnCallFrame({
          callFrameId,
          expression: 'localObj',
          throwOnSideEffect: true,
          returnByValue: true,
        }));
    await Protocol.Debugger.resume();
    await Protocol.Debugger.disable();
  },

  async function testRuntimeEvaluateAwaitPromise() {
    await logResult(
        'awaitPromise returnByValue with non-promise side-effect getter',
        await Protocol.Runtime.evaluate({
          expression: 'globalObjEvalJson',
          throwOnSideEffect: true,
          returnByValue: true,
          awaitPromise: true,
        }));
    await logResult(
        'awaitPromise returnByValue with resolved promise side-effect getter',
        await Protocol.Runtime.evaluate({
          expression: 'resolvedPromiseJson',
          throwOnSideEffect: true,
          returnByValue: true,
          awaitPromise: true,
        }));
    await logResult(
        'awaitPromise returnByValue with resolved promise pure object',
        await Protocol.Runtime.evaluate({
          expression: 'resolvedPromisePure',
          throwOnSideEffect: true,
          returnByValue: true,
          awaitPromise: true,
        }));
    await logResult(
        'awaitPromise deep serialization with resolved promise side-effect getter',
        await Protocol.Runtime.evaluate({
          expression: 'resolvedPromiseDeep',
          throwOnSideEffect: true,
          serializationOptions: {serialization: 'deep'},
          awaitPromise: true,
        }));
    await logResult(
        'awaitPromise returnByValue with rejected promise side-effect getter',
        await Protocol.Runtime.evaluate({
          expression: 'rejectedPromiseJson',
          throwOnSideEffect: true,
          returnByValue: true,
          awaitPromise: true,
        }));
  },

  async function testRuntimeCallFunctionOnAwaitPromise() {
    const {result: {result: {objectId}}} = await Protocol.Runtime.evaluate({
      expression: '({})',
    });
    await logResult(
        'callFunctionOn awaitPromise returnByValue with side-effect getter',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return globalObjCallJson; }',
          throwOnSideEffect: true,
          returnByValue: true,
          awaitPromise: true,
        }));
    await logResult(
        'callFunctionOn awaitPromise deep serialization with side-effect getter',
        await Protocol.Runtime.callFunctionOn({
          objectId,
          functionDeclaration: 'function() { return globalObjCallDeep; }',
          throwOnSideEffect: true,
          serializationOptions: {serialization: 'deep'},
          awaitPromise: true,
        }));
  },

  async function testVerifyNoSideEffectsOccurred() {
    const {result: {result: {value}}} = await Protocol.Runtime.evaluate({
      expression: 'sideEffects',
      returnByValue: true,
    });
    InspectorTest.log('sideEffects length: ' + value.length);
  },
]);
