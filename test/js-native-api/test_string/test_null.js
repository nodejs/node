'use strict';
// Addons: test_string, test_string_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');

// Test passing NULL to object-related Node-APIs.
const { testNull } = require(addonPath);

const expectedResult = {
  envIsNull: 'Invalid argument',
  stringIsNullNonZeroLength: 'Invalid argument',
  stringIsNullZeroLength: 'napi_ok',
  resultIsNull: 'Invalid argument',
};

assert.deepStrictEqual(expectedResult, testNull.test_create_latin1());
assert.deepStrictEqual(expectedResult, testNull.test_create_utf8());
assert.deepStrictEqual(expectedResult, testNull.test_create_utf16());
