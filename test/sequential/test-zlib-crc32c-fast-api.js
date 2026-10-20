// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

{
  function testFastPath() {
    const expected = 0x86a072c0; // zlib.crc32c('test', 0)
    assert.strictEqual(zlib.crc32c('test', 0), expected);
    return expected;
  }

  eval('%PrepareFunctionForOptimization(zlib.crc32c)');
  testFastPath();
  eval('%OptimizeFunctionOnNextCall(zlib.crc32c)');
  testFastPath();
  testFastPath();

  if (common.isDebug) {
    const { internalBinding } = require('internal/test/binding');
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('zlib.crc32c'), 2);
  }
}
