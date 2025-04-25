// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');

const { URL } = require('url');
const assert = require('assert');

const { internalBinding } = require('internal/test/binding');

// One argument is required
assert.throws(() => {
  URL.canParse();
}, {
  code: 'ERR_MISSING_ARGS',
  name: 'TypeError',
});

// It should not throw when called without a base string
assert.strictEqual(URL.canParse('https://example.org'), true);

{
  // Only javascript methods can be optimized through %OptimizeFunctionOnNextCall
  // This is why we surround the C++ method we want to optimize with a JS function.
  function canParse(input) {
    return URL.canParse(input);
  }

  function canParseWithBase(input, base) {
    return URL.canParse(input, base);
  }

  eval('%PrepareFunctionForOptimization(canParse)');
  canParse('https://nodejs.org');
  eval('%OptimizeFunctionOnNextCall(canParse)');
  canParse('https://nodejs.org');

  eval('%PrepareFunctionForOptimization(canParseWithBase)');
  canParseWithBase('https://nodejs.org');
  eval('%OptimizeFunctionOnNextCall(canParseWithBase)');
  canParseWithBase('/contact', 'https://nodejs.org');

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('url.canParse'), 1);
    assert.strictEqual(getV8FastApiCallCount('url.canParse.withBase'), 1);
  }
}
