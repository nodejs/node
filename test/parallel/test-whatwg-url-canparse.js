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
  function testFastPaths() {
    // `canParse` binding has two overloads.
    assert.strictEqual(URL.canParse('https://www.example.com/path/?query=param#hash'), true);
    assert.strictEqual(URL.canParse('/', 'http://n'), true);
  }

  // Since our JS function contains other javascript functions,
  // we need to specify which function we want to optimize. This is why
  // the next line does not optimize "testFastPaths" but "URL.canParse"
  eval('%PrepareFunctionForOptimization(URL.canParse)');
  testFastPaths();
  eval('%OptimizeFunctionOnNextCall(URL.canParse)');
  testFastPaths();

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('url.canParse'), 1);
    assert.strictEqual(getV8FastApiCallCount('url.canParse.withBase'), 1);
  }
}
