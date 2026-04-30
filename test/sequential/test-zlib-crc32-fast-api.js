// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

{
  function testFastPath() {
    const expected = 0xd87f7e0c; // zlib.crc32('test', 0)
    assert.strictEqual(zlib.crc32('test', 0), expected);
    return expected;
  }

  eval('%PrepareFunctionForOptimization(zlib.crc32)');
  testFastPath();
  eval('%OptimizeFunctionOnNextCall(zlib.crc32)');
  testFastPath();
  testFastPath();

  if (common.isDebug) {
    const { internalBinding } = require('internal/test/binding');
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('zlib.crc32'), 2);
  }
}
