'use strict';
const common = require('../../common');
const assert = require('assert');

// Test passing NULL to object-related N-APIs.
const { testNull } = require(`./build/${common.buildType}/test_string`);

const expectedResult = {
  envIsNull: 'Invalid argument',
  stringIsNullNonZeroLength: 'Invalid argument',
  stringIsNullZeroLength: 'napi_ok',
  resultIsNull: 'Invalid argument',
};

assert.deepStrictEqual(expectedResult, testNull.test_create_latin1());
assert.deepStrictEqual(expectedResult, testNull.test_create_utf8());
assert.deepStrictEqual(expectedResult, testNull.test_create_utf16());
