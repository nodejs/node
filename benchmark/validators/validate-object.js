'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e4],
  objectToTest: [
    'object',
    'null',
    'array',
    'function'
  ],
  option: [
    'default',
    'allowNullable',
    'allowArray',
    'allowFunction',
    'allowAll',
  ],
}, {
  flags: ['--expose-internals'],
});

function getObjectToTest(objectToTest) {
  switch (objectToTest) {
    case 'object':
      return { foo: 'bar' };
    case 'null':
      return null;
    case 'array':
      return ['foo', 'bar'];
    case 'function':
      return () => 'foo';
    default:
      throw new Error(`Value ${objectToTest} is not a valid objectToTest.`);
  }
}

function getOptions(option) {
  const {
    kValidateObjectAllowNullable,
    kValidateObjectAllowArray,
    kValidateObjectAllowFunction,
  } = require('internal/validators');

  switch(option) {
    case 'default':
      return 0;
    case 'allowNullable':
      return kValidateObjectAllowNullable;
    case 'allowArray':
      return kValidateObjectAllowArray;
    case 'allowFunction':
      return kValidateObjectAllowFunction;
    case 'allowAll':
      return kValidateObjectAllowNullable | kValidateObjectAllowArray | kValidateObjectAllowFunction;
    default:
      throw new Error(`Value ${ option } is not a valid option.`)
  }
}

let _validateResult;

function main({ n, objectToTest, option }) {
  const {
    validateObject,
  } = require('internal/validators');

  const value = getObjectToTest(objectToTest);
  const options = getOptions(option);

  bench.start();
  for (let i = 0; i < n; ++i) {
    try {
      _validateResult = validateObject(value, 'Object', options);
    } catch (e) {
      _validateResult = undefined;
    }
  }
  bench.end(n);

  assert.ok(!_validateResult);
}
