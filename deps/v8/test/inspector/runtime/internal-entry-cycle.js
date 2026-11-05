// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that describing cyclic internal#entry wrappers terminates.');

(async () => {
  const {result: {result: {objectId}}} = await Protocol.Runtime.evaluate(
      {expression: 'new Map([[1, 2], [3, 4], [5, 6]])'});
  const {result: {internalProperties}} =
      await Protocol.Runtime.getProperties({objectId});
  const {value: {objectId: entriesId}} =
      internalProperties.find(({name}) => name === '[[Entries]]');

  async function logEntries() {
    const {result: {result}} = await Protocol.Runtime.getProperties(
        {objectId: entriesId, ownProperties: true, generatePreview: true});
    const entries =
        result.filter(({value}) => value?.subtype === 'internal#entry');
    for (const {name, value: {description}} of entries) {
      InspectorTest.log(`${name}: ${description}`);
    }
    return entries.map(({value: {objectId}}) => objectId);
  }

  InspectorTest.log('Original entries:');
  const entryIds = await logEntries();

  // Make entry 0 refer to entry 1 (non-cyclic nesting: A -> B).
  await Protocol.Runtime.callFunctionOn({
    objectId: entryIds[0],
    functionDeclaration: 'function(other) { this.value = other; }',
    arguments: [{objectId: entryIds[1]}],
  });

  InspectorTest.log('Nested non-cyclic entries:');
  await logEntries();

  // Make the entry wrappers cyclic:
  // - entry 0 <-> entry 1 (2-hop cycle: A -> B -> A)
  // - entry 1 key refers to itself (direct key cycle)
  // - entry 2 value refers to itself via a property of another object
  await Protocol.Runtime.callFunctionOn({
    objectId: entryIds[1],
    functionDeclaration:
        'function(other) { this.key = this; this.value = other; }',
    arguments: [{objectId: entryIds[0]}],
  });
  await Protocol.Runtime.callFunctionOn({
    objectId: entryIds[2],
    functionDeclaration: 'function() { this.value = {entry: this}; }',
  });

  InspectorTest.log('Cyclic entries:');
  await logEntries();

  InspectorTest.completeTest();
})();
