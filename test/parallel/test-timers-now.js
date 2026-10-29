// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('timers');

// Return value of getLibuvNow() should easily fit in a SMI after start-up.
// We need to use the binding as the receiver for fast API calls.
assert(binding.getLibuvNow() < 0x3ffffff);

{
  // Only javascript methods can be optimized through %OptimizeFunctionOnNextCall
  // This is why we surround the C++ method we want to optimize with a JS function.
  function getLibuvNow() {
    return binding.getLibuvNow();
  }

  eval('%PrepareFunctionForOptimization(getLibuvNow)');
  getLibuvNow();
  eval('%OptimizeFunctionOnNextCall(getLibuvNow)');
  assert(getLibuvNow() < 0x3ffffff);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('timers.getLibuvNow'), 1);
  }
}
