// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that a failed side-effect check while describing a nested ' +
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
  // The key of the outer entry is itself a Map, so previewing it runs
  // EntryMirror::getEntries for the inner map, whose key is the Error with
  // the side-effecting prototype.
  var innerMap = new Map([[makeErrorWithSideEffectPrototype('inner-key'), 1]]);
  var outerMap = new Map([[innerMap, 123]]);
`);

(async () => {
  await Protocol.Runtime.enable();
  const {result: {result: {objectId}}} =
      await Protocol.Runtime.evaluate({expression: 'outerMap'});
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

  // Serializing the entry wrapper previews the inner Map key, which describes
  // the Error whose prototype proxy getter fails the side-effect check. The
  // termination exception must survive until the side-effect check scope
  // converts it into an EvalError.
  const response = await Protocol.Runtime.callFunctionOn({
    objectId: entry.value.objectId,
    functionDeclaration: 'function() { return this; }',
    throwOnSideEffect: true,
    returnByValue: true,
  });
  InspectorTest.log(
      'callFunctionOn on internal#entry with nested side-effect Error key: ' +
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
