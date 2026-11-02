// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that object previews handle a pending termination correctly.');

contextGroup.addScript(`
  var sideEffects = [];
  var errWithProxyProto = new Error();
  Object.setPrototypeOf(errWithProxyProto, new Proxy({}, {
    get(target, prop) {
      if (prop === 'name') {
        sideEffects.push('side_effect');
        return 'custom';
      }
    }
  }));
  var m = new Map([[Promise.resolve(errWithProxyProto), 123]]);
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

  // The previous (not side-effect free) requests already described the Error,
  // so reset the side-effect log before the side-effect free request.
  await Protocol.Runtime.evaluate({expression: 'sideEffects.length = 0'});

  // Describing the entry previews the Promise key, whose [[PromiseResult]]
  // is the Error whose prototype proxy getter fails the side-effect check.
  // The preview must not continue with the own properties of the Promise
  // while the termination exception is pending.
  const response = await Protocol.Runtime.callFunctionOn({
    objectId: entry.value.objectId,
    functionDeclaration: 'function() { return this; }',
    throwOnSideEffect: true,
    returnByValue: true,
  });
  InspectorTest.log(
      'callFunctionOn on internal#entry with Promise key: ' +
      (response.result.exceptionDetails ?
           'exception (' + response.result.exceptionDetails.text + ' ' +
               response.result.exceptionDetails.exception.description + ')' :
           'ok (' + JSON.stringify(response.result.result.value) + ')'));

  const {result: {result: {value: sideEffects}}} =
      await Protocol.Runtime.evaluate(
          {expression: 'sideEffects', returnByValue: true});
  InspectorTest.log('sideEffects length: ' + sideEffects.length);
  InspectorTest.completeTest();
})();
