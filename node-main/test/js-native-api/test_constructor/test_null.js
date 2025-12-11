'use strict';
const common = require('../../common');
const assert = require('assert');

// Test passing NULL to object-related N-APIs.
const { testNull } = require(`./build/${common.buildType}/test_constructor`);
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
