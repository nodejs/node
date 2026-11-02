// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that a failed side-effect check while describing an ' +
    'internal#entry key is reported instead of being cleared.');

contextGroup.addScript(`
  var sideEffects = [];
  function makeErrorWithSideEffectPrototype(tag) {
    const err = new Error();
    Object.setPrototypeOf(err, new Proxy({}, {
      get(target, prop) {
        sideEffects.push(tag);
        return 'custom';
      }
    }));
    return err;
  }
  var mapWithErrorKey =
      new Map([[makeErrorWithSideEffectPrototype('entry-key'), 123]]);
`);

(async () => {
  await Protocol.Runtime.enable();
  const {result: {result: {objectId}}} =
      await Protocol.Runtime.evaluate({expression: 'mapWithErrorKey'});
  const {result: {internalProperties}} =
      await Protocol.Runtime.getProperties({objectId});
  const {value: {objectId: entriesId}} =
      internalProperties.find(({name}) => name === '[[Entries]]');
  const {result: {result}} = await Protocol.Runtime.getProperties(
      {objectId: entriesId, ownProperties: true});
  const entry = result.find(({value}) => value?.subtype === 'internal#entry');

  // The previous (not side-effect free) requests already described the Error
  // key, so reset the side-effect log before the side-effect free request.
  await Protocol.Runtime.evaluate({expression: 'sideEffects.length = 0'});

  // Serializing the entry wrapper describes its Error key, whose prototype
  // proxy getter fails the side-effect check. The termination exception must
  // survive until the side-effect check scope converts it into an EvalError.
  const response = await Protocol.Runtime.callFunctionOn({
    objectId: entry.value.objectId,
    functionDeclaration: 'function() { return this; }',
    throwOnSideEffect: true,
    returnByValue: true,
  });
  InspectorTest.log(
      'callFunctionOn on internal#entry with side-effect Error key: ' +
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
