// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that a failed side-effect check in ' +
    'doesAttributeHaveObservableSideEffectOnGet is not swallowed.');

contextGroup.addScript(`
  var sideEffects = [];
  var key = {};
  Object.defineProperty(key, 'body', {get: Date.now, configurable: true});
  var m = new Map([[key, 123]]);
`);

(async function test() {
  await Protocol.Runtime.enable();
  const {result: {result: {objectId}}} =
      await Protocol.Runtime.evaluate({expression: 'm'});
  const {result: {internalProperties}} =
      await Protocol.Runtime.getProperties({objectId, ownProperties: true});
  const {value: {objectId: entriesId}} =
      internalProperties.find(({name}) => name === '[[Entries]]');
  const {result: {result}} = await Protocol.Runtime.getProperties(
      {objectId: entriesId, ownProperties: true});
  const entry = result.find(({value}) => value?.subtype === 'internal#entry');

  // Case 1: Request[Symbol.hasInstance] fails the side-effect check and
  // terminates execution; the following Response check must not run.
  await Protocol.Runtime.evaluate({
    expression: `
      globalThis.Request = {
        [Symbol.hasInstance](o) {
          sideEffects.push('Request');
          return false;
        }
      };
      globalThis.Response = {
        [Symbol.hasInstance](o) {
          throw new Error('boom');
        }
      };
    `,
  });
  let response = await Protocol.Runtime.callFunctionOn({
    objectId: entry.value.objectId,
    functionDeclaration: 'function() { return this; }',
    throwOnSideEffect: true,
    returnByValue: true,
  });
  InspectorTest.log(
      'Request side-effect: ' +
      (response.result.exceptionDetails ?
           'exception (' + response.result.exceptionDetails.text + ' ' +
               response.result.exceptionDetails.exception.description + ')' :
           'ok (' + JSON.stringify(response.result.result.value) + ')'));

  // Case 2: Response[Symbol.hasInstance] fails the side-effect check and
  // terminates execution; ValueMirror::getProperties must not invoke the
  // 'body' getter afterwards and clear the pending termination exception.
  await Protocol.Runtime.evaluate({
    expression: `
      delete globalThis.Request;
      globalThis.Response = {
        [Symbol.hasInstance](o) {
          sideEffects.push('Response');
          return false;
        }
      };
    `,
  });
  response = await Protocol.Runtime.callFunctionOn({
    objectId: entry.value.objectId,
    functionDeclaration: 'function() { return this; }',
    throwOnSideEffect: true,
    returnByValue: true,
  });
  InspectorTest.log(
      'Response side-effect: ' +
      (response.result.exceptionDetails ?
           'exception (' + response.result.exceptionDetails.text + ' ' +
               response.result.exceptionDetails.exception.description + ')' :
           'ok (' + JSON.stringify(response.result.result.value) + ')'));

  InspectorTest.completeTest();
})();
