// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-flush-code

let {session, contextGroup, Protocol} = InspectorTest.start('Checks internal [[Entries]] in Runtime.getProperties output');

Protocol.Runtime.enable();

InspectorTest.runTestSuite([
  function maps(next) {
    checkExpression('new Map([[1,2],[3,4]])')
      .then(() => checkExpression('new Map()'))
      .then(next);
  },

  function mapIterators(next) {
    checkExpression('new Map([[1,2],[3,4]]).entries()')
      .then(() => checkExpression('it = new Map([[1,2],[3,4]]).entries(); it.next(); it'))
      .then(() => checkExpression('it = new Map([[1,2],[3,4]]).keys(); it.next(); it'))
      .then(() => checkExpression('it = new Map([[1,2],[3,4]]).values(); it.next(); it'))
      .then(() => checkExpression('it = new Map([[1,2],[3,4]]).entries(); it.next(); it.next(); it'))
      .then(() => checkExpression('new Map([[1, undefined], [2, () => 42], [3, /abc/], [4, new Error()]]).entries()'))
      .then(next);
  },

  function sets(next) {
    checkExpression('new Set([1,2])')
      .then(() => checkExpression('new Set()'))
      .then(next);
  },

  function setIterators(next) {
    checkExpression('new Set([1,2]).values()')
      .then(() => checkExpression('it = new Set([1,2]).values(); it.next(); it'))
      .then(() => checkExpression('it = new Set([1,2]).keys(); it.next(); it'))
      .then(() => checkExpression('it = new Set([1,2]).entries(); it.next(); it'))
      .then(() => checkExpression('it = new Set([1,2]).values(); it.next(); it.next(); it'))
      .then(next);
  },

  function weakMaps(next) {
    checkExpression('new WeakMap()')
      .then(() => checkExpression('new WeakMap([[{ a: 2 }, 42]])'))
      .then(next);
  },

  function weakSets(next) {
    checkExpression('new WeakSet()')
      .then(() => checkExpression('new WeakSet([{a:2}])'))
      .then(next);
  }
]);

function checkExpression(expression)
{
  InspectorTest.log(`expression: ${expression}`);
  var entriesObjectId;
  return Protocol.Runtime.evaluate({ expression: expression })
    .then(message => Protocol.Runtime.getProperties({ objectId: message.result.result.objectId }))
    .then(message => message.result.internalProperties.filter(p => p.name === '[[Entries]]')[0])
    .then(entries => entriesObjectId = entries.value.objectId)
    .then(() => Protocol.Runtime.callFunctionOn({ objectId: entriesObjectId, functionDeclaration: 'function f() { return this; }', returnByValue: true }))
    .then(message => InspectorTest.logMessage(message.result.result.value))
    .then(() => Protocol.Runtime.getProperties({ objectId: entriesObjectId, ownProperties: true }))
    .then(message => InspectorTest.logMessage(message));
}
