// Flags: --expose-internals --allow-natives-syntax
'use strict';
const common = require('../common');
const assert = require('node:assert');

const { internalBinding } = require('internal/test/binding');

// Because registering a Blob URL requires generating a random
// UUID, it can only be done if crypto support is enabled.
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const blob = new Blob([JSON.stringify({ hello: 'world' }, null, 2)], {
  type: 'application/json',
});

function testFastPath() {
  const objURL = URL.createObjectURL(blob);
  URL.revokeObjectURL(objURL);
}

eval('%PrepareFunctionForOptimization(URL.revokeObjectURL)');
testFastPath();

eval('%OptimizeFunctionOnNextCall(URL.revokeObjectURL)');
testFastPath();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('blob.revokeObjectURL'), 1);
}
