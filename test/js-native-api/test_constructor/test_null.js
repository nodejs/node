'use strict';
// Addons: test_constructor, test_constructor_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');

// Test passing NULL to object-related Node-APIs.
const { testNull } = require(addonPath);
const expectedResult = {
  envIsNull: 'Invalid argument',
  nameIsNull: 'Invalid argument',
  lengthIsZero: 'napi_ok',
  nativeSideIsNull: 'Invalid argument',
  dataIsNull: 'napi_ok',
  propsLengthIsZero: 'napi_ok',
  propsIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
};

assert.deepStrictEqual(testNull.testDefineClass(), expectedResult);
