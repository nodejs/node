// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that mirroring an arguments object with an accessor "length" ' +
    'does not run user-defined getters on Object.prototype');

contextGroup.addScript(`
var getterCalls = 0;

function makeArguments() { return arguments; }

var accessorLengthArgs = makeArguments(1, 2, 3);
Object.defineProperty(accessorLengthArgs, 'length', {
  get() { ++getterCalls; return 0; },
});

var dataLengthArgs = makeArguments(1, 2, 3);

// Installed last: Object.defineProperty itself reads "value" from its
// descriptor argument.
Object.defineProperty(Object.prototype, 'value', {
  __proto__: null,
  get() { ++getterCalls; return 0; },
  configurable: true,
});

// Buffered until Runtime.enable replays them.
console.log(accessorLengthArgs);
console.log(dataLengthArgs);
//# sourceURL=test.js`);

function logRemoteObject({type, className, description}) {
  InspectorTest.logMessage({type, className, description});
}

async function logGetterCalls() {
  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'getterCalls'});
  InspectorTest.log(`getterCalls: ${result.value}`);
}

Protocol.Runtime.onConsoleAPICalled(
    ({params}) => logRemoteObject(params.args[0]));

InspectorTest.runAsyncTestSuite([
  async function testReplayOnEnable() {
    await Protocol.Runtime.enable();
    await logGetterCalls();
  },

  async function testEvaluate() {
    for (const expression of ['accessorLengthArgs', 'dataLengthArgs']) {
      const {result: {result}} = await Protocol.Runtime.evaluate(
          {expression, generatePreview: true});
      logRemoteObject(result);
    }
    await logGetterCalls();
    await Protocol.Runtime.disable();
  },
]);
